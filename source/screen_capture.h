#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct ScreenCaptureStatus {
  bool imported, top_bmp, bottom_bmp, top_raw, bottom_raw;
  uint32_t top_format, bottom_format, top_stride, bottom_stride;
} ScreenCaptureStatus;

/* Capture the LCD buffers already displayed when a debug dump is requested. */
ScreenCaptureStatus ScreenCapture_Save(const char *directory);
