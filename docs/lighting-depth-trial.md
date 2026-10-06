# Landing Site lighting and depth trial

Branch: `codex/super-metroid-lighting-depth`, based on `b5d38d0` (the
resume/session-log branch). Reference video:
https://www.youtube.com/watch?v=IMEYEe3Smoc (PixelRamp / Life2Build).
This is a local experiment; it does not imply a release or merge.

## What is implemented

SETUP → VIDEO → **SHIP LIGHTING** toggles an A/B comparison. The trial build
starts with it enabled unless an existing `scene_lighting` preference says
otherwise. The setting is saved with the other video preferences. Only normal
gameplay (`game_state=8`) in Landing Site (`room=0x91f8`) receives the effect.
Other rooms, loading/fades, pause screens and CPU fallback retain normal output.

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
still matched the preserved copy afterward.

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
  tests/scene_lighting_test.c source/scene_lighting.c -o /tmp/sm-lighting-test
/tmp/sm-lighting-test

make FULL_NATIVE=1 LTO=1 BUILD=build-lighting-trial OUTPUT=output-lighting-trial \
  BUILD_FLAGS="-DSM3DS_OLD3DS -DSM3DS_PHASE_DIAG -DSM3DS_LIGHTING_TRIAL" 3dsx
make FULL_NATIVE=1 LTO=1 BUILD=build-lighting-trial OUTPUT=output-lighting-trial \
  BUILD_FLAGS="-DSM3DS_OLD3DS -DSM3DS_PHASE_DIAG -DSM3DS_LIGHTING_TRIAL" cia
```

Optional fourth preview argument `fire`, `jump`, or `move` checks actual
native inputs before capturing. Use a separate output directory for each.
The builder assumes the output folder exists. Run 3DSX/CIA targets sequentially
because the root Makefile recurses into a shared build directory.

The preview build requires a C compiler and GNU ld's `--wrap`. Its cache is
under `build-lighting-preview`. The tool uses a separate `preview-only.srm`
write destination. Use a user ROM and SRAM; neither input is supplied publicly.

## Verification and remaining checks

Host tests check scene/state guards, HUD/weather preservation, camera-relative
lights, bounded source count, shadow clipping and unchanged source alpha/depth.
The existing differential model test matches main/sub color and alpha across
216 animated frames with lighting disabled. Link `source/scene_lighting.c`
when building model tests. Existing widescreen, atlas and vertex tests remain
applicable. 3DSX and CIA compile with native/LTO/OLD3DS/PHASE_DIAG.

GPU blend/scissor behavior in Azahar and performance on real Old/New 3DS remain
unverified. On the console/emulator, load slot 2 normally and toggle SHIP
LIGHTING. Move/jump near the ship, fire and hold charge, compare standard and
widescreen, then pause and leave the room. Capture matched diagnostics before
claiming an FPS result. Returning to other rooms must disable the scene effect.
