#include "level_factory.h"
#include "level_objects.h"
#include "level_run.h"
#include "spatial_attachment.h"
#include "model_assets.h"
#include "vehicle_presentation.h"
#include "scene_instance_owner.h"
#include "scene_projection_storage.h"
#include "level_player_slots.h"
#include "level_player_scene.h"
#include "level_player_resources.h"
#include "game_intrusive_list.h"
#include "effect_resource_pool.h"
#include "gameplay_state.h"
#include "level_effects.h"
#include "level_effect_runtime.h"
#include "attachment_effect.h"
#include "menu_sound.h"
#include "player_feedback.h"
#include "display_transition.h"
#include "game_initializers.h"
#include "gameplay_leaves.h"
#include "gameplay_geometry.h"
#include "presentation_commands.h"
#include "scene_orders.h"
#include "vehicle_targeting.h"
#include "portable_crt.h"
#include <math.h>
#include "gameplay_messages.h"
#include "event_queue.h"
#include "effect_model.h"
#include "effect_profile.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* The existing presentation instance owns behavior/model state. This list owns
 * allocations, including acquired copies no longer linked to a template pool.
 * The four legacy arrays only publish borrowed asset inputs to consumers. */
typedef struct VehicleAllocation {GameAddress object;struct VehicleAllocation *next;} VehicleAllocation;
struct GameVehicleFactory {VehicleAllocation *objects;GameModelDefinition assets[8][11];unsigned defined[8][11];GameModelDefinition infantry[11],aircraft[11],class29[4];};
typedef struct VehicleFactoryOperation {GameApplication *app;const VehiclePresentationFamily *family;} VehicleFactoryOperation;
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
static float real(GameApplication *a,GameAddress p){uint32_t bits=get(a,p);float value;memcpy(&value,&bits,4);return value;}
static uint16_t aircraft_heading(GameApplication *,float,float);
static int owns(GameApplication *a,GameAddress object){
    VehicleAllocation *node=a->vehicle_factory?a->vehicle_factory->objects:NULL;
    for(;node;node=node->next)if(node->object==object)return 1;
    return 0;
}
static GameAddress allocate(void *context,size_t size){
    GameApplication *a=context;VehicleAllocation *node=malloc(sizeof(*node));GameAddress object;
    if(!node)return 0;
    object=scene_projection_storage_allocate(size,"citadel vehicle payload");
    if(!object){free(node);return 0;}
    node->object=object;node->next=a->vehicle_factory->objects;a->vehicle_factory->objects=node;return object;
}
static int release_object(void *context,GameAddress object){
    GameApplication *a=context;VehicleAllocation **link;
    if(!object)return 1;
    if(!a->vehicle_factory)return 0;
    for(link=&a->vehicle_factory->objects;*link;link=&(*link)->next)if((*link)->object==object){
        VehicleAllocation *node=*link;
        if(!scene_projection_storage_free(object))return 0;
        *link=node->next;free(node);(void)level_player_slots_forget_scene_member(object);return 1;
    }
    return 0;
}
static int release_page(void *context,GameAddress page){
    GameApplication *a=context;unsigned i,j;
    if(!page)return 1;
    if(!scene_projection_storage_free(page))return 0;
    if(a->vehicle_factory)for(i=0;i<8;i++)for(j=0;j<11;j++)if(a->vehicle_factory->assets[i][j].markers==page)a->vehicle_factory->assets[i][j].markers=0;
    return 1;
}
static int clone_model(void *context,GameAddress source,GameAddress *copy){GameApplication *a=context;return scene_instance_owner_clone(&a->memory,source,copy)==1;}
static int release_model(void *context,GameAddress model){
    GameApplication *a=context;unsigned i,j;
    if(!model)return 1;
    if(!game_application_scene_model_release(a,model))return 0;
    if(a->vehicle_factory)for(i=0;i<8;i++)for(j=0;j<11;j++)if(a->vehicle_factory->assets[i][j].model==model)a->vehicle_factory->assets[i][j].model=0;
    return 1;
}
static int return_packet(void *context,GameAddress event){return game_level_factory_detach_packet_chain(context,event);}
static int return_event(void *context,GameAddress event){
    GameApplication *a=context;EffectResource *resource=effect_resource_find(event);
    if(!resource || resource->owner->object_kind<1 || resource->owner->object_kind>3 ||
       resource->event_route!=resource->owner->object_kind-1)return 0;
    return effect_resource_pool_return(&a->memory,resource->owner,event)==1;
}
static int return_contact(void *context,GameAddress event){return game_level_factory_release_pool_member(context,event);}
static int base_release(void *context,GameAddress object){
    GameApplication *a=context;PresentationReleaseServices s={a,release_model,release_page,
        return_contact,return_packet};
    return presentation_instance_release(&a->memory,object,&s,1,1)==1;
}
static int sprite_load(void *context,const char *name,ResourceBlob *blob){return game_application_load_blob(context,name,blob);}
static uint32_t sprite_texture(void *context,const uint8_t *pixels,uint32_t width,uint32_t height,uint32_t flags){
    return game_level_texture_from_indexed(context,pixels,width,height,flags);
}
/*40c780/40c810 share539690/5395e0. The existing instance owns the model,
 *markers and runtime buffers; its immutable profile bank is retained by copies. */
static int class40_finish(GameApplication *a,GameAddress object){
    put(a,object,0x571cc0);game_memory_write_u8(&a->memory,object+12,0x40);
    put(a,object+16,get(a,object+16)|0x3010);put(a,object+0x150,0);put(a,object+0x154,0);
    put(a,get(a,object+0x54)+0x40,object);return !a->memory.fault;
}
GameAddress game_level_class40_factory(GameApplication *a,uint16_t slot,unsigned op,int32_t count){
    GameAddress source,object,model=0,markers=0,profiles[7]={0};
    PresentationConstructionServices copy={a,clone_model};unsigned i,j;int constructed=0;
    if(!a || slot || op>2 || count>65536 || a->memory.fault)return 0;
    if(!a->vehicle_factory){a->vehicle_factory=calloc(1,sizeof(*a->vehicle_factory));if(!a->vehicle_factory)return 0;}
    source=get(a,0x5adbf8);if(source && !owns(a,source))return 0;
    if(!source){
        EffectProfileServices services={a,sprite_load,sprite_texture};PresentationInstanceDefinition d={0};
        for(i=0;i<7;i++){
            EffectProfileRequest request={0};char name[128];GameAddress text=get(a,0x583358+i*4);
            for(j=0;j<sizeof(name);j++){name[j]=(char)game_memory_read_u8(&a->memory,text+j);if(!name[j])break;}
            if(j==sizeof(name) || a->memory.fault)goto failed;
            request.name=name;request.frames=(int16_t)get(a,0x583378+i*4);
            request.directions=(int16_t)get(a,0x583398+i*4);request.duration=(int32_t)get(a,0x5833b8+i*4);
            request.scale=real(a,0x5833d8+i*4);request.flags=(uint8_t)get(a,0x5833f8+i*4);
            profiles[i]=effect_profile_create(&a->memory,&request,&services);if(!profiles[i])goto failed;
        }
        model=effect_model_create_bank(profiles,7);if(!model)goto failed;
        memset(profiles,0,sizeof(profiles));markers=scene_projection_storage_allocate(0x54,"class40 markers");if(!markers)goto failed;
        for(i=0;i<3;i++){
            float y=(float)((long double)i*real(a,0x5730fc)-real(a,0x5730fc));uint32_t bits;memcpy(&bits,&y,4);
            for(j=0;j<3;j++)put(a,markers+i*28+j*4,0x42dac000);
            put(a,markers+i*28+12,0);put(a,markers+i*28+16,bits);put(a,markers+i*28+20,0);
            put(a,markers+i*28+24,0x42dac000);
        }
        d.dimensions[0]=218.75f;d.dimensions[1]=140;d.dimensions[2]=175;
        d.radius=0x435ac000;d.resource=markers;d.count=3;d.model=model;d.type=0x40;d.flags=2;
        source=allocate(a,0x158);if(!source)goto failed;
        if(!presentation_instance_create(&a->memory,source,&d,1))goto failed;
        constructed=1;markers=model=0;
        if(!class40_finish(a,source))goto failed;
        put(a,0x5adbf8,source);if(a->memory.fault)goto failed;
    }
    if(!op)return source;
    if(op==2){object=get(a,source+4);
        if(object){game_list_detach_node(&a->memory,object,8,4);put(a,object+4,0);put(a,object+8,0);return a->memory.fault?0:object;}
        count=1;
    }
    while(count>0){object=allocate(a,0x158);if(!object)return 0;
        if(!presentation_instance_create_copy(&a->memory,object,source,1,&copy)){release_object(a,object);return 0;}
        if(!class40_finish(a,object)){game_level_class40_destroy(a,object);return 0;}
        if(op==2)return object;
        game_list_insert_after(&a->memory,object,source,4,8);if(a->memory.fault)return 0;count--;
    }
    return source;
failed:{GameMemory fault=a->memory;a->memory.fault=0;
    if(constructed)game_level_class40_destroy(a,source);else if(source)release_object(a,source);
    if(model)effect_model_release(model);if(markers)scene_projection_storage_free(markers);
    for(i=0;i<7;i++)if(profiles[i]){effect_profile_release(&a->memory,profiles[i]);scene_projection_storage_free(profiles[i]);}
    if(fault.fault)a->memory=fault;return 0;}
}
static int class40_guns_release(GameApplication *a,GameAddress object){
    unsigned i;for(i=0;i<2;i++){GameAddress gun=get(a,object+0x150+i*4);
        if(a->memory.fault)return 0;if(!gun)continue;
        game_leaf_0043b4c0(&a->memory,gun);if(a->memory.fault || !scene_projection_storage_free(gun))return 0;
        put(a,object+0x150+i*4,0);
    }return !a->memory.fault;
}
int game_level_class40_destroy(GameApplication *a,GameAddress object){
    VehicleAllocation *node;
    if(!a || !owns(a,object) || game_memory_read_u8(&a->memory,object+12)!=0x40 || a->memory.fault)return 0;
    for(node=a->vehicle_factory->objects;node;node=node->next){
        const PresentationInstance *instance=presentation_instance_find(node->object);
        if(node->object!=object && instance && instance->definition.source==object)return 0;
    }
    if(!class40_guns_release(a,object))return 0;
    if(get(a,object+4) || get(a,object+8)){game_list_detach_node(&a->memory,object,8,4);put(a,object+4,0);put(a,object+8,0);}
    if(a->memory.fault || !base_release(a,object))return 0;
    if(get(a,0x5adbf8)==object)put(a,0x5adbf8,0);
    return !a->memory.fault && release_object(a,object);
}
int game_level_class40_reset(GameApplication *a,GameAddress object){
    return a && owns(a,object) && game_memory_read_u8(&a->memory,object+12)==0x40 &&
        class40_guns_release(a,object) && game_level_presentation_reset(a,object,2);
}
/*40ed80/40ee10 retain only yaw;40ee80/40eee0 translate the vertical marker
 *column. Applying a general hull matrix here would rotate the wrong geometry. */
