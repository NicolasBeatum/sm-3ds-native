# Global lighting and depth trial

Branch: `codex/super-metroid-lighting-depth`, based on `b5d38d0` (the
resume/session-log branch). Reference video:
https://www.youtube.com/watch?v=IMEYEe3Smoc (PixelRamp / Life2Build).
This is a local experiment; it does not imply a release or merge.

## What is implemented

SETUP → VIDEO → **LIGHTING** toggles an A/B comparison. The trial build
starts with it enabled unless an existing `scene_lighting` preference says
otherwise. The setting is saved with the other video preferences. Lighting now
runs in **every room during normal gameplay** (`game_state=8`), with Samus,
charging and projectile lights. The three fixed ship lamps remain exclusive to
Landing Site (`room=0x91f8`). Other rooms use a brighter, neutral ambient profile
so their original palette determines their hue.

Loading/fades, pause screens, frozen-time X-ray, the expanded eye beam and CPU
fallback retain normal output. Thus the effect is global for compatible PICA
room rendering, not for every cinematic/special PPU mode. Static lamps outside
the ship have not been authored individually.

- A cooler, darker distant BG2 separates scenery from the foreground. BG1 and
  objects have stronger ambient light; nearest texture sampling stays intact.
- Three warm light sources track the ship's hatch/side lamps in world space.
  Samus has a small teal fill light. Charging and active player projectiles add
  lights, with a fixed budget of eight sources and squared radial falloff.
- Tile luminance gradients approximate directional surface relief. This is
  per-quad shading, not an authored normal map or per-pixel lighting shader.
- Samus's palette-4 OAM tiles near her position project alpha silhouettes onto
  low-priority BG2. Foreground depth hides them. The blend preserves the SNES
  math alpha and does not write depth/stencil. Weather and HUD do not cast
  shadows. The fixed projection is approximate and is not physical ray tracing.
- A reusable 64×64 radial texture provides a small additive glow clipped below
  the HUD and inside the image. It does not blur the pixel art.

The depth impression comes from layer lighting and projected shadows. There is
no perspective camera, mesh extrusion, stereo eye separation, HDR output or
4K reconstruction. This prototype adapts parts of the reference's look to
PICA200; it does not reproduce PixelRamp's full rendering pipeline.

## Save 2 and host preview

The local English-ROM `sm.smc.srm` slot 2 has a valid checksum (`0x113b`),
Crateria area 0 / station 0, and 99 energy. A copy in the ignored trial output
folder was used for a native-engine run. After loading slot 2, the engine
reached room `91f8`, game state 8, Samus `(1152,1088)` and camera `(1024,1024)`.
The original SRAM file was never used as the game's write destination.
Native input checks also passed after turning Samus away from her forward-facing
ship pose: firing spawned one projectile (five scene lights), jumping moved
her up to Y=1048, and moving right reached X=1185. The software model also
rendered these three frames with bounded shadow counts. The source SRAM hash
matched the preserved copy after those initial checks. Subsequent interactive
play can naturally update the user save; preview runs only read the copy.

`tools/lighting_preview.c` runs the actual native logic, captures scanline PPU
state and writes a reference image/WRAM. `tools/lighting_raster_preview.c`
rasterizes the same model quads in software for A/B visual inspection, at
256×224 and 400×224. The stationary widescreen sample emits 23 shadow quads
on main and none on sub. The software unlit standard image has mean absolute
channel error 1.01 / 255 and maximum 6 against the native CPU image, principally
from RGB5/TEV quantization. The preview is not an Azahar GPU capture.

Local output: `output-lighting-trial/comparison.png`, `wide-on.png`, and
`SuperMetroid3DSPort.{3dsx,smdh,cia}`. ROM/SRAM/dumps remain local ignored files.
No game ROM is packaged in 3DSX/CIA.

## Reproduce

```sh
mkdir -p output-lighting-trial
python3 tools/build_lighting_preview.py
output-lighting-trial/lighting-preview USER_ROM USER_SRAM output-lighting-trial
cc -O2 -w -iquote sm -Isource tools/lighting_raster_preview.c \
  source/ppu_gpu_model.c source/scene_lighting.c \
  -o output-lighting-trial/lighting-raster-preview
output-lighting-trial/lighting-raster-preview output-lighting-trial 1 0
output-lighting-trial/lighting-raster-preview output-lighting-trial 1 1

cc -O2 -std=c11 -Wall -Wextra -Werror -iquote sm -Isource \
  tests/scene_lighting_test.c source/scene_lighting.c source/ppu_gpu_model.c \
  -o /tmp/sm-lighting-test
/tmp/sm-lighting-test

make FULL_NATIVE=1 LTO=1 BUILD=build-lighting-trial OUTPUT=output-lighting-global-trial \
  BUILD_FLAGS="-DSM3DS_OLD3DS -DSM3DS_PHASE_DIAG -DSM3DS_LIGHTING_TRIAL" 3dsx
make FULL_NATIVE=1 LTO=1 BUILD=build-lighting-trial OUTPUT=output-lighting-global-trial \
  BUILD_FLAGS="-DSM3DS_OLD3DS -DSM3DS_PHASE_DIAG -DSM3DS_LIGHTING_TRIAL" cia
```

