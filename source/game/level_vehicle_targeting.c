#include "level_run.h"
#include <stdio.h>
#include "level_factory.h"
#include "level_player_scene.h"
#include "level_player_resources.h"
#include "level_player_slots.h"
#include "level_objects.h"
#include "vehicle_targeting.h"
#include "gameplay_geometry.h"
#include "gameplay_leaves.h"
#include "gameplay_state.h"
#include "entity_queries.h"
#include "game_matrix.h"
#include "game_state_ops.h"
#include "game_intrusive_list.h"
#include "player_scene_children.h"
#include "menu_sound.h"
#include "presentation_commands.h"
#include "effect_model.h"
#include "scene_orders.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
/*Unreconstructed class programs remain mandatory links, never CPU fallback. */
static int target_cell_score(GameApplication *,GameAddress,GameAddress,int32_t,int32_t,uint32_t,uint32_t *,float *,GameAddress *);
extern int game_level_other_scene_transient_reset(GameApplication *,GameAddress,GameAddress);
extern int game_level_vehicle_control(GameApplication *,GameAddress,GameAddress);
extern int game_level_vehicle_rotation_rate(GameApplication *,GameAddress,GameAddress,int16_t *);
extern int game_input_world_line(GameApplication *,const float[3],const float[3],float[3],int *);
extern int game_level_scene_order_issue(GameApplication *,GameAddress,const PresentationCommand *,int);
extern int game_level_attachment_transform(GameApplication *,GameAddress,GameAddress,const float[12]);
extern int game_level_class27_destination(GameApplication *,GameAddress,const float[3]);
extern int game_level_vehicle_settle(GameApplication *,GameAddress);
extern int game_level_scene_target_order(void *,GameAddress,uint32_t,GameAddress);
extern int game_level_scene_identity_order(GameApplication *,GameAddress,uint32_t,uint32_t);
extern int game_level_scene_replace_command(GameApplication *,GameAddress,GameAddress,int);
extern int game_level_surface_particles(GameApplication *,GameAddress,const float[3],int32_t,GameAddress);
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
static int32_t integer(uint32_t v){int32_t s;memcpy(&s,&v,4);return s;}
static float real(GameApplication *a,GameAddress p){uint32_t v=get(a,p);float f;memcpy(&f,&v,4);return f;}
static uint32_t float_bits(float f){uint32_t v;memcpy(&v,&f,4);return v;}
static int16_t aircraft_short(uint16_t bits){int16_t v;memcpy(&v,&bits,2);return v;}
static int aircraft_near(long double difference,float threshold){return isnan(difference) || isnan(threshold) || fabsl(difference)<threshold;}
/*55b840: flight heading/bank, travel, terrain forecast and altitude response.
 *The existing presentation tail owns every timer and velocity. */
int game_level_aircraft_flight(GameApplication *a,GameAddress o){
    int difference,bank,roll;uint16_t yaw;uint32_t speed,tick,mode,voice;GameAddress owner,sound_bank;
    float threshold,ground,forecast[2],height;long double lower,upper,predicted,y,step;unsigned i;
    if(!a || get(a,o)!=0x577a90 || a->memory.fault)return 0;
    game_memory_write_u8(&a->memory,o+0xd8,game_memory_read_u8(&a->memory,o+0xd8)|8);
    difference=aircraft_short(game_memory_read_u16(&a->memory,o+0x140));yaw=game_memory_read_u16(&a->memory,o+36);
    if(abs(difference)<5){yaw=game_memory_read_u16(&a->memory,o+0x13c);game_memory_write_u16(&a->memory,o+0x140,0);}
    else{
        yaw=(uint16_t)((yaw+(difference>0?-5:5))&2047);difference+=difference>0?-5:5;
        game_memory_write_u16(&a->memory,o+0x140,(uint16_t)difference);
        bank=aircraft_short(game_memory_read_u16(&a->memory,o+0x13e));roll=aircraft_short(game_memory_read_u16(&a->memory,o+40));
        roll=aircraft_short((uint16_t)(roll+(bank<0?-5:bank>0?5:0)));game_memory_write_u16(&a->memory,o+40,(uint16_t)roll);
        if(abs(roll)>=abs(bank))game_memory_write_u16(&a->memory,o+0x13e,(uint16_t)(-bank));
    }
    game_memory_write_u16(&a->memory,o+36,yaw);
    owner=get(a,0x5f0210+(get(a,o+20)&63)*4);mode=(game_memory_read_u8(&a->memory,o+0xd8)&4)!=0;
    if(!game_level_player_catalog_value(a,owner,0x26,5,mode,&speed))return 0;
    threshold=(float)((long double)integer(speed)+real(a,0x577c68));
    if(!aircraft_near((long double)real(a,o+24)-real(a,o+0xf8),threshold))put(a,o+24,float_bits((float)((long double)real(a,o+0xb4)+real(a,o+24))));
    if(!aircraft_near((long double)real(a,o+32)-real(a,o+0x100),threshold))put(a,o+32,float_bits((float)((long double)real(a,o+0xb8)+real(a,o+32))));
    if(aircraft_near((long double)real(a,o+24)-real(a,o+0xf8),threshold) &&
       aircraft_near((long double)real(a,o+32)-real(a,o+0x100),threshold) && yaw==game_memory_read_u16(&a->memory,o+0x13c)){
        put(a,o+24,get(a,o+0xf8));put(a,o+32,get(a,o+0x100));put(a,o+0xe0,5);return !a->memory.fault;
    }
    /*42d390 uses signed/unsigned height bits and asymmetric inclusive bounds. */
    lower=(long double)real(a,o+0x38)*real(a,0x57217c);
    upper=(long double)real(a,o+0x38)*real(a,0x572180)+real(a,0x572184);
    if(integer(get(a,o+28))>0x46ea6000 || get(a,o+28)>0xc6ea6000 ||
       !(lower<=real(a,o+24)) || real(a,o+32)<(float)lower || upper<real(a,o+24) || !(real(a,o+32)<(float)upper)){
        game_memory_write_u8(&a->memory,o+0xd8,game_memory_read_u8(&a->memory,o+0xd8)&~8u);
        return game_level_aircraft_reset(a,o);
    }
    ground=(float)gameplay_geometry_terrain_height(&a->memory,real(a,o+24),real(a,o+32));
    if((long double)real(a,o+28)-real(a,0x577c84)<ground)put(a,o+28,float_bits((float)((long double)ground+real(a,0x577c84))));
    if(get(a,get(a,o+0xdc)+12)==2)y=(long double)real(a,o+0x15c)+real(a,o+28);
    else{
        tick=get(a,0x5ac6ac);
        if(integer(get(a,o+0x17c))<=integer(tick)){
            forecast[0]=(float)((long double)real(a,o+0xb4)*real(a,0x577c70)+real(a,o+24));
            forecast[1]=(float)((long double)real(a,o+0xb8)*real(a,0x577c70)+real(a,o+32));put(a,o+0x17c,tick+12);
            for(i=0;i<2;i++){uint32_t bits=float_bits(forecast[i]);if(bits>0x80000000)bits=0;else if(integer(bits)>0x47ffff80)bits=0x47ffff80;memcpy(forecast+i,&bits,4);}
            predicted=gameplay_geometry_terrain_height(&a->memory,forecast[0],forecast[1])+real(a,o+0x16c);height=(float)predicted;
            if(predicted>real(a,o+0x174)){
                put(a,o+0x174,float_bits((float)((long double)real(a,o+0x16c)+height)));put(a,o+0x178,tick);
            }else if(real(a,o+0x174)>height && integer(get(a,o+0x178)+60)<=integer(tick))
                put(a,o+0x174,float_bits((float)((long double)real(a,o+0x16c)+height)));
        }
        y=real(a,o+28);height=real(a,o+0x170);
        if(!isnan(height) && !isnan(real(a,o+0x174)) && height!=real(a,o+0x174)){
            int rising=height<real(a,o+0x174);step=(long double)real(a,o+0x128)*real(a,rising?0x577c8c:0x577c90);
            put(a,o+0x170,float_bits((float)((long double)height+step)));y+=step;
            if(rising?real(a,o+0x170)>real(a,o+0x174):real(a,o+0x170)<real(a,o+0x174)){
                y=real(a,o+0x174);put(a,o+0x170,get(a,o+0x174));
            }
        }
    }
    put(a,o+28,float_bits((float)y));sound_bank=get(a,0x628a90);tick=get(a,0x5ac6ac);voice=get(a,o+0x180);
    if(get(a,sound_bank+0x468) && (tick&3)){
        int playing;if(!game_menu_sound_playing(a,24,voice,&playing))return 0;
        if(playing){GameAddress reference=get(a,0x58f890);long double x=(long double)real(a,o+24)-real(a,reference+12),z=(long double)real(a,o+32)-real(a,reference+20);
            float volume=sound_playback_attenuation(&a->memory,get(a,sound_bank+0x6c),(float)(x*x+z*z));
            if(!game_menu_sound_volume(a,24,voice,volume))return 0;
        }
    }
    return !a->memory.fault;
}
static uint32_t truncate_word(long double v){return (uint32_t)(uint64_t)(!isfinite(v)||v< -0x1p63L||v>=0x1p63L?INT64_MIN:(int64_t)v);}
static int32_t cell(float f){int32_t n=integer(truncate_word(f));return n>=0?n/4096:-1-(int32_t)((0u-(uint32_t)n-1u)/4096u);}
static int acquire(void *c,GameAddress o,GameAddress method,uint32_t flags,GameAddress *out){
    GameApplication *a=c;int32_t x,z,radius,x0,x1,z0,z1;GameAddress best=0,last=0;float score=268435456.0f;
    if(method!=0x4cbe70)return 0;
    x=cell(real(a,o+24));z=cell(real(a,o+32));radius=integer(get(a,o+0x120));
    radius=radius>=0?radius/4096+(radius%4096!=0):radius/4096;
    x0=x-radius;if(x0<0)x0=0;x1=x+radius;if(x1>32)x1=32;
    z0=z-radius;if(z0<0)z0=0;z1=z+radius;if(z1>32)z1=32;
    /*4cbe70 caches the cell callback for one X column and retains priority,
     *distance and the last nonzero callback return independently. */
    if(radius<0 || x0>32 || z0>32)return 0;
    for(x=x0;x<x1;x++){
        method=get(a,get(a,o)+0x1b0);
        for(z=z0;z<z1;z++){
            GameAddress selected=last;
            if(!target_cell_score(a,o,method,x,z,flags,&best,&score,&selected))return 0;
            if(selected)last=selected;
        }
    }
    *out=last;return !a->memory.fault;
}
int game_level_vehicle_target_acquire(GameApplication *a,GameAddress o,uint32_t flags,GameAddress *out){
    /*4cc1c0 returns8040 irrespective of its caller's argument. */
    (void)flags;
    { GameAddress method=get(a,get(a,o)+0x1b4);
      if(method!=0x4cc1c0 && method!=0x409320 && method!=0x40eb80)return 0;
      return acquire(a,o,get(a,get(a,o)+0x1ac),method==0x409320?0x4000:method==0x40eb80?0x8000:0x8040,out); }
}
/*4cc1d0/54b3e0: attack value times the actual resistance coefficient,
 *minus the candidate's remaining health; original signed truncation. */
