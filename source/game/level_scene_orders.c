#include "level_run.h"
#include "scene_orders.h"
#include "level_player_resources.h"
#include "presentation_instance.h"
#include "runtime_node_pool.h"
#include "entity_queries.h"
#include "menu_sound.h"
#include "level_factory.h"
#include "level_effect_runtime.h"
#include "effect_model.h"
#include "gameplay_geometry.h"
#include "recyclable_records.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
extern int game_level_vehicle_target_acquire(GameApplication *,GameAddress,uint32_t,GameAddress *);
extern int game_level_vehicle_settle(GameApplication *,GameAddress);
extern int game_level_vehicle_control(GameApplication *,GameAddress,GameAddress);
extern int game_level_class27_target_changed(GameApplication *,GameAddress);
extern int game_level_infantry_notice(GameApplication *,GameAddress,uint32_t);
extern int game_level_vehicle_targeting_idle(GameApplication *,GameAddress);
extern int game_level_scene_command_publish(GameApplication *,GameAddress,RuntimeNode *);
extern int game_level_vehicle_fire_visible(GameApplication *,GameAddress);
extern int game_level_vehicle_fire_class24(GameApplication *,GameAddress,GameAddress);
extern int game_level_vehicle_command(GameApplication *,GameAddress,GameAddress,uint32_t);
extern int game_level_scene_identity_order(GameApplication *,GameAddress,uint32_t,uint32_t);
extern int game_level_scene_target_priority(GameApplication *,GameAddress,GameAddress,GameAddress,int32_t *);
extern int game_level_citadel_activate(GameApplication *,GameAddress,GameAddress,int,uint16_t *);
extern int game_input_world_line(GameApplication *,const float[3],const float[3],float[3],int *);
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
static int acquire(void *c,GameAddress o,GameAddress *out){return game_level_vehicle_target_acquire(c,o,0x8040,out);}
int game_level_scene_order_issue(GameApplication *a,GameAddress o,const PresentationCommand *command,int target_command){
    PresentationCommand decoded;unsigned i;
    if(!a || !command || a->memory.fault)return 0;
    RuntimeNode *node=runtime_node_pool_find(get(a,o+0xdc));
    PresentationLifecycle *state=presentation_instance_lifecycle(o);PresentationPose *pose=presentation_instance_pose(o);
    GameAddress method=get(a,get(a,o)+(target_command?0x164:0x160));
    if(!node || !state || !pose || method!=(target_command?0x53c1d0u:0x53c210u))return 0;
    presentation_command_seed_position(node,pose->identity,pose->position);
    decoded=*command;
    /*53c1d0 ->4cee60 supplies this point even for target-only requests.
     *Decode it after the priority gate, just as the original builder does. */
    if(target_command && !decoded.has_position &&
       presentation_command_plan(node,decoded.priority,decoded.kind)!=PRESENTATION_COMMAND_IGNORE){
        for(i=0;i<3;i++){uint32_t bits=get(a,0x594470+i*4);memcpy(decoded.position+i,&bits,4);}
        decoded.has_position=1;
    }
    if(!presentation_command_build(&a->memory,node,&decoded,&node))return 0;
    state->runtime.packet_chain=node->projection;put(a,o+0xdc,node->projection);
    return !a->memory.fault && game_level_scene_command_publish(a,o,node);
}
static int issue(void *c,GameAddress o,const PresentationCommand *command){return game_level_scene_order_issue(c,o,command,command->kind==1 || command->kind==5 || command->kind==6);}
static int advance(void *c,GameAddress o,int cancel){
    GameApplication *a=c;unsigned visits=0;
    if(!cancel)return game_level_vehicle_settle(a,o);
    if(get(a,get(a,o)+0x15c)!=0x539d60)return 0;
    while(get(a,get(a,o+0xdc)+0x2c)&2u){
        if(++visits>65536 || !game_level_vehicle_settle(a,o))return 0;
    }
    return !a->memory.fault;
}
static int speed(void *c,GameAddress o,long double *out){
    GameApplication *a=c;uint32_t value;int32_t signed_value;GameAddress owner;
    if(get(a,get(a,o)+0x140)!=0x539a30 || get(a,get(a,o)+0x60)!=0x53c550)return 0;
    owner=get(a,0x5f0210+(get(a,o+20)&63u)*4);
    if(!game_level_player_catalog_value(a,owner,game_memory_read_u8(&a->memory,o+12),5,
        (game_memory_read_u8(&a->memory,o+0xd8)&4u)!=0,&value))return 0;
    memcpy(&signed_value,&value,4);*out=signed_value;return !a->memory.fault;
}
static int census(void *c,GameAddress root,uint8_t type,int observed){
    GameApplication *a=c;GameAddress method=get(a,get(a,root)+(observed?0x6c:0x64));uint32_t offset;
    if(method==(observed?0x4777d0u:0x4777b0u))return !a->memory.fault;
    if(method!=(observed?0x47f560u:0x47f500u)){
        fprintf(stderr,"native scene census failed: root=%08x method=%08x observed=%d kind=%02x\n",
            (unsigned)root,(unsigned)method,observed,type);return 0;
    }
    offset=type==0x21?0x504:type==0x27?0x508:type==0x40?0x500:0;
    if(offset){offset+=observed?12:0;put(a,root+offset,get(a,root+offset)+1u);}
    return !a->memory.fault;
}
static SceneOrderServices services(GameApplication *);
static int move(void *c,GameAddress o,float x,float z){
    GameApplication *a=c;SceneOrderServices s=services(a);
    if(get(a,get(a,o)+0x174)==0x450ef0)return game_level_class29_move(a,o,x,z);
    if(get(a,get(a,o)+0x174)==0x40cab0)return game_level_class40_move(a,o,x,z);
    if(get(a,get(a,o)+0x174)==0x55b190)return game_level_aircraft_move(a,o,x,z);
    return scene_order_move(&a->memory,o,get(a,get(a,o)+0x174),x,z,&s);
}
static int other(void *c,GameAddress o,GameAddress method){
    if(method==0x40cc40 || method==0x40eaf0 || method==0x40eb00 || method==0x40eb10 || method==0x40eb20)return game_level_class40_profile(c,o,method);
    if(method==0x40d590)return game_level_class40_control(c,o);
    if(method==0x40ce40)return game_level_class40_combat(c,o);
    if(method==0x40d900)return game_level_class40_reaction(c,o);
    if(method==0x40ce20){GameApplication *a=c;SceneOrderServices s=services(a);
        return scene_order_execute(&a->memory,o,0x4ca9b0,&s) &&
            (get(a,o+0xe0)!=5 || game_level_class40_profile(a,o,get(a,get(a,o)+0x1c4)));}
    if(method==0x4511a0 || method==0x4516d0 || method==0x451170)return game_level_vehicle_control(c,o,method);
    if(method==0x451190)return game_level_vehicle_control(c,o,get(c,get(c,o)+0x168));
    if(method==0x406150)return game_level_scene_command_publish(c,o,runtime_node_pool_find(get(c,o+0xdc)));
    if(method==0x40f1b0 || method==0x40f220 || method==0x40cd20 || method==0x55b580 || method==0x55b780 || method==0x55b840 || method==0x55bc60 || method==0x55bc70 || method==0x53cba0 || method==0x409610)
        return game_level_vehicle_control(c,o,method);
    if(method==0x544d50 || method==0x544f90 || method==0x546f50 || method==0x5472e0 || method==0x544500 || method==0x544730 || method==0x5460f0)
        return game_level_vehicle_control(c,o,method);
    if(method==0x55ae60 || method==0x55af90)return game_level_vehicle_control(c,o,method);
    if(method==0x4047f0)return game_level_vehicle_targeting_idle(c,o);
    if(method==0x404950 || method==0x403b60 || method==0x4059c0 || method==0x53c000 ||
       method==0x405930 || method==0x53a3e0 || method==0x4e8510 || method==0x4062b0)
        return game_level_vehicle_control(c,o,method);
    if(method==0x545eb0 || method==0x545fb0 || method==0x546270 || method==0x546450 || method==0x545680 || method==0x546100 || method==0x544970)
        return game_level_vehicle_control(c,o,method);
    /*The supported unit tables select the native programs above. An unknown
     *selector is a failed route, never an emulated call or successful no-op. */
    fprintf(stderr,"Unsupported native scene order %08lx for %08lx\n",
        (unsigned long)method,(unsigned long)o);
    return 0;
}
static int line(void *c,const float from[3],const float to[3],int *clear){float hit[3];return game_input_world_line(c,from,to,hit,clear);}
static int attack(void *c,GameAddress o,GameAddress target){
    GameApplication *a=c;GameAddress method=get(a,get(a,o)+0x178);
    if(method==0x409330)return game_level_vehicle_fire_class24(a,o,target);
    if(method==0x40dbf0)return game_level_class40_fire(a,o,target);
    if(method==0x405bb0)return game_level_vehicle_fire_visible(a,o);
    if(method!=0x4ca270)return 0;
    put(a,o+0xe0,2);put(a,o+0xf0,target);return !a->memory.fault;
}
/*53b990/405a30 accept an object, not an identity. Reuse the command pool
 *rather than building a second command representation. */
