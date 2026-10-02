#include "ppu_gpu_model.h"
#include "wide_config.h"

#include <stddef.h>
#include <string.h>
#ifdef SM3DS_PHASE_DIAG
#include "SDL2/SDL.h"
PicaBuildTiming g_pica_build_timing;
#endif

static const uint8_t kMorton[64] = {
    0,1,4,5,16,17,20,21,2,3,6,7,18,19,22,23,
    8,9,12,13,24,25,28,29,10,11,14,15,26,27,30,31,
    32,33,36,37,48,49,52,53,34,35,38,39,50,51,54,55,
    40,41,44,45,56,57,60,61,42,43,46,47,58,59,62,63,
};
static const uint8_t kSpriteSizes[8][2] = {
    {8,16},{8,32},{8,64},{16,32},{16,64},{32,64},{16,32},{16,32},
};

typedef struct WindowSpans {
  int16_t edges[6];
  uint8_t nr, bits;
} WindowSpans;

static unsigned Min(unsigned a, unsigned b) { return a < b ? a : b; }
static int IMin(int a, int b) { return a < b ? a : b; }
static int IMax(int a, int b) { return a > b ? a : b; }
static uint32_t Hash(uint32_t key) { return (key * 2654435761u) >> 18; }
static uint32_t Encode(uint16_t rgb, unsigned alpha) {
  return (((rgb & 31) * 8 + 1) << 24) |
         ((((rgb >> 5) & 31) * 8 + 1) << 16) |
         ((((rgb >> 10) & 31) * 8 + 1) << 8) | alpha;
}

void PicaAtlasInit(PicaAtlas *a, uint32_t *pixels) {
  memset(a, 0, sizeof(*a));
  memset(pixels, 0, PICA_ATLAS_W * PICA_ATLAS_H * 4);
  for (unsigned i = 0; i < 64; i++) pixels[i] = 0xffffffffu;
  a->tile[0].valid = true;
  a->tile[0].opaque = true;
  a->dirty[0] = 1;
}

void PicaAtlasBegin(PicaAtlas *a) {
  if (++a->frame == 0) {
    a->frame = 1;
    memset(a->bgFrame, 0, sizeof(a->bgFrame));
    for (unsigned i = 1; i < PICA_SLOTS; i++)
      a->tile[i].used = a->tile[i].checked = 0;
  }
  a->hits = a->decodes = a->live = 0;
}

static void Remove(PicaAtlas *a, uint32_t key) {
  unsigned i = Hash(key);
  while (a->hash[i] && a->tile[a->hash[i]].key != key)
    i = (i + 1) & (PICA_HASH - 1);
  if (!a->hash[i]) return;
  a->hash[i] = 0;
  for (unsigned j = (i + 1) & (PICA_HASH - 1); a->hash[j];
       j = (j + 1) & (PICA_HASH - 1)) {
    unsigned slot = a->hash[j];
    a->hash[j] = 0;
    unsigned k = Hash(a->tile[slot].key);
    while (a->hash[k]) k = (k + 1) & (PICA_HASH - 1);
    a->hash[k] = slot;
  }
}

static int Tile(PicaFrame *f, unsigned address, unsigned palette, unsigned bpp) {
  PicaAtlas *a = f->atlas;
  address &= 0x7fff;
  uint32_t key = address | (palette << 15) | ((bpp == 4) << 23);
  unsigned h = Hash(key), slot;
  while (a->hash[h] && a->tile[a->hash[h]].key != key)
    h = (h + 1) & (PICA_HASH - 1);
  slot = a->hash[h];
  if (!slot) {
    unsigned scanned = 0;
    do {
      a->cursor = a->cursor % (PICA_SLOTS - 1) + 1;
      slot = a->cursor;
    } while (a->tile[slot].used == a->frame && ++scanned < PICA_SLOTS - 1);
    if (a->tile[slot].used == a->frame) {
      f->failure = "atlas-live-capacity";
      return -1;
    }
    if (a->tile[slot].valid) Remove(a, a->tile[slot].key);
    h = Hash(key);
    while (a->hash[h]) h = (h + 1) & (PICA_HASH - 1);
    a->hash[h] = slot;
    a->tile[slot].valid = false;
    a->tile[slot].key = key;
  }
  PicaTile *t = &a->tile[slot];
  if (t->used != a->frame) {
    t->used = a->frame;
    a->live++;
  }
  if (t->checked == a->frame && t->valid) {
    a->hits++;
    return t->opaque ? (int)slot : -2;
  }
  t->checked = a->frame;
  unsigned words = bpp == 4 ? 16 : 8;
  unsigned colors = 1u << bpp;
  const uint16_t *src = f->memory->vram + address;
  const uint16_t *pal = f->memory->cgram + palette;
  if (t->valid && !memcmp(t->source, src, words * 2) &&
      !memcmp(t->palette, pal, colors * 2)) {
    a->hits++;
    return t->opaque ? (int)slot : -2;
  }
  memcpy(t->source, src, words * 2);
  memcpy(t->palette, pal, colors * 2);
  t->valid = true;
  t->opaque = false;
  a->decodes++;
  a->dirty[slot / 32] |= 1u << (slot & 31);
  uint32_t *dest = f->pixels + slot * 64;
  for (unsigned y = 0; y < 8; y++) {
    uint32_t bits = src[y];
    if (bpp == 4) bits |= (uint32_t)src[y + 8] << 16;
    for (unsigned x = 0; x < 8; x++) {
      unsigned i = 7 - x;
      unsigned p = ((bits >> i) & 1) | ((bits >> (i + 7)) & 2) |
                   ((bits >> (i + 14)) & 4) | ((bits >> (i + 21)) & 8);
      dest[kMorton[y * 8 + x]] = p ? Encode(pal[p], 255) : 0;
      t->opaque |= p != 0;
    }
  }
  return t->opaque ? (int)slot : -2;
}

