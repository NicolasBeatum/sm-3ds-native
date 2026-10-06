#include "scene_lighting.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static unsigned emitted;
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
  assert(!s.enabled);
  SceneLighting_Begin(&s,true,0x91f8,8,1024,1024,1152,1140,0,60);
  assert(s.count == 5 && s.samusX == 128 && s.samusY == 116);
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
  s.castShadow=true; s.samusX=0;
  q=original; q.x0=0; q.x1=8;
  SceneLighting_Tile(&f,1,1,&q); /* Entire projected sprite falls outside. */
  assert(!s.shadowCount);
  q=original; SceneLighting_Tile(&f,1,1,&q);
  assert(s.shadowCount==1 && q.a==original.a);
  assert(SceneLighting_EmitShadows(&f,0) && emitted==1 && !s.shadowCount);
  puts("Lighting: scene guards, camera tracking, HUD/weather preservation, light bounds and shadow clipping passed.");
}
