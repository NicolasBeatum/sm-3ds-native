# Changelog

## Unreleased

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
- Use the approved pixel-art 3DS and Samus ship artwork for the large CIA
  HOME-menu banner. The small HOME icon is unchanged.
- Resolve repeated background tile/palette descriptors once per GPU frame,
  sharing the result between main and sub layers. Layers with changing
  graphics banks retain the original lookup path. VRAM and palette changes
  are revalidated on the next frame; geometry, HDMA and gameplay are unchanged.
- Identify this renderer revision and LTO setting in diagnostic dumps for
  hardware comparison. Old/New 3DS model detection and automatic New 3DS
  CPU/L2 acceleration remain enabled in both package formats.

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
