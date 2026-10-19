#define _POSIX_C_SOURCE 200809L
#include "../source/session_log.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(void) {
  char directory[] = "/tmp/sm-session-log-test-XXXXXX";
  assert(mkdtemp(directory));
  assert(SessionLog_Init(directory, 1000, "trial", "native"));
  char first[320];
  snprintf(first, sizeof(first), "%s", SessionLog_Path());
  assert(!SessionLog_CheckpointDue(5999));
  assert(SessionLog_CheckpointDue(6000));
  SessionLog_MarkCheckpoint(6000);
  assert(!SessionLog_CheckpointDue(6001));
  assert(SessionLog_Record(6001, "resume_begin", "room=abcd\nno_injected_event"));
  /* The file is already readable/closed before any normal exit, as it must
   * be when an external termination prevents cleanup from running. */
  FILE *file = fopen(first, "r");
  assert(file);
  char text[1024] = {0};
  assert(fread(text, 1, sizeof(text) - 1, file));
  fclose(file);
  assert(strstr(text, "event=session_start"));
  assert(strstr(text, "event=resume_begin room=abcd no_injected_event\n"));
  assert(!strstr(text, "shutdown_complete"));

  assert(SessionLog_Init(directory, 7000, "trial", "native"));
  const char *second = SessionLog_Path();
  assert(second && strcmp(first, second));
  char pointer[320];
  snprintf(pointer, sizeof(pointer), "%s/last-session.txt", directory);
  file = fopen(pointer, "r");
  assert(file && fgets(text, sizeof(text), file));
  fclose(file);
  text[strcspn(text, "\n")] = 0;
  assert(!strcmp(text, second));
  assert(access(first, F_OK) == 0); /* Reopening preserves the failed session. */
  assert(SessionLog_Record(7100, "shutdown_complete", "game"));

  char folder[320];
  snprintf(folder, sizeof(folder), "%s", second);
  assert(unlink(second) == 0);
  *strrchr(folder, '/') = 0;
  assert(rmdir(folder) == 0);
  assert(!SessionLog_Record(7200, "checkpoint", "directory_removed"));
  assert(!SessionLog_Path());
  assert(!SessionLog_CheckpointDue(12000));
  snprintf(folder, sizeof(folder), "%s", first);
  assert(unlink(first) == 0);
  *strrchr(folder, '/') = 0;
  assert(rmdir(folder) == 0);
  assert(unlink(pointer) == 0);
  assert(rmdir(directory) == 0);

  /* Logging failure is optional and must not stop or retry every frame. */
  assert(!SessionLog_Init("/nonexistent/sm-session-log", 0, "trial", "native"));
  assert(!SessionLog_Path());
  assert(!SessionLog_CheckpointDue(5000));
  assert(!SessionLog_Record(5000, "checkpoint", ""));
  return 0;
}
