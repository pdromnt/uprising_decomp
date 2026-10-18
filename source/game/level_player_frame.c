#include "level_player_scene.h"
#include "level_player_resources.h"
#include "level_run.h"
#include "level_factory.h"
#include "level_effect_runtime.h"
#include "presentation_instance.h"
#include "player_frame.h"
#include "player_terrain.h"
#include "entity_vertex_transform.h"
#include "gameplay_spatial.h"
#include "gameplay_geometry.h"
#include "gameplay_leaves.h"
#include "game_intrusive_list.h"
#include "game_matrix.h"
#include "scene_projection_storage.h"
#include "menu_sound.h"
#include "gameplay_messages.h"
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

/* Required ordinary C programs, not CPU fallback. Stations and the immediate
 * death recipe remain unresolved until their actual behavior is recovered. */
extern int game_level_player_death_effect(GameApplication *,GameAddress,GameAddress);
extern int game_level_player_station(GameApplication *,GameAddress,unsigned);
extern int game_level_scene_contacts(GameApplication *,GameAddress,GameAddress,const float[3],int *);
extern int game_level_contact_response(GameApplication *,GameAddress,GameAddress,GameAddress,const uint32_t[3],const float[3]);
extern int game_input_queue_remove(GameApplication *,GameAddress,GameAddress,GameAddress);
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
static float real(GameApplication *a,GameAddress p){uint32_t bits=get(a,p);float v;memcpy(&v,&bits,4);return v;}
static uint32_t truncate_value(long double v){return (uint32_t)(uint64_t)(!isfinite(v) || v< -0x1p63L || v>=0x1p63L?INT64_MIN:(int64_t)v);}
static int positional_sound(GameApplication *a,GameAddress o,int16_t id){
    SoundPlaybackRequest request={id,(float)game_leaf_horizontal_distance_squared(&a->memory,o+24,get(a,0x58f890)),1,0,0,0};
    return !a->memory.fault && game_menu_sound_play(a,&request);
}
static int unlink_player(void *c,GameAddress o){GameApplication *a=c;game_list_detach_node(&a->memory,o,4,8);return !a->memory.fault;}
static int insert_player(void *c,GameAddress o){GameApplication *a=c;float x=real(a,o+24),z=real(a,o+32);
    if(!isfinite(x) || !isfinite(z) || x<0 || z<0 || x>=131072 || z>=131072)return 0;
    (void)gameplay_spatial_insert(&a->memory,0x5f0800,o,(int32_t)x/4096,(int32_t)z/4096,(get(a,o+16)&0x400)!=0);
    return !a->memory.fault;
}
static int refresh(GameApplication *a,GameAddress o,GameAddress method){
    PresentationPose *pose=presentation_instance_pose(o);PlayerRig *rig=presentation_instance_player_rig(o);PlayerControls *controls=presentation_instance_player_controls(o);
    if(!pose || !rig || !controls)return 0;
    if(method==0x4f5b20 || method==0x4f5e60)return player_rig_build(&a->memory,pose,controls,rig,method==0x4f5b20);
    if(method==0x4f6470 || method==0x4f6730)return player_transform_vertex_ranges(&a->memory,pose,rig,method==0x4f6730);
    return 0;
}
static int sample(void *c,GameAddress o){GameApplication *a=c;PresentationPose *pose=presentation_instance_pose(o);PlayerControls *controls=presentation_instance_player_controls(o);PlayerRig *rig=presentation_instance_player_rig(o);
    return pose && controls && rig && player_rig_sample_pose(&a->memory,pose,controls,rig);
}
/*4f2100: commit clamped pose/deltas, refresh geometry and resolve contacts in
 *the retained order. Death finalization owns the chain if a response kills us. */
