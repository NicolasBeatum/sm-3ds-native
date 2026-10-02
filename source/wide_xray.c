#include "wide_xray.h"
#include "wide_config.h"
#include "src/ida_types.h"
#include "src/variables.h"
#include "src/funcs.h"
#include "src/sm_rtl.h"
#include <string.h>

static struct {
  bool valid;
  uint16_t room, cameraX, cameraY;
  int startX[2], startY;
  uint16_t tiles[2][1024], backup[2048];
} g_xray;

static void NativeWindow(int x, int y, uint16_t *table) {
  for (unsigned i = 0; i < 256; i++) table[i] = 0xff;
  bool left = samus_pose_x_dir == 4;
  if ((x < 0 && left) || (x >= 256 && !left)) return;
  CalculateXrayHdmaTableInner(x, y, xray_angle, demo_input,
                             x < 0 || x >= 256, table);
}

static void IncludeFragment(int packed, int offset, int lo, int hi,
                             int *left, int *right) {
  int l = (packed & 255) + offset, r = (packed >> 8) + offset;
  if (l < lo) l = lo;
  if (r > hi) r = hi;
  if (l > r) return;
  if (l < *left) *left = l;
  if (r > *right) *right = r;
}

static void Windows(PicaFrame *f) {
  uint16_t side[2][256];
  int x = (int)samus_x_pos - (int)layer1_x_pos + (samus_pose_x_dir == 4 ? -3 : 3);
  int y = (int)samus_y_pos - (int)layer1_y_pos - (samus_movement_type == 5 ? 12 : 16);
  bool project = demo_input_pre_instr <= 2 && y >= 1 && y < 230 &&
                 xray_angle <= 256 && demo_input <= 10;
  if (project) {
    NativeWindow(x + kWideExtraX, y, side[0]);
    NativeWindow(x - kWideExtraX, y, side[1]);
  }
  for (unsigned row = 0; row < f->height; row++) {
    const PicaLine *p = &f->lines[row];
    int left = p->window2left, right = p->window2right;
    if (project && ((p->windowsel >> 20) & 8)) {
      int ll = 400, lr = -1, rl = 400, rr = -1;
      /* Three translated native tables cover all 400 pixels. The middle
       * fragment is the captured SNES window, never a fitted/recreated cone. */
      IncludeFragment(side[0][row], -72, -72, -1, &ll, &lr);
      IncludeFragment(side[1][row], 72, 256, 327, &rl, &rr);
      if (left <= right) {
        /* Off-screen native ramps have a one-line phase difference at some
         * angles. Extend only a clamped endpoint, never bridge a gap across
         * unscanned native pixels while the beam starts widening. */
        if (left == 0 && ll <= lr && lr == -1) left = ll;
        if (right == 255 && rl <= rr && rl == 256) right = rr;
      } else if (ll <= lr && (rl > rr || samus_pose_x_dir == 4)) {
        left = ll; right = lr;
      } else if (rl <= rr) {
        left = rl; right = rr;
      }
    }
    f->xrayLeft[row] = left;
    f->xrayRight[row] = right;
  }
}

static void PutOverride(int startX, int startY, int x, int y, unsigned block) {
  int col = x - startX / 16, row = y - startY / 16;
  if (col < 0 || col >= 16 || row < 0 || row >= 16) return;
  const uint16_t *src = (const uint16_t *)(g_ram + 0xa000) + (block & 1023) * 4;
  uint16_t *dst = (uint16_t *)(g_ram + 0x4000) + row * 64 + col * 2;
  /* Match LoadBlockToXrayTilemap, including its vertical row swap. */
  unsigned flip = (block & 0x800) ? 2 : 0;
  dst[0] = src[flip]; dst[1] = src[flip + 1];
  dst[32] = src[flip ^ 2]; dst[33] = src[(flip ^ 2) + 1];
}

static void ItemOverrides(int startX, int startY) {
  const uint16_t *drawing = (const uint16_t *)RomPtr(0x84839d);
  for (int i = 39; i >= 0; i--) {
    if (plm_header_ptr[i] < FUNC16(PlmPreInstr_GotoLinkIfTriggered)) continue;
    unsigned item = plm_room_arguments[i];
    if (item >= 256 || (item_bit_array[item >> 3] & (1u << (item & 7)))) continue;
    unsigned block = plm_block_indices[i] / 2;
    const uint8_t *p = RomPtr_84(drawing[plm_variables[i] >> 1]);
    PutOverride(startX, startY, block % room_width_in_blocks,
                 block / room_width_in_blocks, GET_WORD(p + 2) & 0xfff);
  }
  const RoomDefRoomstate *state = get_RoomDefRoomstate(roomdefroomstate_ptr);
  if (!state->xray_special_casing_ptr) return;
  const XraySpecialCasing *p = (const XraySpecialCasing *)RomPtr_8F(state->xray_special_casing_ptr);
  for (; p->x_block || p->y_block; p++)
    PutOverride(startX, startY, p->x_block, p->y_block, p->level_data_block);
}

