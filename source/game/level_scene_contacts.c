#include "level_run.h"
#include "level_player_resources.h"
#include "level_factory.h"
#include "scene_contacts.h"
#include "player_contact.h"
#include "effect_resource_pool.h"
#include "presentation_commands.h"
#include "entity_queries.h"
#include "presentation_instance.h"
#include "level_effect_runtime.h"
#include "level_player_scene.h"
#include "gameplay_state.h"
#include "menu_sound.h"
#include "portable_crt.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
extern int game_level_projectile_impact(GameApplication *,GameAddress,const float[3],GameAddress,int,GameAddress);

/*Native class programs have ordinary arguments. Unsupported selector guards
 *remain runtime reconstruction blockers; they never enter emulation. */
extern int game_level_scene_collision(GameApplication *,GameAddress,GameAddress,GameAddress,const float[3],float[3],uint32_t *);
extern int game_level_scene_killed(GameApplication *,GameAddress,GameAddress,GameAddress);
extern int game_level_vehicle_death_detach(GameApplication *,GameAddress,GameAddress,GameAddress);
extern int game_level_secondary_descriptor_retire(GameApplication *,GameAddress,unsigned);
extern int game_level_articulated_descriptor_retire(GameApplication *,GameAddress);
extern int game_level_infantry_notice(GameApplication *,GameAddress,uint32_t);
extern int game_level_vehicle_settle(GameApplication *,GameAddress);
extern int game_level_vehicle_reset(GameApplication *,GameAddress);
extern int game_level_vehicle_command(GameApplication *,GameAddress,GameAddress,uint32_t);
extern int game_level_scene_command_publish(GameApplication *,GameAddress,RuntimeNode *);
extern int game_level_scene_order_issue(GameApplication *,GameAddress,const PresentationCommand *,int);
extern int game_level_class27_reposition(GameApplication *,GameAddress);
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
static int intersect(void *c,GameAddress candidate,GameAddress o,GameAddress method,float contact[3],int *hit){
    GameApplication *a=c;uint32_t result;int ok;
    if(method!=0x42dfd0 && method!=0x482bb0 && method!=0x419e60){a->memory.fault=1;a->memory.fault_address=method;return 0;}
    ok=game_level_scene_collision(a,candidate,o,method,NULL,contact,&result);
    if(!ok)return 0;
    *hit=(int)result;return 1;
}
static GameAddress allocate(void *c){GameApplication *a=c;EffectResourcePool *pool=effect_resource_pool_find(0x631910);return pool?effect_resource_pool_acquire_kind(&a->memory,pool,2):0;}
int game_level_scene_contacts(GameApplication *a,GameAddress o,GameAddress method,const float previous[3],int *rollback){
    SceneContactQueryServices s={a,intersect,allocate};
    if(!a || a->memory.fault)return 0;
    if(method==0x543f90)return scene_contacts_class27_gather(&a->memory,o,&s,rollback);
    if(method!=0x534cd0)return 0;
    return scene_contacts_gather(&a->memory,o,previous,&s,rollback);
}
static int resistance(void *c,GameAddress o,GameAddress method,uint32_t *value){return game_level_scene_property(c,o,method,value);}
static int catalog(void *c,GameAddress owner,uint8_t kind,GameAddress *record){return game_level_player_catalog_record(c,owner,kind,record);}
static int impact(void *c,GameAddress o,GameAddress source,GameAddress method,uint32_t *value){
    GameApplication *a=c;(void)source;(void)value;
    /*Supported scene tables select42ec70, implemented in game_contact_damage.
     *A different method remains an explicit runtime reconstruction blocker. */
    fprintf(stderr,"Unreconstructed contact impact %08x object=%08x\n",(unsigned)method,(unsigned)o);
    a->memory.fault=1;a->memory.fault_address=method;return 0;
}
int game_level_contact_damage(GameApplication *a,GameAddress o,GameAddress other,int32_t *value){GameContactDamageServices s={a,resistance,catalog,impact};return game_contact_damage(&a->memory,o,other,&s,value);}
static int damage(void *c,GameAddress o,GameAddress other,int32_t *value){return game_level_contact_damage(c,o,other,value);}
static int killed(void *c,GameAddress o,GameAddress other,GameAddress method){return game_level_scene_killed(c,o,other,method);}
int game_level_scene_reset(GameApplication *c,GameAddress o,GameAddress method){
    if(!c || c->memory.fault)return 0;
    if(method==0x43d2e0)return game_level_fragment_reset(c,o);
    if(method==0x555120)return game_level_attachment_reset(c,o);
    if(method==0x4412f0)return game_level_arrival_reset(c,o);
    if(method==0x4e7540)return game_level_group_vehicle_reset(c,o);
    if(method==0x4f4f50)return !((GameApplication *)c)->memory.fault; /*Actual player RET. */
    if(method==0x5362d0)return game_level_presentation_reset(c,o,1);
    if(method==0x539c10)return game_level_presentation_reset(c,o,2);
    if(method==0x4fc9d0 || method==0x50d720)return game_level_scaled_effect_reset(c,o);
    if(method==0x533900)return game_level_ground_producer_reset(c,o);
    if(method==0x482910)return game_level_station_payload_reset(c,o,0);
    if(method==0x48e960)return game_level_mole_reset(c,o);
    if(method==0x42d8f0)return game_level_presentation_reset(c,o,0);
    if(method==0x471a70)return game_level_descent_update(c,o,method);
    if(method==0x40a060 || method==0x40a430)return game_level_presentation_reset(c,o,1);
    if(method==0x419c20)return game_level_spawn_reset(c,o);
    if(method==0x544410)return game_level_infantry_reset(c,o);
    if(method==0x55b370)return game_level_aircraft_reset(c,o);
    if(method==0x451070)return game_level_presentation_reset(c,o,2);
    if(method==0x40cc70)return game_level_class40_reset(c,o);
    if(method==0x451c10)return game_level_carrier_reset(c,o);
    if(method==0x4365e0 || method==0x50f040 || method==0x51ad50 || method==0x4fa610)return game_level_station_payload_reset(c,o,1);
    if(method==0x407e30)return game_level_vehicle_death_detach(c,o,0,method);
    if(method==0x4f69d0)return game_level_player_finalize(c,o);
    if(method==0x51ad90 || method==0x4fa650)return game_level_secondary_descriptor_retire(c,o,1);
    if(method==0x436640 || method==0x50f0b0)return game_level_articulated_descriptor_retire(c,o);
    if(method==0x551fb0)return game_level_vehicle_reset(c,o);
    fprintf(stderr,"Unreconstructed scene reset %08x object=%08x\n",(unsigned)method,(unsigned)o);
    c->memory.fault=1;c->memory.fault_address=method;return 0;
}
static int reset(void *c,GameAddress o,GameAddress method){return game_level_scene_reset(c,o,method);}
/*4e8c00: shared group-unit damage, arrival and death announcements. Preserve
 *the signed cooldowns, stored health-ratio comparison and original RNG stream. */
