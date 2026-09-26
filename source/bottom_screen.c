#include "bottom_screen.h"

#include <3ds.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "src/variables.h"

/* Native 320x240 companion screen inspired by MetroidArch's dual-screen UI.
 * The palette, 5x7 font, tabs and status layout deliberately track that UI,
 * while all values come straight from sm-3ds' live decompiled game state. */

enum BottomTab {
  kBottomTab_Map,
  kBottomTab_Items,
  kBottomTab_Setup,
};

typedef struct UiColor {
  uint8_t r, g, b;
} UiColor;

static const UiColor kBg = {30, 33, 44};
static const UiColor kPanel = {38, 42, 56};
static const UiColor kBorder = {58, 64, 86};
static const UiColor kBorderHi = {115, 124, 155};
static const UiColor kDim = {105, 110, 128};
static const UiColor kEnergy = {204, 71, 145};
static const UiColor kAccent = {255, 158, 68};
static const UiColor kWhite = {235, 238, 248};
static const UiColor kMapFill = {64, 48, 112};
static const UiColor kMapLine = {124, 86, 202};
static const UiColor kSamus = {255, 70, 70};

static enum BottomTab g_bottom_tab = kBottomTab_Map;
enum {
  kBottomTextureWidth = 512,
  kBottomTextureHeight = 256,
};

static uint8_t *g_bottom_cache;
static unsigned g_bottom_frame;
static bool g_bottom_dirty = true;

static inline void PutPixel(uint8_t *fb, int x, int y, UiColor color) {
  if ((unsigned)x >= 320 || (unsigned)y >= 240)
    return;
  const int i = (y * kBottomTextureWidth + x) * 4;
  fb[i + 0] = color.b;
  fb[i + 1] = color.g;
  fb[i + 2] = color.r;
  fb[i + 3] = 0xff;
}

static void FillRect(uint8_t *fb, int x, int y, int w, int h, UiColor color) {
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > 320) w = 320 - x;
  if (y + h > 240) h = 240 - y;
  if (w <= 0 || h <= 0)
    return;
  for (int py = y; py < y + h; py++) {
    uint8_t *p = fb + (py * kBottomTextureWidth + x) * 4;
    for (int px = 0; px < w; px++, p += 4) {
      p[0] = color.b;
      p[1] = color.g;
      p[2] = color.r;
      p[3] = 0xff;
    }
  }
}

static void StrokeRect(uint8_t *fb, int x, int y, int w, int h, int thickness, UiColor color) {
  FillRect(fb, x, y, w, thickness, color);
  FillRect(fb, x, y + h - thickness, w, thickness, color);
  FillRect(fb, x, y, thickness, h, color);
  FillRect(fb, x + w - thickness, y, thickness, h, color);
}

static void Panel(uint8_t *fb, int x, int y, int w, int h) {
  FillRect(fb, x, y, w, h, kPanel);
  StrokeRect(fb, x, y, w, h, 2, kBorder);
}

