#include "wide_bounds.h"
#include "wide_config.h"

static int Max(int a, int b) { return a > b ? a : b; }
static int Min(int a, int b) { return a < b ? a : b; }
static int Clamp(int value, int low, int high) {
  return Max(low, Min(value, high));
}

WideWorldSpan WideBounds_Compute(int camera_x, int camera_y,
                                 unsigned room_width_blocks,
                                 unsigned room_width_scrolls,
                                 unsigned room_height_scrolls,
                                 const uint8_t *scrolls) {
  int room_right = (int)room_width_blocks * 16;
  int world_left = 0, world_right = room_right;

  /* A zero scroll entry prevents the camera from entering that screen.
   * Keep the original viewport intact, but do not reveal adjacent locked
   * screens in the new margins. Check both rows if the viewport straddles
   * a horizontal screen boundary. */
  if (scrolls && room_width_scrolls && room_height_scrolls &&
      room_width_scrolls * room_height_scrolls <= 512 &&
      room_width_blocks == room_width_scrolls * 16) {
    int screen_x = (camera_x + kSnesWidth / 2) / kSnesWidth;
    if (screen_x >= 0 && screen_x < (int)room_width_scrolls) {
      int first_y = Clamp(camera_y / kSnesWidth, 0,
                          (int)room_height_scrolls - 1);
      int last_y = Clamp((camera_y + kSnesHeight - 1) / kSnesWidth, 0,
                         (int)room_height_scrolls - 1);
      for (int row = first_y; row <= last_y; row++) {
        const uint8_t *cells = scrolls + row * room_width_scrolls;
        int first_x = screen_x, last_x = screen_x;
        while (first_x > 0 && cells[first_x - 1]) first_x--;
        while (last_x + 1 < (int)room_width_scrolls && cells[last_x + 1])
          last_x++;
        world_left = Max(world_left, first_x * kSnesWidth);
        world_right = Min(world_right, (last_x + 1) * kSnesWidth);
      }
    }
  }

  WideWorldSpan span = {
      .left = Clamp(kWideExtraX + world_left - camera_x, 0, kWideWidth),
      .right = Clamp(kWideExtraX + world_right - camera_x, 0, kWideWidth),
  };
  /* The extra area is optional. Never obscure the game's native view. */
  span.left = Min(span.left, kWideExtraX);
  span.right = Max(span.right, kWideExtraX + kSnesWidth);
  return span;
}
