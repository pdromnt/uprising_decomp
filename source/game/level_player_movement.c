#include "level_run.h"
#include "player_collision.h"
#include "player_state.h"
#include "presentation_instance.h"
#include "presentation_collision.h"
#include "gameplay_spatial.h"
#include "menu_sound.h"
#include <string.h>
/*4f2440/4f9610/4f9730. Collision retries keep real C vectors; no temporary
 *guest stack, serialized pose mirror or register-call adapter. */
typedef struct MovementQuery {
    GameApplication *app;GameAddress player;const float *previous;float *contact;
} MovementQuery;
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
/*The three established collision programs have different real semantics:
 *skinned records, world overlap, and swept controller contact respectively. */
int game_level_scene_collision(GameApplication *a,GameAddress candidate,GameAddress player,
    GameAddress method,const float previous[3],float contact[3],uint32_t *collides){
    int hit,status;
    if(a->memory.fault)return 0;
    if(method==0x419e60)status=presentation_spawn_collision(&a->memory,candidate,player,contact,&hit);
    else if(method==0x482bb0)status=presentation_record_collision(&a->memory,candidate,player,contact,&hit);
    else if(method==0x482f20)status=presentation_record_controller_collision(&a->memory,candidate,player,contact,&hit);
    else if(method==0x42dfd0 || method==0x535b30 || method==0x42e300)
        status=presentation_collision(&a->memory,candidate,player,method==0x42e300?2:method==0x535b30,previous,contact,&hit);
    else{a->memory.fault=1;a->memory.fault_address=method;return 0;}
    if(!status)return 0;
    *collides=(uint32_t)hit;return !a->memory.fault;
}
/*5350d0: first accepted overlap in stable neighboring-bucket/list order.
 *This uses the existing owned index, and the same native collision programs
 *as movement. It does not allocate contact records or apply player filters. */
int game_level_scene_overlap(GameApplication *a,GameAddress object,GameAddress method,
    const float position[3],float contact[3],int *hit){
    GameAddress heads[9],candidate;uint32_t bits;float x,z;unsigned i,visits;
    if(!a || !hit || a->memory.fault)return 0;
    if(method!=0x5350d0){a->memory.fault=1;a->memory.fault_address=method;return 0;}
    bits=get(a,object+0x18);memcpy(&x,&bits,4);bits=get(a,object+0x20);memcpy(&z,&bits,4);
    if(a->memory.fault || !gameplay_spatial_query_heads(gameplay_spatial_grid(0x5f0800),x,z,heads))return 0;
    *hit=0;
    for(i=0;i<9;i++){
        candidate=heads[i];visits=0;
        while(candidate){
            if(++visits>65536)return 0;
            if(candidate!=object && !(get(a,candidate+16)&0x400u)){
                uint32_t result;GameAddress collision=get(a,get(a,candidate)+0x28);
                if(a->memory.fault || !game_level_scene_collision(a,candidate,object,collision,position,contact,&result))return 0;
                if(result){*hit=1;return 1;}
            }
            candidate=get(a,candidate+4);if(a->memory.fault)return 0;
        }
    }
    return !a->memory.fault;
}
static int buckets(void *context,GameAddress player,uint32_t xbits,uint32_t zbits,GameAddress *out){
    float x,z;(void)context;(void)player;
    memcpy(&x,&xbits,4);memcpy(&z,&zbits,4);
    return gameplay_spatial_query_group(0x5f0800,x,z,out);
}
static int probe(void *context,GameAddress candidate,GameAddress player,
    uint32_t unused_first,uint32_t unused_second,uint32_t *collides){
    MovementQuery *q=context;GameApplication *a=q->app;GameAddress method=get(a,get(a,candidate)+0x28);
    (void)unused_first;(void)unused_second;
    return !a->memory.fault && game_level_scene_collision(a,candidate,player,method,q->previous,q->contact,collides);
}
static int query(void *context,const float previous[3],float contact[3],GameAddress excluded,GameAddress *collider){
    MovementQuery *q=context;GameApplication *a=q->app;MovementQuery vectors={a,q->player,previous,contact};
    GamePlayerCollisionOps services={&vectors,buckets,probe};
    GameAddress method=get(a,get(a,q->player)+(excluded?0x7c:0x80));
    if(method!=(excluded?0x4f9730u:0x4f9610u) || a->memory.fault)return 0;
    /*The legacy kernel's two address arguments are used only by its probe
     *port. This port owns real borrowed vectors instead; no addresses needed. */
    return game_player_find_collision(game_player_ref(&a->memory,q->player),0,0,excluded,!excluded,&services,collider);
}
static int engine(void *context,GameAddress bank,uint32_t bits){
    MovementQuery *q=context;float pitch;memcpy(&pitch,&bits,4);
    return game_menu_sound_frequency(q->app,bank,9,0,pitch);
}
static int landing(void *context,GameAddress bank,const uint32_t words[6]){
    MovementQuery *q=context;SoundPlaybackRequest request;
    request.sound=(int16_t)words[0];memcpy(&request.distance_squared,words+1,4);
    memcpy(&request.volume,words+2,4);memcpy(&request.pitch,words+3,4);
    request.flags=words[4];request.force=words[5]!=0;
    if(bank!=get(q->app,0x628a90))return 0;
    return game_menu_sound_play(q->app,&request);
}
int game_level_player_input_tick(GameApplication *a,GameAddress queue,uint32_t tick){
    GameAddress player=queue-0x13c;const PresentationInstance *instance=presentation_instance_find(player);
    PresentationPose *pose=presentation_instance_pose(player);PlayerControls *controls=presentation_instance_player_controls(player);
    PlayerRig *rig=presentation_instance_player_rig(player);PresentationLifecycle *lifecycle=presentation_instance_lifecycle(player);
    PlayerCollisionParameters parameters;PlayerCollisionServices services={0};MovementQuery context={a,player,NULL,NULL};
    uint32_t result,death;(void)tick;
    if(!a || !instance || !pose || !controls || !rig || !lifecycle || a->memory.fault)return 0;
    memcpy(&parameters.radius,&instance->definition.radius,4);
    parameters.clearance=instance->definition.dimensions[1];parameters.max_speed=lifecycle->runtime.fractions[1];
    death=get(a,player+0x498);memcpy(&parameters.death_tick,&death,4);
    services.context=&context;services.query=query;
    services.motion.context=&context;services.motion.update_engine_scalar=engine;
    services.terrain.context=&context;services.terrain.landing_sound=landing;
    return !a->memory.fault && player_resolve_movement_collision(&a->memory,player,pose,controls,rig,&parameters,&services,&result);
}
