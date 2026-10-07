#include "scene_lighting.h"
#include <string.h>
#include <stdlib.h>

static int Clamp(int x, int low, int high) {
  return x < low ? low : x > high ? high : x;
}

void SceneLighting_AddLight(SceneLighting *s, int x, int y, int radius,
                            unsigned r, unsigned g, unsigned b) {
  if (!s->enabled || s->count == SCENE_MAX_LIGHTS || radius <= 0) return;
  s->lights[s->count++] = (SceneLight){x,y,Clamp(radius,1,256),
      Clamp(r,0,255),Clamp(g,0,255),Clamp(b,0,255)};
}

void SceneLighting_Begin(SceneLighting *s, bool enabled, unsigned room,
                         unsigned state, int cameraX, int cameraY,
                         int samusX, int samusY, unsigned origin,
                         unsigned charge) {
  s->count = s->shadowCount = s->environmentStart = 0;
  s->castShadow = false;
  s->liquidType = 0;
  s->enabled = enabled && state == 8;
  s->ship = room == 0x91f8;
  s->room = room;
  if (!s->enabled) { s->fieldValid=false; return; }
  const uint8_t interior[2][3]={{150,158,170},{88,101,121}};
  memcpy(s->ambient,interior,sizeof(interior));
  s->samusX = samusX - cameraX + (int)origin;
  s->samusY = samusY - cameraY;
  /* Ship hatch and two side lamps, fixed to room coordinates. */
  if (s->ship) {
    SceneLighting_AddLight(s, 1152 - cameraX + (int)origin, 1120 - cameraY,
                            112, 106, 78, 34);
    SceneLighting_AddLight(s, 1112 - cameraX + (int)origin, 1156 - cameraY,
                            72, 74, 52, 20);
    SceneLighting_AddLight(s, 1192 - cameraX + (int)origin, 1156 - cameraY,
                            72, 74, 52, 20);
  }
  SceneLighting_AddLight(s, s->samusX, s->samusY - 14,
                          56, s->ship ? 26 : 42, s->ship ? 42 : 54,
                          s->ship ? 38 : 48);
  if (charge >= 16)
    SceneLighting_AddLight(s, s->samusX + 10, s->samusY - 8,
                            48 + Clamp(charge, 0, 60), 88, 100, 62);
  s->environmentStart=s->count;
}

/* Low block IDs are the shared door-cap graphics from the game's bank-84
 * draw definitions (A767/A827/A8E7/A9B3). Check the live block, not a PLM,
 * because closed blue doors need not have a running PLM object. */
