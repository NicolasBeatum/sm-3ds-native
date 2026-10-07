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
  s->count = s->shadowCount = s->environmentStart = s->backgroundCount = 0;
  s->castShadow = false;
  s->liquidType = 0;
  /* Door/elevator transitions fade palettes while remaining in states 9-11.
   * Keep the same modulation throughout instead of flashing back to unlit. */
  s->enabled = enabled && state >= 8 && state <= 11;
  s->transition = state >= 9 && state <= 11;
  s->glowAlpha = 65;
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

/* Resolve the live 8x8 map, including both extra tilemap banks. Coordinates
 * are screen-relative, so background lamps follow BG2 parallax, not BG1. */
static unsigned BackgroundTile(const PicaFrame *f, const BgLayer *bg, int x, int y) {
  unsigned wx=(x-(int)f->originX+bg->hScroll)&(bg->tilemapWider?511:255);
  unsigned wy=(y+bg->vScroll)&(bg->tilemapHigher?511:255);
  unsigned map=bg->tilemapAdr+((wy>>3)&31)*32+((wx>>3)&31)+
      (wx>=256?0x400:0)+(wy>=256?(bg->tilemapWider?0x800:0x400):0);
  return f->memory->vram[map&0x7fff];
}

static uint32_t BackgroundHash(const PicaFrame *f, const BgLayer *bg, unsigned id) {
  uint32_t hash=2166136261u;
  for (unsigned i=0;i<16;i++)
    hash=(hash^f->memory->vram[(bg->tileAdr+id*16+i)&0x7fff])*16777619u;
  return hash;
}

typedef struct BackgroundMotif {
  uint16_t ids[4];
  uint32_t hashes[4];
  uint8_t size, color, radius;
} BackgroundMotif;

/* Art fingerprints, not a brightness heuristic or a room whitelist. The two
 * organic motifs are interpreted as bioluminescence; the other two are the
 * complete red/blue status-light icons in Tourian machinery. Matching all
 * tiles prevents these shared indices becoming lamps in another tileset. */
static const BackgroundMotif kBackgroundMotifs[] = {
  {{0xec,0xed,0xfc,0xfd},{0x97469c3d,0x2051f0ef,0xda492305,0xd809cf01},2,1,80},
  {{0xee,0xef,0xfe,0xff},{0x0bbd809c,0x1435066d,0x839b9a65,0x35e56785},2,1,80},
  {{0x23},{0x188a690d},1,8,72},
  {{0x33},{0xe5efd179},1,10,72},
};

static bool BackgroundExposed(const PicaFrame *f, int x, int y) {
  const PicaWideRoomLayer *room=&f->wideRoom[0];
  if (!room->blocks || !f->wideRoomWidth || !f->wideRoomHeight) return true;
  int wx=room->cameraX+x-(int)f->originX,wy=room->cameraY+y;
  if (wx<0 || wy<0 || wx>=(int)f->wideRoomWidth*16 || wy>=(int)f->wideRoomHeight*16)
    return false;
  unsigned type=room->blocks[(wy/16)*f->wideRoomWidth+wx/16]>>12;
  /* Conservative full-block occlusion; slopes/extensions can contain gaps. */
  return !((0xd700u>>type)&1u);
}

