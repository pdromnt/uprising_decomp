#include "level_objects.h"
#include "level_run.h"
#include "level_routes.h"
#include "level_effect_runtime.h"
#include "level_factory.h"
#include "game_initializers.h"
#include "gameplay_catalog.h"
#include "gameplay_geometry.h"
#include "gameplay_spatial.h"
#include "gameplay_state.h"
#include "scene_instance_owner.h"
#include "presentation_pose.h"
#include "presentation_instance.h"
#include "game_skin_transform.h"
#include "level_player_scene.h"
#include "attachment_effect.h"
#include "sprite_fragment.h"
#include "spatial_sample.h"
#include "presentation_state_copy.h"
#include "effect_model.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
static float value(GameApplication *a,GameAddress p){uint32_t b=get(a,p);float f;memcpy(&f,&b,4);return f;}
static void store(GameApplication *a,GameAddress p,float f){uint32_t b;memcpy(&b,&f,4);put(a,p,b);}
static int ground_prepare(void *c,GameAddress object,float p[3],uint16_t heading){GameApplication *a=c;
    return get(a,get(a,object)+0x68)==0x42ed30 && game_level_object_footprint(a,object,p,heading);}
static int ground_place(void *c,GameAddress object,uint32_t id,const float p[3],const uint16_t angles[3]){
    return game_level_scene_object_place(c,object,id,p,angles);}
static int spatial(GameApplication *a,GameAddress o,const float p[3]) {
    GameAddress bucket;uint8_t kind=game_memory_read_u8(&a->memory,o+12);
    if(!isfinite(p[0]) || !isfinite(p[2]) || p[0]<-2147483648.0f || p[0]>=2147483648.0f ||
       p[2]<-2147483648.0f || p[2]>=2147483648.0f || !gameplay_spatial_query_group(0x5f0800,p[0],p[2],&bucket))return 0;
    (void)gameplay_spatial_insert(&a->memory,0x5f0800,o,(int32_t)p[0]>>12,(int32_t)p[2]>>12,
        (get(a,o+0x10)&0x400)!=0 && kind!=9 && kind!=12 && kind!=0xa0);
    return !a->memory.fault;
}
/*42d420/4a6610/4a6740. Small 64h/88h scene records have no matrix at80h;
 *their model owns it. Never import/export a larger presentation view. */
int game_level_scene_object_world(GameApplication *a,GameAddress o){
    GameAddress model=get(a,o+0x54);SceneInstance *instance;uint16_t angles[3];float p[3],matrix[12];unsigned i;
    if(!model || a->memory.fault)return 0;
    for(i=0;i<3;i++){angles[i]=game_memory_read_u16(&a->memory,o+0x24+i*2);p[i]=value(a,o+0x18+i*4);}
    gameplay_geometry_orientation_values(&a->memory,angles,p,matrix);if(a->memory.fault)return 0;
    instance=scene_instance_owner_find(model);
    if(instance){memcpy(instance->local,matrix,sizeof(matrix));return scene_projection_storage_project_holder(instance);}
    {EffectModel *effect=effect_model_find(model);if(!effect)return 0;memcpy(effect->local,matrix,sizeof(matrix));}
    return 1;
}
int game_level_scene_object_place(GameApplication *a,GameAddress o,uint32_t id,const float p[3],const uint16_t input_angles[3]) {
    /*42d420 dispatches the object's world/vertex programs. Spawn meshes use
     *the same placement/property/spatial algorithm with their native overrides. */
    if(get(a,o)==0x572038)return game_level_object_place_common(a,o,id,p,input_angles,0);
    GameMemory *m=&a->memory;GameAddress model=get(a,o+0x54);
    const char *stage="model binding";
    uint16_t angles[3];unsigned i;
    if(!model || m->fault)goto fail;
    put(a,o+0x14,id);game_initializer_00413690(m,model,get(a,0x5f0210+(id&63u)*4)+0x10);
    for(i=0;i<3;++i){angles[i]=input_angles[i]&2047u;store(a,o+0x18+i*4,p[i]);game_memory_write_u16(m,o+0x24+i*2,angles[i]);}
    if(!game_level_scene_object_world(a,o))goto fail;
    stage="vertex transform";
    {GameAddress transform=get(a,get(a,o)+12);
        /*482390 extends4a6740 with skin planes,aim points and spawn slots.
         *Running only its prefix leaves cloned generator target data at zero. */
        if(transform==0x482390){if(!game_presentation_transform(m,o))goto fail;}
        else if(transform==0x4a6740){if(!game_presentation_transform_vertices(m,o))goto fail;}
        else goto fail;
    }
    stage="catalog properties";
    for(i=0;i<3;++i){uint32_t property;
        if(!game_level_scene_property(a,o,get(a,get(a,o)+0x44+i*4),&property)){
            fprintf(stderr,"native placement property failed: slot=%u method=%08x\n",i,get(a,get(a,o)+0x44+i*4));goto fail;}
        put(a,o+0x58+i*4,property);}
    stage="spatial insertion";if(m->fault || !spatial(a,o,p))goto fail;
    return 1;
fail:
    fprintf(stderr,"native scene placement failed: %s object=%08x model=%08x position=(%g,%g,%g)\n",
        stage,(unsigned)o,(unsigned)model,(double)p[0],(double)p[1],(double)p[2]);return 0;
}
typedef struct Placement {GameApplication *a;GameAddress object;} Placement;
/*434780: the emitted ground object owns recipe/tick/class fields and places
 *its native model at actual terrain height. It has no projectile pose view. */
