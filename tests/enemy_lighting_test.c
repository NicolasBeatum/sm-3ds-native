#include "src/ida_types.h"
#include "enemy_lighting.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
  SceneLighting s={0};
  PicaFrame f={.lighting=&s,.width=400,.height=224,.originX=72,
    .hudEndY=31,.worldLeft=0,.worldRight=400};
  EnemyData enemies[32]={{0}};
  const unsigned ids[]={0xd6bf,0xd1bf,0xd23f,0xdabf,0xdcbf,0xea3f,0xd73f};
  const unsigned banks[]={0xa3,0xa2,0xa2,0xa3,0xa3,0xa8,0xa3};
  for(unsigned i=0;i<7;i++) {
    SceneLighting_Begin(&s,true,0x9ad9,8,1000,1000,1128,1128,72,0);
    enemies[0]=(EnemyData){.enemy_ptr=ids[i],.bank=banks[i],.health=20,
      .x_pos=1120,.y_pos=1120,.spritemap_pointer=0x9000,.ai_var_B=0xe6b7};
    SceneLighting_AddEnemies(&f,enemies,32,1000,1000);
    assert(s.count==2 && s.lights[1].x==192 && s.lights[1].y==(i==6?116:120));
    enemies[0].properties=kEnemyProps_Invisible;
    s.count=1;SceneLighting_AddEnemies(&f,enemies,32,1000,1000);assert(s.count==1);
    enemies[0].properties=kEnemyProps_Deleted;
    SceneLighting_AddEnemies(&f,enemies,32,1000,1000);assert(s.count==1);
    enemies[0].properties=0;enemies[0].frozen_timer=1;
    SceneLighting_AddEnemies(&f,enemies,32,1000,1000);assert(s.count==1);
    enemies[0].frozen_timer=0;enemies[0].health=0;
    SceneLighting_AddEnemies(&f,enemies,32,1000,1000);assert(s.count==1);
    enemies[0].health=20;enemies[0].spritemap_pointer=addr_kSpritemap_Nothing_A0;
    SceneLighting_AddEnemies(&f,enemies,32,1000,1000);assert(s.count==1);
    enemies[0].spritemap_pointer=0x9000;enemies[0].bank^=1;
    SceneLighting_AddEnemies(&f,enemies,32,1000,1000);assert(s.count==1);
  }
  /* The pad's real sprite palette controls its light, independently of Samus. */
  static Ppu memory;
  f.memory=&memory;
  enemies[0]=(EnemyData){.enemy_ptr=0xd73f,.bank=0xa3,.health=40,
    .x_pos=1120,.y_pos=1120,.spritemap_pointer=0x9645,.palette_index=0x0a00};
  memory.cgram[209]=31|(31<<5);memory.cgram[210]=31<<10;
  s.count=1;SceneLighting_AddEnemies(&f,enemies,32,1000,1000);
  assert(s.count==2 && s.lights[1].r==152 && s.lights[1].g==152 && !s.lights[1].b);
  memory.cgram[209]=0;
  s.count=1;SceneLighting_AddEnemies(&f,enemies,32,1000,1000);
  assert(s.count==2 && s.lights[1].b==152 && !s.lights[1].r);
  memory.cgram[210]=0;
  s.count=1;SceneLighting_AddEnemies(&f,enemies,32,1000,1000);
  assert(s.count==1);
  f.memory=NULL;s.count=1;
  enemies[0].enemy_ptr=0xea3f;enemies[0].bank=0xa8;enemies[0].ai_var_B=0xe695;
  SceneLighting_AddEnemies(&f,enemies,32,1000,1000);assert(s.count==1);
  enemies[0].enemy_ptr=0xdcff; /* Ordinary Zoomer, even with a bright palette. */
  SceneLighting_AddEnemies(&f,enemies,32,1000,1000);assert(s.count==1);
  for(unsigned i=0;i<32;i++) enemies[i]=(EnemyData){.enemy_ptr=0xd6bf,
    .bank=0xa3,.health=20,.x_pos=1100+i,.y_pos=1128,.spritemap_pointer=0x9000};
  SceneLighting_AddEnemies(&f,enemies,32,1000,1000);
  assert(s.count==3 && s.lights[1].x==200 && s.lights[2].x==199);
  assert(s.lights[1].glow && !s.lights[2].glow);
  SceneLighting_AddLight(&s,100,100,48,88,62,28);
  SceneLighting_AddEnemies(&f,enemies,32,1000,1000);assert(s.count==4);
  SceneLighting_Begin(&s,true,0x9ad9,8,1000,1000,1128,1128,72,0);
  for(unsigned i=0;i<32;i++) enemies[i].x_pos=2000;
  SceneLighting_AddEnemies(&f,enemies,32,1000,1000);assert(s.count==1);
  enemies[0].x_pos=1128;enemies[0].y_pos=1008; /* HUD, not visible playfield. */
  SceneLighting_AddEnemies(&f,enemies,32,1000,1000);assert(s.count==1);
  f.hudEndY=0; // Newly visible rows participate in the same shared light budget.
  SceneLighting_AddEnemies(&f,enemies,32,1000,1000);assert(s.count==2);
  SceneLighting_Begin(&s,false,0x9ad9,8,1000,1000,1128,1128,72,0);
  SceneLighting_AddEnemies(&f,enemies,32,1000,1000);assert(!s.count);
  puts("Enemy lighting: recognized species, native visibility/state, nearest two, camera and actor budget passed.");
}
