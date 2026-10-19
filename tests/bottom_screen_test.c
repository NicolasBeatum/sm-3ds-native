/* Exercise real UI hit targets, persistence and raster output on the host. */
#include <assert.h>
#include <stdlib.h>
#include "../source/bottom_screen.c"

uint8_t g_ram[0x20000];
static uint8_t rom[0x400000];
const uint8_t *g_rom=rom;
const uint8_t *RomPtr(uint32_t address) {
  return rom+(((address>>16)<<15)|(address&0x7fff))%sizeof(rom);
}
uint64_t osGetTime(void) {return 1000;}
Result APT_CheckNew3DS(bool *value) {*value=false;return 0;}
void *linearMemAlign(size_t n,size_t alignment) {(void)alignment;return calloc(1,n);}
void linearFree(void *p) {free(p);}
uint8_t *gfxGetFramebuffer(int a,int b,void *c,void *d) {(void)a;(void)b;(void)c;(void)d;return NULL;}
void LightLock_Init(LightLock *p) {*p=0;}
void LightLock_Lock(LightLock *p) {(void)p;}
void LightLock_Unlock(LightLock *p) {(void)p;}
void LightEvent_Init(LightEvent *p,int x) {*p=x;}
void LightEvent_Wait(LightEvent *p) {(void)p;}
void LightEvent_Signal(LightEvent *p) {(void)p;}
Thread threadCreate(void (*fn)(void *),void *p,size_t n,int pri,int cpu,bool detached) {
  (void)fn;(void)p;(void)n;(void)pri;(void)cpu;(void)detached;return NULL;
}
void threadJoin(Thread p,uint64_t timeout) {(void)p;(void)timeout;}
void threadFree(Thread p) {(void)p;}
static void Touch(int x,int y) {BottomScreen_HandleTouch((x+.25f)/320,(y+.25f)/240);}
static uint32_t Pixel(int x,int y) {return ((uint32_t *)g_bottom_cache)[y*512+x];}
int main(int argc,char **argv) {
  assert((argc==2 || argc==5) && BottomScreen_Init());
  game_state=8;
  remove(argv[1]);BottomScreen_LoadSettings(argv[1]);
  assert(TextWidth("ESPAÑOL",1)==41 && TextWidth("ILUMINACIÓN",1)==65);
  assert(TextWidth("ÁÉÍÓÚÑ",1)==35);
  for(unsigned compact=0;compact<2;compact++) {
    g_status_bar_visible[kBottomTab_Setup]=compact;
    int top=compact?44:3,start=QolTop(top),groupY=top+(compact?23:29);
    Touch(270,220);Touch(13+98+20,groupY+8);
    assert(g_setup_group==kSetupMap && !g_map_qol_open);
    Touch(100,start+4*SetupRowStep(compact)+8);
    assert(g_map_qol_open && g_qol_page==kQolOverview);
    unsigned initial=g_item_symbols;
    Touch(100,start+(compact?21:25)+8);assert(g_qol_page==kQolItems && g_item_symbols==initial);
    int optionY=start+(compact?21:25),step=compact?17:21;
    for(unsigned mode=0;mode<3;mode++) {
      Touch(100,optionY+mode*step+8);assert(g_item_symbols==mode);
      for(unsigned language=0;language<2;language++) {
        g_ui_language=language;DrawSetupTab(g_bottom_cache,top);
      }
    }
    /* Gaps and help text must never change the selected mode. */
    Touch(100,optionY+step-1);assert(g_item_symbols==2);
    bool miniBefore=g_minimap_items;
    Touch(100,optionY+3*step+8);assert(g_item_symbols==2 && g_minimap_items!=miniBefore);
    Touch(100,optionY+3*step+(compact?16:18)+3);assert(g_item_symbols==2 && g_minimap_items!=miniBefore);
    Touch(40,start+8);assert(g_qol_page==kQolOverview);
    for(unsigned page=1;page<=2;page++) {
      bool before=page==1?g_map_door_colors:g_map_room_colors;
      Touch(100,start+(compact?21:25)+page*SetupRowStep(compact)+8);
      assert(g_qol_page==(enum QolPage)(kQolItems+page));
      assert((page==1?g_map_door_colors:g_map_room_colors)==before);
      Touch(100,optionY+8);
      assert((page==1?g_map_door_colors:g_map_room_colors)!=before);
      for(unsigned language=0;language<2;language++) {
        g_ui_language=language;DrawSetupTab(g_bottom_cache,top);
      }
      Touch(40,start+8);assert(g_qol_page==kQolOverview);
    }
    Touch(100,start+(compact?21:25)+3*SetupRowStep(compact)+8);
    assert(g_qol_page==kQolStations);
    bool before=g_map_stations;
    Touch(100,start+(compact?21:25)+8);assert(g_map_stations!=before);
    for(unsigned language=0;language<2;language++) {g_ui_language=language;DrawSetupTab(g_bottom_cache,top);}
    Touch(40,start+8);assert(g_qol_page==kQolOverview);
    Touch(40,start+8);assert(!g_map_qol_open && g_setup_group==kSetupMap);
    Touch(60,groupY+8);assert(g_setup_group==kSetupHud && !g_hud_language_open);
    Touch(100,start+5*SetupRowStep(compact)+7);
    assert(g_hud_mode_open);
    for(unsigned mode=0;mode<kMainHudModeCount;mode++) {
      int optionY=start+(compact?21:25)+mode*(compact?17:22);
      Touch(100,optionY+6);assert(BottomScreen_MainHudMode()==mode);
      DrawSetupTab(g_bottom_cache,top);
      Touch(100,optionY+(compact?17:22)-1);assert(BottomScreen_MainHudMode()==mode);
    }
    Touch(40,start+8);assert(!g_hud_mode_open);
    Touch(100,start+6*SetupRowStep(compact)+7);
    assert(g_hud_language_open);
    for(unsigned language=0;language<2;language++) {
      Touch(100,start+(compact?21:25)+language*(compact?25:32)+8);
      assert(g_ui_language==language);
      assert(!strcmp(BottomScreen_Localize("English","Español"),language?"Español":"English"));
      DrawSetupTab(g_bottom_cache,top);
    }
    Touch(40,start+8);assert(!g_hud_language_open && g_setup_group==kSetupHud);
    DrawSetupTab(g_bottom_cache,top);
  }
  /* Reload settings: UI language and all three QoL choices persist. */
  g_ui_language=1;g_item_symbols=1;g_map_door_colors=true;g_map_room_colors=false;
  assert(BottomScreen_SaveSettings());
  g_ui_language=0;g_item_symbols=0;g_map_door_colors=false;g_map_room_colors=true;
  BottomScreen_LoadSettings(argv[1]);
  assert(g_ui_language==1 && g_item_symbols==1 && g_map_door_colors && !g_map_room_colors);
  FILE *f=fopen(argv[1],"w");assert(f);fputs("ui_language=99\nqol_item_mode=99\n",f);fclose(f);
  BottomScreen_LoadSettings(argv[1]);assert(g_ui_language==1 && g_item_symbols==1);
  g_map_stations=false;assert(BottomScreen_SaveSettings());
  g_map_stations=true;BottomScreen_LoadSettings(argv[1]);assert(!g_map_stations);
  g_minimap_items=false;assert(BottomScreen_SaveSettings());
  g_minimap_items=true;BottomScreen_LoadSettings(argv[1]);assert(!g_minimap_items);
  /* Old boolean settings migrate to the matching legacy black-band mode,
   * independently of key order; invalid new values are ignored. */
  for(unsigned mode=0;mode<kMainHudModeCount;mode++) {
    g_main_hud_mode=mode;assert(BottomScreen_SaveSettings());
    g_main_hud_mode=kMainHudFull;BottomScreen_LoadSettings(argv[1]);
    assert(g_main_hud_mode==mode);
  }
  f=fopen(argv[1],"w");assert(f);fputs("hide_main_hud=1\n",f);fclose(f);
  BottomScreen_LoadSettings(argv[1]);assert(g_main_hud_mode==kMainHudBlackBand);
  f=fopen(argv[1],"w");assert(f);fputs("main_hud_mode=2\nhide_main_hud=1\n",f);fclose(f);
  BottomScreen_LoadSettings(argv[1]);assert(g_main_hud_mode==kMainHudEnergyMap);
  f=fopen(argv[1],"w");assert(f);fputs("main_hud_mode=99\n",f);fclose(f);
  BottomScreen_LoadSettings(argv[1]);assert(g_main_hud_mode==kMainHudEnergyMap);
  /* The position dot remains visible and small at every supported zoom. */
  for(int size=2;size<=20;size++) {
    FillRect(g_bottom_cache,0,0,320,240,kWhite);
    DrawSamusMarker(g_bottom_cache,30,30,size,size);
    assert(Pixel(30+size/2,30+size/2)==0xffff4646);
    unsigned changed=0,red=0;
    for(int y=0;y<240;y++) for(int x=0;x<320;x++) {
      changed+=Pixel(x,y)!=0xffebeef8;
      red+=Pixel(x,y)==0xffff4646;
    }
    assert(changed==32 && red==12);
  }
  /* Crateria's native colored fills become neutral, distinct from cyan caps. */
  UiColor c=TintColor((UiColor){80,80,240},0,false);
  assert(c.r>=c.b && c.g>=c.b);
  assert(c.b<160 && c.r<184);
  if(argc==5) {
    FILE *f=fopen(argv[2],"rb");assert(f);
    size_t size=fread(rom,1,sizeof(rom),f);fclose(f);assert(size==0x300000);
    QolMap_Init(rom,size);g_item_symbols=kItemSymbolsSuperMetroid;g_map_stations=true;
    assert(QolMap_ServiceAt(1,5,8,true)==kMapServiceEnergy);
    assert(QolMap_ServiceAt(1,9,13,true)==kMapServiceMissile);
    assert(QolMap_ServiceAt(1,5,5,true)==kMapServiceMap);
    for(unsigned style=0;style<3;style++) {
      g_item_symbols=style;
      assert(!QolMap_UseCircle(1,5,8,true) && !QolMap_UseCircle(1,9,13,true) && !QolMap_UseCircle(1,5,5,true));
    }
    g_item_symbols=kItemSymbolsSuperMetroid;
    assert(!QolMap_ServiceAt(1,5,8,false));
    g_map_stations=false;assert(!QolMap_ServiceAt(1,5,8,true));g_map_stations=true;
    const uint8_t *mt=RomPtr(0xb68000);const uint16_t *mp=(const uint16_t *)RomPtr(0xb6f000);
    for(unsigned kind=1;kind<=3;kind++)for(unsigned flip=0;flip<4;flip++) {
      unsigned entry=0xc6f|(flip<<14)|((NATIVE_MAP_DOT_VARIANTS+kind)<<16);
      for(unsigned y=1;y<=6;y++)for(unsigned x=1;x<=6;x++)
        assert(AreaMapPixel(mt,mp,1,entry,x,y,false,0)==AreaMapPixel(mt,mp,1,0xc6f|((NATIVE_MAP_DOT_VARIANTS+kind)<<16),x,y,false,0));
    }
    for(unsigned d=3;d<5;d++) {
      f=fopen(argv[d],"rb");assert(f);assert(fread(g_ram,1,sizeof(g_ram),f)==sizeof(g_ram));fclose(f);
      QolMap_RefreshColors();
      assert(room_ptr==0x9d19 && area_index==1);
      assert(QolMap_ItemAt(1,17,8,true)==kItemMapCollected && QolMap_UseCircle(1,17,8,true));
      PicaFrame itemFrame={0};g_minimap_items=false;QolMap_PrepareNative(&itemFrame);assert(!itemFrame.nativeMapCircle);
      assert(QolMap_UseCircle(1,17,8,true)); /* Lower maps keep the selected style. */
      g_minimap_items=true;QolMap_PrepareNative(&itemFrame);assert(itemFrame.nativeMapCircle);
      g_minimap_items=false;game_state=15;menu_index=0;QolMap_PrepareNative(&itemFrame);assert(itemFrame.nativeMapCircle);
      game_state=8;
      const uint16_t *map=AreaTilemap(1);uint32_t entry=map[8*32+17];
      assert((entry&1023)==0x77 && NativeMap_Variant(entry));
      const uint8_t *tiles=RomPtr(0xb68000);const uint16_t *pal=(const uint16_t *)RomPtr(0xb6f000);
      uint32_t circle=entry|(NativeMap_Variant(entry)<<16);
      assert(AreaMapPixel(tiles,pal,1,entry,3,3,false,0)!=AreaMapPixel(tiles,pal,1,circle,3,3,false,0));
      for(int y=0;y<8;y++) for(int x=0;x<8;x++)
        if(x<2 || x>5 || y<2 || y>5)assert(AreaMapPixel(tiles,pal,1,entry,x,y,false,0)==AreaMapPixel(tiles,pal,1,circle,x,y,false,0));
      /* Dump 2's door is west of Samus, not on the east edge of her cell. */
      unsigned style=QolMap_CellStyle(1,16,5);
      assert((style&7)==kMapDoorBlue && !((style>>3)&7));
      /* The first blue door west of the ship belongs to the west wall. */
      style=QolMap_CellStyle(0,23,5);assert((style&7)==kMapDoorBlue && !((style>>3)&7));
    }
    puts("Oct 18 dumps: collected missile 0x15 uses the complete circle variant; Pink Brinstar/ship west caps point west.");
  }
  BottomScreen_Fini();remove(argv[1]);
  puts("Bottom UI: QoL navigation, touch gaps, both layouts/languages, persistence, UTF-8 and unobscured item centers passed.");
}
