#include "ppu_gpu.h"
#include "ppu_gpu_model.h"
#include "wide_bounds.h"
#include "sm_pica_shbin.h"
#include "src/variables.h"

#include <3ds.h>
#include <citro2d.h>
#include <stdlib.h>
#include <string.h>
#ifdef SM3DS_PROFILE
#include "SDL2/SDL.h"
#endif

typedef struct Vertex {
  int16_t x, y, z, w, u, v;
  uint8_t r, g, b, a;
} Vertex;
_Static_assert(sizeof(Vertex) == 16, "GPU vertex layout");

typedef struct Range { unsigned first, count; } Range;

#ifdef SM3DS_PROFILE
uint64_t g_profile_pica_prepare_ticks;
uint32_t g_profile_pica_gpu_frames;
uint32_t g_profile_pica_cpu_frames;
uint32_t g_profile_pica_vertices;
uint32_t g_profile_pica_decodes;
uint32_t g_profile_pica_wide_gpu_frames;
uint32_t g_profile_pica_wide_cpu_frames;
#endif

static struct {
  bool initialized, ready, prepared, output, program;
  Ppu *saved;
  PicaLine lines[PICA_MAX_LINES];
  PicaAtlas *cachePool[2], *cache;
  Vertex *vertexPool[2], *vertices;
  C3D_Tex atlasPool[2], atlas, main, sub, result;
  C3D_RenderTarget *mainTarget, *subTarget, *resultTarget;
  DVLB_s *shader;
  shaderProgram_s shaderProgram;
  C3D_AttrInfo attributes;
  C3D_BufInfo buffers;
  int scaleLocation;
  Range ranges[PICA_GROUPS];
  unsigned slot, lastSubmittedSlot, count, width, height, captured, hudLines;
  int worldLeft, worldRight;
  float invWidth, invHeight;
  const char *reason;
} g;

static WideConfig g_wide_config = {
    .output_width = kSnesWidth,
    .hud_end_y = kHudEndLine,
};

static bool Clean(const void *p, size_t bytes) {
  if (!bytes) return true;
  if (R_SUCCEEDED(svcStoreProcessDataCache(CUR_PROCESS_HANDLE,
                                            (u32)(uintptr_t)p, bytes))) return true;
  return R_SUCCEEDED(GSPGPU_FlushDataCache(p, bytes));
}

static uint32_t Rgba(unsigned r, unsigned green, unsigned b, unsigned a) {
  return r | (green << 8) | (b << 16) | (a << 24);
}

static void ResetRanges(unsigned width, unsigned height) {
  memset(g.ranges, 0, sizeof(g.ranges));
  g.count = 0;
  g.width = width;
  g.height = height;
  g.invWidth = 2.0f / width;
  g.invHeight = 2.0f / height;
}

static bool Emit(void *context, unsigned group, const PicaQuad *q) {
  (void)context;
  if (g.count + 6 > PICA_MAX_VERTICES) return false;
  Range *range = &g.ranges[group];
  if (!range->count) range->first = g.count;
  if (range->first + range->count != g.count) return false;
  int16_t depth = q->depth >> 1;
  if (!depth) depth = 1;
  Vertex a = {q->x0,q->y0,depth,1,q->u0,q->v0,q->r,q->g,q->b,q->a};
  Vertex b = a, c = a, d = a;
  b.x = d.x = q->x1; b.u = d.u = q->u1;
  c.y = d.y = q->y1; c.v = d.v = q->v1;
  Vertex *v = &g.vertices[g.count];
  v[0] = a; v[1] = b; v[2] = c; v[3] = c; v[4] = b; v[5] = d;
  g.count += 6;
  range->count += 6;
  return true;
}

static void DrawRange(unsigned group) {
  Range r = g.ranges[group];
  while (r.count) {
    unsigned n = r.count > 32766 ? 32766 : r.count;
    C3D_DrawArrays(GPU_TRIANGLES, r.first, n);
    r.first += n;
    r.count -= n;
  }
}

static void ResetTev(void) {
  for (int i = 0; i < 6; i++) C3D_TexEnvInit(C3D_GetTexEnv(i));
  C3D_TexEnvBufUpdate(C3D_Both, 0);
  C3D_TexEnvBufColor(0);
}