int game_level_class40_geometry(GameApplication *a,GameAddress object,unsigned operation){
    PresentationPose *pose=presentation_instance_pose(object);EffectModel *model;
    float *matrix;float values[12]={0},sine,cosine;uint16_t yaw;unsigned i,j;GameAddress output;
    if(!pose || operation>3 || a->memory.fault)return 0;
    model=effect_model_find(pose->model);if(!model || !model->bank)return 0;
    if(operation<2){
        yaw=(operation?pose->local_angles[2]:pose->angles[0])&2047;
        sine=real(a,0x6298a8+yaw*4);cosine=real(a,0x6298a8+((yaw+512)&2047)*4);
        values[0]=values[10]=cosine;values[2]=sine;values[8]=-sine;values[5]=1;
        values[3]=operation?pose->local_position[0]:pose->position[0];
        values[7]=operation?pose->local_position[1]:pose->position[1];
        values[11]=operation?pose->local_position[2]:pose->position[2];
        matrix=operation?model->local:pose->world_matrix;memcpy(matrix,values,sizeof(values));
        if(!operation)for(i=0;i<12;i++){uint32_t bits;memcpy(&bits,values+i,4);put(a,object+0x80+i*4,bits);}
    }else{
        matrix=operation==3?model->local:pose->world_matrix;output=operation==3?pose->controller:pose->vertices;
        for(i=0;i<pose->vertex_count;i++){
            float point[3]={matrix[3],(float)((long double)real(a,pose->hull+i*28+16)+matrix[7]),matrix[11]};
            for(j=0;j<3;j++){uint32_t bits;memcpy(&bits,point+j,4);put(a,output+i*12+j*4,bits);}
        }
    }
    return !a->memory.fault;
}
/*40f090: both guns use the same vertical anchor, but distinct catalog banks
 *and headings. The attachment constructor is ordinary C43bc70. */
int game_level_class40_guns_build(GameApplication *a,GameAddress object,GameAddress catalog){
    GameAddress guns[2]={0};GameSpatialAttachmentInit init={0};unsigned i;
    unsigned alternate=(game_memory_read_u8(&a->memory,object+0xd8)&4)!=0;
    init.parent=object;init.resource=1;init.position[1]=init.secondary_position[1]=25;
    for(i=0;i<2;i++){
        init.heading=(uint16_t)(i?17:27);
        init.motion=get(a,catalog+(i?(alternate?0x1c8:0x1c4):(alternate?0x1a0:0x19c)));
        guns[i]=scene_projection_storage_allocate(0xbc,"class40 gun");
        if(!guns[i] || a->memory.fault || !game_spatial_attachment_construct(&a->memory,guns[i],&init))goto failed;
    }
    if(!class40_guns_release(a,object))goto failed;
    for(i=0;i<2;i++){put(a,object+0x150+i*4,guns[i]);guns[i]=0;}
    return !a->memory.fault;
failed:for(i=0;i<2;i++)if(guns[i])scene_projection_storage_free(guns[i]);return 0;
}
int game_level_class40_place(GameApplication *a,GameAddress object,uint32_t identity,const float position[3],const uint16_t angles[3]){
    GameAddress catalog,local;uint32_t enabled,speed,tick,word;int32_t elapsed;EffectModel *model;
    if(!a || !owns(a,object) || get(a,object)!=0x571cc0 || a->memory.fault)return 0;
    game_memory_write_u16(&a->memory,object+0x144,0);game_memory_write_u16(&a->memory,object+0x146,1);put(a,object+0x14c,0);
    catalog=get(a,0x5f0210+(identity&63)*4);
    switch(get(a,get(a,catalog)+0x70)){
    case 0x4777e0:enabled=0;break;
    case 0x478ff0:enabled=get(a,catalog+0x218);break;
    default:return 0;
    }
    if(enabled)game_memory_write_u8(&a->memory,object+0xd8,game_memory_read_u8(&a->memory,object+0xd8)|4);
    if(!game_level_entity_scene_place(a,object,identity,position,angles))return 0;
    put(a,object+0xb4,0);
    if(!game_level_player_catalog_value(a,catalog,0x40,5,(game_memory_read_u8(&a->memory,object+0xd8)&4)!=0,&speed))return 0;
    {int32_t signed_speed;float value;memcpy(&signed_speed,&speed,4);value=(float)signed_speed;memcpy(&word,&value,4);put(a,object+0xb8,word);}
    model=effect_model_find(get(a,object+0x54));tick=get(a,0x5ac6ac);
    if(!effect_model_select_profile(&a->memory,model,2,tick))return 0;
    game_memory_write_u8(&a->memory,object+0x148,2);
    if(!game_level_class40_guns_build(a,object,catalog))return 0;
    local=get(a,0x5f0208);word=tick-get(a,local+0x58c);memcpy(&elapsed,&word,4);
    if(elapsed>1 && (get(a,object+20)&63)==game_memory_read_u16(&a->memory,local+8)){
        SoundPlaybackRequest sound={0};put(a,local+0x58c,tick);game_memory_write_u16(&a->memory,local+0x598,0x36);
        sound.sound=0x36;sound.volume=1;if(!game_menu_sound_play(a,&sound))return 0;
    }
    return !a->memory.fault;
}
/*40cab0: inset map bounds, shared destination publication, close-target stop,
 *otherwise select run animation and derive ordinary planar velocity/heading. */
int game_level_class40_move(GameApplication *a,GameAddress object,float x,float z){
    long double lower,upper,threshold,factor;float low,high,dx,dz,vx,vz,velocity;
    GameAddress catalog;EffectModel *model;uint32_t speed,bits;int32_t signed_speed;
    if(!a || !owns(a,object) || get(a,object)!=0x571cc0 || a->memory.fault)return 0;
    lower=(long double)real(a,object+0x38)*real(a,0x571e98);low=(float)lower;
    if(!(lower<x))return !a->memory.fault;
    upper=(long double)real(a,object+0x38)*real(a,0x571e9c)+real(a,0x571ea0);high=(float)upper;
    if(!(upper>x && z>low && z<high))return !a->memory.fault;
    if(!scene_order_move(&a->memory,object,0x4ca140,x,z,NULL))return 0;
    catalog=get(a,0x5f0210+(get(a,object+20)&63)*4);
    if(!game_level_player_catalog_value(a,catalog,0x40,5,(game_memory_read_u8(&a->memory,object+0xd8)&4)!=0,&speed))return 0;
    memcpy(&signed_speed,&speed,4);velocity=(float)signed_speed;memcpy(&bits,&velocity,4);put(a,object+0x128,bits);
    dx=(float)((long double)real(a,object+0xf8)-real(a,object+24));
    dz=(float)((long double)real(a,object+0x100)-real(a,object+32));
    threshold=(long double)velocity+real(a,0x571e94);
    if(!(fabsl(dx)>=threshold) && !(fabsl(dz)>=threshold)){put(a,object+0xe0,5);return !a->memory.fault;}
    model=effect_model_find(get(a,object+0x54));
    if(!effect_model_select_profile(&a->memory,model,0,get(a,0x5ac6ac)))return 0;
    factor=(long double)velocity/sqrtl((long double)dz*dz+(long double)dx*dx);
    vx=(float)((long double)dx*factor);vz=(float)((long double)dz*factor);
    memcpy(&bits,&vz,4);put(a,object+0xb8,bits);memcpy(&bits,&vx,4);put(a,object+0xb4,bits);
    game_memory_write_u16(&a->memory,object+0x24,vehicle_targeting_octant(&a->memory,dx,dz));return !a->memory.fault;
}
/*40cd20: the delayed shot refreshes the selected owned gun five ticks before
 *expiry. Deadline comparisons retain the original signed/wrapping behavior. */
int game_level_class40_timed_gun(GameApplication *a,GameAddress object){
    uint32_t now=get(a,0x5ac6ac),deadline=get(a,object+0x13c),secondary;int32_t end,tick;
    GameAddress gun;uint16_t angles[3];float matrix[12];unsigned i;
    if(a->memory.fault || !owns(a,object) || get(a,object)!=0x571cc0)return 0;
    if(deadline-now==5){
        secondary=get(a,object+0x14c);gun=get(a,object+(secondary?0x154:0x150));
        angles[0]=game_memory_read_u16(&a->memory,object+0x24);
        angles[1]=game_memory_read_u16(&a->memory,object+0x144);
        angles[2]=game_memory_read_u16(&a->memory,object+0x28);
        for(i=0;i<12;i++)matrix[i]=real(a,object+0x80+i*4);
        if(!gun || !game_level_raw_auxiliary_fire(a,gun,angles,matrix))return 0;
        if(secondary)put(a,object+0x14c,0);
    }
    memcpy(&end,&deadline,4);memcpy(&tick,&now,4);if(end<=tick)put(a,object+0xe0,5);
    return !a->memory.fault;
}
/*443f48/450c00/450af0: four one-mesh vehicles. The presentation instance owns
 *the model and markers; the factory retains debris parts until templates retire. */