void SceneLighting_AddBackgroundSources(PicaFrame *f) {
  SceneLighting *s=f->lighting;
  if (!s || !s->enabled || s->ship || s->count>=SCENE_MAX_LIGHTS ||
      !f->memory || !f->lines || f->hudEndY>=f->height) return;
  unsigned first=f->hudEndY;
  while (first<f->height && f->lines[first].forcedBlank) first++;
  if (first==f->height) return;
  const BgLayer *bg=&f->lines[first].bg[1];
  if (bg->bigTiles || !((f->lines[first].screenEnabled[0] |
      (f->lines[first].addSubscreen?f->lines[first].screenEnabled[1]:0))&2)) return;
  bool available[4]={0};
  bool any=false;
  for (unsigned i=0;i<4;i++) {
    const BackgroundMotif *m=&kBackgroundMotifs[i];
    available[i]=true;
    for (unsigned j=0;j<m->size*m->size;j++)
      if (BackgroundHash(f,bg,m->ids[j])!=m->hashes[j]) {available[i]=false;break;}
    any|=available[i];
  }
  if (!any) return; /* Most tilesets need only four small fingerprint checks. */
  /* Unsupported scanline-warped backgrounds cannot give stable lamp positions. */
  for (unsigned y=first;y<f->height;y++) {
    const PicaLine *line=&f->lines[y];
    if (line->forcedBlank) continue;
    const BgLayer *b=&line->bg[1];
    if (b->tileAdr!=bg->tileAdr || b->tilemapAdr!=bg->tilemapAdr ||
        b->hScroll!=bg->hScroll || b->vScroll!=bg->vScroll ||
        b->tilemapWider!=bg->tilemapWider || b->tilemapHigher!=bg->tilemapHigher ||
        b->bigTiles || !((line->screenEnabled[0] |
        (line->addSubscreen?line->screenEnabled[1]:0))&2)) return;
  }
  int left=f->worldLeft,right=f->worldRight;
  if (f->boundBg2) {
    if (left<f->bg2Left) left=f->bg2Left;
    if (right>f->bg2Right) right=f->bg2Right;
  }
  int x0=left-((left-(int)f->originX+bg->hScroll)&7);
  int y0=(int)first-((first+bg->vScroll)&7);
  SceneLight nearest[2]={{0}};
  int distance[2]={0x7fffffff,0x7fffffff};
  unsigned count=0;
  for (int y=y0;y<(int)f->height;y+=8) for (int x=x0;x<right;x+=8) {
    unsigned tile=BackgroundTile(f,bg,x,y);
    for (unsigned i=0;i<4;i++) if (available[i]) {
      const BackgroundMotif *m=&kBackgroundMotifs[i];
      unsigned hf=(tile>>14)&1,vf=(tile>>15)&1;
      unsigned corner=m->size==2?vf*2+hf:0;
      if ((tile&1023)!=m->ids[corner]) continue;
      bool complete=true;
      for (unsigned yy=0;yy<m->size;yy++) for (unsigned xx=0;xx<m->size;xx++) {
        unsigned expected=m->ids[(yy^(m->size==2?vf:0))*m->size+(xx^(m->size==2?hf:0))];
        unsigned t=BackgroundTile(f,bg,x+xx*8,y+yy*8);
        if ((t&0xdc00)!=(tile&0xdc00) || (t&1023)!=expected) complete=false;
      }
      if (!complete) continue;
      int cx=x+m->size*4,cy=y+m->size*4;
      if (m->size==1) {
        /* Neighboring identical LED icons form one panel, not stacked halos. */
        unsigned descriptor=tile&0xdfff;
        if ((BackgroundTile(f,bg,x-8,y)&0xdfff)==descriptor ||
            (BackgroundTile(f,bg,x,y-8)&0xdfff)==descriptor) continue;
        if ((BackgroundTile(f,bg,x+8,y)&0xdfff)==descriptor) cx+=4;
        if ((BackgroundTile(f,bg,x,y+8)&0xdfff)==descriptor) cy+=4;
      }
      if (cx<left || cx>=right || cy<(int)first || cy>=(int)f->height ||
          !BackgroundExposed(f,cx,cy)) continue;
      unsigned c=f->memory->cgram[((tile>>10)&7)*16+m->color];
      unsigned r=c&31,g=(c>>5)&31,b=(c>>10)&31;
      unsigned high=r>g?r:g;high=high>b?high:b;
      if (high<3) continue; /* Unpowered/faded icons do not light the room. */
      int dx=cx-s->samusX,dy=cy-s->samusY,d=dx*dx+dy*dy;
      unsigned place=d<distance[0]?0:1;
      if (d>=distance[place]) continue;
      if (!place) {nearest[1]=nearest[0];distance[1]=distance[0];}
      nearest[place]=(SceneLight){cx,cy,m->radius,r*180/31,g*180/31,b*180/31};
      distance[place]=d;
      if (count<2) count++;
    }
  }
  for (unsigned i=0;i<count && s->count<SCENE_MAX_LIGHTS;i++) {
    const SceneLight *l=&nearest[i];
    SceneLighting_AddLight(s,l->x,l->y,l->radius,l->r,l->g,l->b);
    s->backgroundCount++;
  }
}

void SceneLighting_Ambient(PicaFrame *f) {
  SceneLighting *s=f->lighting;
  if (!s || s->ship || !f->memory || !f->lines ||
      f->worldLeft>=f->worldRight || f->hudEndY>=f->height) return;
  /* Six tile-row samples per layer: 48 actual pixels, not entire palettes.
   * A colored wall contributes diffuse ambience, never a local point light.
   * BG2 sets the room hue; BG1 is a fallback when BG2 has no colored samples. */
  unsigned rgb[2][3]={{0}},weight[2]={0};
  for (unsigned layer=0;layer<2;layer++)
  for (unsigned row=0;row<2;row++) {
    unsigned y=f->hudEndY+(f->height-f->hudEndY)*(row?3:1)/4;
    const PicaLine *p=&f->lines[y];
    if (p->forcedBlank || !((p->screenEnabled[0] |
        (p->addSubscreen?p->screenEnabled[1]:0))&(1u<<layer))) continue;
    const BgLayer *bg=&p->bg[layer];
    if (bg->bigTiles) continue;
    unsigned wy=y+bg->vScroll;
    for (unsigned col=0;col<3;col++) {
      int x=(int)f->originX+16+col*112;
      if (x<f->worldLeft || x>=f->worldRight || (layer==1 && f->boundBg2 &&
          (x<f->bg2Left || x>=f->bg2Right))) continue;
      unsigned tile=BackgroundTile(f,bg,x,y);
      unsigned ty=(wy&7)^((tile&0x8000)?7:0);
      unsigned addr=bg->tileAdr+(tile&1023)*16+ty;
      unsigned a=f->memory->vram[addr&0x7fff],b=f->memory->vram[(addr+8)&0x7fff];
      for (unsigned bit=0;bit<8;bit++) {
        unsigned index=((a>>bit)&1)|(((a>>(bit+8))&1)<<1)|
            (((b>>bit)&1)<<2)|(((b>>(bit+8))&1)<<3);
        if (!index) continue;
        unsigned c=f->memory->cgram[((tile>>10)&7)*16+index];
        unsigned r=c&31,g=(c>>5)&31,blue=(c>>10)&31;
        unsigned high=r>g?r:g; high=high>blue?high:blue;
        unsigned low=r<g?r:g; low=low<blue?low:blue;
        if (high<8 || high-low<6) continue;
        unsigned w=high-low;
        rgb[layer][0]+=r*w;rgb[layer][1]+=g*w;rgb[layer][2]+=blue*w;weight[layer]+=w;
      }
    }
  }
  unsigned layer=weight[1]?1:0;
  if (!weight[layer]) return;
  unsigned high=rgb[layer][0]>rgb[layer][1]?rgb[layer][0]:rgb[layer][1];
  high=high>rgb[layer][2]?high:rgb[layer][2];
  const uint8_t base[2][3]={{150,158,170},{88,101,121}};
  for (unsigned c=0;c<3;c++) {
    int tint=(int)(rgb[layer][c]*48/high)-24;
    s->ambient[0][c]=base[0][c]+tint/2;
    s->ambient[1][c]=base[1][c]+tint;
  }
}

