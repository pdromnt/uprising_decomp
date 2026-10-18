#include "level_factory.h"
#include "model_assets.h"
#include "level_routes.h"
#include "level_objects.h"
#include "effect_factory.h"
#include "effect_profile.h"
#include "effect_model.h"
#include "trail_model.h"
#include "mesh_buffer.h"
#include "scene_instance_owner.h"
#include "scene_projection_storage.h"
#include "effect_resource_pool.h"
#include "runtime_node_pool.h"
#include "level_player_factory.h"
#include "game_initializers.h"
#include "game_intrusive_list.h"
#include "level_player_slots.h"
#include "level_effect_runtime.h"
#include "descent_payload.h"
#include "scaled_effect_model.h"
#include "curve_effect_model.h"
#include "effect_initialization.h"
#include <stdio.h>
#include <string.h>
static GameAddress allocate(void *context,uint32_t size){(void)context;return scene_projection_storage_allocate(size,"factory instance record");}
static GameAddress storage_allocate(void *context,size_t size){(void)context;return scene_projection_storage_allocate(size,"factory owned inputs");}
static int storage_release(void *context,GameAddress p){(void)context;return scene_projection_storage_free(p);}
static int clone_model(void *context,GameAddress source,GameAddress *result) {
    GameApplication *a=context;
    if(game_memory_read_u32(&a->memory,source)==0x5752d0){
        GamePresentationStorageServices s={a,storage_allocate,storage_release,NULL,NULL};
        *result=curve_effect_model_clone(&a->memory,source,&s);return *result!=0;
    }
    if(game_memory_read_u32(&a->memory,source)==0x575128){
        GamePresentationStorageServices s={a,storage_allocate,storage_release,NULL,NULL};
        *result=scaled_effect_model_clone(&a->memory,source,&s);return *result!=0;
    }
    if(game_memory_read_u32(&a->memory,source)==0x573a10){
        GamePresentationStorageServices s={a,storage_allocate,storage_release,NULL,NULL};
        *result=descent_model_clone(&a->memory,source,&s);return *result!=0;
    }
    return scene_instance_owner_clone(&a->memory,source,result)==1;
}
static int release_model(void *context,GameAddress model) {
    GameApplication *a=context;
    if(game_memory_read_u32(&a->memory,model)==0x5752d0){
        GamePresentationStorageServices s={a,storage_allocate,storage_release,NULL,NULL};
        return curve_effect_model_release(&a->memory,model,1,&s);
    }
    if(game_memory_read_u32(&a->memory,model)==0x575128){
        GamePresentationStorageServices s={a,storage_allocate,storage_release,NULL,NULL};
        return scaled_effect_model_release(&a->memory,model,1,&s);
    }
    if(game_memory_read_u32(&a->memory,model)==0x573a10){
        GamePresentationStorageServices s={a,storage_allocate,storage_release,NULL,NULL};
        return descent_model_release(&a->memory,model,1,&s);
    }
    if(effect_model_find(model))return effect_model_release(model);
    if(trail_model_find(model))return trail_model_release(model);
    if(scene_instance_owner_is_holder(model))return scene_instance_owner_release_graph(model)==1;
    if(mesh_buffer_is_native(&a->memory,model))return mesh_buffer_release(&a->memory,model,mesh_buffer_owned_services()) && scene_projection_storage_free(model);
    return 0; /*Unknown model ownership cannot enter the CPU renderer/imports. */
}
static int pool_member(void *context,GameAddress p){return game_level_factory_release_pool_member(context,p);}
static int packet_chain(void *context,GameAddress p){return game_level_factory_detach_packet_chain(context,p);}
/*534c30/48f2d0 and539840/4ce6f0: return the actual native chains to
 *their existing owners. Foreign records remain unfinished, never CPU re-entry. */
int game_level_factory_release_pool_member(GameApplication *a,GameAddress chain){
    EffectResourcePool *pool=effect_resource_pool_find(0x631910);
    return !chain || (pool && effect_resource_pool_return_chain(&a->memory,pool,chain)==1);
}
int game_level_factory_detach_packet_chain(GameApplication *a,GameAddress chain){
    return !chain || runtime_node_pool_return_chain(&a->memory,chain)==1;
}
int game_level_gate_detach_event(GameApplication *a,GameAddress chain){
    return game_level_factory_release_pool_member(a,chain);
}
static int destroy(void *context,GameAddress object){return game_level_factory_destroy(context,object);}
extern int game_level_scaled_effect_reset(GameApplication *,GameAddress);
extern int game_level_ground_controller_release(GameApplication *,GameAddress,uint8_t,int);
static int release_record(GameAddress object){
    if(!scene_projection_storage_free(object))return 0;
    (void)level_player_slots_forget_scene_member(object);return 1;
}
static int load(void *context,const char *name,ResourceBlob *blob){return game_application_load_blob(context,name,blob);}
static uint32_t texture(void *context,const uint8_t *bytes,uint32_t width,uint32_t height,uint32_t flags){return game_level_texture_from_indexed(context,bytes,width,height,flags);}
/*The four shared sprite-profile loaders select real C family algorithms. */
static int effect_assets(void *context,GameAddress loader) {
    GameApplication *a=context;unsigned kind;EffectProfileServices services={a,load,texture};
    switch(loader) {
    case 0x43d890:kind=5;break;case 0x551530:kind=0x41;break;
    case 0x5379d0:kind=0x43;break;case 0x49aae0:kind=0x44;break;
    default:return 0;
    }
    return effect_profile_family_load(&a->memory,kind,&services);
}
static int projectile_describe(void *context,const ProjectileFamily *f,uint16_t slot,PresentationInstanceDefinition *d){return game_level_projectile_describe(context,f,slot,d);}
/*48ee40: one real decoded hump model supplies the mutable mine meshes.
 *The cache owns its holder; each clone owns only its output mesh buffers. */