/* Same hand-drawn 5x7 pixel font used by the reference project. */
static const uint8_t *Glyph(char c) {
  static const uint8_t A[7] = {4,10,17,17,31,17,17};
  static const uint8_t B[7] = {30,17,17,30,17,17,30};
  static const uint8_t C[7] = {15,16,16,16,16,16,15};
  static const uint8_t D[7] = {30,17,17,17,17,17,30};
  static const uint8_t E[7] = {31,16,16,30,16,16,31};
  static const uint8_t F[7] = {31,16,16,30,16,16,16};
  static const uint8_t G[7] = {15,16,16,23,17,17,15};
  static const uint8_t H[7] = {17,17,17,31,17,17,17};
  static const uint8_t I[7] = {31,4,4,4,4,4,31};
  static const uint8_t J[7] = {7,2,2,2,2,18,12};
  static const uint8_t K[7] = {17,18,20,24,20,18,17};
  static const uint8_t L[7] = {16,16,16,16,16,16,31};
  static const uint8_t M[7] = {17,27,21,21,17,17,17};
  static const uint8_t N[7] = {17,25,21,21,19,17,17};
  static const uint8_t O[7] = {14,17,17,17,17,17,14};
  static const uint8_t P[7] = {30,17,17,30,16,16,16};
  static const uint8_t Q[7] = {14,17,17,17,21,18,13};
  static const uint8_t R[7] = {30,17,17,30,20,18,17};
  static const uint8_t S[7] = {15,16,16,14,1,1,30};
  static const uint8_t T[7] = {31,4,4,4,4,4,4};
  static const uint8_t U[7] = {17,17,17,17,17,17,14};
  static const uint8_t V[7] = {17,17,17,17,17,10,4};
  static const uint8_t W[7] = {17,17,17,21,21,27,17};
  static const uint8_t X[7] = {17,17,10,4,10,17,17};
  static const uint8_t Y[7] = {17,17,10,4,4,4,4};
  static const uint8_t Z[7] = {31,1,2,4,8,16,31};
  static const uint8_t N0[7] = {14,17,19,21,25,17,14};
  static const uint8_t N1[7] = {4,12,4,4,4,4,31};
  static const uint8_t N2[7] = {14,17,1,2,4,8,31};
  static const uint8_t N3[7] = {30,1,1,6,1,1,30};
  static const uint8_t N4[7] = {2,6,10,18,31,2,2};
  static const uint8_t N5[7] = {31,16,16,30,1,1,30};
  static const uint8_t N6[7] = {14,16,16,30,17,17,14};
  static const uint8_t N7[7] = {31,1,2,4,8,8,8};
  static const uint8_t N8[7] = {14,17,17,14,17,17,14};
  static const uint8_t N9[7] = {14,17,17,15,1,1,14};
  static const uint8_t DOT[7] = {0,0,0,0,0,4,4};
  static const uint8_t SLASH[7] = {1,1,2,4,8,16,16};
  static const uint8_t DASH[7] = {0,0,0,31,0,0,0};
  static const uint8_t COLON[7] = {0,4,4,0,4,4,0};
  static const uint8_t PERCENT[7] = {17,18,2,4,8,9,17};
  static const uint8_t PLUS[7] = {0,4,4,31,4,4,0};
  static const uint8_t EMPTY[7] = {0,0,0,0,0,0,0};
  static const uint8_t *letters[26] = {A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P,Q,R,S,T,U,V,W,X,Y,Z};
  static const uint8_t *numbers[10] = {N0,N1,N2,N3,N4,N5,N6,N7,N8,N9};
  if (c >= 'a' && c <= 'z') c -= 'a' - 'A';
  if (c >= 'A' && c <= 'Z') return letters[c - 'A'];
  if (c >= '0' && c <= '9') return numbers[c - '0'];
  switch (c) {
    case '.': return DOT;
    case '/': return SLASH;
    case '-': return DASH;
    case ':': return COLON;
    case '%': return PERCENT;
    case '+': return PLUS;
    default: return EMPTY;
  }
}

static int TextWidth(const char *text, int scale) {
  return text[0] ? ((int)strlen(text) * 6 - 1) * scale : 0;
}

static void DrawText(uint8_t *fb, int x, int y, const char *text, int scale, UiColor color) {
  for (; *text; text++, x += 6 * scale) {
    const uint8_t *rows = Glyph(*text);
    for (int row = 0; row < 7; row++)
      for (int col = 0; col < 5; col++)
        if (rows[row] & (1 << (4 - col)))
          FillRect(fb, x + col * scale, y + row * scale, scale, scale, color);
  }
}

static void DrawTextCentered(uint8_t *fb, int cx, int y, const char *text, int scale, UiColor color) {
  DrawText(fb, cx - TextWidth(text, scale) / 2, y, text, scale, color);
}

static const char *AreaName(unsigned area) {
  static const char *names[] = {
    "CRATERIA", "BRINSTAR", "NORFAIR", "WRECKED SHIP",
    "MARIDIA", "TOURIAN", "CERES", "DEBUG"
  };
  return area < 8 ? names[area] : "UNKNOWN";
}

static bool IsLiveGameplay(void) {
  return game_state >= 7 && game_state <= 0x0b;
}