static int target_priority(GameApplication *a,GameAddress o,GameAddress record,GameAddress candidate,int32_t *out){
    uint32_t alternate,attack,kind,resistance,method;long double value;
    method=get(a,get(a,o)+0x1b8);
    if((method!=0x4cc1d0 && method!=0x55ca80 && method!=0x40eb90) || get(a,get(a,o)+0x60)!=0x53c550)return 0;
    alternate=(game_memory_read_u8(&a->memory,o+0xd8)&4u)!=0;
    if(method==0x55ca80){
        uint32_t identity=get(a,o+20)&63,choice;GameAddress bank;
        if(game_memory_read_u8(&a->memory,candidate+12)!=0x27 || !get(a,o+0x194)){*out=0;return !a->memory.fault;}
        if(!game_level_player_catalog_choice(a,get(a,0x5f0210+identity*4),0x26,&choice))return 0;
        bank=(identity==4?0x5ecba8:identity==5?0x5e6e30:0x5eda38)+choice*112u+(alternate?52u:0u);
        attack=get(a,bank);kind=get(a,bank+8);
    }else if(method==0x40eb90 && game_memory_read_u16(&a->memory,o+0x146) && (get(a,candidate+16)&0x8000)){
        /*40eb90 secondary weapon uses the owner's actual tier and faction
         *recipe, then the same54b3e0 resistance/health scoring as primary fire. */
        uint32_t identity=get(a,o+20)&63,choice;GameAddress bank;
        if(!game_level_player_catalog_choice(a,get(a,0x5f0210+identity*4),0x40,&choice))return 0;
        bank=(identity==4?0x5e67e8:identity==5?0x5e8388:0x5e7658)+choice*112u+(alternate?52u:0u);
        attack=get(a,bank+28);kind=get(a,bank);
    }else{attack=get(a,record+(alternate?0x7c:0x28));kind=get(a,record+(alternate?0x80:0x2c));}
    resistance=get(a,candidate+0x60);
    if(kind>=18 || resistance>=13)return 0;
    value=(long double)integer(attack)*real(a,get(a,0x59dae4)+(kind*13u+resistance)*4u)-integer(get(a,candidate+0x5c));
    *out=integer(truncate_word(value));return !a->memory.fault;
}
int game_level_scene_target_priority(GameApplication *a,GameAddress o,GameAddress root,GameAddress target,int32_t *out){
    GameAddress record;
    return game_level_player_catalog_record(a,root,game_memory_read_u8(&a->memory,o+12),&record) &&
        target_priority(a,o,record,target,out);
}
static int target_cell_score(GameApplication *a,GameAddress o,GameAddress method,int32_t x,int32_t z,uint32_t flags,uint32_t *best,float *score,GameAddress *selected){
    GameAddress candidate,record,chosen=0,old=*selected;uint32_t masked=old?get(a,old+16)&flags:0;unsigned visits=0;float radius_squared;
    if(method!=0x4cbf60 || x<0 || x>=32 || z<0 || z>=32)return 0;
    put(a,0x59440c,get(a,0x59440c)+1u);
    if(!game_level_player_catalog_record(a,get(a,0x58973c),game_memory_read_u8(&a->memory,o+12),&record))return 0;
    radius_squared=(float)((long double)integer(get(a,o+0x120))*integer(get(a,o+0x120)));
    candidate=get(a,0x5f0804+((uint32_t)x*32u+(uint32_t)z)*12u);
    while(candidate && !a->memory.fault){
        NativeEntityRelationshipResult relationship;uint32_t candidate_flags;int32_t priority;float squared;long double dx,dy,dz,wide;
        if(++visits>65536)return 0;
        if(!native_entity_relationship_query(&a->memory,candidate,o,&relationship))return 0;
        if(relationship.decision<NATIVE_ENTITY_RELATION_ACCEPT_NULL_SECOND)goto next;
        candidate_flags=get(a,candidate+16);
        if(candidate_flags&0x800u || (masked==flags && !(candidate_flags&flags)))goto next;
        dx=(long double)real(a,o+24)-real(a,candidate+24);dy=(long double)real(a,o+28)-real(a,candidate+28);dz=(long double)real(a,o+32)-real(a,candidate+32);
        wide=(dy*dy+dz*dz)+dx*dx;squared=(float)wide;
        if(wide>radius_squared)goto next;
        {float from[3],to[3],hit[3];int clear;unsigned i;
            for(i=0;i<3;i++){from[i]=real(a,o+24+i*4);to[i]=real(a,candidate+24+i*4);}
            from[1]=(float)((long double)from[1]+real(a,o+0x30));to[1]=(float)((long double)to[1]+real(a,candidate+0x30));
            if(!game_input_world_line(a,from,to,hit,&clear))return 0;
            if(!clear)goto next;
        }
        if(!target_priority(a,o,record,candidate,&priority))return 0;
        if(priority<=0)goto next;
        if(priority>=1024)priority=1023;
        if(masked==flags){
            int better=integer(*best)<=priority,closer=*score>squared;
            if(!(better && closer)){
                float weighted_new,quantized;int32_t index;uint32_t word;
                if(!better && !closer)goto next;
                if(integer(float_bits(squared))>0x4d800000)squared=268435456.0f;
                put(a,0x594410,get(a,0x594410)+1u);
                if(*best>=1024)return 0;
                weighted_new=(float)((long double)priority*real(a,0x5ab500+*best*4));
                word=truncate_word(sqrtl(squared));index=integer(word);index=index>=0?index/16:-1-(int32_t)((0u-word-1u)/16u);quantized=(float)index;
                word=truncate_word(sqrtl(*score))&0xfffffff3u;index=integer(word);index=index>=0?index/4:-1-(int32_t)((0u-word-1u)/4u);
                if(!((long double)real(a,0x5ab500+(uint32_t)index)*quantized<weighted_new))goto next;
            }
        }
        chosen=candidate;*best=(uint32_t)priority;*score=squared;masked=get(a,candidate+16)&flags;
next:candidate=get(a,candidate+4);
    }
    *selected=chosen;return !a->memory.fault;
}
static int settle(void *c,GameAddress o,GameAddress method){return game_level_vehicle_control(c,o,method);}
static int limit(void *c,GameAddress o,GameAddress method,int16_t *out){GameApplication *a=c;(void)o;
    return !a->memory.fault && vehicle_targeting_pitch_limit(method,out);
}
static int rate(void *c,GameAddress o,GameAddress method,int16_t *out){return game_level_vehicle_rotation_rate(c,o,method,out);}
static int directions(void *c,GameAddress o,GameAddress method,float yaw,float pitch){GameApplication *a=c;uint32_t bits;
    if(method!=0x4e84a0)return 0;
    memcpy(&bits,&yaw,4);put(a,o+0x1f4,bits);memcpy(&bits,&pitch,4);put(a,o+0x1f0,bits);
    put(a,o+0x1ec,1);put(a,o+0x1f8,1);return !a->memory.fault;
}
static int blocked(void *c,GameAddress o,GameAddress method,int *out){GameApplication *a=c;
    if(method==0x4061d0){*out=0;return !a->memory.fault;} /*Actual XOR EAX,EAX; RET. */
    return method==0x5546a0 && vehicle_targeting_obstructed_arc(&a->memory,o,out);
}
static int line(void *c,const float from[3],const float to[3],int *clear){float hit[3];return game_input_world_line(c,from,to,hit,clear);}
static int auxiliary(void *c,GameAddress o,GameAddress weapon,const uint16_t angles[3],const float matrix[12]){
    GameApplication *a=c;PlayerAttachmentState *state=player_scene_children_attachment(weapon);PlayerAttachmentFrame frame={0};int result;(void)o;
    if(!state)return game_level_raw_auxiliary_fire(a,weapon,angles,matrix);
    frame.tick=get(a,0x5ac6ac);frame.motion_delay=get(a,state->motion+0x20);frame.parent_flags=get(a,state->parent+16);
    memcpy(frame.angles,angles,6);memcpy(frame.world_matrix,matrix,48);result=game_level_player_attachment_update(a,weapon,&frame);
    return result>=0;
}
static int emit_target_shot(GameApplication *a,GameAddress weapon,const float p[3]){
    GameAddress motion=get(a,weapon+20),child,owner=get(a,0x58973c);uint32_t id,flags,slot;uint16_t angles[3]={0};
    child=game_level_factory_take(a,game_memory_read_u8(&a->memory,motion+12),(uint16_t)get(a,motion+16));if(!child)return 0;
    id=game_state_next_owner_sequence(&a->memory,owner);
    if(a->memory.fault || !game_level_projectile_launch(a,child,id,p,angles,motion,get(a,weapon+4),get(a,get(a,child)+0xac),1))goto failed;
    owner=get(a,0x58973c);flags=get(a,child+16);slot=(flags&1)?((flags&64)?0x80:0x1c):0xe4;
    if(a->memory.fault || !level_player_slots_attach_scene_member(owner,child,slot))goto failed;
    game_list_insert_after(&a->memory,child,owner+slot,0x48,0x4c);
    if(!a->memory.fault)return 1;
    level_player_slots_forget_scene_member(child);
failed:(void)game_level_factory_destroy(a,child);return 0;
}
/*43c410 consumes the existing auxiliary record directly. No parallel child
 *state is allocated, and failed acquisitions do not manufacture a shot. */
