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
  if (!s->enabled) { s->lampValid=false; return; }
  memset(s->emission,0,sizeof(s->emission));
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
  SceneLight doors[16];
  int bounds[16][4];
  unsigned count=0;
  for (int y=y0; y<y1; y++) for (int x=x0; x<x1; x++) {
    unsigned block = room->blocks[y*f->wideRoomWidth+x];
    unsigned id = block & 0x3ff, type=block>>12;
    if (id>=0x40 || (id&3)==3 ||
        (type!=8 && type!=12 && type!=13 && type!=5)) continue;
    unsigned color=(id&15)/4;
    int sx=x*16+8-room->cameraX+f->originX, sy=y*16+8-room->cameraY;
    unsigned j=0;
    while(j<count && (doors[j].r!=color ||
          doors[j].x-sx < -56 || doors[j].x-sx > 56 ||
          doors[j].y-sy < -56 || doors[j].y-sy > 56)) j++;
    if (j<count) {
      bounds[j][0]=sx<bounds[j][0]?sx:bounds[j][0];
      bounds[j][1]=sx>bounds[j][1]?sx:bounds[j][1];
      bounds[j][2]=sy<bounds[j][2]?sy:bounds[j][2];
      bounds[j][3]=sy>bounds[j][3]?sy:bounds[j][3];
      doors[j].x=(bounds[j][0]+bounds[j][1])/2;
      doors[j].y=(bounds[j][2]+bounds[j][3])/2;
    } else if(count<16) {
      doors[count]=(SceneLight){sx,sy,88,color,0,0};
      bounds[count][0]=bounds[count][1]=sx;
      bounds[count][2]=bounds[count][3]=sy;
      count++;
    }
  }
  /* At most two nearby caps; keep room for moving lights and lamp cores. */
  static const uint8_t rgb[4][3]={{48,156,58},{156,48,82},{156,132,32},{54,100,174}};
  for (unsigned n=0; n<2 && count; n++) {
    unsigned nearest=0; int best=0x7fffffff;
    for(unsigned j=0;j<count;j++) {
      int dx=doors[j].x-s->samusX, dy=doors[j].y-s->samusY;
      int distance=dx*dx+dy*dy;
      if(distance<best) {best=distance;nearest=j;}
    }
    SceneLight l=doors[nearest]; const uint8_t *c=rgb[l.r];
    SceneLighting_AddLight(s,l.x,l.y,l.radius,c[0],c[1],c[2]);
    doors[nearest]=doors[--count];
  }
}

SceneEmission SceneLighting_Emission(PicaFrame *f, unsigned slot) {
  SceneLighting *s=f->lighting;
  PicaTile *tile=f->atlas?&f->atlas->tile[slot]:NULL;
  if (tile && (tile->emission&0x8000)) {
    unsigned c=tile->emission;
    return (SceneEmission){.key=slot+1,.r=(c&31)<<3,
      .g=((c>>5)&31)<<3,.b=((c>>10)&31)<<3};
  }
  SceneEmission *cached=&s->emission[(slot*2654435761u)>>24];
  if(!tile && cached->key==slot+1) return *cached;
  unsigned bright=0,dark=0,r=0,g=0,b=0;
  for(unsigned i=0;i<64;i++) {
    uint32_t p=f->pixels[slot*64+i];
    unsigned pr=p>>24,pg=(p>>16)&255,pb=(p>>8)&255;
    unsigned luma=(pr*3+pg*6+pb)/10;
    dark+=luma<64;
    unsigned high=pr>pg?pr:pg; high=high>pb?high:pb;
    unsigned low=pr<pg?pr:pg; low=low<pb?low:pb;
    /* Ignore white specular highlights on metal and bright wall textures. */
    if (high>=216 && high-low>=56 && luma>=130) {
      bright++;r+=pr;g+=pg;b+=pb;
    }
  }
  *cached=(SceneEmission){.key=slot+1};
  if(bright>=3 && bright<=20 && dark>=32) {
    cached->r=r/bright*3/4; cached->g=g/bright*3/4; cached->b=b/bright*3/4;
  }
  if (tile) {
    tile->emission=0x8000 | (cached->r>>3) | ((cached->g>>3)<<5) |
        ((cached->b>>3)<<10);
    cached->r&=248; cached->g&=248; cached->b&=248;
  }
  return *cached;
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
  memset(s->field, 0, sizeof(s->field));
  /* The atlas revision changes on decode/eviction, palette animation or
   * reinitialization. Pure host fixtures without an atlas remain per-frame. */
  if (!f->atlas) memset(s->relief, 0, sizeof(s->relief));
  else {
    /* The 3DS alternates two atlas buffers. Give each half of the existing
     * relief table instead of evicting every entry when the buffer changes. */
    unsigned bank=s->reliefAtlas[0]==f->atlas?0:
                  s->reliefAtlas[1]==f->atlas?1:
                  !s->reliefAtlas[0]?0:!s->reliefAtlas[1]?1:(s->reliefBank^1);
    if (s->reliefAtlas[bank]!=f->atlas) {
      s->reliefAtlas[bank]=f->atlas;
      memset(s->relief+bank*256,0,256*sizeof(SceneRelief));
    }
    s->reliefBank=bank;
  }
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
  /* One shared 8-pixel field for main/sub and BG/OBJ. Compute only inside
   * each source's screen bounds; division is once per light, never per quad.
   * Cell centers align with 8x8 tiles; cropped/flipped tiles keep their UVs. */
  for (unsigned i = 0; i < s->count; i++) {
    const SceneLight *l = &s->lights[i];
    int radius2 = l->radius * l->radius;
    uint32_t reciprocal = (1u << 24) / radius2;
    int left = Clamp((l->x - l->radius) / 8, 0, (int)f->width / 8);
    int right = Clamp((l->x + l->radius + 7) / 8, 0, (int)f->width / 8);
    int top = Clamp((l->y - l->radius) / 8, (int)f->hudEndY / 8,
                    ((int)f->height + 7) / 8);
    int bottom = Clamp((l->y + l->radius + 7) / 8, top, ((int)f->height + 7) / 8);
    for (int y = top; y < bottom; y++) {
      int dy = y * 8 + 4 - l->y;
      for (int x = left; x < right; x++) {
        int dx = x * 8 + 4 - l->x;
        int distance2 = dx * dx + dy * dy;
        if (distance2 >= radius2) continue;
        unsigned weight = ((uint32_t)(radius2 - distance2) * reciprocal) >> 16;
        weight = (weight * weight) >> 8;
        s->field[y][x][0] += (l->r * weight) >> 8;
        s->field[y][x][1] += (l->g * weight) >> 8;
        s->field[y][x][2] += (l->b * weight) >> 8;
      }
    }
  }
}

