#include "gpu_presenter.h"
#include "ppu_gpu.h"
#include "bottom_screen.h"
#include "wide_config.h"
#include "video_layout.h"
#include "src/ida_types.h"
#include "src/variables.h"

#include <3ds.h>
#include <citro2d.h>
#include <stddef.h>

enum {
  kTextureWidth = 256,
  kTextureHeight = 256,
  kSourceWidth = kSnesWidth,
  kSourceHeight = kSnesHeight,
};

static C3D_RenderTarget *g_top_target;
static C3D_RenderTarget *g_bottom_target;
static C3D_Tex g_top_texture;
static C3D_Tex g_bottom_texture;
static Tex3DS_SubTexture g_top_subtexture;
static Tex3DS_SubTexture g_top_gpu_subtexture;
static Tex3DS_SubTexture g_bottom_subtexture;
static bool g_initialized;
static bool g_frame_active;

/* PPU pixels are stored as little-endian 0x00RRGGBB. The display transfer
 * sees that byte layout as an ARGB texture, so map G/B/A to R/G/B exactly as
 * the reference 3DS port does. */
static void ConfigureArgbTextureEnv(void) {
  C3D_TexEnv *env = C3D_GetTexEnv(0);
  C3D_TexEnvInit(env);
  C3D_TexEnvSrc(env, C3D_RGB, GPU_TEXTURE0, GPU_CONSTANT, GPU_PREVIOUS);
  C3D_TexEnvOpRgb(env, GPU_TEVOP_RGB_SRC_G,
                  GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR);
  C3D_TexEnvFunc(env, C3D_RGB, GPU_MODULATE);
  C3D_TexEnvSrc(env, C3D_Alpha, GPU_CONSTANT, GPU_CONSTANT, GPU_CONSTANT);
  C3D_TexEnvFunc(env, C3D_Alpha, GPU_REPLACE);
  C3D_TexEnvColor(env, C2D_Color32(255, 0, 0, 255));

  env = C3D_GetTexEnv(1);
  C3D_TexEnvInit(env);
  C3D_TexEnvSrc(env, C3D_RGB, GPU_TEXTURE0, GPU_CONSTANT, GPU_PREVIOUS);
  C3D_TexEnvOpRgb(env, GPU_TEVOP_RGB_SRC_B,
                  GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR);
  C3D_TexEnvFunc(env, C3D_RGB, GPU_MULTIPLY_ADD);
  C3D_TexEnvColor(env, C2D_Color32(0, 255, 0, 255));

  env = C3D_GetTexEnv(2);
  C3D_TexEnvInit(env);
  C3D_TexEnvSrc(env, C3D_RGB, GPU_TEXTURE0, GPU_CONSTANT, GPU_PREVIOUS);
  C3D_TexEnvOpRgb(env, GPU_TEVOP_RGB_SRC_ALPHA,
                  GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR);
  C3D_TexEnvFunc(env, C3D_RGB, GPU_MULTIPLY_ADD);
  C3D_TexEnvColor(env, C2D_Color32(0, 0, 255, 255));
}

static void CleanDataCache(const void *address, size_t size) {
  Result result = svcStoreProcessDataCache(
      CUR_PROCESS_HANDLE, (u32)(uintptr_t)address, (u32)size);
  if (R_FAILED(result))
    GSPGPU_FlushDataCache(address, size);
}