static int group_notice(GameApplication *a,GameAddress o,uint32_t code){
    static const uint16_t damaged[3]={0x69,0x67,0x68};
    static const uint16_t low[5]={0x6a,0x6b,0x6c,0x6e,0x6f};
    GameAddress local=get(a,0x5f0208);uint32_t word,tick,maximum,bits,index;int32_t elapsed,health,max_health,ratio_bits;
    uint16_t announcement;SoundPlaybackRequest sound={0};float ratio,threshold;long double extended;
    if(code!=0 && code!=2 && code!=3)return !a->memory.fault;
    tick=get(a,0x5ac6ac);word=tick-get(a,local+0x58c);memcpy(&elapsed,&word,4);
    if(elapsed<=(code==0?288:code==2?96:1) ||
       (get(a,o+20)&63u)!=game_memory_read_u16(&a->memory,local+8))return !a->memory.fault;
    if(code==0){
        if(!game_level_scene_property(a,o,get(a,get(a,o)+0x44),&maximum))return 0;
        word=get(a,o+0x58);memcpy(&health,&word,4);memcpy(&max_health,&maximum,4);
        extended=(long double)health/max_health;ratio=(float)extended;memcpy(&bits,&ratio,4);memcpy(&ratio_bits,&bits,4);
        word=get(a,0x574d04);memcpy(&threshold,&word,4);
        if(isnan(extended) || isnan(threshold) || extended<threshold)announcement=0x70;
        else if(ratio_bits>=0x3ecccccd){
            uint64_t double_bits=(uint64_t)get(a,0x574d10)|((uint64_t)get(a,0x574d14)<<32);double upper;
            memcpy(&upper,&double_bits,8);
            if(!isnan(ratio) && !isnan(upper) && (long double)ratio>=upper)announcement=0x66;
            else{index=portable_crt_rand_step(&a->memory)*3u/32768u;announcement=damaged[index];}
        }else{index=portable_crt_rand_step(&a->memory)*5u/32768u;announcement=low[index];}
    }else{
        put(a,local+0x58c,tick);index=portable_crt_rand_step(&a->memory)*(code==2?5u:8u)/32768u;
        announcement=(uint16_t)((code==2?0x61:0x71)+index);
    }
    local=get(a,0x5f0208);game_memory_write_u16(&a->memory,local+0x598,announcement);
    sound.sound=announcement;sound.volume=1;
    if(a->memory.fault || !game_menu_sound_play(a,&sound))return 0;
    if(code==0){tick=get(a,0x5ac6ac);put(a,get(a,0x5f0208)+0x58c,tick);}
    return !a->memory.fault;
}
int game_level_scene_notice(GameApplication *c,GameAddress o,GameAddress method,uint32_t code){
    if(!c || c->memory.fault)return 0;
    if(method==0x42ec20 || method==0x407ed0 || method==0x451780)return 1; /*Actual shared/class22/class29 RET4. */
    if(method==0x4e8c00)return group_notice(c,o,code);
    if(method==0x40e680)return game_level_class40_notice(c,o,code);
    if(method==0x546630)return game_level_infantry_notice(c,o,code);
    if(method==0x4fabf0 || method==0x4394f0 || method==0x511000 || method==0x51b000 || method==0x51b290 || method==0x51b520 || method==0x51b840)
        return game_level_station_payload_notice(c,o,method,code);
    return method==0x553750 && game_level_vehicle_command(c,o,method,code);
}
static int notice(void *c,GameAddress o,GameAddress method,uint32_t code){return game_level_scene_notice(c,o,method,code);}
static int settle(void *c,GameAddress o,GameAddress method){
    GameApplication *a=c;if(method==0x53c000)return game_level_vehicle_settle(a,o);
    fprintf(stderr,"Unreconstructed scene settle %08x object=%08x\n",(unsigned)method,(unsigned)o);
    a->memory.fault=1;a->memory.fault_address=method;return 0;
}
static int credit(void *c,GameAddress o,GameAddress attacker,GameAddress method,int32_t amount){
    GameApplication *a=c;NativeEntityRelationshipResult result;
    if(method==0x4ca320)return game_level_group_reaction(a,o,attacker,amount);
    if(method==0x451000){
        GameAddress local=get(a,0x5f0208);uint32_t tick=get(a,0x5ac6ac),word=tick-get(a,local+0x58c);int32_t elapsed;
        if(!native_entity_relationship_query(&a->memory,attacker,o,&result))return 0;
        memcpy(&elapsed,&word,4);
        if(result.decision>=NATIVE_ENTITY_RELATION_ACCEPT_NULL_SECOND && elapsed>96 &&
           (get(a,o+20)&63)==game_memory_read_u16(&a->memory,local+8)){
            put(a,local+0x58c,tick);
            return notice(a,o,get(a,get(a,o)+0x58),0);
        }
        return !a->memory.fault;
    }
    if(method==0x55c100){
        GameAddress local=get(a,0x5f0208);uint32_t tick=get(a,0x5ac6ac),elapsed=tick-get(a,0x5a09a0),identity=get(a,o+20)&63;
        int32_t signed_elapsed;SoundPlaybackRequest sound={0};(void)amount;memcpy(&signed_elapsed,&elapsed,4);
        if(signed_elapsed<=96 || identity!=game_memory_read_u16(&a->memory,local+8))return !a->memory.fault;
        /*The retail relation lookup uses attacker row and aircraft column. */
        if(get(a,0x5f0160+(get(a,attacker+20)&63)*24+identity*4))return !a->memory.fault;
        put(a,0x5a09a0,tick);put(a,local+0x58c,tick);game_memory_write_u16(&a->memory,local+0x598,52);
        sound.sound=52;sound.volume=1;return !a->memory.fault && game_menu_sound_play(a,&sound);
    }
    if(method==0x543ee0){
        GameAddress local,target;uint32_t tick;
        if(get(a,get(a,o+0xdc)+12)==11)return !a->memory.fault;
        if(!native_entity_relationship_query(&a->memory,attacker,o,&result))return 0;
        if(result.decision>=NATIVE_ENTITY_RELATION_ACCEPT_NULL_SECOND){
            local=get(a,0x5f0208);tick=get(a,0x5ac6ac);
            {uint32_t elapsed=tick-get(a,local+0x58c);int32_t signed_elapsed;memcpy(&signed_elapsed,&elapsed,4);
                if(signed_elapsed>96 && (get(a,o+20)&63)==game_memory_read_u16(&a->memory,local+8)){
                    put(a,local+0x58c,tick);return notice(a,o,get(a,get(a,o)+0x58),0);
                }}
        }else{
            target=get(a,o+0xf0);
            if(target && get(a,target+0x58)){put(a,o+0x164,0);put(a,o+0x168,0);
                return get(a,get(a,o)+0x1c8)==0x544970 && game_level_class27_reposition(a,o);}
        }
        return !a->memory.fault;
    }
    if(method!=0x407490)return 0; /*Unknown unit reaction selector. */
    if(!native_entity_relationship_query(&a->memory,attacker,o,&result))return 0;
    if(result.decision>=NATIVE_ENTITY_RELATION_ACCEPT_NULL_SECOND){put(a,o+0xc8,attacker);put(a,o+0x12c,0);put(a,o+0xcc,get(a,attacker+20));}
    return !a->memory.fault;
}
static int avoid(void *c,GameAddress o,GameAddress other,GameAddress method,const float position[3]){
    GameApplication *a=c;PresentationLifecycle *state;RuntimeNode *node;PresentationCommand command={0};
    if(method!=0x53c210)return 0; /*Supported unit tables share this native writer. */
    state=presentation_instance_lifecycle(o);node=state?runtime_node_pool_find(state->runtime.packet_chain):NULL;
    if(!node || node->in_pool)return 0;
    command.kind=12;command.target=other;command.target_identity=get(a,other+20);command.has_target=command.has_position=1;
    memcpy(command.position,position,12);
    if(a->memory.fault || !presentation_command_build(&a->memory,node,&command,&node))return 0;
    state->runtime.packet_chain=node->projection;put(a,o+0xdc,node->projection);
    return game_level_scene_command_publish(a,o,node);
}
static int contact_killed(void *c,GameAddress object,GameAddress other){GameApplication *a=c;
    return game_level_scene_killed(a,object,other,get(a,get(a,object)+0x10));
}
static int feedback_clear(void *c,GameAddress bank){return game_menu_sound_queue_clear(c,bank);}
static int feedback_submit(void *c,GameAddress bank,const int16_t sounds[8],int *accepted){return game_menu_sound_queue_submit(c,bank,sounds,accepted);}
int game_level_contact_response(GameApplication *a,GameAddress o,GameAddress other,GameAddress method,const uint32_t velocity[3],const float position[3]){
    SceneContactResponseServices s={a,settle,avoid,damage,killed,reset,notice,credit};
    if(!a || a->memory.fault)return 0;
    if(method==0x550a70 || method==0x45e800 || method==0x45fc80 || method==0x460030)return 1; /*Actual RET20 methods. */
    if(method==0x4347e0)return (int32_t)get(a,o+0x58)<=0?!a->memory.fault:game_level_position_shot_detonate(a,o);
    if(method==0x43d300){
        uint32_t word=get(a,o+0x58);int32_t health;memcpy(&health,&word,4);
        return health<=0?!a->memory.fault:game_level_fragment_reset(a,o);
    }
    /*40ea20: the sprite unit is crushed by a fast/steering player. Contact
     *with its own active target consumes one existing profile charge. */
    if(method==0x40ea20){
        if(game_memory_read_u8(&a->memory,other+12)==0x20){
            uint32_t bx=get(a,other+0x1ac),bz=get(a,other+0x1b4),threshold=get(a,0x571f00);
            float x,z,limit;long double square;int16_t steering;uint16_t word;
            memcpy(&x,&bx,4);memcpy(&z,&bz,4);memcpy(&limit,&threshold,4);
            square=(long double)z*z+(long double)x*x;
            word=game_memory_read_u16(&a->memory,other+0x1b8);memcpy(&steering,&word,2);
            if(!(square>limit) && steering<=10)return scene_contact_response(&a->memory,o,other,position,&s);
            {GameAddress table=get(a,o);
                return game_level_scene_killed(a,o,other,get(a,table+0x10)) && reset(a,o,get(a,table+0x20));}
        }
        if(get(a,o+0xf0)==other && game_memory_read_u16(&a->memory,o+0x146) && (get(a,other+0x10)&0x8000u)){
            if(get(a,get(a,o)+0x17c)!=0x40cc40 || !game_level_class40_profile(a,o,0x40cc40))return 0;
            game_memory_write_u16(&a->memory,o+0x146,(uint16_t)(game_memory_read_u16(&a->memory,o+0x146)-1u));
            return !a->memory.fault;
        }
        return scene_contact_response(&a->memory,o,other,position,&s);
    }
    if(method==0x4718f0)return game_level_descent_update(a,o,method);
    if(method==0x451080){
        uint32_t word=get(a,o+0x58);int32_t health;int active;uint8_t kind;
        RuntimeNode *node;PresentationCommand command={0};unsigned i;
        memcpy(&health,&word,4);if(health<=0)return !a->memory.fault;
        if(!native_entity_type_activity(&a->memory,other,&active))return 0;
        kind=game_memory_read_u8(&a->memory,other+12);
        if(active || (get(a,other+16)&4) || kind==0x41 || kind==11 || kind==10)
            if(!scene_contact_response(&a->memory,o,other,position,&s))return 0;
        node=runtime_node_pool_find(get(a,o+0xdc));
        if(!node || node->kind==9)return !a->memory.fault;
        command.priority=node->sequence;command.kind=9;command.has_position=1;command.parameter=96;
        for(i=0;i<3;i++){word=get(a,o+24+i*4);memcpy(command.position+i,&word,4);}
        return game_level_scene_order_issue(a,o,&command,0);
    }
    if(method==0x5445d0){
        uint32_t node,kind,other_kind,flags;GameAddress target;int active;unsigned i;
        int32_t health;uint32_t word=get(a,o+0x58);memcpy(&health,&word,4);
        if(health<=0)return !a->memory.fault;
        if(!native_entity_type_activity(&a->memory,other,&active))return 0;
        flags=get(a,other+16);other_kind=game_memory_read_u8(&a->memory,other+12);
        node=get(a,o+0xdc);kind=get(a,node+12);
        if(!active && !(flags&4) && other_kind!=0x41 && other_kind!=11 && other_kind!=10){
            if(kind==11)return !a->memory.fault;
            target=get(a,node+16);
            if(kind!=12 && kind>=5 && kind<=7 && target && get(a,target+20)==get(a,node+20)){
                put(a,o+0x164,0);put(a,o+0x168,0);
                return get(a,get(a,o)+0x1c8)==0x544970 && game_level_class27_reposition(a,o);
            }
            if(kind!=12){PresentationCommand command={0};command.kind=12;command.has_position=1;
                for(i=0;i<3;i++){uint32_t bits=get(a,o+24+i*4);memcpy(command.position+i,&bits,4);}
                return game_level_scene_order_issue(a,o,&command,0);
            }
            return !a->memory.fault;
        }
        if(kind==11 && other_kind==10)
            return game_level_scene_killed(a,o,other,get(a,get(a,o)+16)) && reset(a,o,get(a,get(a,o)+0x20));
        return scene_contact_response(&a->memory,o,other,position,&s);
    }
    if(method==0x419d90)return game_level_spawn_contact(a,o,other,velocity,position);
    if(method==0x40a070 || method==0x40a440){
        uint32_t owner,attacker;
        if((int32_t)get(a,o+0x58)<=0)return !a->memory.fault;
        owner=get(a,o+20)&63u;attacker=get(a,other+20)&63u;
        if(a->memory.fault)return 0;
        if(get(a,0x5f0160+owner*24+attacker*4))return !a->memory.fault;
        return (method==0x40a440 || game_level_physical_impact(a,o,position)) && game_level_presentation_reset(a,o,1);
    }
    if(method==0x433f30){
        uint8_t attacker=game_memory_read_u8(&a->memory,other+12);uint32_t subtype=get(a,o+0x64);
        if((int32_t)get(a,o+0x58)<=0)return !a->memory.fault;
        if(attacker==0x27 || (attacker==0x20 && subtype!=2 && subtype!=3 && subtype!=4 && subtype!=12 && subtype!=13 && subtype!=14)){
            put(a,o+0x58,0);
            return game_level_scene_killed(a,o,other,get(a,get(a,o)+16)) && reset(a,o,get(a,get(a,o)+0x20));
        }
        method=0x42da70;
    }
    if(method==0x42da70){
        GameplayEntityContactServices generic={a,damage,contact_killed,reset,{a,feedback_clear,feedback_submit}};
        return gameplay_entity_contact(&a->memory,o,other,&generic);
    }
    if(method==0x53a130)return scene_contact_response(&a->memory,o,other,position,&s); /*Original ignores incoming velocity. */
    if(method==0x555ae0)return game_level_attachment_contact(a,o,other);
    if(method==0x48e700)return game_level_mole_contact(a,o,other,position);
    if(method==0x4fc900 || method==0x50d5a0)return game_level_scaled_effect_contact(a,o,other,position);
    if(method==0x4f4550){float incoming[3];memcpy(incoming,velocity,12);return game_level_player_contact(a,o,other,incoming,position);}
    /*432d50 emits the shared impact but deliberately keeps the projectile
     *alive. Its enclosing432e50 contact loop decides subsequent retirement. */
    if(method==0x432d50){
        PresentationPose *pose=presentation_instance_pose(o);int32_t health;
        if(!pose)return 0;
        memcpy(&health,&pose->properties[0],4);
        if(health<=0)return !a->memory.fault;
        return get(a,get(a,o)+0xb0)==0x42fa00 && game_level_projectile_impact(a,o,position,other,0,0x42fa00);
    }
    if(method==0x42f8d0 || method==0x433710 || method==0x548700 || method==0x548010){
        PresentationPose *pose=presentation_instance_pose(o);uint32_t reset=get(a,get(a,o)+0x20u);
        if(!pose)return 0;
        if((int32_t)pose->properties[0]<=0)return !a->memory.fault;
        if((method==0x548700 && game_memory_read_u8(&a->memory,other+12)==0x11) ||
           (method==0x548010 && game_memory_read_u8(&a->memory,other+12)==0x12))return !a->memory.fault;
        if(!game_level_projectile_impact(a,o,position,other,0,method==0x548010?0x5480b0:get(a,get(a,o)+0xb0u)))return 0;
        if(method==0x433710 && !game_level_fragment_projectile_split(a,o))return 0;
        return reset==0x5362d0u && game_level_presentation_reset(a,o,1);
    }
    fprintf(stderr,"Unreconstructed contact response %08x object=%08x\n",(unsigned)method,(unsigned)o);
    a->memory.fault=1;a->memory.fault_address=method;return 0;
}