GameAddress game_level_class29_factory(GameApplication *a,uint16_t slot,unsigned op,int32_t count){
    GameAddress source,object;GameModelDefinition *asset;
    PresentationConstructionServices copy={a,clone_model};
    if(!a || slot>=4 || op>2 || count>65536 || a->memory.fault)return 0;
    if(!a->vehicle_factory){a->vehicle_factory=calloc(1,sizeof(*a->vehicle_factory));if(!a->vehicle_factory)return 0;}
    source=get(a,0x5adae0+slot*4);asset=&a->vehicle_factory->class29[slot];
    if(source && !owns(a,source))return 0;
    if(!source){PresentationInstanceDefinition d={0};
        if(get(a,0x624f8c)==10)return 0;
        if(!game_model_definition_release(a,asset) || !game_model_definition_read(a,ASSET_MODEL_SIMPLE,slot,0x584768,1,0,1,asset))return 0;
        if(asset->marker_count>INT16_MAX)goto failed;
        memcpy(d.dimensions,asset->dimensions,12);d.type=0x29;d.flags=0xa;d.radius=asset->radius;
        d.resource=asset->markers;d.count=(int32_t)asset->marker_count;d.model=asset->model;
        source=allocate(a,0x170);if(!source)goto failed;
        if(!presentation_instance_create(&a->memory,source,&d,1)){release_object(a,source);goto failed;}
        asset->model=asset->markers=0;if(!vehicle_presentation_class29_finish(&a->memory,source,0))return 0;
        put(a,0x5cee20+slot*4,asset->part_count);put(a,0x5cee30+slot*4,asset->parts);put(a,0x5adae0+slot*4,source);
        if(a->memory.fault)return 0;
    }
    if(!op)return source;
    if(op==2){object=get(a,source+4);if(object){game_list_detach_node(&a->memory,object,8,4);return a->memory.fault?0:object;}count=1;}
    while(count-- >0){object=allocate(a,0x170);if(!object)return 0;
        if(!presentation_instance_create_copy(&a->memory,object,source,1,&copy)){release_object(a,object);return 0;}
        if(!vehicle_presentation_class29_finish(&a->memory,object,source))return 0;
        if(a->memory.fault)return 0;
        if(op==2)return object;
        game_list_insert_after(&a->memory,object,source,4,8);if(a->memory.fault)return 0;
    }
    return source;
failed:(void)game_model_definition_release(a,asset);return 0;
}
int game_level_class29_destroy(GameApplication *a,GameAddress object){
    VehicleAllocation *node;unsigned slot;
    if(!a || !owns(a,object) || game_memory_read_u8(&a->memory,object+12)!=0x29 || a->memory.fault)return 0;
    for(node=a->vehicle_factory->objects;node;node=node->next){const PresentationInstance *instance=presentation_instance_find(node->object);
        if(node->object!=object && instance && instance->definition.source==object)return 0;}
    if(get(a,object+4) || get(a,object+8))game_list_detach_node(&a->memory,object,8,4);
    if(a->memory.fault || !base_release(a,object))return 0;
    for(slot=0;slot<4;slot++)if(get(a,0x5adae0+slot*4)==object)put(a,0x5adae0+slot*4,0);
    return !a->memory.fault && release_object(a,object);
}
/*443cf0/55a7c0: eleven real class26 templates, backed by the same model,
 *presentation and allocation owners as the other pooled scene families. */
GameAddress game_level_aircraft_factory(GameApplication *a,uint16_t slot,unsigned op,int32_t count){
    GameAddress source,object;GameModelDefinition *asset;PresentationConstructionServices copy={a,clone_model};
    if(!a || slot>=11 || op>2 || a->memory.fault)return 0;
    if(!a->vehicle_factory){a->vehicle_factory=calloc(1,sizeof(*a->vehicle_factory));if(!a->vehicle_factory)return 0;}
    source=get(a,0x5ada78+slot*4);asset=&a->vehicle_factory->aircraft[slot];
    if(source && !owns(a,source))return 0;
    if(!source){PresentationInstanceDefinition d={0};
        if(get(a,0x624f8c)==10)return 0;
        if((asset->model || asset->markers || asset->parts) && !game_model_definition_release(a,asset))return 0;
        if(!game_model_definition_read(a,ASSET_MODEL_SIMPLE,slot,0x584708,1,0,1,asset))return 0;
        if(asset->marker_count>INT16_MAX)goto failed_definition;
        memcpy(d.dimensions,asset->dimensions,12);d.type=0x26;d.flags=0x700a;d.radius=asset->radius;
        d.resource=asset->markers;d.count=(int32_t)asset->marker_count;d.model=asset->model;
        source=allocate(a,0x198);if(!source)goto failed_definition;
        if(!presentation_instance_create(&a->memory,source,&d,1)){release_object(a,source);goto failed_definition;}
        asset->model=asset->markers=0;
        put(a,source,0x577a90);game_memory_write_u8(&a->memory,source+12,0x26);put(a,source+16,get(a,source+16)|0x7008);put(a,source+0x194,0);
        put(a,0x63a2c0+slot*4,asset->part_count);put(a,0x63a290+slot*4,asset->parts);put(a,0x5ada78+slot*4,source);
        if(a->memory.fault)return 0;
    }
    if(!op)return source;
    if(op==2){object=get(a,source+4);
        if(object){game_list_detach_node(&a->memory,object,8,4);put(a,object+4,0);put(a,object+8,0);return a->memory.fault?0:object;}count=1;
    }
    while(count>0){object=allocate(a,0x198);if(!object)return 0;
        if(!presentation_instance_create_copy(&a->memory,object,source,1,&copy)){release_object(a,object);return 0;}
        put(a,object,0x577a90);game_memory_write_u8(&a->memory,object+12,0x26);put(a,object+16,get(a,object+16)|0x7008);put(a,object+0x194,0);
        if(a->memory.fault)return 0;
        if(op==2)return object;
        game_list_insert_after(&a->memory,object,source,4,8);if(a->memory.fault)return 0;count--;
    }
    return source;
failed_definition:(void)game_model_definition_release(a,asset);return 0;
}
static int aircraft_gun_release(GameApplication *a,GameAddress object){
    GameAddress gun=get(a,object+0x194);if(a->memory.fault)return 0;
    if(gun){game_leaf_0043b4c0(&a->memory,gun);if(a->memory.fault || !scene_projection_storage_free(gun))return 0;put(a,object+0x194,0);}
    return !a->memory.fault;
}
/*55cd20->539840: copies must retire before their borrowed template hull. */
int game_level_aircraft_destroy(GameApplication *a,GameAddress object){
    VehicleAllocation *node;unsigned slot;
    if(!a || !owns(a,object) || game_memory_read_u8(&a->memory,object+12)!=0x26 || a->memory.fault)return 0;
    for(node=a->vehicle_factory->objects;node;node=node->next){const PresentationInstance *instance=presentation_instance_find(node->object);
        if(node->object!=object && instance && instance->definition.source==object)return 0;}
    if(!aircraft_gun_release(a,object))return 0;
    if(get(a,object+4) || get(a,object+8)){game_list_detach_node(&a->memory,object,8,4);put(a,object+4,0);put(a,object+8,0);}
    if(a->memory.fault || !base_release(a,object))return 0;
    for(slot=0;slot<11;slot++)if(get(a,0x5ada78+slot*4)==object)put(a,0x5ada78+slot*4,0);
    return !a->memory.fault && release_object(a,object);
}
extern int game_level_scene_order_issue(GameApplication *,GameAddress,const PresentationCommand *,int);
static int32_t aircraft_signed(uint32_t bits){int32_t value;memcpy(&value,&bits,4);return value;}
static uint32_t aircraft_truncate(long double value){return (uint32_t)(uint64_t)(!isfinite(value) || value< -0x1p63L || value>=0x1p63L?INT64_MIN:(int64_t)value);}
static void aircraft_float(GameApplication *a,GameAddress p,float value){uint32_t bits;memcpy(&bits,&value,4);put(a,p,bits);}
/*450d50: class29's catalog component query is a verified zero return. */
int game_level_class29_place(GameApplication *a,GameAddress o,uint32_t identity,const float p[3],const uint16_t angles[3]){
    GameAddress root,method;
    if(!a || !owns(a,o) || get(a,o)!=0x573230 || a->memory.fault)return 0;
    root=get(a,0x5f0210+(identity&63)*4);method=get(a,get(a,root)+0x70);
    if(method!=0x4777e0 && method!=0x478ff0)return 0;
    return game_level_entity_scene_place(a,o,identity,p,angles);
}
/*450ef0 derives planar velocity, a target heading and the close-range heading
 *lock after the shared destination publisher. No register/call-frame state. */
int game_level_class29_move(GameApplication *a,GameAddress o,float x,float z){
    float dx,dz,point[3],distance,threshold;long double factor;uint16_t yaw,current;int16_t pitch;
    GameAddress catalog;uint32_t speed;int difference;
    if(!scene_order_move(&a->memory,o,0x4ca140,x,z,NULL))return 0;
    dx=(float)((long double)real(a,o+0xf8)-real(a,o+24));dz=(float)((long double)real(a,o+0x100)-real(a,o+32));
    if(dx==0 && dz==0){put(a,o+0xe0,5);return !a->memory.fault;}
    factor=(long double)real(a,o+0x128)/sqrtl((long double)dz*dz+(long double)dx*dx);
    aircraft_float(a,o+0xb8,(float)((long double)dz*factor));aircraft_float(a,o+0xb4,(float)((long double)dx*factor));
    point[0]=real(a,o+0xf8);point[1]=real(a,o+0xfc);point[2]=real(a,o+0x100);
    if(!vehicle_targeting_point_angles(&a->memory,o,point,&yaw,&pitch))return 0;
    game_memory_write_u16(&a->memory,o+0x13c,yaw);
    catalog=get(a,0x5f0210+(get(a,o+20)&63)*4);
    if(!game_level_player_catalog_value(a,catalog,0x29,5,(game_memory_read_u8(&a->memory,o+0xd8)&4)!=0,&speed))return 0;
    threshold=(float)((long double)aircraft_signed(speed)*real(a,0x573404));
    dx=fabsf(dx);dz=fabsf(dz);
    distance=(float)(dx>dz?(long double)dz*real(a,0x571f18)+dx:(long double)dx*real(a,0x571f18)+dz);
    current=game_memory_read_u16(&a->memory,o+36);difference=(int)(yaw&1023)-(int)(current&1023);
    if(!(distance>=threshold) && abs(difference)<28)game_memory_write_u16(&a->memory,o+0x13c,current);
    return !a->memory.fault;
}
/*410ef0's real lookup arithmetic; inputs are rounded coordinate deltas. */
static uint16_t aircraft_heading(GameApplication *a,float x,float z){
    long double scale=real(a,0x571f34),value;uint32_t index,word;uint16_t angle;
    if(fabsl(x)<fabsl(z)){
        value=(long double)x*scale/z+scale;index=aircraft_truncate(value);angle=game_memory_read_u16(&a->memory,0x62e8b0+index*2);
        memcpy(&word,&z,4);return (angle+(aircraft_signed(word)>0?0u:1024u))&2047;
    }
    value=(long double)z*scale/x+scale;index=aircraft_truncate(value);memcpy(&word,&x,4);
    angle=(uint16_t)((aircraft_signed(word)>0?512u:1536u)-game_memory_read_u16(&a->memory,0x62e8b0+index*2));return angle&2047;
}
/*55ab20: the same activation program supports explicit destinations and target
 *objects. Imperial4bcc20 supplies(-1,-1), which deliberately skips new orders. */
