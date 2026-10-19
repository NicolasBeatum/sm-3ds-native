#pragma once
#include "item_map.h"
#include "map_features.h"
#include "ppu_gpu_model.h"

void QolMap_Init(const uint8_t *rom,size_t size);
enum ItemMapState QolMap_ItemAt(unsigned area,int x,int y,bool explored);
bool QolMap_UseCircle(unsigned area,int x,int y,bool explored);
void QolMap_PrepareNative(PicaFrame *frame);
/* Same native-dot replacement for the CPU presentation buffer. */
void QolMap_ApplyCpu(uint8_t *pixels,size_t pitch,const Ppu *ppu);

void QolMap_RefreshColors(void);
uint16_t QolMap_CellStyle(unsigned area,int x,int y);


unsigned QolMap_ServiceAt(unsigned area,int x,int y,bool explored);
