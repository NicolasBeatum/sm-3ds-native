# Background preparation / indexed geometry trial

Branch: `codex/hud-grapple-room-performance`. Reference: `b8771d7`.
Local trial packages only; no merge or release is implied.

## Dumps examined

These are the five captures supplied in `Escritorio/dumps`. All report Old 3DS,
widescreen enabled, native engine, PICA200 and CPU budget 80%. The table uses
the recent 120 rendered frames at each capture, rather than the cumulative
session (the latter includes different rooms and UI activity).

| Dump | Room | Recent FPS | Frame interval | Main backgrounds | Secondary backgrounds | Logic | Bottom screen |
|---|---|---:|---:|---:|---:|---:|---:|
| 20261001-185632-000 | acb3 | 39.24 | 25.481 ms | 3.127 ms | 12.776 ms | 2.442 ms | 0.009 ms |
| 20261001-190558-001 | a788 | 38.23 | 26.151 ms | 3.209 ms | 13.257 ms | 2.485 ms | 0.010 ms |
| 20261001-191321-002 | cfc9 | 37.31 | 26.801 ms | 7.800 ms | 9.201 ms | 2.450 ms | 0.010 ms |
| 20261001-191520-003 | d461 | 31.42 | 31.817 ms | 12.256 ms | 9.910 ms | 2.367 ms | 0.011 ms |
| 20261001-191957-004 | a2ce | 42.91 | 23.301 ms | 7.136 ms | 4.971 ms | 2.425 ms | 0.010 ms |

Background preparation is the largest measured CPU span in these rooms.
The first two are lava FX (type 2); the next two are water FX (type 6), with
layer switching / HDMA. The secondary SNES layers are part of the original
color composition and cannot simply be dropped. Timings are CPU wall spans,
and asynchronous GPU work / waits may also contribute to frame intervals.

## Changes

- Compare relevant background registers and visibility in a single inline
  scanline-band pass. Test scroll first, which changes frequently in distorted
  backgrounds. This replaces a whole `BgLayer` memcmp followed by another
  visibility/window pass, without changing sampling or HDMA timing.
- Store four shared corners per quad instead of six duplicated triangle
  vertices. An immutable, once-flushed index buffer emits exactly the same
  `a,b,c` and `c,b,d` triangles. Depth, UV, colors, diagonal, ordering, stencil
  rules and palette/nearest filtering remain unchanged.
- Rebase the vertex buffer at every group/chunk. A chunk has at most 32,766
  indices and indices remain local to that chunk. The maximum drawable quad
  count remains 43,690. Vertex memory writes/cache clean spans shrink by 1/3;
  the two vertex pools minus the new shared index buffer save 2,730,628 bytes
  of linear memory. These are data-size reductions, not a measured FPS gain.
- New dumps identify these changes with `ppu_bg_lookup=inline-band-v3` and
  `ppu_geometry=indexed-quads-v1`. Existing diagnostic metrics remain enabled.

A lazy tilemap-row cache was also tried. It preserved pixels but added lookup
overhead in host measurements, so it was discarded.

## Verification and limits

- Main/sub color and alpha rasters and decoded atlas pixels match the previous
  model across 216 animated synthetic frames, including HDMA scroll, windows,
  room-side tiles, flips, priority, tile-bank changes, transparent/opaque
  transitions, palette/VRAM changes and frame-counter rollover.
- Indexed expansion matches the previous triangle vertex bytes. Chunk tests
  cover empty/small draws, the chunk boundary, partial chunks and full original
  capacity. Existing widescreen/eye-beam and atlas invalidation tests pass.
- The differential benchmark now includes CPU vertex packing (six vertices
  for the reference, four for the trial). On this desktop run, total model and
  packing spans were about 4–14% shorter. The background grouping change alone
  was close to neutral on desktop. These are synthetic CPU timings; they do
  not include GPU execution, cache clean calls or the rest of the game and do
  not establish a percentage/FPS gain on Old 3DS.
- Both 3DSX and CIA compile locally. Hardware comparison remains pending.

```sh
git show b8771d7:source/ppu_gpu_model.c > /tmp/sm-ppu-reference.c
cc -O2 -std=c11 -iquote sm -Isource \
  -DPicaAtlasInit=ReferenceAtlasInit -DPicaAtlasBegin=ReferenceAtlasBegin \
  -DPicaBuildFrame=ReferenceBuildFrame -DPicaCaptureLine=ReferenceCaptureLine \
  -DPicaHudLineCount=ReferenceHudLineCount -DPicaScrollOffset=ReferenceScrollOffset \
  -c /tmp/sm-ppu-reference.c -o /tmp/sm-ppu-reference.o
cc -O2 -std=c11 -iquote sm -Isource tests/ppu_gpu_frame_compare.c \
  source/ppu_gpu_model.c /tmp/sm-ppu-reference.o -o /tmp/sm-ppu-compare
/tmp/sm-ppu-compare
cc -O2 -std=c11 -iquote sm -Isource tests/ppu_gpu_vertices_test.c \
  -o /tmp/sm-indexed-test
/tmp/sm-indexed-test
```

## Trial packages

`output-background-trial/SuperMetroid3DSPort.{3dsx,cia}` use the same native,
LTO, OLD3DS and PHASE_DIAG flags as the supplied dumps. The packages retain
the configurable HUD, screen sizing, maps, banner and suspend/resume trial.
No ROM/IPS is included.

To evaluate the change, capture the same rooms at the same position in FIT
with widescreen enabled, using the same package format as the baseline. Check
water/lava distortion and translucency while moving before generating a dump.
Compare recent room timings separately from cumulative session averages.
