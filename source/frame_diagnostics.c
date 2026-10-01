#include "frame_diagnostics.h"

#include <inttypes.h>
#include <string.h>

enum { kRecentFrameCount = 120, kFrameBudgetUs = 16667 };

typedef struct FrameSample {
  uint32_t frame, interval_ticks, game_ticks, logic_ticks, ppu_ticks;
  uint32_t top_ticks, bottom_ticks;
  uint32_t present_ticks, work_ticks;
  uint32_t bg_main_ticks, obj_main_ticks, bg_sub_ticks, obj_sub_ticks;
  uint32_t compose_ticks, upload_ticks;
  bool wide, pica_gpu, presenter, phase_profile, ppu_detail, gameplay;
  uint16_t game_state, room, map_x, map_y, scroll_x, scroll_y;
  bool sector_changed, scroll_changed, bottom_redrawn;
} FrameSample;

static FrameSample g_samples[kRecentFrameCount];
static unsigned g_next, g_count;
static uint64_t g_ticks_per_second;

/* Like the reference port, keep whole-session aggregates as well as the
 * recent ring. This uses fixed memory and never writes to SD during play. */
typedef struct SessionTiming {
  uint64_t frames, intervals, interval_ticks, work_ticks;
  uint64_t game_ticks, logic_ticks, ppu_ticks, top_ticks, bottom_ticks, present_ticks;
  uint64_t phase_frames, detail_frames, detail_ticks[6];
  uint64_t over_budget, gpu_frames;
  uint32_t max_interval, max_work;
} SessionTiming;
static SessionTiming g_session[3]; /* all, gameplay standard, gameplay wide */
static SessionTiming g_sector_changes, g_scroll_changes, g_bottom_redraws;
static bool g_scene_valid;
static uint16_t g_scene[5];

void FrameDiagnostics_Init(uint64_t ticks_per_second) {
  g_ticks_per_second = ticks_per_second;
  g_next = g_count = 0;
  memset(g_session, 0, sizeof(g_session));
  memset(&g_sector_changes, 0, sizeof(g_sector_changes));
  memset(&g_scroll_changes, 0, sizeof(g_scroll_changes));
  memset(&g_bottom_redraws, 0, sizeof(g_bottom_redraws));
  g_scene_valid = false;
}

static uint64_t AsUs(uint64_t ticks) {
  return g_ticks_per_second ? ticks * 1000000 / g_ticks_per_second : 0;
}

static void RecordSession(SessionTiming *t, const FrameSample *s) {
  t->frames++;
  if (s->interval_ticks) {
    t->intervals++;
    t->interval_ticks += s->interval_ticks;
    if (s->interval_ticks > t->max_interval) t->max_interval = s->interval_ticks;
  }
  t->game_ticks += s->game_ticks;
  t->top_ticks += s->top_ticks;
  t->bottom_ticks += s->bottom_ticks;
  t->present_ticks += s->present_ticks;
  t->work_ticks += s->work_ticks;
  t->gpu_frames += s->pica_gpu;
  if (s->work_ticks > t->max_work) t->max_work = s->work_ticks;
  if (AsUs(s->work_ticks) > kFrameBudgetUs) t->over_budget++;
  if (s->phase_profile) {
    t->phase_frames++;
    t->logic_ticks += s->logic_ticks;
    t->ppu_ticks += s->ppu_ticks;
  }
}

void FrameDiagnostics_Record(uint32_t frame, uint32_t interval_ticks,
                             uint32_t game_ticks, uint32_t logic_ticks,
                             uint32_t ppu_ticks, uint32_t top_ticks,
                             uint32_t bottom_ticks, uint32_t present_ticks,
                             bool wide, bool pica_gpu, bool presenter,
                             bool phase_profile, bool gameplay) {
  FrameSample *sample = &g_samples[g_next];
  *sample = (FrameSample){
      .frame = frame,
      .interval_ticks = interval_ticks,
      .game_ticks = game_ticks,
      .logic_ticks = logic_ticks,
      .ppu_ticks = ppu_ticks,
      .top_ticks = top_ticks,
      .bottom_ticks = bottom_ticks,
      .present_ticks = present_ticks,
      .work_ticks = game_ticks + top_ticks + bottom_ticks + present_ticks,
      .wide = wide,
      .pica_gpu = pica_gpu,
      .presenter = presenter,
      .phase_profile = phase_profile,
      .gameplay = gameplay,
  };
  RecordSession(&g_session[0], sample);
  if (gameplay) RecordSession(&g_session[wide ? 2 : 1], sample);
  g_next = (g_next + 1) % kRecentFrameCount;
  if (g_count < kRecentFrameCount) g_count++;
}

