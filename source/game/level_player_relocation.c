#include "level_player_scene.h"
#include "level_effect_runtime.h"
#include "level_factory.h"
#include "level_objects.h"
#include "level_player_slots.h"
#include "player_relocation.h"
#include "presentation_instance.h"
#include "entity_vertex_transform.h"
#include "gameplay_geometry.h"
#include "gameplay_catalog.h"
#include "game_intrusive_list.h"
#include "menu_sound.h"
#include "display_transition.h"
#include "player_feedback.h"
#include <math.h>
#include <limits.h>
#include <string.h>

extern int game_level_vehicle_death_detach(GameApplication *,GameAddress,GameAddress,GameAddress);
extern int game_level_secondary_descriptor_retire(GameApplication *,GameAddress,unsigned);
extern int game_level_articulated_descriptor_retire(GameApplication *,GameAddress);
extern int game_level_arrival_delegate_create(GameApplication *,GameAddress,GameAddress);
extern int game_application_options_load(GameApplication *,uint32_t);
extern int game_level_display_mask_resize(GameApplication *,uint32_t);
typedef struct Relocation {GameApplication *app;GameAddress player;const float *destination;} Relocation;
typedef struct Arrival {GameApplication *app;GameAddress object;EffectAnimation *animation;} Arrival;
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
static float real(GameApplication *a,GameAddress p){uint32_t bits=get(a,p);float value;memcpy(&value,&bits,4);return value;}
static int positional_sound(GameApplication *a,const float p[3],int16_t id){GameAddress reference=get(a,0x58f890);long double dx=(long double)p[0]-real(a,reference+12),dz=(long double)p[2]-real(a,reference+20);
    SoundPlaybackRequest request={id,(float)(dx*dx+dz*dz),1,0,0,0};return !a->memory.fault && game_menu_sound_play(a,&request);
}
static int sequence(GameApplication *a,GameAddress catalog,uint32_t *id){uint32_t next;
    if(level_player_slots_owns_player(catalog)){if(!level_player_slots_next_owner_sequence(&a->memory,catalog,id,&next))return 0;}
    else{uint32_t n=get(a,catalog+24);next=n+1;*id=(n<<6)|game_memory_read_u16(&a->memory,catalog+8);}
    put(a,catalog+24,next);return !a->memory.fault;
}
typedef struct PlayerSpawn {GameApplication *app;GameAddress object;} PlayerSpawn;
static int spawn_height(void *context,float x,float z,float *detail,long double *terrain){
    PlayerSpawn *s=context;
    *detail=(float)gameplay_geometry_detail_height(&s->app->memory,x,z);
    *terrain=gameplay_geometry_terrain_height(&s->app->memory,x,z);return !s->app->memory.fault;
}
static float spawn_sine(void *context,unsigned phase){PlayerSpawn *s=context;return real(s->app,0x6298a8+(phase&2047u)*4);}
static uint32_t spawn_integer(long double value){
    return (uint32_t)(uint64_t)(!isfinite(value) || value< -0x1p63L || value>=0x1p63L?INT64_MIN:(int64_t)value);
}
static int spawn_clear(void *context,const float position[3],int *clear){
    PlayerSpawn *s=context;GameApplication *a=s->app;uint32_t x,z,index;
    x=spawn_integer(position[0])&0xfffffe3fu;z=spawn_integer(position[2])&0xfffffe00u;
    index=(x>>6u)|((x&0x80000000u)?0xfc000000u:0u);index+=z*4u;
    if(game_memory_read_u8(&a->memory,get(a,0x597d90)+index+5)&0x40u){*clear=0;return !a->memory.fault;}
    return !a->memory.fault && gameplay_geometry_position_clear(&a->memory,s->object,position,clear);
}
static int spawn_resize(void *app,uint32_t diameter){return game_level_display_mask_resize(app,diameter);}
/*4f6bb0: reuse the dead player's model/rig allocation, choose the original
 *clear spawn rings, rebuild its owned children and restore surviving ammo.
 *Camera, display masks and queued speech keep their established native owners. */
