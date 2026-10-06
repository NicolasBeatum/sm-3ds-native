#include "ppu_gpu_model.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static PicaAtlas atlas;
static uint32_t pixels[PICA_ATLAS_W * PICA_ATLAS_H];
static unsigned expected[PICA_SLOTS], uploaded[PICA_SLOTS], calls;
static bool reject;

static bool Clean(void *context, const void *data, size_t bytes) {
  assert(context == &atlas && bytes && bytes % 256 == 0);
  unsigned start = ((const uint32_t *)data - pixels) / 64;
  unsigned end = start + bytes / 256;
  assert(start > 0 && end <= PICA_SLOTS);
  assert(start == 1 || !expected[start-1]);
  assert(end == PICA_SLOTS || !expected[end]);
  calls++;
  for (unsigned i=start;i<end;i++) {
    assert(expected[i] && !uploaded[i]);
    uploaded[i]++;
  }
  return !reject;
}

int main(void) {
  uint32_t random = 37;
  for (unsigned scenario=0;scenario<100;scenario++) {
    memset(atlas.dirty,0,sizeof(atlas.dirty));
    memset(expected,0,sizeof(expected));
    /* Clean, full, alternating, across-word ranges, then random subsets. */
    for(unsigned i=1;i<PICA_SLOTS;i++) {
      random = random * 1664525u + 1013904223u;
      bool dirty = scenario == 1 || (scenario == 2 && (i & 1)) ||
          (scenario == 3 && i>=29 && i<101) ||
          (scenario>=4 && (random >> 24)<scenario);
      if(dirty) { atlas.dirty[i/32] |= 1u << (i&31); expected[i]=1; }
    }
    atlas.dirty[0] |= 1; /* Reserved white tile is never a frame upload. */
    uint32_t original[PICA_SLOTS/32];
    memcpy(original,atlas.dirty,sizeof(original));
    reject=true; calls=0; memset(uploaded,0,sizeof(uploaded));
    assert(PicaAtlasFlush(&atlas,pixels,Clean,&atlas) == (scenario==0));
    assert(!memcmp(original,atlas.dirty,sizeof(original)));
    assert(!memcmp(expected,uploaded,sizeof(expected)));
    reject=false; calls=0; memset(uploaded,0,sizeof(uploaded));
    assert(PicaAtlasFlush(&atlas,pixels,Clean,&atlas));
    assert(!memcmp(expected,uploaded,sizeof(expected)));
    assert(atlas.dirty[0]==1);
    for(unsigned i=1;i<PICA_SLOTS/32;i++) assert(!atlas.dirty[i]);
    calls=0;
    assert(PicaAtlasFlush(&atlas,pixels,Clean,&atlas) && !calls);
  }
  puts("Atlas dirty ranges: exact contiguous uploads, word boundaries and failed-upload retries passed.");
}
