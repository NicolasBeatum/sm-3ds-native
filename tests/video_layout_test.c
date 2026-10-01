#include "../source/video_layout.h"
#include <assert.h>
#include <math.h>
int main(void) {
  for (int wide = 0; wide < 2; wide++) {
    TopVideoLayout l = TopVideoLayout_Get(kVideoMode_OneToOne, wide);
    assert(l.height == 224 && l.width == l.source_width && l.y == 8);
    assert(l.x * 2 + l.width == 400 && !l.source_x);
    l = TopVideoLayout_Get(kVideoMode_Stretched, wide);
    assert(!l.x && !l.y && l.width == 400 && l.height == 240);
    assert(l.source_width == (wide ? 400 : 256) && !l.source_x);
  }
  TopVideoLayout fit = TopVideoLayout_Get(kVideoMode_Fit, false);
  assert(fit.x == 63 && fit.width == 274 && fit.height == 240);
  fit = TopVideoLayout_Get(kVideoMode_Fit, true);
  assert(fit.width == 400 && fit.source_width == 374 && fit.source_x == 13);
  // A 224->240 nearest enlargement lands exactly on source row 21 at
  // output row 22. Tiny opposite triangle roundoff must choose one row.
  float row = (22.0f + .5f) * 224 / 240;
  assert(row == 21);
  assert(floorf(row - .00001f + TOP_SAMPLE_BIAS) == 21);
  assert(floorf(row + .00001f + TOP_SAMPLE_BIAS) == 21);
  // At 1:1 the bias leaves every source pixel unchanged.
  for (int i = 0; i < 400; i++) assert(floorf(i + .5f + TOP_SAMPLE_BIAS) == i);
}