int game_level_position_shot_place(GameApplication *a,GameAddress object,uint32_t identity,
        const float input[3],const uint16_t angles[3],GameAddress recipe,uint8_t owner_class){
    float p[3];memcpy(p,input,sizeof(p));
    p[1]=gameplay_geometry_terrain_height(&a->memory,p[0],p[2]);
    put(a,object+0x64u,recipe);put(a,object+0x68u,get(a,0x5ac6acu));
    game_memory_write_u8(&a->memory,object+0x6cu,owner_class);
    return !a->memory.fault && game_level_scene_object_place(a,object,identity,p,angles);
}
static int property(void *c,unsigned i,uint32_t *v){Placement *p=c;
    const PresentationInstance *instance=presentation_instance_find(p->object);
    if(i==0 && instance && instance->has_attachment){*v=attachment_effect_detail(&p->a->memory,p->object);return !p->a->memory.fault;}
    return game_level_scene_property(p->a,p->object,get(p->a,get(p->a,p->object)+0x44+i*4),v);}
static int insert(void *c){Placement *p=c;float v[3];unsigned i;for(i=0;i<3;++i)v[i]=value(p->a,p->object+0x18+i*4);return spatial(p->a,p->object,v);}
static int laser_refresh(void *c,unsigned operation){
    Placement *p=c;PresentationPose *pose=presentation_instance_pose(p->object);
    if(!pose)return 0;
    return operation==0u?presentation_pose_build_laser_world(&p->a->memory,pose):
        operation==1u?presentation_pose_build_laser_controller(&p->a->memory,pose):
        operation==2u?presentation_pose_transform_laser_vertices(&p->a->memory,pose):0;
}
static int attachment_refresh(void *c,unsigned operation){
    Placement *p=c;
    return operation==0?attachment_effect_present(&p->a->memory,p->object):
        operation==1?attachment_effect_controller(&p->a->memory,p->object):0;
}
static int spawn_refresh(void *c,unsigned operation){
    Placement *p=c;GameMemory *m=&p->a->memory;PresentationPose *pose=presentation_instance_pose(p->object);
    int falling=(game_memory_read_u8(m,p->object+0xdc)&1)!=0,ok;
    if(!pose || m->fault)return 0;
    ok=operation==0?presentation_pose_build_spawn_world(m,pose,falling):
        operation==1?presentation_pose_build_spawn_controller(m,pose,falling):
        operation<4?presentation_pose_transform_vertices(m,pose,operation==3):0;
    return ok && presentation_pose_export(m,p->object,pose);
}
static int effect_refresh(void *c,unsigned operation){
    Placement *p=c;PresentationPose *pose=presentation_instance_pose(p->object);
    if(!pose)return 0;
    if(game_memory_read_u8(&p->a->memory,p->object+12)==5)
        return operation==0?fragment_build_world(&p->a->memory,p->object):operation==1?fragment_build_model(&p->a->memory,p->object):0;
    if(operation==0){presentation_pose_build_billboard(pose);return 1;}
    return operation==1?presentation_pose_build_billboard_controller(&p->a->memory,pose):0;
}
static int mole_refresh(void *c,unsigned operation){Placement *p=c;
    return operation==1 && game_level_mole_controller(p->a,p->object);
}
static int gate_refresh(void *c,unsigned operation){Placement *p=c;return operation==1 && game_level_gate_controller(p->a,p->object);}
static int multipart_refresh(void *c,unsigned operation){Placement *p=c;GameMemory *m=&p->a->memory;
    if(get(p->a,p->object)==0x572c78){
        if(operation==0)return presentation_instance_articulated_present(m,p->object);
        if(operation==1)return presentation_instance_articulated_render(m,p->object);
        if(operation==2)return presentation_instance_articulated_vertices(m,p->object);
        return operation==3 && presentation_instance_articulated_render_vertices(m,p->object);
    }
    if(get(p->a,p->object)!=0x5753b0)return 0;
    return operation<2?spatial_sample_frame(m,p->object,(int)operation):
        operation<4?spatial_sample_vertices(m,p->object,operation==3):0;
}
/*Archive pose refresh shares the actual placement programs, without resetting
 *health, identity, recipe or publishing a second spatial-list insertion. */