static void BindState(void) {
  C3D_BindProgram(&g.shaderProgram);
  C3D_SetAttrInfo(&g.attributes);
  C3D_SetBufInfo(&g.buffers);
  C3D_FVUnifSet(GPU_VERTEX_SHADER, g.scaleLocation,
                g.invWidth, -g.invHeight, -1.0f / 32768.0f, 1.0f);
  C3D_CullFace(GPU_CULL_NONE);
  C3D_DepthMap(true, -1.0f, 0.0f);
  C3D_FragOpMode(GPU_FRAGOPMODE_GL);
  C3D_EarlyDepthTest(false, GPU_EARLYDEPTH_GREATER, 0);
  C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ZERO,
                 GPU_ONE, GPU_ZERO);
  C3D_AlphaTest(false, GPU_ALWAYS, 0);
  C3D_StencilTest(false, GPU_ALWAYS, 0, 0xff, 0);
  C3D_StencilOp(GPU_STENCIL_KEEP, GPU_STENCIL_KEEP, GPU_STENCIL_KEEP);
}

static void Viewport(void) {
  C3D_SetViewport(0, 256 - g.height, g.width, g.height);
  C3D_SetScissor(GPU_SCISSOR_NORMAL, 0, 256 - g.height, g.width, 256);
}

static void RestoreC2D(void) {
  ResetTev();
  C3D_AlphaTest(false, GPU_ALWAYS, 0);
  C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_ALL);
  C3D_StencilTest(false, GPU_ALWAYS, 0, 0xff, 0);
  C3D_StencilOp(GPU_STENCIL_KEEP, GPU_STENCIL_KEEP, GPU_STENCIL_KEEP);
  C3D_SetScissor(GPU_SCISSOR_DISABLE, 0, 0, 0, 0);
  C2D_Prepare();
  C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_COLOR);
}

static bool DrawLayers(C3D_RenderTarget *target, unsigned sub) {
  C3D_FrameSplit(GX_CMDLIST_FLUSH);
  C3D_RenderTargetClear(target, C3D_CLEAR_ALL, 0, 0);
  if (!C3D_FrameDrawOn(target)) return false;
  BindState();
  Viewport();
  ResetTev();
  C3D_TexBind(0, &g.atlas);
  C3D_TexEnv *e = C3D_GetTexEnv(0);
  C3D_TexEnvSrc(e, C3D_Both, GPU_TEXTURE0, GPU_PRIMARY_COLOR, GPU_PREVIOUS);
  C3D_TexEnvFunc(e, C3D_Both, GPU_MODULATE);
  C3D_AlphaTest(true, GPU_GREATER, 0);
  C3D_DepthTest(true, GPU_GREATER, GPU_WRITE_ALL);
  DrawRange(sub * 2);
  C3D_StencilTest(true, GPU_NOTEQUAL, 1, 1, 1);
  C3D_StencilOp(GPU_STENCIL_KEEP, GPU_STENCIL_REPLACE, GPU_STENCIL_REPLACE);
  DrawRange(sub * 2 + 1);
  return true;
}

