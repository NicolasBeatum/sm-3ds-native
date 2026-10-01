#include "../source/app_lifecycle.h"
#include <3ds.h>
#include <assert.h>
#include <string.h>

static aptHookFn callback;
static void *callback_param;
static u32 cpu_limit = 80;
static int audio_paused, pause_calls, cpu_writes;
static bool fail_get_cpu;

void aptHook(aptHookCookie *cookie, aptHookFn hook, void *param) {
  assert(!callback);
  callback = hook;
  callback_param = param;
  cookie->registered = 1;
}
void aptUnhook(aptHookCookie *cookie) {
  assert(cookie->registered);
  cookie->registered = 0;
  callback = NULL;
}
Result APT_GetAppCpuTimeLimit(u32 *limit) {
  if (fail_get_cpu) return -1;
  *limit = cpu_limit;
  return 0;
}
Result APT_SetAppCpuTimeLimit(u32 limit) {
  assert(limit > 0 && limit <= 80);
  cpu_limit = limit;
  ++cpu_writes;
  return 0;
}
void SDL_PauseAudioDevice(uint32_t device, int paused) {
  assert(device == 7);
  assert(paused == 1); /* Hooks must never unpause before DSP restoration. */
  audio_paused = paused;
  ++pause_calls;
}
static void Event(APT_HookType event) {
  assert(callback);
  callback(event, callback_param);
}

int main(void) {
  AppLifecycle_Init();
  AppLifecycle_Init(); /* No duplicate registration. */
  assert(!AppLifecycle_ConsumeResume());
  /* Startup selector can sleep before an audio device exists. */
  Event(APTHOOK_ONSLEEP);
  Event(APTHOOK_ONWAKEUP);
  assert(AppLifecycle_ConsumeResume());
  assert(!pause_calls && !cpu_writes);
  AppLifecycle_SetAudioDevice(7);

  Event(APTHOOK_ONSUSPEND);
  assert(audio_paused && cpu_limit == 30);
  assert(!AppLifecycle_ConsumeResume());
  Event(APTHOOK_ONSUSPEND); /* Preserve the original 80% across duplicates. */
  Event(APTHOOK_ONSLEEP);
  Event(APTHOOK_ONWAKEUP);
  assert(!AppLifecycle_ConsumeResume()); /* Still inside HOME. */
  Event(APTHOOK_ONRESTORE);
  assert(audio_paused && cpu_limit == 30);
  assert(AppLifecycle_ConsumeResume());
  assert(cpu_limit == 80 && audio_paused);
  assert(!AppLifecycle_ConsumeResume());

  int before = cpu_writes;
  Event(APTHOOK_ONSLEEP);
  Event(APTHOOK_ONWAKEUP);
  assert(AppLifecycle_ConsumeResume());
  assert(cpu_writes == before); /* Sleep must not negotiate CPU budget. */

  fail_get_cpu = true;
  Event(APTHOOK_ONSUSPEND);
  Event(APTHOOK_ONRESTORE);
  assert(AppLifecycle_ConsumeResume());
  assert(cpu_writes == before && cpu_limit == 80);
  fail_get_cpu = false;

  cpu_limit = 20; /* Preserve a lower budget already granted by the system. */
  Event(APTHOOK_ONSUSPEND);
  assert(cpu_limit == 20);
  Event(APTHOOK_ONRESTORE);
  assert(AppLifecycle_ConsumeResume());
  assert(cpu_limit == 20);

  AppLifecycle_RecordExternalGap();
  FILE *f = tmpfile();
  assert(f && AppLifecycle_WriteDiagnostics(f));
  rewind(f);
  char text[512] = {0};
  assert(fread(text, 1, sizeof(text) - 1, f));
  assert(strstr(text, "apt_suspend_count=4\n"));
  assert(strstr(text, "apt_sleep_count=3\n"));
  assert(strstr(text, "external_pause_gap_count=1\n"));
  fclose(f);
  AppLifecycle_Fini();
  AppLifecycle_Fini();
  assert(!callback); /* No callbacks may touch a closed audio device. */
  return 0;
}