int game_level_scene_target_order(void *c,GameAddress o,uint32_t priority,GameAddress target){
    GameApplication *a=c;GameAddress method=get(a,get(a,o)+0x198);NativeEntityRelationshipResult relation;
    RuntimeNode *node;PresentationCommand command={0};unsigned i;
    if(method==0x55c4e0)return !a->memory.fault; /*Actual empty class26 command. */
    if(method!=0x53b990 && method!=0x405a30 && method!=0x546c50)return 0;
    if(!native_entity_relationship_query(&a->memory,target,o,&relation))return 0;
    if(relation.decision<NATIVE_ENTITY_RELATION_ACCEPT_NULL_SECOND)
        return method==0x546c50 && get(a,o+0xf0)?game_level_class27_target_changed(a,o):!a->memory.fault;
    if(method==0x405a30 && !get(a,o+0xd4)){
        GameAddress notify=get(a,get(a,o)+0x58);
        if(!game_level_scene_notice(a,o,notify,2))return 0;
    }
    node=runtime_node_pool_find(get(a,o+0xdc));if(!node)return 0;
    if((method==0x53b990 || method==0x546c50) && (get(a,o+20)&63u)<4 && node->kind==3){
        uint32_t x,z;memcpy(&x,node->position,4);memcpy(&z,node->position+2,4);
        if(!((x|z)&0x7fffffffu)){
            for(i=0;i<3;i++){uint32_t v=get(a,o+24+i*4);memcpy(node->position+i,&v,4);put(a,node->projection+24+i*4,v);}
        }
    }
    command.priority=priority&255u;command.kind=5;command.target=target;
    command.has_target=target!=0;command.has_position=1;
    if(target)command.target_identity=get(a,target+20);
    for(i=0;i<3;i++){uint32_t v=get(a,0x594470+i*4);memcpy(command.position+i,&v,4);}
    if(!issue(a,o,&command))return 0;
    if(method==0x405a30){put(a,o+0xcc,0);put(a,o+0xc8,0);put(a,o+0x1fc,0);}
    if(method==0x546c50 && get(a,o+0xf0))return game_level_class27_target_changed(a,o);
    return !a->memory.fault;
}
static int identity_order(void *c,GameAddress o,uint32_t priority,uint32_t identity){return game_level_scene_identity_order(c,o,priority,identity);}
static int target_priority(void *c,GameAddress o,GameAddress root,GameAddress target,int32_t *out){return game_level_scene_target_priority(c,o,root,target,out);}
static int notice(void *c,GameAddress o,uint32_t reason){GameApplication *a=c;GameAddress method=get(a,get(a,o)+0x58);
    if(method==0x40e680)return game_level_class40_notice(a,o,reason);
    if(method==0x546630)return game_level_infantry_notice(a,o,reason);
    return game_level_scene_notice(a,o,method,reason);}
