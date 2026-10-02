# X-Ray widescreen trial

Branch: `codex/hud-grapple-room-performance`. Local trial, not a release.

## Evidence

The supplied `20261001-191957-004/top.bmp` shows X-Ray active in room `a2ce`
with a uniform gray band outside the room on the left. Its WRAM identifies
X-Ray HDMA pre-instruction `88:86ef`, window 2 color math, angle 167, width 10
and frozen gameplay. The other four supplied October dumps show background
performance scenes, not active X-Ray.

The original cone uses an unsigned 0–255 window. During scanning, BG2 also
changes from a room background to a scanned image of BG1. The side renderer
was still using the normal BG2 reconstruction and the original window limits.

## Changes

- Generate translated side windows with the game's existing integer HDMA
  cone routines. Retain the captured center interval on every scanline.
  Extend only clamped endpoints; do not bridge gaps across unscanned center
  pixels during widening or at extreme angles.
- Reconstruct side-only scanned tilemaps using the native block/BTS, item
  and room special-casing reveal rules. Snapshot once per frozen camera/room,
  rather than rebuilding tiles as Samus aims. Save and restore the full 4 KiB
  native scratch region around reveal calls. Do not upload these maps into
  native VRAM or change gameplay state.
- Use BG1 room coordinates for the temporary scanned BG2 sides. Leave the
  native center tilemap and scroll unchanged. Respect the game's rooms where
  X-Ray cannot reveal blocks.
- Group render bands using the extended second-window limits, including
  color composition. Mask empty space beyond physical room boundaries after
  color math, below the HUD.
- Activate this path only for an active X-Ray HDMA object and widescreen.
  Do not reinterpret unrelated freeze/window effects. Clear cached side
  pointers on stage transitions; deactivation uses the captured native window.
- Dumps include `xray_render=translated-native-window-v1`.

## Verification

- Replay the supplied WRAM with its user-provided Spanish ROM: 121 scanlines
  extend into the side bands, center window membership is unchanged, and all
  128 KiB of WRAM match byte for byte after preparation.
- 132 combinations of position, angle and width in both facing directions
  preserve native center membership and WRAM. Deactivation and unrelated HDMA
  cases exclude stale cones and tilemaps.
- Renderer tests cover native center tiles, reconstructed scanned side tiles,
  left/right windows changing per row and black room margins below the HUD.
- Main/sub color and alpha rasters match the earlier model across 216 animated
  synthetic non-X-Ray frames. Previous background/geometry optimizations remain.
- CIA and 3DSX compile. The new appearance and transitions still need testing
  on the console; these host checks do not establish hardware FPS or prove
  every special room renders correctly.

```sh
cc -O2 -std=c11 -ffunction-sections -fdata-sections -iquote sm -Isource \
  -c sm/src/sm_91.c -o /tmp/sm91-xray.o
cc -O2 -std=c11 -ffunction-sections -fdata-sections -iquote sm -Isource \
  tests/wide_xray_native_test.c source/wide_xray.c /tmp/sm91-xray.o \
  -Wl,--gc-sections -o /tmp/sm-xray-test
/tmp/sm-xray-test /path/to/user-rom.sfc /path/to/xray-dump/memory.wram
cc -O2 -std=c11 -iquote sm -Isource tests/ppu_gpu_model_wide_test.c \
  source/ppu_gpu_model.c -o /tmp/sm-wide-test
/tmp/sm-wide-test
```

No ROM, IPS or dump assets are included in the repository or packages.

## Packages and console check

`output-xray-wide-trial/SuperMetroid3DSPort.{3dsx,cia}` use native, LTO,
OLD3DS and PHASE_DIAG flags, retaining the HUD/video/suspend trial features.

In the dump's room, check X-Ray near both walls with widescreen on, aim up
and down through both side bands, then release and reactivate it. Check
hidden blocks/items and that empty room margins stay black. Compare the same
scene with widescreen off to confirm the native center appearance.