static void ConfigureCompose(unsigned flags) {
  ResetTev();
  GPU_TEVSRC main = (flags & 4) ? GPU_CONSTANT : GPU_TEXTURE0;
  C3D_TexEnv *e = C3D_GetTexEnv(0);
  C3D_TexEnvColor(e, Rgba(1,1,1,127));
  C3D_TexEnvSrc(e, C3D_RGB, main, GPU_TEXTURE1, GPU_PREVIOUS);
  C3D_TexEnvFunc(e, C3D_RGB, (flags & 1) ? GPU_SUBTRACT : GPU_ADD);
  C3D_TexEnvSrc(e, C3D_Alpha, GPU_TEXTURE1, GPU_CONSTANT, GPU_PREVIOUS);
  C3D_TexEnvFunc(e, C3D_Alpha, GPU_SUBTRACT);
  C3D_TexEnvScale(e, C3D_Alpha, GPU_TEVSCALE_2);
  C3D_TexEnvBufUpdate(C3D_RGB, 1);

  e = C3D_GetTexEnv(1);
  C3D_TexEnvColor(e, Rgba(1,1,1,128));
  if (flags & 1) {
    C3D_TexEnvSrc(e, C3D_RGB, GPU_PREVIOUS, GPU_CONSTANT, GPU_PREVIOUS);
    C3D_TexEnvOpRgb(e, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_ALPHA,
                    GPU_TEVOP_RGB_SRC_COLOR);
    C3D_TexEnvFunc(e, C3D_RGB, GPU_MODULATE);
  } else {
    C3D_TexEnvSrc(e, C3D_RGB, main, GPU_TEXTURE1, GPU_CONSTANT);
    C3D_TexEnvOpRgb(e, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR,
                    GPU_TEVOP_RGB_SRC_ALPHA);
    C3D_TexEnvFunc(e, C3D_RGB, GPU_INTERPOLATE);
  }
  e = C3D_GetTexEnv(2);
  if (flags & 2) {
    C3D_TexEnvSrc(e, C3D_RGB, GPU_PREVIOUS, GPU_PREVIOUS_BUFFER, GPU_PREVIOUS);
    C3D_TexEnvOpRgb(e, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR,
                    GPU_TEVOP_RGB_SRC_ALPHA);
    C3D_TexEnvFunc(e, C3D_RGB, GPU_INTERPOLATE);
  } else {
    C3D_TexEnvSrc(e, C3D_RGB, GPU_PREVIOUS_BUFFER, GPU_PREVIOUS, GPU_PREVIOUS);
    C3D_TexEnvFunc(e, C3D_RGB, GPU_REPLACE);
  }
  e = C3D_GetTexEnv(3);
  unsigned bias = (flags & 1) ? 1 : 0;
  C3D_TexEnvColor(e, Rgba(bias,bias,bias,(flags & 8) ? 255 : 127));
  C3D_TexEnvSrc(e, C3D_RGB, GPU_PREVIOUS, GPU_CONSTANT, GPU_PREVIOUS);
  C3D_TexEnvFunc(e, C3D_RGB, GPU_ADD);
  C3D_TexEnvSrc(e, C3D_Alpha, GPU_TEXTURE0, GPU_CONSTANT, GPU_PREVIOUS);
  C3D_TexEnvFunc(e, C3D_Alpha, GPU_SUBTRACT);
  C3D_TexEnvScale(e, C3D_Alpha, GPU_TEVSCALE_2);
  e = C3D_GetTexEnv(4);
  C3D_TexEnvColor(e, Rgba(1,1,1,255));
  C3D_TexEnvSrc(e, C3D_RGB, GPU_PREVIOUS, main, GPU_PREVIOUS);
  C3D_TexEnvOpRgb(e, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR,
                  GPU_TEVOP_RGB_SRC_ALPHA);
  C3D_TexEnvFunc(e, C3D_RGB, GPU_INTERPOLATE);
  e = C3D_GetTexEnv(5);
  C3D_TexEnvSrc(e, C3D_Alpha, GPU_CONSTANT, GPU_PREVIOUS, GPU_PREVIOUS);
  C3D_TexEnvColor(e, 0xffffffff);
  C3D_TexEnvFunc(e, C3D_Alpha, GPU_REPLACE);
}

static bool DrawComposition(void) {
  C3D_FrameSplit(GX_CMDLIST_FLUSH);
  C3D_RenderTargetClear(g.resultTarget, C3D_CLEAR_COLOR, 0, 0);
  if (!C3D_FrameDrawOn(g.resultTarget)) return false;
  BindState();
  Viewport();
  C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_COLOR);
  C3D_TexBind(0, &g.main);
  C3D_TexBind(1, &g.sub);
  for (unsigned i = 0; i < 16; i++) {
    if (!g.ranges[4 + i].count) continue;
    ConfigureCompose(i);
    DrawRange(4 + i);
  }
  return true;
}

