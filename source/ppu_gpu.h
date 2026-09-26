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
bool PpuGpuVisibleWorldSpan(int *left, int *right);
const char *PpuGpuReason(void);
