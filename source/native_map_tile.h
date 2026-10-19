#pragma once
#include <stdbool.h>
#include <stdint.h>

enum { NATIVE_MAP_DOT_VARIANTS = 7 };

/* Native dot tiles, with different wall configurations. Other
 * cells (including secrets with no original symbol) keep their original art. */
static inline unsigned NativeMap_Variant(unsigned entry) {
  switch(entry&1023) {
    case 0x6f:return 1;
    case 0x8e:return 2;
    case 0x8f:return 3;
    case 0x5e:return 4;
    case 0x6e:return 5;
    case 0x76:return 6;
    case 0x77:return 7;
    default:return 0;
  }
}
/* Native palette indices: 1 = room fill, 2 = walls/item symbol. Change only
 * the central 4x4 pixels; both flips preserve this symmetric circle. */
static inline unsigned NativeMap_CirclePixel(unsigned x,unsigned y,unsigned original) {
  if(x<2 || x>5 || y<2 || y>5) return original;
  static const uint8_t rows[4]={6,9,9,6};
  return 1+((rows[y-2]>>(5-x))&1);
}

enum NativeMapService { kMapServiceNone, kMapServiceEnergy,
    kMapServiceMissile, kMapServiceMap };
/* Compact versions of the native map capitals (B6:8600 A, B6:8680 E,
 * B6:8780 M), fitted to the save tile's 6x6 lettering. Like native save
 * tile 4D, index 1 is the dark letter and index 2 the light plaque. */
static inline unsigned NativeMap_ServicePixel(unsigned kind,unsigned x,unsigned y,unsigned original) {
  if(!kind || kind>3 || x<1 || x>6 || y<1 || y>6) return original;
  static const uint8_t glyphs[3][6]={
    {63,48,63,48,48,63}, /* E: energy */
    {30,51,51,63,51,51}, /* A: ammo */
    {51,63,63,51,51,51}  /* M: map */
  };
  return ((glyphs[kind-1][y-1]>>(6-x))&1)?1:2;
}
