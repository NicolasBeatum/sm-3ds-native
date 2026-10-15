# Global lighting and depth trial

## More atmospheric lights with bounded bloom

Latest trial: `output-lighting-atmosphere-trial/`, based on `f69c976`.
The scenery budget increases from two to four sources, within the existing
8-source total. Nearby instances of the same motif are clustered using a 48-pixel
neighborhood, and native doors/liquids/player sources retain budget priority.
Recognized BG1 graphics now include complete Tourian red pilot lamps and the
shared elevator's yellow indicators. Both mirrored elevator halves must be
present; they create one central warm pool. Existing blue organic/LED motifs,
ambient tint, contact depth and Landing Site appearance remain.

Only the nearest two selected scenery sources receive glow quads. The others
contribute to the cached modulation grid, avoiding extra halo overdraw. Ship,
door, liquid and player halos retain their existing behavior. `SceneLight` uses
its existing tail padding for a bloom flag and remains 16 bytes. No new glow
texture or rendering pass is added, and the total source limit stays eight.

Two fixed 64-entry emitter caches store native map coordinates/descriptors.
Graphics fingerprints and tilemap checksums validate these each frame; unchanged
maps reuse detection across camera/parallax motion and atlas swaps. Palette
color/intensity and foreground visibility are evaluated live. Changed maps/art
invalidate detection; unusually dense custom maps use a bounded visible scan
instead of silently losing emitters beyond cache capacity. BG1 lamps embedded
in machinery are allowed; BG2 motifs behind complete solid blocks are rejected.
Warped/disabled layers retain their prior conservative behavior.

ARM lighting state grows 7056→8112 bytes (+1056 for both caches); the diagnostic
ring stays 14400 bytes. No heap allocations, per-tile clock calls, physics,
game clocks, frame pacing or suspend/audio configuration changes are introduced.
`SM3DS_SYSTEM_CORE_PERCENT=30` remains. Dumps retain schema 6 and source/time
columns; `background_sources` now counts recognized scenery from either BG1 or
BG2. Model tag: `cached-color-grid-16px-spread-emitters-v4`.

Validation:

- Ten native biome captures × 256/400 pixels: all HUD rows and OFF renders are
  byte-identical to the previous trial; Landing Site ON is also identical.
  Green Brinstar and Tourian have four scenery lights; Red Brinstar has one
  newly recognized warm elevator light. Other sampled rooms retain their
  existing sources. This recognizes shared assets, not every light in the game.
- Twenty scene/viewport pairs × ten cold/warm double-atlas frames: identical
  quad bytes with/without timing instrumentation; field/source/shadow counters
  agree. Native motif regression covers map invalidation, scrolling cache
  reuse, flipped/incomplete motifs, palette-off, changed graphics, dense-map
  overflow, large wrapped scrolls, HUD/world bounds and the two-halo budget.
- Lighting/environment/widescreen/model timing and diagnostic summary tests
  pass. Optional ASan/UBSan could not link because the host sanitizer runtimes
  are missing; regular regression builds and strict source warnings pass.
- Native AC83→AC5A door sequence: all 106 captured frames at both viewports keep
  lighting applied, HUD exact and OFF output identical. Previews replay native
  snapshots in host software, not the physical 3DS GPU.
- PC model + vertex packing versus `f69c976`: 30 scenario pairs, median nine
  400-frame batches after warmup. Seven scenes at width 400 and three changed
  scenes at width 256, each stationary, synthetic camera motion, and motion
  with a BG1 map descriptor invalidated every four frames. Median changes range
  -2.82% to +7.21% (-0.0031 to +0.0085 ms). The stress mode approximates map
  invalidation, not a native traversal recording. First-build timings are also
  recorded separately but are single cold samples including atlas decoding.
  GPU/glow draw, upload, audio and whole-game timing are excluded. Matched Old
  3DS dumps are still needed for device cost/FPS.
- CIA/3DSX build succeeds with FULL_NATIVE/LTO, OLD3DS, PHASE_DIAG, LIGHTING_TRIAL
  and SYSTEM_CORE_PERCENT=30. Logs, software comparisons, raw performance data
  and checksums are saved beside the packages.

## Background-guided lighting

