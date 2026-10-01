#pragma once
#include "bottom_screen.h"

typedef struct TopVideoLayout {
  int x, y, width, height;
  int source_x, source_width;
} TopVideoLayout;

TopVideoLayout TopVideoLayout_Get(enum BottomScreenVideoMode mode, bool wide);
/* Stay off exact nearest-sampler boundaries after fractional enlargement.
 * 1/64 texel is below a visible pixel and above interpolation roundoff. */
#define TOP_SAMPLE_BIAS (1.0f / 64.0f)