int game_level_player_position_finalize(GameApplication *a,GameAddress o,uint32_t *alive){
    GamePlayerPositionFinalizePlan plan=game_player_plan_position_finalize(game_player_ref(&a->memory,o));
    GameAddress record;int hit;float position[3];unsigned i;
    if(!game_player_commit_position_finalize(game_player_ref(&a->memory,o),&plan))return 0;
    if(!refresh(a,o,get(a,plan.vtable+8)) || !refresh(a,o,get(a,plan.vtable+12)))return 0;
    for(i=0;i<3;i++)position[i]=real(a,o+24+i*4);
    if(!game_level_scene_contacts(a,o,get(a,plan.vtable+0x78),position,&hit))return 0;
    plan.first_record=get(a,o+0xb0);record=plan.first_record;
    while(record && !a->memory.fault){
        EffectResource *owned=effect_resource_find(record);GameAddress other=owned?owned->contact:get(a,record+12);
        float point[3];uint32_t velocity[3]={0,0,0};
        if(owned)memcpy(point,owned->contact_vector,12);else for(i=0;i<3;i++)point[i]=real(a,record+16+i*4);
        if(get(a,other+16)&2)for(i=0;i<3;i++)velocity[i]=get(a,other+0xb4+i*4);
        if(!game_level_contact_response(a,o,other,get(a,plan.vtable+0x2c),velocity,point))return 0;
        memset(velocity,0,12);
        if(!game_level_contact_response(a,other,o,get(a,get(a,other)+0x2c),velocity,point))return 0;
        if(!get(a,o+0x58)){*alive=0;return !a->memory.fault;}
        record=owned?(owned->next?owned->next->projection:0):get(a,record+4);
    }
    if(plan.first_record){EffectResourcePool *pool=effect_resource_pool_find(0x631910);
        if(!pool || effect_resource_pool_return_chain(&a->memory,pool,get(a,o+0xb0))<=0)return 0;
        put(a,o+0xb0,0);
    }
    *alive=1;return !a->memory.fault;
}
typedef struct Delivery {GameApplication *app;GameAddress player,table;} Delivery;
static int catalog(void *c,unsigned method,unsigned item,unsigned variant,int32_t *out){Delivery *e=c;uint32_t v;GameAddress world=get(e->app,0x58973c);
    int ok=method?game_level_player_catalog_mount(e->app,world,(uint8_t)item,0x20,variant,&v):game_level_player_catalog_value(e->app,world,0x20,item,variant,&v);
    if(!ok)return 0;
    memcpy(out,&v,4);return 1;
}
static int feedback(void *c,unsigned enabled){Delivery *e=c;GameAddress text=enabled?0x596a40:0x596a60;
    return game_message_enqueue(e->app,get(e->app,0x628a88),text,1);
}
static int delivery_refresh(void *c,unsigned operation){Delivery *e=c;GameApplication *a=e->app;
    if(operation<3){static const unsigned slots[3]={8,0x90,0x94};if(!operation)e->table=get(a,e->player);return refresh(a,e->player,get(a,e->table+slots[operation]));}
    {PresentationPose *pose=presentation_instance_pose(e->player);PlayerControls *controls=presentation_instance_player_controls(e->player);PlayerRig *rig=presentation_instance_player_rig(e->player);
     return pose && controls && rig && player_rig_record_pose(&a->memory,pose,controls,rig,operation!=3);}
}
static int delivery_spatial(void *c,unsigned operation,int *alive){Delivery *e=c;uint32_t value;int ok;
    if(operation==0)return unlink_player(e->app,e->player);
    if(operation==2)return insert_player(e->app,e->player);
    ok=game_level_player_position_finalize(e->app,e->player,&value);if(ok && alive)*alive=(int)value;return ok;
}
static int interact(void *c,unsigned operation){Delivery *e=c;return game_level_player_station(e->app,e->player,operation);}
int game_level_player_event_deliver(GameApplication *a,GameAddress o,GameAddress node,int packet){
    Delivery context={a,o,0};PlayerEventServices services={&context,catalog,feedback,delivery_refresh,delivery_spatial,interact};
    PresentationPose *pose=presentation_instance_pose(o);PlayerControls *controls=presentation_instance_player_controls(o);EffectResource *record=effect_resource_find(node);
    if(!pose || !controls || !record)return 0;
    return packet?player_packet_apply(&a->memory,o,pose,controls,record,&services):player_event_apply(&a->memory,o,pose,controls,record,&services);
}
static int remove_event(void *c,GameMemory *m,GameAddress queue,GameAddress node){GameApplication *a=c;GameAddress method=get(a,get(a,queue)+0x18);(void)m;
    if(method==0x4f0970){EffectResource *event=effect_resource_find(node);int ok=game_level_player_event_deliver(a,queue-0x13c,node,0);
        if(!ok){PlayerRig *rig=presentation_instance_player_rig(queue-0x13c);
            fprintf(stderr,"native player event failed: deliver node=%08x code=%u mask=%u fault=%u/%08x channels=%d/%d\n",node,event?event->event_code:255,event?event->packet_mask:0,m->fault,m->fault_address,rig?rig->channels[0].active:-99,rig?rig->channels[1].active:-99);}
        return ok;}
    return game_input_queue_remove(a,queue,node,method);
}
static int release_event(void *c,GameAddress node){GameApplication *a=c;EffectResource *record=effect_resource_find(node);
    int ok=record && record->owner->object_kind>=1 && record->owner->object_kind<=3 && record->event_route==record->owner->object_kind-1 && effect_resource_pool_return(&a->memory,record->owner,node);
    if(!ok)fprintf(stderr,"native player event failed: release node=%08x kind=%u route=%u\n",node,record?record->owner->object_kind:UINT16_MAX,record?record->event_route:255);
    return ok;
}
static int health(void *c,GameMemory *m,GameAddress o,uint32_t *out){GameApplication *a=c;(void)m;return game_level_scene_property(a,o,get(a,get(a,o)+0x44),out);}
static int collisions(void *c,GameAddress o,uint32_t *alive){return game_level_player_position_finalize(c,o,alive);}
static int trunc_point(void *c,GameAddress p,uint32_t *out){GameApplication *a=c;*out=truncate_value(real(a,p));return !a->memory.fault;}
static int height(void *c,float x,float z,long double *out){GameApplication *a=c;*out=gameplay_geometry_terrain_height(&a->memory,x,z);return !a->memory.fault;}
static int below(void *c,GamePlayerRef p,long double terrain,int *out){GameApplication *a=c;long double clearance=(long double)real(a,p.address+28)-terrain-real(a,p.address+0x30),limit=real(a,0x574d6c);
    *out=isnan(clearance) || isnan(limit) || clearance<limit;return !a->memory.fault;
}
static int terrain_damage(void *c,GamePlayerRef p,GamePlayerTerrainResult *result){GameApplication *a=c;uint32_t damage;int32_t signed_damage;long double detail=gameplay_geometry_detail_height(&a->memory,real(a,p.address+24),real(a,p.address+32));
    damage=truncate_value(detail/((long double)real(a,p.address+0x30)*real(a,0x574d70))*real(a,0x574d74))-get(a,p.address+0x5c);
    memcpy(&signed_damage,&damage,4);if(signed_damage<=1)damage=1;
    if(a->memory.fault || !game_level_player_apply_damage(a,p.address,damage))return 0;
    if((int32_t)get(a,p.address+0x58)>0)return !a->memory.fault;
    if(get(a,get(a,p.address)+0x10)!=0x4f6f90 || !game_level_player_death_effect(a,p.address,0) || !game_level_player_finalize(a,p.address))return 0;
    *result=GAME_PLAYER_TERRAIN_PLAYER_DIED;return 1;
}
static int terrain_effect(void *c,GamePlayerRef p,GameAddress unused){GameApplication *a=c;GameAddress record=get(a,get(a,p.address+0x54)+0x50),points=get(a,record);int32_t count=(int32_t)get(a,record+20),tick=(int32_t)get(a,0x5ac6ac);float local[3],matrix[12],world[3];unsigned i;int emitted;(void)unused;
    if(count<=0)return 0;
    points+=(uint32_t)(tick%count)*12u;
    for(i=0;i<3;i++)local[i]=real(a,points+i*4);
    for(i=0;i<12;i++)matrix[i]=real(a,p.address+0x80+i*4);
    game_matrix_transform_point_values(world,matrix,local);
    if(a->memory.fault || !game_level_surface_particles_result(a,p.address,world,6,0x6297a0,&emitted))return 0;
    return !emitted || positional_sound(a,p.address,4);
}
/*4f13c0 uses owned records throughout. Only the weapon matrix API needs a
 *bounded transient workspace; no register frame or emulated stack is created. */