Latest trial: `output-lighting-background-trial/`, based on `0717f62`.
Ambient hue samples six actual eight-pixel tile rows per layer (48 pixels),
excluding transparent/unused palette entries. Colored BG2 samples determine
the hue; BG1 is a fallback. Existing bounded foreground/background modulation
and contact depth remain. Landing Site's normal appearance is byte-identical.

Local BG2 lights require fingerprints of complete known art, not brightness:
two 16×16 blue organic motifs from Green Brinstar (an artistic interpretation
as bioluminescence), and red/blue 8×8 machinery status icons from Tourian.
Indices alone are not sufficient; all motif graphics and map parts/flips must
match. Identical adjacent LED icons form one panel. Live CGRAM supplies color
and intensity, so unpowered/faded palettes cease emission. BG2 tilemap banks,
flips, viewport bounds and its own parallax position determine placement.
Full native collision blocks conservatively reject a buried source center.

At most the nearest two background emitters are selected, within the existing
8-source total; native doors/liquids and player lights keep their budget first.
Most tilesets exit after four short graphics fingerprint checks. The same
assets are recognized wherever these descriptors/graphics occur, without room
IDs; this does not recognize every lamp or every tileset in the whole game.
No normal maps, ray tracing, extra texture or new rendering pass is added.
Up to two more halos use the existing glow texture/pass. The approximation can
still illuminate across obstacles or change nearest sources while moving.
Scanline-warped/disabled BG2 skips local motif sources, retaining ambience.

Diagnostics advance to schema 6, appending `background_sources` to CSV and
`*_background_sources_total` to summary cohorts. All prior columns remain in
order. Existing `light_prepare_us` includes motif lookup; full model/ON-OFF spans
include the color field and tile modulation. No per-tile timing calls are added.
Model tag: `cached-color-grid-16px-background-motifs-v3`. ARM lighting state
7052→7056 bytes; the 120-frame diagnostic ring 13920→14400 bytes. These are
individual allocations, not total memory/stack/code measurements.

Validation and artifacts:

- Ten native scenes × two viewports: HUD exact, all OFF renders exact versus
  the previous trial, Landing Site ON exact. Two BG2 sources in Green Brinstar
  and Tourian; no BG2 points in the other eight scenes. Comparisons are host
  software renders of native snapshots, not device GPU captures.
- Twenty scene/viewport pairs × ten cold/warm double-atlas frames: quad bytes
  stable and instrumented/plain results identical. Positive native motif
  regression additionally tests mirrored/complete flowers, paired LEDs,
  palette-off, changed art at identical indices, parallax/origin, solid-block
  occlusion, warped registers, BG2 bounds and the total source budget. It reads
  local native snapshots, storing no ROM graphics in tracked source.
- Unit checks for unused palette colors, BG2 precedence/fallback, lighting,
  environment, widescreen, diagnostic schema/totals and model counters pass.
- The existing 106-frame native AC83→AC5A door route is checked again in both
  viewports for retained lighting, unchanged HUD and byte-identical OFF output.
- CPU model/vertex benchmark versus `0717f62`: six scenes × stationary/moving
  synthetic camera, width 400, median nine measured 400-frame batches after
  warmup. Two motif scenes add about 0.010–0.012 ms (+7–8.3% model work). Others
  range -2.7% to +0.7%; raw data in `performance-host.json` / `.txt`. This excludes
  GPU/glow drawing, audio, upload and whole-game FPS; Old 3DS matched dumps are
  needed to measure the physical device cost.
- CIA/3DSX retain FULL_NATIVE/LTO, OLD3DS, PHASE_DIAG, LIGHTING_TRIAL and
  `SM3DS_SYSTEM_CORE_PERCENT=30`. Game timing, clocks, physics and lifecycle
  remain unchanged. The user previously reported suspension working.

Reproduce the native motif checks with existing local fixtures:

```sh
cc -O2 -w -iquote sm -Isource tools/lighting_background_check.c \
  source/scene_lighting.c source/ppu_gpu_model.c -o /tmp/sm-background-check
/tmp/sm-background-check output-lighting-biomes/brinstar-verde \
  output-lighting-biomes/tourian
```

## Continuous door lighting and contact depth

