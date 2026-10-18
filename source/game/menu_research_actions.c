#include "menu_widgets.h"
#include "runtime_utilities.h"
#include <stdio.h>
#include <string.h>
extern int game_menu_request(GameApplication *,GameAddress,uint32_t,uint32_t,uint32_t);
extern int game_menu_quick_feedback(GameApplication *);
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
/*4a7110: unlock the actual faction parameter record; no second upgrade object. */
int game_menu_research_unlock(GameApplication *a) {
    static const struct {GameAddress address;uint32_t value;} values[]={
        {0x602834,2},{0x602838,2},{0x60283c,2},{0x602848,2},{0x60284c,2},
        {0x602864,0},{0x602868,0},{0x602844,2},{0x602854,2},{0x602858,2},
        {0x602830,2},{0x602850,2},{0x602840,3},{0x602860,2},{0x60285c,1},
        {0x6027f0,4},{0x6027f4,4},{0x6027f8,4},{0x602804,4},{0x602820,2},
        {0x602814,2},{0x602824,4},{0x60282c,3}};
    GameAddress player=get(a,0x5f0208);uint32_t slot;unsigned i;
    if(!player)return !a->memory.fault;
    slot=game_memory_read_u16(&a->memory,player+8);if(slot>=6 || a->memory.fault)return 0;
    for(i=0;i<sizeof(values)/sizeof(values[0]);++i)put(a,values[i].address+slot*240,values[i].value);
    return !a->memory.fault;
}
/*48fcd0: actual optional resource labels consume signed decimal values. The
 * retail formats58bae8/58baec are both %d; no guest formatter/import is needed. */
int game_menu_resource_feedback(GameApplication *a) {
    unsigned i;char text[32];
    for(i=0;i<2;++i) {
        RuntimeResourceDescriptor d=runtime_resource_descriptor(&a->memory,i);
        if(a->memory.fault)return 0;
        if(d.owner) {
            int64_t value=d.value<0x80000000u?(int64_t)d.value:(int64_t)d.value-INT64_C(4294967296);
            snprintf(text,sizeof(text),"%ld",(long)value);
            if(!game_menu_text_image_set_bytes(a,d.owner,text,0))return 0;
        }
    }
    return !a->memory.fault;
}
/*The cheat's consumer chain also rebuilds the research selector and its score
 * and resource labels. Keep that order, including the shared dirty flag. */
int game_menu_complete_research(GameApplication *a) {
    if(!game_menu_research_unlock(a) || !game_menu_request(a,get(a,0x59d4c0),530,1001,0) ||
       !game_menu_quick_feedback(a) || !game_menu_resource_feedback(a))return 0;
    put(a,0x5f003c,1);return !a->memory.fault;
}
/*523680/6a0/6c0: shared selectors and resource increment buttons retain their
 * original wrapped arithmetic, signed limits and redraw publications. */
int game_menu_quick_parameter_action(GameApplication *a,uint32_t method,uint32_t argument) {
    if(method==0x523680) {
        if(!game_menu_quick_feedback(a) || !game_menu_resource_feedback(a))return 0;
        put(a,0x5f003c,1);
    } else if(method==0x5236a0) {
        if(!game_menu_quick_feedback(a))return 0;
        put(a,0x5eff00,1);
    } else if(method==0x5236c0) {
        uint32_t delta=argument*10u,resource=get(a,0x5ac910)+delta,total;
        if((int32_t)resource>999 || (int32_t)resource<0)return !a->memory.fault;
        total=get(a,0x5acbec)+delta;
        if((int32_t)total<0)return !a->memory.fault;
        put(a,0x5ac910,resource);put(a,0x5acbec,total);
        if(!game_menu_resource_feedback(a))return 0;
        put(a,0x5eff00,1);
    } else return 0;
    return !a->memory.fault;
}
static int same(GameApplication *a,GameAddress first,GameAddress second,int *equal) {
    unsigned i;*equal=0;
    for(i=0;i<1024;++i) {
        uint8_t x=game_memory_read_u8(&a->memory,first+i),y=game_memory_read_u8(&a->memory,second+i);
        if(a->memory.fault)return 0;
        if(x!=y)return 1;
        if(!x){*equal=1;return 1;}
    }
    return 0;
}
/*52cb80: the credits cheat replaces labels in their existing eight-byte rows,
 * then changes the first Bombs substring in each selected label to Clown. The
 * application data owner holds these mutable strings; no retail file is written. */
int game_menu_cheat_action(GameApplication *a) {
    static const GameAddress names[][2]={
        {0x59a774,0x59a780},{0x59a790,0x59a7a4},{0x59a7b4,0x59a7c8},{0x59a7d8,0x59a7ec},
        {0x59a7fc,0x59a808},{0x59a820,0x59a82c},{0x59a838,0x59a83c},{0x59a848,0x59a85c},
        {0x59a868,0x59a878},{0x59a894,0x59a8ac},{0x59a8c4,0x59a8d4},{0x59a8ec,0x59a900},
        {0x59a910,0x59a924},{0x59a93c,0x59a950},{0x59a968,0x59a984},{0x59a990,0x59a9a4},
        {0x59a9b4,0x59a9c8},{0x59a9e8,0x59a9f8},{0x59aa14,0x59aa30},{0x59aa44,0x59aa58},
        {0x59aa6c,0x59aa78},{0x59aa90,0x59aa98},{0x59aabc,0x59aacc},{0x59aad8,0x59aae0},
        {0x59aaf4,0x59ab04},{0x59ab14,0x59ab24},{0x59ab34,0x59ab40},{0x59ab50,0x59ab60},
        {0x59ab70,0x59ab8c},{0x59ab9c,0x59abac},{0x59abc4,0x59abd8},{0x59abec,0x59ac04},
        {0x59ac1c,0x59ac2c}};
    uint32_t count=get(a,0x58b028),i;unsigned j;
    if(a->memory.fault)return 0;
    if((int32_t)count<=0)return 1;
    /*The table ends immediately before its count at58b028. */
    if(count>(0x58b028u-0x58ac10u)/8u)return 0;
    for(i=0;i<count;++i) {
        GameAddress row=0x58ac10+i*8,label=get(a,row);int equal;
        for(j=0;j<sizeof(names)/sizeof(names[0]);++j) {
            if(!same(a,label,names[j][0],&equal))return 0;
            if(equal){put(a,row,names[j][1]);break;}
        }
    }
    for(i=0;i<count;++i) {
        GameAddress label=get(a,0x58ac10+i*8);unsigned length;
        for(length=0;length<1024;++length)if(!game_memory_read_u8(&a->memory,label+length))break;
        if(length==1024 || a->memory.fault)return 0;
        for(j=0;j+5<=length;++j) {
            char word[5];if(!game_memory_read(&a->memory,label+j,word,sizeof(word)))return 0;
            if(!memcmp(word,"Bombs",5)) {if(!game_memory_write(&a->memory,label+j,"Clown",5))return 0;break;}
        }
    }
    return !a->memory.fault;
}
