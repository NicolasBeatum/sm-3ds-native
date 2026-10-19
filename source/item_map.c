#include "item_map.h"
#include <string.h>

static unsigned Read(const uint8_t *rom,unsigned bank,unsigned address) {
  unsigned offset=((bank&127)<<15)|(address&32767);
  return rom[offset]|(rom[offset+1]<<8);
}
static bool Room(unsigned p) { return p>=0x8000 && p<0xe5d2-13; }

static bool AddState(ItemMapIndex *out,const uint8_t *rom,unsigned state,
                     unsigned area,unsigned mx,unsigned my,unsigned w,unsigned h) {
  if(state<0x8000 || state>0xffe6) return false;
  unsigned list=Read(rom,0x8f,state+20);
  if(!list) return true;
  if(list<0x8000) return false;
  for(unsigned i=0;i<128;i++,list+=6) {
    if(list>0xfffa) return false;
    unsigned plm=Read(rom,0x8f,list);
    if(!plm) return true;
    /* 21 native items, each with normal, Chozo and hidden block variants. */
    if(area>=6 || plm<0xeed7 || plm>0xefcf || (plm-0xeed7)%4) continue;
    unsigned xy=Read(rom,0x8f,list+2),bit=Read(rom,0x8f,list+4);
    unsigned bx=xy&255,by=xy>>8;
    unsigned x=mx+bx/16,y=my+by/16+1;
    if(bit>=512 || bx>=w*16 || by>=h*16 || x>=64 || y>=32) return false;
    unsigned cell=area*2048+y*64+x;
    bool exists=false;
    for(unsigned j=0;j<out->count;j++)
      if(out->entries[j].cell==cell && out->entries[j].bit==bit) {exists=true;break;}
    if(exists) continue;
    if(out->count==ITEM_MAP_CAPACITY) return false;
    out->entries[out->count++]=(ItemMapEntry){cell,bit};
  }
  return false;
}

bool ItemMap_BuildWithVisitor(ItemMapIndex *out,const uint8_t *rom,size_t size,
    uint16_t root,ItemMapVisitState *visit,void *context) {
  memset(out,0,sizeof(*out));
  if(!rom || size<0x80000 || !Room(root)) return false;
  uint16_t rooms[320]={root};
  unsigned roomCount=1;
  for(unsigned n=0;n<roomCount;n++) {
    unsigned room=rooms[n];
    unsigned area=Read(rom,0x8f,room+1)&255,xy=Read(rom,0x8f,room+2);
    unsigned wh=Read(rom,0x8f,room+4),w=wh&255,h=wh>>8;
    if(area>7 || !w || !h || w>16 || h>16) goto invalid;
    unsigned doors=Read(rom,0x8f,room+9);
    if(doors<0x8000) goto invalid;
    /* Door arrays have no count. Stop at the first non-door pointer, like
     * the native data layout; all reads and traversal storage are bounded. */
    for(unsigned j=0;j<64 && doors<=0xfffe;j++,doors+=2) {
      unsigned door=Read(rom,0x8f,doors);
      if(door<0x8000 || door>0xfff4) break;
      unsigned dest=Read(rom,0x83,door);
      if(!Room(dest)) break;
      bool seen=false;
      for(unsigned i=0;i<roomCount;i++) if(rooms[i]==dest) {seen=true;break;}
      if(!seen) {
        if(roomCount==320) goto invalid;
        rooms[roomCount++]=dest;
      }
    }
    unsigned p=room+11;
    bool finished=false;
    for(unsigned j=0;j<16 && p<=0xfffa;j++) {
      unsigned code=Read(rom,0x8f,p);p+=2;
      unsigned state;
      if(code==0xe5e6) {state=p;finished=true;}
      else if(code==0xe5ff || code==0xe652 || code==0xe669) {
        state=Read(rom,0x8f,p);p+=2;
      } else if(code==0xe612 || code==0xe629) {
        state=Read(rom,0x8f,p+1);p+=3;
      } else goto invalid;
      if(!AddState(out,rom,state,area,xy&255,xy>>8,w,h)) goto invalid;
      if(visit) visit(context,rom,room,state);
      if(finished) break;
    }
    if(!finished) goto invalid;
  }
  for(unsigned i=1;i<out->count;i++) {
    ItemMapEntry entry=out->entries[i];unsigned j=i;
    while(j && out->entries[j-1].cell>entry.cell) {
      out->entries[j]=out->entries[j-1];j--;
    }
    out->entries[j]=entry;
  }
  out->valid=true;
  return true;
invalid:
  out->count=0;out->valid=false;return false;
}

bool ItemMap_Build(ItemMapIndex *out,const uint8_t *rom,size_t size,uint16_t root) {
  return ItemMap_BuildWithVisitor(out,rom,size,root,NULL,NULL);
}

enum ItemMapState ItemMap_State(const ItemMapIndex *s,unsigned area,int x,int y,
                               const uint8_t bits[64]) {
  if(!s->valid || !bits || area>=6 || (unsigned)x>=64 || (unsigned)y>=32)
    return kItemMapNone;
  unsigned cell=area*2048+y*64+x,lo=0,hi=s->count;
  while(lo<hi) {
    unsigned mid=(lo+hi)/2;
    if(s->entries[mid].cell<cell) lo=mid+1;else hi=mid;
  }
  bool found=false;
  for(;lo<s->count && s->entries[lo].cell==cell;lo++) {
    unsigned bit=s->entries[lo].bit;
    if(!(bits[bit>>3]&(1u<<(bit&7)))) return kItemMapRemaining;
    found=true;
  }
  return found?kItemMapCollected:kItemMapNone;
}
