/* Integration check with a user-supplied ROM and an active X-Ray WRAM dump.
 * Neither input is included in the repository. See docs/xray-widescreen-trial.md. */
#include "wide_xray.h"
#include "wide_config.h"
#include "src/ida_types.h"
#include "src/variables.h"
#include "src/funcs.h"
#include "src/sm_rtl.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

uint8 g_ram[0x20000];
const uint8 *g_rom;
const uint8 *RomPtr(uint32 address) {
  return g_rom + (((address >> 1) & 0x3f8000) | (address & 0x7fff));
}
bool Unreachable(void) { abort(); }
static uint8 backup[sizeof(g_ram)];

static void AssertCenter(const PicaFrame *frame, const PicaLine *lines) {
  for (unsigned y = 0; y < kSnesHeight; y++)
    for (int x = 0; x < kSnesWidth; x++)
      assert((x >= lines[y].window2left && x <= lines[y].window2right) ==
             (x >= frame->xrayLeft[y] && x <= frame->xrayRight[y]));
}

int main(int argc, char **argv) {
  assert(argc == 3);
  FILE *file = fopen(argv[1], "rb");
  assert(file);
  fseek(file, 0, SEEK_END);
  long size = ftell(file);
  assert(size > 0);
  rewind(file);
  uint8 *rom = malloc(size);
  assert(rom && fread(rom, 1, size, file) == (size_t)size);
  fclose(file);
  g_rom = rom + (size % 32768 == 512 ? 512 : 0);
  file = fopen(argv[2], "rb");
  assert(file && fread(g_ram, 1, sizeof(g_ram), file) == sizeof(g_ram));
  fclose(file);
  memcpy(backup, g_ram, sizeof(g_ram));

  PicaLine lines[kSnesHeight] = {0};
  for (unsigned y = 0; y < kSnesHeight; y++) {
    lines[y].windowsel = (reg_WOBJSEL >> 4) << 20;
    lines[y].window2left = hdma_table_1[y] & 255;
    lines[y].window2right = hdma_table_1[y] >> 8;
  }
  PicaFrame frame = {
    .width = kWideWidth, .height = kSnesHeight, .originX = kWideExtraX,
    .lines = lines,
    .wideRoom = {{.blocks = level_data, .cameraX = layer1_x_pos,
                  .cameraY = layer1_y_pos}}
  };
  WideXray_Prepare(&frame);
  assert(frame.xrayActive && frame.xrayTiles[0] && frame.xrayTiles[1]);
  assert(!memcmp(backup, g_ram, sizeof(g_ram)));
  AssertCenter(&frame, lines);
  unsigned extended = 0;
  for (unsigned y = 0; y < kSnesHeight; y++)
    if (frame.xrayLeft[y] < 0 || frame.xrayRight[y] > 255) extended++;
  assert(extended);
  printf("Actual X-Ray dump: %u extended rows; center and WRAM unchanged.\n", extended);

  /* Cone extremes and widening in both directions. Match the actual native
   * source coordinate at any camera offset, rather than using a fitted cone. */
  const int positions[] = {2, 77, 128, 253};
  const unsigned widths[] = {0, 3, 10};
  const unsigned angles[] = {0, 20, 54, 64, 74, 108, 128, 148, 182, 192, 202, 236, 256};
  unsigned cases = 0;
  int sourceY = samus_y_pos - layer1_y_pos - (samus_movement_type == 5 ? 12 : 16);
  assert(sourceY > 0 && sourceY < 230);
  for (unsigned pi = 0; pi < sizeof(positions) / sizeof(*positions); pi++)
    for (unsigned wi = 0; wi < sizeof(widths) / sizeof(*widths); wi++)
      for (unsigned ai = 0; ai < sizeof(angles) / sizeof(*angles); ai++) {
        unsigned angle = angles[ai], width = widths[wi];
        if (angle < 128 ? (angle < width || angle + width > 128) :
                         (angle - width < 128 || angle + width > 256)) continue;
        samus_pose_x_dir = angle < 128 ? 8 : 4;
        samus_x_pos = layer1_x_pos + positions[pi];
        xray_angle = angle;
        demo_input = width;
        int sourceX = positions[pi] + (samus_pose_x_dir == 4 ? -3 : 3);
        uint16 center[256];
        for (unsigned y = 0; y < 256; y++) center[y] = 0xff;
        if (sourceX >= 0 && sourceX < 256)
          CalculateXrayHdmaTableInner(sourceX, sourceY, angle, width, false, center);
        for (unsigned y = 0; y < kSnesHeight; y++) {
          lines[y].window2left = center[y] & 255;
          lines[y].window2right = center[y] >> 8;
        }
        memcpy(backup, g_ram, sizeof(g_ram));
        WideXray_Prepare(&frame);
        assert(!memcmp(backup, g_ram, sizeof(g_ram)));
        AssertCenter(&frame, lines);
        cases++;
      }
  printf("%u cone position/angle/width cases preserve the original center.\n", cases);

  /* Transitions do not project stale cones or tilemaps. An unrelated HDMA
   * object using freeze and color windows must not activate this path. */
  demo_input_pre_instr = 3;
  WideXray_Prepare(&frame);
  assert(!frame.xrayTiles[0] && !frame.xrayTiles[1]);
  for (unsigned y = 0; y < kSnesHeight; y++)
    assert(frame.xrayLeft[y] == lines[y].window2left &&
           frame.xrayRight[y] == lines[y].window2right);
  demo_input_pre_instr = 2;
  for (unsigned i = 0; i < 6; i++) hdma_object_pre_instructions[i] = 0;
  WideXray_Prepare(&frame);
  assert(!frame.xrayActive && !frame.xrayTiles[0]);
  free(rom);
  return 0;
}
