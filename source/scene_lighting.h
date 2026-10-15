#pragma once
#include "ppu_gpu_model.h"

enum { SCENE_MAX_LIGHTS = 8, SCENE_MAX_SHADOWS = 128,
       SCENE_MAX_SCENERY_LIGHTS = 4, SCENE_EMITTER_CACHE = 64,
       SCENE_FIELD_SHIFT = 4, SCENE_FIELD_CELL = 1 << SCENE_FIELD_SHIFT,
       SCENE_FIELD_W = (400 + SCENE_FIELD_CELL - 1) / SCENE_FIELD_CELL,
       SCENE_FIELD_H = (PICA_MAX_LINES + SCENE_FIELD_CELL - 1) / SCENE_FIELD_CELL };
typedef struct SceneLight {
  int x, y, radius;
  uint8_t r, g, b;
  bool glow; /* Uses existing tail padding; diffuse-only scenery costs no halo. */
} SceneLight;
typedef struct SceneEmitter {
  uint16_t x, y, tile;
  uint8_t motif;
} SceneEmitter;
typedef struct SceneEmitterCache {
  uint32_t mapHash;
  uint16_t tileAdr, mapAdr;
  uint8_t wider, higher, mask, count;
  bool valid, overflow;
  SceneEmitter entries[SCENE_EMITTER_CACHE];
} SceneEmitterCache;
/* Zero-initialized key has no dependence on atlas buffer addresses. */
typedef struct SceneFieldKey {
  int lights[SCENE_MAX_LIGHTS][6];
  unsigned count, width, height, hudEndY;
  uint8_t base[3][3];
  /* Visible collision cells plus a one-block border. Changes to doors and
   * destructible blocks invalidate contact shading without texture probes. */
  uint8_t occupied[SCENE_FIELD_H + 2][SCENE_FIELD_W + 2];
} SceneFieldKey;
/* Initialize to zero once; colors are rebuilt only when this key changes. */
typedef struct SceneLighting {
  bool enabled, castShadow, ship, fieldValid;
  bool transition;
  uint8_t glowAlpha;
  int samusX, samusY;
  unsigned count, shadowCount, environmentStart, room, backgroundCount;
  SceneLight lights[SCENE_MAX_LIGHTS];
  /* Native tilemap coordinates survive camera/parallax and atlas swaps.
   * Live palette and foreground visibility are still checked every frame. */
  SceneEmitterCache emitters[2];
  PicaQuad shadows[SCENE_MAX_SHADOWS];
  int shadowX, shadowY;
  unsigned liquidType;
  int liquidY;
  uint8_t ambient[2][3];
  /* Finished BG1/BG2/OBJ modulation colors, shared by both SNES screens. */
  uint8_t color[3][SCENE_FIELD_H][SCENE_FIELD_W][3];
  SceneFieldKey fieldKey;
} SceneLighting;

/* The model has already clipped these quads to the viewport. Backgrounds use
 * this lookup directly; only objects need the separate shadow routine. */
static inline void SceneLighting_ApplyColor(const SceneLighting *s,
                                            unsigned layer, PicaQuad *q) {
  unsigned x=(q->x0+q->x1)>>(SCENE_FIELD_SHIFT+1);
  unsigned y=(q->y0+q->y1)>>(SCENE_FIELD_SHIFT+1);
  const uint8_t *color=s->color[layer][y][x];
  q->r=color[0]; q->g=color[1]; q->b=color[2];
}

/* Gameplay lighting in every room. Coordinates include the widescreen origin.
 * Fixed ship lamps are added only in Landing Site. */
void SceneLighting_Begin(SceneLighting *scene, bool enabled, unsigned room,
                         unsigned state, int cameraX, int cameraY,
                         int samusX, int samusY, unsigned origin,
                         unsigned charge);
void SceneLighting_AddLight(SceneLighting *scene, int x, int y, int radius,
                            unsigned r, unsigned g, unsigned b);
void SceneLighting_Prepare(PicaFrame *frame);
/* Diffuse room color from sampled tile pixels, excluding unused palette colors. */
void SceneLighting_Ambient(PicaFrame *frame);
/* Recognized BG1/BG2 emitters: live graphics, palette and layer scroll. */
void SceneLighting_AddBackgroundSources(PicaFrame *frame);
/* Liquid Y is screen-relative; room blocks identify the live door caps. */
void SceneLighting_AddRoomSources(PicaFrame *frame);
void SceneLighting_Tile(PicaFrame *frame, unsigned group, unsigned slot,
                        PicaQuad *quad);
bool SceneLighting_EmitShadows(PicaFrame *frame, unsigned sub);
