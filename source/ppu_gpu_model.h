#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "src/snes/ppu.h"

enum {
  PICA_ATLAS_W = 1024,
  PICA_ATLAS_H = 512,
  PICA_SLOTS = 8192,
  PICA_HASH = 16384,
  PICA_MAX_LINES = 240,
  PICA_MAX_VERTICES = 262140,
  PICA_GROUPS = 21,
};

#ifdef SM3DS_PHASE_DIAG
typedef struct PicaBuildTiming {
  uint64_t bg_main, obj_main, bg_sub, obj_sub, compose;
} PicaBuildTiming;
extern PicaBuildTiming g_pica_build_timing;
#endif

/* Only state that can affect a Mode 1 scanline is copied here.  VRAM,
 * CGRAM and OAM are retained once in PicaFrame::memory. */
typedef struct PicaLine {
  BgLayer bg[3];
  uint16_t objTileAdr1, objTileAdr2;
  uint8_t objSize;
  uint8_t screenEnabled[2], screenWindowed[2];
  uint8_t mosaicEnabled, mosaicSize;
  uint8_t window1left, window1right, window2left, window2right;
  uint32_t windowsel;
  uint8_t windowLogic[6];
  uint8_t clipMode, preventMathMode;
  bool addSubscreen, subtractColor, halfColor;
  uint8_t mathEnabled;
  uint8_t fixedColorR, fixedColorG, fixedColorB;
  bool forcedBlank;
  uint8_t brightness, mode;
  bool bg3priority;
} PicaLine;

typedef struct PicaTile {
  uint32_t key, checked, used;
  uint16_t source[16], palette[16];
  bool valid, opaque;
} PicaTile;

typedef struct PicaAtlas {
  PicaTile tile[PICA_SLOTS];
  uint16_t hash[PICA_HASH];
  uint32_t frame, cursor, hits, decodes, live;
  uint32_t dirty[PICA_SLOTS / 32];
  uint8_t objectColumns[128][PICA_MAX_LINES];
  /* Resolve each BG tile/palette descriptor once per frame. Priority and
   * flips affect the quad, but not the decoded texture. */
  uint16_t bgSlots[3][8192];
  uint16_t bgBase[3];
  uint32_t bgFrame[3];
} PicaAtlas;

typedef struct PicaQuad {
  int16_t x0, y0, x1, y1;
  int16_t u0, v0, u1, v1;
  uint16_t depth;
  uint8_t r, g, b, a;
} PicaQuad;

typedef bool PicaEmit(void *context, unsigned group, const PicaQuad *quad);

/* Room blocks provide side tiles without writing into the SNES's adjacent
 * 256-pixel BG tilemaps. The center keeps using the original VRAM image. */
typedef struct PicaWideRoomLayer {
  const uint16_t *blocks;
  int cameraX, cameraY;
  uint16_t scrollX, scrollY;
  bool followScroll;
} PicaWideRoomLayer;

typedef struct PicaFrame {
  const Ppu *memory;
  const PicaLine *lines;
  PicaAtlas *atlas;
  uint32_t *pixels;
  unsigned width, height;
  unsigned originX, hudEndY;
  int worldLeft, worldRight;
  int bg2Left, bg2Right;
  bool boundBg2;
  bool extendEyeBeam;
  int16_t beamLeft[PICA_MAX_LINES], beamRight[PICA_MAX_LINES];
  const int16_t *objectX;
  const uint8_t *objectXValid;
  PicaWideRoomLayer wideRoom[2];
  const uint16_t *wideTileTable;
  unsigned wideRoomWidth, wideRoomHeight;
  PicaEmit *emit;
  void *context;
  const char *failure;
  uint32_t quads[PICA_GROUPS];
  uint16_t bandEnd[PICA_MAX_LINES];
} PicaFrame;

void PicaAtlasInit(PicaAtlas *atlas, uint32_t *pixels);
void PicaAtlasBegin(PicaAtlas *atlas);
void PicaCaptureLine(PicaLine *out, const Ppu *ppu);
unsigned PicaHudLineCount(const PicaFrame *frame);
bool PicaBuildFrame(PicaFrame *frame);
int PicaScrollOffset(unsigned scroll, unsigned base, bool wider);