void SceneLighting_AddRoomSources(PicaFrame *f) {
  SceneLighting *s = f->lighting;
  if (!s || !s->enabled) return;
  s->environmentStart=s->count;
  if (s->count==SCENE_MAX_LIGHTS) return;
  if ((s->liquidType == 2 || s->liquidType == 4) &&
      s->liquidY < (int)f->height + 64) {
    unsigned n = f->width == 400 ? 3 : 2;
    int width = f->worldRight - f->worldLeft;
    int y = s->liquidY < (int)f->hudEndY ? (int)f->hudEndY + 48 : s->liquidY + 16;
    unsigned r=154,g=72,b=24;
    /* Lava and acid can share orange graphics. Read the liquid's BG3 palette
     * rather than assuming acid is green; custom palettes follow their art. */
    if(f->memory && f->lines) {
      unsigned sampleY=Clamp(y,f->hudEndY,f->height-1);
      const BgLayer *bg=&f->lines[sampleY].bg[2];
      unsigned wx=(128+bg->hScroll)&(bg->tilemapWider?511:255);
      unsigned wy=(sampleY+bg->vScroll)&(bg->tilemapHigher?511:255);
      unsigned map=bg->tilemapAdr+((wy>>3)&31)*32+((wx>>3)&31)+
          (wx>=256?0x400:0)+(wy>=256?(bg->tilemapWider?0x800:0x400):0);
      unsigned palette=(f->memory->vram[map&0x7fff]&0x1c00)>>8;
      unsigned best=0;
      for(unsigned i=1;i<4;i++) {
        unsigned c=f->memory->cgram[palette+i];
        unsigned cr=c&31,cg=(c>>5)&31,cb=(c>>10)&31;
        unsigned high=cr>cg?cr:cg;high=high>cb?high:cb;
        unsigned low=cr<cg?cr:cg;low=low<cb?low:cb;
        unsigned luma=cr*3+cg*6+cb;
        if(high-low>=4 && luma>best) {
          best=luma; r=cr*154/31;g=cg*154/31;b=cb*154/31;
        }
      }
    }
    for (unsigned i=0; i<n && width>0; i++)
      SceneLighting_AddLight(s, f->worldLeft + width*(2*i+1)/(2*n), y,
          160,r,g,b);
  }
  const PicaWideRoomLayer *room = &f->wideRoom[0];
  if (!room->blocks || !f->wideRoomWidth || !f->wideRoomHeight) return;
  int x0 = Clamp((room->cameraX + f->worldLeft - (int)f->originX - 64)/16,
                 0, f->wideRoomWidth);
  int x1 = Clamp((room->cameraX + f->worldRight - (int)f->originX + 79)/16,
                 0, f->wideRoomWidth);
  int y0 = Clamp((room->cameraY + (int)f->hudEndY - 64)/16, 0, f->wideRoomHeight);
  int y1 = Clamp((room->cameraY + (int)f->height + 79)/16, 0, f->wideRoomHeight);
  /* A lone low block ID (notably ordinary solid 0x8000) is not a door.
   * Validate the complete four-block cap and its flips from bank-84 draw
   * definitions. Closed bodies use D/5; opening frames use type 8. */
  SceneLight doors[2]={{0}}; int distance[2]={0x7fffffff,0x7fffffff};
  unsigned count=0;
  static const uint8_t rgb[4][3]={{48,156,58},{156,48,82},{156,132,32},{54,100,174}};
  for (int y=y0;y<y1;y++) for (int x=x0;x<x1;x++) {
    unsigned block=room->blocks[y*f->wideRoomWidth+x];
    unsigned id=block&0x3ff,type=block>>12,gfx=block&0xfff;
    if (id>=0x40 || (type!=8 && type!=12)) continue;
    unsigned stride,expect[3],body;
    bool vertical=id<0x10;
    if (vertical) {
      if ((id&3)==3 || (gfx&0x800) || y+3>=(int)f->wideRoomHeight) continue;
      stride=f->wideRoomWidth;
      expect[0]=gfx+0x20; expect[1]=(gfx+0x20)^0x800; expect[2]=gfx^0x800;
      body=13;
    } else {
      if (!(id&0x10) || (id&3)!=1 || x+3>=(int)f->wideRoomWidth) continue;
      stride=1;
      expect[0]=gfx-1; expect[1]=(gfx-1)^0x400; expect[2]=gfx^0x400;
      body=5;
    }
    unsigned index=y*f->wideRoomWidth+x;
    bool cap=true;
    for (unsigned part=0;part<3;part++) {
      unsigned tile=room->blocks[index+(part+1)*stride];
      unsigned t=tile>>12;
      if ((tile&0xfff)!=expect[part] || (t!=body && t!=8)) {cap=false;break;}
    }
    if (!cap) continue;
    unsigned color=(id&15)/4;
    int sx=x*16+8-room->cameraX+f->originX+(vertical?0:24);
    int sy=y*16+8-room->cameraY+(vertical?24:0);
    int dx=sx-s->samusX,dy=sy-s->samusY,d=dx*dx+dy*dy;
    unsigned place=d<distance[0]?0:1;
    if (d>=distance[place]) continue;
    if (!place) {doors[1]=doors[0];distance[1]=distance[0];}
    doors[place]=(SceneLight){sx,sy,88,rgb[color][0],rgb[color][1],rgb[color][2]};
    distance[place]=d;
    if (count<2) count++;
  }
  for (unsigned i=0;i<count;i++) {
    const SceneLight *l=&doors[i];
    SceneLighting_AddLight(s,l->x,l->y,l->radius,l->r,l->g,l->b);
  }
}

