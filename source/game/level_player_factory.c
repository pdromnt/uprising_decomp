#include "level_player_factory.h"
#include "level_factory.h"
#include "model_assets.h"
#include "player_runtime_pool.h"
#include "player_scene_children.h"
#include "presentation_instance.h"
#include "event_queue.h"
#include "scene_projection_storage.h"
#include <string.h>

typedef struct PlayerDefinition {
    PresentationInstanceDefinition base;PlayerRigDefinition rig;
    GameAddress points,parts;uint32_t point_count,part_count;
} PlayerDefinition;
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
static float real(GameApplication *a,GameAddress p){uint32_t word=get(a,p);float value;memcpy(&value,&word,4);return value;}
static int free_page(void *context,GameAddress p){(void)context;return !p || scene_projection_storage_free(p);}
static int release_model(void *context,GameAddress p){return !p || game_application_scene_model_release(context,p);}
static int release_member(void *context,GameAddress p){return game_level_factory_release_pool_member(context,p);}
static int detach_chain(void *context,GameAddress p){return game_level_factory_detach_packet_chain(context,p);}
static PresentationReleaseServices retirement(GameApplication *a){
    PresentationReleaseServices s={a,release_model,free_page,release_member,detach_chain};return s;
}
static int release_base(GameApplication *a,GameAddress object){
    PresentationReleaseServices s=retirement(a);
    if(presentation_instance_find(object) && presentation_instance_release(&a->memory,object,&s,1,1)!=1)return 0;
    player_scene_children_retire(object);return scene_projection_storage_free(object);
}
static int child_release(void *context,GameAddress child,unsigned part){
    (void)context;(void)part;return player_scene_children_release(child)==1;
}
static int projection_clear(void *context,GameAddress object){
    GameApplication *a=context;unsigned i;
    for(i=0;i<15;++i)put(a,object+0x1d4+i*4,0);
    put(a,object+0x210,0);put(a,object+0x1cc,0);put(a,object+0x1d0,0);return !a->memory.fault;
}
static int event_release(void *context,GameAddress event){
    GameApplication *a=context;EffectResource *record=effect_resource_find(event);
    if(!record || record->owner->object_kind<1 || record->owner->object_kind>3 ||
       record->event_route!=record->owner->object_kind-1)return 0;
    return effect_resource_pool_return(&a->memory,record->owner,event);
}
int game_level_player_clear_events(GameApplication *a,GameAddress object){
    GameEventQueueCallbacks callbacks={a,NULL,event_release};
    return game_timed_event_queue_clear(&a->memory,&callbacks,object+0x13c);
}
static int events_clear(void *context,GameAddress object){
    GameApplication *a=context;
    if(!game_level_player_clear_events(a,object))return 0;
    put(a,object+0x13c,0x577a38);return !a->memory.fault;
}
static int base_destroy(void *context,GameAddress object){return release_base(context,object);}
/*4f0880: the existing pool remembers child/base retirement, not a second
 *serialized object. Every heterogeneous child must have its real native owner. */
int game_level_player_factory_destroy(GameApplication *a,GameAddress object){
    GameAddress parts[15];PlayerRuntimeDestroyServices s={a,child_release,projection_clear,events_clear,base_destroy};
    if(!object)return 1;
    /*Finalization can retire children while retaining the playable instance.
     *Refresh its live slots; the pool's retirement cursor still prevents repeats.
     *A completed base has no readable object and retains its completed phase. */
    if((presentation_instance_find(object) || !player_runtime_pool_parts(object,parts)) &&
       !player_runtime_pool_bind_parts(&a->memory,object))return 0;
    if(!player_runtime_pool_destroy_parts(object,&s))return 0;
    (void)player_runtime_pool_retire(object);return !a->memory.fault;
}
/*449600/443773: decode the actual three-mesh definition once. Output slots
 *are bounded transient destinations for the existing asset loader, not a guest
 *stack or a persistent serialized mirror. Cached arrays keep their old owners. */