static int aircraft_place(GameApplication *a,GameAddress o,uint32_t identity,const float position[3],
        const uint16_t supplied[3],const float destination[2],GameAddress target,int activate){
    GameAddress root,record,motion,gun=0,local;float p[3],goal[3],point[3]={1,0,0};uint16_t angles[3];uint32_t mode,tick,value;
    SoundPlaybackRequest sound={0};GameSpatialAttachmentInit init={0};
    if(!a || !owns(a,o) || get(a,o)!=0x577a90 || a->memory.fault)return 0;
    root=get(a,0x5f0210+(identity&63)*4);mode=get(a,get(a,root)+0x70);
    if(mode==0x478ff0){if(get(a,root+0x224))game_memory_write_u8(&a->memory,o+0xd8,game_memory_read_u8(&a->memory,o+0xd8)|4);}
    else if(mode!=0x4777e0)return 0;
    game_memory_write_u16(&a->memory,o+0x13c,0);game_memory_write_u16(&a->memory,o+0x140,0);
    put(a,o+0x188,0);put(a,o+0x18c,0);put(a,o+0x190,0);put(a,o+0x154,0x0fffffff);
    if(!activate)put(a,o+0x15c,0);
    value=get(a,0x63a2bc);put(a,o+0x16c,value);
    memcpy(p,position,12);p[1]=(float)(gameplay_geometry_terrain_height(&a->memory,p[0],p[2])+real(a,o+0x16c));
    aircraft_float(a,o+0x170,p[1]);aircraft_float(a,o+0x174,p[1]);put(a,o+0x178,0);put(a,o+0x17c,0);
    memcpy(angles,supplied,6);
    if(activate){
        goal[0]=target?real(a,target+24):destination[0];goal[2]=target?real(a,target+32):destination[1];goal[1]=p[1];
        angles[0]=aircraft_heading(a,(float)((long double)goal[0]-position[0]),(float)((long double)goal[2]-position[2]));
    }
    if(a->memory.fault || !game_level_entity_scene_place(a,o,identity,p,angles))return 0;
    aircraft_float(a,0x63a2bc,(float)((long double)real(a,0x63a2bc)+real(a,0x577c64)));
    if(activate){uint32_t x,z;memcpy(&x,destination,4);memcpy(&z,destination+1,4);
        if(target || (x<=0x80000000 && z<=0x80000000)){
            PresentationCommand command={0};command.kind=2;command.has_position=1;command.parameter=0x140;memcpy(command.position,goal,12);
            if(!game_level_scene_order_issue(a,o,&command,0))return 0;
            command.kind=5;command.parameter=0;command.target=target;command.has_target=target!=0;command.target_identity=target?get(a,target+20):0;
            if(!game_level_scene_order_issue(a,o,&command,0))return 0;
        }}
    {GameAddress reference=get(a,0x58f890);long double x=(long double)p[0]-real(a,reference+12),z=(long double)p[2]-real(a,reference+20);
        sound.sound=24;sound.volume=1;sound.flags=sound.force=1;sound.distance_squared=(float)(x*x+z*z);
        if(!game_menu_sound_play(a,&sound))return 0;put(a,o+0x180,get(a,get(a,0x628a90)));}
    local=get(a,0x5f0208);tick=get(a,0x5ac6ac);
    if((identity&63)==game_memory_read_u16(&a->memory,local+8) && aircraft_signed(tick-get(a,0x5a0998))>96){
        uint32_t random;put(a,0x5a0998,tick);put(a,local+0x58c,tick);random=portable_crt_rand_step(&a->memory);
        sound=(SoundPlaybackRequest){0};sound.sound=(int16_t)(49u+((random*2u)>>15));sound.volume=1;
        game_memory_write_u16(&a->memory,local+0x598,(uint16_t)sound.sound);if(!game_menu_sound_play(a,&sound))return 0;
    }
    if(!game_level_player_catalog_record(a,root,0x26,&record))return 0;
    mode=(game_memory_read_u8(&a->memory,o+0xd8)&4)!=0;
    put(a,o+0x184,get(a,record+(mode?0x6c:0x18)));value=get(a,record+(mode?0x70:0x1c));
    put(a,o+0x158,value);put(a,o+0x160,value);motion=get(a,root+(mode?0x1e4:0x1e0));
    if(aircraft_signed(get(a,motion+0x14))>0){
        gun=scene_projection_storage_allocate(0xbc,"class26 gun");if(!gun)return 0;
        init.parent=o;init.motion=motion;init.heading=17;memcpy(init.position,point,12);memcpy(init.secondary_position,point,12);init.resource=1;
        if(!game_spatial_attachment_construct(&a->memory,gun,&init)){scene_projection_storage_free(gun);return 0;}
    }
    if(!aircraft_gun_release(a,o)){if(gun)scene_projection_storage_free(gun);return 0;}
    put(a,o+0x194,gun);return !a->memory.fault;
}
/*55c710's attachment reconstruction precedes its archived quantity/tick. */
int game_level_aircraft_archive_gun(GameApplication *a,GameAddress o){
    GameAddress root,motion,gun=0;GameSpatialAttachmentInit init={0};
    if(!a || !owns(a,o) || get(a,o)!=0x577a90 || a->memory.fault)return 0;
    root=get(a,0x5f0210+(get(a,o+20)&63u)*4);
    motion=get(a,root+((game_memory_read_u8(&a->memory,o+0xd8)&4)?0x1e4:0x1e0));
    if(aircraft_signed(get(a,motion+0x14))>0){
        gun=scene_projection_storage_allocate(0xbc,"class26 archive gun");if(!gun)return 0;
        init.parent=o;init.motion=motion;init.heading=17;init.resource=1;
        init.position[0]=init.secondary_position[0]=1;
        if(!game_spatial_attachment_construct(&a->memory,gun,&init)){scene_projection_storage_free(gun);return 0;}
    }
    if(!aircraft_gun_release(a,o)){if(gun)scene_projection_storage_free(gun);return 0;}
    put(a,o+0x194,gun);return !a->memory.fault;
}
int game_level_aircraft_place(GameApplication *a,GameAddress o,uint32_t identity,const float position[3],const uint16_t angles[3]){
    return aircraft_place(a,o,identity,position,angles,NULL,0,0);
}
int game_level_aircraft_activate(GameApplication *a,GameAddress o,uint32_t identity,const float position[3],
        const uint16_t angles[3],const float destination[2],GameAddress target){
    return aircraft_place(a,o,identity,position,angles,destination,target,1);
}
/*55b1d0: the command kind selects altitude; velocity, arrival time and bank
 *are derived from the same rounded delta vector used by the retail route. */
int game_level_aircraft_destination(GameApplication *a,GameAddress o,const float point[3]){
    float delta[3],square;long double length,scale;uint16_t yaw,word;int16_t difference;uint32_t x,z;
    if(!a || get(a,o)!=0x577a90 || a->memory.fault)return 0;
    aircraft_float(a,o+0xf8,point[0]);aircraft_float(a,o+0x100,point[2]);put(a,o+0x178,0);put(a,o+0xe0,0);
    if(get(a,get(a,o+0xdc)+12)==2){
        aircraft_float(a,o+0xfc,point[1]);aircraft_float(a,o+0xe8,point[1]);
        delta[1]=(float)((long double)point[1]-real(a,o+28));
    }else{put(a,o+0xfc,get(a,o+0x170));put(a,o+0xe8,get(a,o+0x170));delta[1]=0;}
    delta[0]=(float)((long double)point[0]-real(a,o+24));delta[2]=(float)((long double)point[2]-real(a,o+32));
    square=(float)(((long double)delta[2]*delta[2]+(long double)delta[1]*delta[1])+(long double)delta[0]*delta[0]);
    length=gameplay_geometry_square_root(square);scale=(long double)real(a,o+0x128)/length;
    aircraft_float(a,o+0xb8,(float)((long double)delta[2]*scale));aircraft_float(a,o+0xb4,(float)((long double)delta[0]*scale));
    aircraft_float(a,o+0x15c,(float)((long double)delta[1]*scale));
    put(a,o+0x164,get(a,0x5ac6ac)+aircraft_truncate(length/real(a,o+0x128)));
    memcpy(&x,delta,4);memcpy(&z,delta+2,4);
    yaw=((x&0x7fffffff) && (z&0x7fffffff))?aircraft_heading(a,delta[0],delta[2]):game_memory_read_u16(&a->memory,o+36);
    game_memory_write_u16(&a->memory,o+0x13c,yaw);
    word=(uint16_t)(game_memory_read_u16(&a->memory,o+36)-yaw);memcpy(&difference,&word,2);
    if(difference>1024)word=(uint16_t)(word-2048);else if(difference< -1024)word=(uint16_t)(word+2048);
    memcpy(&difference,&word,2);game_memory_write_u16(&a->memory,o+0x140,word);
    game_memory_write_u16(&a->memory,o+0x13e,(uint16_t)(difference>=0?difference/2:-((-(int)difference+1)/2)));
    return !a->memory.fault;
}
/*55b190 preserves the original terrain-plus-current-altitude argument. */
int game_level_aircraft_move(GameApplication *a,GameAddress o,float x,float z){
    float point[3]={x,0,z};point[1]=(float)(gameplay_geometry_terrain_height(&a->memory,x,z)+real(a,o+0x170));
    return get(a,get(a,o)+0x170)==0x55b1d0 && game_level_aircraft_destination(a,o,point);
}
int game_level_imperial_unit_activate(GameApplication *a,GameAddress o,uint32_t identity,const float p[3]){
    const uint16_t angles[3]={0};const float destination[2]={-1,-1};
    return get(a,get(a,o)+0x1bc)==0x55ab20 && game_level_aircraft_activate(a,o,identity,p,angles,destination,0);
}
extern int game_level_catalog_return_health(GameApplication *,GameAddress,uint8_t,float);
/*55b370: refund remaining production time, unwind altitude allocation, then
 *retire the gun and shared presentation membership through their owners. */