static unsigned TexelLuma(const PicaFrame *f, unsigned slot, int x, int y) {
  unsigned ux = Clamp(x, 0, 7), uy = Clamp(y, 0, 7);
  unsigned morton = (ux & 1) | ((uy & 1) << 1) | ((ux & 2) << 1) |
                    ((uy & 2) << 2) | ((ux & 4) << 2) | ((uy & 4) << 3);
  uint32_t pixel = f->pixels[slot * 64 + morton];
  return ((pixel >> 24) * 3 + ((pixel >> 16) & 255) * 6 +
          ((pixel >> 8) & 255)) / 10;
}

void SceneLighting_Tile(PicaFrame *f, unsigned group, unsigned slot,
                        PicaQuad *q) {
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
  int x = (q->x0 + q->x1) / 2, y = (q->y0 + q->y1) / 2;
  /* Interior contrast matches Landing Site: darker distant scenery and
   * readable foreground/objects. A mild cool ambient preserves palette hues;
   * environmental sources locally restore brightness and warm hot rooms. */
  int r = s->ship ? (object ? 190 : layer == 0 ? 158 : 100) :
                   (object ? 188 : s->ambient[layer][0]);
  int g = s->ship ? (object ? 198 : layer == 0 ? 176 : 126) :
                   (object ? 196 : s->ambient[layer][1]);
  int b = s->ship ? (object ? 210 : layer == 0 ? 202 : 161) :
                   (object ? 208 : s->ambient[layer][2]);
  /* A small luminance gradient gives tile surfaces directional relief.
   * Sample the actual flipped/cropped texture rather than screen tile edges.
   * This is an approximation, not an authored per-pixel normal map. */
  int tx = ((q->u0 + q->u1) / 8) & 7;
  int ty = ((8192 - q->v0 - q->v1) / 16) & 7;
  /* A decoded atlas slot is immutable for this frame. Reuse its relief for
   * repeated tiles and the second SNES screen; cropped centers are keys too. */
  uint32_t key = (slot | (tx << 13) | (ty << 16)) + 1;
  unsigned index=f->atlas ? s->reliefBank*256+((key*2654435761u)>>24) :
                                           (key*2654435761u)>>23;
  SceneRelief *cached = &s->relief[index];
  unsigned revision=f->atlas?f->atlas->contentRevision:0;
  if (cached->key != key || cached->revision!=revision) {
    int nx = (int)TexelLuma(f, slot, tx - 1, ty) -
             (int)TexelLuma(f, slot, tx + 1, ty);
    int ny = (int)TexelLuma(f, slot, tx, ty - 1) -
             (int)TexelLuma(f, slot, tx, ty + 1);
    cached->key = key;
    cached->revision=revision;
    cached->shade[0] = Clamp((-nx-ny)/10,-14,14);
    cached->shade[1] = Clamp(( nx-ny)/10,-14,14);
    cached->shade[2] = Clamp((-nx+ny)/10,-14,14);
    cached->shade[3] = Clamp(( nx+ny)/10,-14,14);
  }
  int relief = cached->shade[(q->u1 < q->u0) | ((q->v1 > q->v0) << 1)];
  r += relief; g += relief; b += relief;
  const uint16_t *light = s->field[Clamp(y / 8,0,SCENE_FIELD_H-1)]
                                   [Clamp(x / 8,0,SCENE_FIELD_W-1)];
  r += !object && layer == 1 ? light[0] * 3 / 4 : light[0];
  g += !object && layer == 1 ? light[1] * 3 / 4 : light[1];
  b += !object && layer == 1 ? light[2] * 3 / 4 : light[2];
  q->r = Clamp(r, 0, 255); q->g = Clamp(g, 0, 255); q->b = Clamp(b, 0, 255);
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
