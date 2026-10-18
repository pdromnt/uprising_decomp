#include "application_entry.h"
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
/*004a7be0: apply the actual84-entry upgrade catalog to the authoritative
 * faction parameter table, consuming its existing budget word. No new state
 * copy: table addresses are fixed-width storage identities, never host pointers. */
int game_menu_quick_apply(GameApplication *a,GameAddress budget) {
    GameAddress root=get(a,0x5f0208),state=get(a,0x59d4c4),offset;uint32_t mode;unsigned i;
    if(!root || !state || !budget)return 0;
    offset=(uint32_t)game_memory_read_u16(&a->memory,root+8)*240u;mode=get(a,state+0x3c);
    for(i=0;i<84 && !a->memory.fault;++i) {
        GameAddress entry=0x58568c+i*16;int32_t kind=(int32_t)get(a,entry-4),wanted=(int32_t)get(a,entry);
        if(kind>=-94 && kind<=-90) {
            static const GameAddress fields[]={0x602884,0x602880,0x60287c,0x602878,0x602874};
            static const uint32_t normal[]={8,3,4,2,8},quick[]={16,6,8,4,16},network[]={24,9,12,6,16};
            unsigned index=(unsigned)(kind+94);GameAddress field=fields[index]+offset;
            uint32_t cap=index==4?9u:(mode==15 || mode==22)?9u:19u;
            int32_t available=(int32_t)(cap-get(a,field));uint32_t cost,step;
            if(wanted==-1 || wanted>available)wanted=available;
            cost=(mode==15 || mode==22)?network[index]:(mode==9 || mode==11)?quick[index]:normal[index];
            for(step=0;step<(uint32_t)(wanted>0?wanted:0);++step) {
                if((int32_t)get(a,budget)<(int32_t)cost)break;
                put(a,field,get(a,field)+1u);put(a,budget,get(a,budget)-cost);
            }
        } else {
            GameAddress field=0x6027f0+offset+(uint32_t)kind*4u,units=get(a,entry+4),vehicles=get(a,entry+8);
            uint32_t cost=0;
            if((int32_t)get(a,field)>=wanted)continue;
            if(units)cost=get(a,units+(uint32_t)wanted*176u+0x50);
            else if(vehicles)cost=get(a,vehicles+(uint32_t)wanted*112u+0x30);
            if((int32_t)cost>(int32_t)get(a,budget))continue;
            put(a,budget,get(a,budget)-cost);put(a,field,(uint32_t)wanted);
        }
    }
    return !a->memory.fault;
}
