#pragma once

#include <stdbool.h>
#include <citro3d.h>

#include "src/snes/ppu.h"

bool PpuGpuInit(void);
void PpuGpuShutdown(void);
bool PpuGpuCanAttempt(void);
bool PpuGpuBegin(Ppu *ppu, unsigned height);
void PpuGpuLine(const Ppu *ppu, unsigned y);
bool PpuGpuFinish(Ppu *ppu);
void PpuGpuCpuFrame(void);
bool PpuGpuPrepared(void);
bool PpuGpuOutputActive(void);
bool PpuGpuDraw(void);
C3D_Tex *PpuGpuOutput(void);
const char *PpuGpuReason(void);

