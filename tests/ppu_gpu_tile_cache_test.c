#include "../source/ppu_gpu_model.h"
#include "../source/wide_config.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef struct Samples {
  const PicaAtlas *atlas;
  const uint32_t *pixels;
  uint32_t main[2], sub[2];
} Samples;

static bool Sample(void *context, unsigned group, const PicaQuad *q) {
  Samples *s = context;
  if (group != 0 && group != 2) return true;
  for (unsigned i = 0; i < 2; i++) {
    int y = i ? 18 : 2;
    if (q->depth == 1 || q->x0 > 0 || q->x1 <= 0 || q->y0 > y || q->y1 <= y)
      continue;
    unsigned u = q->u0 / 4;
    unsigned v = (4096 - q->v0) / 8;
    unsigned slot = (v / 8) * (PICA_ATLAS_W / 8) + u / 8;
    assert(slot < PICA_SLOTS && s->atlas->tile[slot].valid);
    // Test tiles use the same color for every pixel, so the Morton index
    // and partial-tile crop do not affect this sample.
    (group == 0 ? s->main : s->sub)[i] = s->pixels[slot * 64];
  }
  return true;
}

static uint32_t Color(unsigned rgb) {
  return (((rgb & 31) * 8 + 1) << 24) |
         ((((rgb >> 5) & 31) * 8 + 1) << 16) |
         ((((rgb >> 10) & 31) * 8 + 1) << 8) | 255;
}

static void Build(PicaFrame *f, Samples *s) {
  memset(s->main, 0, sizeof(s->main));
  memset(s->sub, 0, sizeof(s->sub));
  PicaAtlasBegin(f->atlas);
  assert(PicaBuildFrame(f));
}

int main(void) {
  Ppu *ppu = calloc(1, sizeof(*ppu));
  PicaAtlas *atlas = calloc(1, sizeof(*atlas));
  uint32_t *pixels = calloc(PICA_ATLAS_W * PICA_ATLAS_H, sizeof(*pixels));
  assert(ppu && atlas && pixels);
  PicaLine lines[32] = {0};
  for (unsigned y = 0; y < 32; y++) {
    lines[y].mode = 1;
    lines[y].brightness = 15;
    lines[y].screenEnabled[0] = lines[y].screenEnabled[1] = 1;
    lines[y].addSubscreen = true;
    lines[y].bg[0].tilemapAdr = 0x4000;
  }
  for (unsigned i = 0; i < 1024; i++) ppu->vram[0x4000 + i] = 1;
  for (unsigned y = 0; y < 8; y++) {
    ppu->vram[16 + y] = 0xffff;       // color 3
    ppu->vram[0x1010 + y] = 0x00ff;  // color 1
  }
  ppu->cgram[1] = 0x001f;
  ppu->cgram[3] = 0x03e0;
  Samples samples = {.atlas = atlas, .pixels = pixels};
  PicaFrame f = {.memory = ppu, .lines = lines, .atlas = atlas,
                .pixels = pixels, .width = kSnesWidth, .height = 32,
                .worldRight = kSnesWidth, .emit = Sample, .context = &samples};
  PicaAtlasInit(atlas, pixels);
  Build(&f, &samples);
  assert(samples.main[0] == Color(0x03e0));
  assert(!memcmp(samples.main, samples.sub, sizeof(samples.main)));
  assert(atlas->decodes == 1 && atlas->live == 1 && atlas->hits > 100);

  // Changing CGRAM next frame must not keep a cached color.
  ppu->cgram[3] = 0x7c00;
  Build(&f, &samples);
  assert(samples.main[0] == Color(0x7c00) && atlas->decodes == 1);

  // Animated VRAM can make a formerly opaque tile transparent and back.
  memset(ppu->vram + 16, 0, 16 * sizeof(uint16_t));
  Build(&f, &samples);
  assert(samples.main[0] == 0 && samples.sub[0] == 0);
  for (unsigned y = 0; y < 8; y++) ppu->vram[16 + y] = 0xffff;
  Build(&f, &samples);
  assert(samples.main[0] == Color(0x7c00));

  // IRQ-selected banks on the same frame retain independent textures.
  for (unsigned y = 16; y < 32; y++) lines[y].bg[0].tileAdr = 0x1000;
  Build(&f, &samples);
  assert(samples.main[0] == Color(0x7c00));
  assert(samples.main[1] == Color(0x001f));
  assert(!memcmp(samples.main, samples.sub, sizeof(samples.main)));

  // A stable new bank and a wrapped frame counter cannot reuse old slots.
  for (unsigned y = 0; y < 32; y++) lines[y].bg[0].tileAdr = 0x1000;
  Build(&f, &samples);
  assert(samples.main[0] == Color(0x001f));
  atlas->frame = UINT32_MAX;
  ppu->cgram[1] = 0x7fff;
  Build(&f, &samples);
  assert(samples.main[0] == Color(0x7fff));
  assert(!memcmp(samples.main, samples.sub, sizeof(samples.main)));

  free(ppu); free(atlas); free(pixels);
  return 0;
}