static uint16_t *BackgroundSlots(PicaAtlas *a, unsigned layer,
                                 unsigned base) {
  /* VRAM/CGRAM are frozen for a GPU frame, but HDMA/IRQ can select another
   * tile bank between scanlines. Main and sub share a bank when possible. */
  if (a->bgFrame[layer] != a->frame || a->bgBase[layer] != base) {
    memset(a->bgSlots[layer], 0, sizeof(a->bgSlots[layer]));
    a->bgFrame[layer] = a->frame;
    a->bgBase[layer] = base;
  }
  return a->bgSlots[layer];
}

static inline int BackgroundTile(PicaFrame *f, uint16_t *slots,
                                 unsigned tile, unsigned base,
                                 unsigned words, unsigned paletteShift,
                                 unsigned bpp) {
  unsigned descriptor = tile & 0x1fff;
  unsigned cached = slots ? slots[descriptor] : 0;
  if (cached) {
    f->atlas->hits++;
    return (int)cached - 3;
  }
  int slot = Tile(f, base + (tile & 1023) * words,
                  (tile & 0x1c00) >> paletteShift, bpp);
  /* Zero means unresolved, one is transparent. Never cache a failure.
   * Tile() pins every resolved slot until PicaAtlasBegin(), so eviction
   * cannot invalidate this shortcut while drawing the remaining layers. */
  if (slots && slot != -1) slots[descriptor] = slot + 3;
  return slot;
}

void PicaCaptureLine(PicaLine *out, const Ppu *p) {
  memset(out, 0, sizeof(*out));
  memcpy(out->bg, p->bgLayer, sizeof(out->bg));
  out->objTileAdr1 = p->objTileAdr1;
  out->objTileAdr2 = p->objTileAdr2;
  out->objSize = p->objSize;
  memcpy(out->screenEnabled, p->screenEnabled, sizeof(out->screenEnabled));
  memcpy(out->screenWindowed, p->screenWindowed, sizeof(out->screenWindowed));
  out->mosaicEnabled = p->mosaicEnabled;
  out->mosaicSize = p->mosaicSize;
  out->window1left = p->window1left;
  out->window1right = p->window1right;
  out->window2left = p->window2left;
  out->window2right = p->window2right;
  out->windowsel = p->windowsel;
  for (unsigned i = 0; i < 6; i++) out->windowLogic[i] = p->windowLayer[i].maskLogic;
  out->clipMode = p->clipMode;
  out->preventMathMode = p->preventMathMode;
  out->addSubscreen = p->addSubscreen;
  out->subtractColor = p->subtractColor;
  out->halfColor = p->halfColor;
  for (unsigned i = 0; i < 6; i++)
    if (p->mathEnabled[i]) out->mathEnabled |= 1u << i;
  out->fixedColorR = p->fixedColorR;
  out->fixedColorG = p->fixedColorG;
  out->fixedColorB = p->fixedColorB;
  out->forcedBlank = p->forcedBlank;
  out->brightness = p->brightness;
  out->mode = p->mode;
  out->bg3priority = p->bg3priority;
}

static void WindowsWithFirstEdges(const PicaFrame *f, const PicaLine *p,
                                  unsigned layer, bool enabled,
                                  int spanLeft, int spanRight,
                                  int window1left, int window1right,
                                  WindowSpans *out) {
  out->edges[0] = spanLeft;
  out->edges[1] = spanRight;
  out->nr = 1;
  out->bits = 0;
  if (!enabled) return;
  unsigned flags = p->windowsel >> (layer * 4);
  unsigned line = p - f->lines;
  int window2left = f->xrayActive ? f->xrayLeft[line] : p->window2left;
  int window2right = f->xrayActive ? f->xrayRight[line] : p->window2right;
  unsigned nr = 1;
  bool w1 = (flags & 2) && window1left <= window1right;
  bool w2 = (flags & 8) && window2left <= window2right;
  int points[4], count = 0;
  if (w1) { points[count++] = window1left + f->originX;
            points[count++] = window1right + 1 + f->originX; }
  if (w2) { points[count++] = window2left + f->originX;
            points[count++] = window2right + 1 + f->originX; }
  for (int n = 0; n < count; n++) {
    int value = points[n];
    if (value <= spanLeft || value >= spanRight) continue;
    unsigned i = 0;
    while (i <= nr && out->edges[i] < value) i++;
    if (i <= nr && out->edges[i] == value) continue;
    for (unsigned j = nr + 1; j > i; j--) out->edges[j] = out->edges[j - 1];
    out->edges[i] = value;
    nr++;
  }
  out->nr = nr;
  for (unsigned i = 0; i < nr; i++) {
    int x = out->edges[i] - (int)f->originX;
    bool a = w1 && x >= window1left && x <= window1right;
    bool b = w2 && x >= window2left && x <= window2right;
    if (w1 && (flags & 1)) a = !a;
    if (w2 && (flags & 4)) b = !b;
    bool masked;
    if (w1 && w2) {
      switch (p->windowLogic[layer] & 3) {
        default: case 0: masked = a || b; break;
        case 1: masked = a && b; break;
        case 2: masked = a != b; break;
        case 3: masked = a == b; break;
      }
    } else {
      masked = w1 ? a : w2 ? b : false;
    }
    if (masked) out->bits |= 1u << i;
  }
}

static void Windows(const PicaFrame *f, const PicaLine *p, unsigned layer,
                    bool enabled, int spanLeft, int spanRight,
                    WindowSpans *out) {
  WindowsWithFirstEdges(f, p, layer, enabled, spanLeft, spanRight,
                        p->window1left, p->window1right, out);
}

static bool IsHudLine(const PicaFrame *f, unsigned y) {
  const PicaLine *p = &f->lines[y];
  return y < f->hudEndY && p->screenEnabled[0] == 4 &&
         p->bg[2].tilemapAdr == 0x5800 && p->mathEnabled == 0;
}

unsigned PicaHudLineCount(const PicaFrame *f) {
  unsigned y = 0;
  while (y < f->height && IsHudLine(f, y)) y++;
  return y;
}

static bool Emit(PicaFrame *f, unsigned group, PicaQuad quad) {
  if (quad.x0 >= quad.x1 || quad.y0 >= quad.y1) return true;
  if (!f->emit(f->context, group, &quad)) {
    f->failure = "vertex-capacity";
    return false;
  }
  f->quads[group]++;
  return true;
}

