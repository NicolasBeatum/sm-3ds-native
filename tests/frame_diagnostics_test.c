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
  assert(strstr(text, "\n121,10000,2000,700,1200,3000,1000,1000,7000,0,0,1,1,20,30,40,50,60,70,1,8,37629,11,4,21,2,1,1,1\n"));
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
  return 0;
}
