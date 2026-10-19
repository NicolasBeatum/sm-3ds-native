#include "map_features.h"
#include <string.h>

static unsigned Word(const uint8_t *rom,unsigned bank,unsigned address) {
  unsigned p=((bank&127)<<15)|(address&32767);
  return rom[p]|(rom[p+1]<<8);
}
static void Door(MapFeatures *m,unsigned area,unsigned mx,unsigned my,
                 unsigned bx,unsigned by,unsigned orientation,unsigned color,unsigned bit) {
  unsigned x=mx+bx/16,y=my+by/16+1;
  static const uint8_t edge[]={1,0,3,2}; /* Cap faces left/right/up/down; wall is opposite. */
  if(area>=6 || x>=64 || y>=32 || orientation>=4) return;
  unsigned key=(area*2048+y*64+x)*4+edge[orientation];
  for(unsigned i=0;i<m->count;i++) if(m->entries[i].key==key) {
    /* Room PLMs override the blue cap spawned by normal door transitions. */
    if(color!=kMapDoorBlue) m->entries[i]=(MapDoor){key,bit,color};
    return;
  }
  if(m->count==768) {m->valid=false;return;}
  m->entries[m->count++]=(MapDoor){key,bit,color};
}
static unsigned Hazards(const uint8_t *rom,unsigned area,unsigned state) {
  unsigned fx=Word(rom,0x8f,state+6),flags=0;
  if(!fx || fx<0x8000) return 0;
  for(unsigned i=0;i<32 && fx<=0xfff0;i++,fx+=16) {
    unsigned door=Word(rom,0x83,fx);
    if(door==0xffff) break;
    unsigned type=Word(rom,0x83,fx+8)>>8;
    if(type==2) flags|=kMapRoomLava;
    if(type==4) flags|=kMapRoomAcid;
    unsigned bits=Word(rom,0x83,fx+12)>>8;
    unsigned list=Word(rom,0x83,0xac46+area*2);
    if(list>=0x8000 && list<=0xfff0) for(unsigned j=0;j<8;j++) if(bits&(1u<<j)) {
      unsigned def=Word(rom,0x83,list+j*2);
      /* PalInit_F761_Norfair1 installs the original Samus heat-damage FX. */
      if(def>=0x8000 && def<=0xfffc && Word(rom,0x8d,def)==0xe440)
        flags|=kMapRoomHeat;
    }
    if(!door) break;
  }
  return flags;
}
static void Visit(void *context,const uint8_t *rom,unsigned room,unsigned state) {
  MapFeatures *m=context;
  unsigned area=Word(rom,0x8f,room+1)&255;
  if(area>=6) return;
  unsigned xy=Word(rom,0x8f,room+2),wh=Word(rom,0x8f,room+4);
  unsigned mx=xy&255,my=xy>>8,w=wh&255,h=wh>>8;
  if(!m->roomCount || m->rooms[m->roomCount-1]!=room) {
    if(m->roomCount==320) {m->valid=false;return;}
    m->rooms[m->roomCount++]=room;
  }
  unsigned list=Word(rom,0x8f,room+9);
  for(unsigned i=0;i<64 && list<=0xfffe;i++,list+=2) {
    unsigned ptr=Word(rom,0x8f,list);
    if(ptr<0x8000 || ptr>0xfff4) break;
    unsigned dest=Word(rom,0x83,ptr);
    if(dest<0x8000 || dest>=0xe5d2-13) break;
    unsigned flags=Word(rom,0x83,ptr+2),pos=Word(rom,0x83,ptr+4);
    /* Elevators have no colored cap. Direction bit 2 is a transition flag. */
    if((flags&0x80) || (flags>>8)>7) continue;
    unsigned cap=Word(rom,0x8f,0xe68a+((flags>>8)*2));
    unsigned orientation=cap==0xc8ba?0:cap==0xc8be?1:cap==0xc8c2?2:cap==0xc8c6?3:4;
    if(orientation==4) continue;
    unsigned da=Word(rom,0x8f,dest+1)&255,dxy=Word(rom,0x8f,dest+2);
    unsigned dwh=Word(rom,0x8f,dest+4);
    if((pos&255)>=(dwh&255)*16 || (pos>>8)>=(dwh>>8)*16) continue;
    Door(m,da,dxy&255,dxy>>8,pos&255,pos>>8,orientation,kMapDoorBlue,0xffff);
  }
  list=Word(rom,0x8f,state+20);
  if(list<0x8000) return;
  for(unsigned i=0;i<128 && list<=0xfffa;i++,list+=6) {
    unsigned plm=Word(rom,0x8f,list);
    if(!plm) break;
    if(plm<0xc842 || plm>0xc8b4 || (plm-0xc842)%6) continue;
    unsigned kind=(plm-0xc842)/6,group=kind/4;
    static const uint8_t color[]={kMapDoorGrey,kMapDoorGreen,kMapDoorRed,
        kMapDoorYellow,kMapDoorBlue};
    unsigned pos=Word(rom,0x8f,list+2),bit=Word(rom,0x8f,list+4);
    if((pos&255)>=w*16 || (pos>>8)>=h*16) continue;
    if(group==0) bit&=~0x7c00;
    if(bit>=512) bit=0xffff;
    Door(m,area,mx,my,pos&255,pos>>8,kind&3,color[group],bit);
  }
}
/* Reuse the native pause-map station lists, rather than guessing from dot
 * graphics or traversing access PLMs that do not represent real stations. */
