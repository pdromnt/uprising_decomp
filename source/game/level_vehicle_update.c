#include "level_run.h"
#include "level_routes.h"
#include "level_objects.h"
#include "level_player_scene.h"
#include "level_factory.h"
#include "level_player_resources.h"
#include "level_player_slots.h"
#include "vehicle_presentation.h"
#include "game_intrusive_list.h"
#include "event_queue.h"
#include "effect_resource_pool.h"
#include "gameplay_geometry.h"
#include "gameplay_catalog.h"
#include "menu_sound.h"
#include "level_effect_runtime.h"
#include "runtime_node_pool.h"
#include "presentation_commands.h"
#include "effect_initialization.h"
#include "vehicle_targeting.h"
#include "gameplay_spatial.h"
#include "game_skin_transform.h"
#include "presentation_instance.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*Distinct native programs take actual C arguments. Unsupported selectors
 *fail explicitly and remain reconstruction blockers, never CPU fallback. */
static int class27_control(GameApplication *,GameAddress,GameAddress);
extern int game_level_class27_combat(GameApplication *,GameAddress);
extern int game_level_aircraft_flight(GameApplication *,GameAddress);
extern int game_level_aircraft_follow(GameApplication *,GameAddress);
extern int game_level_aircraft_combat(GameApplication *,GameAddress);
extern int game_level_class27_timed_refresh(GameApplication *,GameAddress);
extern int game_level_class27_reposition(GameApplication *,GameAddress);
extern int game_level_class27_follow_move(GameApplication *,GameAddress);
extern int game_level_class27_idle(GameApplication *,GameAddress);
extern int game_level_class27_ascent(GameApplication *,GameAddress);
extern int game_level_class27_ascent_step(GameApplication *,GameAddress);
extern int game_level_class27_react(GameApplication *,GameAddress);
extern int game_level_class27_escape(GameApplication *,GameAddress);
extern int game_level_group_member_reset(GameApplication *,GameAddress);
int game_level_vehicle_motion_update(GameApplication *,GameAddress);
int game_level_vehicle_command(GameApplication *,GameAddress,GameAddress,uint32_t);
int game_input_queue_remove(GameApplication *,GameAddress,GameAddress,GameAddress);
int game_level_vehicle_control(GameApplication *,GameAddress,GameAddress);
int game_level_vehicle_settle(GameApplication *,GameAddress);
int game_level_vehicle_targeting_frame(GameApplication *,GameAddress);
extern int game_level_scene_command_publish(GameApplication *,GameAddress,RuntimeNode *);
extern int game_level_scene_contacts(GameApplication *,GameAddress,GameAddress,const float[3],int *); /*534cd0*/
extern int game_level_contact_response(GameApplication *,GameAddress,GameAddress,GameAddress,const uint32_t[3],const float[3]); /*53a130/derived response*/
extern int game_level_particle_emit(GameApplication *,GameAddress,const float[3],const float[3],float,GameAddress,uint16_t,uint32_t,uint32_t); /*42e7d0*/
extern int game_level_surface_particles(GameApplication *,GameAddress,const float[3],int32_t,GameAddress); /*42e670*/
extern int game_level_expansion_start(GameApplication *,GameAddress,uint32_t,const float[3],float,float,int32_t); /*49aa10: owned effect/pose/model*/
extern int game_level_expansion_move(GameApplication *,GameAddress,const float[3]); /*spatial detach, pose refresh and insertion*/
extern int game_level_attachment_transform(GameApplication *,GameAddress,GameAddress,const float[12]);
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static int32_t signed_word(uint32_t v){int32_t s;memcpy(&s,&v,4);return s;}
static uint32_t truncate_word(long double v){return (uint32_t)(uint64_t)(!isfinite(v) || v< -0x1p63L || v>=0x1p63L?INT64_MIN:(int64_t)v);}
static int source_property(void *,GameAddress,unsigned,uint32_t *);
int game_level_scene_property(GameApplication *a,GameAddress o,GameAddress method,uint32_t *out){
    GameAddress catalog,record;uint32_t alternate,stage=0,maximum;unsigned index;
    if(!a || !out || a->memory.fault)return 0;
    if(method==0x499580 || method==0x4995a0)
        return gameplay_source_property(&a->memory,o,method==0x499580?1u:2u,a,source_property,out);
    /*4fa6f0/4fa710: descriptor disabling changes the SAME small payload's
     *armor/sprite properties. Its flag and catalog remain authoritative. */
    if(method==0x4fa6f0 || method==0x4fa710){
        if(method==0x4fa710){uint32_t identity=get(a,o+20);
            if(get(a,0x594d3c) && identity && get(a,0x5f0210+(identity&63u)*4)!=get(a,0x58973c)){
                *out=0;return !a->memory.fault;
            }
        }
        if(game_memory_read_u8(&a->memory,o+0xa4)&1u){*out=0;return !a->memory.fault;}
        return game_level_scene_property(a,o,method==0x4fa6f0?0x42ebb0:0x483350,out);
    }
    if(method==0x43cda0 || method==0x43cdc0 || method==0x43cde0)
        return gameplay_catalog_object_property(&a->memory,o,(method-0x43cda0u)/32u,out)==1;
    if(method==0x433e20 || method==0x433e40 || method==0x433e60)
        return gameplay_catalog_object_property(&a->memory,o,(method-0x433e20u)/32u,out)==1;
    if(method==0x53c550){*out=(game_memory_read_u8(&a->memory,o+0xd8)&4)!=0;return !a->memory.fault;}
    /*483350: damaged scenery uses sprite11 after crossing half health.
     *Its ordinary fallback and visibility rules are the shared42ebe0 path. */
    if(method==0x483350){
        uint32_t identity=get(a,o+0x14),health=get(a,o+0x58);
        catalog=get(a,0x5f0210+(identity&63u)*4);
        if(get(a,0x594d3c) && identity && catalog!=get(a,0x58973c)){*out=0;return !a->memory.fault;}
        if(identity && health){
            if(!game_level_scene_property(a,o,get(a,get(a,o)+0x44),&maximum))return 0;
            if(signed_word(maximum)>=signed_word(health*2u)){*out=11;return !a->memory.fault;}
        }
        return game_level_scene_property(a,o,0x42ebe0,out);
    }
    if(method==0x42eb80 || method==0x42ebb0){
        catalog=get(a,0x5f0210+(get(a,o+0x14)&63u)*4);
        if(!game_level_player_catalog_record(a,catalog,game_memory_read_u8(&a->memory,o+12),&record))return 0;
        *out=record?get(a,record+(method==0x42eb80?0:4)):(method==0x42eb80?25:0);
        return !a->memory.fault;
    }
    catalog=get(a,0x5f0210+(get(a,o+0x14)&63u)*4);
    if(method==0x42ebe0){
        *out=0;
        if(get(a,0x594d3c) && catalog!=get(a,0x58973c))return !a->memory.fault;
        return game_level_player_catalog_value(a,catalog,game_memory_read_u8(&a->memory,o+12),2,0,out);
    }
    if(get(a,get(a,o)+0x60)!=0x53c550)return 0;
    alternate=(game_memory_read_u8(&a->memory,o+0xd8)&4)!=0;
    if(method==0x5545c0)return game_level_player_catalog_value(a,catalog,0x23,0,alternate,out);
    if(method==0x554560 || method==0x5545f0 || method==0x554630)stage=get(a,o+0x2b4);
    if(method==0x53c560 || method==0x53c5a0){
        if(!game_level_player_catalog_record(a,catalog,game_memory_read_u8(&a->memory,o+12),&record))return 0;
        index=method==0x53c5a0?4:0;
        *out=record?get(a,record+index+(alternate?84:0)):(index?0:25);return !a->memory.fault;
    }
    if(method==0x554560){
        uint32_t bits=get(a,0x577920);float factor;memcpy(&factor,&bits,4);
        if(!game_level_player_catalog_value(a,catalog,0x23,0,alternate,out))return 0;
        if(stage!=30)*out=truncate_word((long double)signed_word((*out-100u)*stage)*factor)+100u;
        return !a->memory.fault;
    }
    if(method==0x5545f0){
        if(stage!=30){*out=0;return !a->memory.fault;}
        return game_level_player_catalog_value(a,catalog,0x23,1,alternate,out);
    }
    if(method!=0x554630 && method!=0x53c5e0)return 0;
    *out=0;
    if(get(a,0x594d3c) && catalog!=get(a,0x58973c) && (method==0x53c5e0 || get(a,o+0x14)))return !a->memory.fault;
    if(method==0x554630){
        if(stage!=30)return !a->memory.fault;
        if(get(a,o+0x14) && get(a,o+0x58)){
            if(!game_level_scene_property(a,o,get(a,get(a,o)+0x44),&maximum))return 0;
            if(signed_word(maximum)>=signed_word(get(a,o+0x58)*2u)){*out=12;return !a->memory.fault;}
        }
        /*Its ordinary base fallback has the stricter catalog visibility gate. */
        return game_level_scene_property(a,o,0x53c5e0,out);
    }
    if(!game_level_player_catalog_record(a,catalog,game_memory_read_u8(&a->memory,o+12),&record))return 0;
    index=alternate?92:8;*out=record?get(a,record+index):0;return !a->memory.fault;
}
static int source_property(void *c,GameAddress o,unsigned index,uint32_t *out){GameApplication *a=c;
    return game_level_scene_property(a,o,index==0?get(a,get(a,o)+0x44):index==1?0x42ebb0:0x42ebe0,out);
}
static int property(void *c,GameAddress o,GameAddress method,uint32_t *out){return game_level_scene_property(c,o,method,out);}
static int particles(void *c,GameAddress o,const float p[3],const float v[3],float radius,GameAddress recipe,uint16_t angle,uint32_t extra,uint32_t count){return game_level_particle_emit(c,o,p,v,radius,recipe,angle,extra,count);}
static int surface(void *c,GameAddress o,const float p[3],int32_t chance,GameAddress recipe){return game_level_surface_particles(c,o,p,chance,recipe);}
static GameAddress take(void *c){return game_level_factory_take(c,0x44,0);}
static int start(void *c,GameAddress o,uint32_t identity,const float p[3],float width,float height,int32_t duration){return game_level_expansion_start(c,o,identity,p,width,height,duration);}
static int stop(void *c,GameAddress o){ExpansionEffect *effect=presentation_instance_expansion(o);(void)c;if(!effect)return 0;expansion_effect_shrink(effect);return 1;}
static int publish(void *c,GameAddress root,GameAddress o){
    GameApplication *a=c;uint32_t flags=get(a,o+16),slot=(flags&1)?((flags&64)?0x80:0x1c):0xe4;
    if(a->memory.fault || !level_player_slots_attach_scene_member(root,o,slot))return 0;
    game_list_insert_after(&a->memory,o,root+slot,0x48,0x4c);
    if(a->memory.fault){level_player_slots_forget_scene_member(o);return 0;}
    return 1;
}
static int identity(void *c,GameAddress root,uint32_t *out){
    GameApplication *a=c;uint32_t n=get(a,root+0x18);game_memory_write_u32(&a->memory,root+0x18,n+1);
    *out=(n<<6)|game_memory_read_u16(&a->memory,root+8);return !a->memory.fault;
}
static int remove_event(void *c,GameMemory *m,GameAddress queue,GameAddress node){return game_input_queue_remove(c,queue,node,game_memory_read_u32(m,game_memory_read_u32(m,queue)+0x18));}
static int release_event(void *c,GameAddress node){
    GameApplication *a=c;EffectResource *record=effect_resource_find(node);
    /* Mouse angles and buttons use different pools. Return the detached event
     * to its actual owner, as the player's timed queue already does. */
    int ok=record && record->owner->object_kind>=1 && record->owner->object_kind<=3 &&
        record->event_route==record->owner->object_kind-1 &&
        effect_resource_pool_return(&a->memory,record->owner,node);
    if(!ok)fprintf(stderr,"native vehicle event failed: release node=%08x kind=%u route=%u\n",
        node,record?record->owner->object_kind:UINT16_MAX,record?record->event_route:255);
    return ok;
}
static int expire(void *c,GameAddress queue){GameApplication *a=c;GameEventQueueCallbacks ops={a,remove_event,release_event};return game_timed_event_queue_expire(&a->memory,&ops,queue);}
static int command(void *c,GameAddress o,GameAddress method,uint32_t code){return game_level_vehicle_command(c,o,method,code);}
static int refresh(void *c,GameAddress o,GameAddress method){return (method==0x407060 || method==0x4035f0) && game_level_vehicle_motion_update(c,o);}
static int move(void *c,GameAddress o,const float p[3]){return game_level_expansion_move(c,o,p);}
static int attachment(void *c,GameAddress o,GameAddress method,const float matrix[12]){return game_level_attachment_transform(c,o,method,matrix);}
int game_level_scene_update(GameApplication *a,GameAddress o,GameAddress method){
    VehicleUpdateServices ops={a,property,particles,surface,take,start,stop,publish,identity,expire,command,refresh,move,attachment};
    if(!a || a->memory.fault)return 0;
    if(method==0x42df90 || method==0x42dfa0)return 1; /*Actual base RET methods;42dfa0 also zeroes AX. */
    if(method==0x45e570)return game_level_gate_update(a,o);
    if(method==0x45fe90)return game_level_teleporter_update(a,o);
    if(method==0x535250)return 1; /*Actual extended-effect RET. */
    if(method==0x4a65f0)return 1; /*Scenery's actual empty motion override. */
    if(method==0x4a6610)return game_level_scene_object_world(a,o);
    if(method==0x482390)return game_presentation_transform(&a->memory,o);
    if(method==0x4347f0){
        uint32_t fuse;
        if(signed_word(get(a,o+0x58))<=0)return !a->memory.fault;
        fuse=get(a,get(a,o+0x64)+0x24);if(!fuse)return !a->memory.fault;
        return signed_word(get(a,o+0x68)+fuse)>signed_word(get(a,0x5ac6ac))?
            !a->memory.fault:game_level_position_shot_detonate(a,o);
    }
    if(method==0x4718f0 || method==0x4719a0 || method==0x471a70)return game_level_descent_update(a,o,method);
    if(method==0x548650)return get(a,get(a,o)+0x88)==0x42f300 && game_level_projectile_update(a,o,0x42f300);
    if(method==0x48e7a0)return game_level_mole_update(a,o);
    if(method==0x547dd0)return game_level_motion_seed_update(a,o);
    if(method==0x459710)return 1; /*ClassFE motion override is an actual RET. */
    if(method==0x4be060)return 1; /*Constructed imperial motion override is a retail RET. */
    if(method==0x459770){extern int game_level_resource_cluster_update(GameApplication *,GameAddress);return game_level_resource_cluster_update(a,o);}
    if(method==0x4ca0f0){
        GameAddress target=get(a,o+0xc8),motion;
        if(target && get(a,target+20)!=get(a,o+0xcc))game_memory_write_u32(&a->memory,o+0xc8,0);
        target=get(a,o+0xf0);
        if(target && get(a,target+20)!=get(a,o+0xf4))game_memory_write_u32(&a->memory,o+0xf0,0);
        motion=get(a,get(a,o)+0x88);
        if(motion==method || a->memory.fault)return 0;
        return game_level_scene_update(a,o,motion);
    }
    if(method==0x4035f0 || method==0x407060 || method==0x53b3e0)return game_level_vehicle_motion_update(a,o);
    if(method==0x53a3e0 || method==0x4e8510 || method==0x4062b0 || method==0x405930)
        return game_level_vehicle_control(a,o,method);
    if(method==0x55ae60 || method==0x55af90)return game_level_aircraft_release(a,o,method);
    if(method==0x536790)
        return game_level_presentation_flight(a,o);
    if(method==0x40a3d0){
        uint32_t expiry=get(a,o+0xd4)+96u,tick=get(a,0x5ac6ac);
        float position[3];unsigned i;
        if(get(a,get(a,o)+0x20)!=0x40a430 || get(a,get(a,o)+0x88)!=0x536790 || a->memory.fault)return 0;
        if(signed_word(expiry)<=signed_word(tick))return game_level_presentation_reset(a,o,1);
        if((tick&3u)==3u){
            for(i=0;i<3;i++){uint32_t bits=get(a,o+0x18+i*4);memcpy(position+i,&bits,4);}
            if(!game_level_surface_particles(a,o,position,0,0x629750))return 0;
        }
        return game_level_presentation_flight(a,o);
    }
    if(method==0x419c50)return game_level_spawn_update(a,o);
    if(method==0x4f13c0)return game_level_player_frame(a,o);
    if(method==0x451c40)return game_level_carrier_update(a,o);
    if(method==0x533730 || method==0x533cf0 || method==0x5345a0 || method==0x5347a0 || method==0x533f40)
        return game_level_ground_producer_update(a,o,method);
    if(method==0x482690 || method==0x51ad00)return game_level_station_payload_update(a,o,method);
    if(get(a,o)==0x572c78 || get(a,o)==0x5753b0){
        if(method==0x4366a0 || method==0x4369e0 || method==0x50ed40 || method==0x50f120)
            return game_level_station_payload_update(a,o,method);
        if(method==0x436c90 || method==0x437020 || method==0x437320 || method==0x437ab0 ||
           method==0x50f370 || method==0x50f620 || method==0x50f890 || method==0x50fbf0)
            return game_level_station_payload_refresh(a,o,method);
    }
    if(method==0x536240 && game_memory_read_u8(&a->memory,o+12)==0x2a)return game_level_carrier_update(a,o);
    if(method==0x53e010 || method==0x53ecd0 || method==0x53f790)return game_level_group_member_reset(a,o);
    if(method==0x5527f0)return vehicle_presentation_update(&a->memory,o,&ops);
    if(method==0x536240){
        GameAddress motion=get(a,get(a,o)+0x88);
        int ok;
        if(motion==method || a->memory.fault)return 0;
        ok=game_level_scene_update(a,o,motion);
        if(!ok)fprintf(stderr,"native scene motion failed: kind=%02x method=%08x object=%08x\n",game_memory_read_u8(&a->memory,o+12),motion,o);
        return ok;
    }
    if(method==0x43d530)return game_level_fragment_update(a,o);
    if(method==0x43d820)return game_level_fragment_motion(a,o);
    if(method==0x4d9810)return game_level_trail_update(a,o);
    if(method==0x537dc0)return game_level_particle_update(a,o);
    if(method==0x49a7f0)return game_level_expansion_update(a,o);
    if(method==0x550b90)return game_level_animation_update(a,o);
    if(method==0x441110)return game_level_arrival_update(a,o);
    if(method==0x555200)return game_level_attachment_update(a,o);
    if(method==0x555850)return game_level_attachment_motion(a,o);
    if(method==0x430820 || method==0x433650 || method==0x4fc460 || method==0x4fc510 || method==0x50d050 || method==0x50d100 || method==0x42f300 || method==0x4318f0 || method==0x431d40 || method==0x432190 || method==0x4329b0 || method==0x432e50 || method==0x4326b0)
        return game_level_projectile_update(a,o,method);
    fprintf(stderr,"Unreconstructed scene update %08x object=%08x\n",(unsigned)method,(unsigned)o);
    a->memory.fault=1;a->memory.fault_address=method;return 0;
}
static int feedback_clear(void *c,GameAddress bank){return game_menu_sound_queue_clear(c,bank);}
static int feedback_submit(void *c,GameAddress bank,const int16_t sounds[8],int *accepted){return game_menu_sound_queue_submit(c,bank,sounds,accepted);}
int game_level_vehicle_command(GameApplication *a,GameAddress o,GameAddress method,uint32_t code){
    GamePresentationFeedbackServices s={a,feedback_clear,feedback_submit};
    if(!a || a->memory.fault)return 0;
    /*539da0: a capture/stop notification retires the entire command chain
     *only when its byte priority reaches the current signed priority. The
     *same node pool owns the replacement idle command and its publication. */
    if(method==0x539da0){
        RuntimeNode *node=runtime_node_pool_find(get(a,o+0xdc));
        PresentationLifecycle *state=presentation_instance_lifecycle(o);
        PresentationCommand idle={0};
        if(!node || !node->live || node->in_pool || !state || a->memory.fault)return 0;
        if((int32_t)(uint8_t)code<signed_word(node->sequence))return 1;
        if(!presentation_command_replace(&a->memory,state,&idle))return 0;
        game_memory_write_u32(&a->memory,o+0xdc,state->runtime.packet_chain);
        return !a->memory.fault && game_level_scene_command_publish(a,o,
            runtime_node_pool_find(state->runtime.packet_chain));
    }
    if(method==0x4e8c00)return game_level_scene_notice(a,o,method,code);
    if(method==0x40e680)return game_level_class40_notice(a,o,code);
    if(method!=0x553750)return 0;
    return vehicle_presentation_command(&a->memory,o,code,&s);
}
int game_input_queue_remove(GameApplication *a,GameAddress queue,GameAddress event,GameAddress method){
    if(!a || a->memory.fault)return 0;
    /*560e20 reports runtime error25 then exits255; it is never an empty
     *notification. Preserve failure when an abstract queue reaches this slot. */
    if(method==0x560e20){
        fprintf(stderr,"Invalid abstract input queue notification at %08x\n",(unsigned)queue);
        a->memory.fault=1;a->memory.fault_address=method;return 0;
    }
    if(method==0x554aa0)return queue>=0x264 && vehicle_presentation_remove_input(&a->memory,queue-0x264,event);
    if(method==0x4f0970)return queue>=0x13c && game_level_player_event_deliver(a,queue-0x13c,event,0);
    /*The live player574e50 and class23 5776b8 queues are above; retired
     *577a38 is abstract. No other notification belongs to these consumers. */
    fprintf(stderr,"Unsupported input queue notification %08x at %08x\n",(unsigned)method,(unsigned)queue);
    a->memory.fault=1;a->memory.fault_address=method;return 0;
}
static int motion_height(void *c,GameAddress o,float x,float z,long double *out){GameApplication *a=c;(void)o;*out=gameplay_geometry_terrain_height(&a->memory,x,z);return !a->memory.fault;}
static int motion_control(void *c,GameAddress o,GameAddress method){
    int ok=game_level_vehicle_control(c,o,method);
    if(!ok)fprintf(stderr,"native vehicle motion control failed: object=%08x method=%08x\n",o,method);
    return ok;
}
static int motion_refresh(void *c,GameAddress o,GameAddress method){
    GameApplication *a=c;
    if(method==0x40ed80)return game_level_class40_geometry(a,o,0);
    if(method==0x40ee80)return game_level_class40_geometry(a,o,2);
    if(game_memory_read_u8(&a->memory,o+12)==0x26 || game_memory_read_u8(&a->memory,o+12)==0x29){
        PresentationPose *pose=presentation_instance_pose(o);unsigned i;if(!pose)return 0;
        if(method==0x535260){
            if(!game_memory_read(&a->memory,o+24,pose->position,12))return 0;
            for(i=0;i<3;i++)pose->angles[i]=game_memory_read_u16(&a->memory,o+36+i*2);
            return presentation_pose_build_world(&a->memory,pose) && game_memory_write(&a->memory,o+0x80,pose->world_matrix,48);
        }
        if(method==0x5353a0)return presentation_pose_build_controller(&a->memory,pose);
        if(method==0x5354d0){pose->flags=get(a,o+16);return presentation_pose_transform_vertices(&a->memory,pose,0);}
    }
    if(game_memory_read_u8(&a->memory,o+12)==0x27){
        if(method==0x535260)return game_level_infantry_geometry(a,o,0);
        if(method==0x544260)return game_level_infantry_geometry(a,o,2);
    }
    if(method==0x553ef0 || method==0x4e65a0 || method==0x407ee0)return vehicle_presentation_group_geometry(&a->memory,o,0);
    if(method==0x553b90 || method==0x4e6c20 || method==0x4083e0)return vehicle_presentation_group_geometry(&a->memory,o,2);
    fprintf(stderr,"unsupported native motion refresh: object=%08x method=%08x\n",o,method);
    return 0;
}
static int motion_gather(void *c,GameAddress o,GameAddress method,const float p[3],int *hit){return game_level_scene_contacts(c,o,method,p,hit);}
static int motion_contact(void *c,GameAddress o,GameAddress other,GameAddress method,const uint32_t velocity[3],const float p[3]){return game_level_contact_response(c,o,other,method,velocity,p);}
static int motion_release(void *c,GameAddress chain){
    GameApplication *a=c;EffectResourcePool *pool=effect_resource_pool_find(0x631910);
    return pool && effect_resource_pool_return_chain(&a->memory,pool,chain)>0;
}
static int motion_spatial(void *c,GameAddress o,int insert){
    GameApplication *a=c;uint32_t xb,zb;float x,z;
    if(!insert){(void)game_list_detach_node(&a->memory,o,8,4);return !a->memory.fault;}
    xb=get(a,o+0x18);zb=get(a,o+0x20);memcpy(&x,&xb,4);memcpy(&z,&zb,4);
    if(a->memory.fault)return 0;
    if((xb!=0x80000000u && xb>=0x48000000u) || (zb!=0x80000000u && zb>=0x48000000u)){
        fprintf(stderr,"Invalid vehicle placement %u: %g, %g\n",(unsigned)o,(double)x,(double)z);exit(666);
    }
    (void)gameplay_spatial_insert(&a->memory,0x5f0800,o,(int32_t)x/4096,(int32_t)z/4096,(get(a,o+16)&0x400)!=0);
    return !a->memory.fault;
}
static int motion_stop_voice(void *c,uint32_t voice){return game_menu_sound_stop(c,0x81,voice);}
int game_level_vehicle_motion_update(GameApplication *a,GameAddress o){
    VehicleMotionServices s={a,motion_height,motion_control,motion_refresh,motion_gather,motion_contact,motion_release,motion_spatial,motion_stop_voice};
    if(!a || a->memory.fault)return 0;
    if(get(a,get(a,o)+0x88)==0x53b3e0)return vehicle_base_motion(&a->memory,o,&s);
    return game_memory_read_u8(&a->memory,o+12)==0x21?vehicle_group_motion(&a->memory,o,&s):vehicle_presentation_motion(&a->memory,o,&s);
}
static int16_t signed_short(uint16_t bits){int16_t v;memcpy(&v,&bits,2);return v;}
int game_level_vehicle_rotation_rate(GameApplication *a,GameAddress o,GameAddress method,int16_t *out){
    GameAddress catalog,record;unsigned offset;
    if(method!=0x4e83e0 && method!=0x4e8420)return 0;
    catalog=get(a,0x5f0210+(get(a,o+0x14)&63u)*4);
    if(!game_level_player_catalog_record(a,catalog,game_memory_read_u8(&a->memory,o+12),&record))return 0;
    if(!record){*out=10;return !a->memory.fault;}
    if(get(a,get(a,o)+0x60)!=0x53c550)return 0;
    offset=(method==0x4e83e0?24:28)+((game_memory_read_u8(&a->memory,o+0xd8)&4)?84:0);
    *out=signed_short(game_memory_read_u16(&a->memory,record+offset));return !a->memory.fault;
}
/*403b60/4059c0: exact short wrap, signed half-rate, bitwise +/-1 direction
 *selection, pitch limits and controlled/free heading. These live records are
 *the same records used by motion and event expiry; there is no copied state. */