int game_level_aircraft_reset(GameApplication *a,GameAddress o){
    uint32_t tick,maximum;GameAddress local,root;long double altitude;
    if(!a || !owns(a,o) || get(a,o)!=0x577a90 || a->memory.fault)return 0;
    if(!game_menu_sound_stop(a,24,get(a,o+0x180)))return 0;
    tick=get(a,0x5ac6ac);local=get(a,0x5f0208);
    if(!get(a,o+0x58)){
        if(aircraft_signed(tick-get(a,0x5a099c))>96 && (get(a,o+20)&63)==game_memory_read_u16(&a->memory,local+8)){
            SoundPlaybackRequest sound={0};put(a,0x5a099c,tick);put(a,local+0x58c,tick);
            game_memory_write_u16(&a->memory,local+0x598,53);sound.sound=53;sound.volume=1;
            if(!game_menu_sound_play(a,&sound))return 0;
        }
    }else if(!(get(a,o+16)&0x20000)){
        root=get(a,0x5f0210+(get(a,o+20)&63)*4);
        if(!game_level_scene_property(a,o,get(a,get(a,o)+0x44),&maximum) ||
           !game_level_catalog_return_health(a,root,0x26,(float)((long double)aircraft_signed(get(a,o+0x58))/aircraft_signed(maximum))))return 0;
    }
    altitude=(long double)real(a,0x63a2bc)-real(a,0x577c64);
    aircraft_float(a,0x63a2bc,(float)altitude);
    if(isnan(altitude) || altitude<real(a,0x577c80))put(a,0x63a2bc,0x455ac000);
    return aircraft_gun_release(a,o) && game_level_presentation_reset(a,o,2);
}
/*443da1/446683/447a80: each of the eleven class27 templates owns the model and
 *markers; copies borrow markers, clone the model and own both render buffers.
 *The factory's existing allocation list also owns acquired, unlinked copies.
 *Only the points/parts remain in the asset owner after template publication. */
GameAddress game_level_infantry_factory(GameApplication *a,uint16_t slot,unsigned op,int32_t count){
    GameAddress source,object;GameModelDefinition *asset;
    PresentationConstructionServices copy={a,clone_model};
    if(!a || slot>=11 || op>2 || a->memory.fault)return 0;
    if(!a->vehicle_factory){a->vehicle_factory=calloc(1,sizeof(*a->vehicle_factory));if(!a->vehicle_factory)return 0;}
    source=get(a,0x5adbc8+slot*4);asset=&a->vehicle_factory->infantry[slot];
    if(a->memory.fault || (source && !owns(a,source)))return 0;
    if(!source){PresentationInstanceDefinition d={0};
        if(get(a,0x624f8c)==10 || a->memory.fault)return 0;
        /*A failed prior construction retains its asset outputs for cleanup. */
        if((asset->model || asset->markers || asset->points || asset->parts) &&
           !game_model_definition_release(a,asset))return 0;
        if(!game_model_definition_read(a,ASSET_MODEL_SIMPLE,slot,0x584738,1,1,1,asset))return 0;
        if(asset->marker_count>INT16_MAX)goto definition_failed;
        memcpy(d.dimensions,asset->dimensions,sizeof(d.dimensions));d.type=0x27;
        d.flags=0x700a;d.radius=asset->radius;d.resource=asset->markers;
        d.count=(int32_t)asset->marker_count;d.model=asset->model;
        source=allocate(a,0x19c);if(!source)goto definition_failed;
        if(!presentation_instance_create(&a->memory,source,&d,1)){
            release_object(a,source);goto definition_failed;
        }
        /*Ownership has transferred, including if a later publication fails. */
        asset->model=asset->markers=0;
        game_initializer_presentation_object(&a->memory,source,get(a,source+16)|0x7008);
        put(a,0x5adca0+slot*4,asset->point_count);put(a,0x5adaa8+slot*4,asset->points);
        put(a,0x631af8+slot*4,asset->part_count);put(a,0x631ac8+slot*4,asset->parts);
        put(a,0x5adbc8+slot*4,source);
        if(a->memory.fault)return 0;
    }
    if(!op)return source;
    if(op==2){object=get(a,source+4);
        if(object){game_list_detach_node(&a->memory,object,8,4);put(a,object+4,0);put(a,object+8,0);return a->memory.fault?0:object;}
        count=1;
    }
    while(count>0){
        object=allocate(a,0x19c);if(!object)return 0;
        if(!presentation_instance_create_copy(&a->memory,object,source,1,&copy)){
            release_object(a,object);return 0;
        }
        game_initializer_presentation_object(&a->memory,object,get(a,object+16)|0x7008);
        if(a->memory.fault)return 0;
        if(op==2)return object;
        game_list_insert_after(&a->memory,object,source,4,8);
        if(a->memory.fault)return 0;
        --count;
    }
    return 1;
definition_failed:
    (void)game_model_definition_release(a,asset);return 0;
}
static int infantry_guns_release(GameApplication *a,GameAddress object){
    static const unsigned fields[]={0x13c,0x144,0x140};unsigned i;
    for(i=0;i<3;i++){GameAddress gun=get(a,object+fields[i]);
        if(a->memory.fault)return 0;
        if(!gun)continue;
        game_leaf_0043b4c0(&a->memory,gun);
        if(a->memory.fault || !scene_projection_storage_free(gun))return 0;
        put(a,object+fields[i],0);
    }
    return !a->memory.fault;
}
/*544410 ->539c10: retire the three actual attachment owners before returning
 *commands, spatial/catalog membership and the instance to its template pool. */
int game_level_infantry_reset(GameApplication *a,GameAddress object){
    return a && owns(a,object) && game_memory_read_u8(&a->memory,object+12)==0x27 &&
        infantry_guns_release(a,object) && game_level_presentation_reset(a,object,2);
}
int game_level_infantry_destroy(GameApplication *a,GameAddress object){
    VehicleAllocation *node;unsigned slot;
    if(!a || !owns(a,object) || game_memory_read_u8(&a->memory,object+12)!=0x27 || a->memory.fault)return 0;
    for(node=a->vehicle_factory->objects;node;node=node->next){
        const PresentationInstance *instance=presentation_instance_find(node->object);
        if(node->object!=object && instance && instance->definition.source==object)return 0;
    }
    if(!infantry_guns_release(a,object))return 0;
    if(get(a,object+4) || get(a,object+8)){
        game_list_detach_node(&a->memory,object,8,4);put(a,object+4,0);put(a,object+8,0);
    }
    if(a->memory.fault || !base_release(a,object))return 0;
    for(slot=0;slot<11;slot++)if(get(a,0x5adbc8+slot*4)==object)put(a,0x5adbc8+slot*4,0);
    return !a->memory.fault && release_object(a,object);
}
/*544260: the ordinary world transform also updates this class's local probe.
 *Use the same authoritative pose/cache and bounded legacy probe words. */
int game_level_infantry_geometry(GameApplication *a,GameAddress object,unsigned operation){
    PresentationPose *pose=presentation_instance_pose(object);unsigned i;
    if(!pose || a->memory.fault || operation>3)return 0;
    if(operation==0){
        if(!game_memory_read(&a->memory,object+24,pose->position,12))return 0;
        for(i=0;i<3;i++)pose->angles[i]=game_memory_read_u16(&a->memory,object+36+i*2);
        return presentation_pose_build_world(&a->memory,pose) && game_memory_write(&a->memory,object+0x80,pose->world_matrix,48);
    }
    if(operation==1)return presentation_pose_build_controller(&a->memory,pose);
    if(operation==3)return presentation_pose_transform_vertices(&a->memory,pose,1);
    return presentation_pose_transform_infantry_vertices(&a->memory,object,pose);
}
/*546a70: actual class27 gun builder. All three records use the already
 *reconstructed43bc70 attachment constructor and43b4c0 retirement. */
int game_level_infantry_guns_build(GameApplication *a,GameAddress object,GameAddress catalog){
    GameSpatialAttachmentInit init={0};GameAddress guns[3]={0},motions[3],points;
    uint32_t slot;unsigned i,j,alternate=(game_memory_read_u8(&a->memory,object+0xd8)&4)!=0;
    static const unsigned fields[]={0x13c,0x140,0x144};
    if(!game_level_player_catalog_value(a,catalog,0x27,3,0,&slot) || slot>=11 ||
       get(a,0x5adca0+slot*4)<2 || !(points=get(a,0x5adaa8+slot*4)) || a->memory.fault)return 0;
    motions[0]=get(a,catalog+(alternate?0x1b8:0x1b4));
    motions[1]=get(a,catalog+(alternate?0x1ec:0x1e8));
    motions[2]=get(a,catalog+(alternate?0x1f4:0x1f0));
    for(i=0;i<3;i++){
        if(i && (int32_t)get(a,motions[i]+0x14)<=0)continue;
        memset(&init,0,sizeof(init));init.parent=object;init.motion=motions[i];
        init.heading=(uint16_t)(i==0?13:i==1?17:11);init.resource=1;
        if(i==0){init.orient_to_motion=1;
            for(j=0;j<3;j++){init.position[j]=real(a,points+j*4);init.secondary_position[j]=real(a,points+12+j*4);}
        }else{init.position[0]=init.secondary_position[0]=1;
            if(i==2)init.slot=get(a,motions[i]+0x20);}
        guns[i]=scene_projection_storage_allocate(0xbc,"infantry gun");
        if(!guns[i] || a->memory.fault || !game_spatial_attachment_construct(&a->memory,guns[i],&init))goto failed;
    }
    if(!infantry_guns_release(a,object))goto failed;
    for(i=0;i<3;i++){
        put(a,object+fields[i],guns[i]);
        if(a->memory.fault)goto failed;
        guns[i]=0;
    }
    return !a->memory.fault;
failed:
    for(i=0;i<3;i++)if(guns[i])scene_projection_storage_free(guns[i]);return 0;
}
/*5436e0 ->539a70 ->546a70: real placement and command initialization,
 *followed by the actual gun owners and local-player positional feedback. */
