#include "scene_lighting.h"
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
  assert(group == 21 && q->depth == 0x7200 && q->a == 78);
  assert(q->x0 >= 0 && q->x1 <= 256 && q->y0 >= 32 && q->y1 <= 224);
  emitted++;
  return true;
}
int main(void) {
  SceneLighting s;
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
  assert(s.field[12][16][1]>0); /* Samus fill in another room. */
  SceneLighting_AddLight(&s,220,44,48,88,62,28);
  SceneLighting_Prepare(&f);
  q=original; q.x0=216; q.x1=224; q.y0=40; q.y1=48;
  SceneLighting_Tile(&f,0,1,&q);
  assert(q.r==255 && q.g==255 && q.b<255 && q.a==original.a);
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
  for(unsigned y=0;y<SCENE_FIELD_H;y++)
    for(unsigned x=0;x<SCENE_FIELD_W;x++)
      for(unsigned c=0;c<3;c++) assert(!s.field[y][x][c]);
  SceneLighting_Begin(&s,true,0x91f8,8,1024,1024,1152,1140,0,0);
  SceneLighting_Prepare(&f);
  s.castShadow=true; s.samusX=0;
  q=original; q.x0=0; q.x1=8;
  SceneLighting_Tile(&f,1,1,&q); /* Entire projected sprite falls outside. */
  assert(!s.shadowCount);
  q=original; SceneLighting_Tile(&f,1,1,&q);
  assert(s.shadowCount==1 && q.a==original.a);
  assert(SceneLighting_EmitShadows(&f,0) && emitted==1 && !s.shadowCount);
  /* Cached relief must preserve flips and invalidate after palette/tile
   * changes on the next frame, even when the same atlas slot is reused. */
  SceneLighting_Begin(&s,true,0xa70b,8,0,0,128,152,0,0);
  s.count=0;
  pixels[64+Morton(3,4)]=pixels[64+Morton(4,3)]=0xffffffff;
  SceneLighting_Prepare(&f);
  q=original; SceneLighting_Tile(&f,0,1,&q);
  assert(q.r==204 && q.g==204 && q.b==204);
  q=original; q.u0=32; q.u1=0; q.v0=4032; q.v1=4096;
  SceneLighting_Tile(&f,0,1,&q);
  assert(q.r==232 && q.g==232 && q.b==232);
  pixels[64+Morton(3,4)]=pixels[64+Morton(4,3)]=0;
  SceneLighting_Prepare(&f);
  q=original; SceneLighting_Tile(&f,0,1,&q);
  assert(q.r==218);
  /* A rejected frame must restore the caller's callback/context. */
  f.width=255; f.context=&s;
  assert(!PicaBuildFrame(&f) && f.emit==Capture && f.context==&s && f.lighting==&s);
  f.xrayActive=true;
  assert(!PicaBuildFrame(&f) && !s.enabled && f.emit==Capture && f.context==&s);
  puts("Lighting: global rooms, state guards, shared field, camera, HUD/weather, source bounds and shadows passed.");
}