static int navigation_speed(void *c,GameAddress o,long double *out){
    GameApplication *a=c;GameAddress table=get(a,o),catalog;uint32_t alternate,value;
    if(get(a,table+0x140)!=0x539a30 || !game_level_scene_property(a,o,get(a,table+0x60),&alternate))return 0;
    catalog=get(a,0x5f0210+(get(a,o+20)&63u)*4);
    if(!game_level_player_catalog_value(a,catalog,game_memory_read_u8(&a->memory,o+12),5,alternate,&value))return 0;
    *out=signed_word(value);return !a->memory.fault;
}
static int navigation_normal(void *c,float out[3],float x,float z){GameApplication *a=c;return gameplay_geometry_terrain_normal_values(&a->memory,out,x,z);}
static float navigation_real(GameApplication *a,GameAddress p){uint32_t bits=get(a,p);float value;memcpy(&value,&bits,4);return value;}
static int navigation_voice(void *c,GameAddress o){
    GameApplication *a=c;GameAddress bank=get(a,0x628a90),reference;uint32_t voice=get(a,o+0x144);long double dx,dz;int playing;float volume;
    if(signed_word(voice)<0 || !get(a,bank+0x468) || (get(a,0x5ac6ac)&3))return !a->memory.fault;
    if(!game_menu_sound_playing(a,0x81,voice,&playing))return 0;
    if(!playing)return !a->memory.fault;
    reference=get(a,0x58f890);dx=(long double)navigation_real(a,o+0x18)-navigation_real(a,reference+12);
    dz=(long double)navigation_real(a,o+0x20)-navigation_real(a,reference+20);
    volume=sound_playback_attenuation(&a->memory,get(a,bank+0x210),(float)(dz*dz+dx*dx));
    return !a->memory.fault && game_menu_sound_volume(a,0x81,voice,volume);
}
int game_level_class29_ground(GameApplication *a,GameAddress o){
    VehicleNavigationServices s={a,navigation_speed,motion_height,navigation_normal,navigation_voice};
    return vehicle_class29_destination(&a->memory,o,&s);
}
int game_level_class29_steer(GameApplication *a,GameAddress o){return vehicle_class29_steer(&a->memory,o);}
int game_level_vehicle_control(GameApplication *a,GameAddress o,GameAddress method){
    GameAddress table;int16_t yaw_rate,pitch_rate;uint16_t angle;uint32_t direction;uint8_t flags;
    if(!a || a->memory.fault)return 0;
    if(method==0x40f1b0){
        GameAddress node=get(a,o+0xdc);float x,z,goalx,goalz;unsigned i;
        if(get(a,o+0xe0)==7)return !a->memory.fault;
        x=navigation_real(a,o+24);z=navigation_real(a,o+32);goalx=navigation_real(a,node+24);goalz=navigation_real(a,node+32);
        if((x==goalx || isnan(x) || isnan(goalx)) && (z==goalz || isnan(z) || isnan(goalz)))return game_level_vehicle_settle(a,o);
        for(i=0;i<3;i++)game_memory_write_u32(&a->memory,o+0xe4+i*4,get(a,o+24+i*4));
        game_memory_write_u32(&a->memory,o+0xe0,7);game_memory_write_u32(&a->memory,o+0x134,0);return !a->memory.fault;
    }
    if(method==0x40f220){
        GameAddress node=get(a,o+0xdc);uint32_t count=get(a,o+0x134),word;unsigned i;
        long double step=(long double)signed_word(count);float point[3];
        game_memory_write_u8(&a->memory,o+0xd8,game_memory_read_u8(&a->memory,o+0xd8)|8);
        point[0]=(float)((((long double)navigation_real(a,node+24)-navigation_real(a,o+0xe4))*step)*navigation_real(a,0x571f08)+navigation_real(a,o+0xe4));
        point[2]=(float)((((long double)navigation_real(a,node+32)-navigation_real(a,o+0xec))*step)*navigation_real(a,0x571f08)+navigation_real(a,o+0xec));
        point[1]=(float)(gameplay_geometry_terrain_height(&a->memory,point[0],point[2])+navigation_real(a,o+0x30));
        game_memory_write_u32(&a->memory,o+0x134,count+1);
        if(!(get(a,o+16)&0x200000)){
            for(i=0;i<3;i++)game_memory_write_u32(&a->memory,o+24+i*4,get(a,node+24+i*4));
            game_memory_write_u32(&a->memory,o+0xe0,5);
        }else for(i=0;i<3;i++){memcpy(&word,point+i,4);game_memory_write_u32(&a->memory,o+24+i*4,word);}
        return !a->memory.fault;
    }
    if(method==0x4511a0)return game_level_class29_ground(a,o);
    if(method==0x4516d0)return game_level_class29_steer(a,o);
    if(method==0x40cd20)return game_level_class40_timed_gun(a,o);
    if(method==0x40d590)return game_level_class40_control(a,o);
    if(method==0x40ce40)return game_level_class40_combat(a,o);
    if(method==0x40d900)return game_level_class40_reaction(a,o);
    if(method==0x40cc40 || method==0x40eaf0 || method==0x40eb00 || method==0x40eb10 || method==0x40eb20)return game_level_class40_profile(a,o,method);
    if(method==0x451170)return !a->memory.fault; /*Verified RET. */
    if(method==0x545680)return game_level_class27_combat(a,o);
    if(method==0x546100)return game_level_class27_timed_refresh(a,o);
    if(method==0x544970)return game_level_class27_reposition(a,o);
    if(method==0x544d50)return game_level_class27_follow_move(a,o);
    if(method==0x544f90)return game_level_class27_idle(a,o);
    if(method==0x546f50)return game_level_class27_ascent(a,o);
    if(method==0x5472e0)return game_level_class27_ascent_step(a,o);
    if(method==0x544500)return game_level_class27_react(a,o);
    if(method==0x544730)return game_level_class27_escape(a,o);
    if(method==0x5460f0)return !a->memory.fault; /*Verified RET. */
    if(method==0x55bc60)return !a->memory.fault; /*Class26's actual empty idle method. */
    if(method==0x55b840)return game_level_aircraft_flight(a,o);
    if(method==0x55b780)return game_level_aircraft_follow(a,o);
    if(method==0x55b580)return game_level_aircraft_combat(a,o);
    if(method==0x55bc70)return game_level_aircraft_delivery(a,o);
    if(method==0x53cba0 || method==0x409610)return !a->memory.fault; /*Verified RET movement/contact overrides. */
    if(method==0x545eb0 || method==0x545fb0 || method==0x546270 || method==0x546450)
        return class27_control(a,o,method);
    /*Verified RET bodies used by the state table, not unsupported stubs. */
    if(method>=0x53a050 && method<=0x53a120 && !(method&15))return 1;
    if(method==0x405930)return vehicle_heading_step(&a->memory,o);
    if(method==0x53a3e0 || method==0x4e8510 || method==0x4062b0){
        VehicleNavigationServices s={a,navigation_speed,motion_height,navigation_normal,navigation_voice};
        if(method==0x53a3e0)return vehicle_destination_step(&a->memory,o,&s);
        return method==0x4e8510?vehicle_group_destination(&a->memory,o,&s):vehicle_group_ascent(&a->memory,o,&s);
    }
    if(method==0x55ae60 || method==0x55af90)return game_level_aircraft_release(a,o,method);
    if(method==0x4059c0){
        if(!(game_memory_read_u8(&a->memory,o+0xd8)&1))game_memory_write_u16(&a->memory,o+0x14c,
            (game_memory_read_u16(&a->memory,o+0x148)-game_memory_read_u16(&a->memory,o+0x24))&2047u);
        return !a->memory.fault;
    }
    if(method==0x53c000)return game_level_vehicle_settle(a,o);
    if(method==0x404950)return game_level_vehicle_targeting_frame(a,o);
    if(method!=0x403b60)return 0;
    table=get(a,o);
    if(!game_level_vehicle_rotation_rate(a,o,get(a,table+0x1d4),&yaw_rate) || !game_level_vehicle_rotation_rate(a,o,get(a,table+0x1d8),&pitch_rate))return 0;
    if(get(a,o+0x1f8)){
        yaw_rate=(int16_t)(yaw_rate>=0?yaw_rate/2:-((-(int32_t)yaw_rate+1)/2));
        pitch_rate=(int16_t)(pitch_rate>=0?pitch_rate/2:-((-(int32_t)pitch_rate+1)/2));
        game_memory_write_u32(&a->memory,o+0x1f8,0);
    }
    direction=get(a,o+0x1f4);
    if(direction==0x3f800000u || direction==0xbf800000u){
        angle=game_memory_read_u16(&a->memory,o+0x148);
        angle=(uint16_t)(direction==0x3f800000u?angle-(uint16_t)yaw_rate:angle+(uint16_t)yaw_rate);
        flags=game_memory_read_u8(&a->memory,o+0xd8);game_memory_write_u8(&a->memory,o+0xd8,flags|8);
        game_memory_write_u16(&a->memory,o+0x148,angle&2047u);
    }
    direction=get(a,o+0x1f0);
    if(direction==0xbf800000u || direction==0x3f800000u){
        int16_t limit;GameAddress limit_method;
        angle=game_memory_read_u16(&a->memory,o+0x14a);
        angle=(uint16_t)(direction==0xbf800000u?angle-(uint16_t)pitch_rate:angle+(uint16_t)pitch_rate);
        game_memory_write_u16(&a->memory,o+0x14a,angle);flags=game_memory_read_u8(&a->memory,o+0xd8);
        game_memory_write_u8(&a->memory,o+0xd8,flags|8);
        limit_method=get(a,get(a,o)+(direction==0xbf800000u?0x1cc:0x1d0));
        if(!vehicle_targeting_pitch_limit(limit_method,&limit))return 0;
        if(direction==0xbf800000u?signed_short(angle)<limit:signed_short(angle)>limit){
            game_memory_write_u32(&a->memory,o+0x1f0,0);game_memory_write_u16(&a->memory,o+0x14a,(uint16_t)limit);
        }
    }
    return !a->memory.fault;
}
/*53c000 ->4ca2a0 ->4cecc0/4cec80 ->406150/53c030. Advance or reuse the
 *existing command owner, then use the same publisher as scene placement. */