int game_level_infantry_place(GameApplication *a,GameAddress object,uint32_t identity,const float position[3],const uint16_t angles[3]){
    GameAddress catalog,local;uint32_t cursor,enabled,bits;int32_t sample;float value;
    if(!a || !owns(a,object) || a->memory.fault)return 0;
    put(a,object+0x17c,0);put(a,object+0x180,0);put(a,object+0x164,0);put(a,object+0x168,0);
    put(a,object+0x178,UINT32_MAX);put(a,object+0x16c,UINT32_MAX);put(a,object+0x170,0x44fa0000);
    cursor=get(a,0x6308c0);if(cursor==1024)cursor=0;if(cursor>=1024 || a->memory.fault)return 0;
    {long double f=(long double)real(a,0x6308c8+cursor*4)*real(a,0x576f88);
        uint32_t word=(uint32_t)(uint64_t)(!isfinite(f)||f< -0x1p63L||f>=0x1p63L?INT64_MIN:(int64_t)f);
        memcpy(&sample,&word,4);}
    put(a,0x6308c0,cursor+1);value=(float)((long double)sample+real(a,0x576f8c));memcpy(&bits,&value,4);put(a,object+0x174,bits);
    put(a,object+0x160,0);put(a,object+0x148,0);
    catalog=get(a,0x5f0210+(identity&63)*4);
    switch(get(a,get(a,catalog)+0x70)){
    case 0x4777e0:enabled=0;break;
    case 0x478ff0:enabled=get(a,catalog+0x220);break;
    default:return 0;
    }
    if(enabled)game_memory_write_u8(&a->memory,object+0xd8,(uint8_t)(game_memory_read_u8(&a->memory,object+0xd8)|4));
    if(a->memory.fault || !game_level_entity_scene_place(a,object,identity,position,angles) ||
       !game_level_infantry_guns_build(a,object,catalog))return 0;
    local=get(a,0x5f0208);
    if((get(a,object+0x14)&63)==game_memory_read_u16(&a->memory,local+8) &&
       (int32_t)(get(a,0x5ac6ac)-get(a,local+0x58c))>96){
        put(a,local+0x58c,get(a,0x5ac6ac));game_memory_write_u16(&a->memory,local+0x598,75);
        SoundPlaybackRequest sound={75,0,1,0,0,0};
        if(!game_menu_sound_play(a,&sound))return 0;
    }
    return !a->memory.fault;
}
static int describe(void *context,uint16_t slot,VehiclePresentationDefinition *out){
    VehicleFactoryOperation *operation=context;GameApplication *a=operation->app;const VehiclePresentationFamily *family=operation->family;
    GameModelDefinition *owner=&a->vehicle_factory->assets[family->type-0x21][slot];
    SceneAssetRequest request={0};GameAddress work;unsigned i,j;uint64_t total;int ok;
    static const unsigned offsets[14]={0,12,16,20,24,28,32,44,56,68,72,76,80,84};
    if(owner->model || owner->markers || owner->points || owner->parts)return 0;
    if(get(a,family->point_counts+slot*4) || get(a,family->points+slot*4) ||
       get(a,family->part_counts+slot*4) || get(a,family->parts+slot*4) || a->memory.fault)return 0;
    work=scene_projection_storage_allocate(88,"citadel vehicle definition outputs");if(!work)return 0;
    request.format=ASSET_MODEL_THREE_MESH;request.index=slot;request.names=family->names;
    for(i=0;i<14;i++)request.outputs[i]=work+offsets[i];
    ok=game_model_asset_definition_load(a,&request);
    /* Capture ownership before testing the result, so partial producer outputs
     * remain reachable for cleanup. A clean failure can be retried. */
    owner->format=ASSET_MODEL_THREE_MESH;owner->model=get(a,work+68);owner->markers=get(a,work+16);
    owner->point_count=get(a,work+72);owner->points=get(a,work+76);
    owner->part_count=get(a,work+80);owner->parts=get(a,work+84);
    a->vehicle_factory->defined[family->type-0x21][slot]=owner->model || owner->markers || owner->points || owner->parts;
    total=0;
    for(i=0;i<3;i++){out->base.dimensions[i]=real(a,work+i*4);out->groups[i]=get(a,work+20+i*4);total+=out->groups[i];
        for(j=0;j<3;j++)out->anchors[i][j]=real(a,work+32+i*12+j*4);}
    owner->marker_count=(uint32_t)total;out->base.radius=get(a,work+12);
    out->base.resource=owner->markers;out->base.model=owner->model;
    scene_projection_storage_free(work);
    if(!ok || total>INT16_MAX || a->memory.fault){(void)game_model_definition_release(a,owner);return 0;}
    /*536e30 consumes the model and markers. The cache owner retains only the
     * point/part arrays used by later auxiliary construction. */
    if(family->type!=0x23)game_memory_write_u8(&a->memory,owner->model+0x3c,game_memory_read_u8(&a->memory,owner->model+0x3c)|2);
    a->vehicle_factory->defined[family->type-0x21][slot]=1;
    put(a,family->point_counts+slot*4,owner->point_count);put(a,family->points+slot*4,owner->points);
    put(a,family->part_counts+slot*4,owner->part_count);put(a,family->parts+slot*4,owner->parts);
    return !a->memory.fault;
}
static GameAddress family_allocate(void *context,size_t size){return allocate(((VehicleFactoryOperation *)context)->app,size);}
static int family_release(void *context,GameAddress p){return release_object(((VehicleFactoryOperation *)context)->app,p);}
GameAddress game_level_vehicle_factory(GameApplication *a,uint8_t kind,uint16_t slot,unsigned operation,int32_t count){
    VehiclePresentationFactoryServices s={0};GameAddress published,result;
    const VehiclePresentationFamily *family=vehicle_presentation_family(kind);VehicleFactoryOperation context={a,family};
    if(!a || !family || slot>=family->slots || operation>2 || a->memory.fault)return 0;
    if(!a->vehicle_factory){a->vehicle_factory=calloc(1,sizeof(*a->vehicle_factory));if(!a->vehicle_factory)return 0;}
    published=get(a,family->templates+slot*4);
    if(a->memory.fault || (published && !owns(a,published)))return 0;
    s.context=&context;s.type=kind;s.describe=describe;s.allocate=family_allocate;s.release=family_release;
    s.construction.context=a;s.construction.clone_model=clone_model;
    s.retirement.context=a;s.retirement.release_model=release_model;s.retirement.release_storage=release_page;
    result=operation==0?vehicle_presentation_template(&a->memory,slot,&s):operation==1?
        (GameAddress)vehicle_presentation_fill(&a->memory,slot,count,&s):vehicle_presentation_take(&a->memory,slot,&s);
    if(!published){
        GameModelDefinition *owner=&a->vehicle_factory->assets[kind-0x21][slot];
        GameMemory cleanup=a->memory;cleanup.fault=0;
        if(game_memory_read_u32(&cleanup,family->templates+slot*4))owner->model=owner->markers=0;
        else if(a->vehicle_factory->defined[kind-0x21][slot]){
            (void)game_model_definition_release(a,owner);
            game_memory_write_u32(&cleanup,family->point_counts+slot*4,0);game_memory_write_u32(&cleanup,family->points+slot*4,0);
            game_memory_write_u32(&cleanup,family->part_counts+slot*4,0);game_memory_write_u32(&cleanup,family->parts+slot*4,0);
            a->vehicle_factory->defined[kind-0x21][slot]=0;
        }
    }
    if(operation==2 && result){put(a,result+4,0);put(a,result+8,0);}
    return result;
}
int game_level_vehicle_factory_destroy(GameApplication *a,GameAddress object){
    VehicleRetirementServices s={0};VehicleAllocation *node;unsigned i,j;uint8_t kind;
    if(!a || !owns(a,object))return 0;
    kind=game_memory_read_u8(&a->memory,object+12);if(!vehicle_presentation_family(kind))return 0;
    for(node=a->vehicle_factory->objects;node;node=node->next){
        const PresentationInstance *instance=presentation_instance_find(node->object);
        if(node->object!=object && instance && instance->definition.source==object)return 0;
    }
    if(get(a,object+4) || get(a,object+8)){
        (void)game_list_detach_node(&a->memory,object,8,4);put(a,object+4,0);put(a,object+8,0);
        if(a->memory.fault)return 0;
    }
    s.context=a;s.return_event=return_event;s.release_storage=release_page;s.base_release=base_release;
    if(!(kind==0x23?vehicle_presentation_destroy(&a->memory,object,0,&s):vehicle_presentation_base_destroy(&a->memory,object,&s)))return 0;
    for(i=0;i<8;i++){const VehiclePresentationFamily *family=vehicle_presentation_family((uint8_t)(0x21+i));
        if(!family)continue;
        for(j=0;j<family->slots;j++)if(get(a,family->templates+j*4)==object)put(a,family->templates+j*4,0);
    }
    return !a->memory.fault && release_object(a,object);
}
static int shutdown_owned(GameApplication *a){
    GameAddress templates[47];unsigned i,j,n=0;
    if(!a)return 0;
    if(!a->vehicle_factory)return 1;
    for(i=0;i<8;i++){const VehiclePresentationFamily *family=vehicle_presentation_family((uint8_t)(0x21+i));
        if(!family)continue;
        for(j=0;j<family->slots;j++)templates[n++]=get(a,family->templates+j*4);
    }
    for(i=0;i<11;i++)templates[n++]=get(a,0x5adbc8+i*4);
    for(i=0;i<11;i++)templates[n++]=get(a,0x5ada78+i*4);
    for(i=0;i<4;i++)templates[n++]=get(a,0x5adae0+i*4);
    templates[n++]=get(a,0x5adbf8);
    /* Copies borrow template hulls: retire every copy before any template. */
    for(;;){VehicleAllocation *node;GameAddress object=0;
        for(node=a->vehicle_factory->objects;node;node=node->next){
            for(i=0;i<n;i++)if(node->object==templates[i])break;
            if(i==n){object=node->object;break;}
        }
        if(!object)break;
        if(!(game_memory_read_u8(&a->memory,object+12)==0x40?game_level_class40_destroy(a,object):game_memory_read_u8(&a->memory,object+12)==0x29?game_level_class29_destroy(a,object):game_memory_read_u8(&a->memory,object+12)==0x26?game_level_aircraft_destroy(a,object):game_memory_read_u8(&a->memory,object+12)==0x27?
             game_level_infantry_destroy(a,object):game_level_vehicle_factory_destroy(a,object)))return 0;
    }
    for(i=0;i<n;i++){
        if(templates[i] && !(game_memory_read_u8(&a->memory,templates[i]+12)==0x40?game_level_class40_destroy(a,templates[i]):game_memory_read_u8(&a->memory,templates[i]+12)==0x29?game_level_class29_destroy(a,templates[i]):game_memory_read_u8(&a->memory,templates[i]+12)==0x26?game_level_aircraft_destroy(a,templates[i]):game_memory_read_u8(&a->memory,templates[i]+12)==0x27?
            game_level_infantry_destroy(a,templates[i]):game_level_vehicle_factory_destroy(a,templates[i])))return 0;
    }
    for(i=0;i<8;i++){const VehiclePresentationFamily *family=vehicle_presentation_family((uint8_t)(0x21+i));
        if(!family)continue;
        for(j=0;j<family->slots;j++){
            if(!a->vehicle_factory->defined[i][j])continue;
            if(!game_model_definition_release(a,&a->vehicle_factory->assets[i][j]))return 0;
            put(a,family->point_counts+j*4,0);put(a,family->points+j*4,0);
            put(a,family->part_counts+j*4,0);put(a,family->parts+j*4,0);
            a->vehicle_factory->defined[i][j]=0;
        }
    }
    for(i=0;i<4;i++){
        if(!game_model_definition_release(a,&a->vehicle_factory->class29[i]))return 0;
        put(a,0x5cee20+i*4,0);put(a,0x5cee30+i*4,0);
    }
for(i=0;i<11;i++){
        if(!game_model_definition_release(a,&a->vehicle_factory->infantry[i]))return 0;
        if(!game_model_definition_release(a,&a->vehicle_factory->aircraft[i]))return 0;
        put(a,0x63a2c0+i*4,0);put(a,0x63a290+i*4,0);
        put(a,0x5adca0+i*4,0);put(a,0x5adaa8+i*4,0);put(a,0x631af8+i*4,0);put(a,0x631ac8+i*4,0);
    }
    if(a->memory.fault)return 0;
    free(a->vehicle_factory);a->vehicle_factory=NULL;return 1;
}
int game_level_vehicle_factory_shutdown(GameApplication *a){
    int saved_fault,ok;GameAddress saved_address;uint32_t saved_size;
    if(!a)return 0;
    saved_fault=a->memory.fault;saved_address=a->memory.fault_address;saved_size=a->memory.fault_size;
    a->memory.fault=0;ok=shutdown_owned(a);
    if(saved_fault){a->memory.fault=saved_fault;a->memory.fault_address=saved_address;a->memory.fault_size=saved_size;}
    return ok;
}
static int place_entity(void *context,GameAddress object,uint32_t identity,GameAddress point,const uint16_t angles[3]){
    GameApplication *a=context;float position[3];unsigned i;
    for(i=0;i<3;i++)position[i]=real(a,point+i*4);
    return !a->memory.fault && game_level_entity_scene_place(a,object,identity,position,angles);
}
static int catalog_value(void *context,GameAddress catalog,uint8_t kind,uint32_t property,uint32_t alternate,uint32_t *out){
    return game_level_player_catalog_value(context,catalog,kind,property,alternate,out);
}
static GameAddress auxiliary_allocate(void *context,size_t size){(void)context;return scene_projection_storage_allocate(size,"vehicle auxiliary");}
static int prepare_vehicle(void *context,GameAddress object,GameAddress point,uint16_t heading){
    GameApplication *a=context;float position[3];unsigned i;GameAddress method=get(a,get(a,object)+0x68);
    for(i=0;i<3;i++)position[i]=real(a,point+i*4);
    if(a->memory.fault || (method!=0x42ed30 && method!=0x406ea0) || !game_level_object_footprint(a,object,position,heading)){
        fprintf(stderr,"native vehicle footprint failed: object=%08x method=%08x\n",object,method);return 0;}
    for(i=0;i<3;i++){uint32_t bits;memcpy(&bits,position+i,4);put(a,point+i*4,bits);}
    return !a->memory.fault;
}
static int component_enabled(void *context,GameAddress catalog,uint8_t kind,uint32_t *out){
    GameApplication *a=context;GameAddress method=get(a,get(a,catalog)+0x70);
    if(method==0x4777e0)*out=0;
    else if(method==0x478ff0 && kind==0x21)*out=get(a,catalog+0x21c);
    else return 0;
    return !a->memory.fault;
}
/*4e9360/408320: actual grouped-vehicle auxiliary consumers use the same point
 *cache and spatial attachment constructor as their parent factory. Publish only
 *after construction; repeated placement retires the prior owned auxiliaries. */
