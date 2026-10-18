#include "level_load.h"
#include "level_player_factory.h"
#include "level_player_resources.h"
#include "level_player_scene.h"
#include "display_transition.h"
#include "level_factory.h"
#include "effect_resource_pool.h"
#include "event_queue.h"
#include <limits.h>
#include <string.h>

/*4a86xx slot placement. The slot owner owns each factory result before
 *publication, including failures during camera binding or child creation. */
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
static float real(GameApplication *a,GameAddress p){uint32_t w=get(a,p);float v;memcpy(&v,&w,4);return v;}
static void write_real(GameApplication *a,GameAddress p,float v){uint32_t w;memcpy(&w,&v,4);put(a,p,w);}
static int anchor(void *c,float x,float z){
    GameApplication *a=c;
    write_real(a,0x603018,x*512.0f);
    put(a,0x60301c,0);
    write_real(a,0x603020,z*512.0f);
    return !a->memory.fault;
}
static int spawn(void *c,uint32_t slot,float x,float z,uint16_t heading){
    GameApplication *a=c;if(slot>=16)return 0;
    game_memory_write_u16(&a->memory,0x603058+slot*2,heading);
    write_real(a,0x603028,x*512.0f);
    put(a,0x60302c,0);
    write_real(a,0x603030,z*512.0f);
    return !a->memory.fault;
}
static int assignment(void *c,uint32_t slot,uint32_t *out){
    GameApplication *a=c;if(slot>=6)return 0;
    *out=get(a,0x5f01f0+slot*4);return !a->memory.fault;
}
static int subtype(void *c,uint32_t player,uint32_t *out){
    return game_level_player_catalog_value(c,player,0x20,3,0,out);
}
static int make(void *c,uint32_t subtype,uint32_t *out){
    if(subtype>UINT16_MAX)return 0;
    *out=game_level_player_factory_take(c,(uint16_t)subtype);return *out!=0;
}
static int publish(void *c,uint32_t player,uint32_t scene){
    GameApplication *a=c;put(a,player+0x3bc,scene);
    put(a,player+0x448,scene);put(a,player+0x450,scene);
    return !a->memory.fault;
}
static int attach(void *c,uint32_t player,uint32_t scene){
    GameApplication *a=c;GameAddress camera=0;
    if(get(a,0x5f0208)==player)camera=get(a,0x58f8a8);
    if(a->memory.fault)return 0;
    return !camera || game_camera_bind_scene(&a->memory,camera,scene);
}
static int place(void *c,uint32_t player,uint32_t scene,uint16_t heading,
    uint32_t identity,uint32_t next,int first){
    GameApplication *a=c;float position[3];uint16_t angles[3]={heading,0,0};unsigned i;
    if(first){put(a,scene+0x214,0);put(a,scene+0x218,1);}
    put(a,player+0x18,next);
    for(i=0;i<3;++i)position[i]=real(a,0x603028+i*4);
    if(a->memory.fault)return 0;
    /*Only the player factory's ordinary4f4f60 method is supported here. */
    if(get(a,get(a,scene)+4)!=0x4f4f60 || a->memory.fault)return 0;
    return game_level_player_scene_place(a,scene,identity,position,angles);
}
int game_level_slot_place(void *c,uint32_t index,uint32_t count,const LevelPlayerSlot *slot){
    static const LevelPlayerSceneOps ops={anchor,spawn,assignment,subtype,make,publish,attach,place};
    return level_player_slots_place_owned(&((GameApplication*)c)->memory,index,count,slot,c,&ops);
}
static int release_event(void *c,GameAddress event){
    GameApplication *a=c;EffectResource *r=effect_resource_find(event);
    if(!r || r->owner->object_kind<1 || r->owner->object_kind>3 ||
       r->event_route!=r->owner->object_kind-1)return 0;
    return effect_resource_pool_return(&a->memory,r->owner,event);
}
static int release_resource(GameApplication *a,GameAddress field){
    GameAddress resource=get(a,field);EffectResourcePool *pool=effect_resource_pool_find(0x5ad7d0);
    if(a->memory.fault || (resource && (!pool ||
       effect_resource_pool_return_chain(&a->memory,pool,resource)!=1)))return 0;
    put(a,field,0);return !a->memory.fault;
}
static int release_base(GameApplication *a,GameAddress player){
    static const uint16_t heads[]={0x64,0xc8,0x12c};unsigned i;
    for(i=0;i<3;++i){GameAddress child;
        while((child=get(a,player+heads[i]))!=0){
            if(a->memory.fault || !game_level_factory_destroy(a,child))return 0;
            /*Destruction must unlink from the canonical intrusive owner. */
            if(get(a,player+heads[i])==child)return 0;
        }
        if(a->memory.fault)return 0;
    }
    return game_level_player_base_resources_release(a,player);
}
int game_level_player_scene_destroy(GameApplication *a,GameAddress player){
    static const uint16_t resources[]={0x470,0x478,0x4a0,0x4a4,0x48c,0x490,
        0x494,0x47c,0x49c,0x4ac,0x4b0,0x480,0x474,0x484,0x488,0x498,0x1fc,0x4a8};
    GameEventQueueCallbacks events={a,NULL,release_event};GameAddress scene;unsigned i;
    LevelPlayerClass kind;
    if(!a || !level_player_slots_owns_player(player))return 0;
    kind=level_player_slots_class(player);
    if(kind==LEVEL_PLAYER_BASE)return release_base(a,player);
    if(kind==LEVEL_PLAYER_FACTION || kind==LEVEL_PLAYER_IMPERIAL){
        /*478980/4797f0: the imperial record retires before ordinary scene and
         *resource ownership.1fc is an owned weapon only in this derived class. */
        if(kind==LEVEL_PLAYER_IMPERIAL && (!game_level_imperial_release(a,player) || !release_resource(a,player+0x1fc)))return 0;
        if(!release_base(a,player))return 0;
        put(a,player+0x294,0);return release_resource(a,player+0x1dc);
    }
    if(kind!=LEVEL_PLAYER_SCENE)return 0; /*Foreign controller classes remain blockers. */
    scene=get(a,player+0x3bc);
    if(a->memory.fault || (scene && !game_level_player_factory_destroy(a,scene)))return 0;
    put(a,player+0x3bc,0); /*Do not repeat a successful scene retirement. */
    if(!game_event_list_clear_head(&a->memory,&events,player+0x458))return 0;
    put(a,player+0x45c,0);
    for(i=0;i<sizeof(resources)/sizeof(resources[0]);++i)
        if(!release_resource(a,player+resources[i]))return 0;
    if(!game_event_list_clear_head(&a->memory,&events,player+0x460))return 0;
    put(a,player+0x464,0);put(a,player+0x3c4,0);put(a,player+0x3c8,0);
    for(i=0;i<10;++i){put(a,player+0x3cc+i*4,0);put(a,player+0x3f4+i*4,0);put(a,player+0x41c+i*4,0);}
    put(a,player+0x444,0);put(a,player+0x448,0);put(a,player+0x44c,UINT32_MAX);put(a,player+0x450,0);
    if(a->memory.fault || !release_base(a,player))return 0;
    put(a,player+0x294,0);
    return release_resource(a,player+0x1dc);
}
