#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

void AppLifecycle_Init(void);
void AppLifecycle_SetAudioDevice(uint32_t device);
bool AppLifecycle_ConsumeResume(void);
void AppLifecycle_RecordExternalGap(void);
bool AppLifecycle_WriteDiagnostics(FILE *file);
void AppLifecycle_Fini(void);

typedef struct {
  const char *name;
  uint64_t time_ms;
  uint32_t background;
} AppLifecycleEvent;
/* Hooks queue events in memory; the main thread drains them after SDL's pump.
 * No SD access or file logging is permitted inside an APT hook. */
bool AppLifecycle_ReadEvent(AppLifecycleEvent *event);
