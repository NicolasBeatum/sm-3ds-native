/* Expanded rows come from room data, selected native HUD stays unlit, and
 * all existing playfield pixels/registers remain unchanged at both widths. */
#include "ppu_gpu_model.h"
#include "wide_config.h"
#include "scene_lighting.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static Ppu memory, originalMemory;
static PicaLine lines[224], original[224], hud[31];
static PicaAtlas atlas;
static uint32_t pixels[PICA_ATLAS_W*PICA_ATLAS_H];
static uint32_t output[224][400], baseline[224][400];
static uint16_t depth[224][400], blocks[32*32], tiles[4096];
static unsigned overlay;
static bool Capture(void *ctx,unsigned group,const PicaQuad *q) {
  (void)ctx;
  if(group!=0 && group!=23)return true;
  if(group==23) {overlay++;assert(q->r==255 && q->g==255 && q->b==255);}
  for(int y=q->y0;y<q->y1;y++)for(int x=q->x0;x<q->x1;x++) {
    assert(x>=0 && x<400 && y>=0 && y<224);
    if(group!=23 && q->depth<depth[y][x])continue;
    unsigned tx=(q->u0+(x-q->x0)*(q->u1-q->u0)/(q->x1-q->x0))/4;
    unsigned ty=(4096-(q->v0+(y-q->y0)*(q->v1-q->v0)/(q->y1-q->y0)))/8;
    unsigned a=tx&7,b=ty&7,m=(a&1)|((b&1)<<1)|((a&2)<<1)|((b&2)<<2)|((a&4)<<2)|((b&4)<<3);
    uint32_t c=pixels[((ty/8)*128+tx/8)*64+m];
    if(!(c&255))continue;
    depth[y][x]=q->depth;output[y][x]=c;
  }
  return true;
}
static void Build(PicaFrame *f) {
  memset(output,0,sizeof(output));memset(depth,0,sizeof(depth));overlay=0;
  PicaAtlasBegin(&atlas);assert(PicaBuildFrame(f));
}
int main(void) {
  /* Native HUD DMA reserves row zero; the map occupies all three rows
   * at screen y=7..30, including the row below Samus. */
  assert(!MainHudKeepsPixel(kMainHudEnergyMap,208,6));
  assert(MainHudKeepsPixel(kMainHudEnergyMap,208,7));
  assert(MainHudKeepsPixel(kMainHudEnergyMap,247,30));
  assert(!MainHudKeepsPixel(kMainHudEnergyMap,248,30));
  assert(!MainHudKeepsPixel(kMainHudEnergyMap,208,31));
  memory.cgram[1]=31;memory.cgram[2]=31<<5;memory.cgram[3]=31<<10;
  memory.cgram[5]=31<<10;memory.cgram[6]=0;
  for(unsigned y=0;y<8;y++) {
    memory.vram[16+y]=255; // 4bpp tile1: palette1
    memory.vram[32+y]=0xff00; // tile2: palette2
    memory.vram[0x2000+8+y]=y<4?0x00ff:0xff00; // HUD blue glyph / opaque black background
  }
  for(unsigned i=0;i<1024;i++)memory.vram[0x1000+i]=1;
  for(unsigned col=0;col<32;col++)for(unsigned row=0;row<4;row++)
    memory.vram[0x5800+row*32+col]=0x401;
  for(unsigned i=0;i<4096;i++)tiles[i]=2;
  for(unsigned y=0;y<224;y++) {
    lines[y].mode=1;lines[y].brightness=15;
    lines[y].screenEnabled[0]=y<31?4:1;
    lines[y].screenEnabled[1]=4; // Native subscreen retains the HUD map after its IRQ.
    lines[y].bg[0].tilemapAdr=0x1000;
    lines[y].bg[2].tilemapAdr=0x5800;
    lines[y].bg[2].tileAdr=0x2000;
  }
  memcpy(original,lines,sizeof(lines));originalMemory=memory;
  PicaAtlasInit(&atlas,pixels);
  for(unsigned wide=0;wide<2;wide++) {
    unsigned width=wide?400:256,origin=wide?72:0;
    PicaFrame f={.memory=&memory,.lines=lines,.atlas=&atlas,.pixels=pixels,
      .width=width,.height=224,.originX=origin,.hudEndY=31,.worldRight=width,
      .wideRoom={{.blocks=blocks,.cameraX=80}},.wideRoomWidth=32,.wideRoomHeight=32,
      .wideTileTable=tiles,.emit=Capture};
    Build(&f);memcpy(baseline,output,sizeof(output));
    for(unsigned mode=0;mode<kMainHudModeCount;mode++) {
      memcpy(lines,original,sizeof(lines));f.hudEndY=31;f.nativeHud=NULL;f.expandedHudLines=0;
      bool expanded=PicaExpandHud(&f,lines,hud,mode);
      assert(expanded==MainHudExpands(mode));
      Build(&f);
      assert((overlay>0)==(expanded && mode!=kMainHudHidden));
      for(unsigned y=31;y<224;y++)for(unsigned x=0;x<width;x++)
        assert(output[y][x]==baseline[y][x]);
      if(expanded) {
        for(unsigned y=0;y<31;y++)assert(!(lines[y].screenEnabled[1]&4));
        for(unsigned y=0;y<31;y++)for(unsigned x=0;x<width;x++) {
          bool kept=x>=origin && x<origin+256 && MainHudKeepsPixel(mode,x-origin,y) && baseline[y][x]!=0x010101ff;
          if(output[y][x]!=(kept?baseline[y][x]:0x01f901ff)) {
            fprintf(stderr,"wide=%u mode=%u xy=%u,%u got=%08x expected=%08x kept=%d\n",wide,mode,x,y,output[y][x],kept?baseline[y][x]:0x01f901ff,kept);
            abort();
          }
        }
        assert(!memcmp(lines+31,original+31,sizeof(lines)-31*sizeof(*lines)));
        assert(!memcmp(hud,original,sizeof(hud)));
      }
      if(mode==kMainHudEnergyMap) {
        /* Transparency follows the live native palette, without poisoning
         * the ordinary HUD atlas entry or retaining a stale black mask. */
        memory.cgram[6]=31;Build(&f);
        assert(output[4][origin+8]==0xf90101ff);
        memory.cgram[6]=0;Build(&f);
        assert(output[4][origin+8]==0x01f901ff);
      }
      assert(!memcmp(&memory,&originalMemory,sizeof(memory)));
    }
    /* Real BG3 effects use another map and must remain enabled. */
    memcpy(lines,original,sizeof(lines));f.hudEndY=31;f.nativeHud=NULL;f.expandedHudLines=0;
    lines[31].bg[2].tilemapAdr=0x4800;
    assert(PicaExpandHud(&f,lines,hud,kMainHudHidden));
    for(unsigned y=0;y<31;y++)assert(lines[y].screenEnabled[1]&4);
    /* Special effects, missing geometry and fades retain reference behavior. */
    memcpy(lines,original,sizeof(lines));f.hudEndY=31;f.nativeHud=NULL;f.expandedHudLines=0;
    f.xrayActive=true;assert(!PicaExpandHud(&f,lines,hud,kMainHudHidden));f.xrayActive=false;
    f.extendEyeBeam=true;assert(!PicaExpandHud(&f,lines,hud,kMainHudHidden));f.extendEyeBeam=false;
    lines[31].brightness=14;assert(!PicaExpandHud(&f,lines,hud,kMainHudHidden));lines[31].brightness=15;
    f.wideRoom[0].blocks=NULL;assert(!PicaExpandHud(&f,lines,hud,kMainHudHidden));
    assert(!memcmp(lines,original,sizeof(lines)));
  }
  puts("Main HUD: five modes, 256/400px room expansion, native life/map selection, unchanged playfield/PPU and special-effect fallback passed.");
}
