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
      Clamp(r,0,255),Clamp(g,0,255),Clamp(b,0,255),true,false};
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
  s->distantBackground=s->ship;
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
  /* Suit fill reveals nearby surfaces; it is not a luminous fog ball. */
  s->lights[s->count-1].glow=false;
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
    int width = f->worldRight - f->worldLeft;
    int y = s->liquidY + 12;
    unsigned r=200,g=92,b=28;
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
          best=luma; r=cr*200/31;g=cg*200/31;b=cb*200/31;
        }
      }
    }
    /* The whole surface emits, rather than screen-positioned circles. One
     * cached strip replaces two/three point lights and their bloom quads.
     * Its real Y also stays stable when the camera crosses the surface. */
    if(width>0) {
      SceneLighting_AddLight(s, f->worldLeft + width/2, y,112,r,g,b);
      s->lights[s->count-1].surface=true;
      s->lights[s->count-1].glow=false;
    }
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
    doors[place]=(SceneLight){sx,sy,88,rgb[color][0],rgb[color][1],rgb[color][2],true,false};
    distance[place]=d;
    if (count<2) count++;
  }
  for (unsigned i=0;i<count;i++) {
    const SceneLight *l=&doors[i];
    SceneLighting_AddLight(s,l->x,l->y,l->radius,l->r,l->g,l->b);
  }
}

/* Resolve live 8x8 map banks. Cache coordinates are in the native tilemap,
 * independent of BG1/BG2 scroll, viewport origin and the two atlas buffers. */
static unsigned MapTile(const PicaFrame *f, const BgLayer *bg, int x, int y) {
  unsigned wx=x&(bg->tilemapWider?511:255),wy=y&(bg->tilemapHigher?511:255);
  unsigned map=bg->tilemapAdr+((wy>>3)&31)*32+((wx>>3)&31)+
      (wx>=256?0x400:0)+(wy>=256?(bg->tilemapWider?0x800:0x400):0);
  return f->memory->vram[map&0x7fff];
}
static unsigned BackgroundTile(const PicaFrame *f, const BgLayer *bg, int x, int y) {
  return MapTile(f,bg,x-(int)f->originX+bg->hScroll,y+bg->vScroll);
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
  uint8_t size, color, radius, layer;
} BackgroundMotif;

/* Complete art fingerprints, never arbitrary bright pixels. The organic
 * motifs are interpreted as bioluminescence. BG1 adds actual machinery pilot
 * lamps and the elevator's yellow indicators, shared by several biomes. */
static const BackgroundMotif kBackgroundMotifs[] = {
  {{0xec,0xed,0xfc,0xfd},{0x97469c3d,0x2051f0ef,0xda492305,0xd809cf01},2,1,80,1},
  {{0xee,0xef,0xfe,0xff},{0x0bbd809c,0x1435066d,0x839b9a65,0x35e56785},2,1,80,1},
  {{0x23},{0x188a690d},1,8,72,1},
  {{0x33},{0xe5efd179},1,10,72,1},
  {{0x26,0x27,0x36,0x37},{0x0e468161,0xc92c320b,0x6b486d43,0x16ccfb4f},2,8,96,0},
  {{0x2ea,0x2eb,0x2f8,0x2f9},{0x8d1e64df,0x7c733a82,0xa2299402,0x435b87cd},2,1,96,0},
};
enum { kMotifs=sizeof(kBackgroundMotifs)/sizeof(kBackgroundMotifs[0]) };

static bool BackgroundExposed(const PicaFrame *f, int x, int y) {
  const PicaWideRoomLayer *room=&f->wideRoom[0];
  if (!room->blocks || !f->wideRoomWidth || !f->wideRoomHeight) return true;
  int wx=room->cameraX+x-(int)f->originX,wy=room->cameraY+y;
  if (wx<0 || wy<0 || wx>=(int)f->wideRoomWidth*16 || wy>=(int)f->wideRoomHeight*16)
    return false;
  unsigned type=room->blocks[(wy/16)*f->wideRoomWidth+wx/16]>>12;
  return !((0xd700u>>type)&1u);
}

