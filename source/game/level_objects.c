#include "level_objects.h"
#include "level_factory.h"
#include "../core/level_objects.h"
#include "level_player_slots.h"
#include "level_player_resources.h"
#include "level_entity_store.h"
#include "application_name_registry.h"
#include "gameplay_catalog.h"
#include "gameplay_geometry.h"
#include "state_helper_plans.h"
#include "game_intrusive_list.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
static int name(GameApplication *a,const char *text,GameAddress object) {
    uint32_t record,binding=object?get(a,object+0x14):0;
    return !a->memory.fault && application_name_registry_register(text,object,binding,&record);
}
static uint32_t integer(float x){return !isfinite(x)||x<-2147483648.0f||x>=2147483648.0f?0x80000000u:(uint32_t)(int32_t)x;}
static int begin(void *context,uint32_t count,uint32_t first) {
    /*The original begin/end callbacks contain no actions. Construction and
     * linking below execute for real; these are not successful gameplay stubs. */
    (void)context;(void)count;(void)first;return 1;
}
static void end(void *context){(void)context;}
static int construct(void *context,uint32_t index,const LevelObjectDefinition *d,uint32_t *result) {
    GameApplication *a=context;GameMemory *m=&a->memory;uint32_t kind=(uint32_t)d->kind,slot=UINT32_MAX,assignment,packed,next,id;
    GameAddress player=0,object,node,following;float p[3];uint8_t type;unsigned i,offset;uint32_t flags;
    (void)index;*result=0;
    if(kind>=0xb0 && kind<0xc0){slot=kind-0xb0;kind=0xb0;}
    else if(kind>=0x77 && kind<0x7b){slot=kind-0x77;kind=0x77;}
    if(kind==0xb0) {
        for(i=0;i<4;++i){GameAddress candidate;if(!level_player_slots_scene_player(i,&candidate))return 0;if(candidate && get(a,candidate+0x14))player=candidate;}
    }else {
        int owner=d->owner==8?5:d->owner;
        if(owner<0 || owner>8)return 0;
        assignment=get(a,0x5f01f0+(uint32_t)owner*4);
        if(assignment==UINT32_MAX || (assignment<4 && get(a,0x5ac680)))return !m->fault;
        if(!level_player_slots_scene_player(assignment,&player))return 0;
    }
    if(m->fault || !player || kind>255) {
        game_application_log(a,"native object failed: %s\n","player binding");return 0;
    }
    if(get(a,0x5acc88))return !strcmp(d->name,"NULL") || name(a,d->name,0);
    if(kind!=0xb0 && kind!=0x77) {
        if(!game_level_player_catalog_value(a,player,(uint8_t)kind,3,0,&slot))return 0;
    }
    object=game_level_factory_take(a,(uint8_t)kind,(uint16_t)slot);
    if(!object || m->fault) {
        char detail[160];snprintf(detail,sizeof(detail),"factory kind=%u slot=%u name=%s fault=%d address=%08x",
            kind,slot,d->name,m->fault,m->fault_address);
        game_application_log(a,"native object failed: %s\n",detail);return 0;
    }
    if(!level_entity_store_track_guest(object)){(void)game_level_factory_destroy(a,object);return 0;}
    flags=get(a,object+0x10);
    if((flags&0x100000) && d->variant>0)put(a,object+0xac,(uint32_t)d->variant);
    if(flags&0x40)*result=object;
    p[0]=d->x*512.0f;p[1]=d->y*512.0f;p[2]=d->z*512.0f;
    type=game_memory_read_u8(m,object+0xc);
    if(type==9 && game_memory_read_u16(m,player+8)>4) {
        node=get(a,object+0x50);following=get(a,node+4);
        put(a,object+8,node);put(a,object+4,following);if(following)put(a,following+8,object);put(a,node+4,object);
        *result=0;return !m->fault && level_entity_store_transfer_guest(object);
    }
    if(type!=0x26 && type!=0x27 && type!=0xb0){
        uint32_t bits=get(a,object+0x30);float height;memcpy(&height,&bits,4);
        p[1]=level_object_start_height(type,d->y,height,gameplay_geometry_terrain_height(m,p[0],p[2]));
    }
    packed=(integer(d->heading_degrees*5.688889026641846f)&2047u)|(kind<<16);
    if(!level_player_slots_next_owner_sequence(&a->memory,player,&id,&next))return 0;
    put(a,player+0x18,next);
    if(type==9) {
        /*The last two42d420/536250 arguments are masked pitch/roll angles. */
        uint16_t angles[3]={(uint16_t)packed,(uint16_t)get(a,player+0x1fc),10};
        if(!game_level_object_place_angles(a,object,id,p,angles))return 0;
    }else if(!game_level_object_place(a,object,id,p,packed)) {
        char detail[160];snprintf(detail,sizeof(detail),"placement kind=%u method=%08x name=%s object=%08x fault=%d address=%08x",
            kind,get(a,get(a,object)+0x78),d->name,object,m->fault,m->fault_address);
        game_application_log(a,"native object failed: %s\n",detail);return 0;
    }
    offset=state_helper_flagged_member_offset(get(a,object+0x10));
    if(m->fault || !level_player_slots_attach_scene_member(player,object,offset))return 0;
    (void)game_list_insert_after(m,object,player+offset,0x48,0x4c);
    if(m->fault){level_player_slots_forget_scene_member(object);return 0;}
    if(!level_entity_store_transfer_guest(object))return 0;
    return !m->fault && (!strcmp(d->name,"NULL") || name(a,d->name,object));
}
static int link(void *context,uint32_t object,uint32_t parent) {
    GameApplication *a=context;if(!object || !parent)return 0;
    put(a,object+0x108,parent);put(a,parent+0x10c,object);return !a->memory.fault;
}
int game_level_objects_load(GameApplication *a,const char *path,int32_t offset,uint32_t *next) {
    return level_objects_load(path,offset,a,begin,construct,link,end,next);
}
