#include "scene_lighting.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint16_t blocks[16*14];
static uint32_t pixels[PICA_ATLAS_W*PICA_ATLAS_H];
static SceneLighting scene;
static bool Accept(void *ctx,unsigned group,const PicaQuad *q) {
  (void)ctx; (void)group; (void)q; return true;
}
static void Frame(PicaFrame *f) {
  SceneLighting_Begin(&scene,true,0xa70b,8,0,0,128,152,0,0);
  PicaAtlasBegin(f->atlas); assert(PicaBuildFrame(f));
}

static void Begin(void) {
  SceneLighting_Begin(&scene,true,0xa70b,8,0,0,128,152,0,0);
  for(unsigned i=0;i<16*14;i++) blocks[i]=0x80ff;
}

int main(void) {
  PicaFrame f={.lighting=&scene,.pixels=pixels,.width=256,.height=224,
      .hudEndY=32,.worldRight=256,.wideRoom={{.blocks=blocks}},
      .wideRoomWidth=16,.wideRoomHeight=14};
  for(unsigned color=0;color<4;color++) {
    Begin();
    unsigned id=color*4;
    blocks[5*16+1]=0xc400|id;
    blocks[6*16+1]=0xd420|id;
    blocks[7*16+1]=0xdc20|id;
    blocks[8*16+1]=0xdc00|id;
    SceneLighting_AddRoomSources(&f);
    assert(scene.count==2 && scene.lights[1].x==24 && scene.lights[1].y==112);
    SceneLight l=scene.lights[1];
    assert(l.radius==88 && (l.r>=156 || l.g>=156 || l.b>=156));
    if(color==0) assert(l.g>l.r && l.g>l.b);
    if(color==1) assert(l.r>l.g && l.r>l.b);
    if(color==2) assert(l.r>l.b && l.g>l.b);
    if(color==3) assert(l.b>l.g && l.b>l.r);
    /* A removed/open cap cannot leave a cached ghost light behind. */
    Begin(); SceneLighting_AddRoomSources(&f); assert(scene.count==1);
    unsigned horizontal=0x10+color*4;
    blocks[5*16+5]=0xc400|(horizontal+1);
    blocks[5*16+6]=0x5400|horizontal;
    blocks[5*16+7]=0x5000|horizontal;
    blocks[5*16+8]=0x5000|(horizontal+1);
    SceneLighting_AddRoomSources(&f);
    assert(scene.count==2 && scene.lights[1].x==112 && scene.lights[1].y==88);
  }
  Begin();
  /* Fully open blue cap animation graphics must not emit. */
  blocks[5*16+1]=0x800f; blocks[6*16+1]=0x002f;
  SceneLighting_AddRoomSources(&f); assert(scene.count==1);
  for(unsigned type=2;type<=6;type+=2) {
    Begin(); scene.liquidType=type; scene.liquidY=180;
    SceneLighting_AddRoomSources(&f);
    assert(scene.count==(type==6?1u:3u)); /* Ordinary water is not emissive. */
    if(type!=6) assert(scene.lights[1].radius==160 && scene.lights[1].r==154);
    if(type==2) assert(scene.lights[1].r>scene.lights[1].g);
    if(type==4) assert(scene.lights[1].r>scene.lights[1].g);
  }
  Begin(); scene.liquidType=2; scene.liquidY=400;
  SceneLighting_AddRoomSources(&f); assert(scene.count==1);
  Begin(); scene.liquidType=2; scene.liquidY=180;
  f.width=400; f.worldLeft=72; f.worldRight=328;
  SceneLighting_AddRoomSources(&f); assert(scene.count==4);
  for(unsigned i=1;i<4;i++) assert(scene.lights[i].x>=72 && scene.lights[i].x<328);
  Begin(); scene.liquidType=2; scene.liquidY=-100;
  SceneLighting_AddRoomSources(&f); assert(scene.lights[1].y>=32);
  for(unsigned i=0;i<20;i++) SceneLighting_AddLight(&scene,128,100,48,100,80,40);
  SceneLighting_AddRoomSources(&f); assert(scene.count==SCENE_MAX_LIGHTS);
  /* Small colored cores emit; flat bright walls and white metal do not. */
  Begin();
  for(unsigned i=0;i<8;i++) pixels[64+i]=0x60f030ff;
  SceneEmission e=SceneLighting_Emission(&f,1);
  assert(e.g>e.r && e.g>e.b);
  for(unsigned i=0;i<64;i++) pixels[128+i]=0xf0f0f0ff;
  e=SceneLighting_Emission(&f,2); assert(!e.r && !e.g && !e.b);
  for(unsigned i=0;i<64;i++) pixels[192+i]=0x60f030ff;
  e=SceneLighting_Emission(&f,3); assert(!e.r && !e.g && !e.b);
  for(unsigned i=0;i<8;i++) pixels[256+i]=0xffffffff;
  e=SceneLighting_Emission(&f,4); assert(!e.r && !e.g && !e.b);
  /* Palette changes are reclassified on the next frame. */
  memset(pixels+64,0,64*4); Begin();
  e=SceneLighting_Emission(&f,1); assert(!e.r && !e.g && !e.b);
  static Ppu p;
  static PicaLine lines[224];
  Begin(); f.memory=&p; f.lines=lines;
  p.cgram[1]=31<<5; /* A green liquid palette must produce green light. */
  scene.liquidType=4; scene.liquidY=180;
  SceneLighting_AddRoomSources(&f);
  assert(scene.lights[1].g>scene.lights[1].r && scene.lights[1].g>scene.lights[1].b);
  /* The CPU saves work by retaining classification and sampled lamp locations.
   * Palette, scroll/window and room changes must still invalidate immediately. */
  static PicaAtlas atlas;
  memset(&p,0,sizeof(p)); memset(lines,0,sizeof(lines));
  for (unsigned y=0;y<224;y++) {
    lines[y].mode=1; lines[y].brightness=15; lines[y].screenEnabled[0]=1;
    lines[y].bg[0].tilemapAdr=0x4000;
  }
  for (unsigned i=0;i<1024;i++) p.vram[0x4000+i]=1;
  p.vram[16]=0xff; p.cgram[1]=31<<5; /* Eight green core pixels. */
  f.width=f.worldRight=256; f.worldLeft=0; f.atlas=&atlas; f.emit=Accept;
  Begin(); PicaAtlasInit(&atlas,pixels); Frame(&f);
  assert(scene.lampCount==2 && scene.lamps[0].g>scene.lamps[0].r);
  assert(scene.ambient[1][1]>101 && scene.ambient[1][0]<88);
  assert(sizeof(PicaTile)==80); /* Classification consumes old padding only. */
  unsigned revision=atlas.contentRevision;
  Frame(&f); assert(scene.lampAge==1 && !atlas.decodes &&
      atlas.contentRevision==revision);
  Frame(&f); assert(scene.lampAge==2);
  Frame(&f); assert(scene.lampAge==3);
  Frame(&f); assert(scene.lampAge==0);
  p.cgram[1]=(31<<10)|(23<<5); Frame(&f); /* Bright cyan/blue core. */
  assert(scene.lampAge==0 && scene.lamps[0].b>scene.lamps[0].g &&
      atlas.contentRevision!=revision);
  assert(scene.ambient[1][2]-121>scene.ambient[1][1]-101);
  for (unsigned y=0;y<224;y++) lines[y].bg[0].hScroll=8;
  Frame(&f); assert(scene.lampAge==0);
  Frame(&f); assert(scene.lampAge==1);
  for (unsigned y=0;y<224;y++) {
    lines[y].screenWindowed[0]=1; lines[y].windowsel=2;
    lines[y].window1left=0; lines[y].window1right=255;
  }
  Frame(&f); assert(scene.lampAge==0 && !scene.lampCount);
  /* Undo the window and animate the core into a broad bright wall: no light
   * survives after a reclassification, including reuse of atlas storage. */
  for (unsigned y=0;y<224;y++) lines[y].screenWindowed[0]=0;
  for (unsigned i=0;i<8;i++) p.vram[16+i]=0xff;
  Frame(&f); assert(!scene.lampCount);
  revision=atlas.contentRevision;
  PicaAtlasInit(&atlas,pixels);
  assert(atlas.contentRevision!=revision);
  Frame(&f); assert(!scene.lampCount);
  puts("Environment: live colored caps, opened doors, lava/acid/water, bounds, light budget and lamp cores passed.");
}