void FrameDiagnostics_RecordPpuDetail(uint32_t bg_main_ticks,
                                      uint32_t obj_main_ticks,
                                      uint32_t bg_sub_ticks,
                                      uint32_t obj_sub_ticks,
                                      uint32_t compose_ticks,
                                      uint32_t upload_ticks) {
  if (!g_count) return;
  FrameSample *sample = &g_samples[(g_next + kRecentFrameCount - 1) %
                                    kRecentFrameCount];
  sample->bg_main_ticks = bg_main_ticks;
  sample->obj_main_ticks = obj_main_ticks;
  sample->bg_sub_ticks = bg_sub_ticks;
  sample->obj_sub_ticks = obj_sub_ticks;
  sample->compose_ticks = compose_ticks;
  sample->upload_ticks = upload_ticks;
  sample->ppu_detail = true;
  uint32_t details[] = {bg_main_ticks, obj_main_ticks, bg_sub_ticks,
                        obj_sub_ticks, compose_ticks, upload_ticks};
  for (unsigned i = 0; i < 3; i++) {
    if (i && (!sample->gameplay || i != (sample->wide ? 2 : 1))) continue;
    g_session[i].detail_frames++;
    for (unsigned j = 0; j < 6; j++) g_session[i].detail_ticks[j] += details[j];
  }
}

void FrameDiagnostics_RecordScene(uint16_t state_value, uint16_t room,
                                  uint16_t map_x, uint16_t map_y,
                                  uint16_t scroll_x, uint16_t scroll_y,
                                  bool bottom_redrawn) {
  if (!g_count) return;
  FrameSample *s = &g_samples[(g_next + kRecentFrameCount - 1) % kRecentFrameCount];
  s->game_state = state_value; s->room = room;
  s->map_x = map_x; s->map_y = map_y;
  s->scroll_x = scroll_x; s->scroll_y = scroll_y;
  s->bottom_redrawn = bottom_redrawn;
  if (s->gameplay && g_scene_valid) {
    s->sector_changed = room != g_scene[0] || map_x != g_scene[1] || map_y != g_scene[2];
    s->scroll_changed = room != g_scene[0] || scroll_x != g_scene[3] || scroll_y != g_scene[4];
  }
  if (s->sector_changed) RecordSession(&g_sector_changes, s);
  if (s->scroll_changed) RecordSession(&g_scroll_changes, s);
  if (bottom_redrawn && s->gameplay) RecordSession(&g_bottom_redraws, s);
  uint16_t scene[] = {room, map_x, map_y, scroll_x, scroll_y};
  memcpy(g_scene, scene, sizeof(scene));
  g_scene_valid = s->gameplay;
}

static bool WriteEvent(FILE *out, const char *name, const SessionTiming *t) {
  uint64_t count = t->frames ? t->frames : 1;
  return fprintf(out, "%s_frames=%" PRIu64 "\n%s_avg_work_us=%" PRIu64
      "\n%s_max_work_us=%" PRIu64 "\n%s_avg_game_us=%" PRIu64
      "\n%s_avg_bottom_us=%" PRIu64 "\n",
      name, t->frames, name, AsUs(t->work_ticks) / count,
      name, AsUs(t->max_work), name, AsUs(t->game_ticks) / count,
      name, AsUs(t->bottom_ticks) / count) > 0;
}