int game_level_vehicle_settle(GameApplication *a,GameAddress o){
    GameAddress table,command;RuntimeNode *node;GameStateRelinkResult result;
    if(!a || a->memory.fault)return 0;
    game_memory_write_u32(&a->memory,o+0xe0,5);table=get(a,o);
    if(get(a,table+0x120)!=0x4ca2a0 ||
       (get(a,table+0x16c)!=0x406150 && get(a,table+0x16c)!=0x53c030 && get(a,table+0x16c)!=0x5475a0 && get(a,table+0x16c)!=0x55b500))return 0;
    command=get(a,o+0xdc);node=runtime_node_pool_find(command);
    if(!node || !node->live || node->in_pool)return 0;
    if(node->next){
        if(!runtime_node_pool_advance(&a->memory,node,&result))return 0;
        game_memory_write_u32(&a->memory,o+0xdc,result.final_eax);
        node=runtime_node_pool_find(result.final_eax);
    }else if(runtime_node_pool_prepare(&a->memory,command)!=1)return 0;
    return !a->memory.fault && game_level_scene_command_publish(a,o,node);
}
static void class27_real(GameApplication *a,GameAddress p,float value){uint32_t bits;memcpy(&bits,&value,4);game_memory_write_u32(&a->memory,p,bits);}
/*543bb0: publish a bounded destination and derive its actual movement vector.
 *The record remains the presentation owner's authoritative class tail. */
