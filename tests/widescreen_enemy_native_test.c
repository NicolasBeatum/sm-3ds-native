/* Native enemy behavior with synthetic RAM/ROM fixtures, no bundled ROM. */
#include "src/ida_types.h"
#include "src/variables.h"
#include "src/funcs.h"
#include "src/sm_rtl.h"
#include "src/enemy_types.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

uint8 g_ram[0x20000];
uint8 g_sprite_viewport_margin;
static uint8 rom[0x400000];
const uint8 *g_rom = rom;
const uint8 *RomPtr(uint32 a) {
  return g_rom + (((a >> 1) & 0x3f8000) | (a & 0x7fff));
}
bool Unreachable(void) { abort(); }
uint16 NextRandom(void) { return ++random_number; }
void QueueSfx2_Max6(uint16 a) { (void)a; }
static unsigned movementCalls;
static bool solidWalls;
uint8 Enemy_MoveRight_IgnoreSlopes(uint16 k, int32 amount) {
  movementCalls++;
  if (solidWalls) return 1;
  EnemyData *e = gEnemyData(k);
  AddToHiLo(&e->x_pos, &e->x_subpos, amount);
  return 0;
}
uint8 Enemy_MoveDown(uint16 k, int32 amount) {
  movementCalls++;
  if (solidWalls) return 1;
  EnemyData *e = gEnemyData(k);
  AddToHiLo(&e->y_pos, &e->y_subpos, amount);
  return 0;
}

