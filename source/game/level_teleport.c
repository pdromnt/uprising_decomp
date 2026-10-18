#include "level_objects.h"
#include "level_factory.h"
#include "level_teleporters.h"
#include "level_player_slots.h"
#include "level_entity_store.h"
#include "game_initializers.h"
#include "game_intrusive_list.h"
#include "gameplay_teardown.h"
#include "application_name_registry.h"
#include "scene_instance_owner.h"
#include "scene_projection_storage.h"
#include "gameplay_geometry.h"
#include "state_helper_plans.h"
#include "effect_model.h"
#include "model_assets.h"
#include "level_effect_runtime.h"
#include "terrain_asset.h"
#include "gameplay_leaves.h"
#include "menu_sound.h"
#include <math.h>
#include <string.h>
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
static float value(GameApplication *a,GameAddress p){uint32_t b=get(a,p);float f;memcpy(&f,&b,4);return f;}
static void store(GameApplication *a,GameAddress p,float f){uint32_t b;memcpy(&b,&f,4);put(a,p,b);}
static int32_t signed_word(uint32_t v){int32_t n;memcpy(&n,&v,4);return n;}
extern int game_level_particle_emit(GameApplication *,GameAddress,const float[3],const float[3],float,GameAddress,uint16_t,uint32_t,uint32_t);
/*45fe90: the destination record is the only live animation clock. Retain
 *the pre-clamp phase as the emitted velocity, including unusual restored
 *values; the old hybrid-only visual copy is not a shipping state owner. */
int game_level_teleporter_update(GameApplication *a,GameAddress o){
    float p[3],velocity[3]={0,0,0};uint32_t phase,spin;uint16_t yaw;unsigned i;int rising;
    if(!a || a->memory.fault)return 0;
    if(!game_terrain_cell_presence(&a->memory,o,0))return !a->memory.fault;
    if(!get(a,o+0x70))return !a->memory.fault;
    for(i=0;i<3;i++)p[i]=value(a,o+24+i*4);
    p[1]=(float)((long double)value(a,o+0x30)*value(a,0x57399c)+p[1]);
    rising=get(a,o+0x80)!=0;phase=get(a,o+0x84)+(rising?2u:0xfffffffeu);
    put(a,o+0x84,phase);velocity[1]=(float)signed_word(phase);
    if(rising && signed_word(phase)>=40){put(a,o+0x84,40);put(a,o+0x80,0);}
    else if(!rising && signed_word(phase)<=-40){put(a,o+0x84,(uint32_t)-40);put(a,o+0x80,1);}
    spin=(get(a,o+0x7c)+20u)&1023u;put(a,o+0x7c,spin);yaw=game_memory_read_u16(&a->memory,o+36);
    if(a->memory.fault || !game_level_particle_emit(a,o,p,velocity,600.0f,0x585658,yaw,0,spin))return 0;
    velocity[1]=-velocity[1];
    return game_level_particle_emit(a,o,p,velocity,600.0f,0x585658,yaw,0,(uint16_t)(0u-spin));
}
/*442a10/4099b0/45fd80: the destination template uses the existing owned
 *billboard model, not an imported mesh graph. Inputs are ordinary values. */
GameAddress game_level_teleporter_destination_template(GameApplication *a){
    GameAddress object=get(a,0x5add9c),model;unsigned i;
    if(object || a->memory.fault)return object;
    if(get(a,0x624f8c)==10 || a->memory.fault)return 0;
    model=effect_model_create(1);if(!model)return 0;
    object=scene_projection_storage_allocate(0x88,"teleporter destination template");
    if(!object){effect_model_release(model);return 0;}
    game_initializer_0042cfb0(&a->memory,object);
    for(i=0;i<3;++i)store(a,object+0x2c+i*4,300.0f);
    store(a,object+0x38,300.0f);put(a,object+0x3c,0);
    game_memory_write_u16(&a->memory,object+0x44,0);put(a,object+0x54,model);
    put(a,object,0x573918);game_memory_write_u8(&a->memory,object+12,0xa0);
    put(a,object+16,(get(a,object+16)&~1u)|0xc00u);put(a,model+0x40,object);
    if(a->memory.fault){effect_model_release(model);scene_projection_storage_free(object);return 0;}
    put(a,0x5add9c,object);return a->memory.fault?0:object;
}
/*442a10/449290/45fc00: retain the decoded mesh/hull in the same template
 *that supplies source teleporter clones. No second serialized owner. */
