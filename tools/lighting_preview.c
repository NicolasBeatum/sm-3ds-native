/* Read-only native preview from the user's ROM and SRAM slot 2.
 * All diagnostic output goes to an explicitly supplied directory. */
#include "src/sm_cpu_infra.h"
#include "src/sm_rtl.h"
#include "src/ida_types.h"
#include "src/variables.h"
#include "src/funcs.h"
#include "src/enemy_types.h"
#include "src/snes/ppu.h"
#include "src/spc_player.h"
#include "scene_lighting.h"
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

bool g_debug_flag, g_other_image, g_new_ppu = true;
int g_got_mismatch_count;
SpcPlayer *g_spc_player;
static PicaLine captured[224];
static bool lavaFixture;
static bool shaktoolFixture;
static bool dumpFixture;
static void (*finishPreview)(void);
static uint8 dumpRam[sizeof(g_ram)];
static unsigned DumpWord(const void *address) {
  size_t offset=(const uint8 *)address-g_ram;
  return dumpRam[offset]|((unsigned)dumpRam[offset+1]<<8);
}
static void DumpPosition(void) {
  room_ptr=DumpWord(&room_ptr);door_def_ptr=DumpWord(&door_def_ptr);
  layer1_x_pos=DumpWord(&layer1_x_pos);layer1_y_pos=DumpWord(&layer1_y_pos);
  bg1_x_offset=DumpWord(&bg1_x_offset);bg1_y_offset=DumpWord(&bg1_y_offset);
  samus_x_pos=samus_prev_x_pos=DumpWord(&samus_x_pos);
  samus_y_pos=samus_prev_y_pos=DumpWord(&samus_y_pos);
}
void __real_LoadFromLoadStation(void);
void __wrap_LoadFromLoadStation(void) {
  __real_LoadFromLoadStation();
  if(dumpFixture) {DumpPosition();return;}
  if (shaktoolFixture) {
    room_ptr=0xd8c5;door_def_ptr=0xa798;
    bg1_x_offset=layer1_x_pos=0;bg1_y_offset=layer1_y_pos=0;
    samus_x_pos=samus_prev_x_pos=96;samus_y_pos=samus_prev_y_pos=160;
    return;
  }
  if (!lavaFixture) return;
  /* Host-only diagnostic entry into the real Norfair room AC83. Its ROM
   * header selects FX type 2 with surface Y=198; do not synthesize the FX. */
  room_ptr=0xac83; door_def_ptr=0x952e;
  bg1_x_offset=layer1_x_pos=0;
  bg1_y_offset=layer1_y_pos=0;
  samus_x_pos=samus_prev_x_pos=128;
  samus_y_pos=samus_prev_y_pos=152;
}
void __real_ppu_runLine(Ppu *, int);
void __wrap_ppu_runLine(Ppu *p, int line) {
  if (line > 0 && line <= 224) PicaCaptureLine(&captured[line-1],p);
  __real_ppu_runLine(p,line);
}
void Warning(const char *s) { fprintf(stderr,"%s\n",s); }
void RtlApuLock(void) {}
void RtlApuUnlock(void) {}
void RtlApuQueueLock(void) {}
void RtlApuQueueUnlock(void) {}
void Die(const char *s) { fprintf(stderr, "%s\n", s); exit(1); }