static PicaQuad Solid(int x0, int y0, int x1, int y1,
                      uint16_t rgb, unsigned alpha) {
  PicaQuad q = {x0,y0,x1,y1,2,4092,2,4092,1,
               (rgb&31)*8+1,((rgb>>5)&31)*8+1,((rgb>>10)&31)*8+1,alpha};
  return q;
}

static bool TileQuad(PicaFrame *f, unsigned group, unsigned slot,
                     int x, int y, int w, int h, int sx, int sy,
                     bool hf, bool vf, unsigned depth, unsigned alpha) {
  unsigned col = slot % (PICA_ATLAS_W / 8), row = slot / (PICA_ATLAS_W / 8);
  int u0 = col * 8 + (hf ? 8 - sx : sx), u1 = u0 + (hf ? -w : w);
  int v0 = row * 8 + (vf ? 8 - sy : sy), v1 = v0 + (vf ? -h : h);
  PicaQuad q = {x,y,x+w,y+h,u0*4,4096-v0*8,u1*4,4096-v1*8,
                depth,255,255,255,alpha};
  return Emit(f, group, q);
}

static unsigned EqualRun(const PicaFrame *f, unsigned y,
                         size_t offset, size_t size, unsigned maxRun) {
  unsigned end = y + 1;
  const uint8_t *base = (const uint8_t *)&f->lines[y] + offset;
  while (end < f->height && end - y < maxRun &&
         !memcmp(base, (const uint8_t *)&f->lines[end] + offset, size))
    end++;
  return end - y;
}

static bool HasLayer(const PicaFrame *f, unsigned sub, unsigned mask) {
  for (unsigned y = 0; y < f->height; y++)
    if ((f->lines[y].screenEnabled[sub] & mask) &&
        (!sub || f->lines[y].addSubscreen)) return true;
  return false;
}

typedef struct WideTileRow {
  const uint16_t *blocks, *tileTable;
  int cameraX;
  unsigned blockRow, quadrantY, pixelY;
  bool valid;
} WideTileRow;

static WideTileRow WideRoomRow(const PicaFrame *f, unsigned layer, int y) {
  const PicaWideRoomLayer *room = &f->wideRoom[layer];
  const BgLayer *bg = &f->lines[y - 1].bg[layer];
  int dx = room->followScroll ?
      PicaScrollOffset(bg->hScroll, room->scrollX, bg->tilemapWider) : 0;
  int dy = room->followScroll ?
      PicaScrollOffset(bg->vScroll, room->scrollY, bg->tilemapHigher) : 0;
  int worldY = room->cameraY + y + dy;
  WideTileRow row = {0};
  if (!room->blocks || !f->wideTileTable || !f->wideRoomWidth ||
      !f->wideRoomHeight || worldY < 0 ||
      worldY / 16 >= (int)f->wideRoomHeight) return row;
  row.blocks = room->blocks;
  row.tileTable = f->wideTileTable;
  row.cameraX = room->cameraX + dx;
  row.blockRow = (worldY / 16) * f->wideRoomWidth;
  row.quadrantY = (worldY & 8) ? 2 : 0;
  row.pixelY = worldY & 7;
  row.valid = true;
  return row;
}

int PicaScrollOffset(unsigned scroll, unsigned base, bool wider) {
  unsigned mask = wider ? 511 : 255;
  unsigned delta = (scroll - base) & mask;
  return delta > mask / 2 ? (int)delta - (int)(mask + 1) : (int)delta;
}

static bool WideRoomTile(const PicaFrame *f, const WideTileRow *row, int x,
                         unsigned layer,
                         uint16_t *tile, unsigned *tileX, unsigned *tileY) {
  int worldX = row->cameraX + x - (int)f->originX;
  if (!row->valid || worldX < 0 ||
      worldX / 16 >= (int)f->wideRoomWidth) return false;
  if (layer == 1 && f->xrayTiles[0]) {
    unsigned side = x < (int)f->originX ? 0 : 1;
    int col = (worldX - f->xrayStartX[side]) / 8;
    int worldY = (int)(row->blockRow / f->wideRoomWidth) * 16 +
                  (row->quadrantY ? 8 : 0) + row->pixelY;
    int y = (worldY - f->xrayStartY) / 8;
    if (col < 0 || col >= 32 || y < 0 || y >= 32) return false;
    *tile = f->xrayTiles[side][y * 32 + col];
    *tileX = worldX & 7;
    *tileY = row->pixelY;
    return true;
  }
  unsigned block = row->blocks[row->blockRow + worldX / 16];
  unsigned quadrant = row->quadrantY | ((worldX & 8) ? 1 : 0);
  if (block & 0x400) quadrant ^= 1;
  if (block & 0x800) quadrant ^= 2;
  *tile = row->tileTable[(block & 0x3ff) * 4 + quadrant] ^
      ((block & 0x400) ? 0x4000 : 0) ^
      ((block & 0x800) ? 0x8000 : 0);
  *tileX = worldX & 7;
  *tileY = row->pixelY;
  return true;
}

