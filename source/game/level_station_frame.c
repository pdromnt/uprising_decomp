#include "level_factory.h"
#include "level_effect_runtime.h"
#include "level_objects.h"
#include "level_player_slots.h"
#include "level_player_resources.h"
#include "gameplay_state.h"
#include "game_state_ops.h"
#include "gameplay_catalog.h"
#include "gameplay_geometry.h"
#include "gameplay_leaves.h"
#include "game_intrusive_list.h"
#include "spatial_sample.h"
#include "descent_payload.h"
extern int game_level_scene_update(GameApplication *,GameAddress,GameAddress);
#include "menu_sound.h"
#include <string.h>
#include <stdio.h>
extern int game_level_scene_contacts(GameApplication *,GameAddress,GameAddress,const float[3],int *);
extern int game_level_contact_response(GameApplication *,GameAddress,GameAddress,GameAddress,const uint32_t[3],const float[3]);
extern int game_level_scene_property(GameApplication *,GameAddress,GameAddress,uint32_t *);
extern int game_level_group_member_detach(GameApplication *,GameAddress);
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
static void point(GameApplication *a,GameAddress p,float out[3]){unsigned i;for(i=0;i<3;i++){uint32_t bits=get(a,p+i*4);memcpy(out+i,&bits,4);}}
static int clear_queue(void *c,GameAddress bank){return game_menu_sound_queue_clear(c,bank);}
static int submit_queue(void *c,GameAddress bank,const int16_t sounds[8],int *accepted){return game_menu_sound_queue_submit(c,bank,sounds,accepted);}
int game_level_station_payload_notice(GameApplication *a,GameAddress object,GameAddress method,uint32_t code){
    GamePresentationFeedbackServices feedback={a,clear_queue,submit_queue};int16_t announcement;
    if(!a || a->memory.fault)return 0;
    switch(method){
    case 0x4fabf0:{
        static const int16_t announcements[4]={0x54,0x55,0x56,0x58};
        unsigned kind=game_memory_read_u8(&a->memory,object+12);
        if(code!=1 && code!=3)return !a->memory.fault;
        /*The retail base leaves this local uninitialized outside84..87.
         *Reject that undefined announcement rather than fabricate a sound. */
        if(kind<0x84 || kind>0x87)return 0;
        announcement=announcements[kind-0x84];break;
    }
    case 0x4394f0:announcement=0x5c;break;case 0x511000:announcement=0x59;break;
    case 0x51b000:announcement=0x54;break;case 0x51b290:announcement=0x55;break;
    case 0x51b520:announcement=0x56;break;case 0x51b840:announcement=0x58;break;
    default:return 0;
    }
    return game_presentation_deactivation_feedback(&a->memory,object,code,announcement,&feedback);
}
static int health(void *c,GameAddress object,uint32_t *out){GameApplication *a=c;
    return game_level_scene_property(a,object,get(a,get(a,object)+0x44),out);
}
static int effect_retire(void *c,GameAddress object,int immediate){GameApplication *a=c;ExpansionEffect *effect=presentation_instance_expansion(object);
    if(!effect || a->memory.fault)return 0;
    if(immediate)expansion_effect_remove(effect);else expansion_effect_shrink(effect);
    return 1;
}
static int publish(GameApplication *a,GameAddress owner,GameAddress object){
    uint32_t flags=get(a,object+16),slot=(flags&1)?((flags&64)?0x80:0x1c):0xe4;
    if(a->memory.fault || !level_player_slots_attach_scene_member(owner,object,slot))return 0;
    game_list_insert_after(&a->memory,object,owner+slot,0x48,0x4c);
    if(a->memory.fault){level_player_slots_forget_scene_member(object);return 0;}return 1;
}
static int effect_create(void *c,GameAddress object,const float p[3],float width,float height,GameAddress *out){
    GameApplication *a=c;GameAddress owner=get(a,0x58973c),effect;uint32_t id,next;(void)object;*out=0;
    effect=game_level_factory_take(a,0x44,0);if(!effect)return 0;
    if(level_player_slots_owns_player(owner)){
        if(!level_player_slots_next_owner_sequence(&a->memory,owner,&id,&next))goto failed;
        put(a,owner+24,next);
    }else id=game_state_next_owner_sequence(&a->memory,owner);
    if(a->memory.fault || !game_level_expansion_start(a,effect,id,p,width,height,-1) || !publish(a,owner,effect))goto failed;
    *out=effect;return 1;
failed:
    {
        int fault=a->memory.fault;GameAddress fault_address=a->memory.fault_address;
        a->memory.fault=0;
        (void)game_level_factory_destroy(a,effect);
        if(fault){a->memory.fault=fault;a->memory.fault_address=fault_address;}
        return 0;
    }
}
static int surface(void *c,GameAddress object,GameAddress location){GameApplication *a=c;float p[3];point(a,location,p);
    return !a->memory.fault && game_level_surface_particles(a,object,p,10,0x6297f0);
}
static int credit(void *c,GameAddress owner,uint32_t amount){GameApplication *a=c;
    GameAddress method=get(a,get(a,owner)+0x38);
    if(method==0x47f250)return gameplay_catalog_credit(&a->memory,owner,amount);
    if(method==0x47f290 || method==0x4794b0 || method==0x4794e0){
        put(a,owner+0x22c,get(a,owner+0x22c)+amount);put(a,owner+0x150,get(a,owner+0x150)+amount);return !a->memory.fault;
    }
    if(method==0x477750)return !a->memory.fault; /*Inherited empty credit. */
    fprintf(stderr,"native station credit failed: owner=%08x method=%08x amount=%u\n",(unsigned)owner,(unsigned)method,amount);return 0;
}
static int sound(void *c,GameAddress object,int32_t id){GameApplication *a=c;
    SoundPlaybackRequest request={(int16_t)id,(float)game_leaf_horizontal_distance_squared(&a->memory,object+0x18,get(a,0x58f890)),1,0,0,0};
    return !a->memory.fault && game_menu_sound_play(a,&request);
}
static int gather(void *c,GameAddress object){GameApplication *a=c;float p[3];int rollback;point(a,object+0x18,p);
    return !a->memory.fault && game_level_scene_contacts(a,object,get(a,get(a,object)+0x78),p,&rollback);
}
static int visible(void *c,GameAddress object,uint32_t *out){GameApplication *a=c;*out=game_terrain_cell_presence(&a->memory,object,0);return !a->memory.fault;}
static int contact(void *c,GameAddress object,GameAddress other,const float p[3]){GameApplication *a=c;const uint32_t velocity[3]={0};
    GameAddress method=get(a,get(a,object)+0x2c);int ok=game_level_contact_response(a,object,other,method,velocity,p);
    if(!ok)fprintf(stderr,"native station contact failed: object=%08x other=%08x method=%08x\n",object,other,method);
    return ok;
}
static int release_contacts(void *c,GameAddress head){return game_level_factory_release_pool_member(c,head);}
static int refresh_effect(void *c,GameAddress effect,const float p[3]){return game_level_expansion_move(c,effect,p);}
static int deactivate(void *c,GameAddress object){GameApplication *a=c;return game_level_station_payload_notice(a,object,get(a,get(a,object)+0x58),1);}
static int reset_base(void *c,GameAddress object){return game_level_presentation_reset(c,object,1);}
static int detach_source(void *c,GameAddress source,GameAddress object){GameApplication *a=c;(void)object;
    uint32_t method=get(a,get(a,source)+12);
    return (method==0x53e3e0 || method==0x53f120 || method==0x53fbf0) && game_level_group_member_detach(a,source);
}
static PresentationArticulatedServices services(GameApplication *a){
    PresentationArticulatedServices s={a,health,effect_create,effect_retire,surface,credit,sound,gather,contact,contact,release_contacts,refresh_effect,deactivate,reset_base,detach_source};return s;
}
/*4718f0/4719a0/471a70: the descent is advanced by its contact command.
 *Its existing actor owns contact packets; receivers select native response
 *programs with ordinary arguments. No guest callback participates. */