static bool WriteSession(FILE *out, const char *prefix, const SessionTiming *t) {
  uint64_t count = t->frames ? t->frames : 1;
  uint64_t interval_us = AsUs(t->interval_ticks);
  uint64_t fps = interval_us ? t->intervals * 100000000 / interval_us : 0;
  int result = fprintf(out,
      "%s_frames=%" PRIu64 "\n%s_valid_intervals=%" PRIu64 "\n"
      "%s_measured_fps=%" PRIu64 ".%02" PRIu64 "\n"
      "%s_duration_us=%" PRIu64 "\n%s_max_interval_us=%" PRIu64 "\n"
      "%s_avg_work_us=%" PRIu64 "\n%s_max_work_us=%" PRIu64 "\n"
      "%s_work_frames_over_16667us=%" PRIu64 "\n%s_pica_gpu_frames=%" PRIu64 "\n"
      "%s_avg_game_us=%" PRIu64 "\n%s_avg_top_us=%" PRIu64 "\n"
      "%s_avg_bottom_us=%" PRIu64 "\n%s_avg_present_us=%" PRIu64 "\n"
      "%s_phase_profile_frames=%" PRIu64 "\n%s_avg_logic_us=%" PRIu64 "\n"
      "%s_avg_ppu_us=%" PRIu64 "\n%s_ppu_detail_frames=%" PRIu64 "\n",
      prefix, t->frames, prefix, t->intervals, prefix, fps / 100, fps % 100,
      prefix, interval_us, prefix, AsUs(t->max_interval),
      prefix, AsUs(t->work_ticks) / count, prefix, AsUs(t->max_work),
      prefix, t->over_budget, prefix, t->gpu_frames,
      prefix, AsUs(t->game_ticks) / count, prefix, AsUs(t->top_ticks) / count,
      prefix, AsUs(t->bottom_ticks) / count, prefix, AsUs(t->present_ticks) / count,
      prefix, t->phase_frames,
      prefix, t->phase_frames ? AsUs(t->logic_ticks) / t->phase_frames : 0,
      prefix, t->phase_frames ? AsUs(t->ppu_ticks) / t->phase_frames : 0,
      prefix, t->detail_frames);
  static const char *names[] = {"bg_main", "obj_main", "bg_sub", "obj_sub", "compose", "upload"};
  for (unsigned i = 0; i < 6 && result >= 0; i++)
    result = fprintf(out, "%s_avg_%s_us=%" PRIu64 "\n", prefix, names[i],
        t->detail_frames ? AsUs(t->detail_ticks[i]) / t->detail_frames : 0);
  return result > 0 && !ferror(out);
}

static const FrameSample *SampleAt(unsigned index) {
  unsigned first = (g_next + kRecentFrameCount - g_count) % kRecentFrameCount;
  return &g_samples[(first + index) % kRecentFrameCount];
}

