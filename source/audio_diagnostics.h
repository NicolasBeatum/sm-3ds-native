#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stddef.h>

/* Observe the original PCM and SDL output; never alter or restart playback. */
void AudioDiagnostics_Init(uint32_t device, int frequency, unsigned channels,
                           unsigned samples, unsigned format, int mixer_volume,
                           const char *error);
void AudioDiagnostics_Generated(const int16_t *pcm, size_t samples);
void AudioDiagnostics_Output(const int16_t *pcm, size_t samples);
/* Copy the recent output under SDL's callback lock, then write after unlock. */
bool AudioDiagnostics_Save(const char *directory);
bool AudioDiagnostics_Write(FILE *file);
