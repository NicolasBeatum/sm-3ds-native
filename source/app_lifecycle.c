#include "app_lifecycle.h"

#include <3ds.h>
#include "SDL2/SDL.h"

enum { kHomeSuspended = 1, kSleeping = 2 };
static aptHookCookie g_hook;
static uint32_t g_device, g_suspend_count, g_restore_count;
static uint32_t g_sleep_count, g_wake_count, g_gap_count;
static unsigned g_background;
static u32 g_saved_cpu_limit;
static bool g_registered, g_resume_pending;
static AppLifecycleEvent g_events[32];
static unsigned g_event_read, g_event_count;
static uint32_t g_event_dropped, g_exit_count;

static void QueueEvent(const char *name, uint64_t now) {
  if (g_event_count == 32) {
    g_event_read = (g_event_read + 1) % 32;
    --g_event_count;
    ++g_event_dropped;
  }
  g_events[(g_event_read + g_event_count++) % 32] =
      (AppLifecycleEvent){ name, now, g_background };
}

bool AppLifecycle_ReadEvent(AppLifecycleEvent *event) {
  if (!g_event_count) return false;
  *event = g_events[g_event_read];
  g_event_read = (g_event_read + 1) % 32;
  --g_event_count;
  return true;
}

static void OnAptEvent(APT_HookType event, void *unused) {
  (void)unused;
  uint64_t now = osGetTime();
  const char *name;
  switch (event) {
  case APTHOOK_ONSUSPEND:
    name = "apt_suspend";
    g_background |= kHomeSuspended;
    ++g_suspend_count;
    if (g_device) SDL_PauseAudioDevice(g_device, 1);
    /* Release the foreground system-core budget while HOME runs. Do not
     * renegotiate it inside the sleep notification/acknowledgement path. */
    if (!g_saved_cpu_limit &&
        R_SUCCEEDED(APT_GetAppCpuTimeLimit(&g_saved_cpu_limit)) &&
        g_saved_cpu_limit > 30)
      APT_SetAppCpuTimeLimit(30);
    break;
  case APTHOOK_ONSLEEP:
    name = "apt_sleep";
    g_background |= kSleeping;
    ++g_sleep_count;
    if (g_device) SDL_PauseAudioDevice(g_device, 1);
    break;
  case APTHOOK_ONRESTORE:
    name = "apt_restore";
    g_background &= ~kHomeSuspended;
    ++g_restore_count;
    g_resume_pending = true;
    break;
  case APTHOOK_ONWAKEUP:
    name = "apt_wakeup";
    g_background &= ~kSleeping;
    ++g_wake_count;
    g_resume_pending = true;
    break;
  case APTHOOK_ONEXIT:
    name = "apt_exit_request";
    ++g_exit_count;
    break;
  default:
    return;
  }
  QueueEvent(name, now);
  /* RESTORE precedes DSP wakeup in libctru. Resume audio only after
   * aptMainLoop has returned to the game, with all transitions complete. */
}

void AppLifecycle_Init(void) {
  if (g_registered) return;
  g_device = g_suspend_count = g_restore_count = 0;
  g_sleep_count = g_wake_count = g_gap_count = 0;
  g_background = g_saved_cpu_limit = 0;
  g_resume_pending = false;
  g_event_read = g_event_count = 0;
  g_event_dropped = g_exit_count = 0;
  aptHook(&g_hook, OnAptEvent, NULL);
  g_registered = true;
}

void AppLifecycle_SetAudioDevice(uint32_t device) {
  g_device = device;
}

bool AppLifecycle_ConsumeResume(void) {
  if (!g_resume_pending || g_background) return false;
  g_resume_pending = false;
  if (g_saved_cpu_limit) {
    APT_SetAppCpuTimeLimit(g_saved_cpu_limit);
    g_saved_cpu_limit = 0;
  }
  return true;
}

void AppLifecycle_RecordExternalGap(void) {
  ++g_gap_count;
}

bool AppLifecycle_WriteDiagnostics(FILE *file) {
  return fprintf(file,
      "app_lifecycle=apt-audio-v1\napt_suspend_count=%lu\n"
      "apt_restore_count=%lu\napt_sleep_count=%lu\napt_wake_count=%lu\n"
      "external_pause_gap_count=%lu\napt_exit_request_count=%lu\n"
      "apt_log_events_dropped=%lu\n",
      (unsigned long)g_suspend_count, (unsigned long)g_restore_count,
      (unsigned long)g_sleep_count, (unsigned long)g_wake_count,
      (unsigned long)g_gap_count, (unsigned long)g_exit_count,
      (unsigned long)g_event_dropped) > 0;
}

void AppLifecycle_Fini(void) {
  if (!g_registered) return;
  aptUnhook(&g_hook);
  g_device = 0;
  g_registered = false;
}
