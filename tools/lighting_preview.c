/* Read-only native preview from the user's ROM and SRAM slot 2.
 * All diagnostic output goes to an explicitly supplied directory. */
#include "src/sm_cpu_infra.h"
#include "src/sm_rtl.h"
#include "src/ida_types.h"
#include "src/variables.h"
#include "src/funcs.h"
#include "src/snes/ppu.h"
#include "src/spc_player.h"
#include "scene_lighting.h"
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

bool g_debug_flag, g_other_image, g_new_ppu = true;
int g_got_mismatch_count;
SpcPlayer *g_spc_player;
static PicaLine captured[224];
void __real_ppu_runLine(Ppu *, int);
void __wrap_ppu_runLine(Ppu *p, int line) {
  if (line > 0 && line <= 224) PicaCaptureLine(&captured[line-1],p);
  __real_ppu_runLine(p,line);
}
void Warning(const char *s) { fprintf(stderr,"%s\n",s); }
void RtlApuLock(void) {}
void RtlApuUnlock(void) {}
void Die(const char *s) { fprintf(stderr, "%s\n", s); exit(1); }

int main(int argc, char **argv) {
  if (argc != 4 && argc != 5) { fprintf(stderr,"usage: %s ROM SRAM output-directory [fire|jump|move|slot1]\n",argv[0]); return 1; }
  /* Never permit game SRAM writes to the source save. */
  char path[1024];
  snprintf(path,sizeof(path),"%s/preview-only.srm",argv[3]);
  RtlSetSramPath(path);
  g_spc_player = SpcPlayer_Create();
  SpcPlayer_Initialize(g_spc_player);
  Snes *s = SnesInit(argv[1]);
  if (!s) Die(SnesRomLoadError());
  static uint8 pixels[256*256*4];
  PpuBeginDrawing(s->snes_ppu, pixels, 256*4, 0);
  PpuBeginDrawing(s->my_ppu, pixels, 256*4, 0);
  for (unsigned i = 0; i < 180; i++) RtlRunFrame(0);
  FILE *file = fopen(argv[2], "rb");
  if (!file || fread(g_sram, 1, 8192, file) != 8192) Die("SRAM read failed");
  fclose(file);
  unsigned slot = argc == 5 && !strcmp(argv[4],"slot1") ? 0 : 1;
  if (LoadFromSram(slot)) Die("Save slot has an invalid checksum");
  printf("slot %u: area=%u station=%u health=%u\n",slot+1,area_index,load_station_index,samus_health);
  selected_save_slot = slot;
  loading_game_state = 5;
  game_state = 6;
  coroutine_state_0 = coroutine_state_1 = coroutine_state_2 = coroutine_state_3 = 0;
  for (unsigned i = 0; i < 1200; i++) RtlRunFrame(0);
  printf("room=%04x state=%u samus=(%u,%u) camera=(%u,%u)\n",
          room_ptr,game_state,samus_x_pos,samus_y_pos,layer1_x_pos,layer1_y_pos);
  if (game_state != 8 || (slot == 1 && room_ptr != 0x91f8)) Die("Slot 2 did not reach Landing Site gameplay");
  if (argc == 5 && slot == 1) {
    unsigned input, frames;
    if (!strcmp(argv[4],"fire")) { input=button_config_shoot_x; frames=10; }
    else if (!strcmp(argv[4],"jump")) { input=button_config_jump_a; frames=12; }
    else if (!strcmp(argv[4],"move")) { input=button_config_right|button_config_run_b; frames=24; }
    else Die("Unknown action; choose fire, jump or move");
    /* Samus initially faces the screen on the ship; turn before firing/jumping. */
    if (strcmp(argv[4],"move")) {
      for (unsigned i=0;i<8;i++) RtlRunFrame(0x80);
      RtlRunFrame(0);
    }
    unsigned bits=0, encoded=input;
    for (unsigned i=0;i<16;i++,encoded>>=1) bits=(bits<<1)|(encoded&1);
    unsigned initialX=samus_x_pos, initialY=samus_y_pos;
    for (unsigned i=0;i<frames;i++) RtlRunFrame(bits);
    unsigned projectiles=0;
    for (unsigned i=0;i<10;i++) projectiles+=projectile_type[i]!=0;
    printf("action=%s samus=(%u,%u) charge=%u projectiles=%u\n",
           argv[4],samus_x_pos,samus_y_pos,flare_counter,projectiles);
    if (!strcmp(argv[4],"fire") && !projectiles) Die("Fire did not spawn a projectile");
    if (!strcmp(argv[4],"jump") && samus_y_pos>=initialY) Die("Jump did not move Samus upward");
    if (!strcmp(argv[4],"move") && samus_x_pos<=initialX) Die("Right input did not move Samus");
    if (game_state!=8 || room_ptr!=0x91f8) Die("Action left the trial scene");
  }
  snprintf(path,sizeof(path),"%s/landing-wram.bin",argv[3]);
  file=fopen(path,"wb"); if(!file) Die("WRAM output failed");
  fwrite(g_ram,1,sizeof(g_ram),file); fclose(file);
  snprintf(path,sizeof(path),"%s/landing-lines.bin",argv[3]);
  file=fopen(path,"wb"); if(!file) Die("Lines output failed");
  fwrite(captured,1,sizeof(captured),file); fclose(file);
  snprintf(path,sizeof(path),"%s/landing-ppu.bin",argv[3]);
  file=fopen(path,"wb"); if(!file) Die("PPU output failed");
  fwrite(s->ppu,1,sizeof(*s->ppu),file); fclose(file);

  snprintf(path,sizeof(path),"%s/landing.ppm",argv[3]);
  file=fopen(path,"wb"); if(!file) Die("Image output failed");
  fprintf(file,"P6\n256 224\n255\n");
  for(unsigned y=0;y<224;y++) for(unsigned x=0;x<256;x++) {
    uint8 *p=pixels+(y*256+x)*4; uint8 rgb[3]={p[2],p[1],p[0]};
    fwrite(rgb,1,3,file);
  }
  fclose(file);
  snes_free(s);
  return 0;
}
