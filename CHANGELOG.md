# Changelog

## 0.2.0 - 2026-10-19

A start-to-finish playthrough has been completed on Old 3DS using a development
CIA. This verifies a route to the ending, not every route or 100% completion.
Packages contain no game ROM or translation patch.

### Atmospheric lighting

- Add an optional game-wide lighting mode using PICA200 vertex colors, cached
  ambient fields, colored doors, lava, background lamps and elevator platforms.
- Add a soft light around Samus, selected luminous enemies and short shadows
  clipped to nearby receiving surfaces. Limit active lights to eight, including
  at most two visible enemies, to control the cost on Old 3DS.
- Refine exterior sky handling, Norfair color, background source detection,
  directional stability and room-transition continuity. Preserve original
  textures; this is stylized lighting rather than normal maps or 3D geometry.
- Document OFF/ON GIFs and lossless comparisons across ten areas, including
  green, red, pink and blue Brinstar, plus an elevator comparison.

### Maps and interface

- Add independent map QoL pages with explanations and persistent settings:
  item symbols, colored doors, room colors and station icons.
- Offer OFF, ZERO/FUSION (pending circle, collected dot) and SUPER METROID
  (pending native dot, collected circle) item styles. Read native collection
  bits; keep a cell pending until all its items are collected. Only alter
  explored native item cells.
- Apply the selected style to the pause/lower maps and optionally the in-game
  minimap through its own persisted toggle. Correct native HUD map row offsets.
- Draw consistent one-screen-pixel door dividers on each directed door face.
  Use that face's native unlock state and turn it blue after unlocking.
- Color explored heat, lava, acid and secret rooms using current progress.
  Adjust Crateria's color to keep cyan doors readable.
- Mark lower-map energy, ammo and map stations with E, A and M glyphs adapted
  from the native font. Preserve native pause-map and upper-minimap stations.
- Show Samus as a rounded four-pixel red dot with a dark rim at every map zoom.
- Add English/Spanish lower-screen UI under HUD → LANGUAGE; move map QoL into
  MAP → MAP IMPROVEMENTS.
- Add five upper HUD modes: full, energy, energy plus minimap, hidden with
  expanded scenery, and hidden with the classic black band. Reduced HUDs have
  transparent backgrounds; crop the minimap to its native five-by-three cells.
  Reveal the 31 scenery rows behind the HUD without stretching or moving the
  native camera. Reference-rendered special effects retain the classic band.

### Correctness, performance and diagnostics

- Restore native Shaktool movement through compiler aliasing correctness.
- Reduce repeated BG3 water/quicksand strip work. Batch periodic 4bpp BG2
  scenery under water HDMA, preserving palette/VRAM updates, transparency,
  priority, windows and per-tile lighting. Use the existing atlas with separate
  liquid/scenery cache budgets and a normal-tile fallback.
- For the two October 19 Maridia reconstructions at 400px with lighting ON,
  reduce geometry from 4,877 to 3,451 and from 4,701 to 3,211 pieces. Host CPU
  model/packing time improves about 4% in the stationary cases. These are host
  reconstruction measurements, not new console FPS results. The original
  Old 3DS dumps measured approximately 38 and 39 FPS; hardware retesting remains.
- Preserve original SPC/DSP audio; isolate audio queue/locking work and improve
  audio/DSP lifecycle handling around HOME, suspension and Wi-Fi transitions.
- Add separate lighting preparation, geometry and presentation measurements to
  dumps, keeping game clocks, frame pacing and native gameplay speed intact.
- Refresh English/Spanish README documentation, gameplay badges and actual
  emulator captures; remove temporary capture tools and investigation journals
  from tracked release files.

## 0.1.5 - 2026-10-02

Configurable HUD and video layouts, widescreen rendering and enemy corrections,
background performance work, and improved suspend/resume handling. CIA and 3DSX
use the same native engine and contain no game ROM or translation patch.

### Lower screen and video

- Add ONLY AMMO, AMMO + HOOK and ALL ITEMS status-bar layouts. Reserve slots
  for unowned items, keep health larger, and support touch selection of Grapple
  and X-Ray when equipped.
- Add persisted VERTICAL and HORIZONTAL ammo-number layouts and an independently
  configurable floating X-Ray map shortcut. The shortcut selects the scope;
  normal game controls activate it.
