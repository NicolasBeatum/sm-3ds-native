#include "rom_menu.h"

#include <3ds.h>
#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "SDL2/SDL.h"
#include "bottom_screen.h"
#include "storage_paths.h"

enum { kMaxRoms = 128, kNameSize = 128, kVisibleRows = 8 };
static char g_names[kMaxRoms][kNameSize];

static void ImportBundledRom(void) {
  struct stat st;
  const char *destination = SM3DS_SD_ROOT "/sm.smc";
  if (stat(destination, &st) == 0) return;
  FILE *source = fopen("romfs:/sm.smc", "rb");
  if (!source) return;
  FILE *target = fopen(SM3DS_SD_ROOT "/sm.smc.tmp", "wb");
  if (!target) { fclose(source); return; }
  char buffer[16384];
  size_t count;
  bool okay = true;
  while ((count = fread(buffer, 1, sizeof(buffer), source)) != 0) {
    if (fwrite(buffer, 1, count, target) != count) { okay = false; break; }
  }
  if (ferror(source) || fflush(target) != 0) okay = false;
  if (fclose(target) != 0) okay = false;
  fclose(source);
  if (!okay || rename(SM3DS_SD_ROOT "/sm.smc.tmp", destination) != 0)
    remove(SM3DS_SD_ROOT "/sm.smc.tmp");
}

static bool IsRomName(const char *name) {
  size_t n = strlen(name);
  if (n < 5 || n >= kNameSize) return false;
  const char *ext = name + n - 4;
  return ext[0] == '.' && tolower((unsigned char)ext[1]) == 's' &&
      ((tolower((unsigned char)ext[2]) == 'm' && tolower((unsigned char)ext[3]) == 'c') ||
       (tolower((unsigned char)ext[2]) == 'f' && tolower((unsigned char)ext[3]) == 'c'));
}

static int CompareName(const void *a, const void *b) {
  const char *left = a, *right = b;
  while (*left && *right) {
    int delta = tolower((unsigned char)*left++) - tolower((unsigned char)*right++);
    if (delta) return delta;
  }
  return (unsigned char)*left - (unsigned char)*right;
}

static int ScanRoms(void) {
  DIR *dir = opendir(SM3DS_SD_ROOT);
  if (!dir) return 0;
  int count = 0;
  struct dirent *entry;
  while ((entry = readdir(dir)) && count < kMaxRoms) {
    if (!IsRomName(entry->d_name)) continue;
    char path[256];
    if (snprintf(path, sizeof(path), "%s/%s", SM3DS_SD_ROOT, entry->d_name) >= (int)sizeof(path))
      continue;
    struct stat st;
    if (stat(path, &st) || !S_ISREG(st.st_mode)) continue;
    strcpy(g_names[count++], entry->d_name);
  }
  closedir(dir);
  qsort(g_names, count, sizeof(g_names[0]), CompareName);
  return count;
}

static void DrawMenu(int count, int selected, const char *message) {
  const char *rows[kVisibleRows] = {0};
  int first = selected >= kVisibleRows ? selected - kVisibleRows + 1 : 0;
  for (int i = 0; i < kVisibleRows && first + i < count; i++)
    rows[i] = g_names[first + i];
  BottomScreen_DrawRomSelector(rows, kVisibleRows, selected - first, count,
                               message);
  u8 *top = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
  if (top) memset(top, 0, 400 * 240 * 4);
  BottomScreen_CopyToFramebuffer();
  gfxFlushBuffers();
  gfxSwapBuffers();
  gspWaitForVBlank();
}

bool RomMenu_Select(char *path, size_t path_size, char *name, size_t name_size) {
  mkdir("sdmc:/3ds", 0755);
  mkdir(SM3DS_SD_ROOT, 0755);
  mkdir(SM3DS_SAVE_DIR, 0755);
  mkdir(SM3DS_DUMP_DIR, 0755);
  ImportBundledRom();
  int count = ScanRoms(), selected = 0;
  const char *message = count ? "A LOAD  X REFRESH  START EXIT" :
      "COPY .SMC TO /3DS/SM3DSNATIVE/";
  uint32_t last_move = 0;
  bool up_held = false, down_held = false;
  while (aptMainLoop()) {
    DrawMenu(count, selected, message);
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT) return false;
      if (event.type == SDL_JOYAXISMOTION && event.jaxis.axis == 1) {
        up_held = event.jaxis.value < -16000;
        down_held = event.jaxis.value > 16000;
      }
      if (event.type != SDL_JOYBUTTONDOWN) continue;
      unsigned button = event.jbutton.button;
      if (button == 3 || button == 1) return false; /* START or B */
      if (button == 10) { /* X */
        count = ScanRoms();
        if (selected >= count) selected = count ? count - 1 : 0;
        message = count ? "A LOAD  X REFRESH  START EXIT" :
            "COPY .SMC TO /3DS/SM3DSNATIVE/";
      } else if (button == 6 && count) {
        selected = selected > 0 ? selected - 1 : count - 1;
      } else if (button == 7 && count) {
        selected = (selected + 1) % count;
      } else if (button == 0 && count) {
        int n = snprintf(path, path_size, "%s/%s", SM3DS_SD_ROOT, g_names[selected]);
        if (n < 0 || (size_t)n >= path_size || strlen(g_names[selected]) >= name_size) {
          message = "ROM NAME IS TOO LONG";
          continue;
        }
        struct stat st;
        if (stat(path, &st) != 0 || st.st_size < 0x300000 || st.st_size > 0x800200) {
          message = "INVALID ROM SIZE";
          continue;
        }
        strcpy(name, g_names[selected]);
        return true;
      }
    }
    uint32_t now = SDL_GetTicks();
    if (count && now - last_move > 170 && (up_held != down_held)) {
      selected = up_held ? (selected > 0 ? selected - 1 : count - 1) :
          (selected + 1) % count;
      last_move = now;
    }
    SDL_Delay(16);
  }
  return false;
}