static void DrawStatus(uint8_t *fb) {
  Panel(fb, 5, 4, 310, 40);

  const int tanks = samus_max_health / 100;
  const int filled = samus_health / 100;
  for (int i = 0; i < 14; i++) {
    int x = 11 + (i % 7) * 9;
    int y = 10 + (i / 7) * 12;
    UiColor color = i < filled ? kEnergy : (i < tanks ? kBorderHi : kBorder);
    FillRect(fb, x, y, 7, 8, color);
    if (i < tanks) StrokeRect(fb, x, y, 7, 8, 1, kWhite);
  }

  char text[32];
  snprintf(text, sizeof(text), "E %02u", samus_health % 100);
  DrawText(fb, 78, 17, text, 1, kWhite);

  const unsigned counts[3] = {samus_missiles, samus_super_missiles, samus_power_bombs};
  const unsigned maximums[3] = {samus_max_missiles, samus_max_super_missiles, samus_max_power_bombs};
  const char *labels[3] = {"M", "S", "PB"};
  const int slots[3] = {1, 2, 3};
  const int xs[3] = {124, 188, 250};
  for (int i = 0; i < 3; i++) {
    if (!maximums[i]) continue;
    if (hud_item_index == slots[i]) {
      FillRect(fb, xs[i] - 4, 10, i == 2 ? 62 : 56, 24, kBorder);
      StrokeRect(fb, xs[i] - 4, 10, i == 2 ? 62 : 56, 24, 2, kAccent);
    }
    snprintf(text, sizeof(text), "%s %u", labels[i], counts[i]);
    DrawText(fb, xs[i], 18, text, 1, kWhite);
  }
}

static bool IsMapTileExplored(int x, int y) {
  if ((unsigned)x >= 64 || (unsigned)y >= 32)
    return false;
  const int index = (x >> 3) + 4 * ((x & 0x20) + y);
  return (map_tiles_explored[index] & (0x80 >> (x & 7))) != 0;
}

static void DrawMapTab(uint8_t *fb) {
  Panel(fb, 5, 49, 310, 149);
  char title[32];
  snprintf(title, sizeof(title), "MAP - %s", AreaName(area_index));
  DrawTextCentered(fb, 160, 55, title, 1, kAccent);

  const int samus_tile_x = room_x_coordinate_on_map + (samus_x_pos >> 8);
  const int samus_tile_y = room_y_coordinate_on_map + (samus_y_pos >> 8) + 1;
  const int cols = 30, rows = 15, cell = 8;
  int crop_x = samus_tile_x - cols / 2;
  int crop_y = samus_tile_y - rows / 2;
  if (crop_x < 0) crop_x = 0;
  if (crop_y < 0) crop_y = 0;
  if (crop_x > 64 - cols) crop_x = 64 - cols;
  if (crop_y > 32 - rows) crop_y = 32 - rows;
  const int origin_x = 40, origin_y = 70;

  FillRect(fb, origin_x - 2, origin_y - 2, cols * cell + 4, rows * cell + 4, kBg);
  for (int y = 0; y < rows; y++) {
    for (int x = 0; x < cols; x++) {
      if (!IsMapTileExplored(crop_x + x, crop_y + y)) continue;
      FillRect(fb, origin_x + x * cell, origin_y + y * cell, cell, cell, kMapFill);
      StrokeRect(fb, origin_x + x * cell, origin_y + y * cell, cell, cell, 1, kMapLine);
    }
  }
  const int dot_x = origin_x + (samus_tile_x - crop_x) * cell + cell / 2;
  const int dot_y = origin_y + (samus_tile_y - crop_y) * cell + cell / 2;
  FillRect(fb, dot_x - 3, dot_y - 3, 7, 7, kSamus);
  PutPixel(fb, dot_x, dot_y, kWhite);
}

