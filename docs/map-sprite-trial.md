# Map and sprite performance trial

## Hardware samples available before this trial

Both dumps are from Old 3DS, room `92fd`, with widescreen enabled and almost
the same player position. They describe the ending 120 frames, not the whole
route, so they cannot establish a session-wide improvement or regression.

| Dump | Build | Recent FPS | Mean game work | Mean upper-screen work |
| --- | --- | ---: | ---: | ---: |
| `20260930-181904-000` | 0.1.1 | 50.20 | 12.685 ms | 5.314 ms |
| `20260930-182732-000` | Descriptor-cache trial | 48.62 | 10.992 ms | 7.513 ms |

The trial reduced the measured game work in that sample, while upper-screen
work increased. New schema v4 dumps retain whole-session aggregates and
separate standard/widescreen gameplay totals alongside the recent sample.
They also identify frames where the map sector or camera block changes.

## Changes in this build

- Decode map tile indices and tinted colors once, and use packed pixel writes.
- Check visible bottom-screen state at the existing polling interval; only
  rebuild the texture when that state changes or touch input marks it dirty.
- Save configuration snapshots from a worker, with pending writes drained
  before shutdown. Normal gameplay never requests configuration saves when
  crossing a map sector.
- Build ordered sprite membership by row to avoid scanning every sprite on
  every scanline. Scanline OBJ-size changes retain the original selection loop.
- Apply per-line background scroll deltas to side-band room sampling and room
  bounds, including camera shakes.
- Include the approved space banner in the CIA. Both packages remain ROM-free.

## Verification

Host differential checks matched emitted quads, atlas pixels and tile counters
across 120 frames at 256/400 pixels, including sprite size changes, OAM flips,
palette/VRAM animation and scanline scroll/window changes. A lower-screen
comparison matched room/world map pixels byte for byte at multiple zoom levels,
and checked idle redraw suppression, health updates and final queued settings.
The checked-in regressions cover tile-cache invalidation, widescreen scroll
offsets and session statistics surviving the recent-ring rollover.

Host model timings are not console FPS measurements. Gains, touch response,
settings persistence and side-band shakes still need Azahar/hardware checks.
