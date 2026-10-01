#!/bin/sh
set -eu

cd "$(dirname "$0")"
# Rebuild the animated HOME Menu scene from the chosen GIF. pycgfx is a
# build-time tool; distributed packages need no Python or external files.
: "${PYCGFX_DIR:?Set PYCGFX_DIR to skyfloogle/pycgfx}"
"${BANNER_PYTHON:-python3}" ../tools/build_animated_banner.py \
  --pycgfx "$PYCGFX_DIR" --gif banner-animated/source.gif \
  --output banner.cgfx --work "${BANNER_WORK_DIR:-/tmp/sm3ds-banner-model}"
