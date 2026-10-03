# Silent return to HOME after sleep: diagnostic trial

Based on 0.1.5, branch `codex/resume-session-log`. This adds diagnostics;
it does not claim to fix the reported return to HOME after waking and playing.
The game engine, artwork, saves and audio backend remain those of 0.1.5.

## Files on the microSD

Each launch creates `3ds/sm3dsnative/logs/YYYYMMDD-HHMMSS-NNN/session.log`.
`logs/last-session.txt` points to the most recently opened session. Older
sessions are preserved, including a failed session if the game is reopened.
These small logs accumulate; copy/delete older session folders when no longer
needed. Logs contain the ROM filename and gameplay coordinates, not ROM bytes.

Records identify the app version, build flags, Old/New 3DS hardware family,
ROM loading, APT sleep/wakeup/suspend/restore/exit events, audio-reset stages,
SDL_QUIT, fatal errors handled by the port, and orderly shutdown stages.
Every five seconds of active gameplay a checkpoint records the frame, room,
Samus position, widescreen/render state, linear memory remaining and actual
CPU budget/query result. Manual dumps include the session-log path.

APT hooks enqueue at most 32 events in memory; no file I/O is performed in
APT or audio callbacks. Events are written after the SDL event pump returns
to the main game thread (or after returning from the ROM selector). Therefore
a failure inside the suspended event pump can leave those queued events
unwritten. Dump counters also identify dropped events if the queue overflowed.

Each record closes its file before continuing. SD errors disable logging
without terminating the game. Periodic log-writing time is excluded from the
next frame-interval sample and external-pause detection. This diagnostic build
does perform occasional synchronous SD writes and can cause a brief hitch on
a slow card; it does not write a record for every frame.

## Interpreting a failed session

- `sdl_quit_received` / `apt_exit_request` followed by `shutdown_complete` and
  `process_exit orderly_shutdown=1`: the application followed its close path.
  This is evidence of a close request, not proof of what issued that request.
- `fatal_error` or `process_exit orderly_shutdown=0`: a handled fatal error or
  exit without the complete orderly shutdown path.
- Missing `shutdown_complete` / `process_exit`: cleanup was not completed or
  its log could not be written. An external kill, exception, power-off, hang or
  SD failure can all produce this result; it is not a crash diagnosis by itself.

The last checkpoint is up to five seconds older than the failure. This log is
not a register dump or stack trace and cannot replace a Luma exception dump.

## Console trial

Install the diagnostic CIA, reproduce the same close-lid/wake/play sequence,
then copy `3ds/sm3dsnative/logs/` to the PC. Include the failed session even if
the game was subsequently reopened, and the last normal dump if available.
Report whether suspension used the lid, HOME, POWER or Rosalina, and the
console/Luma version. No need to generate a dump while the failure is happening.

## Host checks

```sh
cc -O2 -std=c11 -Wall -Wextra -Werror tests/session_log_test.c \
  source/session_log.c -o /tmp/session-log-test
/tmp/session-log-test
cc -O2 -std=c11 -Wall -Wextra -Werror -Itests/lifecycle_stubs \
  tests/app_lifecycle_test.c source/app_lifecycle.c -o /tmp/lifecycle-log-test
/tmp/lifecycle-log-test
```

Checks cover records readable before exit, previous-session retention,
latest-session pointer, checkpoint frequency, control-character escaping,
initial/runtime SD failure handling, deferred event timestamps/background
state, APT exit requests and bounded queue overflow, alongside the existing
audio pause/deferred-resume and CPU-budget lifecycle cases.
