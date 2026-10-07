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
  assert(group == 21 && ((q->depth == 0x7200 && q->a == 78) ||
                        (q->depth == 0x8100 && q->a == 96)));
  assert(q->x0 >= 0 && q->x1 <= 256 && q->y0 >= 32 && q->y1 <= 224);
  emitted++;
  return true;
}
int main(void) {
  SceneLighting s={0};
  uint32_t pixels[PICA_ATLAS_W * PICA_ATLAS_H] = {0};
  PicaFrame f = {.width=256,.height=224,.hudEndY=32,
                .worldRight=256,.pixels=pixels,.lighting=&s,.emit=Capture};
  PicaQuad original = {100,100,108,108,0,4096,32,4032,0x8000,255,255,255,127};
  PicaQuad q = original;
  SceneLighting_Begin(&s,false,0x91f8,8,1024,1024,1152,1140,0,0);
  SceneLighting_Tile(&f,0,1,&q);
  assert(!memcmp(&q,&original,sizeof(q)));
  for(unsigned state=0;state<44;state++) {
    SceneLighting_Begin(&s,true,0x91f8,state,1024,1024,1152,1140,0,0);
    assert(s.enabled == (state == 8));
  }
  SceneLighting_Begin(&s,true,0x92fd,8,1024,1024,1152,1140,0,0);
  assert(s.enabled && !s.ship && s.count==1);
  assert(s.lights[0].x==128 && s.lights[0].y==102);
  SceneLighting_Prepare(&f);
  assert(s.color[0][6][8][1]>s.ambient[0][1]); /* Samus fill in another room. */
  SceneLighting_AddLight(&s,220,44,48,88,62,28);
  SceneLighting_Prepare(&f);
  q=original; q.x0=216; q.x1=224; q.y0=40; q.y1=48;
  SceneLighting_Tile(&f,0,1,&q);
  assert(q.r>220 && q.g>210 && q.b>190 && q.r>q.g && q.g>q.b &&
         q.a==original.a);
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
  q=original; q.x0=0; q.x1=8;
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
  assert(s.shadowX==10 && s.shadowY==9); /* Own charge does not steer it. */
  SceneLighting_AddLight(&s,24,128,160,54,100,174);
  SceneLighting_Prepare(&f);
  assert(s.shadowX==12 && s.shadowY==2);
  s.castShadow=true;
  q=original; SceneLighting_Tile(&f,1,1,&q);
  assert(s.shadowCount==1 && s.shadows[0].depth>0x8000 &&
         s.shadows[0].depth<0xc000 && s.shadows[0].a==96);
  assert(s.shadows[0].x0==112 && s.shadows[0].y0==102);
  assert(SceneLighting_EmitShadows(&f,0) && emitted==2 && !s.shadowCount);
  SceneLighting_Begin(&s,true,0xa70b,8,0,0,128,152,0,0);
  SceneLighting_AddLight(&s,128,200,160,154,72,24);
  SceneLighting_Prepare(&f);
  assert(s.shadowX==0 && s.shadowY==-12); /* Lava illuminates from below. */
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
  puts("Lighting: global rooms, state guards, shared field, camera, HUD/weather, source bounds and shadows passed.");
}