void SceneLighting_Ambient(PicaFrame *f) {
  SceneLighting *s=f->lighting;
  if (!s || s->ship || !f->memory || !f->lines ||
      f->worldLeft>=f->worldRight || f->hudEndY>=f->height) return;
  /* Six palette samples in foreground scenery, not the HUD or sprite colors.
   * Broad vegetation/cavern color is diffuse ambience, never a fake lamp.
   * Each of the eight BG palettes is examined at most once per frame. */
  unsigned palettes=0;
  for (unsigned row=0;row<2;row++) {
    unsigned y=f->hudEndY+(f->height-f->hudEndY)*(row?3:1)/4;
    const PicaLine *p=&f->lines[y];
    if (p->forcedBlank || !((p->screenEnabled[0] |
        (p->addSubscreen?p->screenEnabled[1]:0))&1)) continue;
    const BgLayer *bg=&p->bg[0];
    unsigned wy=(y+bg->vScroll)&(bg->tilemapHigher?511:255);
    for (unsigned col=0;col<3;col++) {
      /* Keep samples in the native VRAM center; side room blocks may differ. */
      int x=(int)f->originX+16+col*112;
      if (x<f->worldLeft || x>=f->worldRight) continue;
      unsigned wx=(x-(int)f->originX+bg->hScroll)&(bg->tilemapWider?511:255);
      unsigned map=bg->tilemapAdr+((wy>>3)&31)*32+((wx>>3)&31)+
        (wx>=256?0x400:0)+(wy>=256?(bg->tilemapWider?0x800:0x400):0);
      unsigned tile=f->memory->vram[map&0x7fff];
      if (tile&1023) palettes|=1u<<((tile>>10)&7);
    }
  }
  unsigned rgb[3]={0},weight=0;
  for (unsigned palette=0;palette<8;palette++) if (palettes&(1u<<palette))
    for (unsigned i=1;i<16;i++) {
      unsigned c=f->memory->cgram[palette*16+i];
      unsigned r=c&31,g=(c>>5)&31,b=(c>>10)&31;
      unsigned high=r>g?r:g; high=high>b?high:b;
      unsigned low=r<g?r:g; low=low<b?low:b;
      if (high<8 || high-low<6) continue;
      unsigned w=high-low;
      rgb[0]+=r*w; rgb[1]+=g*w; rgb[2]+=b*w; weight+=w;
    }
  if (!weight) return;
  unsigned high=rgb[0]>rgb[1]?rgb[0]:rgb[1]; high=high>rgb[2]?high:rgb[2];
  /* Stronger colored bounce in the distant layer, with a small foreground
   * influence. Bounded brightness keeps interiors dark and objects readable. */
  const uint8_t base[2][3]={{150,158,170},{88,101,121}};
  for (unsigned c=0;c<3;c++) {
    int tint=(int)(rgb[c]*48/high)-24;
    s->ambient[0][c]=base[0][c]+tint/2;
    s->ambient[1][c]=base[1][c]+tint;
  }
}

void SceneLighting_Prepare(PicaFrame *f) {
  SceneLighting *s = f->lighting;
  SceneLighting_Ambient(f);
  /* One projection per frame: cast away from the nearest environmental
   * source, never from Samus's own fill/charge. Keep a stable diagonal in
   * unlit corridors. This remains a bounded sprite silhouette, not geometry. */
  s->shadowX = s->ship && s->samusX <= s->lights[0].x ? -10 : 10;
  s->shadowY = 9;
  if (!s->ship) {
    int best = 0x7fffffff;
    for (unsigned i=s->environmentStart; i<s->count; i++) {
      const SceneLight *l=&s->lights[i];
      if (l->radius<64) continue;
      int dx=s->samusX-l->x,dy=s->samusY-l->y;
      int distance=dx*dx+dy*dy;
      if (distance<best && distance<l->radius*l->radius && (dx || dy)) {
        best=distance;
        int span=abs(dx)>abs(dy)?abs(dx):abs(dy);
        s->shadowX=dx*12/span; s->shadowY=dy*12/span;
      }
    }
  }
  /* Precompute finished modulation colors for a small grid. Both SNES
   * screens and all repeated tiles read these bytes without texture probes,
   * relief hashes or per-quad light arithmetic. Exact source changes rebuild
   * immediately; a stationary view reuses the grid across atlas swaps. */
  SceneFieldKey key;
  memset(&key,0,sizeof(key));
  key.count=s->count; key.width=f->width; key.height=f->height;
  key.hudEndY=f->hudEndY;
  for (unsigned i=0;i<s->count;i++) {
    const SceneLight *l=&s->lights[i];
    key.lights[i][0]=l->x; key.lights[i][1]=l->y;
    key.lights[i][2]=l->radius; key.lights[i][3]=l->r;
    key.lights[i][4]=l->g; key.lights[i][5]=l->b;
  }
  if (s->ship) {
    const uint8_t ship[3][3]={{158,176,202},{100,126,161},{190,198,210}};
    memcpy(key.base,ship,sizeof(ship));
  } else {
    memcpy(key.base,s->ambient,sizeof(s->ambient));
    key.base[2][0]=188; key.base[2][1]=196; key.base[2][2]=208;
  }
  if (s->fieldValid && !memcmp(&key,&s->fieldKey,sizeof(key))) return;
  memcpy(&s->fieldKey,&key,sizeof(key)); s->fieldValid=true;
  uint16_t field[SCENE_FIELD_H][SCENE_FIELD_W][3]={0};
  unsigned cols=(f->width+SCENE_FIELD_CELL-1)>>SCENE_FIELD_SHIFT;
  unsigned rows=(f->height+SCENE_FIELD_CELL-1)>>SCENE_FIELD_SHIFT;
  for (unsigned i=0;i<s->count;i++) {
    const SceneLight *l=&s->lights[i];
    int radius2=l->radius*l->radius;
    uint32_t reciprocal=(1u<<24)/radius2;
    int left=Clamp((l->x-l->radius)/SCENE_FIELD_CELL,0,cols);
    int right=Clamp((l->x+l->radius+SCENE_FIELD_CELL-1)/SCENE_FIELD_CELL,0,cols);
    int top=Clamp((l->y-l->radius)/SCENE_FIELD_CELL,
                  f->hudEndY>>SCENE_FIELD_SHIFT,rows);
    int bottom=Clamp((l->y+l->radius+SCENE_FIELD_CELL-1)/SCENE_FIELD_CELL,top,rows);
    for (int y=top;y<bottom;y++) {
      int dy=y*SCENE_FIELD_CELL+SCENE_FIELD_CELL/2-l->y;
      for (int x=left;x<right;x++) {
        int dx=x*SCENE_FIELD_CELL+SCENE_FIELD_CELL/2-l->x;
        int distance2=dx*dx+dy*dy;
        if (distance2>=radius2) continue;
        unsigned weight=((uint32_t)(radius2-distance2)*reciprocal)>>16;
        weight=(weight*weight)>>8;
        field[y][x][0]+=(l->r*weight)>>8;
        field[y][x][1]+=(l->g*weight)>>8;
        field[y][x][2]+=(l->b*weight)>>8;
      }
    }
  }
  for (unsigned layer=0;layer<3;layer++)
    for (unsigned y=0;y<rows;y++) for (unsigned x=0;x<cols;x++)
      for (unsigned c=0;c<3;c++)
        s->color[layer][y][x][c]=Clamp(key.base[layer][c]+
            (layer==1?field[y][x][c]*3/4:field[y][x][c]),0,255);
}