static int mole_hump_load(GameApplication *a){
    GameModelDefinition d={0};GameAddress names,container,model;uint32_t vertices,triangles;
    if(game_memory_read_u32(&a->memory,0x5f06a8))return !a->memory.fault;
    names=scene_projection_storage_allocate(4,"mole hump asset name");if(!names)return 0;
    game_memory_write_u32(&a->memory,names,0x58aba8);
    if(!game_model_definition_read(a,ASSET_MODEL_SIMPLE,0,names,0,0,0,&d)){
        scene_projection_storage_free(names);return 0;}
    scene_projection_storage_free(names);model=d.model;container=game_memory_read_u32(&a->memory,model+0x50);
    vertices=game_memory_read_u32(&a->memory,container+0x14);triangles=game_memory_read_u32(&a->memory,container+0x1c);
    if(a->memory.fault || vertices>32767 || triangles>32767){game_model_definition_release(a,&d);return 0;}
    d.model=0;if(!game_model_definition_release(a,&d)){game_application_scene_model_release(a,model);return 0;}
    game_memory_write_u32(&a->memory,0x5f06a8,model);game_memory_write_u32(&a->memory,0x5f06a4,vertices);
    game_memory_write_u32(&a->memory,0x5f06a0,triangles);return !a->memory.fault;
}
void game_level_projectile_factory_services(GameApplication *a,ProjectileFactoryServices *out){
    ProjectileFactoryServices services={a,projectile_describe,allocate,destroy,clone_model};*out=services;
}
static int presentation_describe(void *context,const GamePresentationFactoryFamily *f,uint16_t slot,float dimensions[3],GamePresentationDefinition *d){return game_level_presentation_describe(context,f,slot,dimensions,d);}
void game_level_effect_factory_services(GameApplication *a,EffectFactoryServices *out){
    EffectFactoryServices services={a,effect_assets,allocate,destroy};*out=services;
}
int game_level_projectile_describe(GameApplication *a,const ProjectileFamily *family,uint16_t slot,PresentationInstanceDefinition *out){
    GameModelDefinition d={0};PresentationInstanceDefinition result={0};
    if(!family || slot>=family->slots || !out ||
       !game_model_definition_read(a,ASSET_MODEL_SIMPLE,slot,family->names,
           (family->kind==0x0du || family->kind==0x12u)?0:1,0,0,&d))return 0;
    if(d.marker_count>32767u){game_model_definition_release(a,&d);return 0;}
    /*442ad8..442b02: the four laser variants add their verified material
     *bits. The retirement loop447f32..447f6e bounds this family to four. */
    if(family->kind==2u && slot<4u){
        uint8_t flags=game_memory_read_u8(&a->memory,d.model+0x3cu);
        game_memory_write_u8(&a->memory,d.model+0x3cu,(uint8_t)(flags|0x10u|(slot*4u)));
        if(a->memory.fault){game_model_definition_release(a,&d);return 0;}
    }
    memcpy(result.dimensions,d.dimensions,sizeof(result.dimensions));result.radius=d.radius;
    result.resource=d.markers;result.count=(int32_t)d.marker_count;result.model=d.model;
    /*536520 fresh construction keeps the decoded hull input and allocates its
     *controller. Vertex-copy storage belongs to subsequent copied instances. */
    result.copy_vertices=0;*out=result;return 1;
}
/*443321/445e5e/447d60: the singleton class0x10 template owns its mesh;
 *clones borrow the template and own a mesh clone plus two240-byte buffers. */
/*41a300/41a360: variant model and hull pages remain owned by the existing
 *asset store. These fixed slots are the sole publication, not a second cache. */
