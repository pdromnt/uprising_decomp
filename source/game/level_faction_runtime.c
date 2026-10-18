#include "level_run.h"
#include "level_load.h"
#include <stdint.h>
#include <stdio.h>

extern int game_level_scene_update(GameApplication *,GameAddress,GameAddress);
extern int game_level_scene_transient_reset(GameApplication *,GameAddress,GameAddress);
extern int game_level_descriptor_transient_reset(GameApplication *,GameAddress,GameAddress);
extern int game_level_citadel_finish(GameApplication *,GameAddress);
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}

/*476920/478a20: the real simulation routes for base and AI faction roots.
 *Keep the intrusive scene owner authoritative, including changes made by the
 *native update. The original caches the method table before each update. */
static int next_live(GameApplication *a,GameAddress object,GameAddress method,GameAddress *out){
    unsigned visits=0;
    if(method!=0x42dfb0)return 0;
    object=get(a,object+0x48);
    while(object && !get(a,object+0x58)){
        if(a->memory.fault || ++visits>65536)return 0;object=get(a,object+0x48);
    }
    *out=object;return !a->memory.fault;
}
int game_level_ai_root_update(GameApplication *a,GameAddress root,GameAddress method){
    static const uint16_t lists[]={0x1c,0x80,0xe4};unsigned i,visits;GameAddress object,table,group;
    if(!a || !root || a->memory.fault || (method!=0x476920 && method!=0x478a20)){
        fprintf(stderr,"native faction update failed: root=%08x method=%08x\n",root,method);return 0;
    }
    for(i=0;i<3;i++){
        GameAddress sentinel=root+lists[i];table=get(a,sentinel);
        if(!next_live(a,sentinel,get(a,table+0x50),&object))return 0;
        visits=0;
        while(object){
            if(++visits>65536)return 0;table=get(a,object);
            if(a->memory.fault || !game_level_scene_update(a,object,get(a,table+0x34))){
                fprintf(stderr,"native faction scene failed: root=%08x object=%08x kind=%02x method=%08x\n",
                    root,object,game_memory_read_u8(&a->memory,object+12),get(a,table+0x34));return 0;}
            if(!next_live(a,object,get(a,table+0x50),&object))return 0;
        }
    }
    if(method==0x478a20){
        group=get(a,root+0x294);visits=0;
        while(group){
            if(++visits>65536 || !game_level_citadel_finish(a,group))return 0;
            group=get(a,group+0x64);
        }
    }
    return !a->memory.fault;
}
/*4768f0/4789b0/479910: reset observation census, then scene and descriptor
 *transients. Bounds and links are reread after consumers, as in the reference. */
int game_level_ai_root_reset(GameApplication *a,GameAddress root,GameAddress method){
    GameAddress object,group;unsigned visits=0,i;
    if(!a || !root || a->memory.fault || (method!=0x4768f0 && method!=0x4789b0 && method!=0x479910))return 0;
    object=get(a,root+0xc8);put(a,root+0x1f8,0);
    while(object){
        if(++visits>65536 || !game_level_scene_transient_reset(a,object,get(a,get(a,object)+0x98)))return 0;
        object=get(a,object+0x48);
    }
    if(method!=0x4768f0){
        group=get(a,root+0x294);visits=0;
        while(group){
            if(++visits>65536)return 0;
            for(i=0;(int32_t)i<(int16_t)game_memory_read_u16(&a->memory,group+0x24);i++){
                GameAddress descriptor=get(a,get(a,group+0x18)+i*4);
                if(descriptor && !game_level_descriptor_transient_reset(a,descriptor,get(a,get(a,descriptor)+4)))return 0;
                if(a->memory.fault)return 0;
            }
            group=get(a,group+0x64);
        }
    }
    if(method==0x479910){
        object=get(a,root+0x2e0);
        if(object && !game_level_scene_transient_reset(a,object,get(a,get(a,object)+0x98)))return 0;
    }
    return !a->memory.fault;
}
