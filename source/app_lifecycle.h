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