static int target_fire(GameApplication *a,GameAddress weapon,GameAddress target,int ready){
    GameAddress motion=get(a,weapon+20);uint32_t tick=get(a,0x5ac6ac),remaining;float p[3];SoundPlaybackRequest sound={0};unsigned i;
    if(!get(a,weapon+12))return !a->memory.fault;
    if(integer(get(a,weapon+8)+get(a,motion+0x20))>integer(tick)){put(a,weapon+0xb0,get(a,weapon+0xac));return !a->memory.fault;}
    if(get(a,weapon+0xa8) && integer(get(a,weapon+0xb4)+get(a,weapon+0xb8))>integer(tick))return !a->memory.fault;
    if(ready){GameAddress points=get(a,target+0x40);for(i=0;i<3;i++)p[i]=real(a,(points?points:target+24)+i*4);
        if(!emit_target_shot(a,weapon,p))return 0;
        if(!get(a,weapon+0xa0) && get(a,weapon+0x9c) && !emit_target_shot(a,weapon,p))return 0;
    }
    remaining=get(a,weapon+0xb0)-1u;put(a,weapon+0xb0,remaining);
    if(!get(a,weapon+0xa8) || integer(remaining)<=0)put(a,weapon+8,get(a,0x5ac6ac));
    put(a,weapon+0xb8,get(a,0x5ac6ac));sound.sound=(int16_t)game_memory_read_u16(&a->memory,weapon+16);sound.volume=1;
    {GameAddress parent=get(a,weapon+4),reference=get(a,0x58f890);long double dx=(long double)real(a,parent+24)-real(a,reference+12),dz=(long double)real(a,parent+32)-real(a,reference+20);
        sound.distance_squared=(float)(dz*dz+dx*dx);}
    return !a->memory.fault && game_menu_sound_play(a,&sound);
}
static int class27_program(GameApplication *a,GameAddress o,unsigned slot){
    GameAddress method=get(a,get(a,o)+slot);
    if(method==0x544970){extern int game_level_class27_reposition(GameApplication *,GameAddress);return game_level_class27_reposition(a,o);}
    if(method==0x53c000)return game_level_vehicle_settle(a,o);
    return game_level_vehicle_control(a,o,method);
}
static int class27_retarget(GameApplication *a,GameAddress o,GameAddress target){
    PresentationCommand command={0};
    command.priority=0;command.kind=5;command.target=target;
    command.has_target=1;command.target_identity=get(a,target+20);
    command.has_position=1;
    {unsigned i;for(i=0;i<3;i++)command.position[i]=real(a,target+24+i*4);}
    return game_level_scene_order_issue(a,o,&command,1);
}
static uint32_t class27_random(GameApplication *a,GameAddress scale){
    uint32_t cursor=get(a,0x6308c0),value;
    if(cursor==1024)cursor=0;
    if(cursor>=1024){a->memory.fault=1;return 0;}
    value=truncate_word((long double)real(a,0x6308c8+cursor*4)*real(a,scale));
    put(a,0x6308c0,cursor+1);return value;
}
static uint16_t aircraft_aim_heading(GameApplication *a,float x,float z){
    uint16_t angle;long double scale=real(a,0x577c9c);uint32_t index;
    if(fabsl(x)<fabsl(z)){
        index=truncate_word((long double)x*scale/z+scale);angle=game_memory_read_u16(&a->memory,0x62e8b0+index*2);
        return integer(float_bits(z))>0?angle&2047:(uint16_t)(angle+1024);
    }
    angle=integer(float_bits(x))>0?512:1536;index=truncate_word((long double)z*scale/x+scale);
    return (uint16_t)(angle-game_memory_read_u16(&a->memory,0x62e8b0+index*2));
}
static float class40_sample(GameApplication *a){
    uint32_t cursor=get(a,0x6308c0);float value;
    if(cursor==1024)cursor=0;
    if(cursor>=1024){a->memory.fault=1;return 0;}
    value=real(a,0x6308c8+cursor*4);put(a,0x6308c0,cursor+1);return value;
}
static uint16_t class40_heading(GameApplication *a,float x,float z){
    long double scale=real(a,0x571ecc);uint32_t index;uint16_t angle;
    if(fabsl(x)<fabsl(z)){
        index=truncate_word((long double)x*scale/z+scale);angle=game_memory_read_u16(&a->memory,0x62e8b0+index*2);
        return integer(float_bits(z))>0?angle&2047:(uint16_t)(angle+1024);
    }
    angle=integer(float_bits(x))>0?512:1536;
    index=truncate_word((long double)z*scale/x+scale);
    return (uint16_t)(angle-game_memory_read_u16(&a->memory,0x62e8b0+index*2));
}
static float class40_distance(GameApplication *a,long double dx,long double dz){
    dx=fabsl(dx);dz=fabsl(dz);
    return (float)(dx>dz?(long double)dz*real(a,0x571f18)+dx:(long double)dx*real(a,0x571f18)+dz);
}
static int32_t class40_error(GameApplication *a,GameAddress o){
    uint32_t bits=100u-get(a,o+0x110);
    /*Original arithmetic shift, including out-of-range signed accuracies. */
    int32_t quarter=integer((bits>>2)|((bits&0x80000000u)?0xc0000000u:0));
    return (int16_t)truncate_word((long double)quarter*real(a,0x571ed0));
}
int game_level_class40_profile(GameApplication *a,GameAddress o,GameAddress method){
    uint16_t index;EffectModel *model=effect_model_find(get(a,o+0x54));
    switch(method){case 0x40eaf0:index=0;break;case 0x40eb00:index=1;break;
    case 0x40eb10:index=5;break;case 0x40eb20:index=2;break;
    case 0x40cc40:index=5;put(a,o+0xe0,1);
        game_memory_write_u8(&a->memory,o+0xd8,game_memory_read_u8(&a->memory,o+0xd8)|8);break;
    default:return 0;}
    if(!effect_model_select_profile(&a->memory,model,index,get(a,0x5ac6ac))){
        fprintf(stderr,"native class40 profile failed: object=%08x model=%08x owned=%d bank=%d index=%u profile=%08x fault=%d\n",
            (unsigned)o,(unsigned)get(a,o+0x54),model!=NULL,model && model->bank,index,(unsigned)(model?model->profile:0),a->memory.fault);
        return 0;
    }
    if(method==0x40eb20)game_memory_write_u8(&a->memory,o+0x148,2);
    if(method==0x40cc40)put(a,o+0x13c,get(a,0x5ac6ac)+208u);
    return !a->memory.fault;
}
/*40dbf0: immediate offscreen fire versus delayed visible animation. The gun
 *record, command and billboard retain their existing authoritative owners. */
int game_level_class40_fire(GameApplication *a,GameAddress o,GameAddress target){
    GameAddress table=get(a,o),gun,profile;uint16_t yaw,pitch;
    float dx,dz,dy,distance;uint32_t choice,accuracy=get(a,o+0x110);int32_t error;
    if(a->memory.fault || table!=0x571cc0 || !target)return 0;
    if(!game_terrain_cell_presence(&a->memory,o,target)){
        if(integer(class27_random(a,0x571ea4))<=integer(accuracy))return target_fire(a,get(a,o+0x150),target,1);
        dx=(float)((long double)real(a,target+24)-real(a,o+24));dz=(float)((long double)real(a,target+32)-real(a,o+32));
        distance=class40_distance(a,(long double)real(a,target+24)-real(a,o+24),(long double)real(a,target+32)-real(a,o+32));yaw=class40_heading(a,real(a,target+0x38),distance);
        error=class40_error(a,o);choice=truncate_word((long double)error*class40_sample(a));
        return !a->memory.fault && target_fire(a,get(a,o+0x150),target,integer(choice)<=(int16_t)yaw);
    }
    /*4ca270 publishes this exact target and combat state before aiming. */
    put(a,o+0xe0,2);put(a,o+0xf0,target);pitch=game_memory_read_u16(&a->memory,o+0x144);
    {uint64_t bits=(uint64_t)get(a,0x571ed8)|((uint64_t)get(a,0x571edc)<<32);double stationary;
        memcpy(&stationary,&bits,8);
        if(!(fabsl(real(a,o+0xb8))>stationary)){
            dx=(float)((long double)real(a,target+24)-real(a,o+24));dz=(float)((long double)real(a,target+32)-real(a,o+32));
            yaw=vehicle_targeting_octant(&a->memory,dx,dz);
        }else{
            float origin[3],point[3];unsigned i;
            for(i=0;i<3;i++)origin[i]=real(a,o+24+i*4);
            if(!presentation_target_position(&a->memory,target,origin,point))return 0;
            dx=(float)((long double)point[0]-origin[0]);dz=(float)((long double)point[2]-origin[2]);
            dy=(float)((long double)point[1]-origin[1]);distance=class40_distance(a,(long double)point[0]-origin[0],(long double)point[2]-origin[2]);
            yaw=((float_bits(dx)|float_bits(dz))&0x7fffffff)?class40_heading(a,dx,dz):0;
            if((float_bits(dy)|float_bits(distance))&0x7fffffff){
                pitch=(uint16_t)(0u-class40_heading(a,dy,distance))&2047;
                if(pitch>1024)pitch=(uint16_t)(pitch-2048);
            }else pitch=0;
        }
    }
    if(integer(class27_random(a,0x571ea4))>integer(accuracy)){
        error=class40_error(a,o);
        yaw=(uint16_t)(yaw+truncate_word((long double)(2*error)*class40_sample(a))-error)&2047;
        error=error>=0?error/2:-((-error+1)/2);
        pitch=(uint16_t)(pitch+truncate_word((long double)(2*error)*class40_sample(a))-error)&2047;
        if(pitch>1024)pitch=(uint16_t)(pitch-2048);
    }
    if(get(a,table+0x180)!=0x40cc20 || get(a,table+0x1c0)!=0x40eb00)return 0;
    game_memory_write_u16(&a->memory,o+0x24,yaw);game_memory_write_u16(&a->memory,o+0x144,pitch);
    if(!game_level_class40_profile(a,o,0x40eb00))return 0;
    gun=get(a,o+0x154);
    if(get(a,gun+12)){
        choice=class27_random(a,0x571ee0);target=get(a,o+0xf0);
        if((!choice && game_memory_read_u8(&a->memory,target+12)!=0x40) ||
           ((get(a,target+16)&0x200) && !(choice&~1u)) ||
           (game_memory_read_u8(&a->memory,target+12)==0x27 && !(choice&~3u)) ||
           (game_memory_read_u8(&a->memory,target+12)==0x26 && !(choice&~7u)))put(a,o+0x14c,1);
    }
    profile=get(a,get(a,get(a,o+0x54)+0x110)+4);
    put(a,o+0x13c,get(a,0x5ac6ac)+get(a,profile+8));
    game_memory_write_u8(&a->memory,o+0xd8,game_memory_read_u8(&a->memory,o+0xd8)|8);
    return !a->memory.fault;
}
/*40d590: special command6 arbitration, visibility/weighted RNG gate, fire or
 *a passable perpendicular step. Failed gates retain the original command5. */