int game_level_class27_destination(GameApplication *a,GameAddress o,const float point[3]){
    float p[3],delta[3],ground;long double lower,upper,scale,vx;uint16_t angles[2];int yaw,pitch;unsigned i;
    lower=(long double)navigation_real(a,o+0x38)*navigation_real(a,0x576fa8);
    upper=(long double)navigation_real(a,o+0x38)*navigation_real(a,0x576fac)+navigation_real(a,0x576fb0);
    if(!(lower<point[0]) || !(upper>point[0]) || !(point[2]>(float)lower) || !(point[2]<(float)upper)){
        fprintf(stderr,"class27 destination outside world bounds\n");return !a->memory.fault;
    }
    memcpy(p,point,sizeof(p));class27_real(a,o+0xf8,p[0]);class27_real(a,o+0x100,p[2]);
    ground=(float)gameplay_geometry_terrain_height(&a->memory,p[0],p[2]);
    {uint32_t bits;memcpy(&bits,p+1,4);
        if(bits==0x47000000)p[1]=(float)((long double)ground+navigation_real(a,0x59d8a8));
        else if((long double)ground+navigation_real(a,0x576fb8)>p[1])p[1]=(float)((long double)ground+navigation_real(a,0x576fb8));}
    for(i=0;i<3;i++)delta[i]=(float)((long double)p[i]-navigation_real(a,o+24+i*4));
    scale=navigation_real(a,o+0x128)/sqrtl((float)(((long double)delta[0]*delta[0]+(long double)delta[2]*delta[2])+(long double)delta[1]*delta[1]));
    class27_real(a,o+0xfc,p[1]);class27_real(a,o+0xb8,(float)((long double)delta[2]*scale));
    class27_real(a,o+0x148,(float)((long double)delta[1]*scale));vx=(long double)delta[0]*scale;
    class27_real(a,o+0xb4,(float)vx);game_memory_write_u32(&a->memory,o+0xe0,0);
    {uint32_t ybits;memcpy(&ybits,delta+1,4);
        if(vx==navigation_real(a,0x576f80) && !(get(a,o+0xb8)&0x7fffffff) && signed_word(ybits)<0x44160000){
            int roll=signed_short(game_memory_read_u16(&a->memory,o+0x28));
            game_memory_write_u32(&a->memory,o+0x160,1);game_memory_write_u16(&a->memory,o+0x14e,0);
            game_memory_write_u16(&a->memory,o+0x14c,(game_memory_read_u16(&a->memory,o+0x24)+1023u)&2047);
            game_memory_write_u16(&a->memory,o+0x28,(uint16_t)(roll<0?roll+11:roll-11));return !a->memory.fault;
        }}
    game_memory_write_u32(&a->memory,o+0x160,0);
    {int16_t target_pitch;
        if(!vehicle_targeting_point_angles(&a->memory,o,p,angles,&target_pitch))return 0;
        angles[1]=(uint16_t)target_pitch;}
    game_memory_write_u16(&a->memory,o+0x14c,angles[0]);game_memory_write_u16(&a->memory,o+0x14e,angles[1]);
    yaw=signed_short((uint16_t)abs(signed_short(angles[0])-signed_short(game_memory_read_u16(&a->memory,o+0x24))));
    pitch=signed_short((uint16_t)abs(signed_short(angles[1])-signed_short(game_memory_read_u16(&a->memory,o+0x26))));
    if(yaw>=512){game_memory_write_u32(&a->memory,o+0x164,0);game_memory_write_u32(&a->memory,o+0x168,1);}
    if((yaw>256 && yaw<1792) || pitch>56){
        GameAddress reference=get(a,0x58f890);SoundPlaybackRequest request={0};int j,playing;
        long double dx=(long double)navigation_real(a,o+24)-navigation_real(a,reference+12),dz=(long double)navigation_real(a,o+32)-navigation_real(a,reference+20);
        request.distance_squared=(float)(dx*dx+dz*dz);
        if((long double)navigation_real(a,0x59acc0)*navigation_real(a,0x59acc0)>request.distance_squared){
            for(j=(int)game_memory_read_u8(&a->memory,0x59acbc)-1;j>=0;j--){
                if(!game_menu_sound_playing(a,6,(uint32_t)j,&playing))return 0;
                if(playing)return !a->memory.fault;
            }
            request.sound=6;request.volume=1;return game_menu_sound_play(a,&request);
        }
    }
    return !a->memory.fault;
}
/*546ca0: target class/flags choose the two real pursuit distances. The same
 *gameplay RNG cursor owns both draws, including its original1024 wrap. */