static int definition_read(GameApplication *a,uint16_t index,PlayerDefinition *d){
    SceneAssetRequest request={0};GameAddress work;unsigned i,j;uint64_t total;
    static const unsigned offsets[14]={0,12,16,20,24,28,32,44,56,68,72,76,80,84};
    memset(d,0,sizeof(*d));work=scene_projection_storage_allocate(88,"player model definition outputs");if(!work)return 0;
    request.format=ASSET_MODEL_THREE_MESH;request.index=index;request.names=0x5846d4;
    for(i=0;i<14;++i)request.outputs[i]=work+offsets[i];
    if(!game_model_asset_definition_load(a,&request)){scene_projection_storage_free(work);return 0;}
    for(i=0;i<3;++i){d->base.dimensions[i]=real(a,work+i*4);
        for(j=0;j<3;++j){float value=real(a,work+32+i*12+j*4);
            if(i==0)d->rig.mount_origin[j]=value;else if(i==1)d->rig.turret_origin[j]=value;else d->rig.pivot[j]=value;}}
    d->base.radius=get(a,work+12);d->base.resource=get(a,work+16);
    total=(uint64_t)get(a,work+20)+get(a,work+24)+get(a,work+28);
    d->rig.mount_part=(uint16_t)get(a,work+20);d->rig.turret_part=(uint16_t)(get(a,work+20)+get(a,work+24));
    d->base.model=get(a,work+68);d->point_count=get(a,work+72);d->points=get(a,work+76);
    d->part_count=get(a,work+80);d->parts=get(a,work+84);
    scene_projection_storage_free(work);
    if(total>INT16_MAX || a->memory.fault)return 0;d->base.count=(int32_t)total;return 1;
}
static int definition_release(GameApplication *a,PlayerDefinition *d){
    int ok=release_model(a,d->base.model);
    if(!free_page(a,d->base.resource))ok=0;
    if(!free_page(a,d->points))ok=0;
    if(!free_page(a,d->parts))ok=0;
    memset(d,0,sizeof(*d));return ok;
}
GameAddress game_level_player_factory_template(GameApplication *a,uint16_t index){
    PlayerRuntimePoolSlot slot;PlayerDefinition definition={0};GameAddress object=0;int constructed=0;
    if(index>=PLAYER_RUNTIME_POOL_SLOTS || a->memory.fault)return 0;
    if(player_runtime_pool_slot(index,&slot))return slot.template_object;
    if(get(a,0x5add98+index*4) || get(a,0x5adbc4+index*4) || get(a,0x608148+index*4) ||
       get(a,0x624f8c)==10 || a->memory.fault)return 0;
    if(!definition_read(a,index,&definition))goto failed;
    game_memory_write_u8(&a->memory,definition.base.model+0x3c,
        game_memory_read_u8(&a->memory,definition.base.model+0x3c)|2);
    object=scene_projection_storage_allocate(0x4a4,"player runtime template");if(!object)goto failed;
    if(!presentation_instance_create(&a->memory,object,&definition.base,1))goto failed;
    constructed=1;definition.base.model=definition.base.resource=0;
    if(!player_rig_construct(&a->memory,object,0,&definition.rig) || !player_runtime_pool_bind(index,object))goto failed;
    put(a,0x5add98+index*4,object);put(a,0x5adcf8+index*4,definition.point_count);put(a,0x5adbc4+index*4,definition.points);
    put(a,0x608144+index*4,definition.part_count);put(a,0x608148+index*4,definition.parts);
    return a->memory.fault?0:object;
failed:
    if(constructed)(void)release_base(a,object);else if(object)scene_projection_storage_free(object);
    (void)definition_release(a,&definition);return 0;
}
/*446262/447d20: clone real model/vertex/controller/rig state, then place that
 *allocation in the existing ready pool. Taking it creates the sole active entry. */
int game_level_player_factory_fill(GameApplication *a,uint16_t index,int32_t count){
    GameAddress source=game_level_player_factory_template(a,index);int32_t i;
    if(!source)return 0;
    for(i=0;i<count;++i){GameAddress clone=scene_projection_storage_allocate(0x4a4,"player runtime clone");
        if(!clone)return 0;
        if(!presentation_instance_create_copy(&a->memory,clone,source,1,NULL)){
            scene_projection_storage_free(clone);return 0;
        }
        if(!player_rig_construct(&a->memory,clone,source,NULL) || !player_runtime_pool_attach(index,clone)){
            (void)release_base(a,clone);return 0;
        }
    }return !a->memory.fault;
}
GameAddress game_level_player_factory_take(GameApplication *a,uint16_t index){
    if(!game_level_player_factory_template(a,index) ||
       (!player_runtime_pool_has_ready(index) && !game_level_player_factory_fill(a,index,1)))return 0;
    return player_runtime_pool_take(index);
}
int game_level_player_factory_shutdown(GameApplication *a){
    uint16_t i;
    for(i=0;i<PLAYER_RUNTIME_POOL_SLOTS;++i){PlayerRuntimePoolSlot slot;GameAddress object;
        if(!player_runtime_pool_slot(i,&slot))continue;
        while((object=player_runtime_pool_begin_shutdown(i))!=0)if(!game_level_player_factory_destroy(a,object))return 0;
        if(player_runtime_pool_has_ready(i) || player_runtime_pool_has_borrowed(i))return 0;
        if(!game_level_player_factory_destroy(a,slot.template_object))return 0;
        put(a,0x5add98+i*4,0);
        if(!free_page(a,get(a,0x5adbc4+i*4)))return 0;
        put(a,0x5adbc4+i*4,0);put(a,0x5adcf8+i*4,0);
        if(!free_page(a,get(a,0x608148+i*4)))return 0;
        put(a,0x608148+i*4,0);put(a,0x608144+i*4,0);
        player_runtime_pool_forget(i);
    }return !a->memory.fault;
}
