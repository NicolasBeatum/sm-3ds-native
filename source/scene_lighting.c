#include "scene_lighting.h"
#include <string.h>

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
  s->count = s->shadowCount = 0;
  s->castShadow = false;
  s->enabled = enabled && state == 8;
  s->ship = room == 0x91f8;
  if (!s->enabled) return;
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
                          56, 26, 42, 38);
  if (charge >= 16)
    SceneLighting_AddLight(s, s->samusX + 10, s->samusY - 8,
                            48 + Clamp(charge, 0, 60), 88, 100, 62);
}

void SceneLighting_Prepare(PicaFrame *f) {
  SceneLighting *s = f->lighting;
  memset(s->field, 0, sizeof(s->field));
  memset(s->relief, 0, sizeof(s->relief));
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
    /* Offset away from the ship's hatch, or use a fixed relief elsewhere.
     * Shadows sit between BG2-low and BG1-low, so foreground occludes them. */
    int dx = s->ship && s->samusX <= s->lights[0].x ? -10 : 10;
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
  /* Other rooms keep their palette hue and more ambient light. This avoids
   * tinting hot lava rooms blue or making already-dark rooms unreadable. */
  int r = s->ship ? (object ? 190 : layer == 0 ? 158 : 100) :
                   (object ? 226 : layer == 0 ? 218 : 184);
  int g = s->ship ? (object ? 198 : layer == 0 ? 176 : 126) : r;
  int b = s->ship ? (object ? 210 : layer == 0 ? 202 : 161) : r;
  /* A small luminance gradient gives tile surfaces directional relief.
   * Sample the actual flipped/cropped texture rather than screen tile edges.
   * This is an approximation, not an authored per-pixel normal map. */
  int tx = ((q->u0 + q->u1) / 8) & 7;
  int ty = ((8192 - q->v0 - q->v1) / 16) & 7;
  /* A decoded atlas slot is immutable for this frame. Reuse its relief for
   * repeated tiles and the second SNES screen; cropped centers are keys too. */
  uint32_t key = (slot | (tx << 13) | (ty << 16)) + 1;
  SceneRelief *cached = &s->relief[(key * 2654435761u) >> 23];
  if (cached->key != key) {
    int nx = (int)TexelLuma(f, slot, tx - 1, ty) -
             (int)TexelLuma(f, slot, tx + 1, ty);
    int ny = (int)TexelLuma(f, slot, tx, ty - 1) -
             (int)TexelLuma(f, slot, tx, ty + 1);
    cached->key = key;
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