Optional fourth preview argument `fire`, `jump`, or `move` checks actual
native inputs before capturing. `slot1` loads SRAM slot 1 to inspect another
room. Use a separate output directory for each.
The builder assumes the output folder exists. Run 3DSX/CIA targets sequentially
because the root Makefile recurses into a shared build directory.

The preview build requires a C compiler and GNU ld's `--wrap`. Its cache is
under `build-lighting-preview`. The tool uses a separate `preview-only.srm`
write destination. Use a user ROM and SRAM; neither input is supplied publicly.

## Verification and remaining checks

Host tests check scene/state guards, HUD/weather preservation, camera-relative
lights, bounded source count, shadow clipping and unchanged source alpha/depth.
The existing differential model test matches main/sub color and alpha across
264 animated frames with lighting disabled. Link `source/scene_lighting.c`
when building model tests. Existing widescreen, atlas and vertex tests remain
applicable. 3DSX and CIA compile with native/LTO/OLD3DS/PHASE_DIAG.

The user tested the initial Landing Site build in Azahar and reported that it
looked good. The global extension also passed a native slot-1 capture in Brinstar
(room `a70b`, state 8): one Samus light, 22 main shadow quads, no ship lights.
Global build artifacts: `output-lighting-global-trial/SuperMetroid3DSPort.3dsx`
and `.cia`. These have been compiled but have not been interactively tested in
Azahar or on real Old/New 3DS yet.

Additional tests check exact dirty atlas upload ranges (including word
boundaries and failed-upload retries), cached relief flips/invalidation, and
callback restoration on rejected frames. Differential frames now also include
independent main/sub OBJ enable changes, forced blank, and scanline size changes.
Host AddressSanitizer/UBSan runs were unavailable because their runtime libraries
are missing from this environment; the ordinary tests pass.

## CPU optimization measurements

The model selects lit/unlit emission once per frame, with no lighting branch in
the ordinary tile emitter. A shared 8-pixel RGB light field replaces repeated
per-quad light loops and variable division; reciprocal falloff is computed once
per light. A bounded 512-entry, per-frame relief cache includes cropped texture
centers and all flips, preserving palette animation. This trades an extra 13,100
bytes against the initial lighting struct for less repeated work. The lighting
struct is 16,064 bytes, plus the existing 16-KiB reusable glow texture. Field
preparation is included in `bg_main` phase diagnostics.

Base rendering also reuses OAM selection between main/sub while preserving
screen-specific enables/windows and SNES per-line sprite/tile limits. Atlas
uploads skip clean 32-tile words instead of scanning every slot; flush ranges
and error retry behavior are unchanged. Invisible glow quads are culled.

`tools/build_lighting_benchmark.py` compiles the base model (`b5d38d0`), initial
trial (`59424b8`) and current model with the **same four-vertex packing callback**.
Run `build-lighting-benchmark/lighting-benchmark output-lighting-trial` after
creating stationary and `fire` native captures. Median of nine warm-cache
600-frame batches (one warm-up batch discarded), CPU model/vertex work only:

| Landing Site, width 400 | Initial trial | Optimized | Reduction |
| --- | ---: | ---: | ---: |
| Lighting off | 0.123800 ms | 0.107323 ms | 13.3% |
| Four lights | 0.198328 ms | 0.178265 ms | 10.1% |
| Eight lights | 0.225180 ms | 0.184329 ms | 18.1% |
| Firing, five lights | 0.199561 ms | 0.178750 ms | 10.4% |

The original base model measured 0.106480 ms in the same stationary sample:
optimized-off is within about 1% there. The eight-light row is a synthetic stress
case with the extra lights positioned together; the firing row uses an actual
native projectile. Width 256 also improves (about 8% with four lights and 16%
with eight). Raw local results are `output-lighting-global-trial/performance-host.txt`.

These are host CPU timings, not GPU timings, console FPS or total game-logic
speedups. The model benchmark does not include atlas cache-clean API calls, glow
rendering, audio, gameplay, or GPU synchronization. To establish real performance,
load slot 2 and compare matched diagnostics with SETUP → VIDEO → LIGHTING ON/OFF
at the same position, viewport and input; repeat in other rooms on actual 3DS.