int game_level_class27_target_changed(GameApplication *a,GameAddress o){
    GameAddress target=get(a,o+0xf0),scales[2],bases[2];uint32_t flags;unsigned i;int radius=0;
    if(!target || a->memory.fault)return 0;
    flags=get(a,target+16);
    if(flags&0x4000){scales[0]=0x577024;scales[1]=0x576f88;bases[0]=0x576fd8;bases[1]=0x577028;}
    else if(game_memory_read_u8(&a->memory,target+12)==0x20){scales[0]=0x576f84;scales[1]=0x576ff4;bases[0]=0x57702c;bases[1]=0x576fdc;}
    else if(flags&0x8000){scales[0]=0x576fd8;scales[1]=0x576f8c;bases[0]=0x576f84;bases[1]=0x576ff4;radius=1;}
    else{scales[0]=0x576fd8;scales[1]=0x577030;bases[0]=0x576f84;bases[1]=0x577034;}
    for(i=0;i<2;i++){
        int32_t sample=signed_word(effect_initialization_random(&a->memory,navigation_real(a,scales[i])));
        long double value=sample;if(radius)value+=navigation_real(a,target+0x38);
        value+=navigation_real(a,bases[i]);class27_real(a,o+0x170+i*4,(float)value);
    }
    return !a->memory.fault;
}
/*546450: signed pitch stepping and circular yaw stepping retain the original
 *bank behavior, including its small negative-roll equilibrium. */
