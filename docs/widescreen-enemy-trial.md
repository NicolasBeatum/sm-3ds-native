# Widescreen enemy behavior trial

Parent branch: `codex/hud-grapple-room-performance`.
Engine branch: `codex/widescreen-enemy-behavior`, based on `0d6d996`.
Local trial only; no release or merge is implied.

## Changes

- Restore native through-block movement for Brinstar pipe bugs, Norfair pipe
  bugs and yellow Brinstar wall/pipe bugs. Remove the added solid-block and
  physical-room-edge collision/reset paths. Keep the original directions,
  speeds, emergence, flight phases, timers and reset destinations.
- Retain the extended viewport when testing disappearance. Account for visible
  spritemap pieces beyond the collision radius, so a visible wing/leg is still
  processed and drawn at the edge. Native center/hitbox tests remain the fast
  path. The fallback reads current normal/extended spritemaps only when those
  tests reject an enemy and widescreen is enabled; it does not activate the
  entire room or permanently set off-screen processing flags.
- Yellow curved flight checks visibility before moving. Check after movement
  in widescreen too, so it resets on the crossing frame rather than getting
  culled before its next reset check. Retain the original invisible/respawn
  cycle rather than deleting the pipe's enemy slot.
- Reo (`Rio` in the engine) has a separate 160-pixel proximity trigger. Give a
  visible Reo whose center lies in an added side band the corresponding extra
  horizontal trigger distance. Preserve its original trigger in the center
  and when widescreen is disabled, as well as its native attack states and
  collision behavior. Natural waiting states are still allowed.
- Dessgeega uses the Sidehopper AI. Keep its native jump/movement/collision
  rules; the shared visibility fallback keeps it on the AI list while its
  sprite is partially visible at an extended edge. No special despawn rule
  is added to persistent room enemies.
- Leave the earlier Sbug room-boundary behavior intact.
- Diagnostic dumps identify `enemy_viewport=visible-spritemap-v1` and
  `pipe_movement=native-through-blocks`.

## Verification

`tests/widescreen_enemy_native_test.c` links the actual enemy functions against
synthetic RAM and spritemap/speed-table fixtures. It checks 600 horizontal
positions with widescreen off against the previous native selection, drawing
and disappearance predicates; both side bands and partially visible artwork;
empty/invalid and extended spritemaps; Reo activation and subsequent movement;
Dessgeega movement while selected at both edges; through-block movement in all
three pipe variants and their off-screen resets, including yellow curved
flight. Collision routines are stubbed to report walls in pipe tests and are
never called; movement tests for Reo/Dessgeega use open-space stubs. These tests
do not reproduce full room gameplay or prove hardware appearance.

```sh
for bank in a2 a3 b3; do
  cc -O2 -std=c11 -ffunction-sections -fdata-sections -iquote sm \
    -c sm/src/sm_${bank}.c -o /tmp/sm-${bank}-enemies.o
done
cc -O2 -std=c11 -ffunction-sections -fdata-sections -iquote sm \
  -DEnemy_MoveRight_IgnoreSlopes=UnusedNativeMoveRight \
  -DEnemy_MoveDown=UnusedNativeMoveDown \
  -c sm/src/sm_a0.c -o /tmp/sm-a0-enemies.o
cc -O2 -std=c11 -iquote sm -ffunction-sections -fdata-sections \
  tests/widescreen_enemy_native_test.c /tmp/sm-a0-enemies.o \
  /tmp/sm-a2-enemies.o /tmp/sm-a3-enemies.o /tmp/sm-b3-enemies.o \
  -Wl,--gc-sections -o /tmp/sm-enemy-test
/tmp/sm-enemy-test
```

## Packages

`output-enemy-wide-trial/SuperMetroid3DSPort.{3dsx,cia}` retain the X-Ray fix,
background/geometry optimizations, configurable HUD/video and suspend trial.
They use native, LTO, OLD3DS and PHASE_DIAG flags. No ROM is packaged.

Hardware validation is pending. Check pipe bugs crossing walls and the original
center boundaries, then leaving the extended display. Check Reo and Dessgeega
at both sides while the camera stays still, then compare widescreen off.