bool FrameDiagnostics_WriteSummary(FILE *out) {
  if (!out) return false;
  uint64_t interval_sum = 0, game_sum = 0, logic_sum = 0, ppu_sum = 0;
  uint64_t top_sum = 0;
  uint64_t bottom_sum = 0, present_sum = 0, work_sum = 0;
  uint64_t bg_main_sum = 0, obj_main_sum = 0, bg_sub_sum = 0;
  uint64_t obj_sub_sum = 0, compose_sum = 0, upload_sum = 0;
  unsigned intervals = 0, interval_over = 0, work_over = 0;
  unsigned wide = 0, gpu = 0, presenter = 0, profiled = 0, detailed = 0;
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
    if (s->phase_profile) {
      logic_sum += s->logic_ticks;
      ppu_sum += s->ppu_ticks;
      profiled++;
    }
    if (s->ppu_detail) {
      bg_main_sum += s->bg_main_ticks;
      obj_main_sum += s->obj_main_ticks;
      bg_sub_sum += s->bg_sub_ticks;
      obj_sub_sum += s->obj_sub_ticks;
      compose_sum += s->compose_ticks;
      upload_sum += s->upload_ticks;
      detailed++;
    }
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
      "timing_schema=4\n"
      "recent_timing_scope=last_120_rendered_frames\n"
      "recent_frames=%u\nvalid_intervals=%u\n"
      "measured_fps=%" PRIu64 ".%02" PRIu64 "\n"
      "avg_interval_us=%" PRIu64 "\nmax_interval_us=%" PRIu64 "\n"
      "intervals_over_16667us=%u\n"
      "avg_game_us=%" PRIu64 "\n"
      "phase_profile_frames=%u\navg_logic_us=%" PRIu64 "\n"
      "avg_ppu_us=%" PRIu64 "\navg_top_us=%" PRIu64 "\n"
      "ppu_detail_frames=%u\n"
      "avg_bg_main_us=%" PRIu64 "\navg_obj_main_us=%" PRIu64 "\n"
      "avg_bg_sub_us=%" PRIu64 "\navg_obj_sub_us=%" PRIu64 "\n"
      "avg_compose_us=%" PRIu64 "\navg_upload_us=%" PRIu64 "\n"
      "avg_bottom_us=%" PRIu64 "\navg_present_us=%" PRIu64 "\n"
      "avg_work_us=%" PRIu64 "\nmax_work_us=%" PRIu64 "\n"
      "work_frames_over_16667us=%u\n"
      "recent_widescreen_frames=%u\nrecent_pica_gpu_frames=%u\n"
      "recent_presenter_frames=%u\n"
      "timing_note=phase durations are CPU wall spans; GPU work can finish asynchronously\n"
      "frame_times_file=frame-times.csv\n",
      g_count, intervals, fps_x100 / 100, fps_x100 % 100,
      intervals ? interval_us / intervals : 0, AsUs(max_interval), interval_over,
      AsUs(game_sum) / count, profiled,
      profiled ? AsUs(logic_sum) / profiled : 0,
      profiled ? AsUs(ppu_sum) / profiled : 0,
      AsUs(top_sum) / count, detailed,
      detailed ? AsUs(bg_main_sum) / detailed : 0,
      detailed ? AsUs(obj_main_sum) / detailed : 0,
      detailed ? AsUs(bg_sub_sum) / detailed : 0,
      detailed ? AsUs(obj_sub_sum) / detailed : 0,
      detailed ? AsUs(compose_sum) / detailed : 0,
      detailed ? AsUs(upload_sum) / detailed : 0,
      AsUs(bottom_sum) / count,
      AsUs(present_sum) / count, AsUs(work_sum) / count, AsUs(max_work), work_over,
      wide, gpu, presenter);
  return result > 0 && !ferror(out) &&
      fputs("session_timing_scope=since_launch; intervals exclude dump, suspend and pause gaps\n"
            "gameplay_timing_scope=game_states_7_to_11; grouped by widescreen mode\n", out) >= 0 &&
      WriteSession(out, "session", &g_session[0]) &&
      WriteSession(out, "gameplay_standard", &g_session[1]) &&
      WriteSession(out, "gameplay_widescreen", &g_session[2]) &&
      WriteEvent(out, "sector_change", &g_sector_changes) &&
      WriteEvent(out, "scroll_block_change", &g_scroll_changes) &&
      WriteEvent(out, "bottom_redraw", &g_bottom_redraws) && !ferror(out);
}

bool FrameDiagnostics_WriteCsv(FILE *out) {
  if (!out) return false;
  if (fputs("frame,interval_us,game_us,logic_us,ppu_us,top_us,bottom_us,"
            "present_us,work_us,widescreen,pica_gpu,presenter,phase_profile,"
            "bg_main_us,obj_main_us,bg_sub_us,obj_sub_us,compose_us,upload_us,"
            "ppu_detail,game_state,room_ptr,map_x,map_y,scroll_block_x,"
            "scroll_block_y,map_sector_changed,scroll_block_changed,bottom_redrawn\n",
            out) < 0) return false;
  for (unsigned i = 0; i < g_count; i++) {
    const FrameSample *s = SampleAt(i);
    if (fprintf(out, "%" PRIu32 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64
                     ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64
                     ",%" PRIu64 ",%u,%u,%u,%u,%" PRIu64 ",%" PRIu64
                     ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64
                     ",%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n",
                s->frame, AsUs(s->interval_ticks), AsUs(s->game_ticks),
                AsUs(s->logic_ticks), AsUs(s->ppu_ticks),
                AsUs(s->top_ticks), AsUs(s->bottom_ticks),
                AsUs(s->present_ticks), AsUs(s->work_ticks),
                s->wide, s->pica_gpu, s->presenter, s->phase_profile,
                AsUs(s->bg_main_ticks), AsUs(s->obj_main_ticks),
                AsUs(s->bg_sub_ticks), AsUs(s->obj_sub_ticks),
                AsUs(s->compose_ticks), AsUs(s->upload_ticks),
                s->ppu_detail, s->game_state, s->room, s->map_x, s->map_y,
                s->scroll_x, s->scroll_y, s->sector_changed,
                s->scroll_changed, s->bottom_redrawn) < 0)
      return false;
  }
  return !ferror(out);
}