void SceneLighting_Tile(PicaFrame *f, unsigned group, unsigned slot,
                        PicaQuad *q) {
  (void)slot;
  SceneLighting *s = f->lighting;
  if (!s || !s->enabled || q->y0 < (int)f->hudEndY) return;
  /* BG3 contains the HUD and weather; leave both crisp and unmodified. */
  bool object = group & 1;
  unsigned layer = q->depth == 0x8000 || q->depth == 0xc000 ? 0 :
                   q->depth == 0x7100 || q->depth == 0xb100 ? 1 : 2;
  if (!object && layer == 2) return;
  if (object && s->castShadow && s->shadowCount < SCENE_MAX_SHADOWS) {
    PicaQuad shadow = *q;
    /* Interior silhouettes also reach low-priority BG1 wall art; BG1-high
     * foreground still occludes them. Landing Site keeps its original depth. */
    shadow.x0 += s->shadowX; shadow.x1 += s->shadowX;
    shadow.y0 += s->shadowY; shadow.y1 += s->shadowY;
    shadow.depth = s->ship ? 0x7200 : 0x8100;
    shadow.r = shadow.g = shadow.b = 0;
    shadow.a = s->ship ? 78 : 96;
    int x0 = Clamp(shadow.x0, f->worldLeft, f->worldRight);
    int x1 = Clamp(shadow.x1, f->worldLeft, f->worldRight);
    int y0 = Clamp(shadow.y0, f->hudEndY, f->height);
    int y1 = Clamp(shadow.y1, f->hudEndY, f->height);
    if (x0 < x1 && y0 < y1) {
      int du = (shadow.u1 - shadow.u0) / (shadow.x1 - shadow.x0);
      int dv = (shadow.v1 - shadow.v0) / (shadow.y1 - shadow.y0);
      shadow.u0 += (x0 - shadow.x0) * du;
      shadow.u1 -= (shadow.x1 - x1) * du;
      shadow.v0 += (y0 - shadow.y0) * dv;
      shadow.v1 -= (shadow.y1 - y1) * dv;
      shadow.x0 = x0; shadow.x1 = x1; shadow.y0 = y0; shadow.y1 = y1;
      s->shadows[s->shadowCount++] = shadow;
    }
  }
  unsigned x=Clamp((q->x0+q->x1)>>(SCENE_FIELD_SHIFT+1),0,SCENE_FIELD_W-1);
  unsigned y=Clamp((q->y0+q->y1)>>(SCENE_FIELD_SHIFT+1),0,SCENE_FIELD_H-1);
  const uint8_t *color=s->color[object?2:layer][y][x];
  q->r=color[0]; q->g=color[1]; q->b=color[2];
}

bool SceneLighting_EmitShadows(PicaFrame *f, unsigned sub) {
  SceneLighting *s = f->lighting;
  if (!s || !s->enabled) return true;
  unsigned group = 21 + sub;
  for (unsigned i = 0; i < s->shadowCount; i++) {
    if (!f->emit(f->context, group, &s->shadows[i])) {
      f->failure = "shadow-capacity";
      return false;
    }
    f->quads[group]++;
  }
  s->shadowCount = 0;
  return true;
}