static int CountItems(void) {
  static const uint16_t item_bits[] = {0x0001,0x0020,0x0004,0x1000,0x0002,0x0008,0x0100,0x0200,0x2000};
  static const uint16_t beam_bits[] = {0x1000,0x0002,0x0001,0x0004,0x0008};
  int count = 0;
  for (unsigned i = 0; i < sizeof(item_bits) / sizeof(item_bits[0]); i++)
    count += !!(collected_items & item_bits[i]);
  for (unsigned i = 0; i < sizeof(beam_bits) / sizeof(beam_bits[0]); i++)
    count += !!(collected_beams & beam_bits[i]);
  return count;
}

static void DrawItemLine(uint8_t *fb, int x, int y, const char *label, bool collected, bool equipped) {
  UiColor text = collected ? kWhite : kDim;
  UiColor mark = equipped ? kAccent : (collected ? kBorderHi : kBorder);
  FillRect(fb, x, y + 2, 5, 5, mark);
  DrawText(fb, x + 9, y, label, 1, text);
}

static void DrawItemsTab(uint8_t *fb) {
  Panel(fb, 5, 49, 310, 149);
  char text[32];
  snprintf(text, sizeof(text), "ITEMS %u/14", CountItems());
  DrawText(fb, 14, 57, text, 1, kAccent);
  snprintf(text, sizeof(text), "TIME %02u:%02u:%02u", game_time_hours, game_time_minutes, game_time_seconds);
  DrawText(fb, 175, 57, text, 1, kWhite);

  DrawText(fb, 14, 76, "SUIT / MISC", 1, kAccent);
  DrawItemLine(fb, 14, 88, "VARIA SUIT", collected_items & 0x0001, equipped_items & 0x0001);
  DrawItemLine(fb, 14, 99, "GRAVITY SUIT", collected_items & 0x0020, equipped_items & 0x0020);
  DrawItemLine(fb, 14, 110, "MORPH BALL", collected_items & 0x0004, equipped_items & 0x0004);
  DrawItemLine(fb, 14, 121, "BOMB", collected_items & 0x1000, equipped_items & 0x1000);
  DrawItemLine(fb, 14, 132, "SPRING BALL", collected_items & 0x0002, equipped_items & 0x0002);
  DrawItemLine(fb, 14, 143, "SCREW ATTACK", collected_items & 0x0008, equipped_items & 0x0008);

  DrawText(fb, 172, 76, "BOOTS / BEAM", 1, kAccent);
  DrawItemLine(fb, 172, 88, "HI-JUMP", collected_items & 0x0100, equipped_items & 0x0100);
  DrawItemLine(fb, 172, 99, "SPACE JUMP", collected_items & 0x0200, equipped_items & 0x0200);
  DrawItemLine(fb, 172, 110, "SPEED BOOST", collected_items & 0x2000, equipped_items & 0x2000);
  DrawItemLine(fb, 172, 121, "CHARGE", collected_beams & 0x1000, equipped_beams & 0x1000);
  DrawItemLine(fb, 172, 132, "ICE", collected_beams & 0x0002, equipped_beams & 0x0002);
  DrawItemLine(fb, 172, 143, "WAVE", collected_beams & 0x0001, equipped_beams & 0x0001);
  DrawItemLine(fb, 172, 154, "SPAZER", collected_beams & 0x0004, equipped_beams & 0x0004);
  DrawItemLine(fb, 172, 165, "PLASMA", collected_beams & 0x0008, equipped_beams & 0x0008);
}

static void DrawSetupTab(uint8_t *fb) {
  Panel(fb, 5, 49, 310, 149);
  DrawTextCentered(fb, 160, 61, "METROIDARCH 3DS", 2, kAccent);
  DrawTextCentered(fb, 160, 88, "NATIVE DUAL SCREEN PORT", 1, kWhite);
  DrawText(fb, 20, 111, "STATUS + MAPA EN VIVO", 1, kWhite);
  DrawText(fb, 20, 126, "PESTANAS Y ARMAS TACTILES", 1, kWhite);
  DrawText(fb, 20, 141, "FULL NATIVE: ON", 1, kWhite);
  DrawText(fb, 20, 163, "BASE: SM-3DS + METROIDARCH", 1, kDim);
}

