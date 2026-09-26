#include "../source/ppu_gpu_model.h"
#include "../source/wide_config.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef struct Capture {
  PicaQuad quads[4096];
  unsigned count;
  int objFirstX;
  unsigned objCount;
} Capture;

static bool CaptureQuad(void *context, unsigned group, const PicaQuad *quad) {
  Capture *capture = context;
  if (group == 0) {
    assert(capture->count < 4096);
    capture->quads[capture->count++] = *quad;
  } else if (group == 1) {
    if (!capture->objCount) capture->objFirstX = quad->x0;
    capture->objCount++;
  }
  return true;
}

static unsigned Sample(const Capture *capture, const PicaAtlas *atlas,
                       unsigned x, unsigned y) {
  const PicaQuad *top = NULL;
  for (unsigned i = 0; i < capture->count; i++) {
    const PicaQuad *q = &capture->quads[i];
    if ((int)x >= q->x0 && (int)x < q->x1 &&
        (int)y >= q->y0 && (int)y < q->y1 &&
        (!top || q->depth > top->depth))
      top = q;
  }
  assert(top);
  if (top->depth == 1) return 0;
  unsigned u = (top->u0 + ((int)x - top->x0) *
                (top->u1 - top->u0) / (top->x1 - top->x0)) / 4;
  unsigned v = (4096 - (top->v0 + ((int)y - top->y0) *
                (top->v1 - top->v0) / (top->y1 - top->y0))) / 8;
  unsigned slot = (v / 8) * (PICA_ATLAS_W / 8) + u / 8;
  assert(slot < PICA_SLOTS);
  return 1 + (atlas->tile[slot].key << 6) + (v % 8) * 8 + u % 8;
}

static void BuildWithBounds(PicaFrame *frame, PicaAtlas *atlas,
                            uint32_t *pixels, Capture *capture,
                            unsigned width, unsigned origin,
                            int worldLeft, int worldRight) {
  memset(capture, 0, sizeof(*capture));
  PicaAtlasInit(atlas, pixels);
  PicaAtlasBegin(atlas);
  frame->atlas = atlas;
  frame->pixels = pixels;
  frame->width = width;
  frame->originX = origin;
  frame->hudEndY = kHudEndLine;
  frame->worldLeft = worldLeft;
  frame->worldRight = worldRight;
  frame->emit = CaptureQuad;
  frame->context = capture;
  assert(PicaBuildFrame(frame));
}

static void Build(PicaFrame *frame, PicaAtlas *atlas, uint32_t *pixels,
                  Capture *capture, unsigned width, unsigned origin) {
  BuildWithBounds(frame, atlas, pixels, capture, width, origin, 0, width);
}

static void AssertCenter(const Capture *normal, const PicaAtlas *normalAtlas,
                         const Capture *wide, const PicaAtlas *wideAtlas) {
  for (unsigned y = 0; y < kHudEndLine; y++)
    for (unsigned x = 0; x < kSnesWidth; x++)
      assert(Sample(normal, normalAtlas, x, y) ==
             Sample(wide, wideAtlas, x + kWideExtraX, y));
}