/* Reject almost every tile with a single ID lookup, before checking a motif. */
static int MotifAt(const PicaFrame *f, const BgLayer *bg, unsigned mask,
                   int x, int y, SceneEmitter *e) {
  unsigned tile=MapTile(f,bg,x,y),id=tile&1023;
  unsigned i;
  switch (id) {
    case 0xec:case 0xed:case 0xfc:case 0xfd:i=0;break;
    case 0xee:case 0xef:case 0xfe:case 0xff:i=1;break;
    case 0x23:i=2;break;
    case 0x33:i=3;break;
    case 0x26:case 0x27:case 0x36:case 0x37:i=4;break;
    case 0x2ea:case 0x2eb:case 0x2f8:case 0x2f9:i=5;break;
    default:return -1;
  }
  if (!(mask&(1u<<i))) return -1;
  const BackgroundMotif *m=&kBackgroundMotifs[i];
  unsigned hf=(tile>>14)&1,vf=(tile>>15)&1;
  unsigned corner=m->size==2?vf*2+hf:0;
  if (id!=m->ids[corner]) return -1;
  for (unsigned yy=0;yy<m->size;yy++) for (unsigned xx=0;xx<m->size;xx++) {
    unsigned expected=m->ids[(yy^(m->size==2?vf:0))*m->size+(xx^(m->size==2?hf:0))];
    unsigned t=MapTile(f,bg,x+xx*8,y+yy*8);
    if ((t&0xdc00)!=(tile&0xdc00) || (t&1023)!=expected) return -1;
  }
  int cx=x+m->size*4,cy=y+m->size*4;
  if (m->size==1) {
    unsigned descriptor=tile&0xdfff;
    if ((MapTile(f,bg,x-8,y)&0xdfff)==descriptor ||
        (MapTile(f,bg,x,y-8)&0xdfff)==descriptor) return -1;
    if ((MapTile(f,bg,x+8,y)&0xdfff)==descriptor) cx+=4;
    if ((MapTile(f,bg,x,y+8)&0xdfff)==descriptor) cy+=4;
  }
  /* The two mirrored halves of an elevator make one central warm light. */
  if (i==5) {
    if (hf) return -1;
    unsigned partner=MapTile(f,bg,x+16,y);
    if ((partner&0xdc00)!=((tile&0xdc00)|0x4000) || (partner&1023)!=m->ids[1]) return -1;
    for (unsigned yy=0;yy<2;yy++) for (unsigned xx=0;xx<2;xx++) {
      unsigned t=MapTile(f,bg,x+16+xx*8,y+yy*8);
      if ((t&0xdc00)!=((tile&0xdc00)|0x4000) || (t&1023)!=m->ids[yy*2+(xx^1)]) return -1;
    }
    cx+=8;
  }
  *e=(SceneEmitter){cx,cy,tile,i};
  return i;
}

static bool StableScenery(const PicaFrame *f,unsigned first,unsigned layer) {
  const BgLayer *bg=&f->lines[first].bg[layer];
  for (unsigned y=first;y<f->height;y++) {
    const PicaLine *line=&f->lines[y];
    if (line->forcedBlank) continue;
    const BgLayer *b=&line->bg[layer];
    if (b->tileAdr!=bg->tileAdr || b->tilemapAdr!=bg->tilemapAdr ||
        b->hScroll!=bg->hScroll || b->vScroll!=bg->vScroll ||
        b->tilemapWider!=bg->tilemapWider || b->tilemapHigher!=bg->tilemapHigher ||
        b->bigTiles || !((line->screenEnabled[0] |
        (line->addSubscreen?line->screenEnabled[1]:0))&(1u<<layer))) return false;
  }
  return true;
}

static void CacheEmitters(const PicaFrame *f,const BgLayer *bg,unsigned mask,
                          SceneEmitterCache *cache) {
  unsigned words=1024u<<(bg->tilemapWider+bg->tilemapHigher);
  uint32_t hash=2166136261u;
  for (unsigned i=0;i<words;i++)
    hash=(hash^f->memory->vram[(bg->tilemapAdr+i)&0x7fff])*16777619u;
  if (cache->valid && cache->mapHash==hash && cache->mask==mask &&
      cache->tileAdr==bg->tileAdr && cache->mapAdr==bg->tilemapAdr &&
      cache->wider==bg->tilemapWider && cache->higher==bg->tilemapHigher) return;
  cache->mapHash=hash;cache->mask=mask;cache->tileAdr=bg->tileAdr;
  cache->mapAdr=bg->tilemapAdr;cache->wider=bg->tilemapWider;cache->higher=bg->tilemapHigher;
  cache->valid=true;cache->overflow=false;cache->count=0;
  unsigned width=bg->tilemapWider?512:256,height=bg->tilemapHigher?512:256;
  for (unsigned y=0;y<height;y+=8) for (unsigned x=0;x<width;x+=8) {
    SceneEmitter e;
    if (MotifAt(f,bg,mask,x,y,&e)<0) continue;
    if (cache->count==SCENE_EMITTER_CACHE) {cache->overflow=true;return;}
    cache->entries[cache->count++]=e;
  }
}

