#define _POSIX_C_SOURCE 200809L
#include "../source/audio_diagnostics.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

static uint32_t ticks;
static unsigned status=1;
static bool audio_locked;
uint32_t SDL_GetTicks(void) { return ticks; }
const char *SDL_GetCurrentAudioDriver(void) { return "n3ds-test"; }
unsigned SDL_GetAudioDeviceStatus(uint32_t device) { assert(device==9);return status; }

void SDL_LockAudioDevice(uint32_t device) {assert(device==9 && !audio_locked);audio_locked=true;}
void SDL_UnlockAudioDevice(uint32_t device) {assert(device==9 && audio_locked);audio_locked=false;}
static void Read(char *text,size_t size) {
  FILE *file=tmpfile();assert(file && AudioDiagnostics_Write(file));
  rewind(file);size_t count=fread(text,1,size-1,file);text[count]=0;fclose(file);
}

int main(void) {
  char text[3000];int16_t pcm[1024]={0};
  char directory[]="/tmp/sm3ds-audio-XXXXXX",path[256];
  assert(mkdtemp(directory));snprintf(path,sizeof(path),"%s/audio-output.wav",directory);
  AudioDiagnostics_Init(9,32000,2,2048,0x8010,128,"");
  Read(text,sizeof(text));assert(strstr(text,"audio_callback_age_valid=0\n"));
  AudioDiagnostics_Generated(pcm,1024);ticks=0xfffffff0;
  AudioDiagnostics_Output(pcm,1024);
  pcm[1023]=-1; /* One nonzero sample at the very end must be detected. */
  AudioDiagnostics_Generated(pcm,1024);AudioDiagnostics_Output(pcm,1024);
  assert(pcm[1023]==-1);ticks=0x10;Read(text,sizeof(text));
  assert(strstr(text,"audio_generated_blocks=2\n"));
  assert(strstr(text,"audio_generated_nonzero_blocks=1\n"));
  assert(strstr(text,"audio_callbacks=2\n"));
  assert(strstr(text,"audio_output_nonzero_callbacks=1\n"));
  assert(strstr(text,"audio_callback_age_ms=32\n"));
  assert(AudioDiagnostics_Save(directory));assert(!audio_locked);
  FILE *wave=fopen(path,"rb");assert(wave);
  unsigned char header[44];assert(fread(header,1,44,wave)==44);
  assert(!memcmp(header,"RIFF",4) && !memcmp(header+8,"WAVEfmt ",8));
  int16_t saved[2048];assert(fread(saved,sizeof(*saved),2048,wave)==2048);
  assert(saved[1023]==0 && saved[2047]==-1);fclose(wave);remove(path);
  Read(text,sizeof(text));assert(strstr(text,"audio_capture_peak_abs=1\n"));
  assert(strstr(text,"audio_capture_status=saved\n"));
  status=2;Read(text,sizeof(text));assert(strstr(text,"audio_device_status=2\n"));
  AudioDiagnostics_Init(9,32000,2,2048,0x8010,128,"");
  for(unsigned block=0;block<40;block++) {
    for(unsigned i=0;i<1024;i++)pcm[i]=block+10;
    if(block==39)pcm[1023]=-32768;
    AudioDiagnostics_Output(pcm,1024);
  }
  assert(AudioDiagnostics_Save(directory));
  wave=fopen(path,"rb");assert(wave);
  assert(fseek(wave,44,SEEK_SET)==0);
  int16_t first,last;
  assert(fread(&first,sizeof(first),1,wave)==1 && first==18);
  assert(fseek(wave,44+32767*2,SEEK_SET)==0);
  assert(fread(&last,sizeof(last),1,wave)==1 && last==-32768 && fgetc(wave)==EOF);
  fclose(wave);remove(path);
  Read(text,sizeof(text));assert(strstr(text,"audio_capture_peak_abs=32768\n"));
  assert(strstr(text,"audio_capture_clipped_samples=1\n"));
  assert(!AudioDiagnostics_Save("/nonexistent-sm3ds-directory"));
  Read(text,sizeof(text));assert(strstr(text,"audio_capture_status=open_failed\n"));
  AudioDiagnostics_Init(0,0,0,0,0,128,"DSP init failed\nmissing firmware");
  Read(text,sizeof(text));assert(strstr(text,"audio_opened=0\n"));
  assert(strstr(text,"audio_generated_blocks=0\n"));
  assert(strstr(text,"audio_init_error=DSP init failed missing firmware\n"));
  assert(!rmdir(directory));
  puts("Audio diagnostics: PCM/output detection, immutable samples, status, timer wrap and failed init passed.");
}