static int descent_move(void *c,GameAddress object,GameAddress location){
    GameApplication *a=c;float p[3];int rollback;point(a,location,p);
    return !a->memory.fault && game_level_scene_contacts(a,object,get(a,get(a,object)+0x78),p,&rollback);
}
static int descent_phase(void *c,GameAddress object,GameAddress method){return game_level_scene_update(c,object,method);}
/*471a70 calls5362d0: retire contacts and return the extended presentation,
 *without runtime-entity peers or command packets that kind11 never owns. */
static int descent_reset_base(void *c,GameAddress object){return game_level_presentation_reset(c,object,1);}
int game_level_descent_update(GameApplication *a,GameAddress object,GameAddress method){
    DescentUpdateServices s={0};s.contacts.context=a;s.contacts.command=contact;
    s.contacts.release_commands=release_contacts;s.contacts.reset=descent_reset_base;
    s.move=descent_move;s.phase=descent_phase;
    if(method==0x4718f0)return descent_payload_advance(&a->memory,object,&s);
    if(method==0x4719a0)return descent_payload_step(&a->memory,object,&s);
    return method==0x471a70 && descent_payload_reset(&a->memory,object,&s);
}
static int catalog(void *c,GameAddress owner,uint8_t kind,unsigned item,uint32_t *out){
    return game_level_player_catalog_value(c,owner,kind,item,0,out);
}
static int unit(void *c,GameAddress owner,uint8_t kind,int enabled){GameApplication *a=c;uint32_t method=get(a,get(a,owner)+(enabled?0x5c:0x60));
    uint32_t offset=0;
    if(method==(enabled?0x477790u:0x4777a0u))return !a->memory.fault; /*Inherited empty notifications. */
    if(method!=(enabled?0x47f340u:0x47f420u) || a->memory.fault)return 0;
    switch(kind){
    case 11:offset=0x4e4;break;case 0x21:offset=0x4d0;break;case 0x22:offset=0x4dc;break;
    case 0x24:offset=0x4e0;break;case 0x26:offset=0x4d8;break;case 0x27:offset=0x4d4;break;case 0x40:offset=0x4cc;break;
    default:break;
    }
    if(offset)put(a,owner+offset,get(a,owner+offset)+(enabled?1u:UINT32_MAX));
    return !a->memory.fault;
}
/*510eb0: a ready spatial sampler creates the actual kind11 payload, publishes
 *it in its station owner's list, then consumes readiness and production credit.
 *Its kind11 projectile and shared536250 placement already have native owners. */