- Group SETUP options into HUD, MAP and VIDEO, keeping all options accessible
  when its status bar is enabled.
- Add FIT, STRETCHED and 1:1 upper-screen presentation modes. Keep nearest
  filtering and stabilize sampling at pixel boundaries to address inconsistent
  black pixels on native HUD/text edges without replacing game graphics.

### Rendering and performance

- Reduce repeated widescreen background boundary checks and compare relevant
  scanline state in a single pass. Preserve HDMA, palettes, color math and
  layer priorities.
- Share four GPU vertices per quad using an index buffer instead of writing
  six duplicated vertices. Preserve triangle order, UVs and depth; reduce
  vertex writes by one third and save about 2.6 MiB of linear buffer memory.
- Extend X-Ray's native integer cone and scanned block/item reveal rules into
  both side bands while preserving the captured center window. Keep empty
  room margins black and cache scanned side tilemaps during a frozen view.
- Restore native through-wall movement for the three pipe/wall-bug variants.
  Keep their flight active in the extended viewport and their original
  hide/reset cycle when they leave it.
- Keep AI and animation active for partially visible normal and multipart
  enemy sprites beyond their collision bounds. Extend Reo's activation range
  only in the added side bands; retain Dessgeega's native jump/collision rules.
- Add dump markers identifying the background, geometry, X-Ray and enemy
  revisions for comparisons between builds.

### HOME, sleep and audio

- Pause audio for HOME/sleep transitions and defer audio resume until DSP
  restoration has completed. Restore the foreground CPU budget and reset
  timing/stale input after resume or a long external pause.
- Update the NDSP backend to wait while suspended, wake on DSP transitions,
  and bound audio shutdown waits.
- Record suspend, sleep and external-pause counts in dumps. Rosalina entry
  freezes cannot be guaranteed fixed by these changes and still need broader
  hardware testing.

The user confirmed the current trial works in gameplay. Performance still
varies by room, effect, hardware and widescreen setting; 60 FPS is not
guaranteed. A complete playthrough and 100% completion remain unverified.

## 0.1.3 - 2026-09-30

Performance, map responsiveness, diagnostics and HOME Menu presentation update. Both packages use the same native engine and contain no game ROM or IPS patch.

The port is playable, but a full playthrough and 100% completion have not been verified. Completion from beginning to end is not yet confirmed, and performance varies by scene and hardware. HOME Menu animation and sound require verification on a real console.

- Show the detected Old/New 3DS hardware family in SETUP's information tab,
  independently of the build flags.
- Cache decoded lower-screen map tiles and colors, and rebuild the map texture
  only when visible state changes. Queue configuration writes on a worker so
  map controls do not wait for microSD writes; flush pending settings on exit.
- Select sprite pieces by scanline membership, preserving OAM order, wrapping
  and the original sprite/tile limits. This reduces repeated selection work
  for multipart enemies.
- Apply captured background scroll offsets to the widescreen side bands and
  room bounds so camera shakes continue across the extended viewport.
- Add whole-session timing totals, separate gameplay statistics for standard
  and widescreen modes, and sector/scroll-change events to diagnostic dumps.
  Keep the last 120 frames as a separate detailed sample.
- Use the chosen eight-frame Samus and gunship GIF as a flat animated CIA
  HOME-menu banner, with a three-second opening music cue rendered by the
  native SPC player. Keep the previous pixel-art composition as an alternate
  asset and retain the small HOME icon.
- Resolve repeated background tile/palette descriptors once per GPU frame,
  sharing the result between main and sub layers. Layers with changing
  graphics banks retain the original lookup path. VRAM and palette changes
  are revalidated on the next frame; geometry, HDMA and gameplay are unchanged.
- Identify this renderer revision and LTO setting in diagnostic dumps for
  hardware comparison. Old/New 3DS model detection and automatic New 3DS
  CPU/L2 acceleration remain enabled in both package formats.

- Add five Azahar screenshots, an English/Spanish README pair, an explicit and respectful AI disclosure, and installation links to Releases. Use NicolasBeatum consistently for project attribution.

## 0.1.1 - 2026-09-29

Hardware performance and diagnostics update. Both release packages remain
ROM-free and use the same native game engine.

- Reduced background tile comparisons in normal-width and inactive spans,
  and skipped color-math work when a scanline does not use it.