static int FloorBlock(int pixel) {
  return pixel>=0 ? pixel/16 : -((-pixel+15)/16);
}

static void ContactCells(const PicaFrame *f, SceneFieldKey *key) {
  const PicaWideRoomLayer *room=&f->wideRoom[0];
  if (f->lighting->ship || !room->blocks || !f->wideRoomWidth || !f->wideRoomHeight) return;
  unsigned cols=(f->width+15)/16,rows=(f->height+15)/16;
  for (unsigned y=0;y<rows+2;y++) {
    int by=FloorBlock(room->cameraY+(int)y*16-8);
    if (by<0 || by>=(int)f->wideRoomHeight) continue;
    for (unsigned x=0;x<cols+2;x++) {
      int bx=FloorBlock(room->cameraX+(int)x*16-8-(int)f->originX);
      if (bx<0 || bx>=(int)f->wideRoomWidth) continue;
      unsigned type=room->blocks[by*f->wideRoomWidth+bx]>>12;
      /* Full solid/shootable/spike/bomb blocks and door triggers. Do not
       * invent full walls from air, slopes or unresolvable BTS extensions. */
      key->occupied[y][x]=(0xd700u>>type)&1u;
    }
  }
}

static unsigned ContactShade(const SceneFieldKey *key, unsigned x, unsigned y,
                             unsigned layer) {
  if (layer==2) return 0; /* Samus and enemies retain their readability. */
  const uint8_t (*cells)[SCENE_FIELD_W+2]=key->occupied;
  x++; y++;
  if (cells[y][x]) return layer==1 ? 36 : 0;
  unsigned edges=cells[y-1][x]+cells[y+1][x]+cells[y][x-1]+cells[y][x+1];
  unsigned corners=cells[y-1][x-1]+cells[y-1][x+1]+
                   cells[y+1][x-1]+cells[y+1][x+1];
  return Clamp(edges*9+corners*3,0,layer==1?42:30);
}

static void GlowVisibility(PicaFrame *f) {
  SceneLighting *s=f->lighting;
  if (!f->memory || !f->lines) return;
  bool visible=false;
  for (unsigned y=f->hudEndY;y<f->height;y++)
    if (!f->lines[y].forcedBlank && f->lines[y].brightness) {visible=true;break;}
  if (!visible) {s->glowAlpha=0;return;}
  if (!s->transition) return;
  /* BG palette 0/1 contains HUD and colors deliberately kept during door
   * fades. Use the fading scenery and Samus palette, never those HUD colors. */
  unsigned high=0;
  for (unsigned i=32;i<208;i++) {
    if (i>=128 && i<192) continue;
    unsigned color=f->memory->cgram[i];
    for (unsigned shift=0;shift<15;shift+=5) {
      unsigned channel=(color>>shift)&31;
      if (channel>high) high=channel;
    }
  }
  s->glowAlpha=65*high/31;
}

void SceneLighting_Prepare(PicaFrame *f) {
  SceneLighting *s = f->lighting;
  SceneLighting_Ambient(f);
  GlowVisibility(f);
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
  ContactCells(f,&key);
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
#ifdef SM3DS_PHASE_DIAG
  g_pica_build_timing.light_field_rebuilt = true;
#endif
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
    for (unsigned y=0;y<rows;y++) for (unsigned x=0;x<cols;x++) {
      unsigned shade=ContactShade(&key,x,y,layer);
      for (unsigned c=0;c<3;c++)
        s->color[layer][y][x][c]=Clamp(key.base[layer][c]+
            (layer==1?field[y][x][c]*3/4:field[y][x][c])-
            shade,0,255);
    }
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