int game_level_spatial_sample_produce(GameApplication *a,GameAddress sampler,uint32_t x,uint32_t z,uint32_t *reason){
    GameAddress descriptor=get(a,sampler+0xb4),owner,object,viewer;uint32_t id,next,bits;
    float p[3]={(float)x,0,(float)z},height,camera_x,camera_z;long double dx,dz;SoundPlaybackRequest playback={0};
    if(!reason || a->memory.fault)return 0;
    if(!(game_memory_read_u8(&a->memory,sampler+0xb8)&2) || !descriptor){*reason=1;return !a->memory.fault;}
    owner=get(a,get(a,descriptor+4)+0x54);
    bits=get(a,0x575498);memcpy(&height,&bits,4);
    p[1]=(float)(gameplay_geometry_terrain_height(&a->memory,p[0],p[2])+height);
    object=game_level_factory_take(a,11,0);if(!object)return 0;
    if(level_player_slots_owns_player(owner)){
        if(!level_player_slots_next_owner_sequence(&a->memory,owner,&id,&next))goto failure;
        put(a,owner+24,next);
    }else id=game_state_next_owner_sequence(&a->memory,owner);
    {const uint16_t angles[3]={0,0,0};
        if(!game_level_object_place_angles(a,object,id,p,angles) || !publish(a,owner,object))goto failure;
    }
    viewer=get(a,0x58f890);bits=get(a,viewer+12);memcpy(&camera_x,&bits,4);
    bits=get(a,viewer+20);memcpy(&camera_z,&bits,4);dx=(long double)p[0]-camera_x;dz=(long double)p[2]-camera_z;
    playback.sound=7;playback.distance_squared=(float)(dz*dz+dx*dx);playback.volume=1;
    if(!game_menu_sound_play(a,&playback))return 0;
    put(a,sampler+0xc4,0);
    if(!unit(a,get(a,0x58973c),11,0))return 0;
    game_memory_write_u8(&a->memory,sampler+0xb8,game_memory_read_u8(&a->memory,sampler+0xb8)&0xfdu);
    *reason=0;return !a->memory.fault;
failure:(void)game_level_factory_destroy(a,object);return 0;
}
static int spatial_reset_base(void *c,GameAddress object,int notify){GameApplication *a=c;PresentationArticulatedServices s=services(a);
    return presentation_instance_source_reset(&a->memory,object,notify,&s);
}
static SpatialSampleServices spatial_services(GameApplication *a){SpatialSampleServices s={services(a),visible,gather,unit,spatial_reset_base,catalog};return s;}
static int ring_visible(void *c,GameAddress object,int *out){uint32_t value;if(!visible(c,object,&value))return 0;*out=value!=0;return 1;}
static int ring_duration(void *c,GameAddress object,int32_t *out){uint32_t value;if(!health(c,object,&value))return 0;memcpy(out,&value,4);return 1;}
static int ring_create(void *c,GameAddress object,const float p[3],GameAddress *out){return effect_create(c,object,p,1200,800,out);}
static int ring_emit(void *c,GameAddress object,const float p[3]){return game_level_surface_particles(c,object,p,10,0x6297f0);}
static int ring_catalog_retire(void *c,GameAddress owner,uint8_t kind){GameApplication *a=c;uint32_t method=get(a,get(a,owner)+0x68);(void)kind;
    return (method==0x47f550 || method==0x4777c0) && !a->memory.fault;}