int game_level_spawn_variant_load(GameApplication *a,uint32_t variant){
    GameModelDefinition d={0};GameAddress slot;
    if(variant>8 || a->memory.fault)return 0;
    slot=0x5acd38+variant*4;
    if(game_memory_read_u32(&a->memory,slot))return !a->memory.fault;
    if(!game_model_definition_read(a,ASSET_MODEL_SIMPLE,(uint16_t)variant,0x583db0,0,0,0,&d))return 0;
    if(d.marker_count>20){game_model_definition_release(a,&d);return 0;}
    game_memory_write_u32(&a->memory,0x5acce8+variant*4,d.marker_count);
    game_memory_write_u32(&a->memory,0x5acd10+variant*4,d.markers);
    game_memory_write_u32(&a->memory,slot,d.model);
    return !a->memory.fault;
}
int game_level_spawn_variants_shutdown(GameApplication *a){
    unsigned i;GameAddress model,hull;
    /*The pool/template must retire before any borrowed variant inputs. */
    if(a->memory.fault || game_memory_read_u32(&a->memory,0x5adb78))return 0;
    for(i=0;i<9;i++){
        model=game_memory_read_u32(&a->memory,0x5acd38+i*4);
        hull=game_memory_read_u32(&a->memory,0x5acd10+i*4);
        if(a->memory.fault)return 0;
        if(model){if(!game_application_scene_model_release(a,model))return 0;
            game_memory_write_u32(&a->memory,0x5acd38+i*4,0);}
        if(hull){if(!scene_projection_storage_free(hull))return 0;
            game_memory_write_u32(&a->memory,0x5acd10+i*4,0);}
        game_memory_write_u32(&a->memory,0x5acce8+i*4,0);
    }
    return !a->memory.fault;
}
static int spawn_destroy(GameApplication *a,GameAddress object){
    PresentationReleaseServices s={a,release_model,storage_release,pool_member,packet_chain};
    if(object==game_memory_read_u32(&a->memory,0x5adb78) && !presentation_instance_source_is_empty(object))return 0;
    if(presentation_instance_release(&a->memory,object,&s,1,0)!=1 || !release_record(object))return 0;
    if(object==game_memory_read_u32(&a->memory,0x5adb78))game_memory_write_u32(&a->memory,0x5adb78,0);
    return !a->memory.fault;
}
static GameAddress spawn_create(GameApplication *a,GameAddress source){
    GameAddress object=allocate(a,0xe8),model=0,vertices=0,controller=0;int constructed=0;unsigned i;
    if(!object)return 0;
    if(source){PresentationConstructionServices s={a,clone_model};
        if(!presentation_instance_create_copy(&a->memory,object,source,0,&s))goto failed;
        constructed=1;
        vertices=storage_allocate(a,240);controller=storage_allocate(a,240);
        if(!vertices || !controller || !presentation_instance_take_render_buffers(&a->memory,object,vertices,controller))goto failed;
        vertices=controller=0;if(a->memory.fault)goto failed;
    }else{PresentationInstanceDefinition d={0};uint32_t word;
        model=allocate(a,0x64);if(!model || !mesh_buffer_construct(&a->memory,model,10,12,0x59d81c,0x59d824,mesh_buffer_owned_services()))goto failed;
        game_memory_write_u8(&a->memory,model+0x3c,(uint8_t)(game_memory_read_u8(&a->memory,model+0x3c)|2));
        d.type=0x10;d.radius=0x3f800000;d.model=model;
        for(i=0;i<3;i++){word=game_memory_read_u32(&a->memory,0x624f48+i*4);memcpy(d.dimensions+i,&word,4);}
        if(a->memory.fault || !presentation_instance_create(&a->memory,object,&d,0))goto failed;
        constructed=1;model=0;
    }
    game_memory_write_u32(&a->memory,object,0x572038);game_memory_write_u8(&a->memory,object+12,0x10);
    if(a->memory.fault)goto failed;
    return object;
failed:{GameMemory original_fault=a->memory;a->memory.fault=0;
    if(vertices)(void)storage_release(a,vertices);if(controller)(void)storage_release(a,controller);
    if(constructed)(void)spawn_destroy(a,object);else (void)storage_release(a,object);
    if(model && !release_model(a,model) && !mesh_buffer_is_native(&a->memory,model))
        (void)storage_release(a,model); /*Construction already retired partial mesh inputs. */
    if(original_fault.fault){a->memory.fault=original_fault.fault;a->memory.fault_address=original_fault.fault_address;
        a->memory.fault_size=original_fault.fault_size;a->memory.fault_write=original_fault.fault_write;}
    return 0;}
}
static GameAddress spawn_operation(GameApplication *a,unsigned op,int32_t count){
    GameAddress source=game_memory_read_u32(&a->memory,0x5adb78),object;int32_t i;
    if(!source){source=spawn_create(a,0);if(!source)return 0;game_memory_write_u32(&a->memory,0x5adb78,source);}
    if(a->memory.fault)return 0;if(!op)return source;
    if(op==1){for(i=0;i<count;i++){object=spawn_create(a,source);if(!object)return 0;
        game_list_insert_after(&a->memory,object,source,4,8);if(a->memory.fault)return 0;}
        return 1;}
    if(!game_memory_read_u32(&a->memory,source+4) && !spawn_operation(a,1,1))return 0;
    object=game_memory_read_u32(&a->memory,source+4);if(!object || a->memory.fault)return 0;
    game_list_detach_node(&a->memory,object,8,4);return a->memory.fault?0:object;
}
int game_level_spawn_factory_shutdown(GameApplication *a){
    GameAddress source=game_memory_read_u32(&a->memory,0x5adb78),object;unsigned visits=0;
    if(!source)return !a->memory.fault;
    while((object=game_memory_read_u32(&a->memory,source+4))){
        if(++visits>65536 || !spawn_destroy(a,object))return 0;}
    return !a->memory.fault && spawn_destroy(a,source);
}
/*419c20: discard the selected mesh/hull binding before shared spatial and
 *catalog retirement. Fixed copy buffers and mesh capacity remain owned. */