int game_level_class40_control(GameApplication *a,GameAddress o){
    GameAddress node=get(a,o+0xdc),target=get(a,o+0xf0);float from[3],to[3],hit[3],dx,dz,stepx,stepz;
    long double distance,range,xdelta,zdelta;float first,second,third;uint32_t combined,trial;int clear;unsigned i;
    if(get(a,node+12)!=6)return game_level_vehicle_settle(a,o);
    if(get(a,o+0xe0)!=5)return !a->memory.fault;
    if(!target)return game_level_vehicle_settle(a,o);
    xdelta=(long double)real(a,o+24)-real(a,target+24);zdelta=(long double)real(a,o+32)-real(a,target+32);
    distance=xdelta*xdelta+zdelta*zdelta;
    trial=get(a,o+0x120);range=(long double)integer(trial*trial)*real(a,0x571ec0);
    if(range<distance)return game_level_vehicle_settle(a,o);
    for(i=0;i<3;i++){from[i]=real(a,o+24+i*4);to[i]=real(a,target+24+i*4);}
    from[1]=(float)((long double)from[1]+real(a,o+0x30));to[1]=(float)((long double)to[1]+real(a,target+0x30));
    if(!game_input_world_line(a,from,to,hit,&clear))return 0;
    if(truncate_word(class40_distance(a,xdelta,zdelta))>4000u)goto retry;
    first=class40_sample(a);second=class40_sample(a);third=class40_sample(a);
    combined=truncate_word((long double)integer(get(a,o+0x114))*third)+
        truncate_word((long double)integer(get(a,o+0x11c)<<2)*second);
    if(integer(combined)<integer(truncate_word((long double)integer(get(a,o+0x118))*first)) || !clear)goto retry;
    if(integer(class27_random(a,0x571ea4))<integer(get(a,o+0x118)) && integer(get(a,o+0x12c))>64)
        return get(a,get(a,o)+0x178)==0x40dbf0 && game_level_class40_fire(a,o,target);
    stepx=(float)(((long double)real(a,target+24)-real(a,o+24))*real(a,0x571ec4));
    stepz=(float)(((long double)real(a,target+32)-real(a,o+32))*real(a,0x571ec4));
    if(class27_random(a,0x571e98)){dx=stepz;dz=-stepx;}else{dx=-stepz;dz=stepx;}
    dx=(float)((long double)real(a,o+24)+dx);dz=(float)((long double)real(a,o+32)+dz);
    if(!scene_order_position_clear(&a->memory,o,dx,dz,&clear))return 0;
    return !clear || game_level_class40_move(a,o,dx,dz);
retry:put(a,node+12,5);return !a->memory.fault;
}
/*55be00 acquires a class27 target and fires the actual owned43bec0 gun.
 *Both aim angles consume the original RNG stream and lookup arithmetic. */
int game_level_aircraft_fire(GameApplication *a,GameAddress o){
    GameAddress gun=get(a,o+0x194),target=get(a,o+0x18c);uint32_t tick=get(a,0x5ac6ac);float delta[3],distance,body[12];
    uint16_t angles[3];unsigned i;long double x,z,range;
    if(!gun)return !a->memory.fault;
    if(target && get(a,target+20)!=get(a,o+0x190)){target=0;put(a,o+0x18c,0);}
    if(integer(get(a,o+0xd4)+32)<integer(tick)){
        GameAddress found=0;
        if(get(a,get(a,o)+0x1ac)!=0x4cbe70 || !acquire(a,o,0x4cbe70,0x4000,&found))return 0;
        if(found && found!=target && game_memory_read_u8(&a->memory,found+12)==0x27){target=found;put(a,o+0x18c,target);put(a,o+0x190,get(a,target+20));}
        put(a,o+0xd4,tick);
    }
    if(!target || integer(get(a,gun+8)+get(a,get(a,gun+20)+0x20))>=integer(tick))return !a->memory.fault;
    x=fabsl((float)((long double)real(a,target+24)-real(a,o+24)));z=fabsl((float)((long double)real(a,target+32)-real(a,o+32)));
    distance=(float)(x<z?x*real(a,0x571f18)+z:z*real(a,0x571f18)+x);
    range=(long double)integer(get(a,o+0x120))*real(a,0x577c98);
    if(isnan(distance) || isnan(range) || range<distance)return !a->memory.fault;
    {float origin[3],point[3];
        for(i=0;i<3;i++)origin[i]=real(a,o+24+i*4);
        if(!presentation_target_position(&a->memory,target,origin,point))return 0;
        for(i=0;i<3;i++)delta[i]=(float)((long double)point[i]-origin[i]);}
    angles[0]=(uint16_t)(aircraft_aim_heading(a,delta[0],delta[2])+class27_random(a,0x577ca0)-30u);
    angles[1]=(uint16_t)((class27_random(a,0x577ca0)-aircraft_aim_heading(a,delta[1],distance)-30u)&2047);
    if(angles[1]>1024)angles[1]=(uint16_t)(angles[1]-2048);
    angles[2]=game_memory_read_u16(&a->memory,o+40);
    for(i=0;i<12;i++)body[i]=real(a,o+0x80+i*4);
    return get(a,get(a,gun)+4)==0x43bec0 && game_level_raw_auxiliary_fire(a,gun,angles,body);
}
/*55b780 follows the published command only after the real gun consumer. */
int game_level_aircraft_follow(GameApplication *a,GameAddress o){
    GameAddress root;uint32_t speed;float threshold;
    if(!game_level_aircraft_fire(a,o))return 0;
    if(get(a,o+0xe0)!=5)return !a->memory.fault;
    root=get(a,0x58973c);
    if(!game_level_player_catalog_value(a,root,0x26,5,(game_memory_read_u8(&a->memory,o+0xd8)&4)!=0,&speed))return 0;
    threshold=(float)((long double)integer(speed)+real(a,0x577c68));
    if(aircraft_near((long double)real(a,o+24)-real(a,o+0xe4),threshold) &&
       aircraft_near((long double)real(a,o+32)-real(a,o+0xec),threshold) &&
       aircraft_near((long double)real(a,o+28)-real(a,o+0xe8),threshold))
        return get(a,get(a,o)+0x168)==0x53c000 && game_level_vehicle_settle(a,o);
    return get(a,get(a,o)+0x174)==0x55b190 && game_level_aircraft_move(a,o,real(a,o+0xe4),real(a,o+0xec));
}
/*55b580 connects bombing timers to55ae60's existing payload/arrival owner. */
int game_level_aircraft_combat(GameApplication *a,GameAddress o){
    GameAddress target,root,local;uint32_t tick=get(a,0x5ac6ac),choice,remaining,speed;float threshold;
    if(!game_level_aircraft_fire(a,o))return 0;
    target=get(a,o+0xf0);
    if(!get(a,o+0x150) && (!target || get(a,target+20)!=get(a,o+0xf4))){
        put(a,o+0xe0,5);return get(a,get(a,o)+0x168)==0x53c000 && game_level_vehicle_settle(a,o);
    }
    root=get(a,0x58973c);if(!game_level_player_catalog_choice(a,root,0x26,&choice))return 0;
    if(get(a,o+0x164)+get(a,0x5a0818+choice*40u)==tick){
        put(a,o+0x154,tick);local=get(a,0x5f0208);
        if(integer(tick-get(a,local+0x58c))>96 && (get(a,o+20)&63)==game_memory_read_u16(&a->memory,local+8)){
            SoundPlaybackRequest sound={0};put(a,local+0x58c,tick);game_memory_write_u16(&a->memory,local+0x598,51);
            sound.sound=51;sound.volume=1;if(!game_menu_sound_play(a,&sound))return 0;
        }
    }
    if(get(a,o+0x154)==get(a,0x5ac6ac)){
        if(get(a,get(a,o)+0x1c0)!=0x55ae60 || !game_level_vehicle_control(a,o,0x55ae60))return 0;
        remaining=get(a,o+0x158)-1u;put(a,o+0x158,remaining);
        if(!remaining)return get(a,get(a,o)+0x168)==0x53c000 && game_level_vehicle_settle(a,o);
        if(!game_level_player_catalog_choice(a,get(a,0x58973c),0x26,&choice))return 0;
        put(a,o+0x154,get(a,0x5ac6ac)+get(a,0x5a0818+(get(a,o+0x160)+choice*10u-remaining)*4u));
    }
    if(get(a,o+0xe0)!=5)return !a->memory.fault;
    root=get(a,0x5f0210+(get(a,o+20)&63)*4);
    if(!game_level_player_catalog_value(a,root,0x26,5,(game_memory_read_u8(&a->memory,o+0xd8)&4)!=0,&speed))return 0;
    threshold=(float)((long double)integer(speed)+real(a,0x577c68));
    if(aircraft_near((long double)real(a,o+24)-real(a,o+0x144),threshold) && aircraft_near((long double)real(a,o+32)-real(a,o+0x14c),threshold))
        return get(a,get(a,o)+0x168)==0x53c000 && game_level_vehicle_settle(a,o);
    return get(a,get(a,o)+0x174)==0x55b190 && game_level_aircraft_move(a,o,real(a,o+0x144),real(a,o+0x14c));
}
static int class27_angles_error(GameApplication *a,GameAddress o){
    int pitch=(int16_t)game_memory_read_u16(&a->memory,o+0x26)-(int16_t)game_memory_read_u16(&a->memory,o+0x14e);
    int yaw=(int16_t)game_memory_read_u16(&a->memory,o+0x24)-(int16_t)game_memory_read_u16(&a->memory,o+0x14c);
    return (pitch<0?-pitch:pitch)+(yaw<0?-yaw:yaw);
}
/*545680 uses two target points: the origin and its raised aim point. The
 *larger distance drives avoidance; weapon ranges use the origin distance. */
