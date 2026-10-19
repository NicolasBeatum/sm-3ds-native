#include "scene_lighting.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static Ppu memory;
static PicaLine lines[224];
static SceneLighting scene;
static PicaFrame frame={.memory=&memory,.lines=lines,.lighting=&scene,
  .width=256,.height=224,.hudEndY=32,.worldRight=256};

static void Ambient(void) {
  SceneLighting_Begin(&scene,true,0x9f64,8,0,0,176,400,frame.originX,0);
  SceneLighting_Ambient(&frame);
}

int main(void) {
  for (unsigned y=0;y<224;y++) {
    lines[y].screenEnabled[0]=3;
    lines[y].bg[0].tilemapAdr=0x1000;
    lines[y].bg[1].tilemapAdr=0x1800;
  }
  /* Red foreground, green background with alternating blue tile rows.
   * A one-pixel pan must not recolor every wall in the room. */
  for (unsigned i=0;i<1024;i++) {
    memory.vram[0x1000+i]=1|(1<<10);
    memory.vram[0x1800+i]=2|(2<<10);
  }
  for (unsigned y=0;y<8;y++) {
    memory.vram[16+y]=255;
    memory.vram[32+y]=y&1?0xff00:255;
  }
  memory.cgram[17]=31;
  memory.cgram[33]=31<<5;memory.cgram[34]=31<<10;
  Ambient();
  assert(scene.ambient[1][1]>scene.ambient[1][0]);
  uint8_t initial[2][3];memcpy(initial,scene.ambient,sizeof(initial));
  for (unsigned wide=0;wide<2;wide++) for (int shift=-32;shift<=32;shift++) {
    frame.width=wide?400:256;frame.originX=wide?72:0;
    frame.worldLeft=wide?40:0;frame.worldRight=wide?360:256;
    frame.boundBg2=wide;frame.bg2Left=72;frame.bg2Right=328;
    for (unsigned y=0;y<224;y++) for (unsigned layer=0;layer<2;layer++) {
      lines[y].bg[layer].hScroll=shift;
      lines[y].bg[layer].vScroll=shift*3;
    }
    Ambient();assert(!memcmp(initial,scene.ambient,sizeof(initial)));
  }
  /* Sprite palettes and unused bright entries cannot tint the room. */
  for (unsigned i=128;i<256;i++) memory.cgram[i]=31;
  memory.cgram[35]=31;
  Ambient();assert(!memcmp(initial,scene.ambient,sizeof(initial)));
  /* Live palette changes still update the cached material's contribution. */
  memory.cgram[33]=memory.cgram[34]=31;
  Ambient();assert(scene.ambient[1][0]>scene.ambient[1][1]);
  /* Animated graphics and streamed/replaced map cells invalidate coverage. */
  for (unsigned y=0;y<8;y++) memory.vram[32+y]=255;
  memory.cgram[33]=31<<5;
  Ambient();assert(scene.ambient[1][1]>scene.ambient[1][0]);
  for (unsigned i=0;i<1024;i++) memory.vram[0x1800+i]=1|(1<<10);
  Ambient();assert(scene.ambient[1][0]>scene.ambient[1][1]);
  /* Transparent/disabled BG2 falls back to actual foreground art. */
  memset(memory.vram+0x1800,0,1024*sizeof(uint16_t));
  Ambient();assert(scene.ambient[1][0]>scene.ambient[1][1]);
  for (unsigned y=0;y<224;y++) lines[y].screenEnabled[0]=2;
  Ambient();assert(scene.ambient[1][0]==88 && scene.ambient[1][2]==121);
  puts("Ambient: camera/viewport invariance, material cache, live palettes/graphics and layer fallback passed.");
}
