#include "../source/ppu_gpu_model.h"
#include "../source/wide_config.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef struct Capture {
  PicaQuad quads[4096];
  PicaQuad subQuads[16384];
  unsigned count;
  unsigned subCount;
  int objFirstX;
  unsigned objCount;
  bool colorWindowSide;
  bool blackMaskRight;
} Capture;

static bool CaptureQuad(void *context, unsigned group, const PicaQuad *quad) {
  Capture *capture = context;
  if (group == 0) {
    assert(capture->count < 4096);
    capture->quads[capture->count++] = *quad;
  } else if (group == 2) {
    assert(capture->subCount < 16384);
    capture->subQuads[capture->subCount++] = *quad;
  } else if (group == 1) {
    if (!capture->objCount) capture->objFirstX = quad->x0;
    capture->objCount++;
  } else if (group == 8 && quad->x1 > kWideExtraX + kSnesWidth &&
             quad->y0 >= 20) {
    capture->colorWindowSide = true;
  } else if (group == 20 &&
             quad->x0 == kWideExtraX + kSnesWidth &&
             quad->x1 == kWideWidth && quad->y0 == kHudEndLine) {
    capture->blackMaskRight = true;
  }
  return true;
}

static unsigned SampleQuads(const PicaQuad *quads, unsigned count,
                            const PicaAtlas *atlas, unsigned x, unsigned y) {
  const PicaQuad *top = NULL;
  for (unsigned i = 0; i < count; i++) {
    const PicaQuad *q = &quads[i];
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

static unsigned Sample(const Capture *capture, const PicaAtlas *atlas,
                       unsigned x, unsigned y) {
  return SampleQuads(capture->quads, capture->count, atlas, x, y);
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
                         const Capture *wide, const PicaAtlas *wideAtlas,
                         unsigned height) {
  for (unsigned y = 0; y < height; y++)
    for (unsigned x = 0; x < kSnesWidth; x++)
      assert(Sample(normal, normalAtlas, x, y) ==
             Sample(wide, wideAtlas, x + kWideExtraX, y));
}

int main(void) {
  Ppu *ppu = calloc(1, sizeof(*ppu));
  PicaLine lines[kSnesHeight] = {0};
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
  AssertCenter(normal, normalAtlas, wide, wideAtlas, kHudEndLine);
  assert(Sample(wide, wideAtlas, 0, 0) != 0);
  assert(Sample(wide, wideAtlas, kWideWidth - 1, 0) != 0);

  /* A spotlight can move its color window on every scanline. Background
   * layers that do not use that window must keep their tile-height runs. */
  for (unsigned y = 0; y < kHudEndLine; y++)
    lines[y].screenWindowed[0] = 0;
  Build(&frame, wideAtlas, widePixels, wide, kWideWidth, kWideExtraX);
  unsigned unwindowedQuads = wide->count;
  for (unsigned y = 0; y < kHudEndLine; y++) {
    lines[y].window1left = y;
    lines[y].window1right = 100 + y;
    lines[y].windowsel = 2u << 20;
  }
  Build(&frame, wideAtlas, widePixels, wide, kWideWidth, kWideExtraX);
  assert(wide->count == unwindowedQuads);
  for (unsigned y = 0; y < kHudEndLine; y++) {
    lines[y].window1left = 30 + y / 2;
    lines[y].window1right = y < 13 ? 150 + 8 * y : 255;
    lines[y].clipMode = 2;
  }
  Build(&frame, wideAtlas, widePixels, wide, kWideWidth, kWideExtraX);
  assert(!wide->colorWindowSide);
  frame.extendEyeBeam = true;
  Build(&frame, wideAtlas, widePixels, wide, kWideWidth, kWideExtraX);
  assert(wide->colorWindowSide);
  frame.extendEyeBeam = false;
  frame.height = kSnesHeight;
  for (unsigned y = 0; y < kSnesHeight; y++) {
    lines[y].mode = 1;
    lines[y].brightness = 15;
    lines[y].screenEnabled[0] = 0;
    lines[y].screenEnabled[1] = 4;
    lines[y].screenWindowed[1] = 4;
    lines[y].addSubscreen = true;
    lines[y].bg[2] = lines[y].bg[0];
    lines[y].bg[2].tilemapAdr = 0x1000;
    lines[y].windowsel = (2u << 8) | (2u << 20);
    lines[y].window1left = 40 + y / 2;
    lines[y].window1right = y < 88 ? 80 + y * 2 : 255;
  }
  Build(&frame, normalAtlas, normalPixels, normal, kWideWidth, kWideExtraX);
  unsigned beamBaselineQuads = normal->subCount;
  frame.extendEyeBeam = true;
  Build(&frame, wideAtlas, widePixels, wide, kWideWidth, kWideExtraX);
  assert(wide->subCount < beamBaselineQuads / 2);
  assert(SampleQuads(normal->subQuads, normal->subCount, normalAtlas,
                     kWideWidth - 20, 180) !=
         SampleQuads(wide->subQuads, wide->subCount, wideAtlas,
                     kWideWidth - 20, 180));
  for (unsigned y = 16; y < kSnesHeight; y += 7)
    for (unsigned x = kWideExtraX; x < kWideExtraX + kSnesWidth; x += 5)
      assert(SampleQuads(normal->subQuads, normal->subCount, normalAtlas, x, y) ==
             SampleQuads(wide->subQuads, wide->subCount, wideAtlas, x, y));
  BuildWithBounds(&frame, wideAtlas, widePixels, wide, kWideWidth,
                  kWideExtraX, 0, kWideExtraX + kSnesWidth);
  assert(wide->blackMaskRight);
  frame.extendEyeBeam = false;
  frame.height = kHudEndLine;
  for (unsigned y = 0; y < kHudEndLine; y++) {
    lines[y].screenEnabled[0] = 1;
    lines[y].screenEnabled[1] = 0;
    lines[y].screenWindowed[1] = 0;
    lines[y].addSubscreen = false;
  }
  for (unsigned y = 0; y < kHudEndLine; y++) {
    lines[y].screenWindowed[0] = 1;
    lines[y].window1left = 40;
    lines[y].window1right = 100;
    lines[y].windowsel = 2;
    lines[y].clipMode = 0;
  }

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
  frame.boundBg2 = true;
  frame.bg2Left = kWideExtraX;
  frame.bg2Right = kWideExtraX + kSnesWidth;
  BuildWithBounds(&frame, wideAtlas, widePixels, wide, kWideWidth,
                  kWideExtraX, kWideExtraX,
                  kWideExtraX + kSnesWidth);
  assert(Sample(wide, wideAtlas, 0, 0) == 0);
  assert(Sample(wide, wideAtlas, kWideExtraX + 20, 0) == bg1Center);
  frame.bg2Right = kWideWidth;
  BuildWithBounds(&frame, wideAtlas, widePixels, wide, kWideWidth,
                  kWideExtraX, kWideExtraX,
                  kWideExtraX + kSnesWidth);
  assert(Sample(wide, wideAtlas, 0, 0) == 0);
  assert(Sample(wide, wideAtlas, kWideWidth - 1, 0) != 0);
  frame.boundBg2 = false;
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
  AssertCenter(normal, normalAtlas, wide, wideAtlas, kHudEndLine);

  for (unsigned y = 0; y < kHudEndLine; y++) {
    lines[y].screenEnabled[0] = 2;
    lines[y].screenWindowed[0] = 2;
    lines[y].windowsel = 2u << 4;
    lines[y].bg[1] = lines[y].bg[0];
    lines[y].bg[1].hScroll = 29;
  }
  Build(&frame, normalAtlas, normalPixels, normal, kSnesWidth, 0);
  Build(&frame, wideAtlas, widePixels, wide, kWideWidth, kWideExtraX);
  AssertCenter(normal, normalAtlas, wide, wideAtlas, kHudEndLine);

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
  AssertCenter(normal, normalAtlas, wide, wideAtlas, kHudEndLine);
  assert(Sample(wide, wideAtlas, 0, 0) == 0);
  assert(Sample(wide, wideAtlas, kWideWidth - 1, 0) == 0);

  for (unsigned i = 0; i < 2048; i++) ppu->vram[0x5000 + i] = 2;
  for (unsigned y = 0; y < kHudEndLine; y++)
    lines[y].bg[2].tilemapAdr = 0x5000;
  assert(PicaHudLineCount(&frame) == 0);
  Build(&frame, normalAtlas, normalPixels, normal, kSnesWidth, 0);
  Build(&frame, wideAtlas, widePixels, wide, kWideWidth, kWideExtraX);
  AssertCenter(normal, normalAtlas, wide, wideAtlas, kHudEndLine);
  assert(Sample(wide, wideAtlas, 0, 0) != 0);
  assert(Sample(wide, wideAtlas, kWideWidth - 1, 0) != 0);

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
  /* OAM X=312 can mean either a real right-side sprite or a sprite at -200
   * that wrapped at 512. Preserve the producer's signed coordinate. */
  ppu->oam[0] = 56;
  int16_t unwrappedX[128] = {-200};
  uint8_t unwrappedValid[128] = {1};
  frame.objectX = unwrappedX;
  frame.objectXValid = unwrappedValid;
  Build(&frame, wideAtlas, widePixels, wide, kWideWidth, kWideExtraX);
  assert(wide->objCount == 0);
  unwrappedX[0] = 312;
  Build(&frame, wideAtlas, widePixels, wide, kWideWidth, kWideExtraX);
  assert(wide->objCount && wide->objFirstX == 312 + kWideExtraX);
  frame.objectX = NULL;
  frame.objectXValid = NULL;
  /* Cover all gameplay scanlines, including independently scrolled BG2 and
   * a window that changes halfway down the screen. */
  memset(ppu->highOam, 0, sizeof(ppu->highOam));
  for (unsigned i = 0; i < 256; i += 2) ppu->oam[i] = 0xf000;
  for (unsigned y = 0; y < kSnesHeight; y++) {
    memset(&lines[y], 0, sizeof(lines[y]));
    lines[y].mode = 1;
    lines[y].brightness = 15;
    lines[y].screenEnabled[0] = 3;
    lines[y].bg[0].tilemapAdr = 0x1000;
    lines[y].bg[0].tilemapWider = true;
    lines[y].bg[0].hScroll = 13 + y / 32;
    lines[y].bg[1] = lines[y].bg[0];
    lines[y].bg[1].tilemapAdr = 0x1800;
    lines[y].bg[1].hScroll = 29 + y / 48;
    if (y >= 80 && y < 160) {
      lines[y].screenWindowed[0] = 2;
      lines[y].windowsel = 2u << 4;
      lines[y].window1left = 48;
      lines[y].window1right = 112;
    }
  }
  frame.height = kSnesHeight;
  Build(&frame, normalAtlas, normalPixels, normal, kSnesWidth, 0);
  Build(&frame, wideAtlas, widePixels, wide, kWideWidth, kWideExtraX);
  AssertCenter(normal, normalAtlas, wide, wideAtlas, kSnesHeight);

  /* Virtual room tiles occupy only the side bands. A 256-pixel SNES tilemap
   * must not be widened by writing into the next background's VRAM. */
  uint16_t *roomBlocks = calloc(32 * 16, sizeof(*roomBlocks));
  uint16_t *roomTiles = calloc(1024 * 4, sizeof(*roomTiles));
  assert(roomBlocks && roomTiles);
  for (unsigned i = 0; i < 32 * 16; i++) roomBlocks[i] = 1;
  for (unsigned i = 0; i < 4; i++) roomTiles[4 + i] = 6;
  for (unsigned i = 0; i < 16; i++) {
    ppu->vram[5 * 16 + i] = 0xffff;
    ppu->vram[6 * 16 + i] = 0xffff;
  }
  for (unsigned i = 0; i < 1024; i++) ppu->vram[0x1000 + i] = 5;
  for (unsigned y = 0; y < 40; y++) {
    memset(&lines[y], 0, sizeof(lines[y]));
    lines[y].mode = 1;
    lines[y].brightness = 15;
    lines[y].screenEnabled[0] = 1;
    lines[y].bg[0].tilemapAdr = 0x1000;
  }
  frame.height = 40;
  frame.wideRoom[0] = (PicaWideRoomLayer){roomBlocks, kWideExtraX, 0};
  frame.wideTileTable = roomTiles;
  frame.wideRoomWidth = 32;
  frame.wideRoomHeight = 16;
  Build(&frame, wideAtlas, widePixels, wide, kWideWidth, kWideExtraX);
  unsigned sidePixel = Sample(wide, wideAtlas, 0, 33);
  unsigned centerPixel = Sample(wide, wideAtlas, kWideExtraX + 40, 33);
  assert(sidePixel != 0 && centerPixel != 0);
  assert(sidePixel != centerPixel);
  assert(Sample(wide, wideAtlas, kWideWidth - 8, 33) == sidePixel);

  /* Gameplay BG3 must stop at the physical room edge as well. Otherwise
   * its wrapped 256-pixel tilemap repeats past a doorway. */
  for (unsigned y = 32; y < 40; y++) {
    lines[y].screenEnabled[0] = 4;
    lines[y].bg[2].tilemapAdr = 0x2000;
  }
  for (unsigned i = 0; i < 1024; i++) ppu->vram[0x2000 + i] = 5;
  frame.boundBg2 = true;
  frame.bg2Left = kWideExtraX;
  frame.bg2Right = kWideExtraX + kSnesWidth;
  Build(&frame, wideAtlas, widePixels, wide, kWideWidth, kWideExtraX);
  assert(Sample(wide, wideAtlas, 0, 33) == 0);
  assert(Sample(wide, wideAtlas, kWideExtraX + 40, 33) != 0);
  assert(Sample(wide, wideAtlas, kWideWidth - 1, 33) == 0);
  free(roomTiles);
  free(roomBlocks);

  free(wide);
  free(normal);
  free(widePixels);
  free(normalPixels);
  free(wideAtlas);
  free(normalAtlas);
  free(ppu);
  return 0;
}
