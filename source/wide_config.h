#pragma once

#include <stdbool.h>

enum {
  kSnesWidth = 256,
  kSnesHeight = 224,
  kWideExtraX = 72,
  kWideWidth = kSnesWidth + 2 * kWideExtraX,
  /* IRQ at SNES line 31 switches to gameplay before line 32 is drawn.
   * Visible rows are indexed from zero: HUD [0,31), playfield [31,224). */
  kHudEndLine = 31,
};

typedef struct WideConfig {
  bool enabled;
  unsigned output_width;
  unsigned origin_x;
  unsigned hud_end_y;
} WideConfig;

static inline WideConfig WideConfig_Create(bool enabled) {
  WideConfig config = {
      .enabled = enabled,
      .output_width = enabled ? kWideWidth : kSnesWidth,
      .origin_x = enabled ? kWideExtraX : 0,
      .hud_end_y = kHudEndLine,
  };
  return config;
}
