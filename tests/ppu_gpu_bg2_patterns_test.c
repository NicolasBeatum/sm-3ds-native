/* Exact native BG2 raster checks for HDMA scenery strips. No ROM needed. */
#include "ppu_gpu_model.h"
#include "scene_lighting.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Ppu memory;
static PicaLine lines[224];
static PicaAtlas atlas;
static uint32_t atlasPixels[PICA_ATLAS_W*PICA_ATLAS_H];
static PicaQuad *quads[PICA_GROUPS];
static unsigned count[PICA_GROUPS],capacity[PICA_GROUPS];
static bool Capture(void *context,unsigned group,const PicaQuad *q) {
  (void)context;
  if(count[group]==capacity[group]) {
    capacity[group]=capacity[group]?capacity[group]*2:256;
    quads[group]=realloc(quads[group],capacity[group]*sizeof(*q));
    assert(quads[group]);
  }
  quads[group][count[group]++]=*q;
  return true;
}
static uint32_t Sample(const PicaQuad *q,int x,int y) {
  double u=q->u0+(x-q->x0+.5)*(q->u1-q->u0)/(q->x1-q->x0);
  double v=q->v0+(y-q->y0+.5)*(q->v1-q->v0)/(q->y1-q->y0);
  unsigned tx=(unsigned)(u/4),ty=(unsigned)((4096-v)/8);
  unsigned ux=tx&7,uy=ty&7;
  unsigned morton=(ux&1)|((uy&1)<<1)|((ux&2)<<1)|((uy&2)<<2)|
      ((ux&4)<<2)|((uy&4)<<3);
  unsigned slot=(ty/8)*128+tx/8;
  assert(slot<PICA_SLOTS);
  return atlasPixels[slot*64+morton];
}

static uint32_t actual[400*224];
static uint16_t actualZ[400*224];
static unsigned frameNumber;