int game_level_spawn_reset(GameApplication *a,GameAddress object){
    PresentationPose *pose=presentation_instance_pose(object);
    if(!pose || !mesh_buffer_clear(&a->memory,pose->model))return 0;
    pose->radius=1.0f;pose->vertex_count=0;pose->hull=0;
    memset(pose->dimensions,0,sizeof(pose->dimensions));
    return presentation_pose_export(&a->memory,object,pose) && game_level_presentation_reset(a,object,1);
}
int game_level_factory_destroy(GameApplication *a,GameAddress object) {
    if(presentation_instance_player_rig(object))return game_level_player_factory_destroy(a,object);
    uint8_t kind=game_memory_read_u8(&a->memory,object+0xc);
    const EffectFamily *effect=effect_factory_family(kind);const ProjectileFamily *projectile=projectile_factory_family(kind);
    if(a->memory.fault)return 0;
    if(kind==0x10)return spawn_destroy(a,object);
    if((kind>=0x21 && kind<=0x25) || kind==0x28)return game_level_vehicle_factory_destroy(a,object);
    if(kind==0x27)return game_level_infantry_destroy(a,object);
    if(kind==0x26)return game_level_aircraft_destroy(a,object);
    if(kind==0x29)return game_level_class29_destroy(a,object);
    if(kind==0x40)return game_level_class40_destroy(a,object);
    if(kind==0x2a)return game_level_carrier_destroy(a,object);
    if(kind==4u || kind==0x0eu){
        PresentationReleaseServices s={a,release_model,storage_release,pool_member,packet_chain};
        if(presentation_instance_release(&a->memory,object,&s,1,0)!=1)return 0;
        return release_record(object);
    }
    if(kind==0x88 || kind==0x89)return game_level_station_payload_destroy(a,object);
    if(kind==9u || kind==0xb0u){
        GamePresentationStorageServices s={a,storage_allocate,storage_release,clone_model,release_model};
        return game_presentation_base_destroy(&a->memory,object,&s) && release_record(object);
    }
    if(kind==0xa0 || kind==0xa1)return game_level_teleporter_destroy(a,object);
    if(kind==0xa2)return game_level_gate_destroy(a,object);
    if(kind==10){PresentationPose *pose=presentation_instance_pose(object);
        if(pose && pose->properties[0] && !game_level_attachment_reset(a,object))return 0;}
    if(kind==0x45){PresentationPose *pose=presentation_instance_pose(object);
        if(pose && pose->properties[0] && !game_level_arrival_reset(a,object))return 0;}
    if(kind==0x0f || kind==3){PresentationPose *pose=presentation_instance_pose(object);
        if(pose && pose->properties[0] && !game_level_scaled_effect_reset(a,object))return 0;
    }
    if(kind==0x0c){GameAddress mesh=game_memory_read_u32(&a->memory,object+0xfc);
        PresentationPose *pose=presentation_instance_pose(object);
        if(pose && pose->properties[0] && !game_level_mole_reset(a,object))return 0;
        if(mesh && (!mesh_buffer_release(&a->memory,mesh,mesh_buffer_owned_services()) || !scene_projection_storage_free(mesh)))return 0;
        game_memory_write_u32(&a->memory,object+0xfc,0);
    }
    if((projectile && kind!=0x0f && kind!=3 && kind!=0x0c) || kind==5 || (kind>=0x41 && kind<=0x44)){PresentationPose *pose=presentation_instance_pose(object);
        if(pose && pose->properties[0]){
            if(kind==5){game_memory_write_u32(&a->memory,object+0xdc,0);game_memory_write_u32(&a->memory,object+0xd4,0);}
            if(!game_level_presentation_reset(a,object,1))return 0;
        }}
    if(kind==0x77 || (kind>=0x80 && kind<=0x87)) {
        GamePresentationStorageServices s={a,storage_allocate,storage_release,clone_model,release_model};
        return game_presentation_destroy(&a->memory,object,&s) && release_record(object);
    }
    if(kind==0x60 || kind==0x62 || kind==0x63 || kind==0x64){
        GamePresentationStorageServices s={a,storage_allocate,storage_release,clone_model,release_model};
        GameAddress controller=game_memory_read_u32(&a->memory,object+0xa8);
        if(controller && !game_level_ground_controller_release(a,controller,kind,0))return 0;
        game_memory_write_u32(&a->memory,object+0xa8,0);
        return game_presentation_destroy(&a->memory,object,&s) && release_record(object);
    }
    if(effect || projectile || kind==0x0b) {
        PresentationReleaseServices s={a,release_model,storage_release,pool_member,packet_chain};
        if(presentation_instance_find(object) && presentation_instance_release(&a->memory,object,&s,1,0)!=1)return 0;
        return release_record(object);
    }
    fprintf(stderr,"native factory destruction unsupported: object=%08x kind=%02x\n",object,(unsigned)kind);
    return 0;
}
/*442a10/445610/447d60: recover ordinary kind/variant/count arguments and
 * use existing native template/clone/pool owners. The original mode10 guard
 * rejects a missing template, while taking/refilling an existing one is legal.
 * Other live families remain explicit native links, never hidden CPU fallback. */
/*442eda/445a68/447f-family: the position child emits this small ground record,
 *not the larger shared projectile layout. Its verified legacy fields remain
 *the sole owned record; models/vertices use the existing native graph owners. */
