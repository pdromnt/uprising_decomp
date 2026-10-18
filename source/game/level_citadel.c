#include "level_objects.h"
#include "level_factory.h"
#include "level_citadels.h"
#include "level_player_slots.h"
#include "level_entity_store.h"
#include "gameplay_state.h"
#include "gameplay_geometry.h"
#include "scene_projection_storage.h"
#include "gameplay_messages.h"
#include "level_run.h"
#include "level_player_resources.h"
#include "menu_sound.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
/*These descriptor lifecycle providers are distinct executable behavior, not a
 *generic virtual-call/word-stack adapter. They remain mandatory native links. */
extern int game_level_group_member_reset(GameApplication *,GameAddress);
extern int game_level_group_member_destroy(GameApplication *,GameAddress);
extern int game_level_group_member_activate(GameApplication *,GameAddress,uint32_t,int);
extern int game_level_group_member_disable(GameApplication *,GameAddress);
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
static GameAddress allocate(void *c,size_t size){(void)c;return scene_projection_storage_allocate(size,"citadel descriptor");}
static int release(void *c,GameAddress p){(void)c;return !p || scene_projection_storage_free(p);}
/*53fee0: detached descriptors have already retired their payloads. Drain the
 *existing intrusive list into the same owner that allocated the records. */
int game_level_deferred_collect(GameApplication *a){
    GameAddress record;unsigned visits=0;
    if(!a)return 0;
    while(!a->memory.fault && (record=gameplay_state_global_object_head(&a->memory))){
        uint32_t base,size;
        if(++visits>65536 || !scene_projection_storage_describe(record,&base,&size,NULL) ||
           base!=record || size<0x3c){a->memory.fault=1;return 0;}
        (void)gameplay_state_pop_global_object(&a->memory,record);
        if(a->memory.fault)return 0;
        if(!release(a,record)){put(a,0x631908,record);return 0;}
        /*No payload callback runs here: the original only rebinds the base
         *vtable and frees storage. Read the authoritative head on each pass. */
    }
    return !a->memory.fault;
}
static int reset(void *c,GameAddress p){return game_level_group_member_reset(c,p);}
static int destroy(void *c,GameAddress p){return game_level_group_member_destroy(c,p);}
static int activate(void *c,GameAddress p,uint32_t tick,int enabled){return game_level_group_member_activate(c,p,tick,enabled);}
static int disable(void *c,GameAddress p){return game_level_group_member_disable(c,p);}
static int catalog(void *c,GameAddress p,uint8_t kind,unsigned item,uint32_t *v) {
    return game_level_player_catalog_value(c,p,kind,item,0,v);
}
static int budget(void *c,GameAddress p,uint32_t amount,int consume,uint32_t *v) {
    GameApplication *a=c;GameAddress method;
    if(!consume)return gameplay_catalog_unit_affordable(&a->memory,p,amount,v)==1;
    method=get(a,get(a,p)+0x30);if(a->memory.fault)return 0;
    if(method==0x477740){*v=0;return 1;} /*Verified base-faction method. */
    if(method!=0x47f230 && method!=0x479490)return 0;
    *v=amount;return gameplay_catalog_spend(&a->memory,p,amount);
}
static int create(void *c,uint8_t kind,uint16_t variant,GameAddress *o){*o=game_level_factory_take(c,kind,variant);return *o!=0;}
static int price(void *c,GameAddress p,int16_t unit,uint32_t *v){return game_level_player_catalog_unit_price(c,p,unit,v);}
static int affordable(void *c,GameAddress p,uint32_t cost,uint32_t *v){return budget(c,p,cost,0,v);}
static GameplayGroupServices services(GameApplication *a){GameplayGroupServices s={a,allocate,release,reset,destroy,catalog,budget,create,activate,disable};return s;}
int game_level_citadel_activate(GameApplication *a,GameAddress group,GameAddress owner,int enabled,uint16_t *status){
    GameplayGroupServices s=services(a);return gameplay_group_activate(&a->memory,&s,group,owner,enabled!=0,status);
}
static float real(GameApplication *a,GameAddress p){uint32_t bits=get(a,p);float v;memcpy(&v,&bits,4);return v;}
static int contains(GameApplication *a,GameAddress group,GameAddress player){
    float x=real(a,player+0x18),z=real(a,player+0x20);
    return real(a,group+4)<=x && real(a,group+12)>=x && real(a,group+8)<=z && real(a,group+16)>=z;
}
static int message(GameApplication *a,GameAddress text){return game_message_enqueue(a,get(a,0x628a88),text,1);}
static int32_t rounded(GameApplication *a,long double v){
    int positive=v>real(a,0x574d30);float stored=(float)v;
    long double n=(long double)stored+(positive?real(a,0x574d9c):-real(a,0x574d9c));
    uint32_t bits=(uint32_t)(uint64_t)(!isfinite(n) || n< -0x1p63L || n>=0x1p63L?INT64_MIN:(int64_t)n);int32_t result;
    memcpy(&result,&bits,4);return result;
}
/*4f8ee0/4f9470: real call-square activation and paid repair. Text goes
 *directly to the existing message owner; no guest stack formatter is needed. */