static int build_auxiliaries(void *context,GameAddress object,GameAddress catalog,GameAddress method){
    GameApplication *a=context;uint8_t kind=method==0x4e9360?0x21:method==0x4091c0?0x24:method==0x408b60?0x25:method==0x408ea0?0x28:0x22;
    const VehiclePresentationFamily *family=vehicle_presentation_family(kind);
    GameSpatialAttachmentInit init={0};GameAddress created[2]={0},points,motion[2]={0};uint32_t slot,enabled,point_count;unsigned i,j,count=kind==0x21?2:1;
    if(method!=0x4e9360 && method!=0x408320 && method!=0x4091c0 && method!=0x408ea0 && method!=0x408b60)return 0;
    if(!game_level_player_catalog_value(a,catalog,kind,3,0,&slot) || slot>=family->slots)return 0;
    points=get(a,family->points+slot*4);point_count=get(a,family->point_counts+slot*4);
    if(!points || !point_count)return 0;
    if(kind==0x21){
        if(get(a,get(a,object)+0x60)!=0x53c550)return 0;
        enabled=(game_memory_read_u8(&a->memory,object+0xd8)&4)!=0;
        motion[0]=get(a,catalog+(enabled?0x1a8:0x1a4));motion[1]=get(a,catalog+(enabled?0x1b0:0x1ac));
    }else motion[0]=get(a,catalog+(kind==0x24?0x1d8:kind==0x28?0x1d4:kind==0x25?0x1d0:0x1cc));
    init.parent=object;
    /*408ea0 deliberately tests5adad8, not its own point count table. */
    init.orient_to_motion=get(a,(kind==0x28?0x5adad8:family->point_counts)+slot*4)==2;
    /*One marker is a valid single muzzle.43bc70/43bec0 only use the second
     *position for oriented pairs; never read past the owned one-marker array. */
    for(j=0;j<3;j++){init.position[j]=real(a,points+j*4);
        init.secondary_position[j]=real(a,points+(kind==0x28 || point_count<2?0:12)+j*4);}
    for(i=0;i<count;i++){
        init.motion=motion[i];init.heading=(uint16_t)(kind==0x24?17:kind==0x28?18:kind==0x25?27:12+i);init.mode=kind==0x21?init.orient_to_motion:0;
        init.resource=(kind==0x21 && !i) || kind==0x24?3:1;init.slot=kind==0x24?8:0;init.kind=(kind==0x21 && !i) || kind==0x24?1:0;
        created[i]=auxiliary_allocate(a,0xbc);
        if(!created[i] || a->memory.fault || !game_spatial_attachment_construct(&a->memory,created[i],&init))goto fail;
    }
    for(i=0;i<count;i++){
        GameAddress old=get(a,object+0x21c+i*4);
        if(old && !release_page(a,old))goto fail;
        put(a,object+0x21c+i*4,created[i]);created[i]=0;
    }
    return !a->memory.fault;
fail:for(i=0;i<2;i++)if(created[i])release_page(a,created[i]);return 0;
}
/*4e9170 uses the same live recipes. Class23 retains its constructed auxiliary. */
int game_level_group_vehicle_restore_components(GameApplication *a,GameAddress object){
    if(!a || !owns(a,object) || a->memory.fault)return 0;
    if(game_memory_read_u8(&a->memory,object+12)==0x23)return 1;
    return build_auxiliaries(a,object,get(a,0x589740),get(a,get(a,object)+0x1f8));
}
int game_level_vehicle_place(GameApplication *a,GameAddress object,uint32_t identity,const float position[3],const uint16_t angles[3],GameAddress commander,uint32_t method){
    VehicleActivationServices services={0};GameAddress point;unsigned i;int ok;
    if(!a || !owns(a,object) || a->memory.fault || !vehicle_presentation_family(game_memory_read_u8(&a->memory,object+12)))return 0;
    if(method!=0x406e50 && method!=0x4e63b0 && method!=0x5520b0)return 0;
    /* Native legacy algorithms borrow one scoped point. No emulated call frame
     * or hot-path object synchronization; activation happens on acquisition. */
    point=scene_projection_storage_allocate(12,"vehicle placement point");if(!point)return 0;
    for(i=0;i<3;i++){uint32_t bits;memcpy(&bits,position+i,4);put(a,point+i*4,bits);}
    services.context=a;services.place=place_entity;services.catalog_value=catalog_value;
    services.prepare=prepare_vehicle;services.component_enabled=component_enabled;services.auxiliary_foreign=build_auxiliaries;
    services.allocate=auxiliary_allocate;services.release=release_page;
    ok=!a->memory.fault && (method==0x4e63b0?vehicle_presentation_mode_place(&a->memory,object,identity,point,angles,&services):
        method==0x406e50?vehicle_presentation_gameplay_place(&a->memory,object,identity,point,angles,&services):
        vehicle_presentation_activate(&a->memory,object,identity,point,angles,commander,&services));
    if(!scene_projection_storage_free(point))ok=0;
    return ok;
}

