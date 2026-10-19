#include "scene_lighting.h"
#include "wide_config.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static unsigned emitted;
static unsigned Morton(unsigned x,unsigned y) {
  return (x&1)|((y&1)<<1)|((x&2)<<1)|((y&2)<<2)|
         ((x&4)<<2)|((y&4)<<3);
}
static bool Capture(void *context, unsigned group, const PicaQuad *q) {
  (void)context;
  assert(group == 21 && q->depth == 0x8100 && (q->a == 64 || q->a == 60));
  assert(q->x0 >= 0 && q->x1 <= 256 && q->y0 >= 32 && q->y1 <= 224);
  emitted++;
  return true;
}
int main(void) {
  SceneLighting s={0};
  assert(sizeof(SceneLight)==16);
  uint32_t pixels[PICA_ATLAS_W * PICA_ATLAS_H] = {0};
  PicaFrame f = {.width=256,.height=224,.hudEndY=32,
                .worldRight=256,.pixels=pixels,.lighting=&s,.emit=Capture};
  static Ppu receiver;
  PicaLine receiverLines[224]={0};
  for(unsigned y=0;y<224;y++) {
    receiverLines[y].brightness=15;
    receiverLines[y].screenEnabled[0]=receiverLines[y].screenEnabled[1]=1;
    receiverLines[y].bg[0].tilemapAdr=0x1000;
  }
  for(unsigned i=0;i<16;i++) receiver.vram[i]=0xffff;
  f.memory=&receiver;f.lines=receiverLines;
  PicaQuad original = {100,100,108,108,0,4096,32,4032,0x8000,255,255,255,127};
  PicaQuad q = original;
  SceneLighting_Begin(&s,false,0x91f8,8,1024,1024,1152,1140,0,0);
  SceneLighting_Tile(&f,0,1,&q);
  assert(!memcmp(&q,&original,sizeof(q)));
  for(unsigned state=0;state<44;state++) {
    SceneLighting_Begin(&s,true,0x91f8,state,1024,1024,1152,1140,0,0);
    assert(s.enabled == (state >= 8 && state <= 11));
  }
  SceneLighting_Begin(&s,true,0x92fd,8,1024,1024,1152,1140,0,0);
  assert(s.enabled && !s.ship && s.count==1);
  assert(s.lights[0].x==128 && s.lights[0].y==102 && !s.lights[0].glow);
  SceneLighting_Prepare(&f);
  assert(s.color[0][6][8][1]>s.ambient[0][1]); /* Samus fill in another room. */
  assert(s.color[1][6][8][1]>s.ambient[1][1]); /* Suit still illuminates interior walls. */
  SceneLighting_SetTileset(&s,0xc1,0xb6f6);
  SceneLighting_Prepare(&f);
  assert(s.color[1][6][8][1]==s.ambient[1][1]); /* No suit light on distant clouds. */
  SceneLighting_AddLight(&s,220,44,48,88,62,28);
  SceneLighting_Prepare(&f);
  q=original; q.x0=216; q.x1=224; q.y0=40; q.y1=48;
  SceneLighting_Tile(&f,0,1,&q);
  assert(q.r>220 && q.g>210 && q.b>190 && q.r>q.g && q.g>q.b &&
         q.a==original.a);
  unsigned near=s.color[0][2][13][0];
  assert(s.color[1][2][13][0]>s.ambient[1][0]);
  s.environmentStart=s.count;
  SceneLighting_Prepare(&f);
  assert(s.color[0][2][13][0]==near &&
         s.color[1][2][13][0]==s.ambient[1][0]); /* Layer mask invalidates cache. */
  SceneLighting_Begin(&s,true,0x91f8,8,1024,1024,1152,1140,0,60);
  assert(s.count == 5 && s.samusX == 128 && s.samusY == 116);
  SceneLighting_Prepare(&f);
  q=original; q.x0=220; q.x1=228; q.y0=40; q.y1=48;
  SceneLighting_Tile(&f,0,1,&q);
  assert(q.r<255 && q.b>q.r && q.a==127 && q.depth==original.depth);
  PicaQuad hud=original; hud.y0=8; hud.y1=16;
  q=hud; SceneLighting_Tile(&f,0,1,&q);
  assert(!memcmp(&q,&hud,sizeof(q)));
  PicaQuad weather=original; weather.depth=0x1200;
  q=weather; SceneLighting_Tile(&f,0,1,&q);
  assert(!memcmp(&q,&weather,sizeof(q)));
  SceneLighting before=s;
  SceneLighting_Begin(&s,true,0x91f8,8,1040,1032,1152,1140,72,60);
  assert(s.lights[0].x==before.lights[0].x-16+72);
  assert(s.lights[0].y==before.lights[0].y-8);
  for(unsigned i=0;i<100;i++) SceneLighting_AddLight(&s,0,0,50,255,0,0);
  assert(s.count==SCENE_MAX_LIGHTS);
  /* Offscreen light bounds cannot touch the grid or resurrect old light. */
  s.count=0;
  SceneLighting_AddLight(&s,-30000,30000,256,255,255,255);
  SceneLighting_Prepare(&f);
  for(unsigned y=0;y<(f.height+SCENE_FIELD_CELL-1)/SCENE_FIELD_CELL;y++)
    for(unsigned x=0;x<(f.width+SCENE_FIELD_CELL-1)/SCENE_FIELD_CELL;x++)
      for(unsigned c=0;c<3;c++) assert(s.color[0][y][x][c]==s.fieldKey.base[0][c]);
  SceneLighting_Begin(&s,true,0x91f8,8,1024,1024,1152,1140,0,0);
  SceneLighting_Prepare(&f);
  s.castShadow=true; s.samusX=0;
  q=original; q.x0=0; q.x1=4;
  SceneLighting_Tile(&f,1,1,&q); /* Entire projected sprite falls outside. */
  assert(!s.shadowCount);
  q=original; SceneLighting_Tile(&f,1,1,&q);
  assert(s.shadowCount==1 && q.a==original.a);
  assert(SceneLighting_EmitShadows(&f,0) && emitted==1 && !s.shadowCount);
  /* Texture highlights are surface art, never fake relief or emitters. */
  SceneLighting_Begin(&s,true,0xa70b,8,0,0,128,152,0,0);
  s.count=0;
  pixels[64+Morton(3,4)]=pixels[64+Morton(4,3)]=0xffffffff;
  SceneLighting_Prepare(&f);
  q=original; SceneLighting_Tile(&f,0,1,&q);
  assert(q.r==150 && q.g==158 && q.b==170);
  q=original; q.u0=32; q.u1=0; q.v0=4032; q.v1=4096;
  SceneLighting_Tile(&f,0,1,&q);
  assert(q.r==150 && q.g==158 && q.b==170);
  /* Interiors separate distant scenery without dimming HUD or Samus as far
   * as walls. Shadows reach BG1-low (8000), remain below BG1-high (c000),
   * and point away from a real environmental source rather than charge. */
  q=original; q.depth=0x7100; SceneLighting_Tile(&f,0,1,&q);
  assert(q.r==88 && q.g==101 && q.b==121);
  q=original; SceneLighting_Tile(&f,1,1,&q);
  assert(q.r==188 && q.g==196 && q.b==208);
  SceneLighting_Begin(&s,true,0xa70b,8,0,0,128,152,0,60);
  SceneLighting_Prepare(&f);
  assert(s.shadowX==6 && s.shadowY==5); /* Own charge does not steer it. */
  SceneLighting_AddLight(&s,24,128,160,54,100,174);
  SceneLighting_Prepare(&f);
  assert(s.shadowX==8 && s.shadowY==1);
  s.castShadow=true;
  q=original; SceneLighting_Tile(&f,1,1,&q);
  assert(s.shadowCount==1 && s.shadows[0].depth>0x8000 &&
         s.shadows[0].depth<0xc000 && s.shadows[0].a==60);
  assert(s.shadows[0].x0==108 && s.shadows[0].y0==101);
  assert(SceneLighting_EmitShadows(&f,0) && emitted==2 && !s.shadowCount);
  SceneLighting_Begin(&s,true,0xa70b,8,0,0,128,152,0,0);
  SceneLighting_AddLight(&s,128,200,160,154,72,24);
  SceneLighting_Prepare(&f);
  assert(s.shadowX==0 && s.shadowY==-8); /* Lava illuminates from below. */
  /* A liquid surface gives the same diffuse color across the room, rather
   * than moving circular patches as the widescreen camera pans. The emitter
   * kind must invalidate an otherwise identical point-light cache entry. */
  SceneLighting strip={0};
  PicaFrame liquid={.width=400,.height=224,.hudEndY=32,
    .worldRight=400,.lighting=&strip};
  SceneLighting_Begin(&strip,true,0xaf14,8,0,0,2000,2000,72,0);
  strip.liquidType=2;strip.liquidY=196;
  SceneLighting_AddRoomSources(&liquid);
  assert(strip.count==2 && strip.lights[1].surface);
  SceneLighting_Prepare(&liquid);
  for(unsigned x=1;x<SCENE_FIELD_W;x++)
    assert(!memcmp(strip.color[0][11][0],strip.color[0][11][x],3));
  assert(strip.color[0][11][0][0]>strip.color[0][5][0][0]);
  assert(strip.color[0][11][0][0]>strip.color[0][11][0][1]);
  uint8_t surfaceColor=strip.color[0][11][0][0];
  strip.lights[1].x-=128;
  SceneLighting_Prepare(&liquid);
  assert(strip.color[0][11][0][0]==surfaceColor);
  strip.lights[1].surface=false;
  SceneLighting_Prepare(&liquid);
  assert(strip.color[0][11][0][0]<surfaceColor);
  /* No projected sprite on empty BG1 / distant sky. A single exposed
   * corner also rejects the piece, including flipped tiles and subscreen. */
  s.castShadow=true;
  memset(receiver.vram,0,16*sizeof(uint16_t));
  q=original;SceneLighting_Tile(&f,1,1,&q);assert(!s.shadowCount);
  for(unsigned i=0;i<16;i++) receiver.vram[i]=0xffff;
  q=original;SceneLighting_Tile(&f,1,1,&q);assert(s.shadowCount==1);
  s.shadowCount=0;
  receiverLines[100+s.shadowY].screenEnabled[0]=0;
  q=original;SceneLighting_Tile(&f,1,1,&q);assert(!s.shadowCount);
  receiverLines[100+s.shadowY].screenEnabled[0]=1;
  q=original;SceneLighting_Tile(&f,3,1,&q);assert(s.shadowCount==1);
  s.shadowCount=0;
  /* Exact light changes rebuild immediately; unrelated atlas changes and
   * buffer swaps reuse colors. Flips/cropped UVs cannot create fake light. */
  static PicaAtlas atlases[2];
  SceneLighting_Begin(&s,true,0xa70b,8,0,0,128,152,0,0); s.count=0;
  PicaAtlasInit(&atlases[0],pixels); f.atlas=&atlases[0];
  SceneLighting_Prepare(&f); q=original; SceneLighting_Tile(&f,0,1,&q);
  assert(q.r==150);
  SceneFieldKey saved=s.fieldKey;
  PicaAtlasInit(&atlases[1],pixels); f.atlas=&atlases[1];
  SceneLighting_Prepare(&f);
  assert(!memcmp(&saved,&s.fieldKey,sizeof(saved)));
  SceneLighting_AddLight(&s,104,104,64,100,20,10);
  SceneLighting_Prepare(&f); q=original; SceneLighting_Tile(&f,0,1,&q);
  assert(q.r>230 && q.g>158);
  s.lights[0].x=220;
  SceneLighting_Prepare(&f); q=original; SceneLighting_Tile(&f,0,1,&q);
  assert(q.r==150 && q.g==158 && q.b==170);
  s.count=0; SceneLighting_Prepare(&f);
  /* The native IRQ switches to the playfield at visible row 31. A whole
   * BG tile chunk starting there must be shaded, while row 30 stays HUD. */
  f.hudEndY=WideConfig_Create(false).hud_end_y;
  assert(f.hudEndY==31 && WideConfig_Create(true).hud_end_y==31);
  q=original; q.y0=30; q.y1=31;
  PicaQuad hudEdge=q;
  SceneLighting_Tile(&f,0,1,&q);
  assert(!memcmp(&q,&hudEdge,sizeof(q)));
  q=original; q.y0=31; q.y1=39;
  SceneLighting_Tile(&f,0,1,&q);
  assert(q.r==150 && q.g==158 && q.b==170 && q.y0==31 && q.y1==39);
  /* A rejected frame must restore the caller's callback/context. */
  f.width=255; f.context=&s;
  assert(!PicaBuildFrame(&f) && f.emit==Capture && f.context==&s && f.lighting==&s);
  f.xrayActive=true;
  assert(!PicaBuildFrame(&f) && !s.enabled && f.emit==Capture && f.context==&s);

  /* Contact shading follows actual solid blocks, not texture highlights.
   * Opening/destroying a block updates even when camera/lights stay still. */
  uint16_t blocks[16]={0}; blocks[5]=0x8000;
  PicaFrame contact={.width=64,.height=64,.worldRight=64,.lighting=&s,
    .wideRoom={ {.blocks=blocks} },.wideRoomWidth=4,.wideRoomHeight=4};
  SceneLighting_Begin(&s,true,0xa70b,8,0,0,512,512,0,0); s.count=0;
  SceneLighting_Prepare(&contact);
  assert(s.color[0][1][1][0]==150 && s.color[1][1][1][0]==52);
  assert(s.color[0][1][2][0]==141 && s.color[0][2][2][0]==147);
  assert(s.color[2][1][2][0]==188); /* No extra darkening of sprites. */
  blocks[5]=0;
  SceneLighting_Prepare(&contact);
  assert(s.color[0][1][2][0]==150 && s.color[1][1][1][0]==88);
  blocks[5]=0x1000; /* A slope is not a whole opaque wall. */
  SceneLighting_Prepare(&contact);
  assert(s.color[0][1][2][0]==150);
  contact.wideRoom[0].cameraX=-32;contact.wideRoom[0].cameraY=-32;
  SceneLighting_Prepare(&contact);
  assert(s.color[0][0][0][0]==150);

  /* The distant background contributes color even when foreground palettes
   * differ. Transition halos follow fading scenery, excluding bright HUD. */
  static Ppu memory;
  PicaLine lines[64]={0};
  for(unsigned y=0;y<64;y++) {
    lines[y].mode=1;lines[y].brightness=15;lines[y].screenEnabled[0]=3;
    lines[y].bg[0].tilemapAdr=0x1000;lines[y].bg[1].tilemapAdr=0x1800;
  }
  for(unsigned i=0;i<1024;i++) {
    memory.vram[0x1000+i]=1|(1<<10);memory.vram[0x1800+i]=1|(2<<10);
  }
  memory.cgram[17]=31;memory.cgram[33]=31<<5;
  contact.memory=&memory;contact.lines=lines;contact.wideRoom[0].blocks=NULL;
  SceneLighting_Begin(&s,true,0xa70b,8,0,0,512,512,0,0);
  SceneLighting_Prepare(&contact);
  assert(s.ambient[1][1]>s.ambient[1][0] && s.glowAlpha==65);
  SceneFieldKey normalKey=s.fieldKey;
  for(unsigned state=9;state<=11;state++) {
    SceneLighting_Begin(&s,true,0xa70b,state,0,0,512,512,0,0);
    SceneLighting_Prepare(&contact);
    assert(s.enabled && s.glowAlpha==65);
    assert(!memcmp(&s.fieldKey,&normalKey,sizeof(normalKey)));
  }
  memory.cgram[33]=15<<5;
  SceneLighting_Prepare(&contact);assert(s.glowAlpha==65*15/31);
  memory.cgram[33]=0;
  SceneLighting_Prepare(&contact);assert(s.glowAlpha==0); /* HUD still bright. */
  for(unsigned y=0;y<64;y++)lines[y].forcedBlank=true;
  SceneLighting_Begin(&s,true,0xa70b,8,0,0,512,512,0,0);
  SceneLighting_Prepare(&contact);assert(s.glowAlpha==0);
  puts("Lighting: global rooms, state guards, shared field, camera, HUD/weather, source bounds and shadows passed.");
}