static float class27_distance(GameApplication *a,GameAddress o,GameAddress target,float *origin_squared){
    long double x=(long double)real(a,o+24)-real(a,target+24),z=(long double)real(a,o+32)-real(a,target+32);
    long double y=(long double)real(a,o+28)-real(a,target+28);
    float raised=(float)((long double)real(a,target+28)+real(a,target+0x30));
    long double aim_y=(long double)real(a,o+28)-raised,aim=(aim_y*aim_y+z*z)+x*x;
    *origin_squared=(float)((y*y+x*x)+z*z);
    return aim>*origin_squared?(float)aim:*origin_squared;
}
static int class27_protected_target(GameApplication *a,GameAddress target){
    int count=(int16_t)game_memory_read_u16(&a->memory,0x639a74),i,j;
    float x=real(a,target+24),z=real(a,target+32);
    for(i=0;i<count;i++){
        GameAddress region=get(a,get(a,0x639a70)+(uint32_t)i*4),rectangles;
        int rectangles_count;
        if(!(real(a,region+0x38)<=x && !(real(a,region+0x40)<x) &&
             real(a,region+0x3c)<=z && !(real(a,region+0x44)<z)))continue;
        rectangles_count=(int16_t)game_memory_read_u16(&a->memory,region+0x34);
        if(!rectangles_count)return 1;
        rectangles=get(a,region+0x30);
        for(j=0;j<rectangles_count;j++){
            GameAddress r=rectangles+(uint32_t)j*16;
            if(real(a,r)<=x && !(real(a,r+8)<x) && real(a,r+4)<=z && !(real(a,r+12)<z))return 1;
        }
    }
    return 0;
}
static int class27_gun_matrix(GameApplication *a,GameAddress o,GameAddress weapon,const float matrix[12]){
    (void)o;
    return game_level_attachment_transform(a,weapon,get(a,get(a,weapon)),matrix);
}
/*544970 selects one of eight real candidate directions, requiring both terrain
 *lines and the class-specific bounds test. Failed searches keep the original
 *random fallback, rather than pretending that a destination was found. */
int game_level_class27_reposition(GameApplication *a,GameAddress o){
    static const int xdir[8]={1,0,-1,-1,-1,0,1,1},zdir[8]={1,1,1,0,-1,-1,-1,0};
    extern int game_level_class27_target_changed(GameApplication *,GameAddress);
    float target[3],from[3],p[3],hit[3],height;GameAddress other=get(a,o+0xf0);unsigned i,trial;int direction,clear;
    if(!other)return 0;
    height=(float)(gameplay_geometry_terrain_height(&a->memory,real(a,o+24),real(a,o+32))+real(a,0x59d8a8));
    put(a,o+0x150,float_bits(height));
    if(!game_level_class27_target_changed(a,o))return 0;
    for(i=0;i<3;i++){target[i]=real(a,other+24+i*4);from[i]=real(a,o+24+i*4);}
    for(trial=0;trial<8;trial++){
        uint32_t row=class27_random(a,0x576fcc);float radius;long double ground,lower,upper,dx,dz,distance;
        if(row>=8){a->memory.fault=1;return 0;}
        if(get(a,o+0x16c)!=UINT32_MAX && class27_random(a,0x576fcc)){
            direction=integer(get(a,o+0x16c));direction+=direction>=4?-4:4;put(a,o+0x16c,UINT32_MAX);
        }else direction=integer(get(a,0x576e80+(row*8+trial)*4));
        if(direction<0 || direction>=8){a->memory.fault=1;return 0;}
        radius=real(a,o+0x174);if(direction&1)radius=(float)((long double)radius*real(a,0x576fd0));
        p[0]=(float)((long double)xdir[direction]*radius+from[0]);p[2]=(float)((long double)zdir[direction]*radius+from[2]);
        ground=gameplay_geometry_terrain_height(&a->memory,p[0],p[2])+real(a,0x59d8a8);
        p[1]=ground>height?(float)ground:height;
        dx=(long double)p[0]-target[0];dz=(long double)p[2]-target[2];
        if((long double)integer(get(a,o+0x120)*get(a,o+0x120))*real(a,0x576fd4)<dz*dz+dx*dx)continue;
        if(get(a,get(a,o)+0x188)!=0x544900)return 0;
        lower=(long double)real(a,o+0x38)*real(a,0x576fa8);
        upper=(long double)real(a,o+0x38)*real(a,0x576fac)+real(a,0x576fb0);
        if(!(lower<=p[0]) || upper<p[0] || p[2]<(float)lower || !(p[2]<=(float)upper))continue;
        if(!game_input_world_line(a,p,target,hit,&clear))return 0;
        if(!clear)continue;
        if(!game_input_world_line(a,from,p,hit,&clear))return 0;
        if(!clear)continue;
        dx=fabsl((float)((long double)p[0]-from[0]));dz=fabsl((float)((long double)p[2]-from[2]));
        distance=dz<dx?dz*real(a,0x571f18)+dx:dx*real(a,0x571f18)+dz;
        if(!(distance<real(a,0x576fc8)))continue;
        put(a,o+0x16c,(uint32_t)direction);
        return get(a,get(a,o)+0x1c0)==0x543bb0 && game_level_class27_destination(a,o,p);
    }
    {uint32_t zcursor=get(a,0x6308c0);int32_t xoffset;
        if(zcursor==1024)zcursor=0;
        if(zcursor>=1024){a->memory.fault=1;return 0;}
        /*Retail truncates the first draw, subtracts500, converts that integer
         *again and subtracts another500 before adding X. */
        xoffset=integer(class27_random(a,0x576fd8)-1000u);
        /*The second draw adds its float directly to Z, before FTOL. */
        {uint32_t cursor=get(a,0x6308c0);if(cursor==1024)cursor=0;
            if(cursor>=1024){a->memory.fault=1;return 0;}
            p[2]=(float)((long double)real(a,0x6308c8+cursor*4)*real(a,0x576fd8)+from[2]);put(a,0x6308c0,cursor+1);}
        p[0]=(float)((long double)xoffset+from[0]);p[1]=(float)((long double)from[1]+real(a,0x576fdc));
    }
    if(get(a,get(a,o)+0x1c0)!=0x543bb0 || !game_level_class27_destination(a,o,p))return 0;
    put(a,o+0x16c,UINT32_MAX);return !a->memory.fault;
}
/*Full combat consumer545680. Guns remain the factory's owned188-byte records;
 *all emissions share the existing projectile factory and faction publication. */
