#include "audio_diagnostics.h"
#include "SDL2/SDL.h"
#include <stdlib.h>
#include <string.h>

static uint32_t g_device;
static int g_frequency, g_volume;
static unsigned g_channels, g_samples, g_format;
static char g_error[192];
static const char *g_capture_status="not_saved";
static unsigned g_capture_peak,g_capture_clipped;
#ifdef SM3DS_PHASE_DIAG
enum { kPcmSamples=32768 }; /* 0.512 s of original stereo PCM at 32 kHz. */
static int16_t g_pcm[kPcmSamples];
static size_t g_pcm_write,g_pcm_count;
static SDL_atomic_t g_generated, g_generated_nonzero, g_callbacks, g_output_nonzero;
static SDL_atomic_t g_last_callback;

static bool HasSound(const int16_t *pcm, size_t samples) {
  for (size_t i=0;i<samples;i++) if(pcm[i]) return true;
  return false;
}
#endif

void AudioDiagnostics_Init(uint32_t device, int frequency, unsigned channels,
                           unsigned samples, unsigned format, int mixer_volume,
                           const char *error) {
  g_device=device;g_frequency=frequency;g_channels=channels;
  g_samples=samples;g_format=format;g_volume=mixer_volume;
  g_capture_status="not_saved";g_capture_peak=g_capture_clipped=0;
  snprintf(g_error,sizeof(g_error),"%s",error?error:"");
  for (char *p=g_error;*p;p++) if(*p=='\n'||*p=='\r') *p=' ';
#ifdef SM3DS_PHASE_DIAG
  SDL_AtomicSet(&g_generated,0);SDL_AtomicSet(&g_generated_nonzero,0);
  SDL_AtomicSet(&g_callbacks,0);SDL_AtomicSet(&g_output_nonzero,0);
  SDL_AtomicSet(&g_last_callback,0);
  g_pcm_write=g_pcm_count=0;
#endif
}

void AudioDiagnostics_Generated(const int16_t *pcm,size_t samples) {
#ifdef SM3DS_PHASE_DIAG
  SDL_AtomicAdd(&g_generated,1);
  if(HasSound(pcm,samples)) SDL_AtomicAdd(&g_generated_nonzero,1);
#else
  (void)pcm;(void)samples;
#endif
}

void AudioDiagnostics_Output(const int16_t *pcm,size_t samples) {
#ifdef SM3DS_PHASE_DIAG
  SDL_AtomicAdd(&g_callbacks,1);
  if(HasSound(pcm,samples)) SDL_AtomicAdd(&g_output_nonzero,1);
  SDL_AtomicSet(&g_last_callback,(int)SDL_GetTicks());
  /* SDL holds the callback lock here. Only the dump-time snapshot reads this
   * ring, under the same lock. No allocation or SD write on the audio thread. */
  if(samples>kPcmSamples) {pcm+=samples-kPcmSamples;samples=kPcmSamples;}
  size_t first=samples<kPcmSamples-g_pcm_write?samples:kPcmSamples-g_pcm_write;
  memcpy(g_pcm+g_pcm_write,pcm,first*sizeof(*pcm));
  memcpy(g_pcm,pcm+first,(samples-first)*sizeof(*pcm));
  g_pcm_write=(g_pcm_write+samples)%kPcmSamples;
  g_pcm_count=g_pcm_count+samples<kPcmSamples?g_pcm_count+samples:kPcmSamples;
#else
  (void)pcm;(void)samples;
#endif
}

