#include "gameplay_input.h"
#include "gameplay_catalog.h"
#include <string.h>

/*47bf00: selecting a friendly scene uses its current generation, the real
 * collection/member lookup and the existing pooled root-event owner. */
int game_input_root_select(GameApplication *a,GameAddress root){
    GameMemory *m=&a->memory;GameAddress selected=game_memory_read_u32(m,0x60387c),collection;
    uint32_t identity,flags,visits=0;GameRootEventServices services=game_input_event_services(a);
    if(!selected || selected==game_memory_read_u32(m,root+0x3bc))return !m->fault;
    identity=game_memory_read_u32(m,selected+0x14);
    if(identity!=game_memory_read_u32(m,0x603e38) || (identity&63u)!=game_memory_read_u16(m,root+8))return !m->fault;
    flags=game_memory_read_u32(m,selected+0x10);
    if(m->fault)return 0;
    if(!(flags&0x100u))return (flags&0x20000u)?1:game_root_word_publish(m,&services,root,45,identity);
    collection=game_memory_read_u32(m,root+0x294);
    while(collection && !m->fault){
        uint16_t bits=game_memory_read_u16(m,collection+0x24);int16_t count;unsigned index;
        memcpy(&count,&bits,sizeof count);
        if(++visits>4096u){m->fault=1;return 0;}
        for(index=0;index<(unsigned)(count>0?count:0);++index){
            GameAddress table=game_memory_read_u32(m,collection+0x18),member=game_memory_read_u32(m,table+index*4),match;
            uint32_t method;int status;
            if(!member)continue;
            method=game_memory_read_u32(m,game_memory_read_u32(m,member)+0x14);
            if(m->fault)return 0;
            status=gameplay_catalog_lookup(m,member,method,identity,NULL,NULL,&match);
            if(status!=1){m->fault=1;return 0;}
            if(match)return game_root_command_publish(m,&services,root,41,game_memory_read_u8(m,collection),(uint8_t)index);
            bits=game_memory_read_u16(m,collection+0x24);memcpy(&count,&bits,sizeof count);
        }
        collection=game_memory_read_u32(m,collection+0x64);
    }
    return !m->fault;
}
