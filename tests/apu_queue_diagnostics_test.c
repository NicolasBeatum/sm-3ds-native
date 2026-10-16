#include "src/sm_rtl.h"
#include "src/variables.h"
#include "src/funcs.h"
#include <assert.h>

static uint64_t ticks;
static unsigned locks,unlocks,audio_locks;
uint64_t SDL_GetPerformanceCounter(void) { return ++ticks; }
void RtlApuLock(void) { audio_locks++; }
void RtlApuUnlock(void) { assert(false); }
void RtlApuQueueLock(void) { locks++;ticks+=500; }
void RtlApuQueueUnlock(void) { unlocks++; }

int main(void) {
  /* Exercise normal queue growth, overflow, empty-frame early returns and
   * music upload state. Timing must never skip an unlock or alter RAM. */
  for(unsigned i=0;i<150;i++) {
    is_uploading_apu=i%29==0;
    RtlPushApuState();
    assert(g_diag_apu_wait_ticks==501 && locks==unlocks);
    assert(is_uploading_apu==(i%29==0));
  }
  assert(locks==150 && audio_locks==0);
}
