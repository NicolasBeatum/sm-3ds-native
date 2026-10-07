#pragma once

#include <stdbool.h>
#include <stdint.h>

bool GpuPresenter_Init(void);
bool GpuPresenter_DrawTop(const uint8_t *pixels);
bool GpuPresenter_DrawBottom(const uint8_t *pixels, bool upload);
void GpuPresenter_EndFrame(void);
void GpuPresenter_Fini(void);
#ifdef SM3DS_PHASE_DIAG
typedef struct GpuGlowTiming {
  uint64_t submit_ticks;
  unsigned quads;
} GpuGlowTiming;
GpuGlowTiming GpuPresenter_GetGlowTiming(void);
#endif