static int ring_phase(void *c,GameAddress object,uint32_t code){GameApplication *a=c;return game_level_station_payload_notice(a,object,get(a,get(a,object)+0x58),code);}
static int ring_token(void *c,GameAddress object,uint32_t *out){GameApplication *a=c;
    switch(get(a,get(a,object)+0x90)){
    case 0x51ba30:*out=0x40;break;case 0x51bb10:*out=0x21;break;
    case 0x51bba0:*out=0x27;break;case 0x51bc30:*out=0x26;break;default:return 0;
    }return !a->memory.fault;
}
static int ring_notify(void *c,GameAddress owner,uint32_t token,int remove){return unit(c,owner,(uint8_t)token,!remove);}
static GameplayEffectRingServices ring_services(GameApplication *a){
    GameplayEffectRingServices s={a,ring_visible,ring_duration,effect_retire,ring_create,ring_emit,ring_catalog_retire,ring_phase,detach_source,ring_token,ring_notify};return s;
}
extern int game_level_ground_controller_release(GameApplication *,GameAddress,uint8_t,int);
/*533900/458580: only the imperial producer releases its shared site controller.
 *The last site detaches worker heads before returning the controller's packets.
 *482910 then retires effects/slot flags and returns this same record to its pool. */
int game_level_ground_producer_reset(GameApplication *a,GameAddress object){
    GameAddress owner=get(a,0x5f0210+(get(a,object+20)&63u)*4u),controller=get(a,object+0xa8);
    GameplayEffectRingServices s=ring_services(a);
    if(a->memory.fault)return 0;
    if(owner==get(a,0x5f0224) && controller){
        if(!game_level_ground_controller_release(a,controller,game_memory_read_u8(&a->memory,object+12),1))return 0;
        put(a,object+0xa8,0);
    }
    return !a->memory.fault && gameplay_effect_ring_reset(&a->memory,&s,object,-1);
}
int game_level_station_payload_reset(GameApplication *a,GameAddress object,int notify){
    GameAddress table=get(a,object);PresentationArticulatedServices s=services(a);SpatialSampleServices spatial=spatial_services(a);
    if(a->memory.fault)return 0;
    if(table==0x572c78)return presentation_instance_articulated_reset(&a->memory,object,notify,&s);
    if(table==0x5753b0)return spatial_sample_reset(&a->memory,object,notify,&spatial);
    if(get(a,table+0x20)==0x533900)return game_level_ground_producer_reset(a,object);
    if(get(a,table+0x20)==0x4fa610){GameplayEffectRingServices ring=ring_services(a);
        /*4fa610 notifies death and detaches its descriptor;4fa650 only clears it. */
        return gameplay_effect_ring_reset(&a->memory,&ring,object,notify);
    }
    if(get(a,table+0x20)==0x482910){GameplayEffectRingServices ring=ring_services(a);
        return gameplay_effect_ring_reset(&a->memory,&ring,object,-1);
    }
    if(get(a,table+0x20)==0x51ad50){GameplayEffectRingServices ring=ring_services(a);
        return gameplay_descriptor_payload_retire(&a->memory,&ring,object,notify);
    }
    return 0;
}
int game_level_station_payload_update(GameApplication *a,GameAddress object,GameAddress method){
    PresentationArticulatedServices s=services(a);SpatialSampleServices spatial=spatial_services(a);
    if(a->memory.fault)return 0;
    switch(method){
    case 0x4366a0:return get(a,object)==0x572c78 && presentation_instance_articulated_update(&a->memory,object,&s);
    case 0x4369e0:return get(a,object)==0x572c78 && presentation_instance_articulated_advance(&a->memory,object,&s);
    case 0x50ed40:return get(a,object)==0x5753b0 && spatial_sample_advance(&a->memory,object,&spatial);
    case 0x50f120:return get(a,object)==0x5753b0 && spatial_sample_step(&a->memory,object,&spatial);
    case 0x482690:{GameplayEffectRingServices ring=ring_services(a);return gameplay_effect_ring_update(&a->memory,&ring,object);}
    case 0x51ad00:{GameplayEffectRingServices ring=ring_services(a);return gameplay_descriptor_payload_update(&a->memory,&ring,object);}
    default:return 0;
    }
}