int game_level_class27_steer(GameApplication *a,GameAddress o){
    int16_t yaw=signed_short(game_memory_read_u16(&a->memory,o+0x24));
    int16_t pitch=signed_short(game_memory_read_u16(&a->memory,o+0x26));
    int16_t target_yaw=signed_short(game_memory_read_u16(&a->memory,o+0x14c));
    int16_t target_pitch=signed_short(game_memory_read_u16(&a->memory,o+0x14e));
    int16_t roll=signed_short(game_memory_read_u16(&a->memory,o+0x28)),delta;int increase;
    if(yaw==target_yaw && pitch==target_pitch && !get(a,o+0x168)){
        if(roll<0)roll=(int16_t)(roll+11);if(roll>0)roll=(int16_t)(roll-11);
        game_memory_write_u16(&a->memory,o+0x28,(uint16_t)roll);return !a->memory.fault;
    }
    delta=(int16_t)abs((int)target_pitch-pitch);
    if(pitch>target_pitch && delta>14)pitch=(int16_t)(pitch-14);
    else if(pitch<target_pitch && delta>14)pitch=(int16_t)(pitch+14);
    else pitch=target_pitch;
    game_memory_write_u16(&a->memory,o+0x26,(uint16_t)pitch);
    delta=(int16_t)abs((int)target_yaw-yaw);if(delta>1024)delta=(int16_t)(2048-delta);
    if(delta<=31){game_memory_write_u16(&a->memory,o+0x24,(uint16_t)target_yaw);return !a->memory.fault;}
    increase=yaw<=1024?(yaw<target_yaw && (int)target_yaw-yaw<1024):!(yaw>target_yaw && (int)yaw-target_yaw<1024);
    game_memory_write_u16(&a->memory,o+0x24,(uint16_t)(yaw+(increase?31:-31)));
    if(!get(a,o+0x160)){
        roll=(int16_t)(roll+(increase?-11:11));if(roll< -512)roll=-512;else if(roll>=512)roll=512;
        game_memory_write_u16(&a->memory,o+0x28,(uint16_t)roll);
    }
    return !a->memory.fault;
}
static int class27_course(GameApplication *a,GameAddress o){
    GameAddress target=get(a,o+0xf0);float p[3],delta[3],sum;long double z,scale;
    uint16_t yaw;int16_t pitch;unsigned i;
    {float origin[3];for(i=0;i<3;i++)origin[i]=navigation_real(a,o+24+i*4);
        if(!target || !presentation_target_position(&a->memory,target,origin,p))return 0;}
    for(i=0;i<3;i++){
        class27_real(a,o+0x154+i*4,p[i]);class27_real(a,o+0xf8+i*4,p[i]);
        delta[i]=(float)((long double)p[i]-navigation_real(a,o+24+i*4));
    }
    z=(long double)p[2]-navigation_real(a,o+32);
    sum=(float)(((long double)delta[0]*delta[0]+z*delta[2])+(long double)delta[1]*delta[1]);
    scale=(long double)navigation_real(a,o+0x128)/sqrtl((long double)sum);
    class27_real(a,o+0xb4,(float)((long double)delta[0]*scale));
    class27_real(a,o+0x148,(float)((long double)delta[1]*scale));
    class27_real(a,o+0xb8,(float)((long double)delta[2]*scale));
    if(!vehicle_targeting_point_angles(&a->memory,o,p,&yaw,&pitch))return 0;
    game_memory_write_u16(&a->memory,o+0x14c,yaw);game_memory_write_u16(&a->memory,o+0x14e,(uint16_t)pitch);
    game_memory_write_u32(&a->memory,o+0xe0,4);return !a->memory.fault;
}
/*545fb0/546270: straight pursuit and oscillating pursuit share the same
 *snapshot, axis gates, arrival state and steering consumer. */
