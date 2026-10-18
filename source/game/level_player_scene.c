#include "level_player_scene.h"
#include "level_player_factory.h"
#include "level_player_resources.h"
#include "presentation_instance.h"
#include "presentation_commands.h"
#include "entity_vertex_transform.h"
#include "entity_queries.h"
#include "player_scene_children.h"
#include "player_runtime_pool.h"
#include "gameplay_catalog.h"
#include "gameplay_state.h"
#include "gameplay_spatial.h"
#include "level_factory.h"
#include "level_objects.h"
#include "level_run.h"
#include "level_player_slots.h"
#include "game_state_ops.h"
#include "state_helper_plans.h"
#include "menu_sound.h"
#include "player_weapons.h"
#include "game_matrix.h"
#include "vehicle_presentation.h"
#include "player_feedback.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct PlayerPlacement {GameApplication *app;GameAddress object;} PlayerPlacement;
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
static float real(GameApplication *a,GameAddress p){uint32_t bits=get(a,p);float value;memcpy(&value,&bits,4);return value;}
static int refresh(void *context,unsigned operation){
    PlayerPlacement *p=context;GameMemory *m=&p->app->memory;
    if(game_memory_read_u8(m,p->object+12)==0x40)return game_level_class40_geometry(p->app,p->object,operation);
    if((game_memory_read_u8(m,p->object+12)==0x26 || game_memory_read_u8(m,p->object+12)==0x29)){PresentationPose *pose=presentation_instance_pose(p->object);int ok;if(!pose)return 0;
        ok=operation==0?presentation_pose_build_world(m,pose):operation==1?presentation_pose_build_controller(m,pose):
            presentation_pose_transform_vertices(m,pose,operation==3);
        return ok && presentation_pose_export(m,p->object,pose);}
    if(game_memory_read_u8(m,p->object+12)==0x27)return game_level_infantry_geometry(p->app,p->object,operation);
    if(vehicle_presentation_family(game_memory_read_u8(m,p->object+12)))return vehicle_presentation_group_geometry(m,p->object,operation);
    PresentationPose *pose=presentation_instance_pose(p->object);PlayerRig *rig=presentation_instance_player_rig(p->object);
    PlayerControls *controls=presentation_instance_player_controls(p->object);
    if(!pose || !rig || !controls)return 0;
    return operation<2?player_rig_build(m,pose,controls,rig,operation==1):
        player_transform_vertex_ranges(m,pose,rig,operation==3);
}
static int property(void *context,unsigned index,uint32_t *out){
    PlayerPlacement *p=context;GameApplication *a=p->app;GameAddress catalog,record;unsigned alternate;
    if(game_memory_read_u8(&a->memory,p->object+12)==0x27 || game_memory_read_u8(&a->memory,p->object+12)==0x26 || game_memory_read_u8(&a->memory,p->object+12)==0x40){
        return index<3 && game_level_scene_property(a,p->object,get(a,get(a,p->object)+0x44+index*4),out);
    }
    if(vehicle_presentation_family(game_memory_read_u8(&a->memory,p->object+12)) || game_memory_read_u8(&a->memory,p->object+12)==0x29)
        return index<3 && game_level_scene_property(a,p->object,get(a,get(a,p->object)+0x44+index*4),out);
    catalog=get(a,0x5f0210+(get(a,p->object+0x14)&63u)*4);
    if(index==2 && get(a,0x594d3c) && catalog!=get(a,0x58973c)){*out=0;return !a->memory.fault;}
    alternate=(game_memory_read_u8(&a->memory,p->object+0xd8)&4)!=0;
    if(index>2 || !game_level_player_catalog_record(a,catalog,0x20,&record))return 0;
    *out=get(a,record+index*4+(alternate?84:0));return !a->memory.fault;
}
static int insert(void *context){
    PlayerPlacement *p=context;GameApplication *a=p->app;uint32_t x=get(a,p->object+0x18),z=get(a,p->object+0x20);
    float fx,fz;memcpy(&fx,&x,4);memcpy(&fz,&z,4);if(a->memory.fault)return 0;
    /*499cb0 diagnoses out-of-grid coordinates and terminates with666. A
     *malformed placement must not silently disappear from the spatial index. */
    if((x!=0x80000000u && x>=0x48000000u) || (z!=0x80000000u && z>=0x48000000u)){
        fprintf(stderr,"Invalid player placement %u: %g, %g\n",(unsigned)p->object,(double)fx,(double)fz);exit(666);
    }
    (void)gameplay_spatial_insert(&a->memory,0x5f0800,p->object,(int32_t)fx/4096,(int32_t)fz/4096,
        (get(a,p->object+0x10)&0x400)!=0);return !a->memory.fault;
}
static int next_phase(void *context,uint32_t *phase){
    PlayerPlacement *p=context;GameApplication *a=p->app;uint32_t cursor=get(a,0x6308c0);
    if(cursor==1024)cursor=0;if(cursor>1024 || a->memory.fault)return 0;
    *phase=presentation_lifecycle_phase(real(a,0x6308c8+cursor*4),real(a,0x576918));
    put(a,0x6308c0,cursor+1);return !a->memory.fault;
}
static int runtime_property(void *context,unsigned index,uint32_t *bits){
    PlayerPlacement *p=context;GameApplication *a=p->app;GameAddress table=get(a,p->object),catalog;
    static const uint32_t methods[]={0x5521a0,0x4073d0,0x4e83c0,0x4073e0,0x552190,0x4e83a0,0x4e83b0,0x4e83d0,0x4e94b0,0x4073c0,0x5448c0,0x5448d0,0x5448e0,0x5448f0,0x5399e0};
    static const uint32_t values[]={65,100,60,10,10000,35,100,50,11000,55,60,100,40,70,11000};uint32_t value,selected;float fraction;int32_t signed_value;unsigned i;
    if(index>6 || a->memory.fault)return 0;
    selected=get(a,table+0x128+index*4);
    if(index<5){switch(selected){case 0x451130:case 0x539a10:*bits=30;return 1;case 0x451140:case 0x5399d0:*bits=100;return 1;
        case 0x40eb40:*bits=50;return 1;case 0x40eb50:*bits=100;return 1;case 0x40eb60:*bits=70;return 1;case 0x40eb70:*bits=60;return 1;
        case 0x451150:case 0x451160:case 0x5399f0:case 0x539a00:*bits=50;return 1;case 0x55ca70:*bits=10000;return 1;
        case 0x408c70:case 0x408fb0:*bits=55;return 1;case 0x4092d0:*bits=90;return 1;
        case 0x408c80:case 0x408fc0:case 0x4092e0:*bits=100;return 1;
        case 0x408c90:case 0x408fd0:case 0x4092f0:*bits=10;return 1;case 0x409600:*bits=14000;return 1;}}
    if(index<5){for(i=0;i<sizeof(methods)/sizeof(methods[0]);i++)if(selected==methods[i]){*bits=values[i];return 1;}return 0;}
    if(index==5 && selected==0x539a20){*bits=get(a,0x57691c);return !a->memory.fault;}
    if(selected!=(index==5?0x4e8480:0x539a30))return 0;
    if(index==5){*bits=get(a,0x574cf0);return !a->memory.fault;}
    if(get(a,table+0x60)!=0x53c550)return 0;
    catalog=get(a,0x5f0210+(get(a,p->object+0x14)&63u)*4);
    if(!game_level_player_catalog_value(a,catalog,game_memory_read_u8(&a->memory,p->object+12),5,(game_memory_read_u8(&a->memory,p->object+0xd8)&4)!=0,&value))return 0;
    memcpy(&signed_value,&value,4);fraction=(float)signed_value;memcpy(bits,&fraction,4);return 1;
}
static int runtime_packet(void *context,GameAddress *packet){PlayerPlacement *p=context;*packet=runtime_node_pool_acquire(&p->app->memory);return *packet!=0;}
static int runtime_prepare(void *context,GameAddress packet){PlayerPlacement *p=context;return runtime_node_pool_prepare(&p->app->memory,packet)==1;}
static int runtime_refresh(void *context,GameAddress object){
    PlayerPlacement *p=context;
    /*4f87f0 is an actual empty player method, verified against the reference. */
    uint32_t method=get(p->app,get(p->app,object)+0x14c);
    if(method==0x4f87f0)return !p->app->memory.fault;
    if(method!=0x4e8490 && method!=0x539960)return 0;
    return presentation_lifecycle_refresh_properties(&p->app->memory,object,presentation_instance_lifecycle(object),p,runtime_property);
}
int game_level_scene_command_publish(GameApplication *a,GameAddress object,RuntimeNode *node){
    PresentationLifecycle *state=presentation_instance_lifecycle(object);GamePresentationRecordResult result;
    uint32_t method=get(a,get(a,object)+0x16c),tick=0;float position[3]={0};unsigned i;
    if(!state || !node || (method!=0x53c030 && method!=0x406150 && method!=0x5475a0 && method!=0x55b500))return 0;
    if(node->kind==1 || node->kind==8)for(i=0;i<3;i++)position[i]=real(a,node->target+0x18+i*4);
    if(node->kind==9)tick=get(a,0x5ac6ac);
    if(a->memory.fault)return 0;
    presentation_command_publish(state,node,position,tick,&result);
    if(method==0x55b500 && node->kind==5){
        put(a,object+0x158,get(a,object+0x160));put(a,object+0x150,node->target?0:1);
        for(i=0;i<3;i++){uint32_t bits=node->target?get(a,node->target+24+i*4):0;
            if(!node->target)memcpy(&bits,node->position+i,4);put(a,object+0x144+i*4,bits);}}
    if(method==0x5475a0 && node->kind==11)put(a,object+0x178,UINT32_MAX);
    /*406150: derived consumer after53c030, recovered from the actual jump
     * table and406175/406189/40619b bytes. IDs select native behavior only. */
    if(method==0x406150){
        if(node->kind==0){put(a,object+0xf0,node->target);put(a,object+0xf4,node->target_identity);}
        else if(node->kind>=5 && node->kind<=7){game_memory_write_u16(&a->memory,object+0x212,0);game_memory_write_u16(&a->memory,object+0x210,0);}
        else if(node->kind==11)put(a,object+0x144,UINT32_MAX);
    }
    return !a->memory.fault;
}
static int reset_command(GameApplication *a,GameAddress object,GameAddress target,const float *position){
    PresentationLifecycle *state=presentation_instance_lifecycle(object);PresentationPose *pose=presentation_instance_pose(object);
    PresentationCommand command={0};
    if(!state || !pose)return 0;
    command.target=target;command.has_target=target!=0;
    if(target)command.target_identity=get(a,target+0x14);
    if(position){command.has_position=1;memcpy(command.position,position,12);}
    if(a->memory.fault || !presentation_command_replace(&a->memory,state,&command))return 0;
    put(a,object+0xdc,state->runtime.packet_chain);
    return !a->memory.fault;
}
int game_level_scene_replace_command(GameApplication *a,GameAddress object,GameAddress target,int position){
    PresentationPose *pose=presentation_instance_pose(object);if(!pose)return 0;
    if(!reset_command(a,object,target,position?pose->position:NULL))return 0;
    if(vehicle_presentation_family(game_memory_read_u8(&a->memory,object+12)) || game_memory_read_u8(&a->memory,object+12)==0x29)
        return game_level_scene_command_publish(a,object,runtime_node_pool_find(get(a,object+0xdc)));
    return !a->memory.fault;
}
/*53ba00 and53ba50 replace the owned command chain rather than performing a
 *priority-planned append. The first argument of these original methods is
 *unused. Controller history supplies either the actual vector or target. */
