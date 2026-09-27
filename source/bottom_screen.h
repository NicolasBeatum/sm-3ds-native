#pragma once

#include <stdbool.h>
#include <stdint.h>

bool BottomScreen_Init(void);
bool BottomScreen_Draw(void);
const uint8_t *BottomScreen_Pixels(void);
void BottomScreen_CopyToFramebuffer(void);
void BottomScreen_Fini(void);
void BottomScreen_HandleTouch(float normalized_x, float normalized_y);
void BottomScreen_HandleTouchUp(float normalized_x, float normalized_y);
bool BottomScreen_HideMainHud(void);
bool BottomScreen_WidescreenEnabled(void);
void BottomScreen_LoadSettings(const char *path);
bool BottomScreen_SaveSettings(void);
void BottomScreen_DrawRomSelector(const char *const *rows, int row_count,
                                  int selected, int total, bool native_engine,
                                  const char *message);
void BottomScreen_ShowNotice(const char *message, uint64_t until_ms);