int game_level_player_respawn(GameApplication *a,GameAddress player,const float requested[3],uint16_t heading){
    PlayerSpawn context={a,player};PlayerSpawnServices services={&context,spawn_height,spawn_clear,spawn_sine};
    GameDisplayMaskServices display={a,spawn_resize};GameAddress catalog,child,local;
    float position[3];uint16_t angles[3]={heading,0,0};uint32_t identity;unsigned i;int accepted;
    int16_t sounds[8]={127,93,0,0,-1,-1,-1,-1};
    if(!a || !player || !requested || a->memory.fault || get(a,get(a,player)+4)!=0x4f4f60)return 0;
    if(!player_spawn_choose(requested,real(a,player+0x30),real(a,0x574d90),&services,position))return 0;
    catalog=get(a,0x58973c);
    if(!catalog || !sequence(a,catalog,&identity) || !game_level_player_scene_place(a,player,identity,position,angles))return 0;
    for(i=0;i<13;i++){
        child=get(a,player+0x1dc+i*4);
        if(child)put(a,child+12,get(a,player+0x3f0+i*4));
    }
    local=get(a,0x5f0208);if(a->memory.fault)return 0;
    if(get(a,local+0x3bc)!=player)return !a->memory.fault;
    if(!game_camera_activate(&a->memory,get(a,0x58f89c),NULL) || !game_application_options_load(a,1) ||
       !game_display_configure_control(&a->memory,0,&display))return 0;
    sounds[2]=(int16_t)game_presentation_location_token(&a->memory,real(a,player+24),1);
    sounds[3]=(int16_t)game_presentation_location_token(&a->memory,real(a,player+32),0);
    if(a->memory.fault || !game_menu_sound_sequence(a,sounds,&accepted))return 0;
    if(accepted){game_memory_write_u16(&a->memory,local+0x598,127);put(a,local+0x58c,get(a,0x5ac6ac));}
    return !a->memory.fault;
}
static int publish(GameApplication *a,GameAddress catalog,GameAddress object){uint32_t flags=get(a,object+16),slot=(flags&1)?((flags&64)?0x80:0x1c):0xe4;
    if(a->memory.fault || !level_player_slots_attach_scene_member(catalog,object,slot))return 0;
    game_list_insert_after(&a->memory,object,catalog+slot,0x48,0x4c);
    if(a->memory.fault){level_player_slots_forget_scene_member(object);return 0;}return 1;
}
static int height(void *c,float x,float z,long double *out){Relocation *e=c;*out=gameplay_geometry_terrain_height(&e->app->memory,x,z);return !e->app->memory.fault;}
static int destination(void *c,float out[2]){Relocation *e=c;memcpy(out,e->destination,8);return 1;}
static int clear(void *c,const float p[3],int *out){Relocation *e=c;float limits[3];unsigned i;
    for(i=0;i<3;i++)limits[i]=real(e->app,0x57217c+i*4);
    return !e->app->memory.fault && player_relocation_position_clear(&e->app->memory,0x5f0800,real(e->app,e->player+0x38),limits,p,out);
}
static float sine(void *c,unsigned phase){Relocation *e=c;return real(e->app,0x6298a8+phase*4);}
static int angle(void *c,int32_t *out){Relocation *e=c;GameApplication *a=e->app;uint32_t cursor=get(a,0x6308c0);long double value;
    if(cursor==1024)cursor=0;
    if(cursor>1024)return 0;
    put(a,0x6308c0,cursor+1);value=(long double)real(a,0x6308c8+cursor*4)*real(a,0x571f38);
    *out=(int32_t)(uint32_t)(uint64_t)(!isfinite(value) || value< -0x1p63L || value>=0x1p63L?INT64_MIN:(int64_t)value);return !a->memory.fault;
}
static int transition_height(void *c,float x,float z,long double *out){Arrival *e=c;*out=gameplay_geometry_terrain_height(&e->app->memory,x,z);return !e->app->memory.fault;}
static int transition_place(void *c,uint32_t id,const float p[3],uint16_t angle){Arrival *e=c;return game_level_object_place(e->app,e->object,id,p,angle);}
/* This constructor is specifically the relocation's mode2 arrival. The other
 *transition modes retain their required catalog/target constructor programs. */
static int start_arrival(GameApplication *a,GameAddress object,uint32_t id,const float p[3],GameAddress player){
    EffectAnimation *animation=presentation_instance_animation(object);PresentationPose *pose=presentation_instance_pose(object);EffectModel *model=pose?effect_model_find(pose->model):NULL;
    Arrival e={a,object,animation};EffectTransitionServices services={0};
    services.initialization=(EffectInitializationServices){&e,transition_height,transition_place};
    return animation && pose && model && positional_sound(a,p,14) &&
        effect_initialize_transition(&a->memory,animation,pose,model,id,p,0,player,2,0,&services);
}
/*440d80 mode1: descriptor transfers reuse the owned arrival animation. The
 *payload stays alive until441110/4412f0 retires it through its real class method. */
