#include "frame_diagnostics.h"

#include <inttypes.h>

enum { kRecentFrameCount = 120, kFrameBudgetUs = 16667 };

typedef struct FrameSample {
  uint32_t frame, interval_ticks, game_ticks, top_ticks, bottom_ticks;
  uint32_t present_ticks, work_ticks;
  bool wide, pica_gpu, presenter;
} FrameSample;

static FrameSample g_samples[kRecentFrameCount];
static unsigned g_next, g_count;
static uint64_t g_ticks_per_second;

void FrameDiagnostics_Init(uint64_t ticks_per_second) {
  g_ticks_per_second = ticks_per_second;
  g_next = g_count = 0;
}

static uint64_t AsUs(uint64_t ticks) {
  return g_ticks_per_second ? ticks * 1000000 / g_ticks_per_second : 0;
}

void FrameDiagnostics_Record(uint32_t frame, uint32_t interval_ticks,
                             uint32_t game_ticks, uint32_t top_ticks,
                             uint32_t bottom_ticks, uint32_t present_ticks,
                             bool wide, bool pica_gpu, bool presenter) {
  FrameSample *sample = &g_samples[g_next];
  *sample = (FrameSample){
      .frame = frame,
      .interval_ticks = interval_ticks,
      .game_ticks = game_ticks,
      .top_ticks = top_ticks,
      .bottom_ticks = bottom_ticks,
      .present_ticks = present_ticks,
      .work_ticks = game_ticks + top_ticks + bottom_ticks + present_ticks,
      .wide = wide,
      .pica_gpu = pica_gpu,
      .presenter = presenter,
  };
  g_next = (g_next + 1) % kRecentFrameCount;
  if (g_count < kRecentFrameCount) g_count++;
}

static const FrameSample *SampleAt(unsigned index) {
  unsigned first = (g_next + kRecentFrameCount - g_count) % kRecentFrameCount;
  return &g_samples[(first + index) % kRecentFrameCount];
}

bool FrameDiagnostics_WriteSummary(FILE *out) {
  if (!out) return false;
  uint64_t interval_sum = 0, game_sum = 0, top_sum = 0;
  uint64_t bottom_sum = 0, present_sum = 0, work_sum = 0;
  unsigned intervals = 0, interval_over = 0, work_over = 0;
  unsigned wide = 0, gpu = 0, presenter = 0;
  uint32_t max_interval = 0, max_work = 0;
  for (unsigned i = 0; i < g_count; i++) {
    const FrameSample *s = SampleAt(i);
    if (s->interval_ticks) {
      interval_sum += s->interval_ticks;
      intervals++;
      if (AsUs(s->interval_ticks) > kFrameBudgetUs) interval_over++;
      if (s->interval_ticks > max_interval) max_interval = s->interval_ticks;
    }
    game_sum += s->game_ticks;
    top_sum += s->top_ticks;
    bottom_sum += s->bottom_ticks;
    present_sum += s->present_ticks;
    work_sum += s->work_ticks;
    if (AsUs(s->work_ticks) > kFrameBudgetUs) work_over++;
    if (s->work_ticks > max_work) max_work = s->work_ticks;
    wide += s->wide;
    gpu += s->pica_gpu;
    presenter += s->presenter;
  }
  unsigned count = g_count ? g_count : 1;
  uint64_t interval_us = AsUs(interval_sum);
  uint64_t fps_x100 = interval_us ? (uint64_t)intervals * 100000000 / interval_us : 0;
  int result = fprintf(out,
      "timing_schema=1\n"
      "recent_frames=%u\nvalid_intervals=%u\n"
      "measured_fps=%" PRIu64 ".%02" PRIu64 "\n"
      "avg_interval_us=%" PRIu64 "\nmax_interval_us=%" PRIu64 "\n"
      "intervals_over_16667us=%u\n"
      "avg_game_us=%" PRIu64 "\navg_top_us=%" PRIu64 "\n"
      "avg_bottom_us=%" PRIu64 "\navg_present_us=%" PRIu64 "\n"
      "avg_work_us=%" PRIu64 "\nmax_work_us=%" PRIu64 "\n"
      "work_frames_over_16667us=%u\n"
      "recent_widescreen_frames=%u\nrecent_pica_gpu_frames=%u\n"
      "recent_presenter_frames=%u\n"
      "timing_note=phase durations are CPU wall spans; GPU work can finish asynchronously\n"
      "frame_times_file=frame-times.csv\n",
      g_count, intervals, fps_x100 / 100, fps_x100 % 100,
      intervals ? interval_us / intervals : 0, AsUs(max_interval), interval_over,
      AsUs(game_sum) / count, AsUs(top_sum) / count, AsUs(bottom_sum) / count,
      AsUs(present_sum) / count, AsUs(work_sum) / count, AsUs(max_work), work_over,
      wide, gpu, presenter);
  return result > 0 && !ferror(out);
}

bool FrameDiagnostics_WriteCsv(FILE *out) {
  if (!out) return false;
  if (fputs("frame,interval_us,game_us,top_us,bottom_us,present_us,work_us,"
            "widescreen,pica_gpu,presenter\n", out) < 0) return false;
  for (unsigned i = 0; i < g_count; i++) {
    const FrameSample *s = SampleAt(i);
    if (fprintf(out, "%" PRIu32 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64
                     ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%u,%u,%u\n",
                s->frame, AsUs(s->interval_ticks), AsUs(s->game_ticks),
                AsUs(s->top_ticks), AsUs(s->bottom_ticks),
                AsUs(s->present_ticks), AsUs(s->work_ticks),
                s->wide, s->pica_gpu, s->presenter) < 0) return false;
  }
  return !ferror(out);
}