static bool Target(C3D_Tex *texture, C3D_RenderTarget **target,
                   GPU_TEXCOLOR format, bool depth) {
  if (!C3D_TexInitVRAM(texture, 512, 256, format)) return false;
  C3D_TexSetFilter(texture, GPU_NEAREST, GPU_NEAREST);
  C3D_TexSetWrap(texture, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
  *target = C3D_RenderTargetCreateFromTex(texture, GPU_TEXFACE_2D, 0,
                                          depth ? GPU_RB_DEPTH24_STENCIL8 : -1);
  return *target != NULL;
}

static bool SelectBuffers(unsigned slot) {
  g.slot = slot;
  g.atlas = g.atlasPool[slot];
  g.cache = g.cachePool[slot];
  g.vertices = g.vertexPool[slot];
  BufInfo_Init(&g.buffers);
  return BufInfo_Add(&g.buffers, g.vertices, sizeof(Vertex), 3, 0x210) >= 0;
}

bool PpuGpuInit(void) {
  if (g.initialized) return g.ready;
  g.initialized = true;
  g.reason = "initialization";
  g.saved = calloc(1, sizeof(Ppu));
  if (!g.saved) { g.reason = "memory"; return false; }
  for (unsigned slot = 0; slot < 2; slot++) {
    g.cachePool[slot] = calloc(1, sizeof(PicaAtlas));
    g.vertexPool[slot] = linearMemAlign(PICA_MAX_VERTICES * sizeof(Vertex), 128);
    if (!g.cachePool[slot] || !g.vertexPool[slot] ||
        !C3D_TexInit(&g.atlasPool[slot], PICA_ATLAS_W, PICA_ATLAS_H, GPU_RGBA8)) {
      g.reason = "memory";
      return false;
    }
    C3D_TexSetFilter(&g.atlasPool[slot], GPU_NEAREST, GPU_NEAREST);
    C3D_TexSetWrap(&g.atlasPool[slot], GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
    PicaAtlasInit(g.cachePool[slot], g.atlasPool[slot].data);
    if (!Clean(g.atlasPool[slot].data, g.atlasPool[slot].size)) return false;
  }
  if (!SelectBuffers(0) ||
      !Target(&g.main, &g.mainTarget, GPU_RGBA8, true) ||
      !Target(&g.sub, &g.subTarget, GPU_RGBA8, true) ||
      !Target(&g.result, &g.resultTarget, GPU_RGBA5551, false)) {
    g.reason = "targets";
    return false;
  }
  g.shader = DVLB_ParseFile((u32 *)sm_pica_shbin, sm_pica_shbin_size);
  if (!g.shader || !g.shader->numDVLE || R_FAILED(shaderProgramInit(&g.shaderProgram))) {
    g.reason = "shader";
    return false;
  }
  g.program = true;
  if (R_FAILED(shaderProgramSetVsh(&g.shaderProgram, &g.shader->DVLE[0]))) return false;
  g.scaleLocation = shaderInstanceGetUniformLocation(g.shaderProgram.vertexShader,
                                                      "positionScale");
  if (g.scaleLocation < 0) { g.reason = "shader-uniform"; return false; }
  AttrInfo_Init(&g.attributes);
  AttrInfo_AddLoader(&g.attributes, 0, GPU_SHORT, 4);
  AttrInfo_AddLoader(&g.attributes, 1, GPU_SHORT, 2);
  AttrInfo_AddLoader(&g.attributes, 2, GPU_UNSIGNED_BYTE, 4);
  g.ready = true;
  g.reason = "ready";
  return true;
}

void PpuGpuShutdown(void) {
  if (!g.initialized) return;
  if (g.mainTarget) C3D_RenderTargetDelete(g.mainTarget);
  if (g.subTarget) C3D_RenderTargetDelete(g.subTarget);
  if (g.resultTarget) C3D_RenderTargetDelete(g.resultTarget);
  C3D_Tex *textures[] = {&g.atlasPool[0],&g.atlasPool[1],&g.main,&g.sub,&g.result};
  for (unsigned i = 0; i < sizeof(textures) / sizeof(textures[0]); i++)
    if (textures[i]->data) C3D_TexDelete(textures[i]);
  if (g.program) shaderProgramFree(&g.shaderProgram);
  if (g.shader) DVLB_Free(g.shader);
  for (unsigned slot = 0; slot < 2; slot++) {
    if (g.vertexPool[slot]) linearFree(g.vertexPool[slot]);
    free(g.cachePool[slot]);
  }
  free(g.saved);
  memset(&g, 0, sizeof(g));
}

bool PpuGpuCanAttempt(void) {
#ifdef SM3DS_DISABLE_PICA
  return false;
#else
  return g.ready;
#endif
}

void PpuGpuSetWideConfig(WideConfig config) {
  if (config.output_width != (config.enabled ? kWideWidth : kSnesWidth) ||
      config.origin_x != (config.enabled ? kWideExtraX : 0) ||
      config.hud_end_y > kSnesHeight)
    return;
  g_wide_config = config;
}

bool PpuGpuBegin(Ppu *p, unsigned height) {
  g.prepared = g.output = false;
  g.hudLines = 0;
  if (!g.ready || height > PICA_MAX_LINES) return false;
  if (!SelectBuffers(g.lastSubmittedSlot ^ 1u)) {
    g.reason = "vertex-buffer";
    return false;
  }
  memcpy(g.saved, p, sizeof(Ppu));
  g.captured = 0;
  ResetRanges(g_wide_config.output_width, height);
  PicaAtlasBegin(g.cache);
  p->gpuRecording = true;
  p->gpuInvalidWrite = false;
  return true;
}

void PpuGpuLine(const Ppu *p, unsigned y) {
  if (y < g.height) {
    PicaCaptureLine(&g.lines[y], p);
    g.captured++;
  }
}

bool PpuGpuFinish(Ppu *p) {
#ifdef SM3DS_PROFILE
  uint64_t started = SDL_GetPerformanceCounter();
#endif
  p->gpuRecording = false;
  PicaFrame frame = {.memory=g.saved,.lines=g.lines,.atlas=g.cache,
                     .pixels=g.atlas.data,.width=g.width,.height=g.height,
                     .originX=g_wide_config.origin_x,
                     .hudEndY=g_wide_config.hud_end_y,
                     .emit=Emit};
  bool ok = !p->gpuInvalidWrite && g.captured == g.height;
  if (ok) g.hudLines = PicaHudLineCount(&frame);
  if (ok && g.width == kWideWidth) {
    WideWorldSpan span = WideBounds_Compute(
        (int16_t)layer1_x_pos, (int16_t)layer1_y_pos,
        room_width_in_blocks, room_width_in_scrolls,
        room_height_in_scrolls, scrolls);
    g.worldLeft = span.left;
    g.worldRight = span.right;
  }
  if (ok) ok = PicaBuildFrame(&frame);
  if (!ok) {
    g.reason = p->gpuInvalidWrite ? "live-vram-cgram-oam" :
               frame.failure ? frame.failure : "line-count";
    memcpy(p, g.saved, sizeof(Ppu));
    p->gpuRecording = false;
#ifdef SM3DS_PROFILE
    g_profile_pica_prepare_ticks += SDL_GetPerformanceCounter() - started;
#endif
    return false;
  }
  for (unsigned i = 1; i < PICA_SLOTS;) {
    if (!(g.cache->dirty[i / 32] & (1u << (i & 31)))) { i++; continue; }
    unsigned first = i++;
    while (i < PICA_SLOTS && (g.cache->dirty[i / 32] & (1u << (i & 31)))) i++;
    unsigned bytes = (i - first) * 64 * 4;
    if (!Clean((uint8_t *)g.atlas.data + first * 64 * 4, bytes)) ok = false;
    else for (unsigned j = first; j < i; j++)
      g.cache->dirty[j / 32] &= ~(1u << (j & 31));
  }
  if (!Clean(g.vertices, g.count * sizeof(Vertex))) ok = false;
  if (!ok) {
    g.reason = "cache-clean";
    memcpy(p, g.saved, sizeof(Ppu));
    return false;
  }
  g.prepared = g.output = true;
  g.reason = "PICA200";
#ifdef SM3DS_PROFILE
  g_profile_pica_prepare_ticks += SDL_GetPerformanceCounter() - started;
  g_profile_pica_vertices = g.count;
  g_profile_pica_decodes = g.cache->decodes;
#endif
  return true;
}

void PpuGpuCpuFrame(void) {
  g.prepared = g.output = false;
#ifdef SM3DS_PROFILE
  g_profile_pica_cpu_frames++;
  if (g_wide_config.enabled) g_profile_pica_wide_cpu_frames++;
#endif
}

bool PpuGpuPrepared(void) { return g.prepared; }
bool PpuGpuOutputActive(void) { return g.output; }
C3D_Tex *PpuGpuOutput(void) { return g.output ? &g.result : NULL; }
unsigned PpuGpuOutputWidth(void) { return g.output ? g.width : kSnesWidth; }
unsigned PpuGpuHudLines(void) { return g.output ? g.hudLines : 0; }
bool PpuGpuVisibleWorldSpan(int *left, int *right) {
  if (!g.output || g.width != kWideWidth) return false;
  *left = g.worldLeft;
  *right = g.worldRight;
  return true;
}
const char *PpuGpuReason(void) { return g.reason ? g.reason : "uninitialized"; }

bool PpuGpuDraw(void) {
  if (!g.prepared) return g.output;
  bool ok = DrawLayers(g.mainTarget, 0) && DrawLayers(g.subTarget, 1) &&
            DrawComposition();
  RestoreC2D();
  g.prepared = false;
  if (ok) {
    g.lastSubmittedSlot = g.slot;
#ifdef SM3DS_PROFILE
    g_profile_pica_gpu_frames++;
    if (g.width == kWideWidth) g_profile_pica_wide_gpu_frames++;
#endif
  } else {
    g.ready = g.output = false;
    g.reason = "GPU-submit-failed";
  }
  return ok;
}
