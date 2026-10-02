#pragma once
#include "ppu_gpu_model.h"

/* Render-only extension. Native WRAM, VRAM and gameplay state are retained. */
void WideXray_Prepare(PicaFrame *frame);
