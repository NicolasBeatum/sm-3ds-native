#include "qol_map.h"
#include "bottom_screen.h"
#include "src/variables.h"
#include "src/ida_types.h"
#include "native_map_tile.h"

static ItemMapIndex g_items;
static MapFeatures g_features;
static const uint8_t *g_map_rom;
static uint8_t circleCells[256],oldItems[64],oldExplored[256];
static unsigned oldArea=~0u,oldMode=~0u;
void QolMap_Init(const uint8_t *rom,size_t size) {
  g_map_rom=rom;
  MapFeatures_Build(&g_features,&g_items,rom,size,0x91f8);
  oldArea=~0u;
}
enum ItemMapState QolMap_ItemAt(unsigned area,int x,int y,bool explored) {
  if(!explored || BottomScreen_ItemSymbolMode()==kItemSymbolsOff) return kItemMapNone;
  return ItemMap_State(&g_items,area,x,y,item_bit_array);
}
bool QolMap_UseCircle(unsigned area,int x,int y,bool explored) {
  enum ItemMapState state=QolMap_ItemAt(area,x,y,explored);
  enum ItemSymbolMode mode=BottomScreen_ItemSymbolMode();
  return mode==kItemSymbolsZeroFusion ? state==kItemMapRemaining :
      mode==kItemSymbolsSuperMetroid && state==kItemMapCollected;
}
static const uint8_t *CircleCells(void) {
  unsigned mode=BottomScreen_ItemSymbolMode();
  if(BottomScreen_ItemSymbolMode()==kItemSymbolsOff || !g_items.valid || area_index>=6)
    return NULL;
  if(oldArea!=area_index || oldMode!=mode || memcmp(oldItems,item_bit_array,64) ||
      memcmp(oldExplored,map_tiles_explored,256)) {
    memset(circleCells,0,sizeof(circleCells));
    for(unsigned i=0;i<g_items.count;i++) {
      const ItemMapEntry *e=&g_items.entries[i];
      if(e->cell/2048!=area_index) continue;
      unsigned x=e->cell%64,y=(e->cell%2048)/64;
      if(!QolMap_UseCircle(area_index,x,y,true)) continue;
      unsigned byte=((x>>3)&3)+(x>>5)*128+y*4;
      if(map_tiles_explored[byte]&(0x80>>(x&7))) circleCells[byte]|=0x80>>(x&7);
    }
    memcpy(oldItems,item_bit_array,64);memcpy(oldExplored,map_tiles_explored,256);
    oldArea=area_index;oldMode=mode;
  }
  return circleCells;
}

unsigned QolMap_ServiceAt(unsigned area,int x,int y,bool explored) {
  return explored && BottomScreen_MapStationsEnabled() ?
      MapFeatures_ServiceAt(&g_features,area,x,y):0;
}
void QolMap_PrepareNative(PicaFrame *f) {
  f->nativeMapCircle=NULL;
  bool pause=game_state==15 && menu_index==0;
  if(!pause && (game_state!=8 || debug_disable_minimap ||
      (samus_x_pos>>4)>=room_width_in_blocks ||
      (samus_y_pos>>4)>=room_height_in_blocks)) return;
  f->nativeMapCircle=(pause || BottomScreen_MinimapItemsEnabled())?CircleCells():NULL;
  f->nativeMapLayer=pause?0:2;
  f->nativeMapX=room_x_coordinate_on_map+(samus_x_pos>>8);
  f->nativeMapY=room_y_coordinate_on_map+(samus_y_pos>>8)+1;
}

/* Verify that the displayed cell still contains the native dot before
 * editing its 4x4 center. UI windows/sprites covering the map stay untouched.
 * Colors are sampled from the rendered tile, preserving palettes and fades. */
static void CpuCircle(uint8_t *pixels,size_t pitch,int x,int y) {
  if(x+2<0 || x+5>=256 || y+2<0 || y+5>=224) return;
  uint8_t bg[3],fg[3];
  memcpy(bg,pixels+(y+2)*pitch+(x+2)*4,3);
  memcpy(fg,pixels+(y+3)*pitch+(x+3)*4,3);
  if(!memcmp(bg,fg,3)) return;
  for(unsigned yy=2;yy<=5;yy++) for(unsigned xx=2;xx<=5;xx++) {
    bool dot=xx>=3 && xx<=4 && yy>=3 && yy<=4;
    if(memcmp(pixels+(y+yy)*pitch+(x+xx)*4,dot?fg:bg,3)) return;
  }
  for(unsigned yy=2;yy<=5;yy++) for(unsigned xx=2;xx<=5;xx++)
    memcpy(pixels+(y+yy)*pitch+(x+xx)*4,
           NativeMap_CirclePixel(xx,yy,0)==2?fg:bg,3);
}

void QolMap_ApplyCpu(uint8_t *pixels,size_t pitch,const Ppu *p) {
  PicaFrame f={0};QolMap_PrepareNative(&f);
  if(!pixels || !f.nativeMapCircle) return;
  if(f.nativeMapLayer==2) {
    for(int row=0;row<3;row++) for(int col=0;col<5;col++) {
      int x=f.nativeMapX+col-2,y=f.nativeMapY+row-1;
      if((unsigned)x>=64 || (unsigned)y>=32) continue;
      unsigned byte=((x>>3)&3)+(x>>5)*128+y*4;
      unsigned tile=p->vram[0x5820+row*32+26+col];
      if((circleCells[byte]&(0x80>>(x&7))) && NativeMap_Variant(tile))
        CpuCircle(pixels,pitch,208+col*8-(reg_BG3HOFS&255),
                   (row+1)*8-(reg_BG3VOFS&255)-1);
    }
  } else if(p->bgLayer[0].tilemapAdr==0x3000) {
    for(unsigned i=0;i<g_items.count;i++) {
      const ItemMapEntry *e=&g_items.entries[i];
      if(e->cell/2048!=area_index) continue;
      unsigned x=e->cell%64,y=e->cell%2048/64;
      unsigned byte=((x>>3)&3)+(x>>5)*128+y*4;
      unsigned tile=p->vram[0x3000+(x>>5)*1024+y*32+(x&31)];
      if(!(circleCells[byte]&(0x80>>(x&7))) || !NativeMap_Variant(tile)) continue;
      int px=(x*8-reg_BG1HOFS)&511,py=(y*8-reg_BG1VOFS-1)&255;
      if(px>504) px-=512;
      if(py>248) py-=256;
      CpuCircle(pixels,pitch,px,py);
    }
  }
}

void QolMap_RefreshColors(void) {
  MapFeatures_RefreshDoors(&g_features,opened_door_bit_array);
  MapProgress progress={0};
  memcpy(progress.events,events_that_happened,sizeof(progress.events));
  memcpy(progress.bosses,boss_bits_for_area,sizeof(progress.bosses));
  progress.items=collected_items;
  progress.missiles=samus_max_missiles!=0;
  progress.powerBombs=samus_max_power_bombs!=0;
  MapFeatures_RefreshHazards(&g_features,g_map_rom,&progress);
}
uint16_t QolMap_CellStyle(unsigned area,int x,int y) {
  return MapFeatures_At(&g_features,area,x,y);
}
