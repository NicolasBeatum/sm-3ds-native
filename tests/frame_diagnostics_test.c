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
                            1000, true, frame % 2 == 0, true, true);
    FrameDiagnostics_RecordPpuDetail(400, 300, 200, 100, 500, 600);
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
                          false, false, true, true);
  FrameDiagnostics_RecordPpuDetail(20, 30, 40, 50, 60, 70);
  fclose(csv);
  csv = tmpfile();
  assert(csv && FrameDiagnostics_WriteCsv(csv));
  ReadFile(csv, text, sizeof(text));
  assert(strstr(text, "\n1,20000,") == NULL);
  assert(strstr(text, "\n2,20000,"));
  assert(strstr(text, "\n121,10000,2000,700,1200,3000,1000,1000,7000,0,0,1,1,20,30,40,50,60,70,1\n"));
  fclose(summary);
  fclose(csv);
  return 0;
}
