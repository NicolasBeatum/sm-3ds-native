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