static void CaptureDoorRoute(Snes *s, const char *root) {
  /* Diagnostic positioning into AC83's real left-door corridor, followed
   * exclusively by native movement/shooting and the normal room coroutine. */
  samus_x_pos=samus_prev_x_pos=64;
  samus_y_pos=samus_prev_y_pos=128;
  bool saw=false,finished=false;
  for (unsigned step=0;step<800;step++) {
    unsigned input=button_config_left | ((step%12<2)?button_config_shoot_x:0),bits=0;
    for (unsigned j=0;j<16;j++,input>>=1) bits=(bits<<1)|(input&1);
    RtlRunFrame(bits);
    if (game_state>=9 && game_state<=11) saw=true;
    if (!(game_state>=9 && game_state<=11) && !(saw && game_state==8)) continue;
    char directory[1024],path[1200];
    snprintf(directory,sizeof(directory),"%s/door-%03u",root,step);
    if (mkdir(directory,0755)!=0) Die("Door capture directory must not exist");
    const char *names[]={"landing-wram.bin","landing-lines.bin","landing-ppu.bin"};
    const void *data[]={g_ram,captured,s->ppu};
    const size_t sizes[]={sizeof(g_ram),sizeof(captured),sizeof(*s->ppu)};
    for (unsigned i=0;i<3;i++) {
      snprintf(path,sizeof(path),"%s/%s",directory,names[i]);
      FILE *file=fopen(path,"wb");
      if (!file || fwrite(data[i],1,sizes[i],file)!=sizes[i]) Die("Door capture write failed");
      fclose(file);
    }
    snprintf(path,sizeof(path),"%s/state.txt",directory);
    FILE *file=fopen(path,"w");if (!file) Die("Door state write failed");
    fprintf(file,"frame=%u state=%u room=%04x step=%04x\n",
        step,game_state,room_ptr,door_transition_function);fclose(file);
    if (game_state==8) {
      printf("door returned to gameplay step=%u room=%04x\n",step,room_ptr);
      finished=true;break;
    }
  }
  if (!saw || !finished) Die("Native door route did not complete");
}

