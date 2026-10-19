#include "item_map.h"
#include "qol_map.h"
#include "bottom_screen.h"
#include "native_map_tile.h"
#include "src/variables.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

uint8_t g_ram[0x20000];
static bool minimapItems=true;
bool BottomScreen_MinimapItemsEnabled(void) {return minimapItems;}
static enum ItemSymbolMode mode=kItemSymbolsSuperMetroid;
enum ItemSymbolMode BottomScreen_ItemSymbolMode(void) { return mode; }
static uint8_t rom[0x80000];
static void Word(unsigned bank,unsigned address,unsigned value) {
  unsigned offset=((bank&127)<<15)|(address&32767);
  rom[offset]=value;rom[offset+1]=value>>8;
}
static void Header(unsigned p,unsigned x,unsigned doors) {
  Word(0x8f,p+1,x<<8);Word(0x8f,p+3,0x0204); /* area 0; x, y=4; w=2 */
  Word(0x8f,p+5,2);Word(0x8f,p+9,doors);
}
bool BottomScreen_MapStationsEnabled(void) {return true;}
int main(int argc,char **argv) {
  Header(0x91f8,10,0x9400);Header(0x9300,11,0x9420);
  Word(0x8f,0x9400,0x9500);Word(0x83,0x9500,0x9300);
  Word(0x8f,0x9420,0x9520);Word(0x83,0x9520,0x91f8);
  Word(0x8f,0x9203,0xe612);Word(0x8f,0x9206,0x9600);
  Word(0x8f,0x9208,0xe5e6);Word(0x8f,0x920a+20,0xa000);
  Word(0x8f,0x9600+20,0xa020);
  Word(0x8f,0x930b,0xe5e6);Word(0x8f,0x930d+20,0xa040);
  Word(0x8f,0xa000,0xeed7);Word(0x8f,0xa004,0);
  Word(0x8f,0xa006,0xeedb);Word(0x8f,0xa008,0x0101);Word(0x8f,0xa00a,1);
  Word(0x8f,0xa020,0xeed7);Word(0x8f,0xa024,0); /* alternate-state duplicate */
  Word(0x8f,0xa040,0xefcf);Word(0x8f,0xa044,2); /* hidden item */
  Word(0x8f,0xa00c,0xc860);Word(0x8f,0xa00e,0x0401);Word(0x8f,0xa010,3);
  Word(0x8f,0xa046,0xc842);Word(0x8f,0xa048,0x041e);Word(0x8f,0xa04a,0x8405);
  Word(0x83,0x9700,0x91f8);Word(0x83,0x9702,0x0500);Word(0x83,0x9704,0x0401);
  ItemMapIndex index;
  assert(ItemMap_Build(&index,rom,sizeof(rom),0x91f8) && index.count==3);
  uint8_t bits[64]={0};
  assert(ItemMap_State(&index,0,10,5,bits)==kItemMapRemaining);
  bits[0]=1;assert(ItemMap_State(&index,0,10,5,bits)==kItemMapRemaining);
  bits[0]=3;assert(ItemMap_State(&index,0,10,5,bits)==kItemMapCollected);
  assert(ItemMap_State(&index,0,11,5,bits)==kItemMapRemaining);
  assert(ItemMap_State(&index,1,10,5,bits)==kItemMapNone);
  assert(ItemMap_State(&index,0,-1,5,bits)==kItemMapNone);
  QolMap_Init(rom,sizeof(rom));
  assert(QolMap_ItemAt(0,10,5,false)==kItemMapNone);
  assert(QolMap_ItemAt(0,10,5,true)==kItemMapRemaining);
  mode=kItemSymbolsOff;assert(QolMap_ItemAt(0,10,5,true)==kItemMapNone);mode=kItemSymbolsSuperMetroid;
  item_bit_array[0]=3;assert(QolMap_ItemAt(0,10,5,true)==kItemMapCollected);
  game_state=8;reg_INIDISP=15;area_index=0;
  room_width_in_blocks=room_height_in_blocks=32;
  room_x_coordinate_on_map=9;room_y_coordinate_on_map=4;
  map_tiles_explored[(10>>3)+5*4]=0x20;
  item_bit_array[0]=3;
  PicaFrame frame={0};QolMap_PrepareNative(&frame);
  assert(frame.nativeMapCircle && frame.nativeMapLayer==2);
  assert(frame.nativeMapCircle[(10>>3)+5*4]&0x20);
  mode=kItemSymbolsZeroFusion;QolMap_PrepareNative(&frame);
  assert(!(frame.nativeMapCircle[(10>>3)+5*4]&0x20));
  item_bit_array[0]=1;QolMap_PrepareNative(&frame);
  assert(frame.nativeMapCircle[(10>>3)+5*4]&0x20);
  mode=kItemSymbolsSuperMetroid;QolMap_PrepareNative(&frame);
  assert(!(frame.nativeMapCircle[(10>>3)+5*4]&0x20));
  item_bit_array[0]=3;QolMap_PrepareNative(&frame);
  static Ppu ppu;ppu.vram[0x5820+32+29]=0x6f;
  static uint8_t pixels[256*224*4],original[sizeof(pixels)];
  for(unsigned i=0;i<256*224;i++) {
    pixels[i*4]=50;pixels[i*4+1]=70;pixels[i*4+2]=90;
  }
  for(unsigned y=3;y<=4;y++) for(unsigned x=3;x<=4;x++)
    memset(pixels+((15+y)*256+232+x)*4,240,3);
  memcpy(original,pixels,sizeof(pixels));
  QolMap_ApplyCpu(pixels,256*4,&ppu);
  for(unsigned y=0;y<224;y++) for(unsigned x=0;x<256;x++) {
    unsigned offset=(y*256+x)*4;
    if(x<234 || x>237 || y<17 || y>20)
      assert(!memcmp(pixels+offset,original+offset,4));
  }
  assert(pixels[((15+3)*256+232+3)*4]==50); /* Native fill replaces dot center. */
  assert(pixels[((15+2)*256+232+3)*4]==240); /* Native-colored hollow circle. */
  item_bit_array[0]=0;memcpy(pixels,original,sizeof(pixels));
  QolMap_ApplyCpu(pixels,256*4,&ppu);assert(!memcmp(pixels,original,sizeof(pixels)));
  mode=kItemSymbolsOff;QolMap_PrepareNative(&frame);assert(!frame.nativeMapCircle);
  mode=kItemSymbolsSuperMetroid;game_state=9;QolMap_PrepareNative(&frame);assert(!frame.nativeMapCircle);
  game_state=15;menu_index=0;QolMap_PrepareNative(&frame);assert(frame.nativeMapLayer==0);
  menu_index=1;QolMap_PrepareNative(&frame);assert(!frame.nativeMapCircle);
  assert(NativeMap_CirclePixel(3,3,2)==1 && NativeMap_CirclePixel(3,2,1)==2);
  assert(NativeMap_CirclePixel(0,0,2)==2 && NativeMap_Variant(0x77));
  /* Native unlock flags survive room changes and loading that same save. */
  QolMap_RefreshColors();assert((QolMap_CellStyle(0,10,5)&7)==kMapDoorGreen);
  opened_door_bit_array[0]=1<<3;QolMap_RefreshColors();
  assert((QolMap_CellStyle(0,10,5)&7)==kMapDoorBlue);
  game_state=6;QolMap_RefreshColors();game_state=8;QolMap_RefreshColors();
  assert((QolMap_CellStyle(0,10,5)&7)==kMapDoorBlue);
  QolMap_Init(rom,sizeof(rom));QolMap_RefreshColors();
  assert((QolMap_CellStyle(0,10,5)&7)==kMapDoorBlue);
  /* A different save with the lock still closed restores its original color. */
  opened_door_bit_array[0]=0;QolMap_RefreshColors();
  assert((QolMap_CellStyle(0,10,5)&7)==kMapDoorGreen);
  /* The upper minimap switch leaves lower/pause item styles intact and
   * disables the CPU fallback edit as well as the GPU cell mask. */
  item_bit_array[0]=3;minimapItems=false;memcpy(pixels,original,sizeof(pixels));QolMap_PrepareNative(&frame);assert(!frame.nativeMapCircle);
  QolMap_ApplyCpu(pixels,256*4,&ppu);assert(!memcmp(pixels,original,sizeof(pixels)));
  assert(QolMap_UseCircle(0,10,5,true));
  game_state=15;menu_index=0;QolMap_PrepareNative(&frame);assert(frame.nativeMapCircle);
  minimapItems=true;game_state=8;
  Word(0x8f,0x9203,0xffff);
  assert(!ItemMap_Build(&index,rom,sizeof(rom),0x91f8) && !index.count);
  assert(!ItemMap_Build(&index,rom,100,0x91f8));
  if(argc==2) {
    FILE *f=fopen(argv[1],"rb");assert(f);
    assert(!fseek(f,0,SEEK_END));long size=ftell(f);assert(size>0);rewind(f);
    uint8_t *data=malloc(size);assert(data && fread(data,1,size,f)==(size_t)size);fclose(f);
    assert(ItemMap_Build(&index,data,size,0x91f8) && index.count==100);
    uint8_t seen[64]={0};
    for(unsigned i=0;i<index.count;i++) {
      unsigned bit=index.entries[i].bit;assert(!(seen[bit>>3]&(1u<<(bit&7))));
      seen[bit>>3]|=1u<<(bit&7);
    }
    free(data);puts("Native ROM: 100 unique item locations matched.");
  }
  puts("Item map: room graph, alternate states, hidden items, same-cell pickups, save bits, explored visibility and HUD guards passed.");
}