static GameAddress position_operation(GameApplication *a,uint8_t kind,uint16_t slot,unsigned op,int32_t count){
    GameMemory *m=&a->memory;GameAddress source,object;unsigned i;
    int scenery=kind==0xb0u;
    GameAddress publication=scenery?0x5add18u+slot*4u:0x5adad4u;
    GameAddress methods=scenery?0x572b48u:0x572be8u;
    uint32_t size=scenery?0x68u:0x70u,flags=scenery?0x800u:0xc00u;
    GamePresentationStorageServices s={a,storage_allocate,storage_release,clone_model,release_model};
    /*5847a0 contains fifteen scenery names, followed by a null terminator.
     *Each template borrows that descriptor index and owns its model/vertices. */
    if(slot>=(scenery?15u:1u))return 0;
    source=game_memory_read_u32(m,publication);
    if(m->fault)return 0;
    if(!source){
        GameModelDefinition d={0};
        if(game_memory_read_u32(m,0x624f8cu)==10u || m->fault ||
           !game_model_definition_read(a,ASSET_MODEL_SIMPLE,slot,scenery?0x5847a0u:0x5846c0u,scenery,0,0,&d))return 0;
        if(d.marker_count>32767u){game_model_definition_release(a,&d);return 0;}
        source=allocate(a,size);
        if(!source){game_model_definition_release(a,&d);return 0;}
        game_initializer_0042cfb0(m,source);
        for(i=0;i<3u;i++){uint32_t bits;memcpy(&bits,d.dimensions+i,4u);game_memory_write_u32(m,source+0x2cu+i*4u,bits);}
        game_memory_write_u32(m,source+0x38u,d.radius);
        game_memory_write_u32(m,source+0x3cu,d.markers);
        game_memory_write_u16(m,source+0x44u,(uint16_t)d.marker_count);
        game_memory_write_u32(m,source+0x54u,d.model);
        game_memory_write_u32(m,source,methods);
        game_memory_write_u8(m,source+12u,kind);game_memory_write_u32(m,source+16u,flags);
        if(scenery)game_memory_write_u32(m,source+0x64u,slot);
        if(d.marker_count){GameAddress vertices=storage_allocate(a,(size_t)d.marker_count*12u);
            if(!vertices){(void)game_level_factory_destroy(a,source);return 0;}
            game_memory_write_u32(m,source+0x40u,vertices);
        }
        game_memory_write_u32(m,publication,source);
        if(m->fault)return 0;
    }
    if(op==0u)return source;
    if(op==2u){
        object=game_memory_read_u32(m,source+4u);
        if(object){
            game_list_detach_node(m,object,4u,8u);
            game_memory_write_u32(m,object+4u,0u);game_memory_write_u32(m,object+8u,0u);
            return m->fault?0u:object;
        }
        count=1;
    }
    while(count>0){
        object=allocate(a,size);if(!object)return 0;
        if(!game_presentation_base_copy(m,object,source,&s)){
            (void)game_level_factory_destroy(a,object);return 0;
        }
        game_memory_write_u32(m,object,methods);game_memory_write_u8(m,object+12u,kind);
        game_memory_write_u32(m,object+16u,(game_memory_read_u32(m,object+16u)&~1u)|flags);
        if(scenery)game_memory_write_u32(m,object+0x64u,game_memory_read_u32(m,source+0x64u));
        if(op==2u)return m->fault?0u:object;
        game_list_insert_after(m,object,source,4u,8u);if(m->fault)return 0;
        --count;
    }
    return 1;
}
/*442bcb/409f70 and40a000: ordinary physical presentation templates/copies.
 *536520's controller/hull/model use the existing presentation owner. The
 *derived class adds only identity and0x800 to the shared construction flags. */
/*445610's gate branch copies45e2c0 into the same ready list used by447d60.
 *Template input/model and copy output/controller buffers retain their owners. */