extern int game_application_options_load(GameApplication *,uint32_t);
extern int game_level_display_mask_resize(GameApplication *,uint32_t);
static int selector_stop(void *context,int16_t sound){return game_menu_sound_stop(context,sound,0);}
static int selector_play(void *context,int16_t sound,float volume,uint32_t flags){
    SoundPlaybackRequest request={sound,0.0f,volume,0.0f,flags,0};return game_menu_sound_play(context,&request);
}
static int selector_close(void *context){GameApplication *a=context;return game_root_selector_close(&a->memory,NULL);}
static int selector_mode(void *context,uint32_t mode){return game_application_options_load(context,mode);}
static int resize_mask(void *context,uint32_t size){return game_level_display_mask_resize(context,size);}
static int selector_control(void *context,uint32_t mode){
    GameApplication *a=context;GameDisplayMaskServices s={a,resize_mask};return game_display_configure_control(&a->memory,mode,&s);
}
static int selector_message(void *context){GameApplication *a=context;return game_message_enqueue(a,get(a,0x628a88),0x589914,1);}
int game_level_catalog_refresh_selection(void *context,GameAddress catalog){
    GameApplication *a=context;GameplayControllerServices s={a,NULL,selector_stop,selector_play,selector_close,selector_mode,selector_control,selector_message};
    return gameplay_controller_refresh_selection(&a->memory,catalog,&s);
}
static int feedback_clear(void *context,GameAddress bank){return game_menu_sound_queue_clear(context,bank);}
static int feedback_submit(void *context,GameAddress bank,const int16_t sounds[8],int *accepted){return game_menu_sound_queue_submit(context,bank,sounds,accepted);}
static int notify_inactive(void *context,GameAddress object){
    GameApplication *a=context;GamePresentationFeedbackServices s={a,feedback_clear,feedback_submit};
    if(get(a,get(a,object)+0x58)!=0x553750)return 0;
    return game_presentation_deactivation_feedback(&a->memory,object,1,0x57,&s);
}
typedef struct ResetContext {GameApplication *app;GameAddress object;} ResetContext;
static int reset_peer(void *context,GameAddress peer,GameAddress object){
    ResetContext *c=context;GameApplication *a=c->app;GameplayPeerServices s={a,NULL};
    uint32_t method=get(a,get(a,peer)+0x10c);
    return !a->memory.fault && gameplay_state_peer_method(&a->memory,peer,object,method,&s);
}
static int reset_packets(void *context,GameAddress chain){ResetContext *c=context;return return_packet(c->app,chain);}
static int reset_queue(void *context,GameAddress chain){ResetContext *c=context;return return_contact(c->app,chain);}
static int reset_catalog(void *context,uint32_t identity,uint8_t kind){
    ResetContext *c=context;GameApplication *a=c->app;GameAddress catalog=get(a,0x5f0210+(identity&63u)*4);(void)kind;
    /*47f550 and the inherited4777c0 are verified empty notifications. Other
     *catalog implementations still need their actual retirement program. */
    {uint32_t method=get(a,get(a,catalog)+0x68);
        if((method!=0x47f550 && method!=0x4777c0) || a->memory.fault){
            fprintf(stderr,"native presentation catalog retirement unsupported: object=%08x catalog=%08x method=%08x\n",c->object,catalog,method);
            return 0;}}
    (void)level_player_slots_forget_scene_member(c->object);level_effects_retire(c->object);return 1;
}
int game_level_presentation_reset(GameApplication *a,GameAddress object,unsigned kind){
    ResetContext c={a,object};PresentationResetServices s={&c,NULL,reset_peer,reset_packets,reset_queue,reset_catalog};
    PresentationPose *pose=presentation_instance_pose(object);PresentationLifecycle *state=presentation_instance_lifecycle(object);
    if(!pose && kind==1 && get(a,object)==0x5737e0){
        GameAddress contacts=get(a,object+0xb0);
        if(contacts && !game_level_factory_release_pool_member(a,contacts))return 0;
        put(a,object+0xb0,0);kind=0;
    }
    if(!pose && kind==0 && (game_memory_read_u8(&a->memory,object+12)==0xb0 || game_memory_read_u8(&a->memory,object+12)==9 ||
            get(a,object)==0x5737e0 || get(a,object)==0x573918 || get(a,object)==0x5738a0 ||
            game_presentation_factory_family(game_memory_read_u8(&a->memory,object+12)))){
        GameAddress source=get(a,object+0x50);uint32_t identity=get(a,object+20);unsigned i;
        if(!source || a->memory.fault)return 0;
        game_memory_write_u32(&a->memory,object+16,get(a,object+16)&0xff11ffffu);
        game_memory_write_u32(&a->memory,object+0x58,0);
        game_list_detach_node(&a->memory,object,4,8);game_list_detach_node(&a->memory,object,0x48,0x4c);
        if(a->memory.fault || !reset_catalog(&c,identity,game_memory_read_u8(&a->memory,object+12)))return 0;
        for(i=0;i<3;i++){game_memory_write_u32(&a->memory,object+24+i*4,0);game_memory_write_u16(&a->memory,object+36+i*2,0);}
        game_memory_write_u32(&a->memory,object+20,0);
        game_list_insert_after(&a->memory,object,source,4,8);return !a->memory.fault;
    }
    return pose && state && presentation_lifecycle_reset(&a->memory,object,pose,state,kind,&s);
}
static int reset_base(void *context,GameAddress object){return game_level_presentation_reset(context,object,2);}
static int attachment_stop_voice(void *c,uint32_t voice){GameApplication *a=c;int playing;
    if(!get(a,get(a,0x628a90)+0x468))return !a->memory.fault;
    if(!game_menu_sound_playing(a,5,voice,&playing))return 0;
    return !playing || game_menu_sound_stop(a,5,voice);
}
static int attachment_remove_child(void *c,GameAddress child){GameApplication *a=c;ExpansionEffect *effect=presentation_instance_expansion(child);
    if(!effect){
        PresentationLifecycle *state=presentation_instance_lifecycle(child);
        fprintf(stderr,"native attachment child removal failed: child=%08x live=%d selector=%08x\n",child,
            presentation_instance_find(child)!=NULL,state?state->membership.selector:UINT32_MAX);
        return 0;
    }
    expansion_effect_remove(effect);return !a->memory.fault;
}
int game_level_attachment_reset(GameApplication *a,GameAddress object){
    ResetContext c={a,object};PresentationResetServices membership={&c,NULL,reset_peer,reset_packets,reset_queue,reset_catalog};
    AttachmentResetServices services={a,attachment_stop_voice,attachment_remove_child,&membership};
    return attachment_effect_reset(&a->memory,object,&services);
}
extern int game_level_group_member_detach(GameApplication *,GameAddress);
static int detach_controller(void *context,GameAddress controller,GameAddress object){
    GameApplication *a=context;
    uint32_t method=get(a,get(a,controller)+12);(void)object;
    if(a->memory.fault || (method!=0x53e3e0 && method!=0x53f120 && method!=0x53fbf0))return 0;
    return game_level_group_member_detach(a,controller);
}
/*4e7540 retires only the two group presentation auxiliaries, then539c10.
 *It is not551fb0's larger vehicle/controller retirement program. */
int game_level_group_vehicle_reset(GameApplication *a,GameAddress object){
    VehicleRetirementServices s={0};
    if(!a || !owns(a,object) || a->memory.fault)return 0;
    s.context=a;s.release_storage=release_page;s.base_reset=reset_base;
    return vehicle_presentation_auxiliary_reset(&a->memory,object,&s);
}
int game_level_vehicle_reset(GameApplication *a,GameAddress object){
    VehicleRetirementServices s={0};
    if(!a || !owns(a,object) || get(a,0x5adc68)==object || get(a,0x5adc6c)==object || a->memory.fault)return 0;
    s.context=a;s.return_event=return_event;s.release_storage=release_page;s.refresh_selection=game_level_catalog_refresh_selection;
    s.notify_inactive=notify_inactive;s.detach_controller=detach_controller;s.base_reset=reset_base;s.base_release=base_release;
    return vehicle_presentation_reset(&a->memory,object,&s);
}
/*552040 retires a descriptor's payload without recursively detaching the
 *descriptor itself.551fb0 has additional death/controller notifications. */
int game_level_vehicle_descriptor_retire(GameApplication *a,GameAddress object){
    VehicleRetirementServices s={0};unsigned i;
    if(!a || !owns(a,object) || a->memory.fault)return 0;
    s.context=a;s.return_event=return_event;s.release_storage=release_page;
    s.base_reset=reset_base;s.refresh_selection=game_level_catalog_refresh_selection;
    for(i=0;i<2;i++){
        GameAddress field=object+(i?0x2c8:0x2b8),effect=get(a,field);
        if(effect){put(a,effect+0xb4,3);put(a,field,0);}
    }
    if(!vehicle_presentation_clear_events(&a->memory,object+0x264,0,&s))return 0;
    if((game_memory_read_u8(&a->memory,object+0xd8)&1) &&
       !game_level_catalog_refresh_selection(a,get(a,0x5f0210+(get(a,object+0x14)&63u)*4)))return 0;
    if(!vehicle_presentation_base_reset(&a->memory,object,&s))return 0;
    put(a,object+0x294,0);return !a->memory.fault;
}
static int kill_credit(void *c,GameAddress catalog,uint32_t amount){GameApplication *a=c;uint32_t method=get(a,get(a,catalog)+0x34);
    if(method==0x47f250)return gameplay_catalog_credit(&a->memory,catalog,amount);
    if(method==0x47f290 || method==0x4794b0 || method==0x4794e0){
        put(a,catalog+0x22c,get(a,catalog+0x22c)+amount);
        put(a,catalog+0x150,get(a,catalog+0x150)+amount);return !a->memory.fault;
    }
    if(method==0x477750)return !a->memory.fault; /*Original empty base method. */
    return 0;
}
/*42da00 is the vehicle's actual slot14 kill-statistics program.
 *407e30 shares the verified relation scan and auxiliary retirement program.
 *Its base539c10 reset returns commands/contacts and pooled membership through
 *the same native lifecycle owners; the incoming killer is unused here. */
int game_level_vehicle_death_detach(GameApplication *a,GameAddress object,GameAddress other,GameAddress method){
    VehicleRetirementServices services={0};(void)other;
    if(!a || a->memory.fault)return 0;
    if(method==0x42da00)return gameplay_entity_note_kill(&a->memory,object,other,a,kill_credit);
    if(!owns(a,object))return 0;
    if(method!=0x407e30)return 0;
    services.context=a;services.release_storage=release_page;services.base_reset=reset_base;
    return vehicle_presentation_base_reset(&a->memory,object,&services);
}