static void SideMap(int side) {
  int startX = g_xray.startX[side], startY = g_xray.startY;
  uint16_t *dst = (uint16_t *)(g_ram + 0x4000);
  const uint16_t *tiles = (const uint16_t *)(g_ram + 0xa000);
  memset(dst, 0, 4096);
  for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) {
    int bx = startX / 16 + x, by = startY / 16 + y;
    if (bx < 0 || by < 0 || bx >= room_width_in_blocks || by >= room_height_in_blocks) continue;
    unsigned block = level_data[by * room_width_in_blocks + bx];
    unsigned flip = ((block & 0x400) ? 1 : 0) | ((block & 0x800) ? 2 : 0);
    unsigned flags = ((block & 0x400) ? 0x4000 : 0) | ((block & 0x800) ? 0x8000 : 0);
    unsigned pos = y * 64 + x * 2;
    dst[pos] = tiles[(block & 1023) * 4 + (0 ^ flip)] ^ flags;
    dst[pos+1] = tiles[(block & 1023) * 4 + (1 ^ flip)] ^ flags;
    dst[pos+32] = tiles[(block & 1023) * 4 + (2 ^ flip)] ^ flags;
    dst[pos+33] = tiles[(block & 1023) * 4 + (3 ^ flip)] ^ flags;
  }
  /* Reuse the game's block/BTS reveal rules in its scratch tilemap. Save and
   * restore all 4 KiB they can write; this is not uploaded to native VRAM. */
  for (int y = 0; y < 16; y++) {
    int by = startY / 16 + y;
    if (by < 0 || by >= room_height_in_blocks) continue;
    int bx = startX / 16;
    if (bx > 0 && bx < room_width_in_blocks)
      Xray_SetupStage4_Func2(y * 128, by * room_width_in_blocks + bx);
    for (int x = 0; x < 16; x++) {
      bx = startX / 16 + x;
      if (bx < 0 || bx >= room_width_in_blocks) continue;
      Xray_SetupStage4_Func3(16 - x, y * 128 + x * 4, by * room_width_in_blocks + bx);
    }
  }
  ItemOverrides(startX, startY);
  memcpy(g_xray.tiles[side], dst, sizeof(g_xray.tiles[side]));
}

void WideXray_Prepare(PicaFrame *f) {
  f->xrayTiles[0] = f->xrayTiles[1] = NULL;
  f->xrayActive = f->width == kWideWidth && time_is_frozen_flag && (reg_WOBJSEL & 0x80);
  if (!f->xrayActive) { g_xray.valid = false; return; }
  bool xrayObject = false;
  for (unsigned i = 0; i < 6; i++)
    if (hdma_object_channels_bitmask[i] && hdma_object_pre_instruction_bank[i] == 0x88 &&
        hdma_object_pre_instructions[i] == (fnHdmaobjPreInstr_Xray & 0xffff))
      xrayObject = true;
  f->xrayActive &= xrayObject;
  if (!f->xrayActive) { g_xray.valid = false; return; }
  Windows(f);
  if (demo_input_pre_instr > 2 || !CanXrayShowBlocks() || !room_width_in_blocks ||
      !room_height_in_blocks ||
      (uint32_t)room_width_in_blocks * room_height_in_blocks > 0x9600 / 2) return;
  if (!g_xray.valid || g_xray.room != room_ptr ||
      g_xray.cameraX != layer1_x_pos || g_xray.cameraY != layer1_y_pos) {
    g_xray.room = room_ptr; g_xray.cameraX = layer1_x_pos; g_xray.cameraY = layer1_y_pos;
    g_xray.startX[0] = ((int)layer1_x_pos / 16 - 5) * 16;
    g_xray.startX[1] = ((int)layer1_x_pos / 16 + 16) * 16;
    g_xray.startY = (int)layer1_y_pos / 16 * 16;
    memcpy(g_xray.backup, g_ram + 0x4000, sizeof(g_xray.backup));
    SideMap(0); SideMap(1);
    memcpy(g_ram + 0x4000, g_xray.backup, sizeof(g_xray.backup));
    g_xray.valid = true;
  }
  for (unsigned i = 0; i < 2; i++) {
    f->xrayTiles[i] = g_xray.tiles[i];
    f->xrayStartX[i] = g_xray.startX[i];
  }
  f->xrayStartY = g_xray.startY;
  /* BG2 temporarily contains the scanned BG1 image. Its old room/parallax
   * coordinates must not be used for the extended scan. */
  f->wideRoom[1] = f->wideRoom[0];
  f->wideRoom[1].followScroll = false;
}
