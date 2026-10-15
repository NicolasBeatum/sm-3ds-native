# Shaktool and liquid rendering trial

Branch: `codex/super-metroid-lighting-depth`, based on `79d8075`.
Packages and raw results: `output-maridia-fix-trial/`.

## Shaktool

Shaktool starts moving but can stall before completing its route. The engine
aliases `g_ram` through word arrays, `EnemyData`, `EnemyBase` and specific enemy
structures. Upstream `sm/Makefile` requires `-fno-strict-aliasing`; the port only
suppressed aliasing warnings. Restore the upstream option in the common flags,
including the native engine's Ofast/LTO compilation. A clean native rebuild is
required when adopting this flag. No Shaktool instructions, collisions, speeds,
clocks or frame pacing are modified.

An isolated host probe compiles `sm_aa.c`, `sm_a0.c`, `sm_rtl.c` and the native
preview at `-O3 -Ofast -flto`, linking the other engine objects built at O2.
With alias assumptions enabled, the segments stall at X=217–234 and Samus cannot
advance beyond X=235. Adding `-fno-strict-aliasing` to those same optimized
translation units lets the head travel beyond X=800 and Samus exit to D6D0,
the Spring Ball room. This reproduces compiler sensitivity on the host; device
confirmation is still required. It does not demonstrate a collected item.

The `shaktool` preview mode loads actual room D8C5 via incoming door A798,
sets diagnostic equipment/health/ammo in its private RAM, and then uses native
controls and AI. It fires a Power Bomb, follows with right/jump inputs and shoots
only at the final doorway. It fails unless the native transition reaches D6D0
in gameplay state 8. It never writes the source save. Example:

```sh
python3 tools/build_lighting_preview.py
mkdir -p output-maridia-fix-trial/native-route
output-lighting-trial/lighting-preview romfs/sm.smc \
  output-lighting-trial/save2-source.srm \
  output-maridia-fix-trial/native-route shaktool
```

## Liquid overlay rendering

BG3 water/sand/tint tiles used to emit a separate quad for every eight-pixel
slice. Native scanline distortion can split these into one/two-line bands.
This is expensive even with LIGHTING OFF. The optimized path applies to
compatible BG3 patterns throughout the game, without a room whitelist:

- Uniform RGBA rows use one constant-U strip per repeated descriptor run;
  transparent row slices are skipped. Metadata reuses tile padding (80 bytes).
- Nonuniform four-tile patterns use up to eight cached 512×8 horizontal strips
  inside each existing atlas. Canonical rotation shares patterns across scroll
  phases; flips are baked into the copied texels. Native vertical texels,
  priorities, windows, color math and scanline movement remain intact.
- Strips are pinned for each frame. More than eight distinct patterns fall
  back to ordinary tiles. Palette/VRAM changes refresh actual RGBA data; stable
  strips need no additional texture cache flush. No texture or pass is added.

The strips reserve 512 of 8192 slots in each existing 2 MiB atlas (128 KiB of
already allocated storage); normal tile capacity becomes 7679 plus the white
slot. Added metadata is 160 bytes per atlas, 320 across both buffers. Existing
capacity fallback remains available. Animated graphics/palettes can dirty up to
128 KiB of strip data per active atlas; hardware upload cost is not measured by
the host benchmark. These are allocation changes, not a total process memory
measurement.

## Validation and measurements

- Ten native biome snapshots × two widths × lighting ON/OFF: 40 renders are
  byte-identical to 79d8075. Native AC83→AC5A transition: 106 frames × the same
  four modes, all 424 renders identical. These are host raster replays.
- Twenty scene/width pairs × ten alternating atlas builds retain identical
  quad bytes cold/warm. Lighting diagnostics report the expected source counts,
  cached-field rebuild and shadow quads.
- BG3 regression compares actual RGBA pixels and depth to a separate SNES
  tile decoder across 15 frames: scanline scroll, all flips, four-tile patterns,
  eight-pattern capacity/overflow, palette/VRAM edits, windows, map wrapping,
  transparent rows, frame-counter wrap and 256/400 widths. Existing atlas dirty,
  tile-cache, widescreen, vertices, lighting/environment and diagnostic tests pass.
  Native Reo/Dessgeega/three-pipe behavior tests also pass at Ofast/LTO with
  alias assumptions disabled.
  The widescreen beam test now compares BG3 colors rather than atlas addresses,
  because identical pixels can come from different strip UVs.
- CIA/3DSX builds use FULL_NATIVE, LTO, OLD3DS, PHASE_DIAG, LIGHTING_TRIAL and
  SYSTEM_CORE_PERCENT=30. The original empty distributable RomFS is retained.

Host benchmarks use the same no-alias flag on both revisions, median nine
400-frame batches after warmup. Five scenes × two widths × ON/OFF × stationary
or synthetic camera motion produce 40 pairs. Only CPU model construction and
vertex packing are measured; GPU drawing, texture upload, audio and game logic
are excluded. Concurrent host activity also affects timings. Selected stationary
400-pixel results:

| Scene | Lights | CPU before → after (ms) | Quads before → after |
| --- | --- | --- | --- |
| Maridia D617 | OFF | 0.229300 → 0.153843 | 11291 → 6382 |
| Maridia D617 | ON | 0.297400 → 0.213893 | 11313 → 6404 |
| Maridia D1DD | OFF | 0.072535 → 0.055817 | 3471 → 2170 |
| Maridia D1DD | ON | 0.100663 → 0.077812 | 3487 → 2186 |
| Norfair lava | ON | 0.243689 → 0.220229 | 8361 → 7109 |

Rebuild benchmarks with `python3 tools/build_lighting_benchmark.py --reference
79d8075`; each binary accepts `CAPTURE LIGHTS CAMERA WIDTH`. Raw data and raster
verification manifests are beside the packages. These results do not establish
Old 3DS FPS or prove every shallow-water/quicksand room is resolved.

For a device comparison, capture L+R+A after about ten seconds in the same room,
position and viewport with lighting OFF, then ON. Existing schema-6 CPU phase
and frame summaries remain; `ppu_bg_lookup=bg3-repeat-strips-v4` identifies this
renderer. No new per-tile timer or logging overhead is added. Keep the user's
CIA, Old 3DS and widescreen conditions when checking the reported slow rooms.