static int capture(void *c,GameAddress group,GameAddress owner,uint16_t *status){return game_level_citadel_activate(c,group,owner,1,status);}
static int publish(void *c,GameAddress o){GameApplication *a=c;return game_level_scene_command_publish(a,o,runtime_node_pool_find(get(a,o+0xdc)));}
static int landing_sound(void *c,GameAddress o,int ending){
    GameApplication *a=c;GameAddress reference=get(a,0x58f890);float x,z,rx,rz;uint32_t v;SoundPlaybackRequest request={0};
    v=get(a,o+24);memcpy(&x,&v,4);v=get(a,o+32);memcpy(&z,&v,4);
    v=get(a,reference+12);memcpy(&rx,&v,4);v=get(a,reference+20);memcpy(&rz,&v,4);
    request.distance_squared=(float)(((long double)x-rx)*((long double)x-rx)+((long double)z-rz)*((long double)z-rz));
    request.volume=1;request.sound=ending?134:129;request.flags=ending?0:1;request.force=ending?0:1;
    if(ending && !game_menu_sound_stop(a,129,get(a,o+0x144)))return 0;
    if(a->memory.fault || !game_menu_sound_play(a,&request))return 0;
    if(!ending)put(a,o+0x144,get(a,get(a,0x628a90)));
    return !a->memory.fault;
}
static SceneOrderServices services(GameApplication *a){SceneOrderServices s={a,acquire,issue,advance,speed,move,census,other,line,attack,game_level_scene_target_order,identity_order,target_priority,notice,capture,publish,landing_sound};return s;}
static int reaction_maximum(void *c,GameAddress o,uint32_t *maximum){GameApplication *a=c;
    return game_level_scene_property(a,o,get(a,get(a,o)+0x44),maximum);
}
static int reaction_command(void *c,GameAddress o,const PresentationCommand *command,int target_only){return game_level_scene_order_issue(c,o,command,target_only);}
int game_level_group_reaction(GameApplication *a,GameAddress o,GameAddress attacker,int32_t damage){
    if(!a || a->memory.fault)return 0;
    SceneReactionServices s={services(a),reaction_maximum,reaction_command};
    return scene_order_reaction(&a->memory,o,attacker,damage,&s);
}
static int32_t combat_signed(uint32_t word){int32_t value;memcpy(&value,&word,4);return value;}
static float combat_real(GameApplication *a,GameAddress p){uint32_t word=get(a,p);float value;memcpy(&value,&word,4);return value;}
static uint32_t combat_word(long double value){return (uint32_t)(uint64_t)(!isfinite(value)||value< -0x1p63L||value>=0x1p63L?INT64_MIN:(int64_t)value);}
static uint8_t combat_cell(float value){return (uint8_t)(combat_word(value)>>9);}
static int32_t combat_random(GameApplication *a,long double scale){
    uint32_t cursor=get(a,0x6308c0);if(cursor==1024)cursor=0;
    if(cursor>=1024){a->memory.fault=1;a->memory.fault_address=0x6308c0;return 0;}
    put(a,0x6308c0,cursor+1);return combat_signed(combat_word(scale*combat_real(a,0x6308c8+cursor*4)));
}
static float combat_distance(GameApplication *a,GameAddress o,GameAddress target){
    long double x=fabsl((long double)combat_real(a,o+24)-combat_real(a,target+24));
    long double z=fabsl((long double)combat_real(a,o+32)-combat_real(a,target+32));
    return (float)(x>z?z*combat_real(a,0x571f18)+x:x*combat_real(a,0x571f18)+z);
}
/*40ce40: acquire/arbitrate, aim/fire, jitter a direct move, then the actual
 *bounded flood/search/simplification and pooled movement-command publication.
 *The temporary route is owned by recyclable_records, not a guest stack. */
