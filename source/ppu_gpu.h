#pragma once

#include <stdbool.h>
#include <citro3d.h>

#include "src/snes/ppu.h"
#include "wide_config.h"

bool PpuGpuInit(void);
void PpuGpuShutdown(void);
bool PpuGpuCanAttempt(void);
void PpuGpuSetWideConfig(WideConfig config);
bool PpuGpuBegin(Ppu *ppu, unsigned height);
void PpuGpuLine(const Ppu *ppu, unsigned y);
bool PpuGpuFinish(Ppu *ppu);
void PpuGpuCpuFrame(void);
bool PpuGpuPrepared(void);
bool PpuGpuOutputActive(void);
bool PpuGpuDraw(void);
C3D_Tex *PpuGpuOutput(void);
unsigned PpuGpuOutputWidth(void);
unsigned PpuGpuHudLines(void);
const char *PpuGpuReason(void);
#ifdef SM3DS_PHASE_DIAG
typedef struct PpuGpuTiming {
  uint64_t bg_main, obj_main, bg_sub, obj_sub, compose, upload;
} PpuGpuTiming;
PpuGpuTiming PpuGpuGetTiming(void);
#endif