int game_level_class27_combat(GameApplication *a,GameAddress o){
    GameAddress target=get(a,o+0xf0),gun;uint32_t tick=get(a,0x5ac6ac);float distance,origin,ground,body[12];
    int valid=target && get(a,target+20)==get(a,o+0xf4),aligned,close,clear=1;unsigned i;
    if(get(a,o+0xe0)==4 && valid){
        float from[3],to[3],hit[3];
        for(i=0;i<3;i++){from[i]=real(a,o+24+i*4);to[i]=real(a,target+24+i*4);}
        if((tick&1) && !game_input_world_line(a,from,to,hit,&clear))return 0;
        ground=(float)gameplay_geometry_terrain_height(&a->memory,from[0],from[2]);
        distance=class27_distance(a,o,target,&origin);
        if(integer(get(a,o+0x148))<=0 &&
            ((long double)real(a,o+0x170)*real(a,o+0x170)>distance || !clear ||
             (long double)real(a,o+0x30)*real(a,0x576f9c)>fabsl((long double)ground-from[1])))
            return class27_program(a,o,0x1c8);
        aligned=class27_angles_error(a,o)<11;close=integer(float_bits(origin))<0x4c095440;
        for(i=0;i<12;i++)body[i]=real(a,o+0x80+i*4);
        gun=get(a,o+0x13c);
        if(gun){
            GameAddress motion=get(a,gun+20);long double range=(long double)integer(get(a,motion+0x24))*real(a,motion+0x1c);
            int ready=integer(get(a,gun+8)+get(a,motion+0x20))<=integer(tick);
            if(!ready && integer(get(a,gun+0xb0))<=0)put(a,gun+0xb0,get(a,gun+0xac));
            if(aligned && !(range*(float)range<origin) && ready &&
               !(get(a,o+0x144) && (game_memory_read_u8(&a->memory,target+12)==0x40 || close))){
                if(game_memory_read_u8(&a->memory,target+12)==0x26 || game_terrain_cell_presence(&a->memory,o,target)){
                    if(!class27_gun_matrix(a,o,gun,body))return 0;
                }else if(!target_fire(a,gun,target,integer(class27_random(a,0x577000))<(integer(float_bits(origin))>0x4c095440?25:75)))return 0;
            }
        }
        gun=get(a,o+0x140);
        if(aligned && !(get(a,target+16)&0x4000) && gun &&
           ((long double)ground-real(a,o+28)<real(a,0x576fdc)) && integer(float_bits(origin))>0x4c3aeb90 &&
           integer(get(a,gun+8)+get(a,get(a,gun+20)+0x20))<integer(tick) && !class27_protected_target(a,target)){
            if(game_terrain_cell_presence(&a->memory,o,target)){
                if(!class27_gun_matrix(a,o,gun,body))return 0;
            }else if(!target_fire(a,gun,target,integer(class27_random(a,0x576fa0))<8))return 0;
        }
        gun=get(a,o+0x144);
        if(class27_angles_error(a,o)<56 && gun && !(get(a,target+16)&0x8000) &&
           (game_memory_read_u8(&a->memory,target+12)==0x40 || close)){
            uint16_t angles[3];int original_pitch,pitch,rate;
            if(!game_matrix_extract_angles(&a->memory,body,angles))return 0;
            original_pitch=(int16_t)angles[1];
            angles[0]=(uint16_t)(angles[0]+(uint16_t)class27_random(a,0x577008)-15u);
            rate=(int16_t)truncate_word((long double)real(a,game_memory_read_u8(&a->memory,target+12)==0x40?0x57700c:0x577010)*real(a,0x577014));
            pitch=(original_pitch+integer(truncate_word((long double)origin*real(a,0x577018)*rate)))&2047;
            angles[1]=(uint16_t)pitch;
            if(game_terrain_cell_presence(&a->memory,o,target)){
                if(get(a,get(a,gun)+4)!=0x43bec0 || !game_level_raw_auxiliary_fire(a,gun,angles,body))return 0;
            }else{
                int delta=pitch-original_pitch,threshold=(delta<0?-delta:delta)<28?7:1;
                if(!target_fire(a,gun,target,integer(class27_random(a,0x576fa0))<threshold))return 0;
            }
        }
        for(i=0;i<3;i++)if(!(real(a,o+0x154+i*4)==real(a,target+24+i*4))){
            if((tick&3) && !class27_program(a,o,0x1c4))return 0;
            break;
        }
    }
    if(get(a,o+0xe0)!=5)return !a->memory.fault;
    target=get(a,o+0xf0);
    if(!target || get(a,target+20)!=get(a,o+0xf4))return class27_program(a,o,0x168);
    if(integer(get(a,o+0xd4)+64u)<integer(tick)){
        GameAddress found=0;put(a,o+0xd4,tick);
        if(!game_level_vehicle_target_acquire(a,o,1,&found))return 0;
        if(found && found!=target)return class27_retarget(a,o,found);
    }
    ground=(float)gameplay_geometry_terrain_height(&a->memory,real(a,o+24),real(a,o+32));
    distance=class27_distance(a,o,target,&origin);
    if(!((long double)real(a,o+0x170)*real(a,o+0x170)>distance) &&
       !((long double)real(a,o+0x30)*real(a,0x576fa8)>fabsl((long double)ground-real(a,o+28))))
        return class27_program(a,o,0x1c4);
    return class27_program(a,o,0x1c8);
}
/*546100: refresh acquisition, then complete or republish the command's saved
 *destination. Ordered tests preserve the original unordered comparison bits. */
int game_level_class27_timed_refresh(GameApplication *a,GameAddress o){
    uint32_t tick=get(a,0x5ac6ac),value;float tolerance,p[3],ground;unsigned i;
    if(integer(get(a,o+0xd4)+16u)<integer(tick)){
        GameAddress target=0;
        if(!game_level_vehicle_target_acquire(a,o,1,&target))return 0;
        put(a,o+0xd4,get(a,0x5ac6ac));
        if(target)return class27_retarget(a,o,target);
    }
    if(get(a,o+0xe0)!=5)return !a->memory.fault;
    if(get(a,get(a,o)+0x60)!=0x53c550 ||
       !game_level_player_catalog_value(a,get(a,0x58973c),game_memory_read_u8(&a->memory,o+12),5,
          (game_memory_read_u8(&a->memory,o+0xd8)&4)!=0,&value))return 0;
    tolerance=(float)((long double)integer(value)+real(a,0x576f90));
    for(i=0;i<3;i++)p[i]=real(a,o+0xe4+i*4);
    for(i=0;i<3;i++)if(fabsl((long double)real(a,o+24+i*4)-p[i])>=tolerance)break;
    if(i==3)return class27_program(a,o,0x168);
    ground=(float)gameplay_geometry_terrain_height(&a->memory,p[0],p[2]);
    if(get(a,o+0xe8)==0x47000000)p[1]=(float)((long double)ground+real(a,0x59d8a8));
    else if((long double)ground+real(a,0x576fb8)>p[1])p[1]=(float)((long double)ground+real(a,0x576fb8));
    put(a,o+0xe8,float_bits(p[1]));
    return get(a,get(a,o)+0x1c0)==0x543bb0 && game_level_class27_destination(a,o,p);
}
static long double class27_planar(GameApplication *a,const float from[3],const float to[3]){
    float x=(float)((long double)from[0]-to[0]),z=(float)((long double)from[2]-to[2]);
    if(x<real(a,0x571f14))x=-x;
    if(z<real(a,0x571f14))z=-z;
    return z<x?(float)((long double)z*real(a,0x571f18)+x):(float)((long double)x*real(a,0x571f18)+z);
}
static int class27_speed(GameApplication *a,GameAddress o,int32_t *out){
    uint32_t value;GameAddress root=get(a,0x5f0210+(get(a,o+20)&63u)*4);
    if(get(a,get(a,o)+0x140)!=0x539a30 || !game_level_player_catalog_value(a,root,
       game_memory_read_u8(&a->memory,o+12),5,(game_memory_read_u8(&a->memory,o+0xd8)&4)!=0,&value))return 0;
    *out=integer(value);return !a->memory.fault;
}
/*544d50: follow a valid moving target, periodically acquiring a replacement.
 *Its planar movement uses the original atan/sine tables and signed quadrant. */
int game_level_class27_follow_move(GameApplication *a,GameAddress o){
    GameAddress target;uint32_t tick=get(a,0x5ac6ac),index;float from[3],p[3],dx,dz,ratio;uint16_t yaw;
    int sign=1;int32_t speed;unsigned i;long double angle_value;
    if(integer(get(a,o+0xd4)+16u)<integer(tick)){
        GameAddress found=0;if(!game_level_vehicle_target_acquire(a,o,1,&found))return 0;
        put(a,o+0xd4,get(a,0x5ac6ac));if(found && found!=get(a,o+0xf0))return class27_retarget(a,o,found);
    }
    if(get(a,o+0xe0)!=5)return !a->memory.fault;
    target=get(a,o+0xf0);
    if(!target || get(a,target+20)!=get(a,o+0xf4))return class27_program(a,o,0x168);
    for(i=0;i<3;i++){from[i]=real(a,o+24+i*4);p[i]=real(a,target+24+i*4);put(a,o+0xe4+i*4,float_bits(p[i]));}
    if(class27_planar(a,from,p)<real(a,0x576fe0))return class27_program(a,o,0x168);
    dx=(float)((long double)p[0]-from[0]);dz=(float)((long double)p[2]-from[2]);
    if(!(float_bits(dx)&0x7fffffff)){uint32_t bits=0x3a83126f;memcpy(&dx,&bits,4);}
    if(float_bits(dx)>0x80000000)sign=-1;
    ratio=(float)((long double)dz/dx);
    if(fabsl((long double)dz/dx)<real(a,0x576f90)){
        index=truncate_word((long double)ratio*real(a,0x576fc4)+real(a,0x576fc4));
        yaw=game_memory_read_u16(&a->memory,0x62e8b0+index*2)&2047;
    }else{
        yaw=integer(float_bits(ratio))>0?512:1536;
        angle_value=(long double)dx/dz*real(a,0x576fc4)+real(a,0x576fc4);
        index=truncate_word(angle_value);yaw=(uint16_t)(yaw-game_memory_read_u16(&a->memory,0x62e8b0+index*2));
    }
    if(!class27_speed(a,o,&speed))return 0;
    p[0]=(float)((long double)from[0]+(float)((long double)speed*real(a,0x6298a8+((yaw+512u)&2047)*4)*sign*real(a,0x576fe8)));
    p[2]=(float)((long double)speed*real(a,0x6298a8+(yaw&2047)*4)*sign*real(a,0x576fe8)+from[2]);
    if(get(a,get(a,o)+0x174)!=0x543ea0 || get(a,get(a,o)+0x1c0)!=0x543bb0)return 0;
    p[1]=(float)(gameplay_geometry_terrain_height(&a->memory,p[0],p[2])+real(a,0x59d8a8));
    return game_level_class27_destination(a,o,p);
}
static int class27_sound(GameApplication *a,GameAddress o,int sound,int looping){
    GameAddress reference=get(a,0x58f890);SoundPlaybackRequest request={0};
    long double x=(long double)real(a,o+24)-real(a,reference+12),z=(long double)real(a,o+32)-real(a,reference+20);
    request.distance_squared=(float)(x*x+z*z);request.sound=(int16_t)sound;request.volume=1;
    request.flags=request.force=looping;return !a->memory.fault && game_menu_sound_play(a,&request);
}
/*546f50: the five-stage special ascent owns its phase/timer and voice in the
 *existing class tail. Movement's phase7 consumes the requested ascent steps. */
