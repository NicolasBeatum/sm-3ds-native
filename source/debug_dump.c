#include "debug_dump.h"

#include <3ds.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "src/sm_rtl.h"
#include "src/variables.h"
#include "bottom_screen.h"
#include "frame_diagnostics.h"
#include "ppu_gpu.h"
#include "screen_capture.h"
#include "storage_paths.h"
#include "version.h"

#ifndef SM3DS_BUILD_FLAGS
#define SM3DS_BUILD_FLAGS ""
#endif
#ifndef SM3DS_BUILD_LTO
#define SM3DS_BUILD_LTO 0
#endif

static bool DumpPath(char *path, size_t capacity, const char *directory,
                     const char *filename) {
  int size = snprintf(path, capacity, "%s/%s", directory, filename);
  return size >= 0 && size < (int)capacity;
}

static bool WriteBinary(const char *directory, const char *filename,
                        const void *data, size_t size) {
  char path[256];
  if (!DumpPath(path, sizeof(path), directory, filename)) return false;
  FILE *f = fopen(path, "wb");
  if (!f) return false;
  bool okay = fwrite(data, 1, size, f) == size;
  if (fclose(f) != 0) okay = false;
  if (!okay) remove(path);
  return okay;
}

static bool WriteFrameTimes(const char *directory) {
  char path[256];
  if (!DumpPath(path, sizeof(path), directory, "frame-times.csv")) return false;
  FILE *f = fopen(path, "w");
  if (!f) return false;
  bool okay = FrameDiagnostics_WriteCsv(f) && fflush(f) == 0;
  if (fclose(f) != 0) okay = false;
  if (!okay) remove(path);
  return okay;
}

bool DebugDump_Write(const char *rom_name, uint32_t frame_number) {
  static unsigned sequence;
  char stamp[24], dump_id[48], directory[256], path[256];
  time_t now = time(NULL);
  struct tm *local = localtime(&now);
  if (!local || !strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", local))
    snprintf(stamp, sizeof(stamp), "clock-%llu", (unsigned long long)osGetTime());

  bool created = false;
  for (unsigned attempt = 0; attempt < 1000; attempt++) {
    snprintf(dump_id, sizeof(dump_id), "%s-%03u", stamp, sequence++ % 1000);
    if (!DumpPath(directory, sizeof(directory), SM3DS_DUMP_DIR, dump_id))
      return false;
    if (mkdir(directory, 0755) == 0) { created = true; break; }
    if (errno != EEXIST) return false;
  }
  if (!created || !DumpPath(path, sizeof(path), directory, "info.txt"))
    return false;
  FILE *f = fopen(path, "w");
  if (!f) { rmdir(directory); return false; }
  ScreenCaptureStatus screens = ScreenCapture_Save(directory);
  bool new3ds = false;
  bool model_known = R_SUCCEEDED(APT_CheckNew3DS(&new3ds));
  u32 cpu_limit = 0;
  APT_GetAppCpuTimeLimit(&cpu_limit);
  int result = fprintf(f,
      "sm3dsnative debug dump v4\n"
      "dump_id=%s\nrom=%s\nframe=%lu\n"
      "app_version=%s\nbuild_flags=%s\nfull_native=%u\n"
      "build_lto=%u\nppu_bg_lookup=descriptor-cache-v1\n"
      "ppu_obj_lookup=row-membership-v1\nbottom_ui=map-cache-event-state-v1\n"
      "hardware=%s\ncpu_time_limit_percent=%lu\nlinear_free_bytes=%lu\n"
      "last_ppu_reason=%s\n"
      "screen_imported=%u\ntop_bmp=%u\nbottom_bmp=%u\n"
      "top_raw=%u\nbottom_raw=%u\n"
      "top_format=%lu\nbottom_format=%lu\n"
      "top_stride=%lu\nbottom_stride=%lu\n"
      "game_state=%u\narea_index=%u\nroom_ptr=%04x\n"
      "samus_x=%u\nsamus_y=%u\nsamus_health=%u\n"
      "widescreen=%u\nhide_main_hud=%u\n"
      "wram_file=memory.wram\nsram_file=save.sram\n",
      dump_id, rom_name ? rom_name : "unknown", (unsigned long)frame_number,
      APP_VERSION, SM3DS_BUILD_FLAGS,
#ifdef FULL_NATIVE
      1u,
#else
      0u,
#endif
      (unsigned)SM3DS_BUILD_LTO,
      !model_known ? "unknown" : new3ds ? "new3ds" : "old3ds",
      (unsigned long)cpu_limit,
      (unsigned long)linearSpaceFree(),
      PpuGpuReason() ? PpuGpuReason() : "unknown",
      screens.imported, screens.top_bmp, screens.bottom_bmp,
      screens.top_raw, screens.bottom_raw,
      (unsigned long)screens.top_format, (unsigned long)screens.bottom_format,
      (unsigned long)screens.top_stride, (unsigned long)screens.bottom_stride,
      game_state, area_index, room_ptr,
      samus_x_pos, samus_y_pos, samus_health,
      BottomScreen_WidescreenEnabled(), BottomScreen_HideMainHud());
  bool okay = result > 0 && FrameDiagnostics_WriteSummary(f) && fflush(f) == 0;
  if (fclose(f) != 0) okay = false;
  if (okay) okay = WriteFrameTimes(directory);
  if (okay) okay = WriteBinary(directory, "memory.wram", g_ram, sizeof(g_ram));
  if (okay) okay = g_sram && WriteBinary(directory, "save.sram", g_sram, 8192);
  if (!okay) {
    if (DumpPath(path, sizeof(path), directory, "info.txt")) remove(path);
    if (DumpPath(path, sizeof(path), directory, "memory.wram")) remove(path);
    if (DumpPath(path, sizeof(path), directory, "save.sram")) remove(path);
    if (DumpPath(path, sizeof(path), directory, "frame-times.csv")) remove(path);
    if (DumpPath(path, sizeof(path), directory, "top.bmp")) remove(path);
    if (DumpPath(path, sizeof(path), directory, "bottom.bmp")) remove(path);
    if (DumpPath(path, sizeof(path), directory, "top.raw")) remove(path);
    if (DumpPath(path, sizeof(path), directory, "bottom.raw")) remove(path);
    rmdir(directory);
  }
  return okay;
}
