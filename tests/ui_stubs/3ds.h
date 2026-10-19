#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef uint32_t u32;
typedef int Result;
typedef void *Thread;
typedef int LightLock;
typedef int LightEvent;
#define R_SUCCEEDED(r) ((r)>=0)
enum {GFX_TOP,GFX_BOTTOM,GFX_LEFT,RESET_ONESHOT};
uint64_t osGetTime(void);
Result APT_CheckNew3DS(bool *value);
void *linearMemAlign(size_t size,size_t alignment);
void linearFree(void *p);
uint8_t *gfxGetFramebuffer(int,int,void *,void *);
void LightLock_Init(LightLock *);
void LightLock_Lock(LightLock *);
void LightLock_Unlock(LightLock *);
void LightEvent_Init(LightEvent *,int);
void LightEvent_Wait(LightEvent *);
void LightEvent_Signal(LightEvent *);
Thread threadCreate(void (*)(void *),void *,size_t,int,int,bool);
void threadJoin(Thread,uint64_t);
void threadFree(Thread);