typedef struct SceneryChoice {
  SceneLight light;
  unsigned motif;
  int distance;
} SceneryChoice;

/* Streaming selection: cap work and spread the pools of light across the
 * view. A nearby icon cannot crowd out all the lamps farther along a wall. */
static void ChooseScenery(const PicaFrame *f,const SceneEmitter *e,int x,int y,
                           unsigned first,int left,int right,
                           SceneryChoice choices[SCENE_MAX_SCENERY_LIGHTS],unsigned *count) {
  const BackgroundMotif *m=&kBackgroundMotifs[e->motif];
  if (x<left || x>=right || y<(int)first || y>=(int)f->height ||
      (m->layer && !BackgroundExposed(f,x,y))) return;
  unsigned c=f->memory->cgram[((e->tile>>10)&7)*16+m->color];
  unsigned r=c&31,g=(c>>5)&31,b=(c>>10)&31;
  unsigned high=r>g?r:g;high=high>b?high:b;
  if (high<3) return;
  int dx=x-f->lighting->samusX,dy=y-f->lighting->samusY,d=dx*dx+dy*dy;
  SceneryChoice candidate={{x,y,m->radius,r*180/31,g*180/31,b*180/31,false,false},e->motif,d};
  for (unsigned i=0;i<*count;i++) {
    dx=x-choices[i].light.x;dy=y-choices[i].light.y;
    /* Keep different hues/lamp kinds even when adjacent. */
    if (choices[i].motif!=e->motif || dx*dx+dy*dy>=48*48) continue;
    if (d<choices[i].distance) choices[i]=candidate;
    return;
  }
  if (*count<SCENE_MAX_SCENERY_LIGHTS) {choices[(*count)++]=candidate;return;}
  unsigned worst=0;
  for (unsigned i=1;i<*count;i++) if (choices[i].distance>choices[worst].distance) worst=i;
  if (d<choices[worst].distance) choices[worst]=candidate;
}

void SceneLighting_AddBackgroundSources(PicaFrame *f) {
  SceneLighting *s=f->lighting;
  if (!s || !s->enabled || s->ship || s->count>=SCENE_MAX_LIGHTS ||
      !f->memory || !f->lines || f->hudEndY>=f->height) return;
  unsigned first=f->hudEndY;
  while (first<f->height && f->lines[first].forcedBlank) first++;
  if (first==f->height) return;
  SceneryChoice choices[SCENE_MAX_SCENERY_LIGHTS];unsigned count=0;
  for (unsigned layer=0;layer<2;layer++) {
    const BgLayer *bg=&f->lines[first].bg[layer];
    if (bg->bigTiles) continue;
    unsigned mask=0;
    for (unsigned i=0;i<kMotifs;i++) {
      const BackgroundMotif *m=&kBackgroundMotifs[i];
      if (m->layer!=layer) continue;
      bool match=true;
      for (unsigned j=0;j<m->size*m->size;j++)
        if (BackgroundHash(f,bg,m->ids[j])!=m->hashes[j]) {match=false;break;}
      if (match) mask|=1u<<i;
    }
    if (!mask || !StableScenery(f,first,layer)) continue;
    int left=f->worldLeft,right=f->worldRight;
    if (layer==1 && f->boundBg2) {
      if (left<f->bg2Left) left=f->bg2Left;
      if (right>f->bg2Right) right=f->bg2Right;
    }
    SceneEmitterCache *cache=&s->emitters[layer];
    CacheEmitters(f,bg,mask,cache);
    if (cache->overflow) {
      /* Unusually dense/custom maps must not lose visible lamps to the cap. */
      int x0=left-((left-(int)f->originX+bg->hScroll)&7);
      int y0=(int)first-((first+bg->vScroll)&7);
      for (int y=y0;y<(int)f->height;y+=8) for (int x=x0;x<right;x+=8) {
        SceneEmitter e;
        int mx=(x-(int)f->originX+bg->hScroll)&(bg->tilemapWider?511:255);
        int my=(y+bg->vScroll)&(bg->tilemapHigher?511:255);
        if (MotifAt(f,bg,mask,mx,my,&e)<0) continue;
        int cx=x+e.x-mx,cy=y+e.y-my;
        ChooseScenery(f,&e,cx,cy,first,left,right,choices,&count);
      }
    } else {
      int w=bg->tilemapWider?512:256,h=bg->tilemapHigher?512:256;
      for (unsigned i=0;i<cache->count;i++) {
        const SceneEmitter *e=&cache->entries[i];
        int cx=((e->x-bg->hScroll)&(w-1))+(int)f->originX;
        int cy=(e->y-bg->vScroll)&(h-1);
        while (cx>left) cx-=w;
        while (cy>(int)first) cy-=h;
        for (int y=cy;y<(int)f->height;y+=h) for (int x=cx;x<right;x+=w)
          ChooseScenery(f,e,x,y,first,left,right,choices,&count);
      }
    }
  }
  /* Give the two nearest selected sources halos; others only affect the
   * cached modulation grid. Existing ship/door/liquid/player bloom is intact. */
  for (unsigned i=0;i<count && s->count<SCENE_MAX_LIGHTS;i++) {
    unsigned nearest=i;
    for (unsigned j=i+1;j<count;j++) if (choices[j].distance<choices[nearest].distance) nearest=j;
    SceneryChoice swap=choices[i];choices[i]=choices[nearest];choices[nearest]=swap;
    const SceneLight *l=&choices[i].light;
    SceneLighting_AddLight(s,l->x,l->y,l->radius,l->r,l->g,l->b);
    s->lights[s->count-1].glow=i<2;
    s->backgroundCount++;
  }
}

