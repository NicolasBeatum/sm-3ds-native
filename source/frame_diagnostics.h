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
/* Audio queue publication is inside game time, outside native logic/PPU.
 * Lock wait is a subset of queue time, not audio generation CPU time. */
void FrameDiagnostics_RecordRuntime(bool profiled, uint32_t apu_queue_ticks,
                                    uint32_t apu_wait_ticks);
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
/* CPU wall spans are nested in existing PPU/top timings, not additional
 * frame work. Model build includes tile shading and silhouette creation. */
void FrameDiagnostics_RecordLighting(bool requested, bool applied, bool profiled,
                                     uint32_t prepare_ticks,
                                     uint32_t shadow_submit_ticks,
                                     uint32_t glow_submit_ticks,
                                     uint32_t model_build_ticks,
                                     unsigned sources, unsigned shadow_quads,
                                     unsigned glow_quads, unsigned background_sources,
                                     bool field_rebuilt);

/* Writes session/recent totals and recent individual samples at dump time. */
bool FrameDiagnostics_WriteSummary(FILE *out);
bool FrameDiagnostics_WriteCsv(FILE *out);
