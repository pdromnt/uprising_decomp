#include "level_factory.h"
#include "level_effect_runtime.h"
#include "level_objects.h"
#include "level_player_resources.h"
#include "level_player_slots.h"
#include "model_assets.h"
#include "spatial_sample.h"
#include "scene_instance_owner.h"
#include "scene_projection_storage.h"
#include <string.h>
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
static GameAddress allocate(void *c,size_t size){(void)c;return scene_projection_storage_allocate(size,"station payload");}
static int release(void *c,GameAddress p){(void)c;return !p || scene_projection_storage_free(p);}
static int release_model(void *c,GameAddress p){return game_application_scene_model_release(c,p);}
static int clone(void *c,GameAddress p,GameAddress *out){GameApplication *a=c;return scene_instance_owner_clone(&a->memory,p,out)==1;}
static int contacts(void *c,GameAddress p){return game_level_factory_release_pool_member(c,p);}
static int packets(void *c,GameAddress p){return game_level_factory_detach_packet_chain(c,p);}
static int describe_spatial(void *c,uint16_t slot,SpatialSampleDefinition *d){return game_level_spatial_describe(c,slot,d);}
static int describe_articulated(void *c,uint16_t slot,PresentationArticulatedDefinition *d){return game_level_articulated_describe(c,slot,d);}
GameAddress game_level_station_payload_factory(GameApplication *a,uint8_t kind,uint16_t slot,unsigned operation,int32_t count){
    PresentationReleaseServices retirement={a,release_model,release,contacts,packets};
    PresentationConstructionServices copying={a,clone};
    if(!a || a->memory.fault || operation>2)return 0;
    if(kind==0x89){
        SpatialSampleFactoryServices s={a,describe_spatial,allocate,release,copying,retirement};
        if(!operation)return spatial_sample_template(&a->memory,slot,&s);
        if(operation==1)return (GameAddress)spatial_sample_fill(&a->memory,slot,count,&s);
        return spatial_sample_take(&a->memory,slot,&s);
    }
    if(kind==0x88){
        PresentationArticulatedFactoryServices s={a,describe_articulated,allocate,release,copying,retirement};
        if(!operation)return presentation_articulated_template(&a->memory,slot,&s);
        if(operation==1)return (GameAddress)presentation_articulated_fill(&a->memory,slot,count,&s);
        return presentation_articulated_take(&a->memory,slot,&s);
    }
    return 0;
}
int game_level_station_payload_destroy(GameApplication *a,GameAddress object){
    const PresentationInstance *instance=presentation_instance_find(object);
    PresentationReleaseServices services={a,release_model,release,contacts,packets};
    uint8_t kind;unsigned slot,visits=0;GameAddress publication;
    if(!a || !instance || a->memory.fault)return 0;
    kind=instance->definition.type;if(kind!=0x88 && kind!=0x89)return 0;
    if(!instance->definition.source){GameAddress child;
        while((child=get(a,object+4))!=0){
            const PresentationInstance *borrow=presentation_instance_find(child);
            if(++visits>65536 || !borrow || borrow->definition.source!=object ||
               !game_level_station_payload_destroy(a,child))return 0;
        }
    }
    if(!presentation_instance_source_is_empty(object) ||
       presentation_instance_release(&a->memory,object,&services,1,0)!=1 || !release(a,object))return 0;
    (void)level_player_slots_forget_scene_member(object);
    for(slot=0;slot<(kind==0x88?2u:6u);slot++){
        publication=(kind==0x88?0x5add90:0x5adb28)+slot*4;
        if(get(a,publication)==object)put(a,publication,0);
    }
    return !a->memory.fault;
}
static int place(void *c,GameAddress object,uint32_t id,const float p[3],const uint16_t angles[3]){
    return game_level_object_place_common(c,object,id,p,angles,1);
}
static int catalog(void *c,GameAddress owner,uint8_t kind,uint32_t item,uint32_t alternate,uint32_t *out){
    return game_level_player_catalog_value(c,owner,kind,item,alternate,out);
}
static int spatial_catalog(void *c,GameAddress owner,uint8_t kind,unsigned item,uint32_t *out){return catalog(c,owner,kind,item,0,out);}
/*The descriptor algorithm supplies an ordinary local point. The legacy native
 *initializer's mutable point view is bounded and owned only for this call. */
