/* Native-capture CPU model + vertex packing only; not hardware FPS/GPU time.
 * Compile each revision against its own headers/scene layout. */
#define _POSIX_C_SOURCE 200809L
#define main RasterPreviewMain
#include "lighting_raster_preview.c"
#undef main
#include "ppu_gpu_vertices.h"
#include <time.h>
static PicaAtlas buffers[2];
static uint32_t bufferPixels[2][PICA_ATLAS_W*PICA_ATLAS_H];
static PicaVertex vertices[PICA_MAX_VERTICES];
static unsigned vertexCount;
static bool Pack(void *ctx,unsigned group,const PicaQuad *q) {
  (void)ctx; count[group]++;
  if(vertexCount+4>PICA_MAX_VERTICES)return false;
  PicaQuadVertices(vertices+vertexCount,q); vertexCount+=4; return true;
}
static double Now(void) {
  struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);
  return t.tv_sec+t.tv_nsec*1e-9;
}
static int Compare(const void *a,const void *b) {
  double x=*(const double *)a,y=*(const double *)b;return (x>y)-(x<y);
}
int main(int argc,char **argv) {
  if(argc!=5){fprintf(stderr,"usage: %s capture-directory lights:0|1 camera:0|1|2 width:256|400\n",argv[0]);return 1;}
  unsigned cameraMode=atoi(argv[3]);
  bool lit=atoi(argv[2])!=0,moving=cameraMode!=0;
  unsigned width=atoi(argv[4]),origin=width==400?72:0;
  assert(width==256||width==400);
  Read(argv[1],"landing-wram.bin",g_ram,sizeof(g_ram));
  Read(argv[1],"landing-ppu.bin",&memory,sizeof(memory));
  Read(argv[1],"landing-lines.bin",lines,sizeof(lines));
  PicaLine originalLines[224];memcpy(originalLines,lines,sizeof(lines));
  unsigned streamAdr=lines[kHudEndLine].bg[0].tilemapAdr&0x7fff;
  unsigned streamWord=memory.vram[streamAdr];
  for(unsigned i=0;i<2;i++)PicaAtlasInit(&buffers[i],bufferPixels[i]);
  PicaFrame f={.memory=&memory,.lines=lines,.width=width,.height=224,
    .originX=origin,.hudEndY=kHudEndLine,.worldRight=width,
    .wideRoom={{.blocks=level_data,.cameraY=layer1_y_pos,
      .scrollY=layer1_y_pos+bg1_y_offset,.followScroll=true}},
    .wideTileTable=(const uint16_t *)(g_ram+0xA000),
    .wideRoomWidth=room_width_in_blocks,.wideRoomHeight=room_height_in_blocks,
    .emit=Pack};
  double trials[9],cold=0;
  for(unsigned rep=0;rep<10;rep++) {
    double start=Now();
    for(unsigned it=0;it<400;it++) {
      memset(count,0,sizeof(count));vertexCount=0;
      unsigned dx=moving?(it%64)*2:0;
      /* Mode 2 also invalidates the scenery map every four frames, like a
       * streamed column update. Synthetic stress, not a gameplay recording. */
      if(cameraMode==2) memory.vram[streamAdr]=streamWord^((it&4)?0x2000:0);
      for(unsigned y=kHudEndLine;y<224;y++) for(unsigned layer=0;layer<2;layer++)
        lines[y].bg[layer].hScroll=originalLines[y].bg[layer].hScroll+dx;
      f.wideRoom[0].cameraX=layer1_x_pos+dx;
      f.wideRoom[0].scrollX=layer1_x_pos+bg1_x_offset+dx;
      f.atlas=&buffers[it&1];f.pixels=bufferPixels[it&1];PicaAtlasBegin(f.atlas);
      SceneLighting_Begin(&lighting,lit,room_ptr,game_state,layer1_x_pos+dx,
          layer1_y_pos,samus_x_pos,samus_y_pos,origin,flare_counter);
      if(lit&&(int16_t)lava_acid_y_pos>=0){lighting.liquidType=fx_type;lighting.liquidY=(int16_t)(lava_acid_y_pos-layer1_y_pos);}
      for(unsigned i=0;i<10;i++) if(projectile_type[i]&&lighting.count<(lighting.ship?6u:4u))
        SceneLighting_AddLight(&lighting,(int16_t)(projectile_x_pos[i]-layer1_x_pos-dx)+origin,
          (int16_t)(projectile_y_pos[i]-layer1_y_pos),48,88,62,28);
      f.lighting=lit?&lighting:NULL;
      double first=(!rep && !it)?Now():0;
      assert(PicaBuildFrame(&f));
      if(first) cold=(Now()-first)*1000;
    }
    if(rep)trials[rep-1]=(Now()-start)*1000/400;
  }
  qsort(trials,9,sizeof(double),Compare);
  printf("{\"scene\":\"%s\",\"width\":%u,\"lit\":%u,\"moving\":%u,\"cpu_ms\":%.6f,\"min_ms\":%.6f,\"max_ms\":%.6f,\"lights\":%u,\"quads\":%u,\"shadows\":%u,\"scene_bytes\":%zu,\"camera_mode\":%u,\"cold_model_ms\":%.6f}\n",
      argv[1],width,lit,moving,trials[4],trials[0],trials[8],lit?lighting.count:0,
      vertexCount/4,count[21]+count[22],sizeof(SceneLighting),cameraMode,cold);
}
