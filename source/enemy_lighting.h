#pragma once
#include "scene_lighting.h"

struct EnemyData;
/* Read-only presentation of at most 32 native enemy slots. */
void SceneLighting_AddEnemies(PicaFrame *frame, const struct EnemyData *enemies,
                             unsigned count, int cameraX, int cameraY);