static void SetWord(uint32 address, uint16 value) {
  uint8 *p = (uint8 *)RomPtr(address);
  p[0] = value; p[1] = value >> 8;
}
static void Fixture(int screenX) {
  memset(g_ram, 0, sizeof(g_ram));
  memset(rom, 0, sizeof(rom));
  movementCalls = 0;
  solidWalls = true;
  layer1_x_pos = 512;
  layer1_y_pos = 0;
  samus_x_pos = 640;
  samus_y_pos = 128;
  room_width_in_blocks = 64;
  room_height_in_blocks = 16;
  cur_enemy_index = 0;
  EnemyData *e = gEnemyData(0);
  e->enemy_ptr = 0xe07f;
  e->bank = 0xa2;
  e->x_pos = layer1_x_pos + screenX;
  e->y_pos = 128;
  e->x_width = e->y_height = 8;
  e->spritemap_pointer = 0x9000;
  // The artwork extends farther than the collision radius: -24..24.
  SetWord(0xa29000, 2);
  SetWord(0xa29002, 0x8000 | (uint16)(-24 & 511));
  SetWord(0xa29007, 0x8000 | 8);
  for (unsigned i = 0; i < 64 * 16; i++) level_data[i] = 0x8000;
  SetWord(0xa28187, 0xffff);     // native left speed -1.0
  SetWord(0xa28187 + 4, 1);     // native right speed +1.0
  SetWord(0xa2838f + 14, 1);    // native upward movement fixture
}
static bool Active(void) {
  DetermineWhichEnemiesToProcess();
  cur_enemy_index = 0;
  return active_enemy_indexes[0] == 0;
}
static void CheckNativeOff(void) {
  g_sprite_viewport_margin = 0;
  for (int x = -150; x < 450; x++) {
    Fixture(x);
    bool expected = x + 8 >= 0 && 8 + 256 - x >= 0;
    assert(Active() == expected);
    assert(!!EnemyWithNormalSpritesIsOffScreen() == !expected);
    assert(!!CheckIfEnemyIsOnScreen() == (x < 0 || x > 256));
    assert(!!IsEnemyLeavingScreen(0) == !(x + 8 >= 0 && x < 256));
  }
}
static void CheckVisibleEdges(void) {
  g_sprite_viewport_margin = 72;
  const int x[] = {-90, -40, 290, 340};
  for (unsigned i = 0; i < 4; i++) {
    Fixture(x[i]);
    assert(Active());
    assert(!EnemyWithNormalSpritesIsOffScreen());
    assert(!CheckIfEnemyIsOnScreen());
    assert(!IsEnemyLeavingScreen(0));
  }
  Fixture(-96); assert(!Active()); assert(IsEnemyLeavingScreen(0));
  Fixture(352); assert(!Active()); assert(IsEnemyLeavingScreen(0));
  // Empty/invalid maps must not keep an invisible off-screen enemy active.
  Fixture(340); SetWord(0xa29000, 0); assert(!Active());
  Fixture(340); SetWord(0xa29000, 0xffff); assert(!Active());
  // An extended map can also have a visible component beyond its hitbox.
  Fixture(340);
  gEnemyData(0)->extra_properties = 4;
  gEnemyData(0)->spritemap_pointer = 0x9100;
  SetWord(0xa29100, 1);
  SetWord(0xa29102, (uint16)-8);
  SetWord(0xa29106, 0x9000);
  assert(Active());
}
static void CheckReo(void) {
  const int x[] = {-60, 310};
  for (unsigned i = 0; i < 2; i++) {
    Fixture(x[i]); g_sprite_viewport_margin = 72;
    assert(Active());
    Rio_Init();
    SetWord(0xa2bbbb, 0x100);
    SetWord(0xa2bbbf, 0x100);
    Rio_1(0);
    assert(Get_Rio(0)->rio_var_B == FUNC16(Rio_3));
    solidWalls = false;
    uint16 before = gEnemyData(0)->x_pos;
    Rio_3(0);
    assert(gEnemyData(0)->x_pos != before);
    Fixture(x[i]); g_sprite_viewport_margin = 0;
    Rio_Init(); Rio_1(0);
    assert(Get_Rio(0)->rio_var_B == FUNC16(Rio_1));
  }
  // Within the original center, keep the original 160-pixel trigger.
  Fixture(250); g_sprite_viewport_margin = 72; samus_x_pos = layer1_x_pos + 50;
  Rio_Init(); Rio_1(0);
  assert(Get_Rio(0)->rio_var_B == FUNC16(Rio_1));
}
static void CheckDessgeega(void) {
  const int x[] = {-90, 340};
  for (unsigned i = 0; i < 2; i++) {
    Fixture(x[i]); g_sprite_viewport_margin = 72;
    assert(Active());
    Enemy_Sidehopper *e = Get_Sidehopper(0);
    e->sideh_var_B = FUNC16(Sidehopper_Func_14);
    e->sideh_var_D = 1; e->sideh_var_E = 1; e->sideh_var_C = 1;
    solidWalls = false;
    uint16 before = e->base.x_pos;
    Sidehopper_Main();
    assert(e->base.x_pos != before);
    assert(movementCalls >= 2);
  }
}
static void CheckPipes(void) {
  for (unsigned margin = 0; margin <= 72; margin += 72) {
    g_sprite_viewport_margin = margin;
    Fixture(128);
    Enemy_PipeBug *e = Get_PipeBug(0);
    e->pbg_var_B = e->base.x_pos; e->pbg_var_C = e->base.y_pos;
    e->pbg_var_F = FUNC16(BrinstarPipeBug_PreInstr_4);
    e->pbg_var_A = 0;
    for (unsigned i = 0; i < 30; i++) BrinstarPipeBug_PreInstr_4(0);
    assert(e->base.x_pos == layer1_x_pos + 188 && !movementCalls);
    e->pbg_var_A = 0x8000;
    BrinstarPipeBug_PreInstr_4(0);
    assert(e->base.x_pos == layer1_x_pos + 186);
    e->base.x_pos = layer1_x_pos + 360;
    e->pbg_var_A = 0;
    BrinstarPipeBug_PreInstr_4(0);
    assert(e->base.properties & kEnemyProps_Invisible);
    assert(e->base.x_pos == e->pbg_var_B);

    Fixture(128); e = Get_PipeBug(0);
    e->pbg_var_03 = 1; e->pbg_var_01 = 0xffff;
    e->pbg_var_07 = e->base.x_pos; e->pbg_var_08 = e->base.y_pos;
    e->pbg_var_06 = 1;
    BrinstarYellowPipeBug_Func_3();
    assert(e->base.x_pos == layer1_x_pos + 129 && !movementCalls);
    BrinstarYellowPipeBug_Func_5();
    assert(e->base.x_pos == layer1_x_pos + 128 && !movementCalls);
    e->base.x_pos = layer1_x_pos + 360;
    BrinstarYellowPipeBug_Func_3();
    assert(e->base.properties & kEnemyProps_Invisible);
    assert(e->base.x_pos == e->pbg_var_07);

    Fixture(128); e = Get_PipeBug(0);
    e->pbg_var_D = e->base.x_pos; e->pbg_var_E = e->base.y_pos;
    NorfairPipeBug_Func_10();
    assert(e->base.x_pos == layer1_x_pos + 129 && !movementCalls);
    NorfairPipeBug_Func_11();
    assert(e->base.x_pos == layer1_x_pos + 128 && !movementCalls);
    e->base.x_pos = layer1_x_pos + 360;
    NorfairPipeBug_8BA8();
    assert(e->base.properties & kEnemyProps_Invisible);
    assert(e->base.x_pos == e->pbg_var_D);
  }
  // Crossing the original edge stays visible/moving only in widescreen.
  Fixture(256); g_sprite_viewport_margin = 72;
  Enemy_PipeBug *e = Get_PipeBug(0);
  e->pbg_var_B = layer1_x_pos + 128; e->pbg_var_C = 128;
  BrinstarPipeBug_PreInstr_4(0);
  assert(e->base.x_pos == layer1_x_pos + 258);
  assert(!(e->base.properties & kEnemyProps_Invisible));
  Fixture(351); g_sprite_viewport_margin = 72;
  e = Get_PipeBug(0);
  e->pbg_var_07 = layer1_x_pos + 128;
  e->pbg_var_08 = 128;
  e->pbg_var_03 = 1;
  e->pbg_var_A = FUNC16(BrinstarYellowPipeBug_Func_7);
  e->pbg_var_E = 40;
  e->pbg_var_F = 1;
  BrinstarYellowPipeBug_Main();
  assert(e->base.properties & kEnemyProps_Invisible);
  assert(e->base.x_pos == e->pbg_var_07);
}
int main(void) {
  CheckNativeOff();
  CheckVisibleEdges();
  CheckReo();
  CheckDessgeega();
  CheckPipes();
  puts("Native enemy tests pass: visible edges, Reo, Dessgeega and three pipe variants.");
  return 0;
}