int main(void) {
  Ppu *ppu = calloc(1, sizeof(*ppu));
  PicaLine lines[kHudEndLine] = {0};
  PicaAtlas *normalAtlas = calloc(1, sizeof(*normalAtlas));
  PicaAtlas *wideAtlas = calloc(1, sizeof(*wideAtlas));
  uint32_t *normalPixels = calloc(PICA_ATLAS_W * PICA_ATLAS_H, 4);
  uint32_t *widePixels = calloc(PICA_ATLAS_W * PICA_ATLAS_H, 4);
  Capture *normal = calloc(1, sizeof(*normal));
  Capture *wide = calloc(1, sizeof(*wide));
  assert(ppu && normalAtlas && wideAtlas && normalPixels && widePixels &&
         normal && wide);

  for (unsigned i = 0; i < 6; i++) ppu->mathEnabled[i] = 1u << i;
  PicaLine captured;
  PicaCaptureLine(&captured, ppu);
  assert(captured.mathEnabled == 0x3f);
  memset(ppu->mathEnabled, 0, sizeof(ppu->mathEnabled));

  for (unsigned i = 0; i < 2048; i++)
    ppu->vram[0x1000 + i] = 1 + (i & 3);
  for (unsigned i = 16; i < 96; i++)
    ppu->vram[i] = 0xffff;
  for (unsigned y = 0; y < kHudEndLine; y++) {
    lines[y].mode = 1;
    lines[y].brightness = 15;
    lines[y].screenEnabled[0] = 1;
    lines[y].bg[0].tilemapAdr = 0x1000;
    lines[y].bg[0].tilemapWider = true;
    lines[y].bg[0].hScroll = 13;
    lines[y].screenWindowed[0] = 1;
    lines[y].windowsel = 2;
    lines[y].window1left = 40;
    lines[y].window1right = 100;
  }
  PicaFrame frame = {.memory = ppu, .lines = lines, .height = kHudEndLine};
  Build(&frame, normalAtlas, normalPixels, normal, kSnesWidth, 0);
  Build(&frame, wideAtlas, widePixels, wide, kWideWidth, kWideExtraX);
  AssertCenter(normal, normalAtlas, wide, wideAtlas);
  assert(Sample(wide, wideAtlas, 0, 0) != 0);
  assert(Sample(wide, wideAtlas, kWideWidth - 1, 0) != 0);

  /* Room limits apply to BG1. BG2 remains visible behind the side walls. */
  for (unsigned i = 0; i < 2048; i++) ppu->vram[0x1800 + i] = 5;
  for (unsigned y = 0; y < kHudEndLine; y++) {
    lines[y].screenEnabled[0] = 3;
    lines[y].screenWindowed[0] = 0;
    lines[y].bg[1] = lines[y].bg[0];
    lines[y].bg[1].tilemapAdr = 0x1800;
  }
  BuildWithBounds(&frame, wideAtlas, widePixels, wide, kWideWidth,
                  kWideExtraX, kWideExtraX,
                  kWideExtraX + kSnesWidth);
  unsigned bg2Edge = Sample(wide, wideAtlas, 0, 0);
  unsigned bg1Center = Sample(wide, wideAtlas, kWideExtraX + 20, 0);
  assert(bg2Edge != 0);
  for (unsigned y = 0; y < kHudEndLine; y++)
    lines[y].screenEnabled[0] = 2;
  Build(&frame, wideAtlas, widePixels, wide, kWideWidth, kWideExtraX);
  assert(Sample(wide, wideAtlas, 0, 0) == bg2Edge);
  assert(Sample(wide, wideAtlas, kWideExtraX + 20, 0) != bg1Center);

  for (unsigned y = 0; y < kHudEndLine; y++) {
    lines[y].screenEnabled[0] = 1;
    lines[y].screenWindowed[0] = 1;
  }

  for (unsigned y = 0; y < kHudEndLine; y++)
    lines[y].windowsel = 3;
  Build(&frame, normalAtlas, normalPixels, normal, kSnesWidth, 0);
  Build(&frame, wideAtlas, widePixels, wide, kWideWidth, kWideExtraX);
  AssertCenter(normal, normalAtlas, wide, wideAtlas);

  for (unsigned y = 0; y < kHudEndLine; y++) {
    lines[y].screenEnabled[0] = 2;
    lines[y].screenWindowed[0] = 2;
    lines[y].windowsel = 2u << 4;
    lines[y].bg[1] = lines[y].bg[0];
    lines[y].bg[1].hScroll = 29;
  }
  Build(&frame, normalAtlas, normalPixels, normal, kSnesWidth, 0);
  Build(&frame, wideAtlas, widePixels, wide, kWideWidth, kWideExtraX);
  AssertCenter(normal, normalAtlas, wide, wideAtlas);

  for (unsigned i = 0; i < 2048; i++)
    ppu->vram[0x5800 + i] = 1 + (i & 3);
  for (unsigned y = 0; y < kHudEndLine; y++) {
    lines[y].screenEnabled[0] = 4;
    lines[y].screenWindowed[0] = 0;
    lines[y].windowsel = 0;
    lines[y].bg[2] = lines[y].bg[0];
    lines[y].bg[2].tilemapAdr = 0x5800;
  }
  assert(PicaHudLineCount(&frame) == kHudEndLine);
  Build(&frame, normalAtlas, normalPixels, normal, kSnesWidth, 0);
  Build(&frame, wideAtlas, widePixels, wide, kWideWidth, kWideExtraX);
  AssertCenter(normal, normalAtlas, wide, wideAtlas);
  assert(Sample(wide, wideAtlas, 0, 0) == 0);
  assert(Sample(wide, wideAtlas, kWideWidth - 1, 0) == 0);

  for (unsigned i = 0; i < 256; i += 2)
    ppu->oam[i] = 0xf000;
  ppu->oam[0] = 4;
  for (unsigned i = 0; i < 16; i++)
    ppu->vram[i] = 0xffff;
  for (unsigned y = 0; y < kHudEndLine; y++) {
    lines[y].screenEnabled[0] = 16;
    lines[y].screenWindowed[0] = 0;
  }
  Build(&frame, normalAtlas, normalPixels, normal, kSnesWidth, 0);
  Build(&frame, wideAtlas, widePixels, wide, kWideWidth, kWideExtraX);
  assert(normal->objCount && wide->objCount);
  assert(normal->objFirstX == 4);
  assert(wide->objFirstX == 4 + kWideExtraX);

  ppu->highOam[0] = 1;
  Build(&frame, normalAtlas, normalPixels, normal, kSnesWidth, 0);
  Build(&frame, wideAtlas, widePixels, wide, kWideWidth, kWideExtraX);
  assert(normal->objCount == 0);
  assert(wide->objCount);
  assert(wide->objFirstX == 260 + kWideExtraX);
  free(wide);
  free(normal);
  free(widePixels);
  free(normalPixels);
  free(wideAtlas);
  free(normalAtlas);
  free(ppu);
  return 0;
}