int game_level_descriptor_transfer(GameApplication *a,GameAddress object,uint32_t id,GameAddress payload){
    EffectAnimation *animation=presentation_instance_animation(object);
    PresentationPose *pose=presentation_instance_pose(object);
    EffectModel *model=pose?effect_model_find(pose->model):NULL;
    Arrival e={a,object,animation};EffectTransitionServices services={0};float p[3];unsigned i;
    if(!animation || !pose || !model || !payload)return 0;
    for(i=0;i<3;i++)p[i]=real(a,payload+0x18+i*4);
    services.initialization=(EffectInitializationServices){&e,transition_height,transition_place};
    return !a->memory.fault && positional_sound(a,p,15) &&
        effect_initialize_transition(&a->memory,animation,pose,model,id,p,0,payload,1,0,&services);
}
typedef struct ProducedArrival {Arrival arrival;float position[3];uint16_t heading;} ProducedArrival;
static int produced_sequence(void *c,GameAddress root,const float p[3],uint16_t heading,uint32_t *id){
    ProducedArrival *e=c;memcpy(e->position,p,12);e->heading=heading;return sequence(e->arrival.app,root,id);
}
static int produced_bind(void *c,GameAddress target,uint32_t id){ProducedArrival *e=c;
    uint16_t angles[3]={e->heading,0,0};return game_level_object_place_angles(e->arrival.app,target,id,e->position,angles);
}
static int produced_publish(void *c,GameAddress root,GameAddress target){ProducedArrival *e=c;return publish(e->arrival.app,root,target);}
static int produced_delegate(void *c,GameAddress root,GameAddress target){ProducedArrival *e=c;return game_level_arrival_delegate_create(e->arrival.app,root,target);}
static int produced_lookup(void *c,GameAddress root,uint32_t identity,GameAddress *result){
    ProducedArrival *e=c;GameApplication *a=e->arrival.app;
    return gameplay_catalog_lookup(&a->memory,root,get(a,get(a,root)+4),identity,NULL,NULL,result)==1;
}
/*53e7f0 stores three pending arrivals plus a fourth overflow slot. */
static int produced_register(void *c,GameAddress station,GameAddress target){
    ProducedArrival *e=c;GameApplication *a=e->arrival.app;unsigned slot=0;
    GameAddress descriptor=get(a,station+0x294);
    if(!descriptor || get(a,descriptor)!=0x576c68)return 0;
    while(slot<3 && get(a,descriptor+0x50+slot*4))slot++;
    put(a,descriptor+0x50+slot*4,target);return !a->memory.fault;
}
/*440d80 mode0: publish through the existing arrival owner. A station request
 *resolves its live identity and registers the unit in that descriptor; missing
 *or retired stations return the unit to its existing factory pool. */