int game_level_player_frame(GameApplication *a,GameAddress o){
    uint32_t death,tick,age;GameAddress child,workspace;GamePlayerFrameGate gate;GamePlayerTerrainResult terrain=GAME_PLAYER_TERRAIN_CONTINUE;int ok;
    GamePlayerFrameCallbacks callbacks={a,health,unlink_player,sample,collisions,insert_player,{a,remove_event,release_event}};
    GamePlayerTerrainOps hazard={a,NULL,trunc_point,height,below,terrain_damage,terrain_effect};
    if(!a || a->memory.fault)return 0;
    if(!get(a,o+0x58))return !a->memory.fault;
    death=get(a,o+0x498);tick=get(a,0x5ac6ac);age=tick-death;
    if(death!=UINT32_MAX && age==93){EffectInitialization request={0};float scale=(float)((long double)real(a,o+0x38)*real(a,0x574d64));unsigned i;
        child=game_level_factory_take(a,0x41,0);if(!child)return 0;
        request.attachment=o;request.subtype=game_memory_read_u8(&a->memory,o+12);request.profile=effect_initialization_profile(&a->memory,5);
        request.scale[0]=request.scale[1]=scale;request.radius=100;request.mark_kind=-1;
        for(i=0;i<3;i++)request.position[i]=real(a,o+24+i*4);
        if(!game_level_effect_emit(a,get(a,0x58973c),child,&request)){(void)game_level_factory_destroy(a,child);return 0;}
        /*The initializer can clamp the supplied position in retail.*/
        for(i=0;i<3;i++){uint32_t bits;memcpy(&bits,request.position+i,4);put(a,o+24+i*4,bits);}
        if(!positional_sound(a,o,15))return 0;
    }
    if(death!=UINT32_MAX && age==111)return game_level_player_finalize(a,o);
    gate=game_player_prepare_frame(game_player_ref(&a->memory,o),&callbacks);
    if(gate!=GAME_PLAYER_FRAME_CONTINUE){
        if(gate!=GAME_PLAYER_FRAME_STOP)fprintf(stderr,"native player frame failed: prepare\n");
        return gate==GAME_PLAYER_FRAME_STOP;
    }
    if(!game_player_update_terrain_hazard(game_player_ref(&a->memory,o),0,&hazard,&terrain)){fprintf(stderr,"native player frame failed: terrain hazard\n");return 0;}
    if(terrain!=GAME_PLAYER_TERRAIN_CONTINUE)return 1;
    if(!(get(a,o+0x140)&0x30000u))return !a->memory.fault;
    workspace=scene_projection_storage_allocate(96,"player firing matrices");if(!workspace)return 0;
    ok=game_level_player_scene_update_weapons(a,o,workspace,workspace+48);
    if(!ok)fprintf(stderr,"native player frame failed: weapons\n");
    return scene_projection_storage_free(workspace) && ok;
}