static GameAddress gate_factory(GameApplication *a,uint16_t slot,unsigned op,int32_t count){
    GameAddress source,object;GameMemory *m=&a->memory;
    if(slot || op>2 || count>65536 || m->fault)return 0;
    source=game_memory_read_u32(m,0x5adb64);
    if(!source){if(game_memory_read_u32(m,0x624f8c)==10)return 0;source=game_level_gate_template_load(a);}
    if(!source || m->fault)return 0;if(!op)return source;
    if(op==2){object=game_memory_read_u32(m,source+4);
        if(object){game_list_detach_node(m,object,4,8);return m->fault?0:object;}count=1;}
    while(count>0){
        object=game_level_gate_create(a);if(!object || m->fault)return 0;
        if(op==2)return object;
        game_list_insert_after(m,object,source,4,8);if(m->fault)return 0;count--;
    }
    return !m->fault;
}
static GameAddress physical_factory(GameApplication *a,uint8_t kind,uint16_t slot,unsigned op,int32_t count){
    GameAddress source,object;GameMemory *m=&a->memory;
    GameAddress publication=kind==4?0x5adb10:0x5adbfc,methods=kind==4?0x571ad0:0x571b90;
    uint32_t size=kind==4?0xdc:0xe0;
    PresentationConstructionServices copying={a,clone_model};
    if(slot>=(kind==4?2:1) || op>2 || m->fault)return 0;
    source=game_memory_read_u32(m,publication+slot*4);
    if(!source){GameModelDefinition asset={0};PresentationInstanceDefinition d={0};
        if(game_memory_read_u32(m,0x624f8c)==10 || m->fault ||
           !game_model_definition_read(a,ASSET_MODEL_SIMPLE,slot,kind==4?0x5846a8:0x5846ac,0,0,0,&asset))return 0;
        if(asset.marker_count>32767){game_model_definition_release(a,&asset);return 0;}
        d.type=kind;d.flags=kind==4?0x803:0x14803;memcpy(d.dimensions,asset.dimensions,sizeof(d.dimensions));
        d.radius=asset.radius;d.resource=asset.markers;d.count=(int32_t)asset.marker_count;d.model=asset.model;
        source=allocate(a,size);
        if(!source || !presentation_instance_create(m,source,&d,0)){
            if(source)storage_release(a,source);
            game_model_definition_release(a,&asset);return 0;
        }
        game_memory_write_u32(m,source,methods);
        game_memory_write_u32(m,publication+slot*4,source);
    }
    if(m->fault || !presentation_instance_find(source) || game_memory_read_u8(m,source+12)!=kind)return 0;
    if(!op)return source;
    if(op==2){
        object=game_memory_read_u32(m,source+4);
        if(object){
            game_list_detach_node(m,object,4,8);
            game_memory_write_u32(m,object+4,0);game_memory_write_u32(m,object+8,0);
            return m->fault?0:object;
        }
        count=1;
    }
    while(count>0){
        object=allocate(a,size);if(!object)return 0;
        if(!presentation_instance_create_copy(m,object,source,0,&copying)){
            if(presentation_instance_find(object))(void)game_level_factory_destroy(a,object);
            else storage_release(a,object);
            return 0;
        }
        game_memory_write_u32(m,object,methods);
        game_memory_write_u8(m,object+12,kind);
        if(op==2)return m->fault?0:object;
        game_list_insert_after(m,object,source,4,8);if(m->fault)return 0;
        --count;
    }
    return 1;
}
static float scaled_real(GameMemory *m,GameAddress p){uint32_t bits=game_memory_read_u32(m,p);float value;memcpy(&value,&bits,4);return value;}
static void scaled_store(GameMemory *m,GameAddress p,float value){uint32_t bits;memcpy(&bits,&value,4);game_memory_write_u32(m,p,bits);}
/*4fbea0/4fc050: same presentation/projectile owner, two shared hull records,
 * and a private procedural model per instance. Clones borrow the template hull
 * and rewrite it just as retail does; there is one writable allocation. */
