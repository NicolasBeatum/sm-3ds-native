/* Native item symbols share the existing atlas, with no map overlay pass. */
#include "ppu_gpu_model.h"
#include "native_map_tile.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static Ppu memory;
static PicaLine lines[64];
static PicaAtlas atlas;
static uint32_t pixels[PICA_ATLAS_W*PICA_ATLAS_H],actual[256*64];
static uint16_t originalVram[32768];
static uint8_t collected[256];
static unsigned frames;
static const unsigned dotTiles[]={0x6f,0x8e,0x8f,0x5e,0x6e,0x76,0x77};
static unsigned Morton(unsigned x,unsigned y) {
  return (x&1)|((y&1)<<1)|((x&2)<<1)|((y&2)<<2)|
      ((x&4)<<2)|((y&4)<<3);
}
static uint32_t Color(unsigned rgb) {
  return (((rgb&31)*8+1)<<24)|((((rgb>>5)&31)*8+1)<<16)|
      ((((rgb>>10)&31)*8+1)<<8)|255;
}
static bool Capture(void *context,unsigned group,const PicaQuad *q) {
  (void)context;
  if((group && group!=23) || q->depth==1) return true;
  for(int y=q->y0;y<q->y1;y++) for(int x=q->x0;x<q->x1;x++) {
    double u=q->u0+(x-q->x0+.5)*(q->u1-q->u0)/(q->x1-q->x0);
    double v=q->v0+(y-q->y0+.5)*(q->v1-q->v0)/(q->y1-q->y0);
    unsigned tx=(unsigned)(u/4),ty=(unsigned)((4096-v)/8);
    unsigned slot=(ty/8)*128+tx/8;
    assert(slot<PICA_SLOTS);
    uint32_t rgba=pixels[slot*64+Morton(tx&7,ty&7)];
    if(rgba&255) actual[y*256+x]=rgba;
  }
  return true;
}
static void Check(PicaFrame *f) {
  memset(actual,0,sizeof(actual));
  PicaAtlasBegin(&atlas);
  if(!PicaBuildFrame(f)) {fprintf(stderr,"Frame %u: %s\n",frames,f->failure);assert(0);}
  frames++;
  unsigned layer=f->nativeMapLayer,words=layer==2?8:16;
  for(unsigned y=0;y<f->height;y++) for(unsigned x=0;x<256;x++) {
    const BgLayer *b=&lines[y].bg[layer];
    unsigned wx=(x+b->hScroll)&(b->tilemapWider?511:255);
    unsigned wy=(y+1+b->vScroll)&255;
    unsigned col=(wx>>3)&31,row=wy>>3;
    unsigned map=b->tilemapAdr+row*32+col+(wx>=256?1024:0);
    unsigned t=memory.vram[map],sx=(wx&7)^((t&0x4000)?7:0);
    unsigned sy=(wy&7)^((t&0x8000)?7:0),bit=7-sx;
    unsigned address=b->tileAdr+(t&1023)*words;
    unsigned bits=memory.vram[address+sy];
    if(words==16) bits|=(uint32_t)memory.vram[address+sy+8]<<16;
    unsigned index=((bits>>bit)&1)|((bits>>(bit+7))&2)|
        ((bits>>(bit+14))&4)|((bits>>(bit+21))&8);
    int mx=col+(wx>=256?32:0),my=row;
    bool circle=f->nativeMapCircle && NativeMap_Variant(t);
    if(layer==2) {
      circle&=row>=1 && row<=3 && col>=26 && col<=30;
      mx=f->nativeMapX+(int)col-28;my=f->nativeMapY+(int)row-2;
    }
    circle&=(unsigned)mx<64 && (unsigned)my<32;
    if(circle) {
      unsigned byte=((mx>>3)&3)+(mx>>5)*128+my*4;
      circle=(collected[byte]&(0x80>>(mx&7)))!=0;
    }
    if(circle && sx>=2 && sx<=5 && sy>=2 && sy<=5) {
      bool edge=(sy==2 || sy==5)?(sx==3 || sx==4):(sx==2 || sx==5);
      index=edge?2:1;
    }
    unsigned palette=(t&0x1c00)>>(layer==2?8:6);
    uint32_t expected=index?Color(memory.cgram[palette+index]):0;
    assert(actual[y*256+x]==expected);
  }
  assert(!memcmp(memory.vram,originalVram,sizeof(originalVram)));
}
static void Setup(unsigned layer) {
  memset(&memory,0,sizeof(memory));memset(lines,0,sizeof(lines));
  memset(collected,0,sizeof(collected));
  unsigned words=layer==2?8:16;
  for(unsigned y=0;y<64;y++) {
    lines[y].mode=1;lines[y].brightness=15;lines[y].bg3priority=true;
    lines[y].screenEnabled[0]=1<<layer;
    lines[y].bg[layer].tilemapAdr=layer==2?0x5800:0x3000;
    lines[y].bg[layer].tilemapWider=layer==0;
  }
  for(unsigned variant=0;variant<NATIVE_MAP_DOT_VARIANTS;variant++) {
    unsigned tile=dotTiles[variant];
    for(unsigned y=0;y<8;y++) for(unsigned x=0;x<8;x++) {
      unsigned index=x==0 || y==0 || (x>=3 && x<=4 && y>=3 && y<=4)?2:1;
      for(unsigned plane=0;plane<(words==8?2:4);plane++)
        memory.vram[tile*words+(plane/2)*8+y]|=
            ((index>>plane)&1)<<(7-x+(plane&1)*8);
    }
  }
  for(unsigned i=0;i<128;i++) memory.cgram[i]=(i*313)&0x7fff;
}
int main(void) {
  PicaFrame f={.memory=&memory,.lines=lines,.atlas=&atlas,.pixels=pixels,
      .width=256,.height=32,.hudEndY=31,.worldRight=256,.emit=Capture,
      .nativeMapCircle=collected,.nativeMapX=12,.nativeMapY=5,.nativeMapLayer=2};
  PicaAtlasInit(&atlas,pixels);Setup(2);
  collected[5*4+1]=0x20; /* World (10,5), native HUD row 2 / column 26 (DMA starts at 5820). */
  for(unsigned variant=0;variant<NATIVE_MAP_DOT_VARIANTS;variant++) for(unsigned flip=0;flip<4;flip++) {
    for(unsigned col=25;col<32;col++)
      memory.vram[0x5820+32+col]=dotTiles[variant]|(flip<<14)|0x1400;
    memcpy(originalVram,memory.vram,sizeof(originalVram));
    Check(&f);Check(&f);assert(!atlas.decodes);
  }
  /* Collected and original versions coexist, without corrupting one another. */
  bool original=false,circle=false;
  for(unsigned i=1;i<PICA_SLOTS;i++) if(atlas.tile[i].used==atlas.frame) {
    if(atlas.tile[i].key&(1u<<24)) circle=true;else original=true;
  }
  assert(original && circle);
  memory.cgram[22]^=0x7fff;Check(&f);assert(atlas.decodes==2);
  f.nativeMapCircle=NULL;Check(&f);
  f.nativeMapCircle=collected;Check(&f);assert(!atlas.decodes);
  /* All three native minimap rows, including the transparent reduced HUD. */
  for(unsigned flip=0;flip<4;flip++) for(unsigned row=1;row<=3;row++) {
    memset(collected,0,sizeof(collected));collected[(5+row-2)*4+1]=0x20;
    memory.vram[0x5800+row*32+26]=0x77|(flip<<14)|0x1400;
    memcpy(originalVram,memory.vram,sizeof(originalVram));Check(&f);Check(&f);assert(!atlas.decodes);
    f.nativeHud=lines;f.expandedHudLines=31;f.hudMode=kMainHudEnergyMap;
    Check(&f);Check(&f);assert(!atlas.decodes);f.nativeHud=NULL;
  }
  /* The pause map uses the native 4bpp tile and horizontal tilemap page. */
  Setup(0);f.nativeMapLayer=0;f.height=64;
  for(unsigned y=0;y<64;y++) lines[y].bg[0].hScroll=248;
  collected[128+4]=0x80; /* World (32,1), in tilemap page 2. */
  for(unsigned variant=0;variant<NATIVE_MAP_DOT_VARIANTS;variant++) for(unsigned flip=0;flip<4;flip++) {
    unsigned tile=dotTiles[variant];
    memory.vram[0x3000+1024+32]=tile|(flip<<14)|0x1000;
    memory.vram[0x3000+1024+33]=tile|(flip<<14)|0x1000;
    memcpy(originalVram,memory.vram,sizeof(originalVram));Check(&f);
  }
  puts("Native map symbols: HUD and pause pixels, original walls/colors, collected-only changes, flips, tilemap pages, palette updates and atlas reuse passed.");
}
