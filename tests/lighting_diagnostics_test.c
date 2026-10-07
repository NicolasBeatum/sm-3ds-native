#include "scene_lighting.h"
#include "wide_config.h"
#include <assert.h>
#include <stdlib.h>

#ifndef SM3DS_PHASE_DIAG
#error This test requires SM3DS_PHASE_DIAG
#endif

uint64_t SDL_GetPerformanceCounter(void) {
  static uint64_t ticks;
  return ++ticks;
}

static unsigned shadow_quads;
static bool Capture(void *context, unsigned group, const PicaQuad *q) {
  (void)context; (void)q;
  if (group == 21 || group == 22) shadow_quads++;
  return true;
}

int main(void) {
  Ppu *ppu = calloc(1, sizeof(*ppu));
  PicaAtlas *atlas = calloc(1, sizeof(*atlas));
  uint32_t *pixels = calloc(PICA_ATLAS_W * PICA_ATLAS_H, sizeof(*pixels));
  assert(ppu && atlas && pixels);
  PicaLine lines[kSnesHeight] = {0};
  for (unsigned y = 0; y < kSnesHeight; y++) {
    lines[y].mode = 1; lines[y].brightness = 15;
    lines[y].screenEnabled[0] = lines[y].screenEnabled[1] = 17;
    lines[y].bg[0].tilemapAdr = 0x1000;
  }
  for (unsigned i = 0; i < 16; i++) ppu->vram[i] = 0xffff;
  for (unsigned i = 0; i < 256; i += 2) ppu->oam[i] = 0xf000;
  ppu->oam[0] = (100 << 8) | 100;
  ppu->oam[1] = 4 << 9; /* Samus palette, at the fill-light position. */
  SceneLighting lighting = {0};
  PicaFrame frame = {.memory=ppu, .lines=lines, .atlas=atlas, .pixels=pixels,
      .width=256, .height=kSnesHeight, .hudEndY=kHudEndLine, .worldRight=256,
      .lighting=&lighting, .emit=Capture};
  PicaAtlasInit(atlas, pixels);
  for (unsigned pass = 0; pass < 2; pass++) {
    PicaAtlasBegin(atlas); shadow_quads = 0;
    SceneLighting_Begin(&lighting, true, 0x91f8, 8, 0, 0, 100, 100, 0, 0);
    assert(PicaBuildFrame(&frame));
    assert(g_pica_build_timing.lighting_applied);
    assert(g_pica_build_timing.light_sources == 4);
    assert(!g_pica_build_timing.background_sources);
    assert(g_pica_build_timing.light_field_rebuilt == (pass == 0));
    assert(g_pica_build_timing.light_prepare > 0);
    assert(g_pica_build_timing.shadow_submit > 0);
    assert(shadow_quads && g_pica_build_timing.shadow_quads == shadow_quads);
  }
  SceneLighting_Begin(&lighting, false, 0x91f8, 8, 0, 0, 100, 100, 0, 0);
  PicaAtlasBegin(atlas);
  assert(PicaBuildFrame(&frame));
  assert(!g_pica_build_timing.lighting_applied);
  assert(!g_pica_build_timing.light_sources && !g_pica_build_timing.shadow_quads);
  assert(!g_pica_build_timing.light_prepare && !g_pica_build_timing.shadow_submit);
  assert(!g_pica_build_timing.light_field_rebuilt);
  /* Special effects and a rejected model must clear previous frame metrics. */
  SceneLighting_Begin(&lighting, true, 0x91f8, 8, 0, 0, 100, 100, 0, 0);
  frame.extendEyeBeam = true;
  assert(PicaBuildFrame(&frame) && !g_pica_build_timing.lighting_applied);
  frame.extendEyeBeam = false; frame.width = 1;
  assert(!PicaBuildFrame(&frame));
  assert(!g_pica_build_timing.light_sources && !g_pica_build_timing.light_prepare);
  free(ppu); free(atlas); free(pixels);
  return 0;
}
