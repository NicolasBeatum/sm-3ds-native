#include "src/ida_types.h"
#include "enemy_lighting.h"

void SceneLighting_AddEnemies(PicaFrame *f, const EnemyData *enemies,
                             unsigned count, int cameraX, int cameraY) {
  SceneLighting *s=f->lighting;
  if(!s || !s->enabled || !enemies) return;
  /* Keep room sources' budget: two small actor lights at most, within the
   * existing four actor slots (six at Landing Site, including ship lamps).
   * Recognition uses native enemy definitions, never sprite brightness. */
  unsigned budget=s->ship?6:4;
  if(s->count>=budget) return;
  SceneLight chosen[2]={{0}};
  int distance[2]={0x7fffffff,0x7fffffff};
  for(unsigned i=0;i<count && i<32;i++) {
    const EnemyData *e=&enemies[i];
    if(!e->health || e->frozen_timer ||
        (e->properties&(kEnemyProps_Invisible|kEnemyProps_Deleted)) ||
        e->spritemap_pointer<0x8000 ||
        e->spritemap_pointer==addr_kSpritemap_Nothing_A0 ||
        e->spritemap_pointer==addr_kExtendedSpritemap_Nothing_A0) continue;
    unsigned radius=48,r=112,g=52,b=16,bank;
    switch(e->enemy_ptr) {
      case 0xd6bf: /* Fireflea: death also darkens the original room. */
        bank=0xa3;radius=64;r=112;g=96;b=40;break;
      case 0xd1bf: /* Norfair erratic fireball. */
      case 0xd23f: /* Rinka energy ring. */
        bank=0xa2;break;
      case 0xdabf: /* Norfair slow fireball. */
      case 0xdcbf: /* Fire Zoomer / Sova. Ordinary Zoomers do not emit. */
        bank=0xa3;break;
      case 0xd73f: /* Live elevator pad: the yellow strip is a lamp. */
        bank=0xa3;radius=72;r=128;g=124;b=42;
        if(f->memory) {
          unsigned palette=128+((e->palette_index>>9)&7)*16,best=0;
          for(unsigned color=1;color<16;color++) {
            unsigned rgb=f->memory->cgram[palette+color];
            unsigned cr=rgb&31,cg=(rgb>>5)&31,cb=(rgb>>10)&31;
            unsigned high=cr>cg?cr:cg;high=high>cb?high:cb;
            unsigned low=cr<cg?cr:cg;low=low<cb?low:cb;
            unsigned luma=cr*3+cg*6+cb;
            if(high-low>=5 && luma>best) {
              best=luma;r=cr*152/31;g=cg*152/31;b=cb*152/31;
            }
          }
          if(!best) continue; /* No ghost light while the pad palette is dark. */
        }
        break;
      case 0xea3f: /* Wrecked Ship spark, only during its active interval. */
        if(e->ai_var_B!=0xe6b7) continue;
        bank=0xa8;radius=40;r=64;g=100;b=128;break;
      default:continue;
    }
    if(e->bank!=bank) continue;
    int x=(int16_t)(e->x_pos-cameraX)+(int)f->originX;
    int y=(int16_t)(e->y_pos-cameraY);
    if(e->enemy_ptr==0xd73f) y-=4; /* Place it on the pad's upper strip. */
    if(x<f->worldLeft || x>=f->worldRight ||
        y<(int)f->hudEndY || y>=(int)f->height) continue;
    int dx=x-s->samusX,dy=y-s->samusY,d=dx*dx+dy*dy;
    unsigned place=d<distance[0]?0:1;
    if(d>=distance[place]) continue;
    if(!place) {chosen[1]=chosen[0];distance[1]=distance[0];}
    chosen[place]=(SceneLight){x,y,radius,r,g,b,false,false};
    distance[place]=d;
  }
  for(unsigned i=0;i<2 && distance[i]!=0x7fffffff && s->count<budget;i++) {
    const SceneLight *l=&chosen[i];
    SceneLighting_AddLight(s,l->x,l->y,l->radius,l->r,l->g,l->b);
    s->lights[s->count-1].glow=i==0; /* One small halo, second diffuse only. */
  }
}