static void DrawTabs(uint8_t *fb) {
  static const char *labels[] = {"MAP", "ITEMS", "SETUP"};
  for (int i = 0; i < 3; i++) {
    const int x = 5 + i * 104;
    UiColor fill = g_bottom_tab == i ? kBorder : kPanel;
    UiColor border = g_bottom_tab == i ? kAccent : kBorder;
    FillRect(fb, x, 204, 100, 31, fill);
    StrokeRect(fb, x, 204, 100, 31, 2, border);
    DrawTextCentered(fb, x + 50, 216, labels[i], 1, kWhite);
  }
}

static void DrawIdle(uint8_t *fb) {
  FillRect(fb, 0, 0, 320, 240, kBg);
  FillRect(fb, 54, 89, 212, 2, kBorder);
  FillRect(fb, 54, 148, 212, 2, kBorder);
  DrawTextCentered(fb, 160, 109, "METROID", 2, kBorderHi);
}

bool BottomScreen_Init(void) {
  g_bottom_cache = linearMemAlign(
      kBottomTextureWidth * kBottomTextureHeight * 4, 0x80);
  if (!g_bottom_cache)
    return false;
  memset(g_bottom_cache, 0, kBottomTextureWidth * kBottomTextureHeight * 4);
  return true;
}

bool BottomScreen_Draw(void) {
  if (!g_bottom_cache)
    return false;

  /* The UI data changes slowly, so rebuild its texture at 15 Hz. The GPU
   * still presents the cached texture every frame to keep both LCD buffers
   * synchronized. */
  bool redraw = g_bottom_dirty || ((g_bottom_frame++ & 3) == 0);
  if (!redraw)
    return false;

  g_bottom_dirty = false;
  if (!IsLiveGameplay()) {
    DrawIdle(g_bottom_cache);
  } else {
    FillRect(g_bottom_cache, 0, 0, 320, 240, kBg);
    DrawStatus(g_bottom_cache);
    if (g_bottom_tab == kBottomTab_Map)
      DrawMapTab(g_bottom_cache);
    else if (g_bottom_tab == kBottomTab_Items)
      DrawItemsTab(g_bottom_cache);
    else
      DrawSetupTab(g_bottom_cache);
    DrawTabs(g_bottom_cache);
  }
  return true;
}

const uint8_t *BottomScreen_Pixels(void) {
  return g_bottom_cache;
}

void BottomScreen_CopyToFramebuffer(void) {
  if (!g_bottom_cache)
    return;
  uint8_t *fb = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL);
  if (!fb)
    return;
  for (int y = 0; y < 240; y++) {
    const uint8_t *src = g_bottom_cache + y * kBottomTextureWidth * 4;
    for (int x = 0; x < 320; x++, src += 4) {
      uint8_t *dst = fb + (x * 240 + (239 - y)) * 4;
      dst[0] = 0xff;
      dst[1] = src[0];
      dst[2] = src[1];
      dst[3] = src[2];
    }
  }
}

void BottomScreen_Fini(void) {
  if (g_bottom_cache)
    linearFree(g_bottom_cache);
  g_bottom_cache = NULL;
}

void BottomScreen_HandleTouch(float normalized_x, float normalized_y) {
  int x = (int)(normalized_x * 320.0f);
  int y = (int)(normalized_y * 240.0f);
  if (y >= 200) {
    if (x < 107) g_bottom_tab = kBottomTab_Map;
    else if (x < 213) g_bottom_tab = kBottomTab_Items;
    else g_bottom_tab = kBottomTab_Setup;
    g_bottom_dirty = true;
    return;
  }
  if (!IsLiveGameplay() || y >= 48)
    return;

  int slot = 0;
  if (x >= 116 && x < 184 && samus_max_missiles) slot = 1;
  else if (x >= 184 && x < 246 && samus_max_super_missiles) slot = 2;
  else if (x >= 246 && samus_max_power_bombs) slot = 3;
  if (slot) {
    hud_item_index = hud_item_index == slot ? 0 : slot;
    samus_auto_cancel_hud_item_index = 0;
    hud_auto_cancel_flag = 0;
    g_bottom_dirty = true;
  }
}