static void Services(MapFeatures *m,const uint8_t *rom) {
  for(unsigned kind=1;kind<=3;kind++) for(unsigned area=0;area<6;area++) {
    unsigned p=Word(rom,0x82,0xc7cb+kind*16+area*2);
    for(unsigned i=0;i<32 && p>=0x8000 && p<=0xfffc;i++,p+=4) {
      unsigned x=Word(rom,0x82,p),y=Word(rom,0x82,p+2);
      if(x&0x8000) break;
      if(x>=512 || y>=256) continue;
      unsigned cell=area*2048+(y/8)*64+x/8;
      bool found=false;
      for(unsigned j=0;j<m->serviceCount;j++) if(m->services[j].cell==cell) {found=true;break;}
      if(found) continue;
      if(m->serviceCount==64) {m->valid=false;return;}
      unsigned j=m->serviceCount++;
      while(j && m->services[j-1].cell>cell) {m->services[j]=m->services[j-1];j--;}
      m->services[j]=(MapService){cell,kind};
    }
  }
}
unsigned MapFeatures_ServiceAt(const MapFeatures *m,unsigned area,int x,int y) {
  if(!m->valid || area>=6 || (unsigned)x>=64 || (unsigned)y>=32) return 0;
  unsigned cell=area*2048+y*64+x,lo=0,hi=m->serviceCount;
  while(lo<hi) {unsigned mid=(lo+hi)/2;if(m->services[mid].cell<cell)lo=mid+1;else hi=mid;}
  return lo<m->serviceCount && m->services[lo].cell==cell ? m->services[lo].kind:0;
}
bool MapFeatures_Build(MapFeatures *m,ItemMapIndex *items,
    const uint8_t *rom,size_t size,uint16_t root) {
  memset(m,0,sizeof(*m));m->valid=true;
  bool ok=ItemMap_BuildWithVisitor(items,rom,size,root,Visit,m);
  m->valid&=ok;
  if(ok) Services(m,rom);
  MapProgress initial={0};
  MapFeatures_RefreshHazards(m,rom,&initial);
  return ok;
}
static unsigned ActiveState(const uint8_t *rom,unsigned room,
                            const MapProgress *p) {
  unsigned area=Word(rom,0x8f,room+1)&255,address=room+11;
  if(area>=6) return 0;
  for(unsigned i=0;i<16 && address<=0xfffa;i++) {
    unsigned code=Word(rom,0x8f,address);address+=2;
    if(code==0xe5e6) return address;
    bool selected=false;
    if(code==0xe5ff) selected=(p->bosses[area]&1)!=0;
    else if(code==0xe652) selected=(p->items&4) && p->missiles;
    else if(code==0xe669) selected=p->powerBombs;
    else if(code==0xe612 || code==0xe629) {
      unsigned bit=Word(rom,0x8f,address)&255;address++;
      selected=code==0xe612 ? bit<64 && (p->events[bit>>3]&(1u<<(bit&7))) :
          (p->bosses[area]&bit)!=0;
    } else return 0;
    unsigned state=Word(rom,0x8f,address);address+=2;
    if(selected) return state>=0x8000 && state<=0xffe6?state:0;
  }
  return 0;
}
void MapFeatures_RefreshHazards(MapFeatures *m,const uint8_t *rom,
                               const MapProgress *progress) {
  if(!m->valid || !rom || !progress || (m->hazardsRefreshed &&
      !memcmp(&m->progress,progress,sizeof(*progress)))) return;
  memset(m->hazards,0,sizeof(m->hazards));
  for(unsigned i=0;i<m->roomCount;i++) {
    unsigned room=m->rooms[i],state=ActiveState(rom,room,progress);
    if(!state) continue;
    unsigned area=Word(rom,0x8f,room+1)&255;
    unsigned xy=Word(rom,0x8f,room+2),wh=Word(rom,0x8f,room+4);
    unsigned mx=xy&255,my=xy>>8,w=wh&255,h=wh>>8;
    unsigned hazards=Hazards(rom,area,state)>>12;
    for(unsigned y=my+1;y<my+1+h && y<32;y++)
      for(unsigned x=mx;x<mx+w && x<64;x++) m->hazards[area][y*64+x]|=hazards;
  }
  m->progress=*progress;m->hazardsRefreshed=true;
}
static void SetEdge(MapFeatures *m,unsigned area,int x,int y,unsigned edge,
    unsigned color,bool force) {
  if(area>=6 || (unsigned)x>=64 || (unsigned)y>=32) return;
  uint16_t *v=&m->doors[area][y*64+x];
  unsigned shift=edge*3,old=(*v>>shift)&7;
  if(force || color>old) *v=(*v&~(7u<<shift))|(color<<shift);
}
void MapFeatures_RefreshDoors(MapFeatures *m,const uint8_t opened[64]) {
  if(!m->valid || !opened || (m->refreshed && !memcmp(m->opened,opened,64))) return;
  memset(m->doors,0,sizeof(m->doors));
  /* Native caps are directional: a blue reverse face does not inherit the
   * lock of a red/green/yellow face, and independent locks keep their bits. */
  for(unsigned i=0;i<m->count;i++) {
    const MapDoor *d=&m->entries[i];
    bool unlocked=d->color!=kMapDoorBlue && d->bit<512 &&
        (opened[d->bit>>3]&(1u<<(d->bit&7)));
    unsigned cell=d->key/4;
    SetEdge(m,cell/2048,cell%64,cell%2048/64,d->key&3,
            unlocked?kMapDoorBlue:d->color,true);
  }
  memcpy(m->opened,opened,64);m->refreshed=true;
}
uint16_t MapFeatures_At(const MapFeatures *m,unsigned area,int x,int y) {
  if(!m->valid || area>=6 || (unsigned)x>=64 || (unsigned)y>=32) return 0;
  unsigned cell=y*64+x;
  return m->doors[area][cell]|(m->hazards[area][cell]<<12);
}
uint32_t MapFeatures_RoomPixel(uint16_t style,bool roomFill,uint32_t original) {
  if(!style) return original;
  /* Color just the existing fill; room outlines and item icons remain crisp. */
  if(roomFill) {
    if(style&kMapRoomHeat) original=0xffa85a18;
    else if(style&kMapRoomAcid) original=0xff974caa;
    else if(style&kMapRoomLava) original=0xffa53e2c;
    else if(style&kMapRoomSecret) original=0xff28765a;
  }
  return original;
}
bool MapFeatures_Divider(uint16_t style,unsigned edge,int x,int y,int w,int h,
    MapDoorStroke *stroke) {
  static const uint32_t colors[]={0,0xff68bfff,0xffff6884,0xff70e890,
      0xffffdf68,0xffb8bdc8};
  if(edge>=4 || w<4 || h<4 || !stroke) return false;
  unsigned color=(style>>(edge*3))&7;
  if(!color || color>=sizeof(colors)/sizeof(*colors)) return false;
  *stroke=(MapDoorStroke){x,y,w,h,colors[color]};
  if(edge<2) {
    stroke->x=x+(edge==1?w-1:0);stroke->y=y+1;
    stroke->w=1;stroke->h=h-2;
  } else {
    stroke->x=x+1;stroke->y=y+(edge==3?h-1:0);
    stroke->w=w-2;stroke->h=1;
  }
  return true;
}
