#pragma once

#include <stdint.h>

typedef struct WideWorldSpan {
  int left;
  int right;
} WideWorldSpan;

WideWorldSpan WideBounds_Compute(int camera_x, int camera_y,
                                 unsigned room_width_blocks,
                                 unsigned room_width_scrolls,
                                 unsigned room_height_scrolls,
                                 const uint8_t *scrolls);