static GameAddress source_template(GameApplication *a){
    GameAddress object=get(a,0x5adb24);GameModelDefinition d={0};unsigned i;
    if(object || a->memory.fault)return object;
    if(get(a,0x624f8c)==10 || a->memory.fault)return 0;
    if(!game_model_definition_read(a,ASSET_MODEL_SIMPLE,0,0x5848ec,0,0,0,&d))return 0;
    if(d.marker_count>32767u)goto fail;
    object=scene_projection_storage_allocate(0x64,"teleporter source template");if(!object)goto fail;
    game_initializer_0042cfb0(&a->memory,object);
    for(i=0;i<3;++i)store(a,object+0x2c+i*4,d.dimensions[i]);
    put(a,object+0x38,d.radius);put(a,object+0x3c,d.markers);
    game_memory_write_u16(&a->memory,object+0x44,(uint16_t)d.marker_count);put(a,object+0x54,d.model);
    put(a,object,0x5738a0);game_memory_write_u8(&a->memory,object+12,0xa1);
    put(a,object+16,get(a,object+16)&~1u);
    if(a->memory.fault)goto fail;
    put(a,0x5adb24,object);return a->memory.fault?0:object;
fail:
    if(object)(void)scene_projection_storage_free(object);
    (void)game_model_definition_release(a,&d);return 0;
}
static int name(GameApplication *a,const char *text,GameAddress o){uint32_t record,binding=o?get(a,o+0x14):0;return !a->memory.fault && application_name_registry_register(text,o,binding,&record);}
static GameAddress make(GameApplication *a,uint8_t kind) {
    GameAddress prototype=game_level_factory_template(a,kind,0),object=0,buffer=0,model=0,source;int count;
    if(!prototype || a->memory.fault)return 0;
    count=(int16_t)game_memory_read_u16(&a->memory,prototype+0x44);if(count<0)return 0;
    object=scene_projection_storage_allocate(kind==0xa1?0x64:0x88,"teleporter scene record");if(!object)return 0;
    if(count){buffer=scene_projection_storage_allocate((size_t)count*12,"teleporter transformed hull");if(!buffer)goto fail;}
    source=get(a,prototype+0x54);
    if(!source)goto fail;
    if(effect_model_find(source)){model=effect_model_clone(&a->memory,source);if(!model)goto fail;}
    else if(scene_instance_owner_clone(&a->memory,source,&model)!=1)goto fail;
    game_initializer_teleporter_scene_object(&a->memory,object,prototype,buffer,model,kind==0xa0);
    if(a->memory.fault)goto fail;
    return object;
fail:
    if(model){if(effect_model_find(model))(void)effect_model_release(model);else (void)scene_instance_owner_discard_clone(model);}
    if(buffer)(void)scene_projection_storage_free(buffer);
    (void)scene_projection_storage_free(object);return 0;
}
GameAddress game_level_teleporter_factory(GameApplication *a,uint8_t kind,uint16_t slot,unsigned operation,int32_t count){
    GameAddress source,object;
    if(!a || (kind!=0xa0 && kind!=0xa1) || slot || operation>2 || a->memory.fault)return 0;
    source=kind==0xa0?game_level_teleporter_destination_template(a):source_template(a);
    if(!source)return 0;
    if(!operation)return source;
    if(operation==2){
        object=get(a,source+4);
        if(object){game_list_detach_node(&a->memory,object,4,8);put(a,object+4,0);put(a,object+8,0);return a->memory.fault?0:object;}
        return make(a,kind);
    }
    while(count>0){
        object=make(a,kind);if(!object)return 0;
        game_list_insert_after(&a->memory,object,source,4,8);
        if(a->memory.fault)return 0;
        --count;
    }
    return 1;
}
static int next(GameApplication *a,GameAddress player,uint32_t *id){uint32_t sequence;if(!level_player_slots_next_owner_sequence(&a->memory,player,id,&sequence))return 0;put(a,player+0x18,sequence);return !a->memory.fault;}
static int attach(GameApplication *a,GameAddress player,GameAddress o) {
    unsigned offset=state_helper_flagged_member_offset(get(a,o+0x10));
    if(a->memory.fault || !level_player_slots_attach_scene_member(player,o,offset))return 0;
    (void)game_list_insert_after(&a->memory,o,player+offset,0x48,0x4c);
    if(a->memory.fault){level_player_slots_forget_scene_member(o);return 0;}
    return level_entity_store_transfer_guest(o);
}
/*45fe10 owns destination coordinates independently of source terrain
 *adjustment.460040 binds the source model's faction descriptor. */