int game_level_player_station(GameApplication *a,GameAddress player,unsigned operation){
    GameAddress group=0,owner;uint32_t value;char text[256];unsigned visits=0;
    if(!a || a->memory.fault || operation>1)return 0;
    if(!operation){
        int count=(int16_t)game_memory_read_u16(&a->memory,0x639a74),i;
        for(i=0;i<count;i++){
            GameAddress candidate=get(a,get(a,0x639a70)+(uint32_t)i*4);
            if(contains(a,candidate,player)){group=candidate;break;}
        }
        owner=get(a,0x58973c);
        if(a->memory.fault)return 0;
        if(!group)return owner!=get(a,0x5f0208)?!a->memory.fault:message(a,0x596bd8);
        {uint16_t status;
            if(!game_level_citadel_activate(a,group,owner,1,&status))return 0;
            if(get(a,0x58973c)!=get(a,0x5f0208))return !a->memory.fault;
            if(status==3){
                SoundPlaybackRequest sound={121,0,1,0,0,0};
                /* 4f8fbc calls 4a2e20(2): request a strategic refresh, not
                 * 4cf300(2)'s application/control mode change. Switching here
                 * routes mouse input through player+264 (a tower queue),
                 * corrupting the tank's mount model and vertex partitions. */
                put(a,0x6027e4,get(a,0x6027e4)|2u);
                if(a->memory.fault || !message(a,0x596bf8) || !message(a,0x596c10))return 0;
                return game_menu_sound_play(a,&sound);
            }
            if(status==1 || status==2)return message(a,status==1?0x596c58:0x596c70);
            if(status!=0)return 0;
            owner=get(a,0x5f0208);
            if(gameplay_catalog_unit_price(&a->memory,owner,0,&value)!=1)return 0;
            snprintf(text,sizeof(text),"GRAB FAILED: YOU NEED $%d  YOU HAVE $%d!",(int32_t)value,(int32_t)get(a,owner+0x22c));
            return game_message_enqueue_bytes(a,get(a,0x628a88),text,1);
        }
    }
    owner=get(a,0x5f0210+(get(a,player+0x14)&63u)*4);
    for(group=get(a,owner+0x294);group;group=get(a,group+0x64)){
        if(++visits>65536 || a->memory.fault)return 0;
        if(contains(a,group,player))break;
    }
    if(!group)return message(a,0x596cd8);
    {uint32_t maximum,method=get(a,get(a,player)+0x44),ignored;int32_t missing,capacity,fee;float rate;
        if(!game_level_player_catalog_value(a,owner,0x20,8,0,&value) ||
           !game_level_scene_property(a,player,method,&maximum))return 0;
        rate=(float)(((long double)(float)(int32_t)value/(int32_t)maximum)*real(a,0x574d5c));
        if(!game_level_scene_property(a,player,method,&maximum))return 0;
        missing=(int32_t)(maximum-get(a,player+0x58));
        if(!missing)return owner!=get(a,0x5f0208)?!a->memory.fault:message(a,0x596cc4);
        capacity=rounded(a,(long double)(int32_t)get(a,owner+0x22c)/rate);
        if(missing>=capacity)missing=capacity;
        fee=rounded(a,(long double)missing*rate);
        put(a,player+0x58,get(a,player+0x58)+(uint32_t)missing);
        if(!budget(a,owner,(uint32_t)fee,1,&ignored))return 0;
        if(owner!=get(a,0x5f0208))return !a->memory.fault;
        snprintf(text,sizeof(text),"wraith repaired for $%d",fee);
        return game_message_enqueue_bytes(a,get(a,0x628a88),text,1);
    }
}
static int height(void *c,float x,float z,float *v){GameApplication *a=c;*v=(float)gameplay_geometry_terrain_height(&a->memory,x,z);return !a->memory.fault;}
static int begin(void *c,uint32_t count) {
    GameApplication *a=c;GameAddress view=level_entity_store_begin(count);if(!view)return 0;
    game_memory_write_u16(&a->memory,0x639a74,(uint16_t)count);put(a,0x639a70,view);return !a->memory.fault;
}
static int attach_owner(GameApplication *a,GameAddress object,const LevelCitadelDefinition *d) {
    static const uint8_t kinds[10]={0x80,0x81,0x82,0x83,0x89,0x84,0x85,0x86,0x87,0x88};
    GameAddress player;uint32_t assignment;uint16_t status;unsigned i;
    GameplayGroupServices s=services(a);GameplayUnitServices units={a,price,affordable};
    if(d->owner==-1)return 1;
    if(d->owner<0 || d->owner>8)return 0;
    assignment=get(a,0x5f01f0+(uint32_t)d->owner*4);
    if(assignment==UINT32_MAX || (assignment<4 && get(a,0x5ac680)) || get(a,0x5acc88))return !a->memory.fault;
    if(!level_player_slots_scene_player(assignment,&player) || !player ||
       !gameplay_group_activate(&a->memory,&s,object,player,1,&status))return 0;
    for(i=1;i<=d->upgrade_count;++i) {
        uint32_t choice;int kind=d->upgrades[i-1].kind;if(kind<1 || kind>10)continue;
        if(!game_level_player_catalog_choice(a,player,kinds[kind-1],&choice))return 0;
        if(choice==UINT32_MAX)continue;
        if(a->memory.fault || !gameplay_group_bind_member(&a->memory,&s,&units,object,(int16_t)kind,i,&status))return 0;
    }
    return !a->memory.fault;
}
/*54aa30 restores upgrades through the same group and member owners as loading. */
int game_level_citadel_restore_member(GameApplication *a,GameAddress group,uint16_t identity,unsigned slot){
    GameplayGroupServices s=services(a);GameplayUnitServices units={a,price,affordable};uint16_t status;
    return gameplay_group_bind_member(&a->memory,&s,&units,group,(int16_t)identity,slot,&status);
}
/*549db0: gameplay requests choose an eligible slot and preserve the actual
 *retail rejection status. Restoration supplies its explicit archive slot. */
