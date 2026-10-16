/* Software raster of the same PICA quads for visual QA on the host.
 * This does not establish hardware GPU output or performance. */
#include "scene_lighting.h"
#include "wide_config.h"
#include "src/ida_types.h"
#include "src/variables.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

uint8 g_ram[0x20000];
static uint32_t atlasPixels[PICA_ATLAS_W * PICA_ATLAS_H];
static PicaAtlas atlas;
static Ppu memory;
static PicaLine lines[224];
static SceneLighting lighting;
static uint32_t color[2][400*224];
static uint16_t depth[2][400*224];
static uint8_t stencil[2][400*224];
static PicaQuad *quads[PICA_GROUPS];
static unsigned count[PICA_GROUPS], capacity[PICA_GROUPS];
static unsigned shadowPixels[2];
static bool Capture(void *ctx, unsigned group, const PicaQuad *q) {
  (void)ctx;
  if (count[group] == capacity[group]) {
    capacity[group] = capacity[group] ? capacity[group]*2 : 256;
    quads[group] = realloc(quads[group], capacity[group]*sizeof(*q));
    assert(quads[group]);
  }
  quads[group][count[group]++] = *q;
  return true;
}
static bool CaptureColored(void *ctx,unsigned group,const PicaQuad *q,const uint8_t rgb[3]) {
  PicaQuad colored=*q;colored.r=rgb[0];colored.g=rgb[1];colored.b=rgb[2];
  return Capture(ctx,group,&colored);
}
static int Clamp(int n) { return n<0?0:n>255?255:n; }
static uint32_t Sample(const PicaQuad *q, int x, int y) {
  if (q->depth == 1) return (q->r<<24)|(q->g<<16)|(q->b<<8)|q->a;
  double u = q->u0 + (x-q->x0+.5)*(q->u1-q->u0)/(q->x1-q->x0);
  double v = q->v0 + (y-q->y0+.5)*(q->v1-q->v0)/(q->y1-q->y0);
  unsigned tx=(unsigned)(u/4), ty=(unsigned)((4096-v)/8);
  unsigned ux=tx&7, uy=ty&7;
  unsigned morton=(ux&1)|((uy&1)<<1)|((ux&2)<<1)|((uy&2)<<2)|((ux&4)<<2)|((uy&4)<<3);
  unsigned slot=(ty/8)*128+tx/8;
  assert(slot<PICA_SLOTS);
  uint32_t p=atlasPixels[slot*64+morton];
  if (!(p&255)) return 0;
  return (((p>>24)*q->r/255)<<24)|((((p>>16)&255)*q->g/255)<<16)|
         ((((p>>8)&255)*q->b/255)<<8)|q->a;
}
static void Draw(unsigned group, unsigned width) {
  unsigned screen=group<4?group/2:group-21;
  bool shadow=group>=21;
  for(unsigned i=0;i<count[group];i++) {
    const PicaQuad *q=&quads[group][i];
    for(int y=q->y0;y<q->y1;y++) for(int x=q->x0;x<q->x1;x++) {
      assert(x>=0 && x<(int)width && y>=0 && y<224);
      unsigned pos=y*width+x;
      uint32_t p=Sample(q,x,y);
      if(!(p&255)) continue;
      if(!shadow && (group&1) && stencil[screen][pos]) continue;
      if(q->depth>depth[screen][pos]) {
        if(shadow) {
          shadowPixels[screen]++;
          uint32_t c=color[screen][pos], result=c&255;
          for(unsigned shift=8;shift<=24;shift+=8)
            result|=((((c>>shift)&255)*(255-q->a)/255)<<shift);
          color[screen][pos]=result;
        } else {
          color[screen][pos]=p;
          depth[screen][pos]=q->depth;
        }
      }
      if(!shadow && (group&1)) stencil[screen][pos]=1;
    }
  }
}
static void Read(const char *root, const char *name, void *data, size_t size) {
  char path[1024]; snprintf(path,sizeof(path),"%s/%s",root,name);
  FILE *f=fopen(path,"rb"); assert(f && fread(data,1,size,f)==size); fclose(f);
}
int main(int argc,char **argv) {
  assert(argc==4);
  unsigned width=atoi(argv[2])?400:256, origin=width==400?72:0;
  Read(argv[1],"landing-wram.bin",g_ram,sizeof(g_ram));
  Read(argv[1],"landing-ppu.bin",&memory,sizeof(memory));
  Read(argv[1],"landing-lines.bin",lines,sizeof(lines));
  PicaAtlasInit(&atlas,atlasPixels); PicaAtlasBegin(&atlas);
  PicaFrame frame={.memory=&memory,.lines=lines,.atlas=&atlas,.pixels=atlasPixels,
    .width=width,.height=224,.originX=origin,.hudEndY=kHudEndLine,.worldRight=width,
    .wideRoom={{.blocks=level_data,.cameraX=layer1_x_pos,.cameraY=layer1_y_pos,
      .scrollX=layer1_x_pos+bg1_x_offset,.scrollY=layer1_y_pos+bg1_y_offset,.followScroll=true}},
    .wideTileTable=(const uint16_t *)(g_ram+0xA000),
    .wideRoomWidth=room_width_in_blocks,.wideRoomHeight=room_height_in_blocks,
    .lighting=&lighting,.emit=Capture,.emitColored=CaptureColored};
  SceneLighting_Begin(&lighting,atoi(argv[3])!=0,room_ptr,game_state,
                      layer1_x_pos,layer1_y_pos,samus_x_pos,samus_y_pos,origin,flare_counter);
  if (lighting.enabled && (int16_t)lava_acid_y_pos>=0) {
    lighting.liquidType=fx_type;
    lighting.liquidY=(int16_t)(lava_acid_y_pos-layer1_y_pos);
  }
  for (unsigned i=0;i<10;i++) if(projectile_type[i] && lighting.count<(lighting.ship?6u:4u))
    SceneLighting_AddLight(&lighting,
      (int16_t)(projectile_x_pos[i]-layer1_x_pos)+(int)origin,
      (int16_t)(projectile_y_pos[i]-layer1_y_pos),48,88,62,28);
  frame.lighting = lighting.enabled ? &lighting : NULL;
  bool ok=PicaBuildFrame(&frame);
  if(!ok) { fprintf(stderr,"PPU model rejected frame: %s\n",frame.failure);return 1; }
  Draw(0,width); Draw(21,width); Draw(1,width);
  Draw(2,width); Draw(22,width); Draw(3,width);
  uint8_t output[400*224*3]={0};
  for(unsigned group=4;group<20;group++) for(unsigned i=0;i<count[group];i++) {
    const PicaQuad *q=&quads[group][i]; unsigned flags=group-4;
    for(int y=q->y0;y<q->y1;y++)for(int x=q->x0;x<q->x1;x++) {
      unsigned pos=y*width+x; uint32_t main=color[0][pos],sub=color[1][pos];
      for(unsigned channel=0;channel<3;channel++) {
        unsigned shift=24-channel*8;
        int m=flags&4?0:(main>>shift)&255, s=(sub>>shift)&255;
        int c=m;
        if((main&255)>127 && !(flags&8)) {
          c=(flags&1)?m-s:m+s;
          if((flags&2) && (sub&255)>127) c/=2;
        }
        output[pos*3+channel]=Clamp(c);
      }
    }
  }
  if(lighting.enabled) for(unsigned y=frame.hudEndY;y<224;y++)for(unsigned x=0;x<width;x++) {
    unsigned pos=y*width+x;
    for(unsigned i=0;i<lighting.count;i++) {
      SceneLight *l=&lighting.lights[i];
      if (!l->glow) continue;
      float radius=l->radius*.55f;
      float dx=(x-l->x)/radius,dy=(y-l->y)/radius;
      float a=1-dx*dx-dy*dy;
      if(a<=0)continue;
      float w=a*a*a*lighting.glowAlpha/255;
      output[pos*3]=Clamp(output[pos*3]+l->r*w);
      output[pos*3+1]=Clamp(output[pos*3+1]+l->g*w);
      output[pos*3+2]=Clamp(output[pos*3+2]+l->b*w);
    }
  }
  char path[1024];snprintf(path,sizeof(path),"%s/%s-%s.ppm",argv[1],
                           width==400?"wide":"standard",lighting.enabled?"on":"off");
  FILE *file=fopen(path,"wb");assert(file);
  fprintf(file,"P6\n%u 224\n255\n",width);
  fwrite(output,1,width*224*3,file);fclose(file);
  char metadata[1024];
  snprintf(metadata,sizeof(metadata),"%s/%s-%s-lights.json",argv[1],
      width==400?"wide":"standard",lighting.enabled?"on":"off");
  file=fopen(metadata,"w");assert(file);
  fprintf(file,"{\"count\":%u,\"scenery\":%u,\"sources\":[",lighting.count,lighting.backgroundCount);
  for(unsigned i=0;i<lighting.count;i++) {
    const SceneLight *l=&lighting.lights[i];
    fprintf(file,"%s{\"x\":%d,\"y\":%d,\"radius\":%d,\"rgb\":[%u,%u,%u],\"glow\":%s}",
        i?",":"",l->x,l->y,l->radius,l->r,l->g,l->b,l->glow?"true":"false");
  }
  fputs("]}\n",file);fclose(file);
  printf("%s: lights=%u background=%u shadows=%u/%u affected-shadow-pixels=%u/%u\n",
         path,lighting.count,lighting.backgroundCount,count[21],count[22],shadowPixels[0],shadowPixels[1]);
}
