/* Exercise the real queue with separate publisher/mixer threads. */
#include <pthread.h>
#include <stdatomic.h>
#include <sched.h>
#include <assert.h>
#include "../sm/src/sm_rtl.c"

static pthread_mutex_t audio=PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t queue=PTHREAD_MUTEX_INITIALIZER;
static _Thread_local bool holds_audio,holds_queue;
static atomic_bool finished;
static SpcPlayer player;
SpcPlayer *g_spc_player=&player;
Snes *g_snes;
bool g_use_my_apu_code=true;

void RtlApuLock(void) {
  assert(!holds_queue && !holds_audio);
  pthread_mutex_lock(&audio);holds_audio=true;
}
void RtlApuUnlock(void) {
  assert(holds_audio && !holds_queue);
  holds_audio=false;pthread_mutex_unlock(&audio);
}
void RtlApuQueueLock(void) {
  assert(!holds_queue);pthread_mutex_lock(&queue);holds_queue=true;
}
void RtlApuQueueUnlock(void) {
  assert(holds_queue);holds_queue=false;pthread_mutex_unlock(&queue);
}
static void *Produce(void *unused) {
  (void)unused;
  for(unsigned i=0;i<100000;i++) {
    unsigned b=i%120;
    for(unsigned p=0;p<4;p++) g_apu_write.ports[p]=b+p;
    RtlPushApuState();
    if(i%16==0) sched_yield();
  }
  atomic_store(&finished,true);return NULL;
}
static void *Consume(void *unused) {
  (void)unused;
  for(;;) {
    RtlApuLock();RtlPopApuState_Locked();
    unsigned b=player.input_ports[0];
    if(player.input_ports[1]) for(unsigned p=0;p<4;p++) assert(player.input_ports[p]==b+p);
    RtlApuQueueLock();assert(g_apu_queue_size<=kApuMaxQueueSize);
    bool done=atomic_load(&finished) && g_apu_queue_size==0;RtlApuQueueUnlock();RtlApuUnlock();
    if(done) return NULL;
    sched_yield();
  }
}
int main(void) {
  pthread_t producer,consumer;
  /* Publication must finish even while synthesis owns the audio mutex. */
  RtlApuLock();assert(!pthread_create(&producer,NULL,Produce,NULL));
  assert(!pthread_join(producer,NULL));RtlResetApuQueue();RtlApuUnlock();
  atomic_store(&finished,false);
  assert(!pthread_create(&producer,NULL,Produce,NULL));
  assert(!pthread_create(&consumer,NULL,Consume,NULL));
  assert(!pthread_join(producer,NULL) && !pthread_join(consumer,NULL));
  assert(!g_apu_queue_size);
  puts("APU queue: publication while mixer locked, 200000 publications, overflow and untorn commands passed.");
}
