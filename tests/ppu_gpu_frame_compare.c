/* Differential raster check. Link the previous model with its public
 * functions renamed to Reference*; see docs/hud-grapple-performance-trial.md. */
#define _POSIX_C_SOURCE 200809L
#include "ppu_gpu_model.h"
#include "wide_config.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void ReferenceAtlasInit(PicaAtlas *, uint32_t *);
void ReferenceAtlasBegin(PicaAtlas *);
bool ReferenceBuildFrame(PicaFrame *);

typedef struct Output { unsigned n;
 const uint32_t *pixels; uint32_t image[2][400*224]; uint16_t depth[2][400*224]; uint8_t object[2][400*224];
} Output;
static const unsigned morton[64]={0,1,4,5,16,17,20,21,2,3,6,7,18,19,22,23,8,9,12,13,24,25,28,29,10,11,14,15,26,27,30,31,32,33,36,37,48,49,52,53,34,35,38,39,50,51,54,55,40,41,44,45,56,57,60,61,42,43,46,47,58,59,62,63};
static bool Capture(void *ctx, unsigned group, const PicaQuad *q) {
  Output *out = ctx;
  assert(out->n < 150000);
  out->n++;
  if(group<4) {
   unsigned screen=group/2;
   for(int y=q->y0;y<q->y1;y++)for(int x=q->x0;x<q->x1;x++){
    int pos=y*400+x; uint32_t pixel;
    if(q->depth==1)pixel=(q->r<<24)|(q->g<<16)|(q->b<<8)|q->a;
    else {
     double u4=q->u0+((x-q->x0+.5)*(q->u1-q->u0))/(q->x1-q->x0);
     double v8=q->v0+((y-q->y0+.5)*(q->v1-q->v0))/(q->y1-q->y0);
     unsigned u=(unsigned)(u4/4), v=(unsigned)((4096-v8)/8);
     unsigned slot=(v/8)*128+u/8; assert(slot<PICA_SLOTS);
     pixel=out->pixels[slot*64+morton[(v%8)*8+u%8]];
     if(!(pixel&255))continue;
     pixel=(pixel&0xffffff00)|q->a;
    }
    if((group&1) && out->object[screen][pos])continue;
    if(q->depth>out->depth[screen][pos]){
      out->depth[screen][pos]=q->depth;out->image[screen][pos]=pixel;
    }
    if(group&1)out->object[screen][pos]=1;
   }
  }
  return true;
}
static bool Discard(void *ctx, unsigned group, const PicaQuad *q) {
  (void)ctx; (void)group; (void)q; return true;
}
static double Now(void) {
  struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec + t.tv_nsec * 1e-9;
}
static void Setup(Ppu *p, PicaLine *lines, unsigned width, unsigned variation) {
  memset(p, 0, sizeof(*p));
  for (unsigned i = 0; i < 0x3000; ++i) p->vram[i] = (i * 197u) ^ (i >> 3);
  for (unsigned i = 0; i < 256; ++i) p->cgram[i] = i * 113u;
  for (unsigned layer = 0; layer < 3; ++layer)
    for (unsigned i = 0; i < 2048; ++i)
      p->vram[0x4000 + layer * 0x800 + i] = (i % (variation ? 800 : 32)) |
          ((i % 8) << 10) | ((i % 7) << 13);
  for (unsigned i=0; i<128; i++) {
    p->oam[i*2] = (i*17%256) | ((i*13%256)<<8);
    p->oam[i*2+1] = 1 | ((i%8)<<9) | ((i%4)<<12) | ((i%3)<<14);
    p->highOam[i/4] |= ((i%3 == 0 ? 2 : 0) << ((i%4)*2));
  }
  memset(p->vram + 0x20, 0, 16 * sizeof(uint16_t));
  for (unsigned y = 0; y < kSnesHeight; ++y) {
    PicaLine *l = &lines[y]; memset(l, 0, sizeof(*l));
    l->mode = 1; l->brightness = 15;
    l->screenEnabled[0] = l->screenEnabled[1] = 23;
    l->objSize = variation == 3 && y > 120 ? 4 : 3;
    l->addSubscreen = true; l->mathEnabled = 7;
    for (unsigned layer = 0; layer < 3; ++layer) {
      l->bg[layer].tilemapAdr = 0x4000 + layer * 0x800;
      l->bg[layer].tileAdr = layer * 0x1000;
      l->bg[layer].tilemapWider = true;
      l->bg[layer].hScroll = variation == 2 ? (y * 3) : 13;
      l->bg[layer].vScroll = 7;
      if (variation == 3 && y >= 90 && y < 150) l->bg[layer].tileAdr += 0x400;
    }
    if (variation == 4) {
      l->screenWindowed[0] = l->screenWindowed[1] = 7;
      l->windowsel = 0x222;
      l->window1left = y % 120; l->window1right = 100 + y % 120;
    }
  }
  if(variation==5) {
   for(unsigned tile=0;tile<768;tile++)for(unsigned row=0;row<4;row++){
    p->vram[tile*16+row]=0;p->vram[tile*16+row+8]=0;
   }
   for(unsigned y=0;y<224;y++)for(unsigned l=0;l<3;l++)lines[y].bg[l].hScroll=y*3;
  }
  if(variation==6)for(unsigned y=0;y<224;y++) {
   if(y<32){lines[y].screenEnabled[1]=0;lines[y].bg[0].tileAdr=0x1000;}
   else {lines[y].screenEnabled[0]=0;lines[y].bg[0].tileAdr=0;}
  }
  if(variation==7)for(unsigned y=0;y<224;y++){
   lines[y].mathEnabled=y>=190?2:0;
   lines[y].screenEnabled[0]=y>=190?2:1;
   for(unsigned l=0;l<3;l++)lines[y].bg[l].hScroll=y*3;
  }
  if(variation==8)for(unsigned y=0;y<224;y++)for(unsigned l=0;l<3;l++)lines[y].bg[l].hScroll=y*3;
  (void)width;
}
int main(void) {
  Ppu *p = calloc(1, sizeof(*p));
  PicaLine lines[kSnesHeight];
  PicaAtlas *a[2] = {calloc(1, sizeof(PicaAtlas)), calloc(1, sizeof(PicaAtlas))};
  uint32_t *pixels[2] = {calloc(PICA_ATLAS_W * PICA_ATLAS_H, 4), calloc(PICA_ATLAS_W * PICA_ATLAS_H, 4)};
  Output *out[2] = {calloc(1, sizeof(Output)), calloc(1, sizeof(Output))};
  uint16_t blocks[64*64], table[4096];
  for(unsigned i=0;i<64*64;i++)blocks[i]=(i*37)%1024;
  for(unsigned i=0;i<4096;i++)table[i]=(i%800)|((i%8)<<10)|((i%7)<<13);
  assert(p && a[0] && a[1] && pixels[0] && pixels[1] && out[0] && out[1]);
  for (unsigned widthMode = 0; widthMode < 2; ++widthMode) {
    unsigned width = widthMode ? kWideWidth : kSnesWidth;
    for (unsigned variation = 0; variation < 9; ++variation) {
      Setup(p, lines, width, variation);
      ReferenceAtlasInit(a[0], pixels[0]); PicaAtlasInit(a[1], pixels[1]);
      PicaFrame f[2];
      for (unsigned i = 0; i < 2; ++i) f[i] = (PicaFrame){.memory=p, .lines=lines,
        .atlas=a[i], .pixels=pixels[i], .width=width, .height=kSnesHeight,
        .originX=widthMode ? kWideExtraX : 0, .worldRight=width,
        .emit=Capture, .context=out[i]};
      if(variation==8 && widthMode)for(unsigned i=0;i<2;i++) {
        f[i].wideTileTable=table;f[i].wideRoomWidth=f[i].wideRoomHeight=64;
        for(unsigned l=0;l<2;l++)f[i].wideRoom[l]=(PicaWideRoomLayer){.blocks=blocks,.cameraX=256,.cameraY=256,.scrollX=13,.scrollY=7,.followScroll=true};
      }
      for (unsigned frame = 0; frame < 12; ++frame) {
        // Palette animation, a tile changing from transparent to opaque,
        // and frame-counter rollover must all produce the original output.
        if (frame == 3) p->cgram[3] ^= 0x7fff;
        if (frame == 5) p->vram[0x20] = 0xffff;
        if (frame == 7) a[0]->frame = a[1]->frame = UINT32_MAX;
        out[0]->n = out[1]->n = 0;
        for(unsigned i=0;i<2;i++){out[i]->pixels=pixels[i];memset(out[i]->image,0,sizeof(out[i]->image));memset(out[i]->depth,0,sizeof(out[i]->depth));memset(out[i]->object,0,sizeof(out[i]->object));}
        ReferenceAtlasBegin(a[0]); PicaAtlasBegin(a[1]);
        assert(ReferenceBuildFrame(&f[0])); assert(PicaBuildFrame(&f[1]));
        assert(!memcmp(out[0]->image[0],out[1]->image[0],sizeof(out[0]->image[0])));
        assert(!memcmp(out[0]->image[1],out[1]->image[1],sizeof(out[0]->image[1])));
        assert(!memcmp(pixels[0], pixels[1], PICA_ATLAS_W * PICA_ATLAS_H * 4));
      }
      double times[2] = {0};
      for (unsigned pass = 0; pass < 3; ++pass)
        for (unsigned i = 0; i < 2; ++i) {
          f[i].emit=Discard;
          double before=Now();
          for (unsigned frame=0; frame<100; ++frame) {
            if (i) { PicaAtlasBegin(a[i]); assert(PicaBuildFrame(&f[i])); }
            else { ReferenceAtlasBegin(a[i]); assert(ReferenceBuildFrame(&f[i])); }
          }
          times[i] += Now()-before;
        }
      printf("width=%u scenario=%u reference=%.3fms cache=%.3fms change=%.1f%% quads=%u->%u\n", width, variation,
        times[0]*1000/300, times[1]*1000/300, (times[1]/times[0]-1)*100,out[0]->n,out[1]->n);
    }
  }
  puts("Main and sub color/alpha rasters match across 216 animated frames, including room block side bands.");
  return 0;
}
