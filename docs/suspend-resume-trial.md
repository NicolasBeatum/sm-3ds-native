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

## Old 3DS CIA: NWM ErrDisp isolation trial

Hardware report: only this port, installed CIA, Old 3DS, after leaving the
console suspended. ErrDisp identifies process 25, reported name `nwn`, title
`0004013000002d02`, address `00109466`, result `f9606c02`. That title belongs
to the NWM wireless system module. The user suspects a reconnect after
sleep. The report does not yet establish whether wireless was enabled,
whether the error happens during sleep or on wake, or the underlying cause.
Do not treat it as proof of either faulty wireless hardware or a port bug.

The native port and its 3DS SDL startup/video/audio/thread paths do not
initialize SOC, AC, NWM or HTTP services or open a network socket. A system
wireless reconnect can nevertheless occur while the title is running.

The diagnostic package in `output-lighting-sleep-trial/` uses
`-DSM3DS_SYSTEM_CORE_PERCENT=30`, instead of the previous initial request
of 80 percent. This leaves more system-core time available to system services;
SDL audio runs on that core, while native gameplay and PICA preparation run
on the application core. It is an isolation experiment, not a confirmed NWM
crash fix. It might affect audio stability or latency; real hardware testing
is still needed. Existing suspend/DSP handling is retained and no additional
CPU-limit IPC is introduced in the sleep acknowledgement callback.

The compile-time setting accepts 10..80 percent; rejected requests fall back
only to lower configured budgets. The default remains 80 for builds that do
not explicitly select this trial. Session logs identify the flag and record
the actual granted CPU limit at startup/resume and five-second checkpoints.

Try the conservative CIA with the same room, settings and close-lid duration
as the failing run, first with wireless off and then on. If it fails again,
retain the exact ErrDisp details and `3ds/sm3dsnative/logs/`, including the
failed session. Events queued during sleep may not reach the SD if the system
fails before the event pump returns; absence of a logged `apt_sleep` therefore
does not prove that the hook was never called. Check gameplay/audio after
several wake cycles. This CIA also contains the playfield-boundary correction.

References:
- [NWM title identification](https://yls8.mtheall.com/ninupdates/titlelist.php?date=07-24-12_08-00-51&soap=1&sys=ctr)
- [NWM services](https://3dbrew.org/wiki/NWM_Services)
- [libctru APT sleep path](https://github.com/devkitPro/libctru/blob/master/libctru/source/services/apt.c)
- [Prior report of the same result with wireless enabled](https://github.com/LumaTeam/Luma3DS/issues/766)