bool AudioDiagnostics_Save(const char *directory) {
#ifdef SM3DS_PHASE_DIAG
  if(!g_device) {g_capture_status="not_opened";return true;}
  int16_t *copy=malloc(sizeof(g_pcm));
  if(!copy) {g_capture_status="no_memory";return false;}
  SDL_LockAudioDevice(g_device);
  size_t count=g_pcm_count,first=(g_pcm_write+kPcmSamples-count)%kPcmSamples;
  size_t chunk=count<kPcmSamples-first?count:kPcmSamples-first;
  memcpy(copy,g_pcm+first,chunk*sizeof(*copy));
  memcpy(copy+chunk,g_pcm,(count-chunk)*sizeof(*copy));
  SDL_UnlockAudioDevice(g_device);
  if(!count) {free(copy);g_capture_status="no_samples";return true;}
  g_capture_peak=g_capture_clipped=0;
  for(size_t i=0;i<count;i++) {
    unsigned value=copy[i]<0?-(int)copy[i]:copy[i];
    if(value>g_capture_peak)g_capture_peak=value;
    g_capture_clipped+=value>=32760;
  }
  char path[1024];
  if(snprintf(path,sizeof(path),"%s/audio-output.wav",directory)>=(int)sizeof(path)) {
    free(copy);g_capture_status="path_too_long";return false;
  }
  FILE *file=fopen(path,"wb");
  if(!file) {free(copy);g_capture_status="open_failed";return false;}
  unsigned bytes=(unsigned)count*2;
  const unsigned fields[]={bytes+36,16,1,g_channels,(unsigned)g_frequency,
                           (unsigned)g_frequency*g_channels*2,g_channels*2,16,bytes};
  const unsigned sizes[]={4,4,2,2,4,4,2,2,4};
  bool okay=fwrite("RIFF",1,4,file)==4;
  for(unsigned f=0;f<9;f++) {
    if(f==1)okay=(fwrite("WAVEfmt ",1,8,file)==8)&&okay;
    if(f==8)okay=(fwrite("data",1,4,file)==4)&&okay;
    for(unsigned i=0;i<sizes[f];i++)
      okay=(fputc((fields[f]>>(8*i))&255,file)!=EOF)&&okay;
  }
  /* Both supported targets (3DS and the host probe) are little-endian. */
  okay=(fwrite(copy,1,bytes,file)==bytes)&&okay;
  free(copy);if(fclose(file))okay=false;
  if(!okay)remove(path);
  g_capture_status=okay?"saved":"write_failed";
  return okay;
#else
  (void)directory;g_capture_status="unavailable";return true;
#endif
}

bool AudioDiagnostics_Write(FILE *file) {
  const char *driver=SDL_GetCurrentAudioDriver();
  if(fprintf(file,"audio_opened=%u\naudio_driver=%s\naudio_frequency=%d\n"
      "audio_channels=%u\naudio_samples=%u\naudio_format=%04x\n"
      "audio_mixer_volume=%d\naudio_device_status=%u\naudio_init_error=%s\n"
      "audio_capture_status=%s\naudio_capture_peak_abs=%u\n"
      "audio_capture_clipped_samples=%u\naudio_pcm_file=%s\n",
      g_device!=0,driver?driver:"unavailable",g_frequency,g_channels,g_samples,
      g_format,g_volume,g_device?(unsigned)SDL_GetAudioDeviceStatus(g_device):0,
      g_error,g_capture_status,g_capture_peak,g_capture_clipped,
      !strcmp(g_capture_status,"saved")?"audio-output.wav":"unavailable")<0) return false;
#ifdef SM3DS_PHASE_DIAG
  unsigned callbacks=(unsigned)SDL_AtomicGet(&g_callbacks);
  return fprintf(file,"audio_diagnostics=pcm-output-v1\naudio_generated_blocks=%u\n"
      "audio_generated_nonzero_blocks=%u\naudio_callbacks=%u\n"
      "audio_output_nonzero_callbacks=%u\naudio_callback_age_ms=%u\n"
      "audio_callback_age_valid=%u\n"
      "audio_timing_note=nonzero PCM proves samples exist, not audible speaker/DSP output; counters are independent atomic snapshots\n",
      (unsigned)SDL_AtomicGet(&g_generated),(unsigned)SDL_AtomicGet(&g_generated_nonzero),
      callbacks,(unsigned)SDL_AtomicGet(&g_output_nonzero),callbacks?
      (unsigned)(SDL_GetTicks()-(uint32_t)SDL_AtomicGet(&g_last_callback)):0,
      callbacks!=0)>0 && !ferror(file);
#else
  return fputs("audio_diagnostics=unavailable\n",file)>=0 && !ferror(file);
#endif
}
