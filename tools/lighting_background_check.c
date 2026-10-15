/* Regression checks against locally captured native art. No ROM graphics are
 * stored in this tool. Run with GREEN_CAPTURE TOURIAN_CAPTURE. */
#define main RasterMain
#include "lighting_raster_preview.c"
#undef main

static void LoadBackground(const char *dir, PicaFrame *f) {
  Read(dir,"landing-wram.bin",g_ram,sizeof(g_ram));
  Read(dir,"landing-ppu.bin",&memory,sizeof(memory));
  Read(dir,"landing-lines.bin",lines,sizeof(lines));
  *f=(PicaFrame){.memory=&memory,.lines=lines,.lighting=&lighting,
      .width=256,.height=224,.hudEndY=31,.worldRight=256,
      .wideRoom={{.blocks=level_data,.cameraX=layer1_x_pos,.cameraY=layer1_y_pos}},
      .wideRoomWidth=room_width_in_blocks,.wideRoomHeight=room_height_in_blocks};
}
static void BeginBackground(PicaFrame *f) {
  SceneLighting_Begin(&lighting,true,room_ptr,8,layer1_x_pos,layer1_y_pos,
                      samus_x_pos,samus_y_pos,f->originX,0);
  SceneLighting_AddBackgroundSources(f);
}
static void EmptyMap(PicaFrame *f) {
  f->wideRoom[0].blocks=NULL;
  for(unsigned y=31;y<224;y++) {
    lines[y].forcedBlank=false;lines[y].screenEnabled[0]=2;
    lines[y].screenEnabled[1]=0;lines[y].addSubscreen=false;
    lines[y].bg[1].hScroll=lines[y].bg[1].vScroll=0;
  }
  unsigned map=lines[31].bg[1].tilemapAdr;
  memset(&memory.vram[map],0,2048*sizeof(uint16_t));
}
static void Put(unsigned x,unsigned y,unsigned tile) {
  unsigned map=lines[31].bg[1].tilemapAdr;
  memory.vram[map+y*32+x]=tile;
}
int main(int argc,char **argv) {
  assert(argc==3);
  PicaFrame f;
  LoadBackground(argv[1],&f);BeginBackground(&f);
  assert(lighting.backgroundCount==4 && lighting.count==5);
  SceneEmitterCache cached=lighting.emitters[1];
  /* A single-tile background scroll moves lamps, independently of BG1. */
  for(unsigned y=31;y<224;y++) lines[y].bg[1].hScroll+=8;
  BeginBackground(&f);
  assert(lighting.backgroundCount==4);
  assert(!memcmp(&cached,&lighting.emitters[1],sizeof(cached)));
  for(unsigned i=1;i<3;i++) assert(lighting.lights[i].b>lighting.lights[i].g);
  EmptyMap(&f);
  const unsigned ids[4]={0xec,0xed,0xfc,0xfd};
  for(unsigned flips=0;flips<4;flips++) {
    for(unsigned y=0;y<2;y++) for(unsigned x=0;x<2;x++)
      Put(10+x,10+y,0x1c00|((flips&1)?0x4000:0)|((flips&2)?0x8000:0)|
          ids[(y^((flips>>1)&1))*2+(x^(flips&1))]);
    BeginBackground(&f);
    assert(lighting.backgroundCount==1 && lighting.lights[1].x==88 && lighting.lights[1].y==88);
    for(unsigned y=31;y<224;y++) lines[y].bg[1].hScroll=8;
    BeginBackground(&f);
    assert(lighting.backgroundCount==1 && lighting.lights[1].x==80 && lighting.lights[1].y==88);
    for(unsigned y=31;y<224;y++) lines[y].bg[1].hScroll=0;
  }
  Put(11,11,0);BeginBackground(&f);assert(!lighting.backgroundCount);
  Put(11,11,0xdc00|ids[0]);BeginBackground(&f);assert(lighting.backgroundCount==1);
  f.width=400;f.originX=72;f.worldRight=400;
  BeginBackground(&f);assert(lighting.backgroundCount==1 && lighting.lights[1].x==160);
  f.width=f.worldRight=256;f.originX=0;
  static uint16_t occlusion[16*14];
  f.wideRoom[0].blocks=occlusion;f.wideRoom[0].cameraX=f.wideRoom[0].cameraY=0;
  f.wideRoomWidth=16;f.wideRoomHeight=14;occlusion[5*16+5]=0x8000;
  BeginBackground(&f);assert(!lighting.backgroundCount);
  f.wideRoom[0].blocks=NULL;
  unsigned old=memory.cgram[113];memory.cgram[113]=0;
  BeginBackground(&f);assert(!lighting.backgroundCount);memory.cgram[113]=old;
  /* Different art at the same shared indices must not emit. */
  memory.vram[0xec*16]^=1;BeginBackground(&f);assert(!lighting.backgroundCount);

  LoadBackground(argv[2],&f);BeginBackground(&f);assert(lighting.backgroundCount==4);
  EmptyMap(&f);Put(10,10,0x1423);Put(11,10,0x1423);
  BeginBackground(&f);
  assert(lighting.backgroundCount==1 && lighting.lights[1].x==88 && lighting.lights[1].y==84);
  assert(lighting.lights[1].r>0 && lighting.lights[1].g==0 && lighting.lights[1].b==0);
  Put(16,10,0x1433);Put(16,11,0x1433);BeginBackground(&f);
  assert(lighting.backgroundCount==2);
  memory.cgram[88]=0;BeginBackground(&f);assert(lighting.backgroundCount==1);
  assert(lighting.lights[1].b>0 && lighting.lights[1].r==0);
  lines[100].bg[1].hScroll=8;BeginBackground(&f);assert(!lighting.backgroundCount);
  lines[100].bg[1].hScroll=0;
  f.boundBg2=true;f.bg2Left=0;f.bg2Right=80;BeginBackground(&f);assert(!lighting.backgroundCount);
  f.boundBg2=false;
  SceneLighting_Begin(&lighting,true,room_ptr,8,0,0,128,168,0,0);
  while(lighting.count<SCENE_MAX_LIGHTS) SceneLighting_AddLight(&lighting,128,100,64,100,100,100);
  SceneLighting_AddBackgroundSources(&f);assert(!lighting.backgroundCount && lighting.count==SCENE_MAX_LIGHTS);

  /* Dense/custom map falls back to the visible scan rather than discarding
   * lamps after the first 64. Large 16-bit scroll values must wrap safely. */
  f.boundBg2=false;f.width=400;f.originX=72;f.worldRight=400;
  memory.cgram[88]=25;
  unsigned map=lines[31].bg[1].tilemapAdr;
  for(unsigned y=0;y<32;y++)for(unsigned x=0;x<64;x++)
    memory.vram[map+(x>=32?1024:0)+y*32+(x&31)]=((x+y)&1)?0x1423:0x1433;
  for(unsigned y=31;y<224;y++) {
    lines[y].bg[1].hScroll=65520;lines[y].bg[1].vScroll=65528;
  }
  BeginBackground(&f);
  assert(lighting.emitters[1].overflow && lighting.backgroundCount==4);
  unsigned halos=0;
  for(unsigned i=1;i<lighting.count;i++) {
    SceneLight *l=&lighting.lights[i];
    assert(l->x>=0 && l->x<400 && l->y>=31 && l->y<224);halos+=l->glow;
  }
  assert(halos==2);

  /* A full 16x16 red pilot light on BG1 is emissive even on solid machinery.
   * The elevator needs both mirrored halves, producing only one warm pool. */
  LoadBackground(argv[2],&f);
  map=lines[31].bg[0].tilemapAdr;
  memset(&memory.vram[map],0,2048*sizeof(uint16_t));
  for(unsigned y=31;y<224;y++) {
    lines[y].screenEnabled[0]=1;lines[y].addSubscreen=false;
    lines[y].bg[0].hScroll=lines[y].bg[0].vScroll=0;
  }
  const unsigned red[4]={0x26,0x27,0x36,0x37};
  for(unsigned flips=0;flips<4;flips++) {
    for(unsigned y=0;y<2;y++) for(unsigned x=0;x<2;x++)
      memory.vram[map+(10+y)*32+10+x]=0x3c00|((flips&1)?0x4000:0)|((flips&2)?0x8000:0)|
          red[(y^((flips>>1)&1))*2+(x^(flips&1))];
    BeginBackground(&f);
    assert(lighting.backgroundCount==1 && lighting.lights[1].x==88 && lighting.lights[1].y==88);
    assert(lighting.lights[1].r>0 && lighting.lights[1].g==0);
  }
  memset(&memory.vram[map],0,2048*sizeof(uint16_t));
  const unsigned elevator[8]={0x22ea,0x22eb,0x62eb,0x62ea,0x22f8,0x22f9,0x62f9,0x62f8};
  for(unsigned y=0;y<2;y++)for(unsigned x=0;x<4;x++)memory.vram[map+(10+y)*32+10+x]=elevator[y*4+x];
  BeginBackground(&f);
  assert(lighting.backgroundCount==1 && lighting.lights[1].x==96 && lighting.lights[1].y==88);
  assert(lighting.lights[1].r>lighting.lights[1].g && lighting.lights[1].g>lighting.lights[1].b);
  memory.vram[map+11*32+13]=0;BeginBackground(&f);assert(!lighting.backgroundCount);
  puts("Scenery: cached BG1/BG2 motifs, paired platform, dense-map fallback, halo budget, complete/flipped flowers, paired LEDs, palette-off, art mismatch, warped map, bounds and cap passed.");
}