static void AmbientMaterials(const PicaFrame *f, const BgLayer *bg,
                             SceneAmbientCache *cache) {
  /* Sample the native map, never screen positions: turning Samus also moves
   * the camera and used to select a different handful of colored pixels.
   * Spread 64 probes across both map pages, with staggered positions to avoid
   * repeatedly hitting the same part of a repeating wall. Read complete tiles
   * so thin details are included and flips cannot alter material coverage. */
  uint16_t samples[64][17];
  uint32_t hash=2166136261u;
  unsigned sx=bg->tilemapWider?8:4,sy=bg->tilemapHigher?8:4;
  for (unsigned row=0;row<8;row++) for (unsigned col=0;col<8;col++) {
    unsigned i=row*8+col;
    unsigned x=(col*sx+(row*3+col)%sx)*8;
    unsigned y=(row*sy+(col*3+row)%sy)*8;
    unsigned tile=MapTile(f,bg,x,y);
    samples[i][0]=tile&0x1fff;
    unsigned addr=bg->tileAdr+(tile&1023)*16;
    for (unsigned ty=0;ty<8;ty++) {
      samples[i][1+ty*2]=f->memory->vram[(addr+ty)&0x7fff];
      samples[i][2+ty*2]=f->memory->vram[(addr+ty+8)&0x7fff];
    }
    for (unsigned j=0;j<17;j++) hash=(hash^samples[i][j])*16777619u;
  }
  if (cache->valid && cache->hash==hash) return;
  cache->hash=hash;cache->valid=true;
  memset(cache->colors,0,sizeof(cache->colors));
  for (unsigned i=0;i<64;i++) for (unsigned r=0;r<8;r++) {
    unsigned palette=((samples[i][0]>>10)&7)*16;
    unsigned a=samples[i][1+r*2],b=samples[i][2+r*2];
    for (unsigned bit=0;bit<8;bit++) {
      unsigned index=((a>>bit)&1)|(((a>>(bit+8))&1)<<1)|
          (((b>>bit)&1)<<2)|(((b>>(bit+8))&1)<<3);
      if (index) cache->colors[palette+index]++;
    }
  }
}

