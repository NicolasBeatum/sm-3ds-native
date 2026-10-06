/* Warm-cache median CPU model/vertex benchmark using native room captures.
 * Link renamed base and initial-trial model objects; see build_lighting_benchmark.py.
 * These host times do not measure console FPS or GPU cost. */
#define _POSIX_C_SOURCE 200809L
#define main RasterPreviewMain
#include "lighting_raster_preview.c"
#undef main
#include "ppu_gpu_vertices.h"
#include <time.h>
bool BeforeBuildFrame(PicaFrame *);
void BeforeAtlasInit(PicaAtlas *, uint32_t *);
void BeforeAtlasBegin(PicaAtlas *);
bool InitialBuildFrame(PicaFrame *);
void InitialAtlasInit(PicaAtlas *, uint32_t *);
void InitialAtlasBegin(PicaAtlas *);
void InitialSceneBegin(SceneLighting *,bool,unsigned,unsigned,int,int,int,int,unsigned,unsigned);
void InitialSceneAdd(SceneLighting *,int,int,int,unsigned,unsigned,unsigned);
static const char *names[]={"base","optimized-off","optimized-on","optimized-eight", "initial-off","initial-on","initial-eight"};
static PicaVertex vertices[PICA_MAX_VERTICES];
static unsigned vertexCount;
static bool Pack(void *ctx, unsigned group, const PicaQuad *q) {
  (void)ctx;
  count[group]++;
  if(vertexCount+4>PICA_MAX_VERTICES) return false;
  PicaQuadVertices(vertices+vertexCount,q);
  vertexCount+=4;
  return true;
}
static double Now(void) {
  struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);
  return t.tv_sec+t.tv_nsec*1e-9;
}
static int Cmp(const void *a,const void *b) {
  double x=*(const double*)a,y=*(const double*)b;return (x>y)-(x<y);
}
static void Bench(const char *root,unsigned width,unsigned mode) {
  unsigned origin=width==400?72:0;
  Read(root,"landing-wram.bin",g_ram,sizeof(g_ram));
  Read(root,"landing-ppu.bin",&memory,sizeof(memory));
  Read(root,"landing-lines.bin",lines,sizeof(lines));
  if(mode==0) BeforeAtlasInit(&atlas,atlasPixels);
  else if(mode>=4) InitialAtlasInit(&atlas,atlasPixels);
  else PicaAtlasInit(&atlas,atlasPixels);
  PicaFrame f={.memory=&memory,.lines=lines,.atlas=&atlas,.pixels=atlasPixels,
    .width=width,.height=224,.originX=origin,.hudEndY=32,.worldRight=width,
    .wideRoom={{.blocks=level_data,.cameraX=layer1_x_pos,.cameraY=layer1_y_pos,
      .scrollX=layer1_x_pos+bg1_x_offset,.scrollY=layer1_y_pos+bg1_y_offset,.followScroll=true}},
    .wideTileTable=(const uint16_t *)(g_ram+0xA000),
    .wideRoomWidth=room_width_in_blocks,.wideRoomHeight=room_height_in_blocks,
    .lighting=mode==0?NULL:&lighting,.emit=Pack};
  double trials[9];
  for(unsigned rep=0;rep<10;rep++) {
    double started=Now();
    for(unsigned iteration=0;iteration<600;iteration++) {
      memset(count,0,sizeof(count));vertexCount=0;
      if(mode==0)BeforeAtlasBegin(&atlas);
      else if(mode>=4)InitialAtlasBegin(&atlas);
      else PicaAtlasBegin(&atlas);
      void (*begin)(SceneLighting *,bool,unsigned,unsigned,int,int,int,int,unsigned,unsigned) =
          mode>=4 ? InitialSceneBegin : SceneLighting_Begin;
      void (*add)(SceneLighting *,int,int,int,unsigned,unsigned,unsigned) =
          mode>=4 ? InitialSceneAdd : SceneLighting_AddLight;
      begin(&lighting,mode>=4 ? mode>=5 : mode>=2,room_ptr,game_state,layer1_x_pos,
        layer1_y_pos,samus_x_pos,samus_y_pos,origin,flare_counter);
      for(unsigned i=0;i<10;i++)if(projectile_type[i])
        add(&lighting,(int16_t)(projectile_x_pos[i]-layer1_x_pos)+origin,
          (int16_t)(projectile_y_pos[i]-layer1_y_pos),48,88,62,28);
      if((mode==3 || mode==6) && lighting.enabled)while(lighting.count<SCENE_MAX_LIGHTS)
        add(&lighting,origin+128,100,108,100,90,60);
      assert(mode==0 ? BeforeBuildFrame(&f) : mode>=4 ? InitialBuildFrame(&f) : PicaBuildFrame(&f));
    }
    if(rep)trials[rep-1]=(Now()-started)*1000/600;
  }
  qsort(trials,9,sizeof(double),Cmp);
  printf("%s width=%u mode=%s lights=%u quads=%u shadows=%u cpu_ms=%.6f range=%.6f..%.6f\n",
      root,width,names[mode],lighting.count,vertexCount/4,count[21]+count[22],
      trials[4],trials[0],trials[8]);
}
int main(int argc,char **argv) {
  if(argc!=2) { fprintf(stderr,"usage: %s captured-directory\n",argv[0]);return 1; }
  char fire[1024];snprintf(fire,sizeof(fire),"%s/fire",argv[1]);
  for(unsigned width=256;width<=400;width+=144) {
    for(unsigned mode=0;mode<7;mode++)Bench(argv[1],width,mode);
    for(unsigned mode=1;mode<6;mode++)if(mode!=3)Bench(fire,width,mode);
  }
  printf("SceneLighting=%zu bytes glow=16384 bytes vertex=%zu bytes quad=%zu bytes\n",
    sizeof(SceneLighting),sizeof(PicaVertex),sizeof(PicaQuad));
  return 0;
}
