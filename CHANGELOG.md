# Changelog

## 0.2.0 - 2026-09-26

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
