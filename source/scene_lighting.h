#pragma once
#include "ppu_gpu_model.h"

enum { SCENE_MAX_LIGHTS = 8, SCENE_MAX_SHADOWS = 128 };
typedef struct SceneLight {
  int x, y, radius;
  uint8_t r, g, b;
} SceneLight;
typedef struct SceneLighting {
  bool enabled, castShadow;
  int samusX, samusY;
  unsigned count, shadowCount;
  SceneLight lights[SCENE_MAX_LIGHTS];
  PicaQuad shadows[SCENE_MAX_SHADOWS];
} SceneLighting;

/* The first trial is deliberately confined to Landing Site during gameplay.
 * Coordinates are in the PPU viewport, including the widescreen origin. */
void SceneLighting_Begin(SceneLighting *scene, bool enabled, unsigned room,
                         unsigned state, int cameraX, int cameraY,
                         int samusX, int samusY, unsigned origin,
                         unsigned charge);
void SceneLighting_AddLight(SceneLighting *scene, int x, int y, int radius,
                            unsigned r, unsigned g, unsigned b);
void SceneLighting_Tile(PicaFrame *frame, unsigned group, unsigned slot,
                        PicaQuad *quad);
bool SceneLighting_EmitShadows(PicaFrame *frame, unsigned sub);