static bool Backgrounds(PicaFrame *f, unsigned sub) {
  unsigned group = sub * 2;
  for (unsigned y = 0; y < f->height;) {
    const PicaLine *p = &f->lines[y];
    unsigned n = 1;
    while (y + n < f->height) {
      const PicaLine *q = &f->lines[y + n];
      if (p->forcedBlank != q->forcedBlank || p->addSubscreen != q->addSubscreen ||
          p->mathEnabled != q->mathEnabled || p->fixedColorR != q->fixedColorR ||
          p->fixedColorG != q->fixedColorG || p->fixedColorB != q->fixedColorB) break;
      n++;
    }
    unsigned alpha = sub ? (!p->addSubscreen ? 255 : 127) :
                           ((p->mathEnabled & 32) ? 255 : 127);
    uint16_t rgb = sub ? (p->fixedColorR | (p->fixedColorG << 5) |
                          (p->fixedColorB << 10)) : f->memory->cgram[0];
    if (p->forcedBlank) { rgb = 0; alpha = 127; }
    if (!Emit(f, group, Solid(0, y, f->width, y + n, rgb, alpha))) return false;
    y += n;
  }

  for (unsigned layer = 0; layer < 3; layer++) {
    unsigned layerBit = 1u << layer;
    if (!HasLayer(f, sub, layerBit)) continue;
    /* Inactive IRQ/HDMA states cannot use a graphics bank. Only bank
     * changes on visible lines need the general descriptor lookup path. */
    unsigned base = f->lines[0].bg[layer].tileAdr;
    bool stableBase = true;
    for (unsigned line = 1; line < f->height; line++)
      if (f->lines[line].bg[layer].tileAdr != base) {
        stableBase = false;
        break;
      }
    if (!stableBase) {
      bool haveBase = false;
      stableBase = true;
      for (unsigned line = 0; line < f->height; line++) {
        const PicaLine *active = &f->lines[line];
        if (active->forcedBlank || !(active->screenEnabled[sub] & layerBit) ||
            (sub && !active->addSubscreen)) continue;
        if (!haveBase) {
          base = active->bg[layer].tileAdr;
          haveBase = true;
        } else if (active->bg[layer].tileAdr != base) {
          stableBase = false;
          break;
        }
      }
    }
    uint16_t *slots = stableBase ? BackgroundSlots(f->atlas, layer, base) : NULL;
    for (unsigned y = 0; y < f->height;) {
      const PicaLine *p = &f->lines[y];
      /* Disabled lines cannot emit tiles. Skip the whole inactive stretch
       * before comparing BG registers and looking up tile rows. */
      if (p->forcedBlank || !(p->screenEnabled[sub] & layerBit) ||
          (sub && !p->addSubscreen)) {
        do {
          y++;
          if (y == f->height) break;
          p = &f->lines[y];
        } while (p->forcedBlank || !(p->screenEnabled[sub] & layerBit) ||
                 (sub && !p->addSubscreen));
        continue;
      }
      const BgLayer *bg = &p->bg[layer];
      WideTileRow roomRow = {0};
      if (layer < 2 && f->wideRoom[layer].blocks)
        roomRow = WideRoomRow(f, layer, y + 1);
      unsigned wy = (y + 1 + bg->vScroll) & (bg->tilemapHigher ? 511 : 255);
      /* A tile-height chunk cannot cross either 8-pixel row boundary.
       * Bound the state comparison now instead of scanning the remaining
       * frame and discarding that result afterward. */
      unsigned maxRun = 8 - (wy & 7);
      if (layer < 2 && f->wideRoom[layer].blocks)
        maxRun = Min(maxRun,
                     8 - roomRow.pixelY);
      unsigned h = Min(maxRun, f->height - y);
      bool windowed = (p->screenWindowed[sub] & layerBit) != 0;
      bool beamWindowTiles = f->extendEyeBeam && sub == 1 && layer == 2 && windowed;
      /* Compare scroll first: a distorted background usually changes it on
       * the next line. Inline the relevant registers and visibility in one
       * pass instead of memcmp(BgLayer) followed by a second state pass. */
      for (unsigned run = 1; run < h; run++) {
        const PicaLine *next = &f->lines[y + run];
        const BgLayer *nextBg = &next->bg[layer];
        if (nextBg->hScroll != bg->hScroll || nextBg->vScroll != bg->vScroll ||
            nextBg->tilemapAdr != bg->tilemapAdr || nextBg->tileAdr != bg->tileAdr ||
            nextBg->tilemapWider != bg->tilemapWider ||
            nextBg->tilemapHigher != bg->tilemapHigher ||
            ((next->screenEnabled[sub] ^ p->screenEnabled[sub]) & layerBit) ||
            ((next->screenWindowed[sub] ^ p->screenWindowed[sub]) & layerBit) ||
            next->forcedBlank != p->forcedBlank ||
            next->addSubscreen != p->addSubscreen ||
            (!sub && ((next->mathEnabled ^ p->mathEnabled) & layerBit)) ||
            (layer == 2 && next->bg3priority != p->bg3priority) ||
            (windowed &&
             ((((next->windowsel ^ p->windowsel) >> (layer * 4)) & 15) ||
              (!beamWindowTiles &&
               (next->window1left != p->window1left ||
                next->window1right != p->window1right ||
                (f->xrayActive ? f->xrayLeft[y + run] != f->xrayLeft[y] :
                                 next->window2left != p->window2left) ||
                (f->xrayActive ? f->xrayRight[y + run] != f->xrayRight[y] :
                                 next->window2right != p->window2right))) ||
              next->windowLogic[layer] != p->windowLogic[layer]))) {
          h = run;
          break;
        }
      }
      WindowSpans win;
      int spanLeft = 0, spanRight = f->width;
      bool hudLine = IsHudLine(f, y);
      if (layer == 0 && f->width == kWideWidth && !hudLine) {
        spanLeft = f->worldLeft;
        spanRight = f->worldRight;
      } else if (layer == 1 && f->boundBg2 &&
                 f->width == kWideWidth && !hudLine) {
        spanLeft = f->bg2Left;
        spanRight = f->bg2Right;
      } else if (layer == 2 && f->boundBg2 &&
                 f->width == kWideWidth && !hudLine) {
        spanLeft = f->bg2Left;
        spanRight = f->bg2Right;
      } else if (layer == 2 && hudLine) {
        spanLeft = f->originX;
        spanRight = spanLeft + kSnesWidth;
      }
      if (spanLeft >= spanRight) { y += h; continue; }
      if (f->extendEyeBeam)
        WindowsWithFirstEdges(f, p, layer, windowed, spanLeft, spanRight,
                              f->beamLeft[y], f->beamRight[y], &win);
      else
        Windows(f, p, layer, windowed, spanLeft, spanRight, &win);
      bool roomSides = f->width == kWideWidth && layer < 2 && !hudLine &&
                       f->wideRoom[layer].blocks;
      unsigned mapRow = bg->tilemapAdr + ((wy >> 3) & 31) * 32 +
                        (wy >= 256 ? (bg->tilemapWider ? 0x800 : 0x400) : 0);
      unsigned bpp = layer == 2 ? 2 : 4;
      unsigned tileWords = bpp == 4 ? 16 : 8;
      unsigned paletteShift = bpp == 4 ? 6 : 8;
      unsigned tileBase = bg->tileAdr;
      unsigned scrollMask = bg->tilemapWider ? 511 : 255;
      int scrollX = (int)bg->hScroll - (int)f->originX;
      unsigned zLow = layer == 0 ? 0x8000 : layer == 1 ? 0x7100 : 0x1200;
      unsigned zHigh = layer == 0 ? 0xc000 : layer == 1 ? 0xb100 :
                       (p->bg3priority ? 0xf200 : 0x5200);
      unsigned tileAlpha = sub ? 255 :
                           ((p->mathEnabled & (1u << layer)) ? 255 : 127);
      if (f->extendEyeBeam && sub == 1 && layer == 2 && windowed) {
        WindowSpans rowWindows[8];
        for (unsigned row = 0; row < h; row++) {
          const PicaLine *line = &f->lines[y + row];
          WindowsWithFirstEdges(f, line, layer, true, spanLeft, spanRight,
                                f->beamLeft[y + row], f->beamRight[y + row],
                                &rowWindows[row]);
        }
        for (int x = spanLeft; x < spanRight;) {
          unsigned wx = (x - (int)f->originX + bg->hScroll) &
                        (bg->tilemapWider ? 511 : 255);
          unsigned pixelX = wx & 7, pixelY = wy & 7;
          unsigned w = Min(8 - pixelX, spanRight - x);
          if (x < (int)f->originX)
            w = Min(w, (int)f->originX - x);
          else if (x < (int)f->originX + kSnesWidth)
            w = Min(w, (int)f->originX + kSnesWidth - x);
          unsigned map = (mapRow + ((wx >> 3) & 31) +
                          (wx >= 256 ? 0x400 : 0)) & 0x7fff;
          uint16_t tile = f->memory->vram[map];
          int slot = BackgroundTile(f, slots, tile, bg->tileAdr, tileWords,
                                    paletteShift, bpp);
          if (slot == -1) return false;
          if (slot >= 0) {
            unsigned z = (tile & 0x2000) ? zHigh : zLow;
            bool whole = true;
            for (unsigned row = 0; row < h && whole; row++) {
              const WindowSpans *rw = &rowWindows[row];
              bool visible = false;
              for (unsigned i = 0; i < rw->nr; i++)
                if (!(rw->bits & (1u << i)) && rw->edges[i] <= x &&
                    rw->edges[i + 1] >= x + (int)w) {
                  visible = true;
                  break;
                }
              whole = visible;
            }
            if (whole) {
              if (!TileQuad(f, group, slot, x, y, w, h, pixelX, pixelY,
                            tile & 0x4000, tile & 0x8000, z, tileAlpha))
                return false;
            } else {
              for (unsigned row = 0; row < h; row++) {
                const WindowSpans *rw = &rowWindows[row];
                for (unsigned i = 0; i < rw->nr; i++) {
                  if (rw->bits & (1u << i)) continue;
                  int left = IMax(x, rw->edges[i]);
                  int right = IMin(x + w, rw->edges[i + 1]);
                  if (left >= right) continue;
                  if (!TileQuad(f, group, slot, left, y + row,
                                right - left, 1, pixelX + left - x,
                                pixelY + row, tile & 0x4000,
                                tile & 0x8000, z, tileAlpha)) return false;
                }
              }
            }
          }
          x += w;
        }
        y += h;
        continue;
      }
      for (unsigned i = 0; i < win.nr; i++) {
        if (win.bits & (1u << i)) continue;
        int x = win.edges[i], end = win.edges[i + 1];
        /* In the original 256-pixel viewport every tile comes from VRAM.
         * Keep the side-band room lookup and boundary splits out of this
         * common path, which runs for thousands of background tiles. */
        if (f->width == kSnesWidth || !roomSides) {
          while (x < end) {
            unsigned wx = (x + scrollX) & scrollMask;
            unsigned pixelX = wx & 7;
            unsigned w = Min(8 - pixelX, end - x);
            unsigned map = (mapRow + ((wx >> 3) & 31) +
                            (wx >= 256 ? 0x400 : 0)) & 0x7fff;
            uint16_t tile = f->memory->vram[map];
            int slot = BackgroundTile(f, slots, tile, tileBase, tileWords,
                                      paletteShift, bpp);
            if (slot == -1) return false;
            if (slot >= 0 &&
                !TileQuad(f, group, slot, x, y, w, h, pixelX, wy & 7,
                          tile & 0x4000, tile & 0x8000,
                          (tile & 0x2000) ? zHigh : zLow, tileAlpha))
              return false;
            x += w;
          }
          continue;
        }
        while (x < end) {
          if (x >= (int)f->originX && x < (int)f->originX + kSnesWidth) {
            int centerEnd = IMin(end, f->originX + kSnesWidth);
            do {
              unsigned wx = (x + scrollX) & scrollMask;
              unsigned pixelX = wx & 7;
              unsigned w = Min(8 - pixelX, centerEnd - x);
              unsigned map = (mapRow + ((wx >> 3) & 31) +
                              (wx >= 256 ? 0x400 : 0)) & 0x7fff;
              uint16_t tile = f->memory->vram[map];
              int slot = BackgroundTile(f, slots, tile, tileBase, tileWords,
                                        paletteShift, bpp);
              if (slot == -1) return false;
              if (slot >= 0 &&
                  !TileQuad(f, group, slot, x, y, w, h, pixelX, wy & 7,
                            tile & 0x4000, tile & 0x8000,
                            (tile & 0x2000) ? zHigh : zLow, tileAlpha))
                return false;
              x += w;
            } while (x < centerEnd);
            continue;
          }
          bool side = roomSides &&
              (x < (int)f->originX || x >= (int)f->originX + kSnesWidth);
          unsigned wx = (x - (int)f->originX + bg->hScroll) &
                        (bg->tilemapWider ? 511 : 255);
          uint16_t tile;
          unsigned pixelX = wx & 7, pixelY = wy & 7;
          unsigned w = Min(8 - (wx & 7), end - x);
          if (x < (int)f->originX)
            w = Min(w, (int)f->originX - x);
          else if (x < (int)f->originX + kSnesWidth)
            w = Min(w, (int)f->originX + kSnesWidth - x);
          if (side) {
            if (!WideRoomTile(f, &roomRow, x, layer, &tile,
                              &pixelX, &pixelY)) {
              x += w;
              continue;
            }
          } else {
            unsigned map = (mapRow + ((wx >> 3) & 31) +
                            (wx >= 256 ? 0x400 : 0)) & 0x7fff;
            tile = f->memory->vram[map];
          }
          int slot = BackgroundTile(f, slots, tile, bg->tileAdr, tileWords,
                                    paletteShift, bpp);
          if (slot == -1) return false;
          if (side) w = Min(w, 8 - pixelX);
          unsigned z = (tile & 0x2000) ? zHigh : zLow;
          if (slot >= 0 && !TileQuad(f, group, slot, x, y, w, h, pixelX, pixelY,
                                     tile & 0x4000, tile & 0x8000, z, tileAlpha))
            return false;
          x += w;
        }
      }
      y += h;
    }
  }
  return true;
}

