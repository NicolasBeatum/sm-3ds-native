#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "hud_mode.h"

enum BottomScreenVideoMode {
  kVideoMode_Fit,
  kVideoMode_Stretched,
  kVideoMode_OneToOne,
};

bool BottomScreen_Init(void);
bool BottomScreen_Draw(void);
const uint8_t *BottomScreen_Pixels(void);
void BottomScreen_CopyToFramebuffer(void);
void BottomScreen_Fini(void);
void BottomScreen_HandleTouch(float normalized_x, float normalized_y);
void BottomScreen_HandleTouchMotion(float normalized_x, float normalized_y);
void BottomScreen_HandleTouchUp(float normalized_x, float normalized_y);
bool BottomScreen_ConsumeDumpRequest(void);
bool BottomScreen_HideMainHud(void);
enum MainHudMode BottomScreen_MainHudMode(void);
bool BottomScreen_WidescreenEnabled(void);
enum BottomScreenVideoMode BottomScreen_VideoMode(void);
void BottomScreen_LoadSettings(const char *path);
bool BottomScreen_SaveSettings(void);
void BottomScreen_DrawRomSelector(const char *const *rows, int row_count,
                                  int selected, int total, const char *message);
void BottomScreen_DrawRomSelectorTop(void);
void BottomScreen_ShowNotice(const char *message, uint64_t until_ms);

bool BottomScreen_SceneLightingEnabled(void);
enum ItemSymbolMode { kItemSymbolsOff, kItemSymbolsZeroFusion, kItemSymbolsSuperMetroid };
enum ItemSymbolMode BottomScreen_ItemSymbolMode(void);

/* Localizes the companion UI; the ROM controls the game text language. */
const char *BottomScreen_Localize(const char *english, const char *spanish);

bool BottomScreen_MapStationsEnabled(void);
bool BottomScreen_MinimapItemsEnabled(void);
