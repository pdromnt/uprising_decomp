#include "application_entry.h"
#include "application_config.h"
#include <stdio.h>
#include "game_state_ops.h"
#include <stdint.h>

extern ResourceFile *game_application_bindings_file(GameApplication *,int write);
static const struct {GameAddress address;uint32_t value;} default_keys[]={
    {0x6047f8u,0x3fu},
    {0x6047fcu,0x3eu},
    {0x604800u,0x42u},
    {0x604804u,0x43u},
    {0x604808u,0x41u},
    {0x60480cu,0x40u},
    {0x604810u,0x3cu},
    {0x604814u,0x45u},
    {0x604818u,0x46u},
    {0x60481cu,0x47u},
    {0x604868u,0x20u},
    {0x6048a8u,0x25u},
    {0x604838u,0x36u},
    {0x604874u,0x22u},
    {0x604878u,0x15u},
    {0x604880u,0x2bu},
    {0x604884u,0x29u},
    {0x604888u,0x14u},
    {0x6048b8u,0x30u},
    {0x604850u,0x11u},
    {0x604830u,0x34u},
    {0x60483cu,0x37u},
    {0x604840u,0x38u},
    {0x604834u,0x35u},
    {0x604844u,0x39u},
    {0x6048a4u,0x24u},
    {0x6048a0u,0x21u},
    {0x604b10u,0x1u},
    {0x604b30u,0x2u},
    {0x604938u,0x5u},
    {0x604a64u,0x6u},
    {0x604b24u,0x3u},
    {0x604b1cu,0x4u},
    {0x604898u,0x7u},
    {0x604864u,0x8u},
    {0x6048dcu,0x9u},
    {0x6048e0u,0xau},
    {0x6048e4u,0xbu},
    {0x6048e8u,0xcu},
    {0x6048ecu,0xdu},
    {0x6048f0u,0xeu},
    {0x6048f4u,0xfu},
    {0x6048fcu,0x16u},
    {0x60494cu,0x1eu},
    {0x604950u,0x12u},
    {0x604a60u,0x10u},
    {0x604904u,0x2eu},
    {0x60482cu,0x13u},
    {0x604824u,0x17u},
    {0x604820u,0x18u},
    {0x60485cu,0x19u},
    {0x604858u,0x1au},
    {0x604b3cu,0x1bu},
    {0x604b14u,0x1cu},
    {0x604b34u,0x1du},
    {0x6047f4u,0x2fu},
    {0x604accu,0x23u},
    {0x604b2cu,0x27u},
};

/* 004d6850, including the fifth label selected by action15. */
int game_menu_bindings_labels(GameApplication *a,unsigned start,unsigned count) {
    if(start>256u || count>256u-start)return 0;
    game_state_refresh_localized_name_slots(&a->memory,start,count,0);
    return !a->memory.fault;
}
/* 004d7420: duplicate bindings intentionally choose the last physical key. */
int game_menu_bindings_reverse(GameApplication *a) {
    GameMemory *m=&a->memory;unsigned key;
    for(key=0;key<256u;++key) {
        uint32_t action=game_memory_read_u32(m,0x6047f0u+key*4u);
        if(m->fault || action>=256u)return 0;
        game_memory_write_u32(m,0x604c38u+action*4u,key);
    }
    return !m->fault;
}
/* 004d6420: input action defaults and the separate 32-entry controller map.
 * These records retain their established owner and consumer addresses. */
int game_menu_bindings_defaults(GameApplication *a) {
    GameMemory *m=&a->memory;unsigned i;
    game_memory_fill_u32(m,0x6047f0u,256u,0,0);
    for(i=0;i<sizeof(default_keys)/sizeof(*default_keys);++i)
        game_memory_write_u32(m,default_keys[i].address,default_keys[i].value);
    game_memory_write_u32(m,0x6048c8u,0x28u);game_memory_write_u32(m,0x604860u,0x2au);
    if(!game_menu_bindings_labels(a,0,256u))return 0;
    game_memory_fill_u32(m,0x604770u,32u,255u,0);
    game_memory_write_u32(m,0x604770u,0);game_memory_write_u32(m,0x604774u,1);
    for(i=0;i<6u;++i)game_memory_write_u32(m,0x604788u+i*4u,i+2u);
    return !m->fault;
}
/* 004d67c0/004d6740: the file stores 255 keyboard words, not 256, followed
 * by 32 controller words. No native pointers or host-endian struct copies. */
int game_menu_bindings_load(GameApplication *a) {
    ResourceFile *file=NULL;uint8_t bytes[1148];GameConfig *configuration=game_application_config(a);char key[32];
    uint32_t values[287];GameMemory *m=&a->memory;unsigned i;
    if(game_config_get(configuration,"keyboard","key_000")){
        for(i=0;i<287;i++){
            const char *section=i<255?"keyboard":"controller";
            snprintf(key,sizeof(key),i<255?"key_%03u":"button_%02u",i<255?i:i-255);
            if(!game_config_get(configuration,section,key))return 0;
            values[i]=game_config_number(configuration,section,key,UINT32_MAX);
            if(i<255 && values[i]>=256)return 0;
        }
    }else{
    file=game_application_bindings_file(a,0);if(!file)return 0;
    if(resource_file_read(file,bytes,sizeof(bytes))!=sizeof(bytes)){resource_file_close(file);return 0;}
    resource_file_close(file);
    for(i=0;i<287u;++i) {
        const uint8_t *p=bytes+i*4u;
        values[i]=(uint32_t)p[0]|(uint32_t)p[1]<<8u|(uint32_t)p[2]<<16u|(uint32_t)p[3]<<24u;
        if(i<255u && values[i]>=256u)return 0;
    }
    }
    for(i=0;i<255u;++i)game_memory_write_u32(m,0x6047f0u+i*4u,values[i]);
    for(i=0;i<32u;++i)game_memory_write_u32(m,0x604770u+i*4u,values[255u+i]);
    return !m->fault && game_menu_bindings_reverse(a) && game_menu_bindings_labels(a,0,256u);
}
int game_menu_bindings_save(GameApplication *a) {
    GameConfig *configuration=game_application_config(a);unsigned i;char key[32];
    for(i=0;i<287u;++i) {
        uint32_t value=game_memory_read_u32(&a->memory,i<255u?0x6047f0u+i*4u:0x604770u+(i-255u)*4u);
        snprintf(key,sizeof(key),i<255?"key_%03u":"button_%02u",i<255?i:i-255);
        if(!game_config_set_number(configuration,i<255?"keyboard":"controller",key,value))return 0;
    }
    if(a->memory.fault)return 0;
    return 1;
}