static unsigned HighOam(const Ppu *p, unsigned index) {
  return p->highOam[index >> 3] >> (index & 7);
}

static int ObjectX(const PicaFrame *f, unsigned index, unsigned high,
                   unsigned size) {
  if (f->width > kSnesWidth && f->objectX && f->objectXValid &&
      f->objectXValid[index >> 1])
    return f->objectX[index >> 1];
  int x = (f->memory->oam[index] & 255) + (high & 1) * 256;
  if (x >= 256) x -= 512;
  /* X=256..327 wraps to negative OAM coordinates. It cannot be a visible
   * left-side sprite at this width, so recover its right-side position. */
  if (f->width > kSnesWidth && x + (int)size <= -(int)f->originX)
    x += 512;
  return x;
}

static bool Objects(PicaFrame *f, unsigned sub) {
  if (!HasLayer(f, sub, 16)) return true;
  uint16_t active[128], first[128], last[128];
  unsigned activeCount = 0;
  for (unsigned index = 0; index < 256; index += 2) {
    if ((f->memory->oam[index] >> 8) == 0xf0) continue;
    active[activeCount++] = index;
    first[index / 2] = f->height;
    last[index / 2] = 0;
  }
  uint8_t (*columns)[PICA_MAX_LINES] = f->atlas->objectColumns;
  memset(columns, 0, sizeof(f->atlas->objectColumns));
  unsigned fixedSize = f->lines[0].objSize;
  bool stableSize = fixedSize < 8;
  for (unsigned y = 1; y < f->height; y++)
    if (f->lines[y].objSize != fixedSize) { stableSize = false; break; }
  uint32_t visible[PICA_MAX_LINES][4] = {{0}};
  int16_t fixedX[128];
  uint8_t fixedWidth[128];
  for (unsigned item = 0; item < activeCount; item++) {
    unsigned index = active[item];
    if (!stableSize) continue;
    unsigned high = HighOam(f->memory, index);
    unsigned size = kSpriteSizes[fixedSize][(high >> 1) & 1];
    int x = ObjectX(f, index, high, size);
    fixedX[item] = x;
    fixedWidth[item] = size;
    if (x + (int)size <= -(int)f->originX ||
        x >= (int)f->width - (int)f->originX) continue;
    unsigned yy = f->memory->oam[index] >> 8;
    /* Build ordered row membership once. A multipart sprite must not cost
     * an OAM scan on rows which it does not intersect. Wrapped Y positions
     * and the original per-line sprite/tile limits are still respected. */
    for (unsigned row = 0; row < size; row++) {
      unsigned y = (yy + row) & 255;
      if (y < f->height) visible[y][item >> 5] |= 1u << (item & 31);
    }
  }
  for (unsigned y = 0; y < f->height; y++) {
    const PicaLine *p = &f->lines[y];
    if (p->forcedBlank || !(p->screenEnabled[sub] & 16) ||
        (sub && !p->addSubscreen)) continue;
    int sprites = 33, tiles = 35;
    bool stop = false;
    if (!stableSize) {
      /* Scanline OBJ-size changes keep the original selection loop. */
      for (unsigned item = 0; item < activeCount && !stop; item++) {
        unsigned index = active[item];
        unsigned row = (y - (f->memory->oam[index] >> 8)) & 255;
        unsigned high = HighOam(f->memory, index);
        unsigned size = kSpriteSizes[p->objSize][(high >> 1) & 1];
        if (row >= size) continue;
        int x = ObjectX(f, index, high, size);
        if (x + (int)size <= -(int)f->originX ||
            x >= (int)f->width - (int)f->originX) continue;
        if (--sprites == 0) break;
        for (unsigned col = 0; col < size; col += 8) {
          int left = x + col + f->originX;
          if (left <= -8 || left >= (int)f->width) continue;
          if (--tiles == 0) { stop = true; break; }
          columns[index / 2][y] |= 1u << (col / 8);
          if (first[index / 2] > y) first[index / 2] = y;
          last[index / 2] = y + 1;
        }
      }
      continue;
    }
    for (unsigned word = 0; word < 4 && !stop; word++) {
      uint32_t members = visible[y][word];
      while (members && !stop) {
        unsigned item = word * 32 + __builtin_ctz(members);
        members &= members - 1;
        unsigned index = active[item];
        unsigned size = fixedWidth[item];
        int x = fixedX[item];
        if (--sprites == 0) { stop = true; break; }
        for (unsigned col = 0; col < size; col += 8) {
          int left = x + col + f->originX;
          if (left <= -8 || left >= (int)f->width) continue;
          if (--tiles == 0) { stop = true; break; }
          columns[index / 2][y] |= 1u << (col / 8);
          if (first[index / 2] > y) first[index / 2] = y;
          last[index / 2] = y + 1;
        }
      }
    }
  }

  for (unsigned item = 0; item < activeCount; item++) {
    unsigned index = active[item];
    for (unsigned y = first[index / 2]; y < last[index / 2];) {
      unsigned mask = columns[index / 2][y];
      if (!mask) { y++; continue; }
      const PicaLine *p = &f->lines[y];
      unsigned yy = f->memory->oam[index] >> 8;
      unsigned high = HighOam(f->memory, index);
      unsigned size = kSpriteSizes[p->objSize][(high >> 1) & 1];
      unsigned attr = f->memory->oam[index + 1];
      unsigned row = (y - yy) & 255;
      bool vf = (attr & 0x8000) != 0;
      if (vf) row = size - 1 - row;
      unsigned h = vf ? 1 + (row & 7) : 8 - (row & 7);
      h = Min(h, last[index / 2] - y);
      for (unsigned dy = 1; dy < h; dy++)
        if (columns[index / 2][y + dy] != mask) { h = dy; break; }
      int x = ObjectX(f, index, high, size);
      unsigned base = (attr & 0x100) ? p->objTileAdr2 : p->objTileAdr1;
      unsigned palette = 128 + ((attr >> 9) & 7) * 16;
      unsigned z = ((((attr >> 12) & 3) * 4 + 2) * 16 + 4 +
                    ((attr & 0x800) ? 0 : 2)) << 8;
      WindowSpans win;
      if (f->extendEyeBeam)
        WindowsWithFirstEdges(f, p, 4, (p->screenWindowed[sub] & 16) != 0,
                              0, f->width, f->beamLeft[y], f->beamRight[y],
                              &win);
      else
        Windows(f, p, 4, (p->screenWindowed[sub] & 16) != 0,
                0, f->width, &win);
      while (mask) {
        unsigned col = __builtin_ctz(mask) * 8;
        mask &= mask - 1;
        int left = x + col + f->originX;
        unsigned usedcol = (attr & 0x4000) ? size - 1 - col : col;
        unsigned usedtile = ((((attr & 255) >> 4) + (row >> 3)) << 4) |
                            (((attr & 15) + (usedcol >> 3)) & 15);
        int slot = Tile(f, (base + usedtile * 16) & 0x7fff, palette, 4);
        if (slot == -1) return false;
        if (slot == -2) continue;
        for (unsigned i = 0; i < win.nr; i++) {
          if (win.bits & (1u << i)) continue;
          int l = IMax(left, win.edges[i]), r = IMin(left + 8, win.edges[i + 1]);
          if (l >= r) continue;
          if (!TileQuad(f, sub * 2 + 1, slot, l, y, r - l, h, l - left,
                        vf ? 7 - (row & 7) : row & 7, attr & 0x4000, vf, z,
                        sub ? 255 : ((attr & 0x800) && (p->mathEnabled & 16) ? 255 : 127)))
            return false;
        }
      }
      y += h;
    }
  }
  return true;
}