static int class27_control(GameApplication *a,GameAddress o,GameAddress method){
    GameAddress target,table=get(a,o);uint32_t speed;float tolerance;unsigned i;
    static const unsigned velocities[3]={0xb4,0x148,0xb8};
    if(method==0x546450)return game_level_class27_steer(a,o);
    if(method==0x545eb0)return class27_course(a,o);
    if(method==0x545fb0){target=get(a,o+0xf0);
        if(!target || get(a,target+20)!=get(a,o+0xf4)){game_memory_write_u32(&a->memory,o+0xe0,5);return !a->memory.fault;}}
    if(get(a,table+0x60)!=0x53c550 || !game_level_player_catalog_value(a,get(a,0x58973c),
        game_memory_read_u8(&a->memory,o+12),5,(game_memory_read_u8(&a->memory,o+0xd8)&4)!=0,&speed))return 0;
    tolerance=(float)((long double)signed_word(speed)+navigation_real(a,0x576f90));
    if(method==0x546270 && (get(a,o+0x168) || (get(a,o+0x164)&0x7fffffff))){
        float future[3],wave=navigation_real(a,o+0x164),step=navigation_real(a,0x57701c);uint16_t yaw;int16_t pitch;
        future[0]=(float)((long double)navigation_real(a,o+24)+navigation_real(a,o+0xb4));
        future[2]=(float)((long double)navigation_real(a,o+32)+navigation_real(a,o+0xb8));
        if(get(a,o+0x168)){
            class27_real(a,o+0x164,(float)((long double)wave+step));
            if(signed_word(get(a,o+0x164))>0x442f0000)game_memory_write_u32(&a->memory,o+0x168,0);
            class27_real(a,o+28,(float)((long double)navigation_real(a,o+28)+step));
        }else if(get(a,o+0x164)&0x7fffffff){
            class27_real(a,o+0x164,(float)((long double)wave-step));
            class27_real(a,o+28,(float)((long double)navigation_real(a,o+28)-step));
        }
        future[1]=(float)((long double)navigation_real(a,o+28)+navigation_real(a,o+0x148));
        if(!vehicle_targeting_point_angles(&a->memory,o,future,&yaw,&pitch))return 0;
        game_memory_write_u16(&a->memory,o+0x14c,yaw);game_memory_write_u16(&a->memory,o+0x14e,(uint16_t)pitch);
        if(!(fabsl((long double)navigation_real(a,o+28)-navigation_real(a,o+0xfc))>=tolerance)){
            game_memory_write_u32(&a->memory,o+0x168,0);game_memory_write_u32(&a->memory,o+0x164,0);
        }
    }
    for(i=0;i<3;i++)if(fabsl((long double)navigation_real(a,o+24+i*4)-navigation_real(a,o+0xf8+i*4))>=tolerance)
        class27_real(a,o+24+i*4,(float)((long double)navigation_real(a,o+24+i*4)+navigation_real(a,o+velocities[i])));
    game_memory_write_u8(&a->memory,o+0xd8,(uint8_t)(game_memory_read_u8(&a->memory,o+0xd8)|8));
    for(i=0;i<3;i++)if(fabsl((long double)navigation_real(a,o+24+i*4)-navigation_real(a,o+0xf8+i*4))>=tolerance)break;
    if(i==3)game_memory_write_u32(&a->memory,o+0xe0,5);
    return get(a,table+0x1cc)==0x546450 && game_level_class27_steer(a,o);
}
