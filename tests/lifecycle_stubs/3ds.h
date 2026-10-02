#pragma once
#include <stdint.h>
typedef uint32_t u32;
typedef int Result;
#define R_SUCCEEDED(result) ((result) >= 0)
typedef enum { APTHOOK_ONSUSPEND, APTHOOK_ONRESTORE, APTHOOK_ONSLEEP,
               APTHOOK_ONWAKEUP, APTHOOK_ONEXIT } APT_HookType;
typedef void (*aptHookFn)(APT_HookType, void *);
typedef struct { int registered; } aptHookCookie;
void aptHook(aptHookCookie *, aptHookFn, void *);
void aptUnhook(aptHookCookie *);
Result APT_GetAppCpuTimeLimit(u32 *);
Result APT_SetAppCpuTimeLimit(u32);