typedef struct BeamEdgeFit {
  int firstY, firstX, lastY, lastX;
} BeamEdgeFit;

static void BeamEdgeSample(BeamEdgeFit *fit, int y, int x) {
  if (x <= 0 || x >= 255) return;
  if (fit->firstY < 0) { fit->firstY = y; fit->firstX = x; }
  fit->lastY = y;
  fit->lastX = x;
}

static int BeamEdgeAt(const BeamEdgeFit *fit, int y) {
  int span = fit->lastY - fit->firstY;
  int value = span >= 4 ? fit->firstX +
      (fit->lastX - fit->firstX) * (y - fit->firstY) / span : -10000;
  return IMax(-512, IMin(value, 768));
}

static void ExtendEyeBeam(const PicaFrame *f, int16_t *left, int16_t *right) {
  BeamEdgeFit leftFit = {.firstY = -1}, rightFit = {.firstY = -1};
  for (unsigned y = 0; y < f->height; y++) {
    const PicaLine *p = &f->lines[y];
    left[y] = p->window1left;
    right[y] = p->window1right;
    if (((p->windowsel >> 20) & 2) &&
        p->window1left <= p->window1right) {
      BeamEdgeSample(&leftFit, y, p->window1left);
      BeamEdgeSample(&rightFit, y, p->window1right);
    }
  }
  if (leftFit.lastY - leftFit.firstY < 4 ||
      rightFit.lastY - rightFit.firstY < 4) return;
  for (unsigned y = 0; y < f->height; y++) {
    const PicaLine *p = &f->lines[y];
    if (!((p->windowsel >> 20) & 2)) continue;
    int projectedLeft = BeamEdgeAt(&leftFit, y);
    int projectedRight = BeamEdgeAt(&rightFit, y);
    if (p->window1left <= p->window1right) {
      if (p->window1left == 0 && projectedLeft < 0) left[y] = projectedLeft;
      if (p->window1right == 255 && projectedRight > 255) right[y] = projectedRight;
    } else if (projectedLeft <= projectedRight &&
               projectedLeft < (int)kSnesWidth + (int)f->originX &&
               projectedRight >= -(int)f->originX &&
               (projectedLeft < 0 || projectedRight >= kSnesWidth)) {
      left[y] = projectedLeft;
      right[y] = projectedRight;
    }
  }
}

