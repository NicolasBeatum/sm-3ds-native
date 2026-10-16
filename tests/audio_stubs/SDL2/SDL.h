#pragma once
#include <stdint.h>
typedef struct { int value; } SDL_atomic_t;
static inline int SDL_AtomicSet(SDL_atomic_t *a,int value) { int old=a->value;a->value=value;return old; }
static inline int SDL_AtomicGet(SDL_atomic_t *a) { return a->value; }
static inline int SDL_AtomicAdd(SDL_atomic_t *a,int n) { int old=a->value;a->value+=n;return old; }
uint32_t SDL_GetTicks(void);
const char *SDL_GetCurrentAudioDriver(void);
unsigned SDL_GetAudioDeviceStatus(uint32_t device);

void SDL_LockAudioDevice(uint32_t device);
void SDL_UnlockAudioDevice(uint32_t device);
