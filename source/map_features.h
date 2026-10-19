#pragma once
#include "item_map.h"
#include "native_map_tile.h"

enum MapDoorColor { kMapDoorNone,kMapDoorBlue,kMapDoorRed,kMapDoorGreen,
    kMapDoorYellow,kMapDoorGrey };
enum MapRoomColor { kMapRoomHeat=0x1000,kMapRoomLava=0x2000,
    kMapRoomAcid=0x4000,kMapRoomSecret=0x8000 };
typedef struct MapDoor { uint16_t key,bit;uint8_t color; } MapDoor;
typedef struct MapProgress {
  uint8_t events[8],bosses[8];
  uint16_t items;
  uint8_t missiles,powerBombs;
} MapProgress;
typedef struct MapService { uint16_t cell;uint8_t kind; } MapService;
typedef struct MapFeatures {
  MapService services[64];
  unsigned serviceCount;
  uint16_t doors[6][2048];
  uint8_t hazards[6][2048];
  MapDoor entries[768];
  unsigned count;
  uint16_t rooms[320];
  unsigned roomCount;
  MapProgress progress;
  uint8_t opened[64];
  bool valid,refreshed,hazardsRefreshed;
} MapFeatures;

/* Shares the item catalog's bounded, read-only native room traversal. */
bool MapFeatures_Build(MapFeatures *map,ItemMapIndex *items,
    const uint8_t *rom,size_t size,uint16_t root);
void MapFeatures_RefreshDoors(MapFeatures *map,const uint8_t opened[64]);
/* Room conditions follow current progress, excluding inactive escape FX. */
void MapFeatures_RefreshHazards(MapFeatures *map,const uint8_t *rom,
                               const MapProgress *progress);
uint16_t MapFeatures_At(const MapFeatures *map,unsigned area,int x,int y);
uint32_t MapFeatures_RoomPixel(uint16_t style,bool roomFill,uint32_t original);
typedef struct MapDoorStroke { int x,y,w,h;uint32_t color; } MapDoorStroke;
/* One final LCD pixel per face, inside its own cell. Opposite caps may differ. */
bool MapFeatures_Divider(uint16_t style,unsigned edge,int x,int y,int w,int h,
    MapDoorStroke *stroke);

unsigned MapFeatures_ServiceAt(const MapFeatures *map,unsigned area,int x,int y);
