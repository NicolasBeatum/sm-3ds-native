#!/bin/sh
set -eu

cd "$(dirname "$0")"
base="$(mktemp --suffix=.png)"
trap 'rm -f "$base"' EXIT

inkscape banner-source.svg --export-filename="$base" --export-width=256 --export-height=128 >/dev/null
magick "$base" \
  \( banner-helmet.png -resize 154x77\! \) -geometry +51+28 -compose over -composite \
  -fill '#11131de8' -draw 'rectangle 48,80 208,99' \
  -font DejaVu-Sans-Bold -pointsize 14 -gravity North -fill '#f5bd55' \
  -annotate +0+80 'SUPER METROID' -alpha off banner.png
