#include "map_features.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static uint8_t rom[0x80000];
static MapFeatures map;
static ItemMapIndex items;
static void Word(unsigned bank,unsigned address,unsigned value) {
  unsigned p=((bank&127)<<15)|(address&32767);
  rom[p]=value;rom[p+1]=value>>8;
}
int main(int argc,char **argv) {
  Word(0x8f,0x91f9,0x0a00);Word(0x8f,0x91fb,0x0204);
  Word(0x8f,0x91fd,2);Word(0x8f,0x9201,0x9400);
  Word(0x8f,0x9203,0xe5e6);
  Word(0x8f,0x9205+6,0x9000);Word(0x8f,0x9205+20,0xa000);
  Word(0x83,0x9008,0x0200);Word(0x83,0x900c,0x0100); /* Lava / heat FX. */
  Word(0x83,0xac46,0xa000);Word(0x83,0xa000,0xf761);Word(0x8d,0xf761,0xe440);
  Word(0x8f,0xa000,0xc860);Word(0x8f,0xa002,0x0401);Word(0x8f,0xa004,3);
  Word(0x8f,0xa006,0xc89c);Word(0x8f,0xa008,0x0108);Word(0x8f,0xa00a,4);
  Word(0x8f,0xa00c,0xc842);Word(0x8f,0xa00e,0x041e);Word(0x8f,0xa010,0x8405);
  assert(MapFeatures_Build(&map,&items,rom,sizeof(rom),0x91f8) && map.valid);
  uint8_t opened[64]={0};MapFeatures_RefreshDoors(&map,opened);
  unsigned style=MapFeatures_At(&map,0,10,5);
  assert((style&7)==kMapDoorGreen && ((style>>6)&7)==kMapDoorYellow);
  assert((style&kMapRoomHeat) && (style&kMapRoomLava));
  assert(((MapFeatures_At(&map,0,11,5)>>3)&7)==kMapDoorGrey);
  opened[0]=1<<3;MapFeatures_RefreshDoors(&map,opened);
  assert((MapFeatures_At(&map,0,10,5)&7)==kMapDoorBlue);
  /* Negative native door arguments never persist as unlocked. */
  opened[0]=255;MapFeatures_RefreshDoors(&map,opened);
  assert(((MapFeatures_At(&map,0,11,5)>>3)&7)==kMapDoorGrey);
  assert(!MapFeatures_At(&map,6,0,0) && !MapFeatures_At(&map,0,-1,0));
  uint32_t wall=0xffeeeeee;
  assert(MapFeatures_RoomPixel(kMapRoomHeat,false,wall)==wall);
  assert(MapFeatures_RoomPixel(kMapRoomHeat,true,wall)==0xffa85a18);
  assert(MapFeatures_RoomPixel(kMapRoomSecret,true,wall)==0xff28765a);
  /* Opposite caps are directional, with independent native lock bits. */
  assert(!(MapFeatures_At(&map,0,12,5)&7));
  assert(!((MapFeatures_At(&map,0,9,5)>>3)&7));
  map.entries[map.count++]=(MapDoor){(5*64+9)*4+1,0,kMapDoorBlue};
  map.refreshed=false;opened[0]=1;
  MapFeatures_RefreshDoors(&map,opened);
  assert((MapFeatures_At(&map,0,10,5)&7)==kMapDoorGreen);
  assert(((MapFeatures_At(&map,0,9,5)>>3)&7)==kMapDoorBlue);
  opened[0]=1<<3;MapFeatures_RefreshDoors(&map,opened);
  assert((MapFeatures_At(&map,0,10,5)&7)==kMapDoorBlue);
  assert(((MapFeatures_At(&map,0,9,5)>>3)&7)==kMapDoorBlue);
  map.entries[map.count-1].color=kMapDoorRed;map.entries[map.count-1].bit=7;
  map.refreshed=false;MapFeatures_RefreshDoors(&map,opened);
  assert((MapFeatures_At(&map,0,10,5)&7)==kMapDoorBlue);
  assert(((MapFeatures_At(&map,0,9,5)>>3)&7)==kMapDoorRed);
  for(int size=4;size<=24;size++) {
    MapDoorStroke right,left,bottom,top;
    assert(MapFeatures_Divider(kMapDoorRed<<3,1,10,20,size,size,&right));
    assert(MapFeatures_Divider(kMapDoorRed,0,10+size,20,size+1,size,&left));
    assert(right.x+1==left.x && right.y==left.y && right.w==1 && left.w==1);
    assert(right.h==size-2 && left.h==right.h && right.color==0xffff6884);
    assert(MapFeatures_Divider(kMapDoorGreen<<9,3,10,20,size,size,&bottom));
    assert(MapFeatures_Divider(kMapDoorGreen<<6,2,10,20+size,size,size+1,&top));
    assert(top.x==bottom.x && top.y==bottom.y+1 && top.h==1 && bottom.h==1);
    assert(top.w==bottom.w && top.color==0xff70e890);
  }
  MapDoorStroke q;
  assert(!MapFeatures_Divider(kMapDoorBlue,0,0,0,2,2,&q));
  /* A dry room's escape-only acid must not leak into the normal map. */
  Word(0x8f,0x9203,0xe612);Word(0x8f,0x9205,14);
  Word(0x8f,0x9206,0x9600);Word(0x8f,0x9208,0xe5e6);
  Word(0x8f,0x920a+6,0);Word(0x8f,0x920a+20,0);
  Word(0x8f,0x9600+6,0x9000);Word(0x8f,0x9600+20,0);
  Word(0x83,0x9008,0x0400);Word(0x83,0x900c,0);
  assert(MapFeatures_Build(&map,&items,rom,sizeof(rom),0x91f8));
  assert(!(MapFeatures_At(&map,0,10,5)&kMapRoomAcid));
  MapProgress progress={0};progress.events[1]=1<<6;
  MapFeatures_RefreshHazards(&map,rom,&progress);
  assert(MapFeatures_At(&map,0,10,5)&kMapRoomAcid);
  progress.events[1]=0;MapFeatures_RefreshHazards(&map,rom,&progress);
  assert(!(MapFeatures_At(&map,0,10,5)&kMapRoomAcid));
  /* The other native selectors follow boss flags and collected capacity. */
  const unsigned codes[]={0xe5ff,0xe629,0xe652,0xe669};
  for(unsigned c=0;c<4;c++) {
    Word(0x8f,0x9203,codes[c]);
    unsigned p=0x9205;
    if(c==1) rom[((0x8f&127)<<15)|(p++&32767)]=4;
    Word(0x8f,p,0x9600);Word(0x8f,p+2,0xe5e6);
    Word(0x8f,p+4+6,0);Word(0x8f,p+4+20,0);
    assert(MapFeatures_Build(&map,&items,rom,sizeof(rom),0x91f8));
    progress=(MapProgress){0};
    MapFeatures_RefreshHazards(&map,rom,&progress);
    assert(!(MapFeatures_At(&map,0,10,5)&kMapRoomAcid));
    if(c<2)progress.bosses[0]=c==0?1:4;
    else if(c==2) {progress.items=4;progress.missiles=1;}
    else progress.powerBombs=1;
    MapFeatures_RefreshHazards(&map,rom,&progress);
    assert(MapFeatures_At(&map,0,10,5)&kMapRoomAcid);
  }
  if(argc==2) {
    FILE *f=fopen(argv[1],"rb");assert(f);
    assert(!fseek(f,0,SEEK_END));long size=ftell(f);rewind(f);
    uint8_t *data=malloc(size);assert(data && fread(data,1,size,f)==(size_t)size);fclose(f);
    assert(MapFeatures_Build(&map,&items,data,size,0x91f8) && map.valid);
    assert(map.serviceCount==14);
    assert(MapFeatures_ServiceAt(&map,1,5,8)==kMapServiceEnergy);
    assert(MapFeatures_ServiceAt(&map,1,9,13)==kMapServiceMissile);
    assert(MapFeatures_ServiceAt(&map,0,23,8)==kMapServiceMap);
    assert(!MapFeatures_ServiceAt(&map,1,17,8)); /* Actual collectible missile. */
    assert(!MapFeatures_ServiceAt(&map,6,5,8) && !MapFeatures_ServiceAt(&map,1,-1,8));
    unsigned kinds[4]={0};
    for(unsigned i=0;i<map.serviceCount;i++)kinds[map.services[i].kind]++;
    assert(kinds[1]==2 && kinds[2]==7 && kinds[3]==5);
    unsigned colors[6]={0},heat=0,lava=0,acid=0;
    for(unsigned i=0;i<map.count;i++) colors[map.entries[i].color]++;
    for(unsigned a=0;a<6;a++) for(unsigned c=0;c<2048;c++) {
      heat+=(map.hazards[a][c]&1)!=0;lava+=(map.hazards[a][c]&2)!=0;
      acid+=(map.hazards[a][c]&4)!=0;
    }
    for(unsigned color=1;color<6;color++) assert(colors[color]);
    assert(heat && lava && acid && items.count==100);
    unsigned normalAcid=acid,escapeAcid=0;
    progress=(MapProgress){0};progress.events[1]=1<<6;
    MapFeatures_RefreshHazards(&map,data,&progress);
    for(unsigned a=0;a<6;a++)for(unsigned c=0;c<2048;c++)
      escapeAcid+=(map.hazards[a][c]&4)!=0;
    assert(escapeAcid>normalAcid);
    printf("Native room states: acid cells normal %u / escape %u.\n",normalAcid,escapeAcid);
    printf("Native ROM: %u door caps; blue %u, red %u, green %u, yellow %u, grey %u; hazard cells heat %u / lava %u / acid %u.\n",
        map.count,colors[1],colors[2],colors[3],colors[4],colors[5],heat,lava,acid);
    free(data);
  }
  puts("Map colors: native FX, directed door faces, native unlocks, plain-blue cap arguments and one-pixel dividers at every supported zoom passed.");
}