- Matched the installed CIA's system-core CPU-time limit to the 3DSX limit,
  removing a packaging-specific performance penalty on Old 3DS.
- Added recent frame timings and display-buffer captures to debug dumps so
  hardware performance can be compared across builds.
- Made shutdown complete promptly when HOME or POWER requests it.
- Replaced the large HOME-menu banner with a cartridge design based on the
  existing Samus helmet art. The small HOME icon is unchanged.

The widescreen renderer and gameplay remain unchanged by the release
packaging. User testing found the current build playable on original hardware;
performance still varies by room and effect.

## 0.1.0 - 2026-09-28

First public ROM-free release. The startup ROM selector now shows the game
title, version and project credits on the upper screen, including
NicolasBeatum as project director and tester. The 3DSX and CIA metadata use
NicolasBeatum as the publisher. Both packages require a user-supplied ROM on
the microSD card.

This release also includes the lower-screen companion UI, widescreen renderer,
Spanish translation ROM validation, HDMA spotlight fixes, per-ROM saves,
persistent setup options and timestamped debug dumps described below.

### Gameplay correctness

- Fixed overlapping room-data copies by using `memmove` when extracting BTS
  and custom-background data from decompressed room blobs.
- Restored correct terrain collision and slope metadata in large rooms.
- Restored the Landing Site left blue door behavior: it blocks while closed,
  reacts to a beam shot, opens, and only then permits the room transition.
- Replaced undefined signed left shifts in fixed-point collision calculations
  with explicit signed multiplication.

### Rendering and performance

- Added an Old 3DS native frame scheduler that preserves visible scanline,
  HDMA, IRQ, VBlank and NMI events without stepping the unused CPU master
  clock two cycles at a time.
- Added a conservative PICA200 Mode 1 renderer for backgrounds, sprites,
  windows, scanline state and color math.
- Added a tile atlas and decoded-tile caches, color conversion caches, sprite
  line masks, and optimized CPU fallback paths.
- Added a Citro3D presenter for both LCDs while retaining a framebuffer
  fallback.
- Preserved exact rendering for unsupported or changing PPU state by falling
  back to the CPU renderer for that frame.

### Audio and platform integration

- Kept stereo output at 32 kHz and replaced floating-point sample stepping
  with 16.16 fixed-point arithmetic.
- Reduced duplicate DSP work and kept the SDL audio thread on the system core
  without lowering an already-granted CPU-time budget.
- Added New 3DS speedup enablement where available and safe Old 3DS CPU budget
  negotiation.
- Added an Azahar-only DSP marker fallback for development environments that
  do not provide `dspfirm.cdc`; real hardware behavior is unchanged.

### Lower screen

- Added a native 320x240 companion UI based on MetroidArch's color palette,
  pixel typography and three-tab layout.
- Added live energy/ammo status, touch ammo selection, explored-area map,
  equipment state, game time and an idle METROID screen.
- Ported MetroidArch's ROM-accurate pause-map decoder, including real tile
  graphics, palettes, flips, Map Station reveals and area tinting.
- Added ROOM/WORLD map switching, zoom controls, all-area world composition,
  area labels/connectors and long-press map markers.
- Ported the real ROM HUD ammo icons and the full-color Redux Samus suit asset,
  including Power/Varia/Gravity palette and pose selection.
- Added the vanilla item-percentage formula and functional status-bar, main-HUD
  and marker-clear setup controls.
- Reduced lower-texture rebuilds to 7.5 Hz and decoded map data per tile to
  preserve the native Old 3DS performance budget.
- Centered the WORLD view on its currently visible map content at every zoom
  level and restricted the lower status bar to the MAP tab.
- Tightened the BEAM equipment list so every entry clears its panel border.
- Added the GPU texture path for the lower screen and a direct framebuffer
  fallback.

### Diagnostics and packaging

- Added optional frame, PPU, PICA200, HDMA and audio profiling.
- Added optional door/BTS trace logging used to isolate the room-data bug.
- Added separate native and emulated-CPU diagnostic build paths.
- Added proper LTO propagation to both compilation and linking.

## Upstream history

This repository remains based on CharlesAverill/sm-3ds and snesrev/sm. The
dual-screen visual direction is based on Raekwon1603/RetroArch branch
`metroidarch-dual-screen`; see `docs/PORTING_NOTES.md` for detailed credits and
technical boundaries.
