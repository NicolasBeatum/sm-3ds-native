# HOME / sleep / Rosalina hardware trial

Based on 0.1.3, on branches `codex/suspend-resume` and SDL
`codex/ndsp-suspend-resume`. No game rendering, palette or gameplay changes.

## Changes

- Pause SDL audio before HOME/sleep transitions. RESTORE only marks pending
  work: audio is resumed by the game loop after libctru's DSP wakeup completes.
- Temporarily return the system-core budget to 30% while HOME runs and restore
  the original foreground budget on return. Sleep callbacks do not negotiate
  CPU limits. New 3DS speedup is reapplied on resume.
- Reset the frame deadline and diagnostic interval after APT resume or a long
  gap between presentation and the next event pump. This also handles returning
  from Rosalina, which freezes application threads without APT hooks. Slow
  rendering inside a frame is not classified as an external pause.
- Discard the partial audio block and stale input after APT resume.
- Teach SDL's NDSP backend to pause its mixer during DSP sleep, restore the
  previous paused state on wakeup, avoid submitting buffers while suspended,
  and wake its waiters on DSP transitions. Poll shutdown every 20 ms while
  waiting for a buffer, and accept a completed buffer even if its completion
  callback was interrupted. Suspended audio sleeps rather than spinning.
- Add APT sleep/restore and external-gap counts to dumps. No extra SD logging
  is performed during gameplay or from the transition callbacks.

## Verification

`tests/app_lifecycle_test.c` covers startup sleep without audio, nested
HOME/sleep events, duplicate suspension, deferred audio resume, preservation
of CPU limits, rejected CPU queries and unregistering before audio teardown.

```sh
cc -O2 -std=c11 -Wall -Wextra -Itests/lifecycle_stubs \
  tests/app_lifecycle_test.c source/app_lifecycle.c -o /tmp/lifecycle-test
/tmp/lifecycle-test
```

Both native/LTO package formats compile with the installed devkitARM toolchain.
Real hardware validation remains necessary: opening Rosalina, returning from
HOME several times, closing/opening the lid, and closing the suspended title
from HOME. APT does not expose Rosalina's freeze operation, so correcting resume
timing alone cannot prove that an entry freeze in Luma3DS is fixed.

After resuming, SAVE DUMP records `app_lifecycle=apt-audio-v1` and the event
counters, alongside the actual CPU budget and normal timing metrics. The public
0.1.3 release is unchanged until hardware testing confirms the correction.

References: libctru `services/apt.c` and `services/dsp.c`, Citro3D's APT hook,
and Luma3DS `k11_extension/source/synchronization.c` and Rosalina `menu.c`.