int game_level_teleporter_destination_place(GameApplication *a,GameAddress o,uint32_t id,float source[3],const float destination[3],GameAddress peer) {
    uint16_t angles[3]={0};unsigned i;
    for(i=0;i<3;++i)store(a,o+0x64+i*4,destination[i]);
    put(a,o+0x70,1);source[1]=(float)((long double)gameplay_geometry_terrain_height(&a->memory,source[0],source[2])+value(a,0x573994));
    if(!game_level_scene_object_place(a,o,id,source,angles))return 0;
    put(a,o+0x7c,0);put(a,o+0x80,0);put(a,o+0x84,0);put(a,o+0x74,peer);return !a->memory.fault;
}
int game_level_teleporter_bind(GameApplication *a,GameAddress o,GameAddress player) {
    GameAddress peer=get(a,o+0x74),model=get(a,peer+0x54);put(a,o+0x78,player);
    if(!model || a->memory.fault)return 0;
    game_initializer_00413690(&a->memory,model,player?player+0x10:0x589760);return !a->memory.fault;
}
/*45ffc0: both enable and disable transitions play the same positional sound.
 *The actual enabled word is retained, rather than normalizing script values. */
int game_level_teleporter_enable(GameApplication *a,GameAddress o,uint32_t enabled){
    SoundPlaybackRequest sound={0};GameAddress camera;long double x,z;
    if(!a || a->memory.fault)return 0;
    if(get(a,o+0x70)==enabled)return !a->memory.fault;
    put(a,o+0x70,enabled);camera=get(a,0x58f890);
    x=(long double)value(a,o+24)-value(a,camera+12);z=(long double)value(a,o+32)-value(a,camera+20);
    sound.sound=7;sound.volume=1;sound.distance_squared=(float)(x*x+z*z);
    return !a->memory.fault && game_menu_sound_play(a,&sound);
}
static int construct(void *context,uint32_t index,const LevelTeleporterDefinition *d,uint32_t *source_output,uint32_t *destination_output) {
    GameApplication *a=context;GameAddress player=0,source,destination;uint32_t id;unsigned i;float heading=d->heading_degrees*5.688889026641846f;
    float p[3]={d->source.x*512.0f,d->source.y,d->source.z*512.0f},target[3]={d->destination.x*512.0f,d->destination.y,d->destination.z*512.0f};
    (void)index;*source_output=*destination_output=0;
    if(get(a,0x5acc88))return !strcmp(d->name,"NULL") || name(a,d->name,0);
    if(!isfinite(heading) || heading<-2147483648.0f || heading>=2147483648.0f)return 0;
    for(i=0;i<6;++i){GameAddress candidate;if(!level_player_slots_scene_player(i,&candidate))return 0;if(candidate && get(a,candidate+0x14))player=candidate;}
    if(!player || a->memory.fault)return 0;
    source=make(a,0xa1);if(!source)return 0;
    if(!level_entity_store_track_guest(source)){(void)game_level_teleporter_destroy(a,source);return 0;}
    destination=make(a,0xa0);if(!destination)return 0;
    if(!level_entity_store_track_guest(destination)){(void)game_level_teleporter_destroy(a,destination);return 0;}
    if(!next(a,player,&id) || !game_level_object_place(a,source,id,p,(uint32_t)(int32_t)heading&2047u) ||
       !next(a,player,&id) || !game_level_teleporter_destination_place(a,destination,id,p,target,source) ||
       !attach(a,player,source) || !attach(a,player,destination) || !game_level_teleporter_bind(a,destination,0) ||
       (strcmp(d->name,"NULL") && !name(a,d->name,destination)))return 0;
    *source_output=source;*destination_output=destination;
    return !a->memory.fault;
}
int game_level_teleporters_load(GameApplication *a,const char *path,int32_t offset,uint32_t *next_offset){return level_teleporters_load(path,offset,a,construct,next_offset);}
int game_level_teleporter_destroy(GameApplication *a,GameAddress o) {
    GameAddress model=gameplay_teardown_begin_scene_object(&a->memory,o);GameplaySceneObjectMembers members;
    if(a->memory.fault)return 0;
    if(model){
        if(effect_model_find(model)){if(!effect_model_release(model))return 0;}
        else if(scene_instance_owner_destruct_graph(model)!=1)return 0;
        put(a,o+0x54,0);
    }
    members=gameplay_teardown_scene_object_members(&a->memory,o);
    if(members.buffer){if(!scene_projection_storage_free(members.buffer))return 0;put(a,o+0x40,0);}
    if(members.auxiliary){if(!scene_projection_storage_free(members.auxiliary))return 0;put(a,o+0x3c,0);}
    gameplay_teardown_finish_scene_object(&a->memory,o);return !a->memory.fault && scene_projection_storage_free(o);
}