int game_level_scene_history_order(GameApplication *a,GameAddress object,unsigned action,GameAddress target,const float position[3]){
    GameAddress method=get(a,get(a,object)+(action==12?0x1a0:0x1a4));
    PresentationPose *pose=presentation_instance_pose(object);
    if(!pose)return 0;
    if(action==12 && method==0x53ba00)return reset_command(a,object,0,position);
    if(action==13 && method==0x53ba50)return reset_command(a,object,target,(pose->identity&63u)<4?pose->position:NULL);
    return 0;
}
/*53b950/53bac0/53b990/53c1d0: resolve the actual catalog member, plan before
 *reading borrowed inputs, build the native node chain and publish its result. */
static int identity_command(GameApplication *a,GameAddress object,uint32_t priority,uint32_t identity,int replace){
    GameAddress catalog=get(a,0x5f0210+(identity&63u)*4),target=0;uint32_t method=get(a,get(a,catalog)+4);
    PresentationLifecycle *state=presentation_instance_lifecycle(object);PresentationPose *pose=presentation_instance_pose(object);
    NativeEntityRelationshipResult relation;RuntimeNode *node;PresentationCommand command={0};unsigned i;
    if(!state || !pose || a->memory.fault || gameplay_catalog_lookup(&a->memory,catalog,method,identity,NULL,NULL,&target)!=1)return 0;
    if(!target)return 1;
    /*53bac0 replaces its node without publishing/advancing a vehicle command. */
    if(replace)return reset_command(a,object,target,(pose->identity&63u)<4?pose->position:NULL);
    node=runtime_node_pool_find(state->runtime.packet_chain);if(!node || node->in_pool)return 0;
    if(!native_entity_relationship_query(&a->memory,target,object,&relation))return 0;
    if(relation.decision<NATIVE_ENTITY_RELATION_ACCEPT_NULL_SECOND)return 1;
    presentation_command_seed_position(node,pose->identity,pose->position);
    if(presentation_command_plan(node,priority&255u,5)!=PRESENTATION_COMMAND_IGNORE){
        command.priority=priority&255u;command.kind=5;command.target=target;command.has_target=command.has_position=1;
        command.target_identity=get(a,target+0x14);
        for(i=0;i<3;++i)command.position[i]=real(a,0x594470+i*4);
        if(a->memory.fault || !presentation_command_build(&a->memory,node,&command,&node))return 0;
    }
    state->runtime.packet_chain=node->projection;put(a,object+0xdc,node->projection);
    return game_level_scene_command_publish(a,object,node);
}
int game_level_scene_identity_order(GameApplication *a,GameAddress object,uint32_t priority,uint32_t identity){
    GameAddress method=get(a,get(a,object)+0x19c);
    if(method==0x55c4d0)return !a->memory.fault; /*Actual empty class26 command. */
    /*546c80 is the class27 forwarder to this same identity-order program. */
    if(method!=0x53b950 && method!=0x4059f0 && method!=0x546c80)return 0;
    return identity_command(a,object,priority,identity,0);
}
int game_level_scene_identity_replace(GameApplication *a,GameAddress object,uint32_t identity){
    return get(a,get(a,object)+0x1a8)==0x53bac0 && identity_command(a,object,0,identity,1);
}
static int feedback_clear(void *context,GameAddress bank){return game_menu_sound_queue_clear(context,bank);}
static int feedback_submit(void *context,GameAddress bank,const int16_t sounds[8],int *accepted){return game_menu_sound_queue_submit(context,bank,sounds,accepted);}
int game_level_entity_runtime_initialize(GameApplication *a,GameAddress object){
    PlayerPlacement context={a,object};PresentationLifecycle *state=presentation_instance_lifecycle(object);
    PresentationInitializeServices initialize={&context,runtime_packet,runtime_prepare,next_phase,runtime_refresh};
    if(!state)return 0;state->initialize_phase=0;state->property_index=0;state->refreshing=0;
    return presentation_lifecycle_initialize(&a->memory,object,state,&initialize);
}
int game_level_entity_archive_frames(void *app,GameAddress object,int rendered){
    GameApplication *a=app;uint8_t kind=game_memory_read_u8(&a->memory,object+12);
    PlayerPlacement context={a,object};
    if(kind!=0x20 && kind!=0x40 && kind!=0x26 && kind!=0x29 && kind!=0x27 && !vehicle_presentation_family(kind))
        return game_level_object_archive_frames(a,object,rendered);
    return refresh(&context,rendered?1:0) && refresh(&context,rendered?3:2);
}
int game_level_entity_archive_publish(void *app,GameAddress object){PlayerPlacement context={app,object};return insert(&context);}
int game_level_entity_scene_place(GameApplication *a,GameAddress object,uint32_t identity,const float position[3],const uint16_t angles[3]){
    PlayerPlacement context={a,object};PresentationPlacement request={0};PresentationPose *pose=presentation_instance_pose(object);
    PresentationLifecycle *state=presentation_instance_lifecycle(object);GameAddress catalog,target;
    PresentationPlacementServices placement={&context,15,refresh,property,insert,0};
    PresentationInitializeServices initialize={&context,runtime_packet,runtime_prepare,next_phase,runtime_refresh};NativeEntityRelationshipResult relation;
    uint8_t kind=game_memory_read_u8(&a->memory,object+12);
    if(!pose || !state || (!vehicle_presentation_family(kind) && kind!=0x27 && kind!=0x26 && kind!=0x29 && kind!=0x40 && !presentation_instance_player_rig(object)))return 0;
    request.identity=identity;request.descriptor=get(a,0x5f0210+(identity&63u)*4)+0x10;request.controller=1;
    memcpy(request.position,position,12);memcpy(request.angles,angles,6);
    if(!presentation_pose_place(&a->memory,object,pose,&request,&placement)){
        fprintf(stderr,"native actor placement failed: pose kind=%02x object=%08x\n",kind,object);return 0;}
    if(!presentation_lifecycle_initialize(&a->memory,object,state,&initialize)){
        fprintf(stderr,"native actor placement failed: runtime kind=%02x object=%08x phase=%u property=%u\n",kind,object,state->initialize_phase,state->property_index);return 0;}
    if((pose->identity&63u)>=4)return 1;
    catalog=get(a,0x5f0210+(pose->identity&63u)*4);
    if(!gameplay_state_entry_matches(&a->memory,catalog)){
        if(!(pose->flags&256) && !game_level_scene_replace_command(a,object,0,1))return 0;
        if(!(pose->flags&256) && kind==0x23){GamePresentationFeedbackServices feedback={a,feedback_clear,feedback_submit};
            if(!game_presentation_deactivation_feedback(&a->memory,object,3,0x57,&feedback))return 0;}
        return !a->memory.fault; /*42ec20 is the player's empty deactivation method. */
    }
    target=get(a,catalog+0x3c4);identity=get(a,catalog+0x3c8);
    if(!native_entity_relationship_query(&a->memory,target,object,&relation))return 0;
    if(relation.decision>=NATIVE_ENTITY_RELATION_ACCEPT_NULL_SECOND)return get(a,get(a,object)+0x19c)==0x55c4d0?!a->memory.fault:identity_command(a,object,2,identity,0);
    if(get(a,0x5f0160+(pose->identity&63u)*24+(get(a,target+0x14)&63u)*4))return identity_command(a,object,0,identity,1);
    return !a->memory.fault;
}
int game_level_player_scene_place(GameApplication *a,GameAddress object,uint32_t identity,const float position[3],const uint16_t angles[3]){
    static const uint16_t shorts[]={0x224,0x26c,0x226,0x26e,0x2f8,0x2fa,0x2c0};
    static const uint16_t words[]={0x2c4,0x2c8,0x140,0x144,0x48c,0x490,0x494};
    static const uint16_t choices[]={0,0,0x2ec,0x2f0,0x2f8,0x2f4,0x2fc,0x2dc,0x300,0x304,0x308,0x30c,0x310,0x314,0x318};
    static const uint16_t motions[]={0x470,0x478,0x4a8,0x47c,0x488,0x484,0x498,0x1fc,0x480,0x49c,0x4ac,0x474,0x48c,0x490,0x494};
    PlayerSceneChildrenDefinition definition={0};GameAddress catalog,mount,motion;uint32_t height,top,bottom,base,index;unsigned i,j;
    if(!presentation_instance_player_rig(object) || player_scene_children_find(object))return 0;
    for(i=0;i<sizeof(shorts)/sizeof(shorts[0]);++i)game_memory_write_u16(&a->memory,object+shorts[i],0);
    for(i=0;i<sizeof(words)/sizeof(words[0]);++i)put(a,object+words[i],0);
    put(a,object+0x498,UINT32_MAX);
    if(!game_level_player_clear_events(a,object) || !game_level_entity_scene_place(a,object,identity,position,angles))return 0;
    catalog=get(a,0x5f0210+(get(a,object+0x14)&63u)*4);motion=get(a,get(a,0x5f0210+(identity&63u)*4)+0x470);
    if(!game_level_player_catalog_value(a,catalog,0x20,5,0,&height) ||
       !game_level_player_catalog_mount(a,catalog,2,0x20,1,&top) || !game_level_player_catalog_mount(a,catalog,2,0x20,0,&bottom) ||
       !game_level_player_catalog_value(a,catalog,0x20,1,0,&base) ||
       !player_scene_children_prepare(&a->memory,object,motion,(int32_t)height,(int32_t)top,(int32_t)bottom,(int32_t)base))return 0;
    if(get(a,0x5eff04) && !player_rig_record_pose(&a->memory,presentation_instance_pose(object),
        presentation_instance_player_controls(object),presentation_instance_player_rig(object),1))return 0;
    if(!game_level_player_catalog_value(a,catalog,0x20,3,0,&index) || index>=15 || get(a,0x5adcf8+index*4)<4)return 0;
    mount=get(a,0x5adbc4+index*4);if(!mount)return 0;
    for(i=0;i<15;++i){definition.choices[i]=choices[i]?(int32_t)get(a,catalog+choices[i]):0;definition.motion[i]=get(a,catalog+motions[i]);}
    for(i=0;i<4;++i)for(j=0;j<3;++j)definition.mounts[i][j]=real(a,mount+i*12+j*4);
    for(i=0;i<3;++i)definition.dimensions[i]=real(a,object+0x2c+i*4);
    definition.small_scale=real(a,0x574e28);definition.lateral_shift=real(a,0x574e2c);
    definition.vertical_shift=real(a,0x574e30);definition.rear_shift=real(a,0x574d68);definition.mount_rear_shift=real(a,0x574e34);
    if(a->memory.fault || !player_scene_children_create(&a->memory,object,&definition))return 0;
    for(i=0;i<2;++i){index=get(a,object+0x214+i*4);if(index>=15)return 0;put(a,object+0x1cc+i*4,get(a,object+0x1d4+index*4));}
    for(i=0;i<2;++i){for(j=0;j<3;++j)put(a,object+0x19c+i*16+j*4,0);game_memory_write_u16(&a->memory,object+0x1a8+i*16,0);}
    for(i=0;i<3;++i)put(a,object+0x1bc+i*4,get(a,object+0x18+i*4));
    game_memory_write_u16(&a->memory,object+0x1c8,game_memory_read_u16(&a->memory,object+0x24));
    return !a->memory.fault && player_runtime_pool_bind_parts(&a->memory,object);
}
static int child_available(void *context,PlayerSceneChildKind kind,int *available){
    GameApplication *a=context;GameAddress world=get(a,0x58973cu),table=get(a,world);
    uint32_t cost,check=get(a,table+0x2cu),spend=get(a,table+0x30u);
    if(!game_level_player_catalog_mount(a,world,kind==PLAYER_SCENE_POSITION?9u:17u,0x20u,4u,&cost))return 0;
    if(check==0x477730u)*available=1;
    else if(check==0x47f210u)*available=gameplay_catalog_can_spend(&a->memory,world,cost);
    else return 0;
    if(a->memory.fault)return 0;
    if(!*available || spend==0x477740u)return 1;
    return spend==0x47f230u && gameplay_catalog_spend(&a->memory,world,cost);
}
static int child_emit(void *context,const PlayerAttachmentEmission *emission){
    GameApplication *a=context;GameAddress object,world;uint32_t identity,next,method=0,head;
    uint8_t kind=game_memory_read_u8(&a->memory,emission->motion+12u);
    uint16_t variant=(uint16_t)get(a,emission->motion+16u);
    if(a->memory.fault)return 0;
    object=game_level_factory_take(a,kind,variant);
    if(!object){fprintf(stderr,"Native shot acquisition failed: kind=%u variant=%u at=%08x\n",(unsigned)kind,(unsigned)variant,(unsigned)a->memory.fault_address);return 0;}
    world=get(a,0x58973cu);
    if(level_player_slots_owns_player(world)){
        if(!level_player_slots_next_owner_sequence(&a->memory,world,&identity,&next))goto failed;
        put(a,world+0x18u,next);
    }else identity=game_state_next_owner_sequence(&a->memory,world);
    if(a->memory.fault)goto failed;
    if(emission->kind==PLAYER_SCENE_POSITION){
        method=get(a,get(a,object)+0x78u);
        if(method!=0x434780u || !game_level_position_shot_place(a,object,identity,emission->position,
            emission->angles,emission->motion,0x20u))goto failed;
    }else{
        method=emission->kind==PLAYER_SCENE_DERIVED_SPATIAL?0x431ee0u:
            emission->kind==PLAYER_SCENE_MOTION_POSITION?0x547890u:get(a,get(a,object)+0xacu);
        if(!game_level_projectile_launch(a,object,identity,emission->position,emission->angles,
            emission->motion,emission->parent,method,emission->strength))goto failed;
    }
    world=get(a,0x58973cu);head=state_helper_flagged_member_offset(get(a,object+16u));
    if(a->memory.fault || (level_player_slots_owns_player(world) &&
       !level_player_slots_attach_scene_member(world,object,head)))goto failed;
    next=get(a,world+head+0x48u);put(a,object+0x4cu,world+head);put(a,object+0x48u,next);
    if(next)put(a,next+0x4cu,object);put(a,world+head+0x48u,object);
    if(!a->memory.fault)return 1;
    (void)level_player_slots_forget_scene_member(object);
failed:
    fprintf(stderr,"Native shot initialization failed: object=%08x method=%08x at=%08x\n",(unsigned)object,(unsigned)method,(unsigned)a->memory.fault_address);
    (void)game_level_factory_destroy(a,object);return 0;
}
static GameAddress child_notice(PlayerSceneChildKind kind){
    return kind==PLAYER_SCENE_POSITION?0x58421cu:kind==PLAYER_SCENE_MOTION_POSITION?0x584220u:
        kind==PLAYER_SCENE_DERIVED_SPATIAL?0x584228u:0x584224u;
}
static int child_environment(void *context,GameAddress parent,PlayerSceneChildKind kind,PlayerAttachmentEnvironment *out){
    GameApplication *a=context;GameAddress world=get(a,0x58973cu);
    memset(out,0,sizeof(*out));out->tick=get(a,0x5ac6acu);out->previous_notice=get(a,child_notice(kind));
    out->current_world_is_local=world==get(a,0x5f0208u);out->feedback_enabled=get(a,0x624f68u)!=0;
    if(out->feedback_enabled){
        out->parent_key=game_memory_read_u8(&a->memory,parent+12u);
        if(out->parent_key==0x23u){GameAddress linked=get(a,get(a,parent+0x294u)+4u);out->linked_world_matches=get(a,linked+0x54u)==world;}
    }
    return !a->memory.fault;
}
static int child_feedback(void *context){
    GameApplication *a=context;const uint32_t values[3]={400u,0u,50u};
    return gameplay_feedback_request(&a->memory,values);
}
static int child_last_round(void *context,PlayerSceneChildKind kind,uint32_t tick){
    GameApplication *a=context;SoundPlaybackRequest request={0x17,0,1,0,0,0};
    if(!game_menu_sound_play(a,&request))return 0;put(a,child_notice(kind),tick);return !a->memory.fault;
}
static int child_notify(void *context,GameAddress parent,uint16_t heading){
    GameApplication *a=context;GameAddress view=get(a,0x58f890u);
    long double x=(long double)real(a,parent+0x18u)-real(a,view+12u),z=(long double)real(a,parent+0x20u)-real(a,view+20u);
    int16_t sound=(int16_t)(heading<0x8000u?(int32_t)heading:(int32_t)heading-65536);
    SoundPlaybackRequest request={sound,(float)(x*x+z*z),1,0,0,0};
    return !a->memory.fault && game_menu_sound_play(a,&request);
}
static PlayerAttachmentServices child_services(GameApplication *a){
    PlayerAttachmentServices services={a,child_available,child_emit,child_notify,child_environment,child_feedback,child_last_round};
    return services;
}
int game_level_player_attachment_submit(GameApplication *a,GameAddress child,const float position[3],const PlayerAttachmentFrame *frame){
    PlayerAttachmentState *state=player_scene_children_attachment(child);
    PlayerAttachmentServices services=child_services(a);
    return state?player_attachment_submit(state,position,frame,&services):
        player_attachment_record_submit(&a->memory,child,position,frame,&services);
}
int game_level_player_attachment_update(GameApplication *a,GameAddress child,const PlayerAttachmentFrame *frame){
    PlayerAttachmentState *state=player_scene_children_attachment(child);
    PlayerAttachmentServices services=child_services(a);
    return state?player_attachment_update(&a->memory,state,frame,&services):
        player_attachment_record_update(&a->memory,child,frame,&services);
}
int game_level_raw_auxiliary_fire(GameApplication *a,GameAddress child,const uint16_t angles[3],const float matrix[12]){
    PlayerAttachmentFrame frame={0};GameAddress motion,parent;
    if(!get(a,child+12u))return !a->memory.fault;
    motion=get(a,child+20u);parent=get(a,child+4u);
    frame.tick=get(a,0x5ac6acu);frame.motion_delay=get(a,motion+0x20u);
    if(get(a,child+12u) && (int32_t)(get(a,child+8u)+frame.motion_delay)<=(int32_t)frame.tick)
        frame.parent_flags=get(a,parent+16u);
    memcpy(frame.angles,angles,6);memcpy(frame.world_matrix,matrix,48);
    return !a->memory.fault && game_level_player_attachment_update(a,child,&frame)>=0;
}
int game_level_attachment_transform(GameApplication *a,GameAddress child,GameAddress method,const float matrix[12]){
    PlayerAttachmentState *state=player_scene_children_attachment(child);
    PlayerAttachmentFrame frame={0};int status;GameAddress motion,parent;uint32_t quantity,last_tick;
    if(method!=0x43c5f0u || get(a,get(a,child)+4u)!=0x43bec0u)return 0;
    memcpy(frame.world_matrix,matrix,sizeof(frame.world_matrix));
    if(!game_matrix_extract_angles(&a->memory,matrix,frame.angles))return 0;
    quantity=state?state->quantity:get(a,child+12u);
    if(quantity){
        motion=state?state->motion:get(a,child+20u);parent=state?state->parent:get(a,child+4u);
        last_tick=state?state->last_tick:get(a,child+8u);
        frame.tick=get(a,0x5ac6acu);frame.motion_delay=get(a,motion+0x20u);
        if((int32_t)(last_tick+frame.motion_delay)<=(int32_t)frame.tick)
            frame.parent_flags=get(a,parent+16u);
    }
    if(a->memory.fault)return 0;
    status=game_level_player_attachment_update(a,child,&frame);
    return status>=0;
}
static int child_weapon_fire(void *context,GameMemory *memory,GameAddress weapon,GameAddress matrix,uint32_t *result){
    GameApplication *a=context;PlayerAttachmentState *state=player_scene_children_attachment(weapon);
    PlayerAttachmentFrame frame={0};unsigned i;int status;
    if(memory!=&a->memory || !state)return 0;
    /*572e38 selects43cc40 (returns0) and43cc50 (empty). This command
     *attachment intentionally has no projectile-fire behavior in retail. */
    if(state->kind==PLAYER_SCENE_COMMAND){if(result)*result=0;return 1;}
    /*43c5f0/43b710/43bab0 recover orientation before entering the child
     *scheduler. Every remaining callback here consumes ordinary C data. */
    for(i=0;i<12u;i++)frame.world_matrix[i]=real(a,matrix+i*4u);
    if(!game_matrix_extract_angles(memory,frame.world_matrix,frame.angles))return 0;
    if(state->quantity){
        frame.tick=get(a,0x5ac6acu);frame.motion_delay=get(a,state->motion+0x20u);
        if((int32_t)(state->last_tick+frame.motion_delay)<=(int32_t)frame.tick)
            frame.parent_flags=get(a,state->parent+16u);
    }
    if(memory->fault)return 0;
    status=game_level_player_attachment_update(a,weapon,&frame);
    if(status<0)return 0;if(result)*result=(uint32_t)status;return 1;
}
static int child_matrix_compose(void *context,GameAddress out,GameAddress left,GameAddress right){
    GameApplication *a=context;return game_matrix_compose(&a->memory,out,left,right);
}
int game_level_player_scene_update_weapons(GameApplication *a,GameAddress object,GameAddress intermediate,GameAddress firing){
    GamePlayerWeaponCallbacks callbacks={a,child_weapon_fire};
    return game_player_update_weapons(game_player_ref(&a->memory,object),&callbacks,
        child_matrix_compose,a,intermediate,firing);
}
