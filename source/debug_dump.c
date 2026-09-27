#include "debug_dump.h"

#include <3ds.h>
#include <stdio.h>
#include <string.h>

#include "src/sm_rtl.h"
#include "src/variables.h"
#include "bottom_screen.h"
#include "storage_paths.h"

static bool WriteBinary(const char *stem, const char *extension,
                        const void *data, size_t size) {
  char path[256];
  if (snprintf(path, sizeof(path), "%s/%s.%s", SM3DS_DUMP_DIR,
               stem, extension) >= (int)sizeof(path)) return false;
  FILE *f = fopen(path, "wb");
  if (!f) return false;
  bool okay = fwrite(data, 1, size, f) == size;
  if (fclose(f) != 0) okay = false;
  if (!okay) remove(path);
  return okay;
}

bool DebugDump_Write(const char *rom_name, uint32_t frame_number) {
  static unsigned sequence;
  char stem[64], path[256];
  snprintf(stem, sizeof(stem), "dump_%llu_%u", (unsigned long long)osGetTime(), sequence++);
  snprintf(path, sizeof(path), "%s/%s.txt", SM3DS_DUMP_DIR, stem);
  FILE *f = fopen(path, "w");
  if (!f) return false;
  int result = fprintf(f,
      "sm3dsnative debug dump v1\n"
      "rom=%s\nframe=%lu\n"
      "game_state=%u\narea_index=%u\nroom_ptr=%04x\n"
      "samus_x=%u\nsamus_y=%u\nsamus_health=%u\n"
      "widescreen=%u\nhide_main_hud=%u\n"
      "wram_file=%s.wram\nsram_file=%s.sram\n",
      rom_name ? rom_name : "unknown", (unsigned long)frame_number,
      game_state, area_index, room_ptr,
      samus_x_pos, samus_y_pos, samus_health,
      BottomScreen_WidescreenEnabled(), BottomScreen_HideMainHud(),
      stem, stem);
  bool okay = result > 0 && fflush(f) == 0;
  if (fclose(f) != 0) okay = false;
  if (!okay) { remove(path); return false; }
  bool wram = WriteBinary(stem, "wram", g_ram, sizeof(g_ram));
  bool sram = g_sram && WriteBinary(stem, "sram", g_sram, 8192);
  if (!wram || !sram) {
    remove(path);
    char binary_path[256];
    snprintf(binary_path, sizeof(binary_path), "%s/%s.wram", SM3DS_DUMP_DIR, stem);
    remove(binary_path);
    snprintf(binary_path, sizeof(binary_path), "%s/%s.sram", SM3DS_DUMP_DIR, stem);
    remove(binary_path);
    return false;
  }
  return true;
}
