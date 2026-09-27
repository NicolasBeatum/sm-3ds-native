#include "../source/wide_bounds.h"
#include "../source/wide_config.h"

#include <assert.h>

int main(void) {
  const uint8_t open[] = {1, 1, 1};
  const uint8_t locked_right[] = {1, 1, 0};
  const uint8_t two_rows[] = {1, 1, 1, 1, 1, 0};
  WideWorldSpan span;

  span = WideBounds_Compute(256, 0, 48, 3, 1, open);
  assert(span.left == 0 && span.right == kWideWidth);

  span = WideBounds_Compute(0, 0, 48, 3, 1, open);
  assert(span.left == kWideExtraX && span.right == kWideWidth);

  span = WideBounds_Compute(256, 0, 48, 3, 1, locked_right);
  assert(span.left == 0 && span.right == kWideExtraX + kSnesWidth);

  span = WideBounds_Compute(256, 64, 48, 3, 2, two_rows);
  assert(span.left == 0 && span.right == kWideExtraX + kSnesWidth);

  span = WideBounds_Compute(0, 0, 16, 1, 1, open);
  assert(span.left == kWideExtraX &&
         span.right == kWideExtraX + kSnesWidth);
  return 0;
}