static bool Compose(PicaFrame *f) {
  WindowSpans windows[PICA_MAX_LINES];
  uint8_t spanFlags[PICA_MAX_LINES][5];
  uint16_t usedFlags = 0;
  for (unsigned y = 0; y < f->height;) {
    const PicaLine *p = &f->lines[y];
    unsigned h = 1;
    while (y + h < f->height) {
      const PicaLine *q = &f->lines[y + h];
      unsigned colorWindows = (p->windowsel >> 20) & 15;
      if (p->clipMode != q->clipMode || p->preventMathMode != q->preventMathMode ||
          p->subtractColor != q->subtractColor || p->halfColor != q->halfColor ||
          p->forcedBlank != q->forcedBlank ||
          (((p->windowsel ^ q->windowsel) >> 20) & 15) ||
          p->windowLogic[5] != q->windowLogic[5] ||
          ((colorWindows & 2) &&
           ((f->extendEyeBeam ? f->beamLeft[y] != f->beamLeft[y + h] :
                                p->window1left != q->window1left) ||
            (f->extendEyeBeam ? f->beamRight[y] != f->beamRight[y + h] :
                                p->window1right != q->window1right))) ||
          ((colorWindows & 8) &&
           ((f->xrayActive ? f->xrayLeft[y] != f->xrayLeft[y + h] :
                            p->window2left != q->window2left) ||
            (f->xrayActive ? f->xrayRight[y] != f->xrayRight[y + h] :
                            p->window2right != q->window2right)))) break;
      h++;
    }
    f->bandEnd[y] = y + h;
    WindowSpans *win = &windows[y];
    if (f->extendEyeBeam)
      WindowsWithFirstEdges(f, p, 5, true, 0, f->width,
                            f->beamLeft[y], f->beamRight[y], win);
    else
      Windows(f, p, 5, true, 0, f->width, win);
    for (unsigned i = 0; i < win->nr; i++) {
      bool inside = (win->bits & (1u << i)) != 0;
      bool clip = p->clipMode == 3 || (p->clipMode == 2 && inside) ||
                  (p->clipMode == 1 && !inside);
      bool prevent = p->preventMathMode == 3 ||
                     (p->preventMathMode == 2 && inside) ||
                     (p->preventMathMode == 1 && !inside);
      unsigned flags = (p->subtractColor ? 1 : 0) | (p->halfColor ? 2 : 0) |
                       (clip ? 4 : 0) | (prevent ? 8 : 0);
      spanFlags[y][i] = p->forcedBlank ? 12 : flags;
      usedFlags |= 1u << spanFlags[y][i];
    }
    y += h;
  }

  /* Each TEV configuration is one contiguous vertex range. HDMA can change
   * state every scanline, so compute its spans once before grouping vertices. */
  for (unsigned wanted = 0; wanted < 16; wanted++) {
    if (!(usedFlags & (1u << wanted))) continue;
    for (unsigned y = 0; y < f->height; y = f->bandEnd[y]) {
      const WindowSpans *win = &windows[y];
      for (unsigned i = 0; i < win->nr; i++) {
        if (spanFlags[y][i] != wanted) continue;
        PicaQuad quad = {win->edges[i],(int)y,win->edges[i+1],f->bandEnd[y],
                         win->edges[i]*8,4096-(int)y*16,win->edges[i+1]*8,
                         4096-(int)f->bandEnd[y]*16,1,255,255,255,255};
        if (!Emit(f, 4 + wanted, quad)) return false;
      }
    }
  }
  /* Color math can otherwise turn empty space beyond a room wall into a
   * solid spotlight color. Mask only the playfield, leaving the HUD intact. */
  if ((f->extendEyeBeam || f->xrayActive) && f->hudEndY < f->height) {
    if (f->worldLeft > 0) {
      PicaQuad black = {0, f->hudEndY, f->worldLeft, f->height,
                        0, 0, 0, 0, 1, 0, 0, 0, 255};
      if (!Emit(f, 20, black)) return false;
    }
    if (f->worldRight < (int)f->width) {
      PicaQuad black = {f->worldRight, f->hudEndY, f->width, f->height,
                        0, 0, 0, 0, 1, 0, 0, 0, 255};
      if (!Emit(f, 20, black)) return false;
    }
  }
  return true;
}

