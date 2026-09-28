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
#include "storage_paths.h"

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
  int result = fprintf(f,
      "sm3dsnative debug dump v2\n"
      "dump_id=%s\nrom=%s\nframe=%lu\n"
      "game_state=%u\narea_index=%u\nroom_ptr=%04x\n"
      "samus_x=%u\nsamus_y=%u\nsamus_health=%u\n"
      "widescreen=%u\nhide_main_hud=%u\n"
      "wram_file=memory.wram\nsram_file=save.sram\n",
      dump_id, rom_name ? rom_name : "unknown", (unsigned long)frame_number,
      game_state, area_index, room_ptr,
      samus_x_pos, samus_y_pos, samus_health,
      BottomScreen_WidescreenEnabled(), BottomScreen_HideMainHud());
  bool okay = result > 0 && fflush(f) == 0;
  if (fclose(f) != 0) okay = false;
  if (okay) okay = WriteBinary(directory, "memory.wram", g_ram, sizeof(g_ram));
  if (okay) okay = g_sram && WriteBinary(directory, "save.sram", g_sram, 8192);
  if (!okay) {
    if (DumpPath(path, sizeof(path), directory, "info.txt")) remove(path);
    if (DumpPath(path, sizeof(path), directory, "memory.wram")) remove(path);
    if (DumpPath(path, sizeof(path), directory, "save.sram")) remove(path);
    rmdir(directory);
  }
  return okay;
}