Latest trial: `output-lighting-transition-trial/`, based on `6434e33`.
The reported bright flash has a direct cause: `SceneLighting_Begin` previously
accepted only gameplay state 8, so entering door states 9-11 restored the
original unlit render. The native door coroutine fades palettes and returns to
state 8 only after the new room's palette has finished appearing. Lighting is
now retained through states 9-11. Samus physics, game logic and frame timing
are unchanged; unsupported PPU modes/brightness fades and X-ray still keep
their existing fallbacks.

Halo opacity follows the fading scenery/Samus palette during these states,
excluding the HUD palettes and their deliberately preserved door-fade colors.
It is zero for blank playfields or a fully faded scenery palette. Colored
modulation remains applied to the original fading tile colors, so it does not
replace the game's fade with an invented transition animation.

For depth outside Landing Site, the existing cached color grid now includes a
small contact-shading mask from native room collision blocks and their immediate
neighbors. Background art near full solid blocks/corners is darker, and the
distant layer behind foreground solids receives a bounded extra reduction.
Sprite readability and the normal Landing Site look are preserved. BG2 palette
samples also contribute to colored diffuse ambience, weighted more than BG1
samples. There are no new point emitters, texture-brightness classifications,
normal maps, per-pixel light rays or extra normal-gameplay render passes.

This is a coarse 16-pixel approximation of local contact depth, not a physical
shadow solver: slopes and BTS extension blocks are conservatively ignored;
lights can still reach across obstacles. Dynamic full-block/door changes are
part of the cache key, so opening/destroying a block updates immediately. A
one-cell occupancy border is bounded by room dimensions, including negative
camera coordinates. The compiled lighting state grows from 6,592 to 7,052
bytes (+460); this figure excludes stack, code and existing textures.

