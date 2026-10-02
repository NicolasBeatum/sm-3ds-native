# HUD, video sizing and widescreen background trial

Branch: `codex/hud-grapple-room-performance`, based on the pending
`codex/suspend-resume` trial. No release or merge is implied by these builds.

## Interface

- HUD ITEMS cycles through ONLY AMMO, AMMO + HOOK and ALL ITEMS.
- Health stays large and closer to the energy tanks. Ammo counts are centered
  below the original ROM icons in VERTICAL mode (the default); the four/five-slot
  layouts use smaller counts. HUD NUMBERS can switch to HORIZONTAL, with compact
  counts beside each ammo icon. Both orientations support all three item modes.
  Unowned items leave their slots empty and reserved in either orientation;
  acquiring an item does not move the other slots. Touch uses the same slot
  boundaries as the drawing, including narrower utility slots in HORIZONTAL.
- Grapple and X-Ray select/deselect the original game HUD slots, just as the
  existing ammo shortcuts do. They appear only when equipped. The X-Ray map
  shortcut selects the scope; the normal game firing control still activates it.
- X-RAY MAP BUTTON is independent of FLOATING MAP BUTTONS (Samus/names).
  It works on the area and world maps and is available in all three HUD modes.
- Setup groups HUD, MAP and VIDEO keep every option accessible even when
  STATUS BAR SETUP is on. The information/dump panel remains under `i`.
- VIDEO offers FIT (previous presentation), STRETCHED and 1:1. Without
  widescreen these display 274x240, 400x240 and 256x224 respectively. With
  widescreen, 1:1 displays the full 400x224 render, with eight blank rows above
  and below; STRETCHED shows that full render at 400x240. FIT retains the
  previous centered crop. This affects presentation, not the game camera.
- Preferences persist in the existing settings file. New keys are `hud_items`
  (0/1/2), `hud_numbers_horizontal` (0/1), `xray_map_button` and `video_mode` (0/1/2).

## Dumps examined

All three dumps are Old 3DS, native engine, widescreen on, PICA200 active,
CPU budget 80%. The table describes the last 120 frames of each capture,
not the complete traversal or a CIA versus 3DSX comparison.

| Dump | Room | FPS | Mean frame interval | Secondary background | Logic | Bottom screen |
|---|---|---:|---:|---:|---:|---:|
| 20260930-202717-000 | a7b3 | 36.65 | 27.284 ms | 14.130 ms | 2.478 ms | 0.098 ms |
| 20260930-203031-001 | acb3 | 41.72 | 23.966 ms | 10.807 ms | 2.458 ms | 0.051 ms |
| 20261001-002726-000 | aab5 | 47.88 | 20.884 ms | 3.934 ms | 2.426 ms | 0.009 ms |

The first two rooms spend most CPU preparation time on the secondary SNES
backgrounds. WRAM shows lava FX type 2, main screen BG3, secondary BG1/BG2/OBJ
and color math enabled for BG3 and backdrop (`CGADSUB=0x24`). Thus the
secondary background is required even above the lava. Skipping it merely
because the main lava layer is absent would change the image.

## Optimization retained

The center of a widescreen room uses the original VRAM tilemap. Process that
center as one interval, avoiding per-tile room-side checks and boundary splits.
Static VRAM-only backgrounds use the same direct loop across their full span.
Cache scroll/base values once per interval. Graphics-bank cache eligibility
ignores inactive lines only when an initial bank change is detected.

The rendering still handles every scanline, HDMA scroll value, palette, window,
priority and sprite. Nothing changes collision, AI, refresh rate or resolution.
Additional transparent-slice and color-math culling experiments were rejected:
their added CPU cost or lack of applicability did not justify keeping them.

## Upper HUD pixel investigation

The latest top.bmp has inconsistent pixels along energy tank/icon edges.
The corresponding ROM HUD tiles have uniform rows. Fractional nearest scaling
can put a sample exactly on a row boundary: output row 22 has source coordinate
`(22 + 0.5) * 224 / 240 = 21`. Opposite floating point interpolation rounding
across triangles can select different neighboring rows.

The trial shifts top-screen sampling by 1/64 of a source texel to choose one
side consistently. It retains nearest filtering, original graphics and colors.
1:1 still maps each source pixel to exactly one display pixel. This is a fix for
the suspected presentation artifact; confirmation on hardware with a new
screenshot dump is still needed. It is not a replacement HUD font or palette.

## Verification

- Host UI trial loaded the latest WRAM/Spanish ROM and checked acquired/missing
  item selection, X-Ray touch without dragging/markers, all compact Setup rows,
  grouping, both number orientations in all three modes and settings round trips.
  Maximum ammo counts fit. All six HUD settings remain reachable with the Setup
  status bar enabled.
- `tests/video_layout_test.c` checks all six sizing combinations, centering,
  and boundary bias without changing integer samples.
- Existing widescreen and atlas invalidation regressions pass.
- `tests/ppu_gpu_frame_compare.c` compares rasterized main/sub color and alpha
  against the pre-change model over 216 animated synthetic frames. It includes
  256/400 output, flips, windows, per-line scroll, bank switches, transparency,
  palette/VRAM changes, sprite sizes, atlas counter rollover and room side blocks.
  Main/sub rasters and decoded atlas pixels match.
- The original desktop timings used a no-op quad consumer; they measured model preparation,
  excluding real GPU vertex submission, GPU work and the rest of the game.
  Wide cases ran roughly 6–18% faster in the sampled runs. This does not predict
  a hardware FPS percentage. Standard cases remained close to the baseline.
  The subsequent benchmark includes CPU vertex packing; see
  [the background/indexed geometry trial](background-october-trial.md) for its
  reference commit, later dumps and results.

Run the differential check with the model from the parent commit:

```sh
git show ec217d1:source/ppu_gpu_model.c > /tmp/sm-ppu-reference.c
cc -O2 -std=c11 -iquote sm -Isource \
  -DPicaAtlasInit=ReferenceAtlasInit -DPicaAtlasBegin=ReferenceAtlasBegin \
  -DPicaBuildFrame=ReferenceBuildFrame -DPicaCaptureLine=ReferenceCaptureLine \
  -DPicaHudLineCount=ReferenceHudLineCount -DPicaScrollOffset=ReferenceScrollOffset \
  -c /tmp/sm-ppu-reference.c -o /tmp/sm-ppu-reference.o
cc -O2 -std=c11 -iquote sm -Isource tests/ppu_gpu_frame_compare.c \
  source/ppu_gpu_model.c /tmp/sm-ppu-reference.o -o /tmp/sm-ppu-compare
/tmp/sm-ppu-compare
cc -O2 -std=c11 tests/video_layout_test.c source/video_layout.c -lm \
  -o /tmp/sm-video-test
/tmp/sm-video-test
```

## Trial packages

`output-hud-grapple-trial/SuperMetroid3DSPort.{3dsx,cia}` use FULL_NATIVE=1,
LTO=1 and `-DSM3DS_OLD3DS -DSM3DS_PHASE_DIAG`. They include the existing
suspend/resume trial and animated banner. The small HOME icon stays unchanged.
RomFS remains the empty stub; no ROM or IPS is packaged.

New dumps identify the render changes with `ppu_bg_lookup=wide-span-cache-v2`
and `top_sampling=nearest-biased-v1`, and record `video_mode`.
Compare new dumps in rooms a7b3/acb3 with the same position, widescreen and
video mode (FIT) as these baselines. Confirm upper HUD edges in FIT and 1:1.
