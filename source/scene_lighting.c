#include "scene_lighting.h"
#include <string.h>

static int Clamp(int x, int low, int high) {
  return x < low ? low : x > high ? high : x;
}

void SceneLighting_AddLight(SceneLighting *s, int x, int y, int radius,
                            unsigned r, unsigned g, unsigned b) {
  if (!s->enabled || s->count == SCENE_MAX_LIGHTS || radius <= 0) return;
  s->lights[s->count++] = (SceneLight){x,y,radius,r,g,b};
}

void SceneLighting_Begin(SceneLighting *s, bool enabled, unsigned room,
                         unsigned state, int cameraX, int cameraY,
                         int samusX, int samusY, unsigned origin,
                         unsigned charge) {
  s->count = s->shadowCount = 0;
  s->castShadow = false;
  s->enabled = enabled && room == 0x91f8 && state == 8;
  if (!s->enabled) return;
  s->samusX = samusX - cameraX + (int)origin;
  s->samusY = samusY - cameraY;
  /* Ship hatch and two side lamps, fixed to room coordinates. */
  SceneLighting_AddLight(s, 1152 - cameraX + (int)origin, 1120 - cameraY,
                          112, 106, 78, 34);
  SceneLighting_AddLight(s, 1112 - cameraX + (int)origin, 1156 - cameraY,
                          72, 74, 52, 20);
  SceneLighting_AddLight(s, 1192 - cameraX + (int)origin, 1156 - cameraY,
                          72, 74, 52, 20);
  SceneLighting_AddLight(s, s->samusX, s->samusY - 14,
                          56, 26, 42, 38);
  if (charge >= 16)
    SceneLighting_AddLight(s, s->samusX + 10, s->samusY - 8,
                            48 + Clamp(charge, 0, 60), 88, 100, 62);
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
    /* Offset away from the ship's hatch. Keep the sprite's alpha silhouette.
     * Shadows sit between BG2-low and BG1-low, so foreground occludes them. */
    int dx = s->samusX <= s->lights[0].x ? -10 : 10;
    shadow.x0 += dx; shadow.x1 += dx;
    shadow.y0 += 9; shadow.y1 += 9;
    shadow.depth = 0x7200;
    shadow.r = shadow.g = shadow.b = 0;
    shadow.a = 78;
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
  int r = object ? 190 : layer == 0 ? 158 : 100;
  int g = object ? 198 : layer == 0 ? 176 : 126;
  int b = object ? 210 : layer == 0 ? 202 : 161;
  /* A small luminance gradient gives tile surfaces directional relief.
   * Sample the actual flipped/cropped texture rather than screen tile edges.
   * This is an approximation, not an authored per-pixel normal map. */
  int tx = ((q->u0 + q->u1) / 8) & 7;
  int ty = ((8192 - q->v0 - q->v1) / 16) & 7;
  int nx = (int)TexelLuma(f, slot, tx - 1, ty) -
           (int)TexelLuma(f, slot, tx + 1, ty);
  int ny = (int)TexelLuma(f, slot, tx, ty - 1) -
           (int)TexelLuma(f, slot, tx, ty + 1);
  if (q->u1 < q->u0) nx = -nx;
  if (q->v1 > q->v0) ny = -ny;
  int relief = Clamp((-nx - ny) / 10, -14, 14);
  r += relief; g += relief; b += relief;
  for (unsigned i = 0; i < s->count; i++) {
    const SceneLight *l = &s->lights[i];
    int dx = x - l->x, dy = y - l->y;
    int radius2 = l->radius * l->radius;
    if (dx < -l->radius || dx > l->radius ||
        dy < -l->radius || dy > l->radius) continue;
    int distance2 = dx * dx + dy * dy;
    if (distance2 >= radius2) continue;
    int weight = (radius2 - distance2) * 256 / radius2;
    weight = weight * weight / 256;
    if (!object && layer == 1) weight = weight * 3 / 4;
    r += l->r * weight / 256;
    g += l->g * weight / 256;
    b += l->b * weight / 256;
  }
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
