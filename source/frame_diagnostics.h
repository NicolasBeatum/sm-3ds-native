#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

/* A small in-memory history. Recording never touches the SD card. */
void FrameDiagnostics_Init(uint64_t ticks_per_second);
void FrameDiagnostics_Record(uint32_t frame, uint32_t interval_ticks,
                             uint32_t game_ticks, uint32_t logic_ticks,
                             uint32_t ppu_ticks, uint32_t top_ticks,
                             uint32_t bottom_ticks, uint32_t present_ticks,
                             bool wide, bool pica_gpu, bool presenter,
                             bool phase_profile);

/* Writes the recent timing summary and individual samples at dump time. */
bool FrameDiagnostics_WriteSummary(FILE *out);
bool FrameDiagnostics_WriteCsv(FILE *out);
