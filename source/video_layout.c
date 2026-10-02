#include "video_layout.h"
#include "wide_config.h"

TopVideoLayout TopVideoLayout_Get(enum BottomScreenVideoMode mode, bool wide) {
  if (mode == kVideoMode_OneToOne)
    return (TopVideoLayout){wide ? 0 : 72, 8, wide ? 400 : 256, 224,
                            0, wide ? 400 : 256};
  if (mode == kVideoMode_Stretched)
    return (TopVideoLayout){0, 0, 400, 240, 0, wide ? 400 : 256};
  enum { cropWidth = (400 * 256 + 274 / 2) / 274 };
  return (TopVideoLayout){wide ? 0 : 63, 0, wide ? 400 : 274, 240,
                          wide ? (400 - cropWidth) / 2 : 0,
                          wide ? cropWidth : 256};
}