static GameAddress scaled_create(GameApplication *a,GameAddress source,uint8_t kind,uint16_t slot){
    GameMemory *m=&a->memory;GameAddress object=allocate(a,0x114),model=0,hull=0;
    PresentationInstanceDefinition d={0};GamePresentationStorageServices s={a,storage_allocate,storage_release,clone_model,release_model};
    unsigned i;int constructed=0;uint32_t radius_word;int32_t radius_integer;float radius;
    if(!object)return 0;
    if(source){
        if(!clone_model(a,game_memory_read_u32(m,source+0x54),&model))goto failed;
        d.source=source;d.flags=game_memory_read_u32(m,source+16)|0xc04;d.copy_vertices=1;
        d.resource=game_memory_read_u32(m,source+0x3c);
        for(i=0;i<3;i++)d.dimensions[i]=scaled_real(m,source+0x2c+i*4);
        d.radius=game_memory_read_u32(m,source+0x38);
    }else{
        char name[256],path[272];MaterialValue value={0};GameAddress material;
        GameAddress label=game_memory_read_u32(m,(kind==3?0x5846c4:0x5846c8)+slot*4);
        for(i=0;i<sizeof(name);i++){name[i]=(char)game_memory_read_u8(m,label+i);if(!name[i])break;}
        if(m->fault || i==sizeof(name) || snprintf(path,sizeof(path),"bitmaps\\%s",name)<0 ||
           !(material=game_model_material_bind(a,path,-1,&value)))goto failed;
        /*4fafd4/50be64 pass texture flags2/0 to439a20, not alpha mode2.
         *Keep the original indexed colors; flame additionally disables filtering. */
        if(kind==0x0f){TextureRecord *texture=game_model_texture_record(a,value.backend_handle);
            if(!texture)goto failed;texture->flags|=2u;}
        model=allocate(a,kind==3?0x7d8:0x5f8);if(!model)goto failed;
        if(!(kind==3?curve_effect_model_construct(m,model,material,&s):scaled_effect_model_construct(m,model,material,0,&s))){storage_release(a,model);model=0;goto failed;}
        hull=allocate(a,56);if(!hull)goto failed;
        d.resource=hull;d.flags=0xc07;d.radius=0x3f800000;
        for(i=0;i<3;i++)d.dimensions[i]=scaled_real(m,0x624f48+i*4);
    }
    d.type=kind;d.count=2;d.model=model;
    if(m->fault || !presentation_instance_create(m,object,&d,0))goto failed;
    constructed=1;model=hull=0;
    game_memory_write_u32(m,object,kind==3?0x5752e0:0x575138);
    if(!presentation_instance_enable_projectile(m,object))goto failed;
    radius_word=effect_initialization_random(m,scaled_real(m,kind==3?0x57529c:0x57510c))+16u;
    memcpy(&radius_integer,&radius_word,4);radius=(float)radius_integer;
    for(i=0;i<4;i++)scaled_store(m,object+0x2c+i*4,radius);
    for(i=0;i<2;i++){
        GameAddress sphere=d.resource+i*28;
        float extent=(float)((long double)radius*scaled_real(m,kind==3?0x5752a8:0x5750e8));
        for(unsigned axis=0;axis<3;axis++)scaled_store(m,sphere+axis*4,extent);
        scaled_store(m,sphere+12,0);scaled_store(m,sphere+16,0);
        scaled_store(m,sphere+20,(float)((long double)radius*scaled_real(m,kind==3?(i?0x5752a0:0x5752a4):(i?0x5750d8:0x575110))));
        scaled_store(m,sphere+24,(float)((long double)radius*scaled_real(m,kind==3?0x5752a0:0x5750d8)));
    }
    for(i=0;i<4;i++)game_memory_write_u32(m,object+0xe4+i*4,0);
    if(!m->fault)return object;
failed:
    if(constructed){PresentationReleaseServices r={a,release_model,storage_release,pool_member,packet_chain};
        /*Keep the record with its registered owner if retirement fails.
         *Freeing it here would leave that owner pointing at released storage. */
        if(presentation_instance_release(m,object,&r,1,0)!=1)return 0;}
    if(model)(void)release_model(a,model);
    if(hull)(void)storage_release(a,hull);
    (void)storage_release(a,object);return 0;
}
static GameAddress scaled_operation(GameApplication *a,uint8_t kind,uint16_t slot,unsigned op,int32_t count){
    GameMemory *m=&a->memory;GameAddress source,object;
    GameAddress publication=kind==3?0x5ada20:0x5adafc;
    if(slot)return 0;
    source=game_memory_read_u32(m,publication);if(m->fault)return 0;
    if(!source){
        if(game_memory_read_u32(m,0x624f8c)==10 || !(source=scaled_create(a,0,kind,slot)))return 0;
        game_memory_write_u32(m,publication,source);
    }
    if(op==0)return source;
    if(op==2){object=game_memory_read_u32(m,source+4);
        if(object){game_list_detach_node(m,object,4,8);return m->fault?0:object;}
        count=1;
    }
    while(count-->0){
        object=scaled_create(a,source,kind,slot);if(!object)return 0;
        if(op==2)return object;
        game_list_insert_after(m,object,source,4,8);if(m->fault)return 0;
    }
    return !m->fault;
}
static GameAddress operation(GameApplication *a,uint8_t kind,uint16_t slot,unsigned op,int32_t count) {
    if(kind==3 || kind==0x0f)return scaled_operation(a,kind,slot,op,count);
    if(kind==0x0b && slot==0){
        GamePresentationStorageServices s={a,storage_allocate,storage_release,NULL,NULL};
        return op==0?descent_payload_template(&a->memory,slot,&s):op==1?
            (GameAddress)descent_payload_fill(&a->memory,slot,count,&s):descent_payload_take(&a->memory,slot,&s);
    }
    if(kind==4 || kind==0x0e)return physical_factory(a,kind,slot,op,count);
    if(kind==0xa2)return gate_factory(a,slot,op,count);
    if(kind==0xa0 || kind==0xa1)return game_level_teleporter_factory(a,kind,slot,op,count);
    if(kind==0x10)return spawn_operation(a,op,count);
    if((kind>=0x21 && kind<=0x25) || kind==0x28)return game_level_vehicle_factory(a,kind,slot,op,count);
    if(kind==0x27)return game_level_infantry_factory(a,slot,op,count);
    if(kind==0x26)return game_level_aircraft_factory(a,slot,op,count);
    if(kind==0x29)return game_level_class29_factory(a,slot,op,count);
    if(kind==0x40)return game_level_class40_factory(a,slot,op,count);
    if(kind==0x2a)return game_level_carrier_factory(a,slot,op,count);
    if(kind==0x88 || kind==0x89)return game_level_station_payload_factory(a,kind,slot,op,count);
    if(kind==9u || kind==0xb0u)return position_operation(a,kind,slot,op,count);
    if(kind==0x20)return op==0?game_level_player_factory_template(a,slot):op==1?
        (GameAddress)game_level_player_factory_fill(a,slot,count):game_level_player_factory_take(a,slot);
    const EffectFamily *effect=effect_factory_family_slot(kind,slot);
    const ProjectileFamily *projectile=projectile_factory_family(kind);
    const GamePresentationFactoryFamily *presentation=game_presentation_factory_family(kind);
    GameAddress published=0;GameMemory *m=&a->memory;
    if(slot==UINT16_MAX)return 0;
    if(effect) {
        EffectFactoryServices s={a,effect_assets,allocate,destroy};
        published=game_memory_read_u32(m,effect->publication);
        if(m->fault || (!published && game_memory_read_u32(m,0x624f8c)==10))return 0;
        return op==0?effect_factory_template(m,effect,&s):op==1?(GameAddress)effect_factory_fill(m,effect,count,&s):effect_factory_take(m,effect,&s);
    }
    if(projectile && slot<projectile->slots) {
        ProjectileFactoryServices s={a,projectile_describe,allocate,destroy,clone_model};
        published=game_memory_read_u32(m,projectile->publication+slot*4u);
        if(m->fault || (!published && game_memory_read_u32(m,0x624f8c)==10))return 0;
        if(kind==0x0c && !mole_hump_load(a))return 0;
        return op==0?projectile_factory_template(m,projectile,slot,&s):op==1?(GameAddress)projectile_factory_fill(m,projectile,slot,count,&s):projectile_factory_take(m,projectile,slot,&s);
    }
    if(presentation && slot<presentation->slots) {
        GamePresentationStorageServices storage={a,storage_allocate,storage_release,clone_model,release_model};
        GamePresentationFactoryServices s={a,presentation_describe,&storage};
        return op==0?game_presentation_factory_template(m,presentation,slot,&s):op==1?(GameAddress)game_presentation_factory_fill(m,presentation,slot,count,&s):game_presentation_factory_take(m,presentation,slot,&s);
    }
    return 0; /*442a10's default kind/variant is invalid, not a CPU route. */
}
GameAddress game_level_factory_template(GameApplication *a,uint8_t kind,uint16_t slot){return operation(a,kind,slot,0,0);}
int game_level_factory_fill(GameApplication *a,uint8_t kind,uint16_t slot,int32_t count){return operation(a,kind,slot,1,count)!=0;}
GameAddress game_level_factory_take(GameApplication *a,uint8_t kind,uint16_t slot){return operation(a,kind,slot,2,0);}

