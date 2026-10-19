#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum ItemMapState { kItemMapNone, kItemMapRemaining, kItemMapCollected };
enum { ITEM_MAP_CAPACITY=128 };
typedef struct ItemMapEntry { uint16_t cell, bit; } ItemMapEntry;
typedef struct ItemMapIndex {
  ItemMapEntry entries[ITEM_MAP_CAPACITY];
  unsigned count;
  bool valid;
} ItemMapIndex;

/* Read-only traversal of the native room/door/state/PLM tables. */
bool ItemMap_Build(ItemMapIndex *index,const uint8_t *rom,size_t size,uint16_t root);
typedef void ItemMapVisitState(void *context,const uint8_t *rom,unsigned room,unsigned state);
bool ItemMap_BuildWithVisitor(ItemMapIndex *index,const uint8_t *rom,size_t size,
    uint16_t root,ItemMapVisitState *visit,void *context);
enum ItemMapState ItemMap_State(const ItemMapIndex *index,unsigned area,int x,int y,
                               const uint8_t collected[64]);
