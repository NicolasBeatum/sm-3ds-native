#include "bottom_screen.h"

#include <3ds.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "src/variables.h"
#include "src/sm_rtl.h"
#include "redux_suit_data.h"

#ifndef SM3DS_BUILD_FLAGS
#define SM3DS_BUILD_FLAGS ""
#endif

#ifndef SM3DS_BUILD_LTO
#define SM3DS_BUILD_LTO 0
#endif


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
static const UiColor kSamus = {255, 70, 70};
static const UiColor kSlot = {48, 52, 68};

static const UiColor kAreaColors[6] = {
  {150, 165, 210}, {110, 210, 110}, {235, 110, 90},
  {210, 180, 110}, {255, 100, 100}, {220, 110, 190},
};

static enum BottomTab g_bottom_tab = kBottomTab_Map;
enum {
  kBottomTextureWidth = 512,
  kBottomTextureHeight = 256,
};

static uint8_t *g_bottom_cache;
static unsigned g_bottom_frame;
static bool g_bottom_dirty = true;
static bool g_world_view;
static int g_room_zoom = 1;
static int g_world_zoom;
static bool g_hide_main_hud;
static bool g_widescreen = true;
static bool g_status_bar_visible[3] = {true, true, false};
static bool g_clear_markers_armed;
static bool g_setup_build_info;
typedef struct MapMarker { uint8_t area, x, y; } MapMarker;
static MapMarker g_markers[16];
static int g_marker_count;
static int g_room_map_x, g_room_map_y, g_room_map_w, g_room_map_h;
static int g_room_crop_x, g_room_crop_y, g_room_cols, g_room_rows;
static uint64_t g_touch_down_ms;
static int g_touch_down_x, g_touch_down_y;

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

static void FillCircle(uint8_t *fb, int cx, int cy, int radius, UiColor color) {
  const int rr = radius * radius;
  for (int y = -radius; y <= radius; y++)
    for (int x = -radius; x <= radius; x++)
      if (x * x + y * y <= rr)
        PutPixel(fb, cx + x, cy + y, color);
}

static UiColor Snes15ToColor(uint16_t color) {
  return (UiColor){
    (uint8_t)((color & 31) * 255 / 31),
    (uint8_t)(((color >> 5) & 31) * 255 / 31),
    (uint8_t)(((color >> 10) & 31) * 255 / 31),
  };
}

static UiColor TintColor(UiColor color, unsigned area, bool dim) {
  const UiColor tint = kAreaColors[area < 6 ? area : 0];
  unsigned divisor = dim ? 510 : 255;
  return (UiColor){
    (uint8_t)(color.r * tint.r / divisor),
    (uint8_t)(color.g * tint.g / divisor),
    (uint8_t)(color.b * tint.b / divisor),
  };
}

static int Snes2bppColorIndex(const uint8_t *tile, int x, int y) {
  int bit = 7 - x;
  return ((tile[y * 2] >> bit) & 1) |
         (((tile[y * 2 + 1] >> bit) & 1) << 1);
}

static int Snes4bppColorIndex(const uint8_t *tile, int x, int y) {
  int bit = 7 - x;
  return ((tile[y * 2] >> bit) & 1) |
         (((tile[y * 2 + 1] >> bit) & 1) << 1) |
         (((tile[16 + y * 2] >> bit) & 1) << 2) |
         (((tile[16 + y * 2 + 1] >> bit) & 1) << 3);
}

static int MapBitIndex(int x, int y) {
  return ((x >> 3) & 3) + (x >> 5) * 128 + y * 4;
}

static const uint8_t *ExploredBitsForArea(unsigned area) {
  if (area > 5)
    return map_tiles_explored;
  return area == area_index ? map_tiles_explored
                            : (const uint8_t *)explored_map_tiles_saved + area * 256;
}

static bool TileBit(const uint8_t *bits, int x, int y) {
  if (!bits || (unsigned)x >= 64 || (unsigned)y >= 32)
    return false;
  return (bits[MapBitIndex(x, y)] & (0x80 >> (x & 7))) != 0;
}

static const uint8_t *MapStationBits(unsigned area) {
  if (area > 7 || !map_station_byte_array[area])
    return NULL;
  const uint8_t *table = RomPtr(0x829717 + area * 2);
  return RomPtr(0x820000 | GET_WORD(table));
}

static const uint16_t *AreaTilemap(unsigned area) {
  if (area > 7) area = 0;
  const uint8_t *p = RomPtr(0x82964a + area * 3);
  uint32_t address = p[0] | (p[1] << 8) | (p[2] << 16);
  return (const uint16_t *)RomPtr(address);
}

