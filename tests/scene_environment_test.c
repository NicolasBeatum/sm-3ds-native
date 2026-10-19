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
  /* Solid walls and isolated low graphics must not create cap lights. */
  for (unsigned i=0;i<16*14;i++) blocks[i]=0x8000;
  SceneLighting_AddRoomSources(&f); assert(scene.count==1);
  Begin(); blocks[5*16+1]=0xc00c;
  SceneLighting_AddRoomSources(&f); assert(scene.count==1);
  /* Blue door opening frames retain their emitter until the cap disappears. */
  Begin(); blocks[5*16+1]=0x840d; blocks[6*16+1]=0x842d;
  blocks[7*16+1]=0x8c2d; blocks[8*16+1]=0x8c0d;
  SceneLighting_AddRoomSources(&f); assert(scene.count==2 && scene.lights[1].b>scene.lights[1].r);
  /* A complete animated horizontal cap is valid; vertical middle tiles
   * cannot masquerade as a horizontal anchor. */
  Begin(); blocks[5*16+5]=0x843d; blocks[5*16+6]=0x843c;
  blocks[5*16+7]=0x803c; blocks[5*16+8]=0x803d;
  SceneLighting_AddRoomSources(&f); assert(scene.count==2 && scene.lights[1].x==112);
  Begin();
  for (unsigned x=1;x<=13;x+=6) {
    blocks[5*16+x]=0xc00c; blocks[6*16+x]=0xd02c;
    blocks[7*16+x]=0xd82c; blocks[8*16+x]=0xd80c;
  }
  SceneLighting_AddRoomSources(&f);
  assert(scene.count==3 && scene.lights[1].x==120 && scene.lights[2].x==216);
  Begin();
  /* Fully open blue cap animation graphics must not emit. */
  blocks[5*16+1]=0x800f; blocks[6*16+1]=0x002f;
  SceneLighting_AddRoomSources(&f); assert(scene.count==1);
  for(unsigned type=2;type<=6;type+=2) {
    Begin(); scene.liquidType=type; scene.liquidY=180;
    SceneLighting_AddRoomSources(&f);
    assert(scene.count==(type==6?1u:2u)); /* Ordinary water is not emissive. */
    if(type!=6) assert(scene.lights[1].radius==112 && scene.lights[1].r==200 && scene.lights[1].surface && !scene.lights[1].glow);
    if(type==2) assert(scene.lights[1].r>scene.lights[1].g);
    if(type==4) assert(scene.lights[1].r>scene.lights[1].g);
  }
  Begin(); scene.liquidType=2; scene.liquidY=400;
  SceneLighting_AddRoomSources(&f); assert(scene.count==1);
  Begin(); scene.liquidType=2; scene.liquidY=180;
  f.width=400; f.worldLeft=72; f.worldRight=328;
  SceneLighting_AddRoomSources(&f); assert(scene.count==2);
  assert(scene.lights[1].x==200);
  Begin(); scene.liquidType=2; scene.liquidY=-100;
  SceneLighting_AddRoomSources(&f); assert(scene.lights[1].y==-88); /* Keep the actual offscreen surface. */
  for(unsigned i=0;i<20;i++) SceneLighting_AddLight(&scene,128,100,48,100,80,40);
  SceneLighting_AddRoomSources(&f); assert(scene.count==SCENE_MAX_LIGHTS);
  static Ppu p;
  static PicaLine lines[224];
  Begin(); f.memory=&p; f.lines=lines;
  p.cgram[1]=31<<5; /* A green liquid palette must produce green light. */
  scene.liquidType=4; scene.liquidY=180;
  SceneLighting_AddRoomSources(&f);
  assert(scene.lights[1].g>scene.lights[1].r && scene.lights[1].g>scene.lights[1].b);
  /* A small bright colored texture must never become an emitter, including
   * palette/VRAM animation and atlas reinitialization. Ambient color remains. */
  static PicaAtlas atlas;
  memset(&p,0,sizeof(p)); memset(lines,0,sizeof(lines));
  for (unsigned y=0;y<224;y++) {
    lines[y].mode=1; lines[y].brightness=15; lines[y].screenEnabled[0]=1;
    lines[y].bg[0].tilemapAdr=0x4000;
  }
  for (unsigned i=0;i<1024;i++) p.vram[0x4000+i]=1;
  p.vram[16]=0xff; p.cgram[1]=31<<5;
  f.width=f.worldRight=256; f.worldLeft=0; f.atlas=&atlas; f.emit=Accept;
  Begin(); PicaAtlasInit(&atlas,pixels); Frame(&f);
  assert(scene.count==1 && scene.ambient[1][1]>101 && scene.ambient[1][0]<88);
  assert(sizeof(PicaTile)==80);
  Frame(&f); assert(scene.count==1 && !atlas.decodes);
  p.cgram[1]=(31<<10)|(23<<5); Frame(&f);
  assert(scene.count==1 && scene.ambient[1][2]-121>scene.ambient[1][1]-101);
  for (unsigned i=0;i<8;i++) p.vram[16+i]=0xff;
  Frame(&f); assert(scene.count==1);
  PicaAtlasInit(&atlas,pixels); Frame(&f); assert(scene.count==1);
  /* Unused bright palette entries cannot determine the room color. */
  p.cgram[1]=31<<5; p.cgram[15]=31|(31<<10);
  Frame(&f);
  assert(scene.ambient[1][1]==125 && scene.ambient[1][0]==64);
  /* Actual BG2 pixels take precedence over a differently colored BG1. */
  for(unsigned y=0;y<224;y++) {
    lines[y].screenEnabled[0]=3;
    lines[y].bg[1].tilemapAdr=0x4400;
    lines[y].bg[1].tileAdr=0x2000;
  }
  for(unsigned i=0;i<1024;i++) p.vram[0x4400+i]=0xc01;
  for(unsigned i=0;i<8;i++) p.vram[0x2010+i]=0xff;
  p.cgram[1]=31; p.cgram[49]=31<<10;
  Frame(&f);
  assert(scene.count==1 && !scene.backgroundCount);
  assert(scene.ambient[1][2]==145 && scene.ambient[1][0]==64);
  /* Transparent BG2 rows fall back to the visible foreground hue. */
  memset(&p.vram[0x2010],0,16*sizeof(uint16_t));
  Frame(&f);
  assert(scene.ambient[1][0]==112 && scene.ambient[1][2]==97);
  puts("Environment: colored caps, opened doors, liquids, bounds and no texture-generated lights passed.");
}