bool GpuPresenter_Init(void) {
  if (!C3D_Init(C3D_DEFAULT_CMDBUF_SIZE))
    return false;
  if (!C2D_Init(C2D_DEFAULT_MAX_OBJECTS)) {
    C3D_Fini();
    return false;
  }
  C2D_Prepare();
  C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_COLOR);

  if (!C3D_TexInitVRAM(&g_top_texture, kTextureWidth, kTextureHeight,
                       GPU_RGBA8)) {
    C2D_Fini();
    C3D_Fini();
    return false;
  }
  C3D_TexSetFilter(&g_top_texture, GPU_NEAREST, GPU_NEAREST);
  C3D_TexSetWrap(&g_top_texture, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);

  g_top_target = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, -1);
  if (!g_top_target) {
    C3D_TexDelete(&g_top_texture);
    C2D_Fini();
    C3D_Fini();
    return false;
  }
  C3D_RenderTargetSetOutput(
      g_top_target, GFX_TOP, GFX_LEFT,
      GX_TRANSFER_FLIP_VERT(0) |
      GX_TRANSFER_OUT_TILED(0) |
      GX_TRANSFER_RAW_COPY(0) |
      GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) |
      GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGBA8) |
      GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO));

  if (!C3D_TexInitVRAM(&g_bottom_texture, 512, 256, GPU_RGBA8)) {
    C3D_RenderTargetDelete(g_top_target);
    C3D_TexDelete(&g_top_texture);
    C2D_Fini();
    C3D_Fini();
    return false;
  }
  C3D_TexSetFilter(&g_bottom_texture, GPU_NEAREST, GPU_NEAREST);
  C3D_TexSetWrap(&g_bottom_texture, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
  g_bottom_target = C3D_RenderTargetCreate(240, 320, GPU_RB_RGBA8, -1);
  if (!g_bottom_target) {
    C3D_TexDelete(&g_bottom_texture);
    C3D_RenderTargetDelete(g_top_target);
    C3D_TexDelete(&g_top_texture);
    C2D_Fini();
    C3D_Fini();
    return false;
  }
  C3D_RenderTargetSetOutput(
      g_bottom_target, GFX_BOTTOM, GFX_LEFT,
      GX_TRANSFER_FLIP_VERT(0) |
      GX_TRANSFER_OUT_TILED(0) |
      GX_TRANSFER_RAW_COPY(0) |
      GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) |
      GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGBA8) |
      GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO));

  g_top_subtexture = (Tex3DS_SubTexture){
      .width = kSourceWidth,
      .height = kSourceHeight,
      .left = 0.0f,
      .top = 1.0f,
      .right = 1.0f,
      .bottom = 1.0f - (float)kSourceHeight / kTextureHeight,
  };
  g_top_gpu_subtexture = (Tex3DS_SubTexture){
      .width = kSourceWidth,
      .height = kSourceHeight,
      .left = 0.0f,
      .top = 1.0f,
      .right = (float)kSourceWidth / 512.0f,
      .bottom = 1.0f - (float)kSourceHeight / 256.0f,
  };
  g_bottom_subtexture = (Tex3DS_SubTexture){
      .width = 320,
      .height = 240,
      .left = 0.0f,
      .top = 1.0f,
      .right = 320.0f / 512.0f,
      .bottom = 1.0f - 240.0f / 256.0f,
  };
  PpuGpuInit();
  g_initialized = true;
  return true;
}

bool GpuPresenter_DrawBottom(const uint8_t *pixels, bool upload) {
  if (!g_frame_active || !pixels)
    return false;
  if (upload) {
    CleanDataCache(pixels, 512 * 256 * sizeof(uint32_t));
    C3D_SyncDisplayTransfer(
        (u32 *)pixels, GX_BUFFER_DIM(512, 256),
        (u32 *)g_bottom_texture.data, GX_BUFFER_DIM(512, 256),
        GX_TRANSFER_FLIP_VERT(0) |
        GX_TRANSFER_OUT_TILED(1) |
        GX_TRANSFER_RAW_COPY(0) |
        GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) |
        GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGBA8) |
        GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO));
  }

  C2D_Image image = {
      .tex = &g_bottom_texture,
      .subtex = &g_bottom_subtexture,
  };
  C2D_DrawParams params = {
      .pos = {.x = 0.0f, .y = 0.0f, .w = 320.0f, .h = 240.0f},
      .center = {0.0f, 0.0f},
      .depth = 0.0f,
      .angle = 0.0f,
  };
  C2D_Flush();
  C2D_TargetClear(g_bottom_target, C2D_Color32(0, 0, 0, 255));
  C2D_SceneBegin(g_bottom_target);
  if (!C2D_DrawImage(image, &params, NULL))
    return false;
  ConfigureArgbTextureEnv();
  C2D_Flush();
  return true;
}