int game_level_class27_ascent(GameApplication *a,GameAddress o){
    uint32_t stage=get(a,o+0x130),timer;long double ground,threshold;int finished=0;
    if(stage>4)return !a->memory.fault;
    if(stage==3){timer=get(a,o+0x134)-1u;put(a,o+0x134,timer);if(!timer)put(a,o+0x130,stage+1);return !a->memory.fault;}
    if(stage==4){
        game_memory_write_u16(&a->memory,o+0x26,0);
        ground=gameplay_geometry_terrain_height(&a->memory,real(a,o+24),real(a,o+32));
        if((long double)real(a,o+28)-real(a,0x576fb8)<ground)return class27_program(a,o,0x168);
        if(get(a,o+0xe0)==7)return !a->memory.fault;
        if(!(ground+real(a,0x59d8a8)>real(a,o+28)))return class27_program(a,o,0x168);
        if(!class27_sound(a,o,17,0))return 0;put(a,o+0xe0,7);return !a->memory.fault;
    }
    if(get(a,o+0xe0)==7)return !a->memory.fault;
    if(integer(get(a,o+0x178))<0){
        if(!class27_sound(a,o,129,1))return 0;put(a,o+0x178,get(a,get(a,0x628a90)));
    }
    if(stage==0){GameAddress target=get(a,get(a,o+0xdc)+16);
        ground=gameplay_geometry_terrain_height(&a->memory,real(a,o+24),real(a,o+32));
        threshold=ground+(long double)real(a,target+0x2c)*real(a,0x577038);
        finished=!(threshold>real(a,o+28));
    }else if(stage==1)finished=(int16_t)game_memory_read_u16(&a->memory,o+0x26)>real(a,0x57703c);
    else finished=!get(a,o+0x134);
    if(!finished){put(a,o+0xe0,7);return !a->memory.fault;}
    put(a,o+0x130,stage+1);
    if(stage==1 || stage==2)put(a,o+0x134,16);
    if(stage==2)put(a,o+0xe0,5);
    if(!game_menu_sound_stop(a,129,get(a,o+0x178)) || !class27_sound(a,o,134,0))return 0;
    put(a,o+0x178,UINT32_MAX);return !a->memory.fault;
}
/*5472e0 consumes ascent's phase7 request, including the timed horizontal leg,
 *landing particles, boundary rollback and the existing looping voice. */
int game_level_class27_ascent_step(GameApplication *a,GameAddress o){
    uint32_t stage,voice=get(a,o+0x178),bank=get(a,0x628a90);uint16_t yaw;unsigned i;
    float p[3],saved[3];int32_t speed;long double lower,upper,ground;
    game_memory_write_u8(&a->memory,o+0xd8,game_memory_read_u8(&a->memory,o+0xd8)|8);
    if(integer(voice)>=0 && get(a,bank+0x468) && !(get(a,0x5ac6ac)&3)){
        int playing;if(!game_menu_sound_playing(a,129,voice,&playing))return 0;
        if(playing){GameAddress reference=get(a,0x58f890);
            long double x=(long double)real(a,o+24)-real(a,reference+12),z=(long double)real(a,o+32)-real(a,reference+20);
            float volume=sound_playback_attenuation(&a->memory,get(a,bank+0x210),(float)(x*x+z*z));
            if(!game_menu_sound_volume(a,129,voice,volume))return 0;
        }
    }
    stage=get(a,o+0x130);if(stage>4 || stage==3)return !a->memory.fault;
    if(stage==0){GameAddress target=get(a,get(a,o+0xdc)+16);
        put(a,o+28,float_bits((float)((long double)real(a,o+28)+real(a,0x577040))));
        if(!((long double)real(a,target+28)+real(a,0x576fb8)>real(a,o+28)))put(a,o+0xe0,5);
        return !a->memory.fault;
    }
    if(stage==1){uint16_t pitch=game_memory_read_u16(&a->memory,o+0x26)+10u;
        game_memory_write_u16(&a->memory,o+0x26,pitch);
        if((int16_t)pitch>real(a,0x57703c))put(a,o+0xe0,5);return !a->memory.fault;
    }
    yaw=game_memory_read_u16(&a->memory,o+0x24);
    if(stage==2){
        put(a,o+24,float_bits((float)((long double)real(a,0x6298a8+(yaw&2047)*4)*real(a,0x577044)+real(a,o+24))));
        put(a,o+32,float_bits((float)((long double)real(a,0x6298a8+((yaw+512u)&2047)*4)*real(a,0x577044)+real(a,o+32))));
        put(a,o+0x134,get(a,o+0x134)-1u);if(!get(a,o+0x134))put(a,o+0xe0,5);return !a->memory.fault;
    }
    for(i=0;i<3;i++)saved[i]=real(a,o+24+i*4);
    game_memory_write_u16(&a->memory,o+0x26,0);
    if(!game_level_surface_particles(a,o,saved,1,0x6297d8) || !class27_speed(a,o,&speed))return 0;
    p[0]=(float)((long double)speed*real(a,0x6298a8+(yaw&2047)*4)*real(a,0x576fa8)+saved[0]);
    p[2]=(float)((long double)speed*real(a,0x6298a8+((yaw+512u)&2047)*4)*real(a,0x576fa8)+saved[2]);
    p[1]=(float)((long double)speed*real(a,0x577048)+saved[1]);
    lower=(long double)real(a,o+0x38)*real(a,0x576fa8);upper=(long double)real(a,o+0x38)*real(a,0x576fac)+real(a,0x576fb0);
    if(!(lower<p[0]) || !(upper>p[0]) || !(p[2]>(float)lower) || !(p[2]<(float)upper)){
        put(a,o+0xe0,5);return !a->memory.fault;
    }
    for(i=0;i<3;i++)put(a,o+24+i*4,float_bits(p[i]));
    ground=gameplay_geometry_terrain_height(&a->memory,p[0],p[2])+real(a,0x59d8a8);
    if(!(ground>p[1]))put(a,o+0xe0,5);
    return !a->memory.fault;
}
/*544500: after collision marks17c, preserve a live attack or issue the real
 *kind12 hold command at the current position. */
int game_level_class27_react(GameApplication *a,GameAddress o){
    GameAddress node=get(a,o+0xdc),target;uint32_t kind=get(a,node+12);unsigned i;
    if(!get(a,o+0x17c) || kind==11)return !a->memory.fault;
    target=get(a,node+16);
    if(get(a,o+0x58) && kind!=12 && kind>=5 && kind<=7 && target && get(a,target+20)==get(a,node+20)){
        put(a,o+0x164,0);put(a,o+0x168,0);return class27_program(a,o,0x1c8);
    }
    if(get(a,o+0x58) && kind!=12){PresentationCommand command={0};command.kind=12;command.has_position=1;
        for(i=0;i<3;i++)command.position[i]=real(a,o+24+i*4);
        put(a,o+0x164,0);put(a,o+0x168,0);return game_level_scene_order_issue(a,o,&command,0);
    }
    return !a->memory.fault;
}
static int class27_attack_order(GameApplication *a,GameAddress o){
    uint32_t kind=get(a,get(a,o+0xdc)+12);return kind>=5 && kind<=7;
}
static int class27_idle_point(GameApplication *a,GameAddress o,uint32_t priority,const float point[3]){
    PresentationCommand command={0};command.priority=priority&255;command.has_position=1;
    memcpy(command.position,point,12);return game_level_scene_order_issue(a,o,&command,0);
}
/*544f90: idle acquisition, root-selected enemy, leader/peer command sharing,
 *following saved destinations, then the original bounded wandering choices. */