int game_level_object_archive_frames(GameApplication *a,GameAddress o,int rendered){
    Placement context={a,o};GameAddress table=get(a,o);unsigned operation=rendered?1u:0u;
    uint32_t world=get(a,table+8),controller=get(a,table+0x90);PresentationPose *pose;
    int (*program)(void *,unsigned)=NULL;int vertices=1;
    if(table==0x5737e0)return game_level_gate_frame(a,o,rendered);
    if(table==0x572c78 || table==0x5753b0)program=multipart_refresh;
    else if(world==0x419ee0 && controller==0x41a050)program=spawn_refresh;
    else if(world==0x430b10 && controller==0x430c90){program=laser_refresh;vertices=!rendered;}
    else if(world==0x555cb0 && controller==0x555ec0){program=attachment_refresh;vertices=0;}
    else if((world==0x551490 && controller==0x5514f0) ||
            (world==0x538130 && controller==0x538190) ||
            (world==0x43d660 && controller==0x43d760)){program=effect_refresh;vertices=0;}
    if(program)return program(&context,operation) && (!vertices || program(&context,rendered?3u:2u));
    if(world==0x535260 && controller==0x5353a0){
        pose=presentation_instance_pose(o);if(!pose)return 0;
        return (rendered?presentation_pose_build_controller(&a->memory,pose):presentation_pose_build_world(&a->memory,pose)) &&
            presentation_pose_transform_vertices(&a->memory,pose,rendered) && presentation_pose_export(&a->memory,o,pose);
    }
    if(!rendered && (world==0x42d370 || world==0x4a6610)){
        uint32_t transform=get(a,table+12);
        if(!game_level_scene_object_world(a,o))return 0;
        return (transform==0x482390?game_presentation_transform(&a->memory,o):
            transform==0x4a6740?game_presentation_transform_vertices(&a->memory,o):0) && !a->memory.fault;
    }
    return 0;
}
int game_level_object_place_common(GameApplication *a,GameAddress o,uint32_t id,const float p[3],const uint16_t angles[3],int controller) {
        PresentationPose pose;PresentationPlacement request={0};Placement context={a,o};
        PresentationPlacementServices services={&context,0,NULL,property,insert,0};
        int multipart=get(a,o)==0x572c78 || get(a,o)==0x5753b0;
        int gate=get(a,o)==0x5737e0;
        int spawn=get(a,o)==0x572038 && get(a,get(a,o)+8)==0x419ee0 &&
            get(a,get(a,o)+0x90)==0x41a050;
        int mole=get(a,o)==0x573fb8 && get(a,get(a,o)+8)==0x535260 &&
            get(a,get(a,o)+12)==0x5354d0 && get(a,get(a,o)+0x90)==0x48eb00 && get(a,get(a,o)+0x94)==0x5355d0;
        int laser=get(a,get(a,o)+8)==0x430b10 && get(a,get(a,o)+12)==0x430dd0 &&
            get(a,get(a,o)+0x90)==0x430c90 && get(a,get(a,o)+0x94)==0x5355d0;
        int attachment=get(a,get(a,o)+8)==0x555cb0 && get(a,get(a,o)+12)==0x5354d0 &&
            get(a,get(a,o)+0x90)==0x555ec0 && get(a,get(a,o)+0x94)==0x5355d0;
        int effect=get(a,get(a,o)+12)==0x5354d0 && get(a,get(a,o)+0x94)==0x5355d0 &&
            ((get(a,get(a,o)+8)==0x551490 && get(a,get(a,o)+0x90)==0x5514f0) ||
             (get(a,get(a,o)+8)==0x538130 && get(a,get(a,o)+0x90)==0x538190) ||
             (get(a,get(a,o)+8)==0x43d660 && get(a,get(a,o)+0x90)==0x43d760));
        if(laser){services.override_mask=7u;services.refresh_override=laser_refresh;}
        if(attachment){services.override_mask=3u;services.refresh_override=attachment_refresh;}
        if(effect){services.override_mask=3u;services.refresh_override=effect_refresh;}
        if(multipart){services.override_mask=15u;services.refresh_override=multipart_refresh;}
        if(spawn){services.override_mask=15u;services.refresh_override=spawn_refresh;}
        if(mole){services.override_mask=2u;services.refresh_override=mole_refresh;}
        if(gate){services.override_mask=2u;services.refresh_override=gate_refresh;}
        /*Only shared ordinary methods are valid here. Distinct controllers
         *remain explicit implementations with real arguments. */
        if(!multipart && !gate && !laser && !attachment && !effect && !spawn && !mole && (get(a,get(a,o)+8)!=0x535260 || get(a,get(a,o)+12)!=0x5354d0 ||
           get(a,get(a,o)+0x90)!=0x5353a0 || get(a,get(a,o)+0x94)!=0x5355d0)
           )
            {GameAddress method=get(a,get(a,o)+4);
                fprintf(stderr,"Unreconstructed placement controller %08x object=%08x\n",(unsigned)method,(unsigned)o);
                a->memory.fault=1;a->memory.fault_address=method;return 0;}
        if(!presentation_pose_import(&a->memory,o,&pose))return 0;
        request.identity=id;memcpy(request.position,p,sizeof(request.position));memcpy(request.angles,angles,sizeof(request.angles));
        request.descriptor=get(a,0x5f0210+(id&63u)*4)+0x10;request.controller=controller;
        return presentation_pose_place(&a->memory,o,&pose,&request,&services);
}
int game_level_object_place_pose(GameApplication *a,GameAddress o,uint32_t id,const float input[3],const uint16_t input_angles[3],int controller) {
    GameAddress method=get(a,get(a,o)+4);float p[3];uint16_t angles[3];
    memcpy(angles,input_angles,sizeof(angles));
    if(a->memory.fault)return 0;
    memcpy(p,input,sizeof(p));
    if(method==0x433e80){
        /*433e80 invokes the actual footprint method before42d420. Scenery's
         *572b48 selects42ed30; other position records can select the base RET. */
        GameAddress footprint=get(a,get(a,o)+0x68);
        if(footprint==0x42ed30) {
            if(!game_level_object_footprint(a,o,p,angles[0]))return 0;
        } else if(footprint!=0x42ec30)return 0;
        angles[1]=angles[2]=0;
        return game_level_scene_object_place(a,o,id,p,angles);
    }
    if(method==0x450d50)return game_level_class29_place(a,o,id,p,angles);
    if(method==0x40c9b0)return game_level_class40_place(a,o,id,p,angles);
    if(method==0x4f4f60)return game_level_player_scene_place(a,o,id,p,angles);
    if(method==0x5336f0){GamePresentationBindingServices s={a,ground_prepare,ground_place};
        if(!game_presentation_bind(&a->memory,o,id,p,angles,1,&s))return 0;
        put(a,o+0xa0,0);put(a,o+0xa4,0);return !a->memory.fault;
    }
    if(method==0x43ce60){GamePresentationBindingServices s={a,ground_prepare,ground_place};
        return game_presentation_bind(&a->memory,o,id,p,angles,1,&s);
    }
    if(method==0x5436e0)return game_level_infantry_place(a,o,id,p,angles);
    if(method==0x539a70)return game_level_entity_scene_place(a,o,id,p,angles);
    if(method==0x55a890)return game_level_aircraft_place(a,o,id,p,angles);
    if(method==0x4519f0)return game_level_carrier_place(a,o,id,p,angles);
    if(method==0x406e50 || method==0x4e63b0)return game_level_vehicle_place(a,o,id,p,angles,0,method);
    if(method==0x45fc90) {
        if(!game_level_object_footprint(a,o,p,angles[0]))return 0;
        return game_level_scene_object_place(a,o,id,p,angles);
    }
    if(method==0x42d420)return game_level_scene_object_place(a,o,id,p,angles);
    /*471a90 forwards the same five values to536250, without extra state. */
    if(method==0x536250 || method==0x471a90)return game_level_object_place_common(a,o,id,p,angles,controller);
    fprintf(stderr,"Unreconstructed object placement %08x object=%08x\n",(unsigned)method,(unsigned)o);
    a->memory.fault=1;a->memory.fault_address=method;return 0;
}
int game_level_object_place_angles(GameApplication *a,GameAddress o,uint32_t id,const float p[3],const uint16_t angles[3]){return game_level_object_place_pose(a,o,id,p,angles,1);}
/*40a1f0/40a360 ->536610: one authoritative pose plus bounded motion words.
 *The derived emitter reference/identity stays in the original owned record. */