static unsigned Check(PicaFrame *f) {
  memset(count,0,sizeof(count));
  memset(actual,0,sizeof(actual));memset(actualZ,0,sizeof(actualZ));
  PicaAtlasBegin(f->atlas);assert(PicaBuildFrame(f));
  unsigned tiles=0;
  for(unsigned i=0;i<count[0];i++) {
    PicaQuad *q=&quads[0][i];if(q->depth==1)continue;tiles++;
    for(int y=q->y0;y<q->y1;y++)for(int x=q->x0;x<q->x1;x++) {
      uint32_t rgba=Sample(q,x,y);if(!(rgba&255))continue;
      if(f->lighting) {
        uint32_t lit=rgba&255;
        lit|=(((rgba>>24)*q->r/255)<<24);
        lit|=(((((rgba>>16)&255)*q->g/255))<<16);
        lit|=(((((rgba>>8)&255)*q->b/255))<<8);rgba=lit;
      }
      actual[y*f->width+x]=rgba;actualZ[y*f->width+x]=q->depth;
    }
  }
  for(unsigned y=0;y<f->height;y++)for(unsigned x=0;x<f->width;x++) {
    PicaLine *l=&lines[y];BgLayer *b=&l->bg[1];uint32_t expected=0;unsigned z=0;
    bool visible=!l->forcedBlank && (l->screenEnabled[0]&2);
    if(f->boundBg2 && f->width==400 && y>=f->hudEndY &&
        ((int)x<f->bg2Left || (int)x>=f->bg2Right))visible=false;
    if(l->screenWindowed[0]&2) {
      int relative=(int)x-(int)f->originX;
      if(relative>=l->window1left && relative<=l->window1right)visible=false;
    }
    if(visible) {
      unsigned wx=(x-f->originX+b->hScroll)&511,wy=(y+1+b->vScroll)&255;
      unsigned map=b->tilemapAdr+((wy>>3)&31)*32+((wx>>3)&31)+(wx>=256?1024:0);
      unsigned t=memory.vram[map&32767],sx=(wx&7)^((t&0x4000)?7:0),sy=(wy&7)^((t&0x8000)?7:0);
      unsigned bits=memory.vram[(b->tileAdr+(t&1023)*16+sy)&32767],bit=7-sx;
      unsigned high=memory.vram[(b->tileAdr+(t&1023)*16+sy+8)&32767];
      unsigned index=((bits>>bit)&1)|(((bits>>(bit+8))&1)<<1)|(((high>>bit)&1)<<2)|(((high>>(bit+8))&1)<<3);
      if(index) {
        unsigned rgb=memory.cgram[((t&0x1c00)>>6)+index];
        expected=(((rgb&31)*8+1)<<24)|((((rgb>>5)&31)*8+1)<<16)|((((rgb>>10)&31)*8+1)<<8)|255;
        z=(t&0x2000)?0xb100:0x7100;
        if(f->lighting) {
          int left=(int)x-(int)(wx&7),right=left+8;
          int start=0,end=f->width;
          if(f->boundBg2 && f->width==400){start=f->bg2Left;end=f->bg2Right;}
          if(left<start)left=start;
          if(right>end)right=end;
          if(l->screenWindowed[0]&2) {
            int a=f->originX+l->window1left,b=f->originX+l->window1right;
            if((int)x<a && right>a)right=a;
            if((int)x>b && left<=b)left=b+1;
          }
          const uint8_t *c=f->lighting->color[1][y>>SCENE_FIELD_SHIFT][((left+right)/2)>>SCENE_FIELD_SHIFT];
          expected=((((expected>>24)*c[0]/255))<<24)|(((((expected>>16)&255)*c[1]/255))<<16)|(((((expected>>8)&255)*c[2]/255))<<8)|255;
        }
      }
    }
    assert(actual[y*f->width+x]==expected);
    assert(actualZ[y*f->width+x]==z);
  }
  frameNumber++;
  return tiles;
}
int main(void) {
  PicaFrame f={.memory=&memory,.lines=lines,.atlas=&atlas,.pixels=atlasPixels,
      .width=400,.height=64,.originX=72,.hudEndY=31,.worldRight=400,.emit=Capture};
  for(unsigned y=0;y<64;y++) {
    lines[y].mode=1;lines[y].brightness=15;lines[y].bg3priority=true;
    lines[y].screenEnabled[0]=y<31?0:2;lines[y].mathEnabled=2;
    lines[y].bg[1].tilemapAdr=0x4000;lines[y].bg[1].tilemapWider=true;
    lines[y].bg[1].hScroll=y*3;lines[y].bg[1].vScroll=249;
  }
  memory.cgram[1]=31;memory.cgram[2]=31<<5;memory.cgram[3]=31<<10;
  memory.cgram[5]=0x3ff;memory.cgram[6]=0x7c1f;memory.cgram[7]=0x7fe0;
  for(unsigned i=0;i<2048;i++)memory.vram[0x4000+i]=1;
  for(unsigned y=0;y<8;y++) {memory.vram[16+y]=(y&1)?0x55aa:0x3cc3;memory.vram[32+y]=0x0ff0;}
  PicaAtlasInit(&atlas,atlasPixels);
  assert(Check(&f)==33);assert(atlas.repeats[PICA_REPEAT_PATTERNS].frame==atlas.frame);
  /* Palette and graphics changes update the reserved strip immediately. */
  memory.cgram[2]=0x7fff;Check(&f);memory.vram[18]^=0xffff;Check(&f);
  for(unsigned flip=0;flip<4;flip++) {
    for(unsigned i=0;i<2048;i++)memory.vram[0x4000+i]=1|((flip&1)?0x4000:0)|((flip&2)?0x8000:0);
    assert(Check(&f)==33);
  }
  /* A second pattern/palette/priority must keep independent texture data. */
  for(unsigned i=0;i<2048;i++)memory.vram[0x4000+i]=(i&16)?0xa402:0x4001;
  Check(&f);
  for(unsigned y=31;y<64;y++) {lines[y].screenWindowed[0]=2;lines[y].windowsel=2<<4;lines[y].window1left=40;lines[y].window1right=79;}
  f.boundBg2=true;f.bg2Left=20;f.bg2Right=380;Check(&f);
  lines[45].forcedBlank=true;Check(&f);lines[45].forcedBlank=false;
  /* Eight different descriptors exercise phase rotation and eight pinned
   * strips. Further distinct patterns in the same frame must use tiles. */
  f.height=128;f.boundBg2=false;
  for(unsigned y=0;y<128;y++) {
    lines[y]=lines[y%31];lines[y].screenEnabled[0]=y<31?0:2;
    lines[y].screenWindowed[0]=0;lines[y].bg[1].hScroll=y*11;
  }
  for(unsigned palette=0;palette<4;palette++) for(unsigned i=1;i<4;i++)
    memory.cgram[palette*4+i]=(i*57+palette*31)&0x7fff;
  for(unsigned y=0;y<8;y++) {
    memory.vram[48+y]=0x9966;memory.vram[64+y]=0x5aa5;
  }
  for(unsigned t=5;t<=8;t++) for(unsigned y=0;y<8;y++) {
    memory.vram[t*16+y]=0x1e78^(t*0x0101+y*0x1111);
    memory.vram[t*16+y+8]=(t+y)*0x121;
  }
  for(unsigned i=0;i<2048;i++) {
    unsigned row=(i%1024)/32;
    memory.vram[0x4000+i]=(1+(i&7))|((row&3)<<14)|((row&12)<<8);
  }
  Check(&f);
  unsigned pinned=0;
  for(unsigned i=PICA_REPEAT_PATTERNS;i<PICA_REPEAT_PATTERNS+PICA_SCENERY_PATTERNS;i++)pinned+=atlas.repeats[i].frame==atlas.frame;
  assert(pinned==PICA_SCENERY_PATTERNS);
  memory.vram[31]^=0xffff;memory.cgram[9]=0x7fff;Check(&f);
  atlas.frame=UINT32_MAX;Check(&f);assert(atlas.frame==1);
  f.height=64;
  /* Uniform and fully transparent rows use the smaller constant-U path. */
  for(unsigned i=0;i<2048;i++)memory.vram[0x4000+i]=1;
  for(unsigned y=0;y<8;y++)memory.vram[16+y]=(y&1)?0xffff:0;
  Check(&f);
  f.width=f.worldRight=256;f.originX=0;f.boundBg2=false;Check(&f);
  /* Lighting must retain the color of every original tile midpoint. */
  static SceneLighting light;f.lighting=&light;
  for(unsigned wide=0;wide<2;wide++) {
    f.width=f.worldRight=wide?400:256;f.originX=wide?72:0;
    SceneLighting_Begin(&light,true,0xd5a7,8,0,0,128,107,f.originX,0);
    Check(&f);
  }
  assert(sizeof(PicaTile)==80);
  printf("BG2 strips: %u frames, exact pixels/priority, 8-tile patterns, flips, map wrap, lighting, palette/VRAM updates, cache fallback and both widths passed.\n",frameNumber);
}