int game_level_class27_idle(GameApplication *a,GameAddress o){
    GameAddress found=0,root=get(a,0x58973c),selected,leader,peer,node,target;
    uint32_t tick=get(a,0x5ac6ac),priority;unsigned i,visits=0;float from[3],p[3]={0,32768,0};
    if(integer(get(a,o+0xd4)+16u)<integer(tick)){
        if(!game_level_vehicle_target_acquire(a,o,1,&found))return 0;
        put(a,o+0xd4,get(a,0x5ac6ac));
    }
    if(found)return class27_retarget(a,o,found);
    node=get(a,o+0xdc);priority=get(a,node+40);
    for(i=0;i<3;i++)from[i]=real(a,o+24+i*4);
    /* Only player catalogs own selection state; AI/rabble catalogs end earlier. */
    if(game_memory_read_u16(&a->memory,root+8)<4 &&
       (selected=get(a,root+0x3c4)) && get(a,selected+20)==get(a,root+0x3c8)){
        NativeEntityRelationshipResult relation;
        if(!native_entity_relationship_query(&a->memory,selected,o,&relation))return 0;
        if(relation.decision>=NATIVE_ENTITY_RELATION_ACCEPT_NULL_SECOND){
            long double x=(long double)from[0]-real(a,selected+24),z=(long double)from[2]-real(a,selected+32);
            if(z*z+x*x<real(a,0x576fec))return game_level_scene_identity_order(a,o,priority,get(a,root+0x3c8));
        }
    }
    leader=get(a,o+0x108);
    if(leader && game_memory_read_u8(&a->memory,leader+12)!=0xfe){
        while(get(a,leader+0x108) && game_memory_read_u8(&a->memory,get(a,leader+0x108)+12)!=0xfe){
            if(++visits>65536)return 0;leader=get(a,leader+0x108);
        }
        {GameAddress command=get(a,leader+0xdc);uint32_t kind=get(a,command+12);
            if(class27_attack_order(a,leader) || kind==13){
                target=kind==13?get(a,command+16):get(a,leader+0xf0);
                if(!target)return !a->memory.fault;
                if(get(a,target+20)==get(a,leader+0xf4))return game_level_scene_target_order(a,o,priority,target);
            }
        }
        node=get(a,o+0xdc);target=get(a,node+16);
        if(!target && !(integer(get(a,node+24))>0 && integer(get(a,node+32))>0)){
            p[0]=real(a,leader+24);p[2]=real(a,leader+32);
            if(class27_planar(a,from,p)>real(a,0x576ff0))return class27_idle_point(a,o,priority,p);
            goto wander;
        }
    }else{
        peer=get(a,o+0x10c);
        while(peer){
            if(++visits>65536)return 0;
            if(!get(a,peer+0xdc))return 0; /*Malformed peer cleanup remains an explicit boundary. */
            if(class27_attack_order(a,peer))return game_level_scene_target_order(a,o,priority,get(a,get(a,peer+0xdc)+16));
            peer=get(a,peer+0x10c);
        }
        node=get(a,o+0xdc);target=get(a,node+16);
        if(!target && !(integer(get(a,node+24))>0 && integer(get(a,node+32))>0)){
            if(leader){
                GameAddress method=get(a,get(a,leader)+0x154);
                if(method==0x53ba50){
                    if(!game_level_scene_replace_command(a,leader,o,(get(a,leader+20)&63)<4))return 0;
                }else{
                    /*545333 also calls the formation owner.4be000 clears
                     *its actual reservation; it does not own a unit command. */
                    GameplayPeerServices peers={a,NULL};
                    if(method!=0x4be000 || !gameplay_state_peer_method(&a->memory,leader,o,method,&peers))return 0;
                }
            }
            goto wander;
        }
    }
    if(target){
        if(get(a,target+20)!=get(a,node+20))return class27_program(a,o,0x168);
        p[0]=real(a,target+24);p[2]=real(a,target+32);
    }else{p[0]=real(a,node+24);p[2]=real(a,node+32);}
    if(class27_planar(a,from,p)>real(a,0x576ff0))return class27_idle_point(a,o,priority,p);
wander:
    if(get(a,o+0xe0)!=5)return !a->memory.fault;
    {int yaw=(int16_t)game_memory_read_u16(&a->memory,o+0x24);float x=0,z=0;
        static const int xs[4]={1,-1,-1,1},zs[4]={1,-1,1,-1};
        if(yaw<1 || yaw>2047)x=8000;
        else if(yaw==1536)z=8000;
        else if(yaw==1024)x=-8000;
        else z=-8000;
        if(get(a,get(a,o)+0x188)!=0x544900 || get(a,get(a,o)+0x1c0)!=0x543bb0)return 0;
        for(i=0;i<4;i++){
            long double lower=(long double)real(a,o+0x38)*real(a,0x576fa8);
            long double upper=(long double)real(a,o+0x38)*real(a,0x576fac)+real(a,0x576fb0);
            p[0]=(float)((long double)from[0]+xs[i]*x);p[2]=(float)((long double)from[2]+zs[i]*z);
            if(!(lower<=p[0]) || upper<p[0] || p[2]<(float)lower || !(p[2]<=(float)upper))continue;
            p[1]=(float)(gameplay_geometry_terrain_height(&a->memory,from[0],from[2])+real(a,0x59d8a8));
            put(a,o+0x150,float_bits(p[1]));return game_level_class27_destination(a,o,p);
        }
    }
    return !a->memory.fault;
}
/*544730: timed hold/escape command after an obstacle. The six original
 *direction pairs are immutable data; the last attempted pair is retained. */
int game_level_class27_escape(GameApplication *a,GameAddress o){
    float p[3];long double lower,upper,x,z;uint32_t tick=get(a,0x5ac6ac);unsigned attempt;
    if(integer(get(a,o+0x134))>integer(tick))return !a->memory.fault;
    if(integer(get(a,o+0x130))>0 && !class27_program(a,o,0x168))return 0;
    lower=(float)((long double)real(a,o+0x38)*real(a,0x576fa8));
    upper=(float)((long double)real(a,o+0x38)*real(a,0x576fac)+real(a,0x576fb0));
    for(attempt=0;attempt<6;attempt++){
        x=(long double)integer(get(a,0x59d8b0+attempt*8))*real(a,0x576fc0)*real(a,o+0xb4)+real(a,o+24);
        if(!(x>lower && upper>x))continue;
        z=(long double)integer(get(a,0x59d8b4+attempt*8))*real(a,0x576fc0)*real(a,o+0xb8)+real(a,o+32);
        if(z>lower && z<upper)break;
    }
    if(attempt<6){
        p[0]=(float)((long double)integer(get(a,0x59d8b0+attempt*8))*real(a,o+0xb4)*real(a,0x576fc0)+real(a,o+24));
        p[2]=(float)((long double)integer(get(a,0x59d8b4+attempt*8))*real(a,o+0xb8)*real(a,0x576fc0)+real(a,o+32));
    }else{p[0]=(float)((long double)real(a,o+24)+real(a,0x576f90));p[2]=(float)((long double)real(a,o+32)+real(a,0x576f90));}
    p[1]=(float)((long double)real(a,o+28)+real(a,0x576fc4));
    if(get(a,get(a,o)+0x1c0)!=0x543bb0 || !game_level_class27_destination(a,o,p))return 0;
    put(a,o+0x134,get(a,0x5ac6ac)+96u);put(a,o+0x130,get(a,o+0x130)+1u);return !a->memory.fault;
}
/*409330: class24 composes articulation, attachment and world transforms before
 *the actual gun update; successful aircraft hits invoke its payload release. */
int game_level_vehicle_fire_class24(GameApplication *a,GameAddress o,GameAddress target){
    float articulation[12],attachment[12],world[12],local[12];
    GameAddress gun=get(a,o+0x21c),motion,parent;PlayerAttachmentFrame frame={0};unsigned i,row,column;int status;
    for(i=0;i<12;i++){articulation[i]=real(a,o+0x150+i*4);attachment[i]=real(a,o+0x180+i*4);world[i]=real(a,o+0x80+i*4);}
    /*The inline retail composition sums X,Y,Z;4033e0 below uses its shared
     *reverse sum. Keep both stored-float boundaries. */
    for(row=0;row<3;row++)for(column=0;column<4;column++){
        unsigned base=row*4;long double value=(long double)attachment[base]*articulation[column];
        value+=(long double)attachment[base+1]*articulation[4+column];
        value+=(long double)attachment[base+2]*articulation[8+column];
        if(column==3)value+=attachment[base+3];local[base+column]=(float)value;
    }
    game_matrix_compose_values(frame.world_matrix,world,local);
    if(get(a,get(a,gun)+4)!=0x43bec0)return 0;
    frame.angles[0]=game_memory_read_u16(&a->memory,o+0x148);
    frame.angles[1]=game_memory_read_u16(&a->memory,o+0x14a);
    frame.angles[2]=game_memory_read_u16(&a->memory,o+0x28);
    motion=get(a,gun+20);parent=get(a,gun+4);frame.tick=get(a,0x5ac6ac);frame.motion_delay=get(a,motion+0x20);
    if(get(a,gun+12) && integer(get(a,gun+8)+frame.motion_delay)<=integer(frame.tick))frame.parent_flags=get(a,parent+16);
    if(a->memory.fault)return 0;
    status=game_level_player_attachment_update(a,gun,&frame);if(status<0)return 0;
    if(status==3 && game_memory_read_u8(&a->memory,target+12)==0x26){
        if(!game_level_vehicle_control(a,target,get(a,get(a,target)+0x1c4)))return 0;
    }
    put(a,o+0x1e4,get(a,0x5acbe8));return !a->memory.fault;
}
int game_level_vehicle_fire_visible(GameApplication *a,GameAddress o){
        GameAddress weapon=get(a,o+0x21c);
        float body[12],head[12],mount[12],middle[12],world[12];uint16_t angles[3];unsigned i;
        if(get(a,get(a,o)+0x178)!=0x405bb0)return 0;
        for(i=0;i<12;i++){body[i]=real(a,o+0x80+i*4);head[i]=real(a,o+0x180+i*4);mount[i]=real(a,o+0x150+i*4);}
        game_matrix_compose_values(middle,head,mount);game_matrix_compose_values(world,body,middle);
        angles[0]=game_memory_read_u16(&a->memory,o+0x148);angles[1]=game_memory_read_u16(&a->memory,o+0x14a);angles[2]=game_memory_read_u16(&a->memory,o+0x28);
        if(!auxiliary(a,o,weapon,angles,world))return 0;
        put(a,o+0x1e4,get(a,0x5acbe8));return !a->memory.fault;
}
static int fire(void *c,GameAddress o,GameAddress weapon,GameAddress target,int ready){GameApplication *a=c;
    if(game_terrain_cell_presence(&a->memory,o,target)){
        if(get(a,get(a,o)+0x178)==0x409330)return game_level_vehicle_fire_class24(a,o,target);
        return game_level_vehicle_fire_visible(a,o);
    }
    return target_fire(a,weapon,target,ready);
}
static VehicleTargetServices services(GameApplication *a){VehicleTargetServices s={a,acquire,settle,limit,rate,directions,blocked,line,fire,auxiliary};return s;}
int game_level_scene_transient_reset(GameApplication *a,GameAddress o,GameAddress method){
    VehicleTargetServices s=services(a);int ok;
    if(method==0x4b4170){extern int game_level_imperial_tick(GameApplication *,GameAddress);return game_level_imperial_tick(a,o);}
    if(method==0x4bdff0)return !a->memory.fault; /*Imperial override is a verified RET4. */
    if(method==0x553730)ok=vehicle_targeting_transient_reset(&a->memory,o,&s);
    else if(method==0x407410)ok=vehicle_targeting_tick(&a->memory,o,&s);
    else ok=game_level_other_scene_transient_reset(a,o,method);
    if(!ok)fprintf(stderr,"native scene transient failed: kind=%02x method=%08x control=%08x object=%08x\n",game_memory_read_u8(&a->memory,o+12),method,get(a,get(a,o)+0x1c0),o);
    return ok;
}
int game_level_vehicle_targeting_frame(GameApplication *a,GameAddress o){VehicleTargetServices s=services(a);return vehicle_targeting_frame(&a->memory,o,&s);}
int game_level_vehicle_targeting_idle(GameApplication *a,GameAddress o){VehicleTargetServices s=services(a);return vehicle_targeting_idle(&a->memory,o,&s);}
