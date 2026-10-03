#include "session_log.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

static char g_path[320];
static bool g_enabled;
static uint64_t g_last_checkpoint;

static void WriteSingleLine(FILE *file, const char *text) {
  if (!text) return;
  for (; *text; ++text) {
    unsigned char c = (unsigned char)*text;
    fputc(c < 32 || c == 127 ? ' ' : c, file);
  }
}

bool SessionLog_Record(uint64_t now_ms, const char *event, const char *detail) {
  if (!g_enabled) return false;
  FILE *file = fopen(g_path, "a");
  if (!file) { g_enabled = false; return false; }
  fprintf(file, "time_ms=%" PRIu64 " event=", now_ms);
  WriteSingleLine(file, event);
  fputc(' ', file);
  WriteSingleLine(file, detail);
  fputc('\n', file);
  bool okay = !ferror(file);
  /* Closing each short batch pushes stdio data through FS before continuing.
   * There is no open buffered log to lose when the process is terminated. */
  if (fclose(file) != 0) okay = false;
  if (!okay) g_enabled = false; /* A failing SD must never terminate gameplay. */
  return okay;
}

bool SessionLog_Init(const char *directory, uint64_t now_ms,
                     const char *version, const char *build_flags) {
  g_enabled = false;
  g_path[0] = 0;
  g_last_checkpoint = now_ms;
  if (mkdir(directory, 0755) != 0 && errno != EEXIST) return false;
  char stamp[32], session[300], latest[320];
  time_t now = time(NULL);
  struct tm *local = localtime(&now);
  if (!local || !strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", local))
    snprintf(stamp, sizeof(stamp), "clock-%" PRIu64, now_ms);
  bool created = false;
  for (unsigned sequence = 0; sequence < 1000; ++sequence) {
    int length = snprintf(session, sizeof(session), "%s/%s-%03u",
                          directory, stamp, sequence);
    if (length < 0 || length >= (int)sizeof(session)) return false;
    if (mkdir(session, 0755) == 0) { created = true; break; }
    if (errno != EEXIST) return false;
  }
  if (!created) return false;
  snprintf(g_path, sizeof(g_path), "%s/session.log", session);
  g_enabled = true;
  char detail[256];
  snprintf(detail, sizeof(detail),
      "session_log=resume-v1 app_version=%s build_flags=%s",
      version, build_flags);
  if (!SessionLog_Record(now_ms, "session_start", detail)) return false;
  int length = snprintf(latest, sizeof(latest), "%s/last-session.txt", directory);
  if (length >= 0 && length < (int)sizeof(latest)) {
    FILE *file = fopen(latest, "w");
    if (file) { fprintf(file, "%s\n", g_path); fclose(file); }
  }
  return true;
}

bool SessionLog_CheckpointDue(uint64_t now_ms) {
  return g_enabled && (now_ms < g_last_checkpoint ||
                       now_ms - g_last_checkpoint >= 5000);
}

void SessionLog_MarkCheckpoint(uint64_t now_ms) {
  g_last_checkpoint = now_ms;
}

const char *SessionLog_Path(void) {
  return g_enabled ? g_path : NULL;
}
