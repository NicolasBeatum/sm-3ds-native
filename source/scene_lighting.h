#pragma once
#include "ppu_gpu_model.h"

enum { SCENE_MAX_LIGHTS = 8, SCENE_MAX_SHADOWS = 128,
       SCENE_FIELD_W = 50, SCENE_FIELD_H = (PICA_MAX_LINES + 7) / 8 };
typedef struct SceneLight {
  int x, y, radius;
  uint8_t r, g, b;
} SceneLight;
typedef struct SceneRelief {
  uint32_t key;
  int8_t shade[4];
} SceneRelief;
typedef struct SceneEmission {
  uint32_t key;
  uint8_t r, g, b;
} SceneEmission;
typedef struct SceneLighting {
  bool enabled, castShadow;
  int samusX, samusY;
  unsigned count, shadowCount;
  SceneLight lights[SCENE_MAX_LIGHTS];
  PicaQuad shadows[SCENE_MAX_SHADOWS];
  bool ship;
  uint16_t field[SCENE_FIELD_H][SCENE_FIELD_W][3];
  SceneRelief relief[512];
  unsigned liquidType;
  int liquidY;
  SceneEmission emission[256];
} SceneLighting;

/* Gameplay lighting in every room. Coordinates include the widescreen origin.
 * Fixed ship lamps are added only in Landing Site. */
void SceneLighting_Begin(SceneLighting *scene, bool enabled, unsigned room,
                         unsigned state, int cameraX, int cameraY,
                         int samusX, int samusY, unsigned origin,
                         unsigned charge);
void SceneLighting_AddLight(SceneLighting *scene, int x, int y, int radius,
                            unsigned r, unsigned g, unsigned b);
void SceneLighting_Prepare(PicaFrame *frame);
/* Liquid Y is screen-relative; room blocks identify the live door caps. */
void SceneLighting_AddRoomSources(PicaFrame *frame);
/* Approximate small luminous cores, excluding broad bright wall surfaces. */
SceneEmission SceneLighting_Emission(PicaFrame *frame, unsigned slot);
void SceneLighting_Tile(PicaFrame *frame, unsigned group, unsigned slot,
                        PicaQuad *quad);
bool SceneLighting_EmitShadows(PicaFrame *frame, unsigned sub);