int game_level_physical_launch(GameApplication *a,GameAddress o,uint32_t id,const float p[3],
        const uint16_t angles[3],const float velocity[3],const uint16_t spin[3],GameAddress emitter){
    GameAddress method;unsigned i;uint32_t identity;
    if(!a || a->memory.fault || !presentation_instance_pose(o))return 0;
    method=get(a,get(a,o)+0xac);
    identity=get(a,emitter+0x14);
    if(method==0x40a1f0){put(a,o+0xd4,emitter);put(a,o+0xd8,identity);}
    else if(method==0x40a360){put(a,o+0xd4,get(a,0x5ac6ac));put(a,o+0xd8,emitter);put(a,o+0xdc,identity);}
    else return 0;
    for(i=0;i<3;i++){
        store(a,o+0xb4+i*4,velocity[i]);
        game_memory_write_u16(&a->memory,o+(i==0?0xcc:i==1?0xd0:0xce),spin[i]);
    }
    if(a->memory.fault || !game_level_object_place_angles(a,o,id,p,angles))return 0;
    put(a,o+0xb0,0);return !a->memory.fault;
}
int game_level_object_place(GameApplication *a,GameAddress object,uint32_t identity,const float position[3],uint32_t packed){
    uint16_t angles[3]={(uint16_t)packed,0,0};
    return game_level_object_place_angles(a,object,identity,position,angles);
}
/*Stable method IDs select ordinary native initializers with real arguments. */
int game_level_projectile_launch(GameApplication *a,GameAddress object,uint32_t identity,
        const float position[3],const uint16_t angles[3],GameAddress recipe,GameAddress shooter,uint32_t method,float strength){
    if(method==0x48dc20)return game_level_mole_launch(a,object,identity,position,angles,recipe,shooter);
    if(method==0x4fca50 || method==0x50d760)return game_level_scaled_effect_launch(a,object,identity,position,angles,recipe,shooter);
    Projectile *state=presentation_instance_projectile(object);PresentationPose *pose=presentation_instance_pose(object);
    ProjectileServices services={a,NULL};uint32_t tick;unsigned i;
    if(!state || !pose || (method!=0x42f970u && method!=0x431000u && method!=0x431d00u &&
       method!=0x432880u && method!=0x431ee0u && method!=0x547890u && method!=0x433190u && method!=0x432350u &&
       method!=0x430250u && method!=0x432b90u))return 0;
    tick=get(a,0x5ac6acu);state->birth_tick=tick;state->recipe=recipe;state->shooter=shooter;
    state->shooter_identity=get(a,shooter+0x14u);
    if(method==0x433190u){
        uint16_t placed[3]={angles[0],0,angles[2]},bits=(uint16_t)(angles[1]-113u);int16_t pitch;
        memcpy(&pitch,&bits,2);if(pitch< -70)pitch=-70;placed[1]=(uint16_t)pitch;
        return !a->memory.fault && game_level_object_place_angles(a,object,identity,position,placed) &&
            projectile_fragment_begin(&a->memory,state,pose,recipe,shooter,1,2);
    }
    if(method==0x430250u){
        uint16_t placed[3];int16_t pitch;GameAddress trigger=get(a,shooter+0x200);
        memcpy(placed,angles,sizeof(placed));
        /*430250 jitters only the first round of this trigger interval. The
         *shared sampler preserves its wrapped cursor and low integer word. */
        if(tick-get(a,trigger+8)==get(a,recipe+0x20))
            placed[0]=(uint16_t)(placed[0]+effect_initialization_random(&a->memory,value(a,0x57232c))-17u);
        memcpy(&pitch,placed+1,2);if(pitch< -256)placed[1]=(uint16_t)-256;
        if(a->memory.fault || !game_level_object_place_angles(a,object,identity,position,placed))return 0;
        put(a,object+0xb0,0);put(a,object+0xe4,0x41b00000);put(a,object+0xe8,0);
    }else if(a->memory.fault || !game_level_object_place_angles(a,object,identity,position,angles))return 0;
    if(method==0x42f970u || method==0x430250u || method==0x432b90u){
        float speed=value(a,recipe+0x1cu);
        for(i=0;i<3u;i++)state->velocity[i]=(float)((long double)pose->world_matrix[i*4u+2u]*speed);
        memset(state->angular_velocity,0,3u*sizeof(uint16_t));
        if(method!=0x432b90u)return !a->memory.fault;
        /*432b90: the falling shot borrows a selected target's identity, or
         *starts ahead of an unaimed player. Its existing e4/e8 owner words
         *feed432e50's tracking; ec is the trail deadline, not another target. */
        {GameAddress target=0,world=get(a,0x58973c);uint32_t target_id=0;float point[3],zero[3]={0,0,0};
            memcpy(point,position,sizeof(point));
            if(game_memory_read_u8(&a->memory,shooter+12)==0x20){
                if(gameplay_state_entry_matches(&a->memory,world)){
                    target=get(a,world+0x3c4);target_id=get(a,target+20);
                    if(!presentation_target_position(&a->memory,target,pose->position,point))return 0;
                }else{
                    float dx=(float)((long double)value(a,0x6298a8+(angles[0]&2047u)*4)*value(a,0x57238c));
                    float dz=(float)((long double)value(a,0x6298a8+((angles[0]+512u)&2047u)*4)*value(a,0x57238c));
                    point[0]=(float)((long double)position[0]+dx);point[2]=(float)((long double)position[2]+dz);
                }
            }
            point[1]=(float)((long double)point[1]+value(a,0x572388));
            memcpy(&state->cell_x,&target,4);memcpy(&state->cell_z,&target_id,4);
            memcpy(pose->position,point,sizeof(point));memset(pose->angles,0,sizeof(pose->angles));
            state->velocity[0]=state->velocity[2]=0;state->velocity[1]=-speed;
            put(a,object+0xb0,0);
            if(a->memory.fault || !game_level_particle_emit(a,object,point,zero,512.0f,0x583fe8,0,0,0))return 0;
            state->target=get(a,0x5ac6ac);return !a->memory.fault;
        }
    }
    if(method==0x547890u)return projectile_begin_motion(&a->memory,state,pose,tick,recipe,shooter,angles[1]);
    if(!projectile_begin(&a->memory,state,pose,tick,recipe,shooter,&services))return 0;
    if(method==0x432350u)return projectile_begin_delayed(&a->memory,state,pose,&services);
    if(method==0x431d00u)state->split=0;
    if(method==0x431ee0u && !projectile_begin_derived(&a->memory,state,pose,
            position,angles[0],strength,&services))return 0;
    if(method==0x432880u){
        GameAddress target=0;uint32_t target_identity=0;int selected=0;
        if(game_memory_read_u8(&a->memory,shooter+12u)==0x20u){
            GameAddress world=get(a,0x58973cu);
            if(gameplay_state_entry_matches(&a->memory,world)){
                target=get(a,world+0x3c4u);target_identity=get(a,target+20u);
                selected=!get(a,0x5f0160u+(identity&63u)*24u+(target_identity&63u)*4u);
            }
        }else{target=get(a,shooter+0xf0u);target_identity=get(a,shooter+0xf4u);selected=target!=0;}
        if(selected){state->target=target;state->target_identity=target_identity;state->search_tick=state->lifetime=0x00ffffffu;
            if(!presentation_target_position(&a->memory,target,pose->position,state->target_position))return 0;}
        state->steering=5.0f;
    }
    return !a->memory.fault;
}
