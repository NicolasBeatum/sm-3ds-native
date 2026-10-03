#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Main-thread SD logging only. APT/audio callbacks must not call this API. */
bool SessionLog_Init(const char *directory, uint64_t now_ms,
                     const char *version, const char *build_flags);
bool SessionLog_Record(uint64_t now_ms, const char *event, const char *detail);
bool SessionLog_CheckpointDue(uint64_t now_ms);
void SessionLog_MarkCheckpoint(uint64_t now_ms);
const char *SessionLog_Path(void);