int main(int argc, char **argv) {
  if (argc != 4 && argc != 5) { fprintf(stderr,"usage: %s ROM SRAM output-directory [fire|jump|move|slot1|slot1-move|acid|lava|shaktool|dump:WRAM|door-left|station:AREA:INDEX]\n",argv[0]); return 1; }
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
  bool slot1Move = argc==5 && !strcmp(argv[4],"slot1-move");
  unsigned slot = argc == 5 && (!strcmp(argv[4],"slot1") || slot1Move) ? 0 : 1;
  if (LoadFromSram(slot)) Die("Save slot has an invalid checksum");
  printf("slot %u: area=%u station=%u health=%u\n",slot+1,area_index,load_station_index,samus_health);
  selected_save_slot = slot;
  shaktoolFixture=argc==5 && !strcmp(argv[4],"shaktool");
  dumpFixture=argc==5 && !strncmp(argv[4],"dump:",5);
  if(dumpFixture) {
    file=fopen(argv[4]+5,"rb");
    if(!file || fread(dumpRam,1,sizeof(dumpRam),file)!=sizeof(dumpRam) || fgetc(file)!=EOF)
      Die("Dump must be a complete 128 KiB WRAM file");
    fclose(file);
    if(DumpWord(&game_state)!=8 || DumpWord(&area_index)>5 ||
        DumpWord(&room_ptr)<0x9000 || DumpWord(&room_ptr)>=0xe000)
      Die("Dump fixture requires a native gameplay room");
    /* Reconstruct room selection/progress, not a hardware savestate: PPU,
     * audio and enemy timers are initialized normally from the local ROM. */
    memcpy(&equipped_items,dumpRam+((uint8 *)&equipped_items-g_ram),96);
    memcpy(events_that_happened,dumpRam+(events_that_happened-g_ram),
        (uint8 *)&loading_game_state-events_that_happened);
    area_index=DumpWord(&area_index);load_station_index=0;
  }
  bool acid = argc==5 && !strcmp(argv[4],"acid");
  bool doorFixture = argc==5 && !strcmp(argv[4],"door-left");
  lavaFixture = doorFixture || (argc==5 && !strcmp(argv[4],"lava"));
  bool stationFixture = argc==5 && !strncmp(argv[4],"station:",8);
  unsigned fixtureArea=0, fixtureIndex=0, fixtureRoom=0;
  if (stationFixture) {
    char extra;
    if (sscanf(argv[4],"station:%u:%u%c",&fixtureArea,&fixtureIndex,&extra)!=2 ||
        fixtureArea>5 || fixtureIndex>18)
      Die("Diagnostic station must be AREA 0..5 / INDEX 0..18");
    const uint16 *lists=(const uint16 *)RomPtr(0x80c4b5);
    const LoadStationList *entry=(const LoadStationList *)RomPtr_80(
        lists[fixtureArea]+14*fixtureIndex);
    fixtureRoom=entry->room_ptr_;
    if (fixtureRoom<0x9000 || fixtureRoom>=0xe000 ||
        get_RoomDefHeader(fixtureRoom)->area_index_!=fixtureArea)
      Die("Diagnostic station does not point to a room in that area");
  }
  if(acid || lavaFixture || stationFixture || shaktoolFixture) {
    /* Host-only diagnostic spawn via a real ROM load station, or a diagnostic
     * room wrapper. Only local preview SRAM is a write destination. */
    area_index=shaktoolFixture?4:stationFixture?fixtureArea:2;
    load_station_index=stationFixture?fixtureIndex:shaktoolFixture?0:10;
    equipped_items|=shaktoolFixture?0x25:0x21;
    if(shaktoolFixture) samus_power_bombs=samus_max_power_bombs=50;
    samus_health=samus_max_health=9999;
  }
  loading_game_state = 5;
  game_state = 6;
  coroutine_state_0 = coroutine_state_1 = coroutine_state_2 = coroutine_state_3 = 0;
  for (unsigned i = 0; i < (dumpFixture?180u:1200u); i++) RtlRunFrame(0);
  if(dumpFixture) {
    if(room_ptr!=DumpWord(&room_ptr) || game_state!=8)
      Die("Reconstructed dump left the requested room");
    /* Keep the requested viewpoint while native scrolling/HDMA settles. */
    for(unsigned i=0;i<4;i++) {DumpPosition();RtlRunFrame(0);}
    printf("dump reconstruction room=%04x samus=%u,%u camera=%u,%u (not exact hardware state)\n",room_ptr,samus_x_pos,samus_y_pos,layer1_x_pos,layer1_y_pos);
  }
  printf("room=%04x state=%u samus=(%u,%u) camera=(%u,%u)\n",
          room_ptr,game_state,samus_x_pos,samus_y_pos,layer1_x_pos,layer1_y_pos);
  if (game_state != 8 || (!acid && !lavaFixture && !stationFixture && !shaktoolFixture && !dumpFixture &&
      slot == 1 && room_ptr != 0x91f8)) Die("Preview did not reach expected gameplay");
  if (stationFixture && (room_ptr!=fixtureRoom || area_index!=fixtureArea))
    Die("Diagnostic station did not reach the selected room and area");
  if (argc == 5 && (slot == 1 || slot1Move) && !acid && !lavaFixture && !stationFixture && !shaktoolFixture && !dumpFixture) {
    unsigned input, frames;
    if (!strcmp(argv[4],"fire")) { input=button_config_shoot_x; frames=10; }
    else if (!strcmp(argv[4],"jump")) { input=button_config_jump_a; frames=12; }
    else if (!strcmp(argv[4],"move") || slot1Move) { input=button_config_right|button_config_run_b; frames=24; }
    else Die("Unknown action; choose fire, jump or move");
    /* Samus initially faces the screen on the ship; turn before firing/jumping. */
    if (strcmp(argv[4],"move") && !slot1Move) {
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
    if ((!strcmp(argv[4],"move") || slot1Move) && samus_x_pos<=initialX) Die("Right input did not move Samus");
    if (game_state!=8 || room_ptr!=(slot1Move?0xa70b:0x91f8)) Die("Action left the trial scene");
  }
  if(shaktoolFixture) {
    if(room_ptr!=0xd8c5) Die("Shaktool fixture loaded wrong room");
    RtlSetSpriteViewportMargin(72);
    unsigned jump=0,jumpConfig=button_config_jump_a;
    for(unsigned i=0;i<16;i++,jumpConfig>>=1)jump=(jump<<1)|(jumpConfig&1);
    unsigned shoot=0,encoded=button_config_shoot_x;
    for(unsigned i=0;i<16;i++,encoded>>=1)shoot=(shoot<<1)|(encoded&1);
    for(unsigned i=0;i<12;i++)RtlRunFrame(0x20);
    for(unsigned i=0;i<12;i++)RtlRunFrame(0);
    for(unsigned i=0;i<12;i++)RtlRunFrame(0x20);
    for(unsigned i=0;i<20;i++)RtlRunFrame(0);
    hud_item_index=3;
    printf("PB start pose=%04x move=%04x ammo=%u shoot=%04x\n",samus_pose,samus_movement_type,samus_power_bombs,shoot);
    for(unsigned i=0;i<10;i++)RtlRunFrame(shoot);
    for(unsigned i=0;i<350;i++)RtlRunFrame(0);
    printf("PB end pose=%04x ammo=%u explosion=%04x\n",samus_pose,samus_power_bombs,power_bomb_explosion_status);
    // Return to standing after the native explosion.
    for(unsigned i=0;i<20;i++)RtlRunFrame(0x10);
    for(unsigned i=0;i<4000;i++) {
      RtlRunFrame(0x80 | (i%120<20?jump:0) | (samus_x_pos>940 && i%24<8?shoot:0));
      if(i%120==0) {
        Enemy_Shaktool *h=Get_Shaktool(384);
        printf("route step=%u room=%04x samus=%u,%u head=%u,%u angles=%04x/%04x step=%04x param=%04x instr=%04x\n",i,room_ptr,samus_x_pos,samus_y_pos,h->base.x_pos,h->base.y_pos,h->shakt_var_A,h->shakt_var_B,h->shakt_var_F,h->shakt_parameter_1,h->base.current_instruction);
      }
      if(room_ptr!=0xd8c5)break;
    }
    for(unsigned i=0;i<300 && game_state!=8;i++)RtlRunFrame(0);
    printf("Shaktool exit room=%04x state=%u samus=%u,%u spring=%u\n",room_ptr,game_state,samus_x_pos,samus_y_pos,!!(collected_items&2));
    if(room_ptr==0xd8c5) {
      for(unsigned i=0;i<7;i++) {
        Enemy_Shaktool *e=Get_Shaktool(i*64);
        printf("Shaktool part=%u enemy=%04x pos=%u,%u angle=%04x/%04x step=%04x param=%04x instr=%04x props=%04x\n",i,e->base.enemy_ptr,e->base.x_pos,e->base.y_pos,e->shakt_var_A,e->shakt_var_B,e->shakt_var_F,e->shakt_parameter_1,e->base.current_instruction,e->base.properties);
      }
    }
    if(room_ptr!=0xd6d0 || game_state!=8)
      Die("Shaktool route did not reach the Spring Ball room");
  }
  printf("FX type=%u liquid_y=%u screen_y=%d\n",fx_type,lava_acid_y_pos,(int16_t)(lava_acid_y_pos-layer1_y_pos));
  if(acid && fx_type!=4) Die("Diagnostic spawn did not reach the acid scene");
  if(lavaFixture && (room_ptr!=0xac83 || fx_type!=2 ||
      (int16_t)(lava_acid_y_pos-layer1_y_pos)<32 ||
      (int16_t)(lava_acid_y_pos-layer1_y_pos)>=224))
    Die("Diagnostic spawn did not reach visible real lava");
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
  if (doorFixture) CaptureDoorRoute(s,argv[3]);
  if(finishPreview) finishPreview();
  snes_free(s);
  return 0;
}
