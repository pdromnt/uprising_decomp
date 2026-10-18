#include "level_factory.h"
#include "level_objects.h"
#include "level_effect_runtime.h"
#include "level_player_slots.h"
#include "model_assets.h"
#include "presentation_instance.h"
#include "scene_instance_owner.h"
#include "scene_projection_storage.h"
#include "game_intrusive_list.h"
#include "carrier_effect.h"
#include "menu_sound.h"
#include <string.h>
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
static int release_model(void *c,GameAddress model){return game_application_scene_model_release(c,model);}
static int release_page(void *c,GameAddress page){(void)c;return !page || scene_projection_storage_free(page);}
static int clone_model(void *c,GameAddress model,GameAddress *out){GameApplication *a=c;return scene_instance_owner_clone(&a->memory,model,out)==1;}
static int return_contacts(void *c,GameAddress chain){return game_level_factory_release_pool_member(c,chain);}
static int return_packets(void *c,GameAddress chain){return game_level_factory_detach_packet_chain(c,chain);}
static int owns(GameApplication *a,GameAddress o){const PresentationInstance *instance=presentation_instance_find(o);
    return a && instance && instance->definition.type==0x2a && get(a,o)==0x573478 && !a->memory.fault;
}
int game_level_carrier_destroy(GameApplication *a,GameAddress o){
    PresentationReleaseServices s={a,release_model,release_page,return_contacts,return_packets};unsigned slot,visits=0;
    if(!owns(a,o))return 0;
    /*Cloned carriers borrow the template's hull. Never retire that template
     *while any acquired or pooled clone still holds its borrowed inputs. */
    if(!presentation_instance_find(o)->definition.source){
        GameAddress child;
        while((child=get(a,o+4))!=0){
            if(++visits>65536 || !owns(a,child) || presentation_instance_find(child)->definition.source!=o ||
               !game_level_carrier_destroy(a,child))return 0;
        }
    }
    if(!presentation_instance_source_is_empty(o))return 0;
    if(presentation_instance_release(&a->memory,o,&s,1,0)!=1 || !scene_projection_storage_free(o))return 0;
    (void)level_player_slots_forget_scene_member(o);
    for(slot=0;slot<4;slot++)if(get(a,0x5add88+slot*4)==o)put(a,0x5add88+slot*4,0);
    return !a->memory.fault;
}
GameAddress game_level_carrier_factory(GameApplication *a,uint16_t slot,unsigned operation,int32_t count){
    GameAddress source,object;PresentationConstructionServices copying={a,clone_model};int32_t i;
    if(!a || slot>=4 || operation>2 || a->memory.fault)return 0;
    source=get(a,0x5add88+slot*4);
    if(!source){
        GameModelDefinition asset={0};PresentationInstanceDefinition d={0};
        if(get(a,0x624f8c)==10 || !game_model_definition_read(a,ASSET_MODEL_SIMPLE,slot,0x584778,1,0,0,&asset))return 0;
        if(asset.marker_count>32767){game_model_definition_release(a,&asset);return 0;}
        game_memory_write_u8(&a->memory,asset.model+0x3c,game_memory_read_u8(&a->memory,asset.model+0x3c)|0x20);
        d.type=0x2a;d.flags=0xc08;memcpy(d.dimensions,asset.dimensions,sizeof(d.dimensions));
        d.radius=asset.radius;d.resource=asset.markers;d.count=(int32_t)asset.marker_count;d.model=asset.model;
        source=scene_projection_storage_allocate(0x108,"carrier template");
        if(!source || !presentation_instance_create(&a->memory,source,&d,0)){
            if(source)scene_projection_storage_free(source);
            game_model_definition_release(a,&asset);return 0;
        }
        put(a,source,0x573478);put(a,0x5add88+slot*4,source);
        if(a->memory.fault)return 0;
    }
    if(!owns(a,source))return 0;
    if(!operation)return source;
    if(operation==2){
        if(!get(a,source+4) && !game_level_carrier_factory(a,slot,1,1))return 0;
        object=get(a,source+4);
        if(!object)return 0;
        game_list_detach_node(&a->memory,object,8,4);put(a,object+4,0);put(a,object+8,0);
        return a->memory.fault?0:object;
    }
    for(i=0;i<count;i++){
        object=scene_projection_storage_allocate(0x108,"carrier pooled clone");if(!object)return 0;
        if(!presentation_instance_create_copy(&a->memory,object,source,0,&copying)){
            scene_projection_storage_free(object);return 0;
        }
        put(a,object,0x573478);game_list_insert_after(&a->memory,object,source,4,8);
        if(a->memory.fault)return 0;
    }
    return 1;
}
static int place(void *c,GameAddress o,uint32_t id,const float p[3],const uint16_t angles[3]){
    return game_level_object_place_common(c,o,id,p,angles,1);
}
static int reset(void *c,GameAddress o){return game_level_presentation_reset(c,o,1);}
static int play(void *c,const SoundPlaybackRequest *r){return game_menu_sound_play(c,r);}
static int stop(void *c,int16_t sound,uint32_t voice){return game_menu_sound_stop(c,sound,voice);}
static int playing(void *c,int16_t sound,uint32_t voice,int *out){return game_menu_sound_playing(c,sound,voice,out);}
static int frequency(void *c,int16_t sound,uint32_t voice,float pitch){GameApplication *a=c;return game_menu_sound_frequency(a,get(a,0x628a90),sound,voice,pitch);}
static int volume(void *c,int16_t sound,uint32_t voice,float distance){GameApplication *a=c;GameAddress bank=get(a,0x628a90);
    float value=sound_playback_attenuation(&a->memory,get(a,bank+12+(uint32_t)(uint16_t)sound*4),distance);
    return !a->memory.fault && game_menu_sound_volume(a,sound,voice,value);
}
static CarrierServices services(GameApplication *a){CarrierServices s={a,place,reset,play,stop,playing,frequency,volume};return s;}
int game_level_carrier_place(GameApplication *a,GameAddress o,uint32_t id,const float p[3],const uint16_t angles[3]){
    CarrierServices s=services(a);return owns(a,o) && carrier_effect_place(&a->memory,o,id,p,angles,&s);
}
int game_level_carrier_update(GameApplication *a,GameAddress o){CarrierServices s=services(a);return owns(a,o) && carrier_effect_update(&a->memory,o,&s);}
int game_level_carrier_reset(GameApplication *a,GameAddress o){CarrierServices s=services(a);return owns(a,o) && carrier_effect_reset(&a->memory,o,&s);}