/*447dd0 native factory retirement. Each canonical template owns its ready
 *intrusive clones. Retire clones before their template/model; storage and model
 *release dispatch is ordinary behavior in game_level_factory_destroy. */
static int retire_publications(GameApplication *a,GameAddress table,unsigned count){
    unsigned i;
    for(i=0;i<count;++i){GameAddress field=table+i*4,object=game_memory_read_u32(&a->memory,field);
        unsigned visits=0;
        if(a->memory.fault)return 0;
        if(!object)continue;
        for(;;){GameAddress child=game_memory_read_u32(&a->memory,object+4);
            if(a->memory.fault)return 0;
            if(!child)break;
            if(child==object || ++visits>65536 || !game_level_factory_destroy(a,child))return 0;
            if(game_memory_read_u32(&a->memory,object+4)==child)return 0;
        }
        if(!game_level_factory_destroy(a,object))return 0;
        game_memory_write_u32(&a->memory,field,0);
    }
    return !a->memory.fault;
}
/*447dd0: retire actual template/ready-list owners before source model recipes
 *and sprite profiles. Native caches own their pages; no address callback. */
int game_level_factories_release(GameApplication *a){
    static const uint8_t ground_kinds[]={0x60,0x62,0x63,0x64};
    static const uint8_t projectile_kinds[]={6,2,3,7,8,0x0c,0x0d,0x0f,0x11,0x12,0x13,0x14,0x15,0x16};
    static const uint8_t effect_kinds[]={5,0x41,0x45,0x43,0x42,0x44};unsigned i;
    EffectFactoryServices effects={a,effect_assets,allocate,destroy};
    for(i=0;i<sizeof(ground_kinds);i++){const GamePresentationFactoryFamily *f=game_presentation_factory_family(ground_kinds[i]);
        if(!f || !retire_publications(a,f->publication,f->slots))return 0;}
    for(i=0;i<sizeof(projectile_kinds);++i){const ProjectileFamily *f=projectile_factory_family(projectile_kinds[i]);
        if(!f || !retire_publications(a,f->publication,f->slots))return 0;}
    for(i=0;i<sizeof(effect_kinds);++i){const EffectFamily *f=effect_factory_family(effect_kinds[i]);
        if(!f || !effect_factory_shutdown(&a->memory,f,&effects))return 0;}
    for(i=0;i<3;++i){const EffectFamily *f=effect_factory_family_slot(10,(uint16_t)i);
        if(!f || !effect_factory_shutdown(&a->memory,f,&effects))return 0;}
    for(i=0x80;i<=0x87;++i){const GamePresentationFactoryFamily *f=game_presentation_factory_family(i);
        if(f && !retire_publications(a,f->publication,f->slots))return 0;}
    /*9 position,2a carriers and88/89 station payloads use the same ready-list
     *contract. Some publications overlap; canonical kind/owner destruction
     *clears each field, so a later range cannot retire an object twice. */
    if(!retire_publications(a,0x5add18,15) || !retire_publications(a,0x5adc64,1) || !retire_publications(a,0x5adb10,2) || !retire_publications(a,0x5adbfc,1) || !retire_publications(a,0x5adad4,1) || !retire_publications(a,0x5add88,4) ||
       !retire_publications(a,0x5add90,2) || !retire_publications(a,0x5adb28,6) ||
       !retire_publications(a,0x5adb64,1) || !retire_publications(a,0x5add9c,1) ||
       !retire_publications(a,0x5adb24,1))return 0;
    if(!retire_publications(a,0x5adb00,4))return 0;
    {GameAddress hump=game_memory_read_u32(&a->memory,0x5f06a8);
        if(hump && !game_application_scene_model_release(a,hump))return 0;
        game_memory_write_u32(&a->memory,0x5f06a8,0);game_memory_write_u32(&a->memory,0x5f06a4,0);game_memory_write_u32(&a->memory,0x5f06a0,0);}
    if(!game_level_player_factory_shutdown(a) || !game_level_vehicle_factory_shutdown(a) ||
       !game_level_spawn_factory_shutdown(a) || !game_level_spawn_variants_shutdown(a))return 0;
    if(!game_model_factory_parts_release(a) || !effect_profile_family_release(&a->memory,5) ||
       !effect_profile_family_release(&a->memory,0x41) || !effect_profile_family_release(&a->memory,0x43) ||
       !effect_profile_family_release(&a->memory,0x44) || !game_level_texture_profiles_release(a))return 0;
    return !a->memory.fault;
}