void SceneLighting_Ambient(PicaFrame *f) {
  SceneLighting *s=f->lighting;
  if (!s || s->ship || !f->memory || !f->lines ||
      f->worldLeft>=f->worldRight || f->hudEndY>=f->height) return;
  /* Cache material coverage; palette animations/fades remain live. No sprite
   * palettes or unused palette entries can color the room. Rebuild coverage
   * only when sampled map/graphics data changes, including streamed tiles. */
  unsigned rgb[2][3]={{0}},weight[2]={0};
  unsigned y=f->hudEndY;
  while (y<f->height && f->lines[y].forcedBlank) y++;
  if (y==f->height) return;
  const PicaLine *p=&f->lines[y];
  for (unsigned layer=0;layer<2;layer++) {
    if (p->forcedBlank || !((p->screenEnabled[0] |
        (p->addSubscreen?p->screenEnabled[1]:0))&(1u<<layer))) continue;
    const BgLayer *bg=&p->bg[layer];
    if (bg->bigTiles) continue;
    SceneAmbientCache *cache=&s->materials[layer];
    AmbientMaterials(f,bg,cache);
    for (unsigned index=1;index<128;index++) {
      if (!cache->colors[index]) continue;
      unsigned c=f->memory->cgram[index];
      unsigned r=c&31,g=(c>>5)&31,blue=(c>>10)&31;
      unsigned high=r>g?r:g; high=high>blue?high:blue;
      unsigned low=r<g?r:g; low=low<blue?low:blue;
      if (high<8 || high-low<6) continue;
      unsigned w=(high-low)*cache->colors[index];
      rgb[layer][0]+=r*w;rgb[layer][1]+=g*w;rgb[layer][2]+=blue*w;weight[layer]+=w;
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
  /* A short silhouette on nearby BG1 art, never on distant BG2 scenery. */
  s->shadowX = s->ship && s->samusX <= s->lights[0].x ? -6 : 6;
  s->shadowY = 5;
  if (!s->ship) {
    int best = 0x7fffffff;
    for (unsigned i=s->environmentStart; i<s->count; i++) {
      const SceneLight *l=&s->lights[i];
      if (l->radius<64) continue;
      int dx=l->surface?0:s->samusX-l->x,dy=s->samusY-l->y;
      int distance=dx*dx+dy*dy;
      if (distance<best && distance<l->radius*l->radius && (dx || dy)) {
        best=distance;
        int span=abs(dx)>abs(dy)?abs(dx):abs(dy);
        s->shadowX=dx*8/span; s->shadowY=dy*8/span;
      }
    }
  }
  /* Precompute finished modulation colors for a small grid. Both SNES
   * screens and all repeated tiles read these bytes without texture probes,
   * relief hashes or per-quad light arithmetic. Exact source changes rebuild
   * immediately; a stationary view reuses the grid across atlas swaps. */
  SceneFieldKey key;
  memset(&key,0,sizeof(key));
  key.count=s->count;
  key.environmentStart=s->distantBackground?s->environmentStart:0;
  key.width=f->width; key.height=f->height;
  key.hudEndY=f->hudEndY;
  ContactCells(f,&key);
  for (unsigned i=0;i<s->count;i++) {
    const SceneLight *l=&s->lights[i];
    key.lights[i][0]=l->x; key.lights[i][1]=l->y;
    key.lights[i][2]=l->radius|(l->surface?512:0); key.lights[i][3]=l->r;
    key.lights[i][4]=l->g; key.lights[i][5]=l->b;
  }
  if (s->ship) {
    const uint8_t ship[3][3]={{158,176,202},{100,126,161},{190,198,210}};
    memcpy(key.base,ship,sizeof(ship));
  } else {
    memcpy(key.base,s->ambient,sizeof(s->ambient));
    key.base[2][0]=188; key.base[2][1]=196; key.base[2][2]=208;
    /* A bright lava surface needs a darker surround, rather than washing
     * every ledge in overlapping light. Leave its original pixels intact. */
    if((s->liquidType==2 || s->liquidType==4) && s->liquidY<(int)f->height+64)
      for(unsigned c=0;c<3;c++) {
        key.base[0][c]-=24;key.base[1][c]-=18;
      }
  }
  if (s->fieldValid && !memcmp(&key,&s->fieldKey,sizeof(key))) return;
#ifdef SM3DS_PHASE_DIAG
  g_pica_build_timing.light_field_rebuilt = true;
#endif
  memcpy(&s->fieldKey,&key,sizeof(key)); s->fieldValid=true;
  uint16_t field[2][SCENE_FIELD_H][SCENE_FIELD_W][3]={0};
  unsigned cols=(f->width+SCENE_FIELD_CELL-1)>>SCENE_FIELD_SHIFT;
  unsigned rows=(f->height+SCENE_FIELD_CELL-1)>>SCENE_FIELD_SHIFT;
  for (unsigned i=0;i<s->count;i++) {
    const SceneLight *l=&s->lights[i];
    int radius2=l->radius*l->radius;
    uint32_t reciprocal=(1u<<24)/radius2;
    int left=l->surface?0:Clamp((l->x-l->radius)/SCENE_FIELD_CELL,0,cols);
    int right=l->surface?(int)cols:Clamp((l->x+l->radius+SCENE_FIELD_CELL-1)/SCENE_FIELD_CELL,0,cols);
    int top=Clamp((l->y-l->radius)/SCENE_FIELD_CELL,
                  f->hudEndY>>SCENE_FIELD_SHIFT,rows);
    int bottom=Clamp((l->y+l->radius+SCENE_FIELD_CELL-1)/SCENE_FIELD_CELL,top,rows);
    for (int y=top;y<bottom;y++) {
      int dy=y*SCENE_FIELD_CELL+SCENE_FIELD_CELL/2-l->y;
      for (int x=left;x<right;x++) {
        int dx=l->surface?0:x*SCENE_FIELD_CELL+SCENE_FIELD_CELL/2-l->x;
        int distance2=dx*dx+dy*dy;
        if (distance2>=radius2) continue;
        unsigned weight=((uint32_t)(radius2-distance2)*reciprocal)>>16;
        weight=(weight*weight)>>8;
        unsigned rgb[3]={(l->r*weight)>>8,(l->g*weight)>>8,(l->b*weight)>>8};
        for(unsigned c=0;c<3;c++) {
          field[0][y][x][c]+=rgb[c];
          /* Suit/charge/projectiles and the ship lamps are close sources.
           * They cannot illuminate the distant clouds behind the ship. */
          if(i>=key.environmentStart) field[1][y][x][c]+=rgb[c];
        }
      }
    }
  }
  for (unsigned layer=0;layer<3;layer++)
    for (unsigned y=0;y<rows;y++) for (unsigned x=0;x<cols;x++) {
      unsigned shade=ContactShade(&key,x,y,layer);
      for (unsigned c=0;c<3;c++)
        s->color[layer][y][x][c]=Clamp(key.base[layer][c]+
            (layer==1?field[1][y][x][c]*3/4:field[0][y][x][c])-
            shade,0,255);
    }
}

/* BG2 can be mountains, clouds or a remote wall. A duplicate silhouette on
 * it looks like a floating ghost. Only accept projected pieces surrounded
 * by actual BG1 pixels. Five bounded probes per Samus piece, no raymarch,
 * texture readback, new render pass or per-background-quad work. */
static bool ShadowReceiver(const PicaFrame *f, unsigned group, int x, int y) {
  if(!f->memory || !f->lines) return false;
  const PicaLine *line=&f->lines[y];
  if(!(line->screenEnabled[group/2]&1)) return false;
  const BgLayer *bg=&line->bg[0];
  unsigned tile=BackgroundTile(f,bg,x,y);
  unsigned wx=(x-(int)f->originX+bg->hScroll)&7;
  unsigned wy=(y+bg->vScroll)&7;
  unsigned bit=(tile&0x4000)?wx:7-wx;
  unsigned row=(tile&0x8000)?7-wy:wy;
  unsigned address=bg->tileAdr+(tile&1023)*16+row;
  unsigned a=f->memory->vram[address&0x7fff];
  unsigned b=f->memory->vram[(address+8)&0x7fff];
  return ((a|a>>8|b|b>>8)>>bit)&1;
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
    /* The receiver is BG1-low; BG1-high foreground still occludes it. */
    shadow.x0 += s->shadowX; shadow.x1 += s->shadowX;
    shadow.y0 += s->shadowY; shadow.y1 += s->shadowY;
    shadow.depth = 0x8100;
    shadow.r = shadow.g = shadow.b = 0;
    shadow.a = s->ship ? 64 : 60;
    int x0 = Clamp(shadow.x0, f->worldLeft, f->worldRight);
    int x1 = Clamp(shadow.x1, f->worldLeft, f->worldRight);
    int y0 = Clamp(shadow.y0, f->hudEndY, f->height);
    int y1 = Clamp(shadow.y1, f->hudEndY, f->height);
    if (x0 < x1 && y0 < y1 &&
        ShadowReceiver(f,group,x0,y0) && ShadowReceiver(f,group,x1-1,y0) &&
        ShadowReceiver(f,group,x0,y1-1) && ShadowReceiver(f,group,x1-1,y1-1) &&
        ShadowReceiver(f,group,(x0+x1)/2,(y0+y1)/2)) {
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