int game_level_citadel_upgrade(GameApplication *a,GameAddress group,uint8_t identity,uint16_t *status){
    GameplayGroupServices s=services(a);GameplayUnitServices units={a,price,affordable};
    return gameplay_group_bind_member(&a->memory,&s,&units,group,identity,UINT32_MAX,status);
}
static int construct(void *c,uint32_t index,const LevelCitadelDefinition *d) {
    GameApplication *a=c;GameAddress object=allocate(a,0x6c);
    if(!object)return 0;
    if(!level_citadels_construct_runtime(&a->memory,index,object,d,a,height)){release(a,object);return 0;}
    if(!level_entity_store_track_guest(object)){(void)game_level_citadel_destroy(a,object);return 0;}
    return level_entity_store_set(index,object) && level_entity_store_transfer_guest(object) && attach_owner(a,object,d);
}
int game_level_citadels_load(GameApplication *a,const char *path,int32_t offset,uint32_t *next){return level_citadels_load(path,offset,a,begin,construct,next);}
int game_level_citadel_destroy(GameApplication *a,GameAddress object) {
    GameplayGroupServices s=services(a);
    if(!gameplay_group_destroy(&a->memory,&s,object) || !release(a,object))return 0;
    level_citadels_retire_runtime(object);return !a->memory.fault;
}
int game_level_citadel_finish(GameApplication *a,GameAddress object){
    GameplayGroupServices s=services(a);return gameplay_group_reset(&a->memory,&s,object);
}