int game_level_class40_combat(GameApplication *a,GameAddress o){
    SceneOrderServices s=services(a);GameMemory *m=&a->memory;
    GameAddress node=get(a,o+0xdc),target=get(a,o+0xf0),gun,recipe,found,route=0,point;
    float from[3],to[3],nearest[3],position[3],width,depth,spread,budget,previous[3];
    int clear,pass,in_range,willing,ordinary,result=0,initialized=0;int32_t distance,range,count,half,jitter;
    uint32_t pending;unsigned i,attempts=1,visits=0;PresentationCommand command={0};
    if(get(a,node+12)!=5 || !target)return game_level_vehicle_settle(a,o);
    if(get(a,o+0xe0)!=5)return !m->fault;
    if(combat_signed(get(a,o+0xd4)+64u)<combat_signed(get(a,0x5ac6ac))){
        put(a,o+0xd4,get(a,0x5ac6ac));
        if(!game_level_vehicle_target_acquire(a,o,1,&found))return 0;
        if(found && found!=get(a,o+0xf0)){
            command.kind=5;command.target=found;command.has_target=1;command.target_identity=get(a,found+20);
            return game_level_scene_order_issue(a,o,&command,1);
        }
    }
    target=get(a,o+0xf0);
    for(i=0;i<3;i++){from[i]=combat_real(a,o+24+i*4);to[i]=combat_real(a,target+24+i*4);}
    from[1]=(float)((long double)from[1]+combat_real(a,o+0x30));to[1]=(float)((long double)to[1]+combat_real(a,target+0x30));
    if(!line(a,from,to,&clear))return 0;
    if(!clear){
        if(!scene_order_interaction(m,o,get(a,get(a,o)+0x18c),&s,&pass))return 0;
        return pass?!m->fault:game_level_vehicle_settle(a,o);
    }
    distance=combat_signed(combat_word(combat_distance(a,o,target)));
    gun=get(a,o+0x150);recipe=get(a,gun+20);
    range=combat_signed(combat_word((long double)combat_signed(get(a,recipe+0x24))*combat_real(a,recipe+0x1c)));
    in_range=range>distance;
    willing=combat_random(a,range)>combat_signed((uint32_t)distance-100u);
    ordinary=combat_random(a,combat_real(a,0x571ea4))<combat_signed(get(a,o+0x118));
    if(!ordinary)ordinary=combat_random(a,distance)<200;
    ordinary=ordinary && !(game_memory_read_u16(m,o+0x146) && (get(a,target+16)&0x8000));
    if((long double)distance>(long double)combat_signed(get(a,o+0x120))*combat_real(a,0x571ea8))return game_level_vehicle_settle(a,o);
    if(in_range && willing && ordinary && combat_signed(get(a,gun+8)+get(a,recipe+0x20))<=combat_signed(get(a,0x5ac6ac)))
        return attack(a,o,get(a,o+0xf0));
    if(!get(a,o+0xe0))return !m->fault;
    if(!scene_order_position_clear(m,o,combat_real(a,target+24),combat_real(a,target+32),&pass))return 0;
    if(pass){
        /*40d113 selects a target point relative to this infantry unit. */
        float source[3];for(i=0;i<3;i++)source[i]=combat_real(a,o+24+i*4);
        if(!presentation_target_position(m,target,source,nearest))return 0;
        width=(float)(fabsl((long double)combat_real(a,o+24)-nearest[0])*combat_real(a,0x571eac));
        depth=(float)(fabsl((long double)combat_real(a,o+32)-nearest[2])*combat_real(a,0x571eac));
        count=combat_signed(combat_word(width));half=combat_signed(((uint32_t)count>>1)|((uint32_t)count&0x80000000u));if(half>250)half=250;
        if(count>500)count=500;spread=(float)count;
        jitter=combat_signed((uint32_t)combat_random(a,spread)-(uint32_t)half);
        position[0]=(float)(combat_real(a,o+24)>nearest[0]?(long double)jitter-width+combat_real(a,o+24):
            -(long double)jitter+combat_real(a,o+24)+width);
        jitter=combat_signed((uint32_t)combat_random(a,spread)-(uint32_t)half);
        position[2]=(float)(combat_real(a,o+32)>nearest[2]?(long double)jitter-depth+combat_real(a,o+32):
            -(long double)jitter+combat_real(a,o+32)+depth);
        if(!scene_order_position_clear(m,o,position[0],position[2],&pass))return 0;
        if(pass)return move(a,o,position[0],position[2]);
    }
    if(get(a,0x60c4cc))return !m->fault;
    route=recyclable_record_allocate(0x24);if(!route)return 0;
    if(!recyclable_outer_initialize(m,route,NULL))goto cleanup;initialized=1;
    target=get(a,o+0xf0);
    if(!recyclable_route_reachable(m,0x60a4c8,combat_cell(combat_real(a,target+24)),combat_cell(combat_real(a,target+32)),
        combat_cell(combat_real(a,o+24)),combat_cell(combat_real(a,o+32)),&pass))goto cleanup;
    if(pass){
        pending=recyclable_route_begin(m,0x60a4c8,combat_cell(combat_real(a,target+24)),combat_cell(combat_real(a,target+32)),
            combat_cell(combat_real(a,o+24)),combat_cell(combat_real(a,o+32)),route,0xbf800000,NULL);
        while(pending && !m->fault){++attempts;pending=recyclable_route_continue(m,0x60a4c8,route,NULL);
            if(attempts>10){if(pending==get(a,0x60c4cc))put(a,0x60c4cc,0);break;}}
    }else attempts=11;
    if(m->fault)goto cleanup;
    if(attempts>10 || combat_signed(get(a,route+0x18))>0x3f800000){
        GameAddress method=get(a,get(a,o)+0x150);
        for(i=0;i<3;i++)position[i]=combat_real(a,get(a,o+0xf0)+24+i*4);
        /*40d3db invokes the actual point consumer. Class40 selects53a8a0's
         *bounded steering search, not4ca1e0's destination-only writer. */
        if(method==0x53a8a0){if(!scene_order_steer(m,o,position,&s,&pass))goto cleanup;}
        else if(method==0x4ca1e0){if(!scene_order_destination(m,o,position,&pass))goto cleanup;}
        else goto cleanup;
        result=pass || (clear && in_range?attack(a,o,get(a,o+0xf0)):game_level_vehicle_settle(a,o));goto cleanup;
    }
    if(!recyclable_route_simplify(m,route,0,NULL))goto cleanup;
    point=get(a,route+12);if(!point)goto cleanup;game_memory_write_u8(m,point+12,1);
    if(!advance(a,o,1))goto cleanup;
    {long double x=(long double)combat_real(a,o+24)-combat_real(a,get(a,o+0xf0)+24);
     long double z=(long double)combat_real(a,o+32)-combat_real(a,get(a,o+0xf0)+32);
     budget=(float)((z*z+x*x)*combat_real(a,0x571eb4));}
    previous[0]=combat_real(a,o+24);previous[2]=combat_real(a,o+32);
    while(point && !m->fault){
        uint8_t cursor=game_memory_read_u8(m,point+12),next=(uint8_t)(cursor+1);long double x,z;
        /*The original tests the float's signed bit pattern, not ftol. */
        uint32_t budget_bits;memcpy(&budget_bits,&budget,4);if(combat_signed(budget_bits)<=0)break;
        if(++visits>65536)goto cleanup;
        position[0]=(float)(game_memory_read_u8(m,get(a,point+16)+cursor)*512u);position[1]=32768;
        position[2]=(float)(game_memory_read_u8(m,get(a,point+20)+cursor)*512u);game_memory_write_u8(m,point+12,next);
        if(next==32){point=get(a,point+4);if(point)game_memory_write_u8(m,point+12,0);}
        else if(next>=game_memory_read_u8(m,point+13))point=0;
        x=(long double)previous[0]-position[0];z=(long double)previous[2]-position[2];
        budget=(float)((long double)budget-z*z-x*x);previous[0]=position[0];previous[2]=position[2];
        position[0]=(float)((long double)position[0]+combat_real(a,0x571ebc));position[2]=(float)((long double)position[2]+combat_real(a,0x571ebc));
        if(point){memset(&command,0,sizeof(command));command.priority=get(a,get(a,o+0xdc)+40);
            memcpy(command.position,position,12);command.has_position=1;command.continue_chain=1;
            if(!game_level_scene_order_issue(a,o,&command,0))goto cleanup;}
    }
    result=!m->fault;
cleanup:{GameMemory cleanup=*m;cleanup.fault=0;
    if(initialized){if(!recyclable_record_destroy(&cleanup,route,1,NULL))result=0;}
    else if(!recyclable_record_release(route))result=0;}
    return result && !m->fault;
}
/*40d900: timed mount release and posture changes, then the recovered eight
 *escape probes. Retail compares uninitialized stack scratch here; ordinary C
 *uses the computed candidate distance deterministically (unproved parity). */