int game_level_arrival_produce(GameApplication *a,GameAddress object,uint32_t id,const float p[3],GameAddress payload){
    return game_level_arrival_produce_requested(a,object,id,p,payload,0,0);
}
int game_level_arrival_produce_requested(GameApplication *a,GameAddress object,uint32_t id,const float p[3],GameAddress payload,uint32_t request,uint16_t heading){
    EffectAnimation *animation=presentation_instance_animation(object);PresentationPose *pose=presentation_instance_pose(object);
    EffectModel *model=pose?effect_model_find(pose->model):NULL;ProducedArrival e={{a,object,animation},{0},0};
    EffectTransitionServices services={0};
    if(!animation || !pose || !model || !payload)return 0;
    services.initialization=(EffectInitializationServices){&e,transition_height,transition_place};
    services.sequence=produced_sequence;services.bind=produced_bind;
    services.publish=produced_publish;services.delegate=produced_delegate;
    services.lookup=produced_lookup;services.register_target=produced_register;
    return positional_sound(a,p,15) && effect_initialize_transition(&a->memory,animation,pose,model,id,p,heading,payload,0,request,&services);
}
int game_level_player_relocate(GameApplication *a,GameAddress player,const float requested[2]){
    PresentationPose *pose=presentation_instance_pose(player);PlayerControls *controls=presentation_instance_player_controls(player);
    GameAddress catalog,effect,arrival;uint32_t id;EffectInitialization departure={0};float p[3];Relocation e={a,player,requested};
    PlayerRelocationServices services={&e,height,destination,clear,angle,sine};
    if(!pose || !controls || a->memory.fault)return 0;
    catalog=get(a,0x5f0210+(pose->identity&63)*4);effect=game_level_factory_take(a,0x41,0);if(!effect)return 0;
    departure.subtype=game_memory_read_u8(&a->memory,player+12);departure.profile=effect_initialization_profile(&a->memory,5);
    departure.scale[0]=departure.scale[1]=(float)((long double)pose->radius*real(a,0x574d64));departure.radius=100;departure.mark_kind=-1;
    memcpy(departure.position,pose->position,12);
    if(!game_level_effect_emit(a,catalog,effect,&departure)){(void)game_level_factory_destroy(a,effect);return 0;}
    /* Initialization clamps the borrowed player point before the search. */
    memcpy(pose->position,departure.position,12);
    if(!positional_sound(a,departure.position,15))return 0;
    arrival=game_level_factory_take(a,0x45,0);if(!arrival)return 0;
    if(!player_relocation_choose(pose,real(a,0x574e18),&services,p) || !sequence(a,catalog,&id) ||
       !start_arrival(a,arrival,id,p,player) || !publish(a,catalog,arrival)){
        (void)game_level_factory_destroy(a,arrival);return 0;
    }
    controls->mode_flags|=1;return !a->memory.fault;
}
static int identity(void *c,GameAddress target,uint32_t *out){Arrival *e=c;*out=get(e->app,target+20);return !e->app->memory.fault;}
static int clear_flag(void *c,GameAddress target,uint32_t flag){Arrival *e=c;put(e->app,target+16,get(e->app,target+16)&~flag);return !e->app->memory.fault;}
static int health(void *c,GameAddress target,int32_t *out){Arrival *e=c;*out=(int32_t)get(e->app,target+0x58);return !e->app->memory.fault;}
static int arrive(void *c,GameAddress player,const float p[3]){Arrival *e=c;GameApplication *a=e->app;
    PresentationPose *pose=presentation_instance_pose(player);PlayerControls *controls=presentation_instance_player_controls(player);PlayerRig *rig=presentation_instance_player_rig(player);uint32_t table,method;unsigned i;
    if(get(a,0x5f0210+(get(a,player+20)&63)*4)!=get(a,0x5f0208))return !a->memory.fault;
    if(!pose || !controls || !rig)return 0;
    memcpy(pose->local_position,p,12);table=get(a,player);
    for(i=0;i<2;i++){method=get(a,table+0x90+i*4);
        if(method==0x4f5b20 || method==0x4f5e60){if(!player_rig_build(&a->memory,pose,controls,rig,method==0x4f5b20))return 0;}
        else if(method==0x4f6470 || method==0x4f6730){if(!player_transform_vertex_ranges(&a->memory,pose,rig,method==0x4f6730))return 0;}
        else return 0;
    }
    controls->mode_flags|=2;return !a->memory.fault;
}
static int remove_target(void *c,GameAddress target){Arrival *e=c;GameApplication *a=e->app;uint32_t slot=0x20,flags=get(a,target+16),method;
    /*441140's type table lives in the original executable's text section,
     *which the portable game does not map. Select its actual native families;
     *reading absent instruction data chose the primary-vehicle slot for every
     *station and called an unrelated geometry method during factory departure. */
    if(flags&0x100){uint8_t type=game_memory_read_u8(&a->memory,target+12);
        if(type==0x23)slot=0x200;
        else if(type>=0x80 && type<=0x87)slot=0x84;
        else if(type==0x88 || type==0x89)slot=0x9c;
        else return 0;}
    method=get(a,get(a,target)+slot);if(a->memory.fault)return 0;
    return game_level_scene_reset(a,target,method);
}
static int mark_removed(void *c,uint32_t id){Arrival *e=c;GameApplication *a=e->app;GameAddress node=get(a,0x605868);unsigned visits=0;(void)id;
    while(node && !a->memory.fault){if(++visits>65536)return 0;
        if(get(a,node+0x24)==e->animation->arrival.identity)game_memory_write_u8(&a->memory,node+0x2c,game_memory_read_u8(&a->memory,node+0x2c)|1);
        node=get(a,node+0x28);}
    return !a->memory.fault;
}
static int finish(GameApplication *a,GameAddress object){EffectAnimation *animation=presentation_instance_animation(object);PresentationPose *pose=presentation_instance_pose(object);Arrival e={a,object,animation};EntityArrivalServices services={&e,identity,clear_flag,remove_target,mark_removed,health,arrive};
    return animation && pose && entity_arrival_finish(&animation->arrival,pose->position,&services);
}
int game_level_arrival_reset(GameApplication *a,GameAddress object){EffectAnimation *animation=presentation_instance_animation(object);
    if(!animation || (animation->arrival.target && !finish(a,object)))return 0;
    animation->arrival.identity=0;return game_level_presentation_reset(a,object,1);
}
int game_level_arrival_update(GameApplication *a,GameAddress object){EffectAnimation *animation=presentation_instance_animation(object);
    if(!animation || (get(a,0x5ac6ac)-animation->birth_tick==18 && !finish(a,object)))return 0;
    return game_level_animation_update(a,object);
}
