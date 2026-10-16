/* Read-only host probe: clock the original SPC/DSP once per native frame.
 * Reuses the room reconstruction frontend; this is not a hardware recording. */
#define main NativePreviewMain
#define RtlRunFrame AudioPreviewFrame
#include "lighting_preview.c"
#undef RtlRunFrame
#undef main

extern bool RtlRunFrame(int inputs);
static FILE *wave;
static uint32_t pcmBytes;
static unsigned nonzeroBlocks;

bool AudioPreviewFrame(int inputs) {
  bool replay=RtlRunFrame(inputs);
  int16_t pcm[534*2];
  RtlRenderAudio(pcm,534,2);
  bool nonzero=false;
  for(unsigned i=0;i<534*2;i++) nonzero|=pcm[i]!=0;
  nonzeroBlocks+=nonzero;
  if(fwrite(pcm,1,sizeof(pcm),wave)!=sizeof(pcm)) Die("PCM write failed");
  pcmBytes+=sizeof(pcm);
  return replay;
}

static void Word(unsigned value,unsigned bytes) {
  for(unsigned i=0;i<bytes;i++) fputc((value>>(8*i))&255,wave);
}

static void FinishAudio(void) {
  for(unsigned i=0;i<300;i++) {
    /* Host-only fixed viewpoint while the native music settles. */
    if(game_state==8 && room_ptr==DumpWord(&room_ptr)) DumpPosition();
    AudioPreviewFrame(0);
  }
}

int main(int argc,char **argv) {
  if(argc!=5 || strncmp(argv[4],"dump:",5)) {
    fprintf(stderr,"usage: %s ROM SRAM OUTPUT_DIRECTORY dump:WRAM\n",argv[0]);return 1;
  }
  char path[1024];snprintf(path,sizeof(path),"%s/native-audio.wav",argv[3]);
  wave=fopen(path,"wb");if(!wave) Die("WAV open failed");
  for(unsigned i=0;i<44;i++) fputc(0,wave);
  finishPreview=FinishAudio;
  int result=NativePreviewMain(argc,argv);
  rewind(wave);
  fwrite("RIFF",1,4,wave);Word(pcmBytes+36,4);fwrite("WAVEfmt ",1,8,wave);
  Word(16,4);Word(1,2);Word(2,2);Word(32000,4);Word(32000*4,4);
  Word(4,2);Word(16,2);fwrite("data",1,4,wave);Word(pcmBytes,4);
  if(fclose(wave)) Die("WAV close failed");
  printf("original SPC/DSP PCM: bytes=%u nonzero_blocks=%u\n",pcmBytes,nonzeroBlocks);
  return result;
}