static UiColor AreaMapPixel(const uint8_t *tiles, const uint16_t *palette,
                            unsigned area, uint16_t entry, int px, int py,
                            bool dim) {
  unsigned tile_index = entry & 0x3ff;
  unsigned palette_row = (entry >> 10) & 7;
  if (entry & 0x4000) px = 7 - px;
  if (entry & 0x8000) py = 7 - py;
  const uint8_t *tile = tiles + tile_index * 32;
  int ci = Snes4bppColorIndex(tile, px, py);
  return TintColor(Snes15ToColor(palette[palette_row * 16 + ci]), area, dim);
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
  static const uint8_t UNDERSCORE[7] = {0,0,0,0,0,0,31};
  static const uint8_t EQUALS[7] = {0,0,31,0,31,0,0};
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
    case '_': return UNDERSCORE;
    case '=': return EQUALS;
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

static const uint16_t kAmmoTilemap[] = {
  0x344b, 0x3449, 0x744b, 0x344c, 0x344a, 0x744c,
  0x3434, 0x7434, 0x3435, 0x7435,
  0x3436, 0x7436, 0x3437, 0x7437,
};

static void DrawHudTile(uint8_t *fb, int x, int y, uint16_t entry) {
  const uint8_t *tiles = RomPtr(0x9ab200);
  const uint16_t *palette = (const uint16_t *)RomPtr(0x9a8000);
  int tx = entry & 0x3ff;
  int pal = (entry >> 10) & 7;
  const uint8_t *tile = tiles + tx * 16;
  for (int py = 0; py < 8; py++) {
    int sy = (entry & 0x8000) ? 7 - py : py;
    for (int px = 0; px < 8; px++) {
      int sx = (entry & 0x4000) ? 7 - px : px;
      int ci = Snes2bppColorIndex(tile, sx, sy);
      if (ci)
        PutPixel(fb, x + px, y + py, Snes15ToColor(palette[pal * 4 + ci]));
    }
  }
}

static void DrawAmmoIcon(uint8_t *fb, int x, int y, int icon) {
  int offset = icon == 0 ? 0 : (icon == 1 ? 6 : 10);
  int width = icon == 0 ? 3 : 2;
  for (int ty = 0; ty < 2; ty++)
    for (int tx = 0; tx < width; tx++)
      DrawHudTile(fb, x + tx * 8, y + ty * 8,
                  kAmmoTilemap[offset + ty * width + tx]);
}

static void DrawStatus(uint8_t *fb) {
  Panel(fb, 5, 3, 310, 38);

  const int tanks = samus_max_health / 100;
  const int filled = samus_health / 100;
  for (int i = 0; i < 14; i++) {
    int x = 17 + (i % 7) * 12;
    int y = 9 + (i / 7) * 12;
    UiColor color = i < filled ? kEnergy : (i < tanks ? kBorderHi : kBorder);
    FillRect(fb, x, y, 10, 9, color);
    if (i < tanks) {
      FillRect(fb, x, y, 10, 1, kWhite);
      FillRect(fb, x, y, 1, 9, kWhite);
    }
  }

  char text[16];
  snprintf(text, sizeof(text), "%02u", samus_health % 100);
  DrawText(fb, 111, 15, text, 2, kWhite);

  const unsigned counts[3] = {samus_missiles, samus_super_missiles, samus_power_bombs};
  const unsigned maximums[3] = {samus_max_missiles, samus_max_super_missiles, samus_max_power_bombs};
  const int slots[3] = {1, 2, 3};
  const int icon_x[3] = {151, 218, 269};
  const int icon_w[3] = {24, 16, 16};
  const int number_x[3] = {179, 239, 290};
  for (int i = 0; i < 3; i++) {
    if (!maximums[i]) continue;
    if (hud_item_index == slots[i]) {
      FillRect(fb, icon_x[i] - 2, 11, icon_w[i] + 4, 22, kSamus);
      StrokeRect(fb, icon_x[i] - 2, 11, icon_w[i] + 4, 22, 1, kWhite);
    }
    DrawAmmoIcon(fb, icon_x[i], 14, i);
    snprintf(text, sizeof(text), "%u", counts[i]);
    DrawText(fb, number_x[i], 15, text, 2, kWhite);
  }
}

static void DrawLine(uint8_t *fb, int x0, int y0, int x1, int y1, UiColor color) {
  int dx = x1 > x0 ? x1 - x0 : x0 - x1;
  int sx = x0 < x1 ? 1 : -1;
  int dy = y1 > y0 ? y0 - y1 : y1 - y0;
  int sy = y0 < y1 ? 1 : -1;
  int error = dx + dy;
  for (;;) {
    PutPixel(fb, x0, y0, color);
    if (x0 == x1 && y0 == y1) break;
    int twice = error * 2;
    if (twice >= dy) { error += dy; x0 += sx; }
    if (twice <= dx) { error += dx; y0 += sy; }
  }
}

static void DrawRoomMap(uint8_t *fb, int top, int bottom) {
  Panel(fb, 5, top, 310, bottom - top);
  const int window_w[] = {30, 24, 20, 16};
  const int window_h[] = {18, 15, 12, 10};
  int zoom = g_room_zoom;
  if (zoom < 0) zoom = 0;
  if (zoom > 3) zoom = 3;
  int cols = window_w[zoom], rows = window_h[zoom];
  int samus_tx = room_x_coordinate_on_map + (samus_x_pos >> 8);
  int samus_ty = room_y_coordinate_on_map + (samus_y_pos >> 8) + 1;
  int crop_x = samus_tx - cols / 2;
  int crop_y = samus_ty - rows / 2;
  if (crop_x < 0) crop_x = 0;
  if (crop_y < 0) crop_y = 0;
  if (crop_x > 64 - cols) crop_x = 64 - cols;
  if (crop_y > 32 - rows) crop_y = 32 - rows;

  int inner_w = 300, inner_h = bottom - top - 8;
  int scale_x = inner_w * 256 / (cols * 8);
  int scale_y = inner_h * 256 / (rows * 8);
  int scale = scale_x < scale_y ? scale_x : scale_y;
  int draw_w = cols * 8 * scale / 256;
  int draw_h = rows * 8 * scale / 256;
  int ox = 160 - draw_w / 2;
  int oy = top + (bottom - top - draw_h) / 2;
  const uint8_t *explored = ExploredBitsForArea(area_index);
  const uint8_t *station = MapStationBits(area_index);
  const uint16_t *tilemap = AreaTilemap(area_index);
  const uint8_t *tiles = RomPtr(0xb68000);
  const uint16_t *palette = (const uint16_t *)RomPtr(0xb6f000);

  g_room_map_x = ox;
  g_room_map_y = oy;
  g_room_map_w = draw_w;
  g_room_map_h = draw_h;
  g_room_crop_x = crop_x;
  g_room_crop_y = crop_y;
  g_room_cols = cols;
  g_room_rows = rows;

  FillRect(fb, ox, oy, draw_w, draw_h, (UiColor){20, 20, 30});
  for (int tile_y = 0; tile_y < rows; tile_y++) {
    int ty = crop_y + tile_y;
    int y0 = oy + tile_y * draw_h / rows;
    int y1 = oy + (tile_y + 1) * draw_h / rows;
    for (int tile_x = 0; tile_x < cols; tile_x++) {
      int tx = crop_x + tile_x;
      bool seen = TileBit(explored, tx, ty);
      bool station_only = !seen && TileBit(station, tx, ty);
      if (!seen && !station_only) continue;
      int x0 = ox + tile_x * draw_w / cols;
      int x1 = ox + (tile_x + 1) * draw_w / cols;
      int index = (tx >> 5) * 1024 + ty * 32 + (tx & 31);
      uint16_t entry = tilemap[index];
      for (int y = y0; y < y1; y++) {
        int py = (y - y0) * 8 / (y1 - y0);
        for (int x = x0; x < x1; x++) {
          int px = (x - x0) * 8 / (x1 - x0);
          PutPixel(fb, x, y, AreaMapPixel(tiles, palette, area_index,
                                          entry, px, py, station_only));
        }
      }
    }
  }
  for (int i = 0; i < g_marker_count; i++) {
    const MapMarker *m = &g_markers[i];
    if (m->area != area_index || m->x < crop_x || m->x >= crop_x + cols ||
        m->y < crop_y || m->y >= crop_y + rows)
      continue;
    int mx = ox + ((m->x - crop_x) * 8 + 4) * draw_w / (cols * 8);
    int my = oy + ((m->y - crop_y) * 8 + 4) * draw_h / (rows * 8);
    StrokeRect(fb, mx - 3, my - 3, 7, 7, 2, kAccent);
  }
  int dot_x = ox + ((samus_tx - crop_x) * 8 + 4) * draw_w / (cols * 8);
  int dot_y = oy + ((samus_ty - crop_y) * 8 + 4) * draw_h / (rows * 8);
  if (dot_x >= ox && dot_x < ox + draw_w && dot_y >= oy && dot_y < oy + draw_h)
    FillCircle(fb, dot_x, dot_y, 4, kSamus);
}

typedef struct WorldLayout {
  int min_x, min_y, max_x, max_y;
  int dest_x, dest_y;
} WorldLayout;

static const WorldLayout kWorldLayout[6] = {
  {6, 0, 57, 19, 6, 4}, {5, 0, 58, 20, 2, 12},
  {2, 0, 38, 18, 30, 30}, {10, 10, 22, 20, 43, 4},
  {10, 0, 43, 20, 28, 14}, {11, 9, 22, 22, 8, 13},
};

typedef struct WorldConnector { uint8_t a, ax, ay, b, bx, by; } WorldConnector;
static const WorldConnector kWorldConnectors[] = {
  {0,6,12,1,6,12}, {1,23,20,0,23,21}, {0,18,21,5,17,21},
  {5,17,13,0,17,13}, {1,34,16,0,34,11}, {1,36,19,4,30,21},
  {0,45,8,3,45,8}, {0,45,4,3,45,4}, {0,45,5,3,45,5},
  {0,43,7,3,43,7}, {3,54,8,0,49,8}, {0,52,14,4,52,14},
  {4,28,32,1,34,30},
};

static const int kWorldLabelPos[6][2] = {
  {25, 10}, {22, 24}, {45, 47}, {50, 8}, {42, 25}, {13, 21},
};

static bool WorldAreaVisible(int area) {
  const uint8_t *explored = ExploredBitsForArea(area);
  const uint8_t *station = MapStationBits(area);
  for (int i = 0; i < 256; i++)
    if (explored[i] || station[i])
      return true;
  return false;
}

static void IncludeWorldBounds(int x0, int y0, int x1, int y1,
                               int *min_x, int *min_y,
                               int *max_x, int *max_y) {
  if (x0 < *min_x) *min_x = x0;
  if (y0 < *min_y) *min_y = y0;
  if (x1 > *max_x) *max_x = x1;
  if (y1 > *max_y) *max_y = y1;
}

static void CenterWorldMap(int tile_scale, int top, int bottom,
                           int *origin_x, int *origin_y) {
  int min_x = 0x7fffffff, min_y = 0x7fffffff;
  int max_x = -0x7fffffff, max_y = -0x7fffffff;
  bool visible_area[6] = {false};

  /* Center the pixels which are actually visible, not the complete logical
   * 70x57 world canvas. Early in a game that canvas is mostly undiscovered,
   * which otherwise leaves Crateria/Brinstar stuck in its upper-left corner. */
  for (int area = 0; area < 6; area++) {
    const WorldLayout *layout = &kWorldLayout[area];
    const uint8_t *explored = ExploredBitsForArea(area);
    const uint8_t *station = MapStationBits(area);
    for (int ty = layout->min_y; ty < layout->max_y; ty++) {
      for (int tx = layout->min_x; tx < layout->max_x; tx++) {
        if (!TileBit(explored, tx, ty) && !TileBit(station, tx, ty))
          continue;
        int wx = layout->dest_x + tx - layout->min_x;
        int wy = layout->dest_y + ty - layout->min_y;
        IncludeWorldBounds(wx * tile_scale, wy * tile_scale,
                           (wx + 1) * tile_scale, (wy + 1) * tile_scale,
                           &min_x, &min_y, &max_x, &max_y);
        visible_area[area] = true;
      }
    }
    if (visible_area[area]) {
      const char *label = AreaName(area);
      int cx = kWorldLabelPos[area][0] * tile_scale;
      int cy = kWorldLabelPos[area][1] * tile_scale;
      int width = TextWidth(label, 1) + 8;
      IncludeWorldBounds(cx - width / 2, cy - 5,
                         cx + (width + 1) / 2, cy + 6,
                         &min_x, &min_y, &max_x, &max_y);
    }
  }

  for (unsigned i = 0;
       i < sizeof(kWorldConnectors) / sizeof(kWorldConnectors[0]); i++) {
    const WorldConnector *c = &kWorldConnectors[i];
    if (!visible_area[c->a] || !visible_area[c->b])
      continue;
    IncludeWorldBounds(c->ax * tile_scale, c->ay * tile_scale,
                       c->ax * tile_scale + 1, c->ay * tile_scale + 1,
                       &min_x, &min_y, &max_x, &max_y);
    IncludeWorldBounds(c->bx * tile_scale, c->by * tile_scale,
                       c->bx * tile_scale + 1, c->by * tile_scale + 1,
                       &min_x, &min_y, &max_x, &max_y);
  }

  if (max_x < min_x || max_y < min_y) {
    min_x = min_y = 0;
    max_x = 70 * tile_scale;
    max_y = 57 * tile_scale;
  }
  *origin_x = 160 - (min_x + max_x) / 2;
  *origin_y = top + (bottom - top) / 2 - (min_y + max_y) / 2;
}

static void DrawWorldMap(uint8_t *fb, int top, int bottom) {
  Panel(fb, 5, top, 310, bottom - top);
  int tile_scale = 2 + g_world_zoom;
  int ox, oy;
  CenterWorldMap(tile_scale, top, bottom, &ox, &oy);

  for (unsigned i = 0; i < sizeof(kWorldConnectors) / sizeof(kWorldConnectors[0]); i++) {
    const WorldConnector *c = &kWorldConnectors[i];
    if (!WorldAreaVisible(c->a) || !WorldAreaVisible(c->b)) continue;
    UiColor color = {
      (uint8_t)((kAreaColors[c->a].r + kAreaColors[c->b].r) / 5),
      (uint8_t)((kAreaColors[c->a].g + kAreaColors[c->b].g) / 5),
      (uint8_t)((kAreaColors[c->a].b + kAreaColors[c->b].b) / 5),
    };
    DrawLine(fb, ox + c->ax * tile_scale, oy + c->ay * tile_scale,
             ox + c->bx * tile_scale, oy + c->by * tile_scale, color);
  }

  for (int area = 0; area < 6; area++) {
    const WorldLayout *layout = &kWorldLayout[area];
    const uint8_t *explored = ExploredBitsForArea(area);
    const uint8_t *station = MapStationBits(area);
    const uint16_t *tilemap = AreaTilemap(area);
    const uint8_t *tiles = RomPtr(0xb68000);
    const uint16_t *palette = (const uint16_t *)RomPtr(0xb6f000);
    for (int ty = layout->min_y; ty < layout->max_y; ty++) {
      for (int tx = layout->min_x; tx < layout->max_x; tx++) {
        bool seen = TileBit(explored, tx, ty);
        bool station_only = !seen && TileBit(station, tx, ty);
        if (!seen && !station_only) continue;
        int dx0 = ox + (layout->dest_x + tx - layout->min_x) * tile_scale;
        int dy0 = oy + (layout->dest_y + ty - layout->min_y) * tile_scale;
        int index = (tx >> 5) * 1024 + ty * 32 + (tx & 31);
        uint16_t entry = tilemap[index];
        for (int py = 0; py < tile_scale; py++)
          for (int px = 0; px < tile_scale; px++)
            PutPixel(fb, dx0 + px, dy0 + py,
                     AreaMapPixel(tiles, palette, area, entry,
                                  px * 8 / tile_scale,
                                  py * 8 / tile_scale, station_only));
      }
    }
  }

  for (int i = 0; i < g_marker_count; i++) {
    const MapMarker *m = &g_markers[i];
    if (m->area >= 6) continue;
    const WorldLayout *layout = &kWorldLayout[m->area];
    if (m->x < layout->min_x || m->x >= layout->max_x ||
        m->y < layout->min_y || m->y >= layout->max_y)
      continue;
    int mx = ox + (layout->dest_x + m->x - layout->min_x) * tile_scale;
    int my = oy + (layout->dest_y + m->y - layout->min_y) * tile_scale;
    StrokeRect(fb, mx - 2, my - 2, 5, 5, 1, kAccent);
  }

  for (int area = 0; area < 6; area++) {
    if (!WorldAreaVisible(area)) continue;
    int x = ox + kWorldLabelPos[area][0] * tile_scale;
    int y = oy + kWorldLabelPos[area][1] * tile_scale;
    const char *label = AreaName(area);
    int w = TextWidth(label, 1) + 8;
    FillRect(fb, x - w / 2, y - 5, w, 11, kBg);
    StrokeRect(fb, x - w / 2, y - 5, w, 11, 1, kBorder);
    DrawTextCentered(fb, x, y - 3, label, 1, kWhite);
  }
}

static void DrawMapControls(uint8_t *fb) {
  int y = 183;
  for (int i = 0; i < 3; i++) {
    int x = 5 + i * 104;
    FillRect(fb, x, y, 100, 23, kPanel);
    StrokeRect(fb, x, y, 100, 23, 2, i == 0 ? kBorderHi : kBorder);
  }
  if (g_world_view) {
    StrokeRect(fb, 45, y + 6, 12, 10, 2, kAccent);
    StrokeRect(fb, 39, y + 4, 12, 10, 2, kWhite);
  } else {
    StrokeRect(fb, 42, y + 5, 16, 13, 2, kAccent);
    StrokeRect(fb, 47, y + 8, 6, 6, 1, kWhite);
  }
  FillRect(fb, 146, y + 10, 28, 3, kAccent);
  FillRect(fb, 251, y + 10, 28, 3, kAccent);
  FillRect(fb, 263, y - 2 + 1, 3, 25, kAccent);
}

static void DrawMapTab(uint8_t *fb, int top) {
  if (g_world_view) DrawWorldMap(fb, top, 180);
  else DrawRoomMap(fb, top, 180);
  DrawMapControls(fb);
}

static int ItemPercent(void) {
  const uint16_t *ram_addrs = (const uint16_t *)RomPtr(0x8be70d);
  const uint16_t *divisors = (const uint16_t *)RomPtr(0x8be717);
  const uint16_t *item_masks = (const uint16_t *)RomPtr(0x8be721);
  const uint16_t *beam_masks = (const uint16_t *)RomPtr(0x8be737);
  int total = 0;
  for (int i = 4; i >= 0; i--) {
    uint16_t addr = ram_addrs[i];
    uint8_t divisor = divisors[i];
    if (addr < 0x2000)
      total += divisor ? GET_WORD(g_ram + addr) / divisor : 0xffff;
  }
  for (int i = 0; i < 11; i++) total += !!(collected_items & item_masks[i]);
  for (int i = 0; i < 5; i++) total += !!(collected_beams & beam_masks[i]);
  return total;
}

static void DrawItemLine(uint8_t *fb, int x, int y, const char *label, bool collected, bool equipped) {
  UiColor text = collected ? kWhite : kDim;
  UiColor mark = equipped ? kAccent : (collected ? kBorderHi : kBorder);
  if (collected) FillCircle(fb, x + 2, y + 4, 3, mark);
  else StrokeRect(fb, x, y + 1, 6, 6, 1, mark);
  DrawText(fb, x + 9, y, label, 1, text);
}

static void DrawReduxSuit(uint8_t *fb, int x, int y, int w, int h) {
  int key = equipped_items & 0x101;
  int variant = key == 0x100 ? 1 : (key == 0x001 ? 2 : (key == 0x101 ? 3 : 0));
  const uint16_t *body = kReduxPalettePower;
  if (equipped_items & 0x20) body = kReduxPaletteGravity;
  else if (equipped_items & 0x01) body = kReduxPaletteVaria;
  const uint8_t *tiles = (const uint8_t *)kReduxTiles;
  for (int dy = 0; dy < h; dy++) {
    int sy = dy * 136 / h;
    int ty = sy >> 3, py = sy & 7;
    for (int dx = 0; dx < w; dx++) {
      int sx = dx * 64 / w;
      int tx = sx >> 3, px = sx & 7;
      uint16_t entry = kReduxTilemaps[variant][ty * 8 + tx];
      int tile_index = entry & 0x3ff;
      int palette_row = (entry >> 10) & 7;
      int source_x = (entry & 0x4000) ? 7 - px : px;
      int source_y = (entry & 0x8000) ? 7 - py : py;
      if (tile_index >= 116 || palette_row >= 4) continue;
      int ci = Snes4bppColorIndex(tiles + tile_index * 32, source_x, source_y);
      if (!ci) continue;
      uint16_t color = palette_row == 1 ? body[ci] : kReduxPalette[palette_row * 16 + ci];
      PutPixel(fb, x + dx, y + dy, Snes15ToColor(color));
    }
  }
}

static void DrawItemsTab(uint8_t *fb, int top) {
  Panel(fb, 5, top, 310, 207 - top);
  char text[32];
  int stat_y = top + (top == 44 ? 6 : 25);
  FillRect(fb, 11, stat_y, 107, 24, kSlot);
  StrokeRect(fb, 11, stat_y, 107, 24, 2, kBorder);
  DrawText(fb, 17, stat_y + 3, "ITEMS", 1, kDim);
  snprintf(text, sizeof(text), "%d.0%%", ItemPercent());
  DrawText(fb, 17, stat_y + 13, text, 1, kWhite);
  FillRect(fb, 202, stat_y, 107, 24, kSlot);
  StrokeRect(fb, 202, stat_y, 107, 24, 2, kBorder);
  DrawText(fb, 208, stat_y + 3, "TIME", 1, kDim);
  snprintf(text, sizeof(text), "%02u:%02u:%02u", game_time_hours, game_time_minutes, game_time_seconds);
  DrawText(fb, 208, stat_y + 13, text, 1, kWhite);

  int body_y = stat_y + (top == 44 ? 28 : 29);
  FillRect(fb, 11, body_y, 112, 58, kSlot);
  StrokeRect(fb, 11, body_y, 112, 58, 2, kBorder);
  DrawText(fb, 17, body_y + 4, "SUIT", 1, kAccent);
  DrawItemLine(fb, 17, body_y + 16, "VARIA SUIT", collected_items & 0x0001, equipped_items & 0x0001);
  DrawItemLine(fb, 17, body_y + 28, "GRAVITY SUIT", collected_items & 0x0020, equipped_items & 0x0020);

  FillRect(fb, 11, body_y + 62, 112, 62, kSlot);
  StrokeRect(fb, 11, body_y + 62, 112, 62, 2, kBorder);
  DrawText(fb, 17, body_y + 66, "MISC.", 1, kAccent);
  DrawItemLine(fb, 17, body_y + 78, "MORPHING BALL", collected_items & 0x0004, equipped_items & 0x0004);
  DrawItemLine(fb, 17, body_y + 89, "BOMB", collected_items & 0x1000, equipped_items & 0x1000);
  DrawItemLine(fb, 17, body_y + 100, "SPRING BALL", collected_items & 0x0002, equipped_items & 0x0002);
  DrawItemLine(fb, 17, body_y + 111, "SCREW ATTACK", collected_items & 0x0008, equipped_items & 0x0008);

  DrawReduxSuit(fb, 132, body_y, 56, 124);

  FillRect(fb, 197, body_y, 112, 58, kSlot);
  StrokeRect(fb, 197, body_y, 112, 58, 2, kBorder);
  DrawText(fb, 203, body_y + 4, "BOOTS", 1, kAccent);
  DrawItemLine(fb, 203, body_y + 16, "HI-JUMP BOOTS", collected_items & 0x0100, equipped_items & 0x0100);
  DrawItemLine(fb, 203, body_y + 28, "SPACE JUMP", collected_items & 0x0200, equipped_items & 0x0200);
  DrawItemLine(fb, 203, body_y + 40, "SPEED BOOSTER", collected_items & 0x2000, equipped_items & 0x2000);

  FillRect(fb, 197, body_y + 62, 112, 62, kSlot);
  StrokeRect(fb, 197, body_y + 62, 112, 62, 2, kBorder);
  DrawText(fb, 203, body_y + 66, "BEAM", 1, kAccent);
  DrawItemLine(fb, 203, body_y + 78, "CHARGE", collected_beams & 0x1000, equipped_beams & 0x1000);
  DrawItemLine(fb, 203, body_y + 87, "ICE", collected_beams & 0x0002, equipped_beams & 0x0002);
  DrawItemLine(fb, 203, body_y + 96, "WAVE", collected_beams & 0x0001, equipped_beams & 0x0001);
  DrawItemLine(fb, 203, body_y + 105, "SPAZER", collected_beams & 0x0004, equipped_beams & 0x0004);
  DrawItemLine(fb, 203, body_y + 114, "PLASMA", collected_beams & 0x0008, equipped_beams & 0x0008);
}

static void DrawSetupRow(uint8_t *fb, int y, int height, const char *label,
                         const char *value, bool enabled) {
  FillRect(fb, 13, y, 294, height, kSlot);
  StrokeRect(fb, 13, y, 294, height, 2, kBorder);
  int text_y = y + (height - 7) / 2;
  DrawText(fb, 21, text_y, label, 1, kWhite);
  if (value)
    DrawText(fb, 299 - TextWidth(value, 1), text_y, value, 1,
             enabled ? kAccent : kDim);
}

static void DrawBuildFlags(uint8_t *fb, int y, int max_lines) {
  const char *flags = SM3DS_BUILD_FLAGS;
  if (!*flags) {
    DrawText(fb, 20, y, "NONE", 1, kDim);
    return;
  }

  for (int line = 0; line < max_lines && *flags; line++) {
    while (*flags == ' ') flags++;
    if (!*flags) break;

    int count = 0;
    while (flags[count] && count < 47) count++;
    if (flags[count]) {
      int word_end = count;
      while (word_end > 0 && flags[word_end] != ' ') word_end--;
      if (word_end > 0) count = word_end;
    }

    char text[48];
    memcpy(text, flags, count);
    text[count] = '\0';
    if (line == max_lines - 1 && flags[count]) {
      int dots = count > 44 ? 44 : count;
      memcpy(text + dots, "...", 4);
    }
    DrawText(fb, 20, y + line * 13, text, 1, kWhite);
    flags += count;
  }
}

static void DrawBuildSetting(uint8_t *fb, int x, int y, const char *name,
                             bool enabled) {
  const char *value = enabled ? "ON" : "OFF";
  DrawText(fb, x, y, name, 1, kWhite);
  DrawText(fb, x + 135 - TextWidth(value, 1), y, value, 1,
           enabled ? kAccent : kDim);
}

static void DrawSetupTab(uint8_t *fb, int top) {
  bool compact = top == 44;
  Panel(fb, 5, top, 310, 207 - top);
  DrawText(fb, 21, top + (compact ? 7 : 13),
           g_setup_build_info ? "BUILD INFO" : "SETUP", 1, kWhite);
  int info_y = top + (compact ? 3 : 7);
  int info_height = compact ? 16 : 20;
  FillRect(fb, 281, info_y, 26, info_height, g_setup_build_info ? kBorder : kSlot);
  StrokeRect(fb, 281, info_y, 26, info_height, 2,
             g_setup_build_info ? kAccent : kBorder);
  FillRect(fb, 293, info_y + 3, 3, 3, kWhite);
  FillRect(fb, 293, info_y + 8, 3, info_height - 11, kWhite);

  if (g_setup_build_info) {
    int flags_y = top + (compact ? 25 : 32);
    int flags_height = compact ? 50 : 76;
    FillRect(fb, 13, flags_y, 294, flags_height, kSlot);
    StrokeRect(fb, 13, flags_y, 294, flags_height, 2, kBorder);
    DrawText(fb, 20, flags_y + 6, "BUILD_FLAGS", 1, kAccent);
    DrawBuildFlags(fb, flags_y + 18, compact ? 2 : 4);
    DrawText(fb, 20, top + (compact ? 80 : 119), "PORT DEFINES", 1, kAccent);
#ifdef SM3DS_OLD3DS
    DrawBuildSetting(fb, 18, top + (compact ? 94 : 133), "OLD3DS", true);
#else
    DrawBuildSetting(fb, 18, top + (compact ? 94 : 133), "OLD3DS", false);
#endif
#ifdef SM3DS_PROFILE
    DrawBuildSetting(fb, 166, top + (compact ? 94 : 133), "PROFILE", true);
#else
    DrawBuildSetting(fb, 166, top + (compact ? 94 : 133), "PROFILE", false);
#endif
#ifdef SM3DS_DOOR_TRACE
    DrawBuildSetting(fb, 18, top + (compact ? 107 : 146), "DOOR_TRACE", true);
#else
    DrawBuildSetting(fb, 18, top + (compact ? 107 : 146), "DOOR_TRACE", false);
#endif
#ifdef SM3DS_DISABLE_PICA
    DrawBuildSetting(fb, 166, top + (compact ? 107 : 146), "DISABLE_PICA", true);
#else
    DrawBuildSetting(fb, 166, top + (compact ? 107 : 146), "DISABLE_PICA", false);
#endif
#ifdef SM3DS_EMULATED_CPU
    DrawBuildSetting(fb, 18, top + (compact ? 120 : 159), "EMULATED_CPU", true);
#else
    DrawBuildSetting(fb, 18, top + (compact ? 120 : 159), "EMULATED_CPU", false);
#endif
    DrawText(fb, 20, top + (compact ? 136 : 174), "BUILD VARIABLES", 1, kAccent);
#ifdef FULL_NATIVE
    DrawBuildSetting(fb, 18, top + (compact ? 149 : 188), "FULL_NATIVE", true);
#else
    DrawBuildSetting(fb, 18, top + (compact ? 149 : 188), "FULL_NATIVE", false);
#endif
    DrawBuildSetting(fb, 166, top + (compact ? 149 : 188), "LTO", SM3DS_BUILD_LTO != 0);
    return;
  }

  int y = top + (compact ? 22 : 32);
  int step = compact ? 23 : 28;
  int height = compact ? 20 : 24;
  DrawSetupRow(fb, y, height, "STATUS BAR MAP",
               g_status_bar_visible[kBottomTab_Map] ? "ON" : "OFF",
               g_status_bar_visible[kBottomTab_Map]);
  DrawSetupRow(fb, y + step, height, "STATUS BAR ITEMS",
               g_status_bar_visible[kBottomTab_Items] ? "ON" : "OFF",
               g_status_bar_visible[kBottomTab_Items]);
  DrawSetupRow(fb, y + 2 * step, height, "STATUS BAR SETUP",
               g_status_bar_visible[kBottomTab_Setup] ? "ON" : "OFF",
               g_status_bar_visible[kBottomTab_Setup]);
  DrawSetupRow(fb, y + 3 * step, height, "WIDESCREEN",
               g_widescreen ? "ON" : "OFF", g_widescreen);
  DrawSetupRow(fb, y + 4 * step, height, "HIDE MAIN HUD",
               g_hide_main_hud ? "ON" : "OFF", g_hide_main_hud);
  DrawSetupRow(fb, y + 5 * step, height, "CLEAR MAP MARKERS",
               g_clear_markers_armed ? "TAP AGAIN" : NULL,
               g_clear_markers_armed);
}

static void DrawTabs(uint8_t *fb) {
  static const char *labels[] = {"MAP", "ITEMS", "SETUP"};
  for (int i = 0; i < 3; i++) {
    const int x = 5 + i * 104;
    UiColor fill = g_bottom_tab == i ? kBorder : kPanel;
    UiColor border = g_bottom_tab == i ? kAccent : kBorder;
    FillRect(fb, x, 210, 100, 27, fill);
    StrokeRect(fb, x, 210, 100, 27, 2, border);
    DrawTextCentered(fb, x + 50, 220, labels[i], 1, kWhite);
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

  /* The UI data changes slowly, so rebuild its texture at 7.5 Hz. The GPU
   * still presents the cached texture every frame to keep both LCD buffers
   * synchronized. */
  bool redraw = g_bottom_dirty || ((g_bottom_frame++ & 7) == 0);
  if (!redraw)
    return false;

  g_bottom_dirty = false;
  if (!IsLiveGameplay()) {
    DrawIdle(g_bottom_cache);
  } else {
    FillRect(g_bottom_cache, 0, 0, 320, 240, kBg);
    bool status_bar = g_status_bar_visible[g_bottom_tab];
    if (status_bar)
      DrawStatus(g_bottom_cache);
    if (g_bottom_tab == kBottomTab_Map) {
      DrawMapTab(g_bottom_cache, status_bar ? 44 : 3);
    } else if (g_bottom_tab == kBottomTab_Items) {
      DrawItemsTab(g_bottom_cache, status_bar ? 44 : 3);
    } else {
      DrawSetupTab(g_bottom_cache, status_bar ? 44 : 3);
    }
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

static void HandleStatusTouch(int x) {
  int slot = 0;
  if (x >= 148 && x < 216 && samus_max_missiles) slot = 1;
  else if (x >= 216 && x < 266 && samus_max_super_missiles) slot = 2;
  else if (x >= 266 && samus_max_power_bombs) slot = 3;
  if (slot) {
    hud_item_index = hud_item_index == slot ? 0 : slot;
    samus_auto_cancel_hud_item_index = 0;
    hud_auto_cancel_flag = 0;
    g_bottom_dirty = true;
  }
}

void BottomScreen_HandleTouch(float normalized_x, float normalized_y) {
  int x = (int)(normalized_x * 320.0f);
  int y = (int)(normalized_y * 240.0f);
  g_touch_down_ms = osGetTime();
  g_touch_down_x = x;
  g_touch_down_y = y;
  if (y >= 209) {
    if (x < 107) g_bottom_tab = kBottomTab_Map;
    else if (x < 213) g_bottom_tab = kBottomTab_Items;
    else {
      if (g_bottom_tab != kBottomTab_Setup) g_setup_build_info = false;
      g_bottom_tab = kBottomTab_Setup;
    }
    g_bottom_dirty = true;
    return;
  }
  if (!IsLiveGameplay())
    return;

  if (g_status_bar_visible[g_bottom_tab] && y < 42) {
    HandleStatusTouch(x);
    return;
  }

  if (g_bottom_tab == kBottomTab_Map && y >= 182 && y < 208) {
    if (x < 107) {
      g_world_view = !g_world_view;
    } else if (x < 213) {
      if (g_world_view) { if (g_world_zoom > 0) g_world_zoom--; }
      else if (g_room_zoom > 0) g_room_zoom--;
    } else {
      if (g_world_view) { if (g_world_zoom < 2) g_world_zoom++; }
      else if (g_room_zoom < 3) g_room_zoom++;
    }
    g_bottom_dirty = true;
    return;
  }

  if (g_bottom_tab == kBottomTab_Setup) {
    int top = g_status_bar_visible[kBottomTab_Setup] ? 44 : 3;
    bool compact = top == 44;
    int info_y = top + (compact ? 3 : 7);
    if (x >= 281 && x < 307 && y >= info_y &&
        y < info_y + (compact ? 16 : 20)) {
      g_setup_build_info = !g_setup_build_info;
      g_bottom_dirty = true;
      return;
    }
    if (g_setup_build_info)
      return;
    int row_y = top + (compact ? 22 : 32);
    int step = compact ? 23 : 28;
    int height = compact ? 20 : 24;
    if (x >= 13 && x < 307 && y >= row_y) {
      int row = (y - row_y) / step;
      if (row < 6 && y < row_y + row * step + height) {
        if (row < 3) g_status_bar_visible[row] = !g_status_bar_visible[row];
        else if (row == 3) g_widescreen = !g_widescreen;
        else if (row == 4) g_hide_main_hud = !g_hide_main_hud;
        else if (g_clear_markers_armed) {
          g_marker_count = 0;
          g_clear_markers_armed = false;
        } else {
          g_clear_markers_armed = true;
        }
      }
    }
    g_bottom_dirty = true;
    return;
  }

}

void BottomScreen_HandleTouchUp(float normalized_x, float normalized_y) {
  int x = (int)(normalized_x * 320.0f);
  int y = (int)(normalized_y * 240.0f);
  if (!IsLiveGameplay() || g_bottom_tab != kBottomTab_Map || g_world_view ||
      osGetTime() - g_touch_down_ms < 550 ||
      (x - g_touch_down_x) * (x - g_touch_down_x) +
          (y - g_touch_down_y) * (y - g_touch_down_y) > 64 ||
      x < g_room_map_x || x >= g_room_map_x + g_room_map_w ||
      y < g_room_map_y || y >= g_room_map_y + g_room_map_h)
    return;
  int tx = g_room_crop_x + (x - g_room_map_x) * g_room_cols / g_room_map_w;
  int ty = g_room_crop_y + (y - g_room_map_y) * g_room_rows / g_room_map_h;
  for (int i = 0; i < g_marker_count; i++) {
    if (g_markers[i].area == area_index && g_markers[i].x == tx && g_markers[i].y == ty) {
      memmove(&g_markers[i], &g_markers[i + 1],
              (g_marker_count - i - 1) * sizeof(g_markers[0]));
      g_marker_count--;
      g_bottom_dirty = true;
      return;
    }
  }
  if (g_marker_count < (int)(sizeof(g_markers) / sizeof(g_markers[0]))) {
    g_markers[g_marker_count++] = (MapMarker){area_index, tx, ty};
    g_bottom_dirty = true;
  }
}

bool BottomScreen_HideMainHud(void) {
  return g_hide_main_hud;
}

bool BottomScreen_WidescreenEnabled(void) {
  return g_widescreen;
}