Research references: [Godot's 2D lighting documentation](https://docs.godotengine.org/en/4.5/tutorials/2d/2d_lights_and_shadows.html)
separates ambient modulation, light sources, layers and occluders;
[GPU Gems' ambient occlusion chapter](https://developer.nvidia.com/gpugems/gpugems/part-iii-materials/chapter-17-ambient-occlusion)
describes geometry-dependent visibility and reusable shading. These principles
motivate the bounded contact approximation; those engines' shader algorithms
are not implemented here.

Validation:

- Native AC83 → AC5A door route: 106 captured frames, including state 9, 104
  frames of state 11, and return to state 8. State 10 completes within the
  native coroutine tick; the separate state-guard test covers it. Both 256/400
  model renders retain applied lighting throughout, preserve HUD rows 0-30,
  and match the previous unlit render byte-for-byte when disabled. Halo opacity
  traverses 13 levels from 65 to zero and back. No model fallback occurred on
  this captured route; this is not a device GPU/sleep validation.
- Tracked `door-left` preview mode reproduces WRAM, captured lines and state
  exactly; PPU snapshots agree after ignoring their three process pointers.
  The route uses diagnostic positioning/health only in host RAM and writes to
  a separate preview SRAM path, leaving the original save unchanged.
- Ten biome scenes × two widths × ON/OFF: HUD unchanged, all OFF images exact,
  and Landing Site ON exact. Visual improvements vary by room; Green Brinstar
  is subtler than contact edges in Maridia. Software comparisons and the native
  route GIF are in the output folder, not hardware screenshots.
- 22 scene/viewport pairs × ten cold/warm frames: instrumented/plain quad
  output equal; cache and shadow counters agree. Lighting/environment,
  widescreen, diagnostic summaries and model instrumentation tests pass.
- Host CPU model/vertex benchmark versus `6434e33`: four scenes, stationary
  and synthetic camera motion at width 400, nine measured 400-frame batches
  after warmup. Median changes range from -0.8% to +2.0%. Raw data is
  `performance-host.json` / `performance-host.txt`. It excludes device GPU,
  audio, cache-clean services and total game speed.
- FULL_NATIVE/LTO CIA/3DSX builds retain OLD3DS, PHASE_DIAG, LIGHTING_TRIAL and
  `SM3DS_SYSTEM_CORE_PERCENT=30`. Dumps retain schema 5, with the model tag
  `cached-color-grid-16px-contact-depth-v2`; obtain matched device ON/OFF dumps
  to measure the actual cost.

To reproduce the native route with a fresh output directory:

```sh
python3 tools/build_lighting_preview.py
mkdir -p output-lighting-transition-trial/door-new
output-lighting-trial/lighting-preview USER_ROM USER_SRAM \
  output-lighting-transition-trial/door-new door-left
python3 tools/build_lighting_benchmark.py --reference 6434e33
```

## Hardware lighting measurements (schema v5)

`output-lighting-diagnostics-trial/` extends the lean trial with lighting
diagnostics; visual behavior, frame limiter, game logic and the 30% system-core
allocation are unchanged. The earlier CIA only reported lighting preparation
inside `bg_main` and shading/shadow creation inside background/object phases.
It could support matched total-render ON/OFF comparisons, but did not identify
the lighting mode or split out preparation, shadows and glow submission.

New `info.txt` and `frame-times.csv` entries identify applied lighting, source
counts, shadow/glow quads, grid reuse/rebuild, preparation and submission CPU
spans, plus the whole model-build span. Recent data covers 120 rendered frames;
session data keeps cumulative ON/OFF GPU-gameplay cohorts. These timings are
nested in existing PPU/top phases. They are not an isolated GPU timer and do
not isolate the per-quad color lookup or silhouette creation. Measure their
extra cost using ON/OFF captures in the same room/position/viewport, as
described in `docs/BUILDING.md`; global session cohorts can mix scenes.

Only a few clock reads are added per lit frame, with no per-tile timers, extra
render passes or SD writes from rendering. Unprofiled/CPU-fallback samples are
marked unavailable; ON requested but suppressed by X-ray/fades is not used as
an OFF baseline. Diagnostic regression tests cover clock conversion, cohort
filtering, recent-ring eviction and retention of session totals.

Verification: frame-diagnostics and model-lighting diagnostics regressions,
existing lighting/environment/widescreen tests, and 22 native-capture/viewport
pairs with ten cold/warm frames each. Instrumented and ordinary model builds
emit identical quad bytes in those captures; cache-rebuild and shadow counters
agree with actual model output. FULL_NATIVE/LTO 3DSX and CIA builds pass. Actual
Old 3DS timing values still require user dumps.

```sh
cc -std=c11 -Wall -Wextra -Werror tests/frame_diagnostics_test.c \
  source/frame_diagnostics.c -o /tmp/sm-frame-diagnostics-test
/tmp/sm-frame-diagnostics-test
cc -O2 -w -DSM3DS_PHASE_DIAG -Itests/profile_stubs -iquote sm -Isource \
  tests/lighting_diagnostics_test.c source/ppu_gpu_model.c source/scene_lighting.c \
  -o /tmp/sm-light-diagnostics-test
/tmp/sm-light-diagnostics-test
```

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
Landing Site (`room=0x91f8`). Other rooms now use similarly dark ambient light,
with a mild cool bias and a darker distant layer, so local sources stand out.

Loading/fades, pause screens, frozen-time X-ray, the expanded eye beam and CPU
fallback retain normal output. Thus the effect is global for compatible PICA
room rendering, not for every cinematic/special PPU mode. Environmental sources now include live colored door caps, visible lava/acid,
and an approximate detector for small colored luminous cores in BG1/BG2.
There are no individually authored lamp maps for every tileset.

- A cooler, darker distant BG2 separates scenery from the foreground. BG1 and
  objects have stronger ambient light; nearest texture sampling stays intact.
- Three warm light sources track the ship's hatch/side lamps in world space.
  Samus has a small teal fill light. Charging and active player projectiles add
  lights, with a fixed budget of eight sources and squared radial falloff.
- Tile luminance gradients approximate directional surface relief. This is
  per-quad shading, not an authored normal map or per-pixel lighting shader.
- Samus's palette-4 OAM tiles near her position project alpha silhouettes onto
  low-priority BG2 at the ship, and low-priority BG1/BG2 walls in interiors.
  High-priority foreground depth hides them. The blend preserves the SNES
  math alpha and does not write depth/stencil. Weather and HUD do not cast
  shadows. Interior projection points away from the nearest environmental
  source in range; unlit corridors use a stable diagonal. This is approximate,
  not physical ray tracing.
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
room; `slot1-move` also walks Samus out of the save station before capturing.
`acid` uses the game's Norfair load-station 10 in room `b236`, with
explicit diagnostic suit/health in host RAM so it can survive capture. This
option belongs only to the host tool and is not compiled into the game.
`lava` wraps the host's load-station entry to spawn in actual room `ac83`
via door `952e`, with camera (0,0) and Samus (128,152). The normal ROM room
header/FX loader selects type 2 and surface Y=198; the tool rejects a capture
without visible real lava. It does not substitute lava graphics into acid.
Use a
separate output directory for each.
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

## Environmental lighting extension

Environmental build artifacts are now under `output-lighting-environment-trial/`.
The same VIDEO → LIGHTING switch controls the entire effect.

- FX types 2 (lava) and 4 (acid) add a row of two sources at width 256 or three
  at width 400, following the live `lava_acid_y_pos`. Their color follows the
  visible liquid's BG3 palette, with a warm fallback. Ordinary water (type 6)
  does not emit. Offscreen liquid below the viewport does not add a glow;
  immersed views use a warm fill near the playfield rather than a source above
  the HUD. Sources are limited to the visible room bounds.
- Green/red/yellow/blue caps use the shared live block graphics from bank-84
  draw definitions A767/A827/A8E7/A9B3, including vertical/horizontal layouts and
  flips. Thus blue doors work without an active PLM, and an opened/removed cap
  cannot leave a persistent light behind. At most two nearby colored caps emit.
  Grey/eye doors and unusual custom door graphics are not separately authored.
- BG1/BG2 sampling groups small colored luminous cores into lamp sources, with
  at most two nearby groups. Classification requires a small bright colored
  region surrounded by dark pixels; white metal highlights and broadly bright
  wall tiles are rejected. This is a heuristic, not semantic lamp metadata or
  ray-traced occlusion: unusual tilesets may need threshold/mask refinement.
  Ship lamps retain their explicit sources. BG3 weather/liquid and HUD are not
  sampled as decor lamps. Background windows and room edges are respected.
- The source budget remains eight. Samus/charge and up to three active
  player projectiles get priority in ordinary rooms (the moving-light count is
  capped at four), then liquid, doors and available lamp slots. Ship builds use
  a moving-light count cap of six, including the three ship lights. Remaining
  projectile sources can be dropped when the budget is full. The lamp pass
  primes the existing descriptor cache, so background emission reuses its tile
  lookups. Lamp classifications are cached once per tile within the frame and
  reset for palette animation; there is no additional GPU readback or render
  target. The lighting struct is now 18,120 bytes (2,056 more than the previous
  global build), plus the reusable 16-KiB glow texture.

Verification: `tests/scene_environment_test.c` covers colored cap hues and
centers, open/removed doors, liquid palette colors and levels, water exclusion,
room bounds, capped counts, wall/white-highlight rejection and classification
invalidation. Existing scene, atlas, widescreen and 264-frame differential tests
still pass. Native Brinstar `a70b` captures show Samus, a blue cap at (24,128), and
two green/yellow lamp groups near (112,44)/(80,52). The native Norfair `b236`
fixture has FX type 4, world liquid Y=697, camera Y=512: its three wide liquid
sources follow screen Y=185 and use orange palette RGB (116,82,0). It emits six
sources including Samus/two luminous decor groups. Software previews are in the
ignored environment output directory. They are not GPU captures.

Environmental detection costs additional CPU compared with the previous global
build; the eight-source limit bounds field/glow work, not total game FPS. A
matched warm-cache Brinstar host sample at width 400 measured about 0.216 ms
with four sources, versus 0.164 ms with only Samus in `2eb526e`; lights-off was
about 0.099/0.096 ms (small host timing variation). Norfair's denser wide fixture
measured about 0.393 ms with six sources and 0.178 ms off. These numbers include
model/vertex preparation only, exclude GPU/glow cost, and are not console FPS.
The earlier optimization table describes the ship/global build before this
extension. Real-console performance and a campaign-wide visual pass remain
pending. No original user SRAM is used as a preview write destination.

## Stronger interior diorama trial

Builds and comparisons: `output-lighting-diorama-trial/`. Ambient vertex RGB is
now BG1 (150,158,170), BG2 (88,101,121), OBJ (188,196,208), compared with the
previous neutral 218/184/226. Samus keeps a small local fill. Landing Site's
existing appearance is unchanged in the matched standard stationary preview.
Colored door radius increases from 68 to 88 pixels, with stronger colored light;
liquid radius increases from 120 to 160 and its brightest palette channel from
116 to 154. Decor lamp radius increases from 52 to 76 and emission from half
to three quarters of the sampled core color. The eight-source budget remains.

Interior silhouettes use depth 8100, above BG1-low (8000) and below BG1-high
(c000), with alpha 96 instead of 78. The ship keeps depth 7200/alpha 78. Their
offset is computed once per frame, up to 12 pixels away from the nearest room
light; own fill, charge and projectiles do not steer it. The shadow quad limit
and number of shadow passes are unchanged. Larger light radii visit more field
cells and cover more glow pixels, so the fixed source count alone does not
prove identical hardware cost.

Visual QA includes Brinstar `a70b` both stationary and after moving to (160,161),
Norfair acid `b236`, and actual lava room `ac83`. The standard lava sample has
six sources and 22 sub-screen shadow quads, with 203 shadowed raster samples;
moving Brinstar has four sources, 15 main shadow quads and 602 shadowed samples.
Software raster statistics confirm depth-tested shadow contribution, not just
generated quads. These counts are not perceptual or hardware measurements.
Off/on and previous/new PNGs use identical captured frames and nearest
enlargement without contrast adjustments. No original SRAM is a write target.

Scene tests additionally verify interior ambient/layer separation, shadow
receiver depth/alpha, projection away from doors and lava, exclusion of own
charge, and stronger bounded door/liquid sources. The 264-frame lights-off
main/sub differential check and atlas/widescreen/vertex tests pass. GPU output
and FPS for this revision still need interactive Azahar/3DS verification.

## Comparisons by area and biome

`output-lighting-biomes/biomas-vista-general.png` compares ten matched frames
with lighting off/on at native 256x224, without contrast processing. Individual
`*-comparacion.png` files enlarge each pair by two using nearest sampling.
The selected scenes cover all six main areas, four Brinstar environments and
two Maridia environments; they are samples, not an inspection of every room.

The host-only preview also accepts `station:AREA:INDEX` (area 0..5, index 0..18).
It reads the actual ROM load-station table, rejects invalid/cross-area entries,
loads the normal native room/FX state with diagnostic suits and health, and
verifies that gameplay reached the requested room. Original SRAM remains only
an input. This tool change does not affect the compiled game.

| Comparison | Native room | Diagnostic entry |
| --- | --- | --- |
| Crateria ship | 91f8 | Existing slot-2 capture |
| Green Brinstar | 9ad9 | station:1:8 |
| Blue Brinstar | 9e9f | station:1:9 |
| Pink Brinstar | 9d19 | station:1:18 |
| Red Brinstar | a322 | station:1:10 |
| Norfair lava | ac83 | Existing lava capture, FX 2 / Y=198 |
| Wrecked Ship | ca08 | station:3:16 |
| Maridia caverns | d617 | station:4:18 |
| Maridia sand | d1dd | station:4:16 |
| Tourian | daae | station:5:8 |

All native captures reached state 8 and both raster passes were accepted.
Off/on HUD pixels match exactly in all ten pairs. New unlit frames differ from
the native CPU reference by approximately 0.65..1.67 mean RGB channel levels
out of 255, reflecting software color-math quantization; these are not direct
Azahar screenshots. Logs and capture/comparison manifests are in the ignored
output directory. Scenes without large emitting sources primarily show darker
scenery and Samus silhouettes; this does not establish complete authored lamp
coverage in every tileset.

## Colored ambience and 3DS cache optimization

Latest builds: `output-lighting-3ds-color-trial/SuperMetroid3DSPort.{3dsx,cia}`,
compiled with native/LTO/OLD3DS/phase diagnostics and the LIGHTING toggle.
Visual comparisons against the previous lit revision are in
`output-lighting-3ds-color-trial/repaso-colores.png` and individual PNG pairs.
Eight samples were reviewed: green/blue/red/pink Brinstar, actual Norfair lava,
Maridia caverns, Wrecked Ship and Tourian.

Interior ambient light now follows colored foreground scenery palettes. Six
native tilemap samples select BG palettes below the HUD; chromatic colors
contribute a bounded tint, up to +/-12 vertex levels on BG1 and +/-24 on BG2.
Thus vegetation, blue caverns and red/amber rooms get distinct color bounce
without extra point sources, halos, render targets or GPU passes. Objects retain
their readable ambient level and Samus's own local fill. This is diffuse
artistic ambience, not a claim that every green leaf is an emissive lamp.
Landing Site retains its existing authored ambient/light profile.

Lamp classification is retained as RGB555 plus a valid bit in the old padding
of `PicaTile`; its size remains 80 bytes. Decode resets this metadata on VRAM,
palette or slot changes; atlas initialization clears it too. The 3DS's two
atlas buffers each retain their own metadata. The selected two decor lights
are reused for at most three intervening frames if the sampled view and first
128 palette colors are unchanged. Camera, scroll/window, room/bounds and palette
changes refresh immediately. Pure VRAM animation in an otherwise identical
view can take at most three frames to update this approximate decor detector.
Doors, liquids, projectiles, shadows, ambience and the light field still update
every frame. A full light budget or disabled effect invalidates the lamp cache.

Relief retains 512 total entries, split into two 256-entry banks for the two
atlas buffers. Content revision validates palette/tile/eviction changes, and
atlas initialization gets a new revision even at the same address. The host
scene struct is 20,520 bytes, about 2.4 KiB more than the previous build; the
reusable glow texture and the eight-light/source and shadow limits are unchanged.
Caller-owned scene structs must be initialized to zero once.

Matched host benchmark against `a08035f`, alternating two atlas/pixel buffers,
width 400, same native captures/four-vertex callback and source counts; median
of nine 600-frame warm batches after discarding warm-up:

| Lit scene | Previous CPU model | Current CPU model | Reduction |
| --- | ---: | ---: | ---: |
| Landing Site | 0.182912 ms | 0.179214 ms | 2.0% |
| Green Brinstar | 0.226508 ms | 0.178395 ms | 21.2% |
| Norfair lava | 0.482735 ms | 0.453904 ms | 6.0% |
| Maridia caverns | 0.570142 ms | 0.541756 ms | 5.0% |

Lights-off is essentially unchanged within host timing variance. This is
stationary-view CPU model/vertex work, not console FPS, GPU time or total game
speedup. Scrolling and animated palettes refresh detection more often, so they
can reduce the cache benefit. Actual Old/New 3DS FPS still needs device testing.
Raw results: `performance-double-atlas-host.txt` in the output directory.

Verification adds actual model/atlas tests for warm lamp reuse, bounded refresh,
palette color changes, window/scroll invalidation, rejection of broad bright
walls, atlas reinitialization and double-buffer relief separation. Cold and
eight warm frames emit identical quad bytes in all eight native samples at
both widths (16 checks). The 264-frame lights-off main/sub differential test,
widescreen, tile cache, dirty atlas and vertex checks pass. Preview output is
still software rasterization rather than an Azahar GPU capture.

## Upper playfield boundary correction

The game switches its PPU registers after SNES scanline 31. Scanline 32
is drawn into zero-based visible row 31, so the HUD occupies rows 0..30.
The former exclusive bound of 32 left the first playfield tile chunk
unmodified: a quad starting at row 31 was excluded from lighting in its
entirety, sometimes leaving an eight-row bright strip. `kHudEndLine` is
now 31 for lighting, glow clipping and the viewport masks. The host preview
uses this same constant instead of a separate hard-coded boundary.

`output-lighting-sleep-trial/` contains regenerated before/after previews
for all ten biome captures. At both 256 and 400 pixels, HUD rows 0..30
remain byte-identical, the lights-off image remains byte-identical, and
the top eight playfield rows receive the effect. The verification record
is `topline-verification.json`; `biomas-borde-corregido.png` is the overview.
These are host raster previews. The lighting regression covers row 30
remaining untouched and a BG quad starting at row 31 receiving the tint.

## Lean lighting with verified emitters

The previous detector could treat a bright colored texture highlight as a
lamp. More significantly, the cap detector accepted isolated low block IDs:
an ordinary solid `0x8000` could become a green door light. Remove the
texture-brightness lamp pass and require the complete four-block door cap,
including the graphics/flips in bank-84 draw definitions A767/A827/A8E7/A9B3.
Closed and opening caps are accepted; isolated pieces, solid walls and open
caps are rejected. Select at most the nearest two valid caps. Keep the three
authored Landing Site lamps, native lava/acid surfaces, Samus fill/charge and
bounded projectile lights. Ordinary water still does not emit. Colored room
ambience stays diffuse and does not add another spotlight.

The lighter renderer precomputes finished BG1/BG2/OBJ colors in a 16-pixel
grid. Both SNES screens read those same RGB bytes. An exact key of sources,
colors, ambient values and viewport dimensions reuses the map until something
changes; player/source movement updates immediately without quantization of
the source position or delayed frames. Background emission reads the map
inline. The unlit renderer still selects its ordinary callback once per frame.

Remove texture-gradient relief calculations and their atlas caches. The
depth effect now relies on darker distant layers, local environmental light
and the projected Samus silhouette. The retained shadow depth, clipping and
nearest environmental-source direction are unchanged. Light sampling is
coarser than the old 8-pixel field; arbitrary decor lamps need verified room
fixtures before they will emit again. No ray-traced geometry/occlusion is added.

The compiled ARM ELF's `g_lighting` shrinks from 20,508 to 6,592 bytes (67.9%).
The host struct shrinks from 20,520 to 6,592 bytes. This is lighting state,
not total application RAM. Atlas layout, the 16 KiB glow texture and shadow
capacity are retained. Fewer false sources also reduce glow draws in affected
rooms; actual GPU timing still needs hardware measurement.

Matched host medians against `d8e57b8`, width 400, alternating two atlas
buffers, four-vertex packing, nine measured 400-frame batches after warm-up:

| Scene / camera | Previous lit CPU | Lean lit CPU | Total model saving | Lighting overhead saving |
| --- | ---: | ---: | ---: | ---: |
| Landing Site / stationary | 0.185739 ms | 0.140925 ms | 24.1% | 59.1% |
| Landing Site / moving | 0.189030 ms | 0.149443 ms | 20.9% | 49.4% |
| Green Brinstar / stationary | 0.180896 ms | 0.135735 ms | 25.0% | 55.1% |
| Green Brinstar / moving | 0.216788 ms | 0.145817 ms | 32.7% | 62.7% |
| Norfair lava / stationary | 0.458684 ms | 0.326669 ms | 28.8% | 54.5% |
| Norfair lava / moving | 0.506658 ms | 0.346333 ms | 31.6% | 58.8% |
| Maridia caverns / stationary | 0.547774 ms | 0.423724 ms | 22.6% | 50.1% |
| Maridia caverns / moving | 0.571871 ms | 0.442059 ms | 22.7% | 48.6% |

Lighting overhead is each revision's lit time minus its own lights-off time;
raw on/off measurements and ranges are in `performance-host.json`. Moving
cases shift the camera/scroll over the native capture synthetically, not an
end-to-end engine replay. These are CPU model/packing measurements, not GPU
times, console FPS or whole-game speedups. They include intentionally fewer
emitters and the simpler surface treatment. No console-performance claim is
made from desktop timings.

`tools/build_lighting_benchmark.py --reference d8e57b8` builds independent
previous/current executables against their own revision's headers, avoiding
scene-struct ABI mismatches. Example invocation:

```sh
build-lighting-benchmark/current-lighting-benchmark \
  output-lighting-biomes/norfair-lava 1 1 400
```

The CIA/3DSX in `output-lighting-lean-trial/` use FULL_NATIVE/LTO and retain
`SM3DS_SYSTEM_CORE_PERCENT=30` from the NWM isolation trial. This lighting
change does not establish a wireless/sleep crash fix. Real Old 3DS testing
must check frame rate, audio and the previously reported Wi-Fi/repose sequence.

Verification: all ten biome captures at 256/400 pixels preserve HUD rows
0..30 and byte-identical lights-off pixels; captured native fire/jump/move
frames pass those checks too. Twenty cold/warm double-atlas checks produce
identical quad bytes. Tests cover solid-wall false positives, complete colored
caps, opening/removed caps, nearest-two selection, texture highlights producing
no emitter, palette-driven ambience, immediate source movement and bounded
shadows. Widescreen/cache/dirty/indexed-vertex/lifecycle tests and the 264-frame
lights-off main/sub color/alpha differential check pass. Biome comparisons and
`biomas-modo-ligero.png` remain host raster previews, not console screenshots.

The precomputed-color approach follows the general principle described in
[Apple's vertex light-map documentation](https://developer.apple.com/documentation/modelio/mdlmesh/generatelightmapvertexcolorswithlights(toconsider:objectstoconsider:vertexattributenamed:)):
reuse computed lighting rather than recalculating it for each primitive.
This port uses a small dynamic color grid, rather than Apple's API or OpenGL.
