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
                             bool phase_profile, bool gameplay);
/* Attach PICA preparation details to the frame most recently recorded. */
void FrameDiagnostics_RecordPpuDetail(uint32_t bg_main_ticks,
                                      uint32_t obj_main_ticks,
                                      uint32_t bg_sub_ticks,
                                      uint32_t obj_sub_ticks,
                                      uint32_t compose_ticks,
                                      uint32_t upload_ticks);
void FrameDiagnostics_RecordScene(uint16_t state_value, uint16_t room,
                                  uint16_t map_x, uint16_t map_y,
                                  uint16_t scroll_x, uint16_t scroll_y,
                                  bool bottom_redrawn);

/* Writes session/recent totals and recent individual samples at dump time. */
bool FrameDiagnostics_WriteSummary(FILE *out);
bool FrameDiagnostics_WriteCsv(FILE *out);