int game_level_articulated_descriptor_place(GameApplication *a,GameAddress object,uint32_t id,
        const float p[3],uint16_t heading,GameAddress descriptor,int restored){
    uint16_t angles[3]={heading,0,0};GameAddress point;uint32_t bits,result;unsigned i;int ok;
    uint8_t kind=game_memory_read_u8(&a->memory,object+12);
    PresentationActivationServices placement={a,place,catalog};SpatialSampleServices spatial={0};
    if((kind!=0x88 && kind!=0x89) || get(a,get(a,object)+0x98)!=(kind==0x88?0x436510u:0x50ec10u) || a->memory.fault)return 0;
    point=allocate(a,12);if(!point)return 0;
    for(i=0;i<3;i++){memcpy(&bits,p+i,4);put(a,point+i*4,bits);}
    spatial.effects.context=a;spatial.catalog_value=spatial_catalog;
    ok=kind==0x88?presentation_instance_articulated_activate(&a->memory,object,id,point,angles,descriptor,restored,&placement,&result):
        spatial_sample_activate(&a->memory,object,id,point,angles,descriptor,restored,&placement,&spatial);
    if(!release(a,point))return 0;
    return ok;
}
static int retire_effect(void *c,GameAddress effect,int immediate){GameApplication *a=c;
    if(!immediate)return 0;
    put(a,effect+0xb4,3);return !a->memory.fault; /*49aad0. */
}
static int base_reset(void *c,GameAddress object){return game_level_presentation_reset(c,object,1);}
static int spatial_base_reset(void *c,GameAddress object,int notify){
    PresentationArticulatedServices services={0};services.context=c;services.reset=base_reset;
    return presentation_instance_source_reset(&((GameApplication *)c)->memory,object,notify,&services);
}
static int unit(void *c,GameAddress owner,uint8_t kind,int enabled){GameApplication *a=c;uint32_t method=get(a,get(a,owner)+(enabled?0x5c:0x60));
    if(kind!=11 || method!=(enabled?0x47f340u:0x47f420u) || a->memory.fault)return 0;
    put(a,owner+0x4e4,get(a,owner+0x4e4)+(enabled?1u:UINT32_MAX));return !a->memory.fault;
}
int game_level_articulated_descriptor_retire(GameApplication *a,GameAddress object){
    uint8_t kind=game_memory_read_u8(&a->memory,object+12);GameAddress method=get(a,get(a,object)+0x9c);
    PresentationArticulatedServices services={0};SpatialSampleServices spatial={0};
    if(a->memory.fault)return 0;
    services.context=a;services.retire_effect=retire_effect;services.reset=base_reset;
    if(kind==0x88 && method==0x436640)return presentation_instance_articulated_reset(&a->memory,object,0,&services);
    spatial.effects=services;spatial.base_reset=spatial_base_reset;spatial.unit=unit;
    return kind==0x89 && method==0x50f0b0 && spatial_sample_reset(&a->memory,object,0,&spatial);
}
int game_level_station_payload_refresh(GameApplication *a,GameAddress object,GameAddress method){
    if(!a || a->memory.fault)return 0;
    switch(method){
    case 0x436c90:return presentation_instance_articulated_present(&a->memory,object);
    case 0x437020:return presentation_instance_articulated_render(&a->memory,object);
    case 0x437320:return presentation_instance_articulated_vertices(&a->memory,object);
    case 0x437ab0:return presentation_instance_articulated_render_vertices(&a->memory,object);
    case 0x50f370:return spatial_sample_frame(&a->memory,object,0);
    case 0x50f620:return spatial_sample_frame(&a->memory,object,1);
    case 0x50f890:return spatial_sample_vertices(&a->memory,object,0);
    case 0x50fbf0:return spatial_sample_vertices(&a->memory,object,1);
    default:return 0;
    }
}
