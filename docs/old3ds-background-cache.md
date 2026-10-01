# Background descriptor cache hardware trial

Branch: `codex/old3ds-background-cache`. This is a trial following v0.1.1,
not a measured 60 FPS release.

## Measured starting point

The Old 3DS dump `20260928-221433-000`, room `9cb3`, widescreen enabled,
reports 43.10 FPS over 120 frames, 23.086 ms average frame work and 10.577 ms
combined main/sub background preparation. These are measurements of the
previous build, not of this optimization.

## Change

Cache the decoded atlas slot by the background's tile/palette descriptor.
Priority and flip bits still control each quad independently. A slot is
resolved once, then reused while the GPU frame's VRAM/CGRAM snapshot is
unchanged. Main/sub rendering share the cache. It uses 48 KiB per atlas,
96 KiB total for the two existing buffered atlases.

The cache is invalidated on the next frame or a different tile bank. Layers
with scanline bank changes use the original lookup path to avoid repeatedly
clearing the table. Transparent tiles, atlas failures and frame-counter
rollover retain their original behavior. No scanline, sprite, HDMA,
resolution, refresh rate or game-logic work is removed.

## Verification

```sh
cc -O2 -Ism tests/ppu_gpu_model_wide_test.c source/ppu_gpu_model.c -o /tmp/ppu-wide-test
/tmp/ppu-wide-test
cc -O2 -Wall -Wextra -Ism tests/ppu_gpu_tile_cache_test.c source/ppu_gpu_model.c -o /tmp/ppu-cache-test
/tmp/ppu-cache-test
```

Both tests pass. A separate differential harness compiled the previous
`dev` model alongside this model and compared all emitted quads, decoded
atlas pixels, tile metadata and counters for 120 frames. It covered normal
and wide output, sparse/dense tile sets, per-line scrolling/windows,
graphics-bank changes, palette/VRAM animation and counter rollover. All
outputs matched.

Desktop synthetic timings with a no-op quad consumer reduced model build
time by approximately 12–32% for stable-bank cases. The bank-changing case
was approximately 2–4% slower from the extra checks. These figures exclude
GPU submission and the rest of the game and do not predict hardware FPS.

## Build and compare

Both trial packages use:

```sh
make FULL_NATIVE=1 LTO=1 BUILD_FLAGS="-DSM3DS_OLD3DS -DSM3DS_PHASE_DIAG" \
  BUILD=build-old3ds-cache OUTPUT=output-old3ds-cache 3dsx
make FULL_NATIVE=1 LTO=1 BUILD_FLAGS="-DSM3DS_OLD3DS -DSM3DS_PHASE_DIAG" \
  BUILD=build-old3ds-cache OUTPUT=output-old3ds-cache cia
```

They contain only the empty RomFS stub. The pending banner artwork is not
part of this trial. Save a dump after spending about ten seconds in the
same room and position as the baseline, initially with widescreen off,
then on. New dumps contain `ppu_bg_lookup=descriptor-cache-v1` and
`build_lto=1`, alongside model, FPS and phase timings.

The same build supports both Old and New 3DS. Existing startup code calls
`osSetSpeedupEnable(true)`; libctru requests the faster New 3DS CPU clock
and L2 cache. The CIA descriptor also permits both. The `SM3DS_OLD3DS`
define does not cap New 3DS performance. A New 3DS hardware dump is still
needed to quantify its gain.
