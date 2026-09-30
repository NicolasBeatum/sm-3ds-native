#!/bin/sh
set -eu

cd "$(dirname "$0")"
# Use the approved pixel-art composition verbatim. The upper-screen title
# was composited from the original screenshot, without generative redraw.
cp banner-next/sm-3ds-native-space-256x128.png banner.png