bool PicaBuildFrame(PicaFrame *f) {
#ifdef SM3DS_PHASE_DIAG
  memset(&g_pica_build_timing, 0, sizeof(g_pica_build_timing));
#endif
  memset(f->quads, 0, sizeof(f->quads));
  f->failure = NULL;
  if (!((f->width == kSnesWidth && f->originX == 0) ||
        (f->width == kWideWidth && f->originX == kWideExtraX)) ||
      !f->height || f->height > PICA_MAX_LINES ||
      f->hudEndY > f->height || f->worldLeft < 0 ||
      f->worldRight > (int)f->width || f->worldLeft > f->worldRight ||
      (f->boundBg2 && (f->bg2Left < 0 || f->bg2Right > (int)f->width ||
                       f->bg2Left > f->bg2Right))) {
    f->failure = "dimensions";
    return false;
  }
  for (unsigned y = 0; y < f->height; y++) {
    const PicaLine *p = &f->lines[y];
    if (p->forcedBlank) continue;
    if (p->mode != 1) { f->failure = "mode7-or-non-mode1"; return false; }
    if (p->brightness != 15) { f->failure = "brightness-fade"; return false; }
    if (p->mosaicEnabled && p->mosaicSize > 1) { f->failure = "mosaic"; return false; }
    if (p->objSize > 7 || p->bg[0].bigTiles || p->bg[1].bigTiles ||
        p->bg[2].bigTiles) { f->failure = "register-range"; return false; }
  }
  if (f->extendEyeBeam)
    ExtendEyeBeam(f, f->beamLeft, f->beamRight);
#ifdef SM3DS_PHASE_DIAG
  uint64_t before = SDL_GetPerformanceCounter();
  if (!Backgrounds(f, 0)) return false;
  uint64_t after = SDL_GetPerformanceCounter();
  g_pica_build_timing.bg_main = after - before;
  if (!Objects(f, 0)) return false;
  before = SDL_GetPerformanceCounter();
  g_pica_build_timing.obj_main = before - after;
  if (!Backgrounds(f, 1)) return false;
  after = SDL_GetPerformanceCounter();
  g_pica_build_timing.bg_sub = after - before;
  if (!Objects(f, 1)) return false;
  before = SDL_GetPerformanceCounter();
  g_pica_build_timing.obj_sub = before - after;
  if (!Compose(f)) return false;
  g_pica_build_timing.compose = SDL_GetPerformanceCounter() - before;
  return true;
#else
  return Backgrounds(f, 0) && Objects(f, 0) && Backgrounds(f, 1) &&
         Objects(f, 1) && Compose(f);
#endif
}
