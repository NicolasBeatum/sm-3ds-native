#include "../source/frame_diagnostics.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void ReadFile(FILE *file, char *buffer, size_t capacity) {
  rewind(file);
  size_t count = fread(buffer, 1, capacity - 1, file);
  buffer[count] = 0;
}

int main(void) {
  FrameDiagnostics_Init(1000000);
  for (unsigned frame = 1; frame <= 120; frame++) {
    FrameDiagnostics_Record(frame, 20000, 5000, 2000, 3000, 7000, 2000,
                            1000, true, frame % 2 == 0, true, true, true);
    FrameDiagnostics_RecordPpuDetail(400, 300, 200, 100, 500, 600);
    FrameDiagnostics_RecordScene(8, 0x92fd, 10, 4, 20, 2, false);
  }

  FILE *summary = tmpfile();
  FILE *csv = tmpfile();
  assert(summary && csv);
  assert(FrameDiagnostics_WriteSummary(summary));
  assert(FrameDiagnostics_WriteCsv(csv));
  char text[20000];
  ReadFile(summary, text, sizeof(text));
  assert(strstr(text, "recent_frames=120\n"));
  assert(strstr(text, "measured_fps=50.00\n"));
  assert(strstr(text, "avg_work_us=15000\n"));
  assert(strstr(text, "avg_logic_us=2000\n"));
  assert(strstr(text, "avg_ppu_us=3000\n"));
  assert(strstr(text, "ppu_detail_frames=120\n"));
  assert(strstr(text, "avg_bg_main_us=400\n"));
  assert(strstr(text, "avg_upload_us=600\n"));
  assert(strstr(text, "intervals_over_16667us=120\n"));
  assert(strstr(text, "recent_pica_gpu_frames=60\n"));

  FrameDiagnostics_Record(121, 10000, 2000, 700, 1200, 3000, 1000, 1000,
                          false, false, true, true, true);
  FrameDiagnostics_RecordPpuDetail(20, 30, 40, 50, 60, 70);
  FrameDiagnostics_RecordScene(8, 0x92fd, 11, 4, 21, 2, true);
  fclose(csv);
  csv = tmpfile();
  assert(csv && FrameDiagnostics_WriteCsv(csv));
  ReadFile(csv, text, sizeof(text));
  assert(strstr(text, "\n1,20000,") == NULL);
  assert(strstr(text, "\n2,20000,"));
  assert(strstr(text, "\n121,10000,2000,700,1200,3000,1000,1000,7000,0,0,1,1,20,30,40,50,60,70,1,8,37629,11,4,21,2,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0\n"));
  FILE *session = tmpfile();
  assert(session && FrameDiagnostics_WriteSummary(session));
  ReadFile(session, text, sizeof(text));
  assert(strstr(text, "session_frames=121\n"));
  assert(strstr(text, "session_duration_us=2410000\n"));
  assert(strstr(text, "gameplay_widescreen_frames=120\n"));
  assert(strstr(text, "gameplay_standard_frames=1\n"));
  assert(strstr(text, "gameplay_standard_measured_fps=100.00\n"));
  assert(strstr(text, "sector_change_frames=1\n"));
  assert(strstr(text, "sector_change_avg_work_us=7000\n"));
  assert(strstr(text, "bottom_redraw_frames=1\n"));
  fclose(session);
  // A slow early frame remains in the session after it leaves the ring.
  FrameDiagnostics_Init(1000000);
  FrameDiagnostics_Record(1, 100000, 90000, 0, 0, 1000, 0, 0,
                          false, true, true, false, true);
  for (unsigned frame = 2; frame <= 250; frame++)
    FrameDiagnostics_Record(frame, 16000, 10000, 0, 0, 1000, 0, 0,
                            false, true, true, false, true);
  session = tmpfile();
  assert(session && FrameDiagnostics_WriteSummary(session));
  ReadFile(session, text, sizeof(text));
  assert(strstr(text, "measured_fps=62.50\n"));
  assert(strstr(text, "session_frames=250\n"));
  assert(strstr(text, "session_measured_fps=61.21\n"));
  assert(strstr(text, "session_max_interval_us=100000\n"));
  fclose(session);
  fclose(summary);
  fclose(csv);

  /* Matched ON/OFF samples, a special-effect frame with ON requested, and a
   * CPU fallback: only actual GPU gameplay contributes to comparison cohorts.
   * Use a non-microsecond clock to verify conversion, and overflow the ring. */
  FrameDiagnostics_Init(2000000);
  FrameDiagnostics_RecordLighting(true, true, true, 99, 99, 99, 99, 8, 8, 8, 2, true);
  for (unsigned frame = 1; frame <= 125; frame++) {
    bool gpu = frame != 4;
    FrameDiagnostics_Record(frame, 40000, 10000, 4000, 6000, 14000, 4000,
                            2000, true, gpu, true, true, true);
    FrameDiagnostics_RecordScene(8, 0x91f8, 0, 0, 0, 0, false);
    if (frame == 2)
      FrameDiagnostics_RecordLighting(false, false, true, 99, 99, 99, 10000, 8, 8, 8, 2, true);
    else if (frame == 3)
      FrameDiagnostics_RecordLighting(true, false, true, 99, 99, 99, 8000, 8, 8, 8, 2, true);
    else
      FrameDiagnostics_RecordLighting(true, true, true, 400, 60, 80, 12000, 4, 10, 3, 2, frame == 1);
  }
  summary = tmpfile(); csv = tmpfile();
  assert(summary && csv);
  assert(FrameDiagnostics_WriteSummary(summary) && FrameDiagnostics_WriteCsv(csv));
  ReadFile(summary, text, sizeof(text));
  assert(strstr(text, "timing_schema=6\n"));
  assert(strstr(text, "recent_lighting_active_frames=120\n"));
  assert(strstr(text, "recent_light_field_rebuilds=0\n"));
  assert(strstr(text, "session_lighting_profile_frames=124\n"));
  assert(strstr(text, "session_lighting_active_frames=122\n"));
  assert(strstr(text, "session_light_field_rebuilds=1\n"));
  assert(strstr(text, "lighting_on_frames=122\n"));
  assert(strstr(text, "lighting_off_frames=1\n"));
  assert(strstr(text, "lighting_on_avg_light_prepare_us=200\n"));
  assert(strstr(text, "lighting_on_avg_shadow_submit_us=30\n"));
  assert(strstr(text, "lighting_on_avg_glow_submit_us=40\n"));
  assert(strstr(text, "lighting_on_avg_model_build_us=6000\n"));
  assert(strstr(text, "lighting_off_avg_model_build_us=5000\n"));
  assert(strstr(text, "lighting_off_avg_light_prepare_us=0\n"));
  assert(strstr(text, "lighting_on_shadow_quads_total=1220\n"));
  assert(strstr(text, "lighting_on_glow_quads_total=366\n"));
  assert(strstr(text, "lighting_on_background_sources_total=244\n"));
  assert(strstr(text, "lighting_off_background_sources_total=0\n"));
  ReadFile(csv, text, sizeof(text));
  assert(strstr(text, "lighting_requested,lighting_applied,lighting_profile,light_prepare_us"));
  assert(strstr(text, ",1,1,1,200,30,40,6000,4,10,3,0,2\n"));
  fclose(summary); fclose(csv);

  /* No instrumentation is reported as unavailable, rather than timed OFF. */
  FrameDiagnostics_Init(1000000);
  FrameDiagnostics_Record(1, 16000, 10000, 0, 0, 1000, 0, 0,
                          false, true, true, false, true);
  FrameDiagnostics_RecordLighting(true, true, false, 99, 99, 99, 99, 8, 8, 8, 2, true);
  summary = tmpfile();
  assert(summary && FrameDiagnostics_WriteSummary(summary));
  ReadFile(summary, text, sizeof(text));
  assert(strstr(text, "session_lighting_profile_frames=0\n"));
  assert(strstr(text, "lighting_off_frames=0\n"));
  fclose(summary);
  return 0;
}