int game_level_class40_reaction(GameApplication *a,GameAddress o){
    static const int dx[8]={-1,0,1,1,1,0,-1,-1},dz[8]={-1,1,1,0,-1,-1,1,0};
    GameAddress target=get(a,o+0xf0);uint32_t tick=get(a,0x5ac6ac),deadline,value;unsigned i,sweep=0;
    float maximum,radius,scale,x,z,best_x=0,best_z=0,best_distance=0,distance;int found=0,clear;
    if(!target || get(a,target+20)!=get(a,o+0xf4) || combat_signed(get(a,target+0x58))<=0)put(a,o+0x13c,tick);
    deadline=get(a,o+0x13c);
    if(deadline-tick==24 && !game_level_class40_reaction_effect(a,o))return 0;
    if(deadline-tick==184 || deadline-tick==24){
        unsigned profile=deadline-tick==184?3:4;EffectModel *model=effect_model_find(get(a,o+0x54));
        if(!effect_model_select_profile(&a->memory,model,(uint16_t)profile,get(a,0x5ac6ac)))return 0;
        game_memory_write_u8(&a->memory,o+0x148,(uint8_t)profile);return !a->memory.fault;
    }
    if(combat_signed(deadline)>combat_signed(tick))return !a->memory.fault;
    if(!game_level_player_catalog_mount(a,get(a,0x58973c),9,0x40,3,&value))return 0;
    radius=maximum=(float)combat_signed(value);
    do{
        if(++sweep>65536)return 0;
        scale=(float)((long double)combat_real(a,o+0x128)*radius);
        for(i=0;i<8;i++){
            x=(float)((long double)dx[i]*scale+combat_real(a,o+24));
            z=(float)((long double)dz[i]*scale+combat_real(a,o+32));
            (void)gameplay_geometry_terrain_height(&a->memory,x,z);
            if(!scene_order_position_clear(&a->memory,o,x,z,&clear))return 0;
            if(!clear)continue;
            {long double ax=fabsl((long double)x-combat_real(a,o+24)),az=fabsl((long double)z-combat_real(a,o+32));
             distance=(float)(ax>az?az*combat_real(a,0x571f18)+ax:ax*combat_real(a,0x571f18)+az);}
            if(!found || distance<best_distance){best_x=x;best_z=z;best_distance=distance;found=1;}
        }
        radius=(float)((long double)radius+combat_real(a,0x571ec8));
    }while(isnan(radius)||isnan(maximum)||radius<=maximum);
    {uint32_t bits;memcpy(&bits,&best_distance,4);
     if(found && combat_signed(bits)>0)return move(a,o,best_x,best_z);}
    put(a,o+0xe0,5);return !a->memory.fault;
}
int game_level_other_scene_transient_reset(GameApplication *a,GameAddress o,GameAddress method){
    SceneOrderServices s=services(a);
    if(method==0x409280 || method==0x408f60 || method==0x408c20){
        GameAddress target=get(a,o+0xc8),following=get(a,o+0xf0);
        if(target && get(a,target+20)!=get(a,o+0xcc))put(a,o+0xc8,0);
        if(following && get(a,following+20)!=get(a,o+0xf4))put(a,o+0xf0,0);
        return !a->memory.fault && other(a,o,get(a,get(a,o)+0x1c0));
    }
    if(method==0x539de0)return scene_order_tick(&a->memory,o,&s);
    if(method==0x4035c0)return scene_order_tick(&a->memory,o,&s) && other(a,o,get(a,get(a,o)+0x1c0));
    return other(a,o,method);
}