bool GpuPresenter_DrawTop(const uint8_t *pixels) {
  if (!g_initialized || !pixels || !C3D_FrameBegin(0))
    return false;
  g_frame_active = true;

  bool gpu_ppu = PpuGpuOutputActive();
  if (gpu_ppu && PpuGpuPrepared())
    gpu_ppu = PpuGpuDraw();
  bool wide = gpu_ppu && PpuGpuOutputWidth() == kWideWidth;

  if (!gpu_ppu) {
    CleanDataCache(pixels, kTextureWidth * kTextureHeight * sizeof(uint32_t));
    C3D_SyncDisplayTransfer(
        (u32 *)pixels, GX_BUFFER_DIM(kTextureWidth, kTextureHeight),
        (u32 *)g_top_texture.data, GX_BUFFER_DIM(kTextureWidth, kTextureHeight),
        GX_TRANSFER_FLIP_VERT(0) |
        GX_TRANSFER_OUT_TILED(1) |
        GX_TRANSFER_RAW_COPY(0) |
        GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) |
        GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGBA8) |
        GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO));
  }

  C2D_Image image = {
      .tex = gpu_ppu ? PpuGpuOutput() : &g_top_texture,
      .subtex = gpu_ppu ? &g_top_gpu_subtexture : &g_top_subtexture,
  };
  TopVideoLayout layout = TopVideoLayout_Get(BottomScreen_VideoMode(), wide);
  Tex3DS_SubTexture sampled = *image.subtex;
  float textureWidth = gpu_ppu ? 512.0f : 256.0f;
  sampled.width = layout.source_width;
  sampled.left = (layout.source_x + TOP_SAMPLE_BIAS) / textureWidth;
  sampled.right = (layout.source_x + layout.source_width + TOP_SAMPLE_BIAS) / textureWidth;
  sampled.top -= TOP_SAMPLE_BIAS / 256.0f;
  sampled.bottom -= TOP_SAMPLE_BIAS / 256.0f;
  image.subtex = &sampled;
  C2D_DrawParams params = {
      .pos = {
          .x = layout.x,
          .y = layout.y,
          .w = layout.width,
          .h = layout.height,
      },
      .center = {0.0f, 0.0f},
      .depth = 0.0f,
      .angle = 0.0f,
  };

  C2D_TargetClear(g_top_target, C2D_Color32(0, 0, 0, 255));
  C2D_SceneBegin(g_top_target);
  C2D_Flush();
  if (!C2D_DrawImage(image, &params, NULL)) {
    GpuPresenter_EndFrame();
    return false;
  }
  if (!gpu_ppu)
    ConfigureArgbTextureEnv();
  C2D_Flush();
  unsigned hudLines = gpu_ppu ? PpuGpuHudLines() :
      (game_state >= kGameState_7_MainGameplayFadeIn &&
       game_state <= kGameState_11_LoadingNextRoom ? kHudEndLine : 0);
  const float hudHeight =
      (float)((hudLines * layout.height + kSnesHeight - 1) / kSnesHeight);
  const u32 black = C2D_Color32(0, 0, 0, 255);
  if (BottomScreen_HideMainHud() && hudLines) {
    C2D_DrawRectSolid(layout.x, layout.y, 0.1f, layout.width,
                      hudHeight, black);
    C2D_Flush();
  } else if (wide && hudLines) {
    /* The rounded-up mask covers one row beyond the image's HUD boundary.
     * Leave the first widened playfield row visible at both sides. */
    const float sideHudHeight = hudHeight > 0.0f ? hudHeight - 1.0f : 0.0f;
    int left = layout.x + ((kWideExtraX - layout.source_x) * layout.width +
                           layout.source_width / 2) / layout.source_width;
    int right = layout.x + ((kWideExtraX + kSnesWidth - layout.source_x) * layout.width +
                            layout.source_width / 2) / layout.source_width;
    C2D_DrawRectSolid(layout.x, layout.y, 0.1f, left - layout.x, sideHudHeight, black);
    C2D_DrawRectSolid(right, layout.y, 0.1f,
                      layout.x + layout.width - right, sideHudHeight, black);
    C2D_Flush();
  }
  return true;
}

void GpuPresenter_EndFrame(void) {
  if (!g_frame_active)
    return;
  C2D_Flush();
  C3D_FrameEnd(0);
  g_frame_active = false;
}

void GpuPresenter_Fini(void) {
  if (!g_initialized)
    return;
  GpuPresenter_EndFrame();
  /* Render-target deletion and C3D_Fini already drain the GPU queue. The
   * extra VBlank wait can stall indefinitely while HOME closes the app. */
  PpuGpuShutdown();
  C3D_RenderTargetDelete(g_bottom_target);
  C3D_TexDelete(&g_bottom_texture);
  C3D_RenderTargetDelete(g_top_target);
  C3D_TexDelete(&g_top_texture);
  C2D_Fini();
  C3D_Fini();
  g_top_target = NULL;
  g_bottom_target = NULL;
  g_initialized = false;
}
