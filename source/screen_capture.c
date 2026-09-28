#include "screen_capture.h"

#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool FilePath(char *path, size_t size, const char *directory,
                     const char *name) {
  int n = snprintf(path, size, "%s/%s", directory, name);
  return n > 0 && n < (int)size;
}

static bool PixelRgb(const uint8_t *src, GSPGPU_FramebufferFormat format,
                     uint8_t *red, uint8_t *green, uint8_t *blue) {
  switch (format) {
    case GSP_RGBA8_OES:
      *red = src[0]; *green = src[1]; *blue = src[2];
      return true;
    case GSP_BGR8_OES:
      *red = src[2]; *green = src[1]; *blue = src[0];
      return true;
    case GSP_RGB565_OES: {
      uint16_t color;
      memcpy(&color, src, sizeof(color));
      *red = ((color >> 11) & 31) * 255 / 31;
      *green = ((color >> 5) & 63) * 255 / 63;
      *blue = (color & 31) * 255 / 31;
      return true;
    }
    default:
      return false;
  }
}

static bool SaveBmp(const char *path, const GSPGPU_CaptureInfoEntry *capture,
                    unsigned width, unsigned height) {
  GSPGPU_FramebufferFormat format = capture->format & 7u;
  unsigned bpp = gspGetBytesPerPixel(format);
  if (!capture->framebuf0_vaddr || !capture->framebuf_widthbytesize ||
      bpp < 2 || bpp > 4 || capture->framebuf_widthbytesize < height * bpp)
    return false;
  size_t row_size = (width * 3 + 3) & ~3u;
  uint32_t file_size = 54 + row_size * height;
  uint8_t header[54] = {
      'B', 'M',
      file_size, file_size >> 8, file_size >> 16, file_size >> 24,
      0, 0, 0, 0, 54, 0, 0, 0,
      40, 0, 0, 0,
      width, width >> 8, width >> 16, width >> 24,
      height, height >> 8, height >> 16, height >> 24,
      1, 0, 24, 0,
  };
  uint8_t *row = malloc(row_size);
  if (!row) return false;
  FILE *f = fopen(path, "wb");
  if (!f) { free(row); return false; }
  const uint8_t *fb = (const uint8_t *)capture->framebuf0_vaddr;
  GSPGPU_InvalidateDataCache(fb, capture->framebuf_widthbytesize * width);
  bool okay = fwrite(header, 1, sizeof(header), f) == sizeof(header);
  for (int y = (int)height - 1; okay && y >= 0; y--) {
    memset(row, 0, row_size);
    for (unsigned x = 0; x < width; x++) {
      const uint8_t *pixel = fb + x * capture->framebuf_widthbytesize +
                             (height - 1 - y) * bpp;
      uint8_t red, green, blue;
      if (!PixelRgb(pixel, format, &red, &green, &blue)) {
        okay = false;
        break;
      }
      row[x * 3] = blue;
      row[x * 3 + 1] = green;
      row[x * 3 + 2] = red;
    }
    if (okay) okay = fwrite(row, 1, row_size, f) == row_size;
  }
  free(row);
  if (fclose(f) != 0) okay = false;
  if (!okay) remove(path);
  return okay;
}

static bool SaveRaw(const char *path, const GSPGPU_CaptureInfoEntry *capture,
                    unsigned width) {
  if (!capture->framebuf0_vaddr || !capture->framebuf_widthbytesize)
    return false;
  size_t size = capture->framebuf_widthbytesize * width;
  GSPGPU_InvalidateDataCache(capture->framebuf0_vaddr, size);
  FILE *f = fopen(path, "wb");
  if (!f) return false;
  bool okay = fwrite(capture->framebuf0_vaddr, 1, size, f) == size;
  if (fclose(f) != 0) okay = false;
  if (!okay) remove(path);
  return okay;
}

ScreenCaptureStatus ScreenCapture_Save(const char *directory) {
  ScreenCaptureStatus status = {0};
  GSPGPU_CaptureInfo capture = {0};
  if (R_FAILED(GSPGPU_ImportDisplayCaptureInfo(&capture))) return status;
  status.imported = true;
  const GSPGPU_CaptureInfoEntry *top = &capture.screencapture[GSP_SCREEN_TOP];
  const GSPGPU_CaptureInfoEntry *bottom = &capture.screencapture[GSP_SCREEN_BOTTOM];
  status.top_format = top->format & 7u;
  status.bottom_format = bottom->format & 7u;
  status.top_stride = top->framebuf_widthbytesize;
  status.bottom_stride = bottom->framebuf_widthbytesize;
  char path[256];
  if (FilePath(path, sizeof(path), directory, "top.bmp"))
    status.top_bmp = SaveBmp(path, top, 400, 240);
  if (FilePath(path, sizeof(path), directory, "bottom.bmp"))
    status.bottom_bmp = SaveBmp(path, bottom, 320, 240);
  if (FilePath(path, sizeof(path), directory, "top.raw"))
    status.top_raw = SaveRaw(path, top, 400);
  if (FilePath(path, sizeof(path), directory, "bottom.raw"))
    status.bottom_raw = SaveRaw(path, bottom, 320);
  return status;
}
