#include "level_run.h"
#include "gameplay_input.h"
#include "level_script.h"
#include "gameplay_teardown.h"
#include "gameplay_geometry.h"
#include "level_scene_render.h"
#include "level_world_render.h"
#include "level_world_surface.h"
#include "gameplay_chat.h"
#include "game_initializers.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
/*Remaining distinct programs, all mandatory native links. These boundaries
 * cannot be replaced by no-op ticks/presentation to obtain a playable link. */
extern uint32_t game_menu_milliseconds(GameApplication *);
extern int game_level_clock_receive(GameApplication *,GameAddress);
extern int game_level_clock_refresh(GameApplication *,GameAddress);
extern int game_level_clock_allowed_tick(GameApplication *,GameAddress,uint32_t,uint32_t *);
extern int game_level_clock_packet_flush(GameApplication *,GameAddress);
extern int game_level_pause_network(GameApplication *); /*441430*/
extern int game_level_deferred_collect(GameApplication *); /*53fee0*/
extern int game_level_network_tick(GameApplication *); /*440880*/
extern int game_level_ai_root_reset(GameApplication *,GameAddress,GameAddress);
extern int game_level_ai_root_update(GameApplication *,GameAddress,GameAddress);
extern int game_level_scene_transient_reset(GameApplication *,GameAddress,GameAddress);
extern int game_level_descriptor_transient_reset(GameApplication *,GameAddress,GameAddress);
extern int game_level_vehicle_input_tick(GameApplication *,GameAddress,uint32_t); /*5521d0*/
extern int game_level_player_input_tick(GameApplication *,GameAddress,uint32_t); /*4f2440*/
extern int game_level_present(GameApplication *); /*418a50*/
extern int game_level_strategic_present(GameApplication *); /*49eff0*/
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
static int32_t signed_word(uint32_t word){int32_t value;memcpy(&value,&word,4);return value;}
static float real(GameApplication *a,GameAddress p){uint32_t word=get(a,p);float value;memcpy(&value,&word,4);return value;}
static void put_real(GameApplication *a,GameAddress p,float value){uint32_t bits;memcpy(&bits,&value,4);put(a,p,bits);}
static uint32_t truncate_word(long double value){return (uint32_t)(uint64_t)(!isfinite(value) || value< -0x1p63L || value>=0x1p63L?INT64_MIN:(int64_t)value);}
/*49c760 builds the four-player coarse decision mask and resets the three
 *per-tick census counters. Its byte grid is the established bounded owner. */
int game_level_world_decisions(GameApplication *a){
    unsigned player;int32_t x,z,minx,minz,maxx,maxz,ix,iz;
    if(!game_memory_fill_u32(&a->memory,0x6020f0,256,0,0))return 0;
    for(player=0;player<4;player++){
        GameAddress root=get(a,0x5f0210+player*4),scene;uint32_t wx,wz;
        put(a,root+0x50c,0);put(a,root+0x510,0);put(a,root+0x514,0);
        if(!get(a,root+0x468))continue;
        scene=get(a,root+0x450);wx=truncate_word(real(a,scene+0x18));wz=truncate_word(real(a,scene+0x20));
        x=signed_word((wx>>12)|((wx&0x80000000u)?0xfff00000u:0));
        z=signed_word((wz>>12)|((wz&0x80000000u)?0xfff00000u:0));
        minx=x-4;maxx=x+4;minz=z-4;maxz=z+4;
        if(minx<0)minx=0;
        if(minz<0)minz=0;
        if(maxx>=32)maxx=31;
        if(maxz>=32)maxz=31;
        /*Retail's do/while loops cannot handle out-of-world rectangles.
         *Reject malformed input rather than writing outside this owned grid. */
        if(minx>maxx || minz>maxz || maxx<0 || maxz<0 || minx>=32 || minz>=32){
            fprintf(stderr,"native decision mask outside world: player=%u scene=%08x position=%g/%g cells=%d/%d\n",player,scene,real(a,scene+0x18),real(a,scene+0x20),x,z);
            a->memory.fault=1;return 0;
        }
        for(iz=minz;iz<=maxz;iz++)for(ix=minx;ix<=maxx;ix++){
            GameAddress cell=0x6020f0+(uint32_t)iz*32u+(uint32_t)ix;
            game_memory_write_u8(&a->memory,cell,game_memory_read_u8(&a->memory,cell)|(uint8_t)(1u<<player));
        }
        if(a->memory.fault)return 0;
    }
    return !a->memory.fault;
}
/*47ae20: notifications reread lists/counts after callbacks. */
static int reset_root(GameApplication *a,GameAddress root){
    GameAddress object,group,member;unsigned visits=0,i;uint32_t method=get(a,get(a,root)+12);
    if(method!=0x47ae20)return !a->memory.fault && game_level_ai_root_reset(a,root,method);
    gameplay_teardown_reset_entity_auxiliary(&a->memory,root,0);
    object=get(a,root+0xc8);gameplay_teardown_reset_entity_auxiliary(&a->memory,root,1);
    while(object){
        if(++visits>65536 || !game_level_scene_transient_reset(a,object,get(a,get(a,object)+0x98)))return 0;
        object=get(a,object+0x48);
    }
    group=get(a,root+0x294);visits=0;
    while(group){
        if(++visits>65536)return 0;
        for(i=0;(int32_t)i<(int16_t)game_memory_read_u16(&a->memory,group+0x24);i++){
            member=get(a,get(a,group+0x18)+i*4);
            if(member && !game_level_descriptor_transient_reset(a,member,get(a,get(a,member)+4)))return 0;
            if(a->memory.fault)return 0;
        }
        group=get(a,group+0x64);
    }
    return !a->memory.fault;
}
/*418640: simulation catch-up and the two original binary32 timing filters. */
static int advance(GameApplication *a,uint32_t requested){
    GameAddress root;uint32_t method;unsigned visits;
    if(get(a,0x5eff04)){
        uint32_t allowed,clock=get(a,0x5accd4),now=get(a,0x5ac6ac);
        if(!game_level_clock_receive(a,clock) || !game_level_clock_refresh(a,get(a,0x5accd4)) ||
           !game_level_clock_allowed_tick(a,get(a,0x5accd4),requested,&allowed))return 0;
        put(a,0x5acc80,allowed);
        if(signed_word(allowed)>signed_word(now)){
            long double weight=real(a,0x572024),blend=real(a,0x572028);
            long double first=(long double)real(a,0x5acc38)*weight+
                (long double)signed_word(requested-get(a,0x5ac918))*real(a,0x572020);
            long double second=(long double)real(a,0x5ac928)*weight+signed_word(requested-allowed);
            put(a,0x5ac918,requested);put_real(a,0x5acc38,(float)(first*blend));put_real(a,0x5ac928,(float)(second*blend));
        }
        requested=get(a,0x5acc80);
    }
    while(signed_word(requested)>signed_word(get(a,0x5ac6ac))){
        if(!game_level_world_decisions(a)){game_application_log(a,"native simulation failed: %s\n","decision mask");return 0;}
        if(!(get(a,0x5ac6ac)&31u) && !game_level_commands_update(a)){game_application_log(a,"native simulation failed: %s\n","script commands");return 0;}
        put(a,0x58973c,get(a,0x589738));visits=0;
        while((root=get(a,0x58973c))){
            if(++visits>65536 || !reset_root(a,root)){game_application_log(a,"native simulation failed: %s\n","root reset");return 0;}
            root=get(a,0x58973c);method=get(a,get(a,root));
            if(method==0x47a980){if(!game_level_root_update(a,root)){game_application_log(a,"native simulation failed: %s\n","player root");return 0;}}
            else if(!game_level_ai_root_update(a,root,method)){game_application_log(a,"native simulation failed: %s\n","AI root");return 0;}
            root=get(a,0x58973c);put(a,0x58973c,get(a,root+4));
            if(a->memory.fault)return 0;
        }
        if(!game_level_deferred_collect(a))return 0;
        if(get(a,0x5eff04) && !(get(a,0x5ac6ac)&127u) && !game_level_network_tick(a))return 0;
        put(a,0x5ac6ac,get(a,0x5ac6ac)+1u);if(a->memory.fault)return 0;
    }
    return !a->memory.fault;
}
/*558be0/558c10 share the actual three signed-short accumulators. */
static int controller(GameApplication *a,GameAddress queue,unsigned phase,uint32_t value){
    uint32_t method=get(a,get(a,queue)+phase*4);unsigned i;
    if(phase==2){
        if(method==0x560e20){a->memory.fault=1;return 0;} /*Pure virtual abort. */
        if(method==0x4f2440)return game_level_player_input_tick(a,queue,value);
        return method==0x5521d0 && game_level_vehicle_input_tick(a,queue,value);
    }
    if(method!=(phase?0x558c10u:0x558be0u) || a->memory.fault)return 0;
    for(i=0;i<3;i++){
        uint16_t bits=game_memory_read_u16(&a->memory,queue+12+i*2);int16_t sample;memcpy(&sample,&bits,2);
        if(phase)bits=0;
        else if(signed_word(value)>1)bits=(uint16_t)(sample/signed_word(value));
        game_memory_write_u16(&a->memory,queue+12+i*2,bits);
    }
    return !a->memory.fault;
}
static int controllers(GameApplication *a,unsigned phase,uint32_t value){
    GameAddress root=get(a,0x5f0208),player=get(a,root+0x3bc),scene;
    if(!controller(a,player+0x13c,phase,value))return 0;
    root=get(a,0x5f0208);scene=get(a,root+0x448);
    return scene==get(a,root+0x3bc) || controller(a,scene+0x264,phase,value);
}
int game_level_frame(GameApplication *a){
    uint32_t now,previous;
    if(!a || a->memory.fault)return 0;
    if(get(a,0x5acbe4)){
        if(!game_level_pause_network(a))return 0;
        if(get(a,0x5eff04) && (!game_level_clock_receive(a,get(a,0x5accd4)) ||
           !game_level_clock_refresh(a,get(a,0x5accd4)) || !game_level_clock_packet_flush(a,get(a,0x5accd4))))return 0;
    }else{
        now=(game_menu_milliseconds(a)-get(a,0x5ac90c))>>5;if(!advance(a,now)){
            game_application_log(a,"native frame failed: %s\n","simulation advance");return 0;
        }
        now=(game_menu_milliseconds(a)-get(a,0x5ac90c))>>5;previous=get(a,0x5acbe8);
        if(signed_word(now-previous)>0){
            if(!controllers(a,0,now-previous))return 0;
            previous=get(a,0x5acbe8);
            for(;signed_word(previous)<signed_word(now);previous++)if(!controllers(a,2,previous))return 0;
            if(get(a,0x5eff04)){
                GameAddress clock=get(a,0x5accd4);
                uint32_t send_tick=(game_menu_milliseconds(a)-get(a,0x5ac90c))>>5;
                if(signed_word(send_tick)>signed_word(get(a,clock+12)+get(a,clock+16))){
                    if(!game_input_root_events(a,get(a,0x5f0208),now) ||
                       !game_level_clock_packet_flush(a,get(a,0x5accd4)))return 0;
                }
            }else if(!game_input_root_events(a,get(a,0x5f0208),now))return 0;
            if(!controllers(a,1,0))return 0;
        }
        put(a,0x5acbe8,now);
    }
    if(get(a,0x604760)==7){if(!game_level_strategic_present(a))return 0;}
    else if(!game_level_present(a)){
        game_application_log(a,"native frame failed: %s\n","world presentation");return 0;
    }
    put(a,0x583604,1);return !a->memory.fault;
}

/*The remaining distinct interface/backend programs keep real native arguments.
 *Their missing definitions block shipping; no generic stage/CPU dispatcher. */
extern uint16_t game_application_key_state(GameApplication *,unsigned,int asynchronous);
extern int game_level_frame_004187a0(GameApplication *);
extern int game_application_render_mode(GameApplication *,int); /*4d7680*/
extern int game_application_frame_present(GameApplication *); /*5189a0*/
extern int game_level_frame_004d9570(GameApplication *);
extern int game_level_frame_00435240(GameApplication *);
extern int game_level_frame_004841e0(GameApplication *);
extern int game_level_frame_004d8b50(GameApplication *,const uint32_t[4],uint32_t[4],const uint32_t[2],float);
extern int game_level_frame_00548c70(GameApplication *);
extern int game_level_frame_004afcc0(GameApplication *);
extern int game_level_frame_004d91f0(GameApplication *,GameAddress);
extern int game_level_frame_00532780(GameApplication *,uint32_t);
extern int game_level_frame_00532870(GameApplication *,int32_t,int32_t,GameAddress,uint32_t);
extern int game_level_frame_004b28d0(GameApplication *);
extern int game_level_frame_004b26a0(GameApplication *);
extern int game_level_frame_004c6eb0(GameApplication *);
extern int game_level_frame_00515f40(GameApplication *,int);
/*418a50: actual frame ordering and viewport/FPS algorithms. Original empty
 *5188e0/518990/5406f0 calls have no executable behavior to preserve. */
int game_level_present(GameApplication *a){
    const char *stage="projected queues";
    float width,height;long double half_w,half_h,margin;
    float center[2],bounds[4],logical_w,logical_h;uint32_t mode,word,tick;
    unsigned i;static const GameAddress clip_fields[4]={0x597f50,0x597f54,0x597f58,0x597f5c};
    static const GameAddress saved_fields[4]={0x626e60,0x626e54,0x626e50,0x626e5c};
    if((int16_t)game_application_key_state(a,0x21,0)<0 &&
       (int16_t)game_application_key_state(a,0x22,0)<0)return game_level_frame_004187a0(a);
    width=(float)signed_word(get(a,0x627928));height=(float)signed_word(get(a,0x62792c));
    half_w=(long double)width*real(a,0x572034);half_h=(long double)height*real(a,0x572034);
    margin=(long double)real(a,0x572010)-real(a,0x59d440);
    /*418ae2 subtracts the cockpit height from the bottom clip argument.
     *The projection center remains at half-height, aligned with the sight. */
    center[0]=(float)half_w;center[1]=(float)half_h;
    bounds[0]=(float)(half_w*margin);bounds[1]=(float)(half_h*margin);
    bounds[2]=(float)((long double)width-bounds[0]);bounds[3]=(float)((long double)height-bounds[1]-real(a,0x626e4c));
    logical_w=(float)((long double)width*real(a,0x59d440));logical_h=(float)((long double)height*real(a,0x59d440));
    if(a->memory.fault || !gameplay_geometry_projection_setup(&a->memory,0x6290c8,center,logical_w,logical_h,bounds)){
        game_application_log(a,"native presentation failed: %s\n","projection setup");return 0;
    }
    if(!game_level_world_present(a))return 0;
    word=get(a,0x59d460)+get(a,0x59d464);put(a,0x59d460,word);
    if(word==10)put(a,0x59d464,UINT32_MAX);else if(!word)put(a,0x59d464,1);
    if(a->memory.fault || !game_level_texture_frame(a) || !game_application_render_mode(a,1) ||
       (get(a,0x597f60) && !game_level_world_surface_draw(a)) ||
       !game_level_world_render(a,get(a,0x6252f8)) || !game_level_colored_render(a,get(a,0x59626c))){
        game_application_log(a,"native presentation failed: %s\n","world render submission");return 0;
    }
    for(i=0;i<3;i++)if(!game_level_projected_render(a,(ProjectedQueueKind)i))goto failed;
    stage="frame auxiliary surface";if(!game_level_frame_004d9570(a))goto failed;
    game_initializer_004e9660(&a->memory);
    if(get(a,0x58404c) && !game_level_frame_00435240(a))return 0;
    if(get(a,0x589ac0) && !game_level_frame_004841e0(a))return 0;
    if(get(a,0x584048) && !game_level_frame_00435240(a))return 0;
    if(get(a,0x589abc) && !game_level_frame_004841e0(a))return 0;
    if(get(a,0x597f88) && (get(a,0x58404c) || get(a,0x589ac0) || get(a,0x584048) || get(a,0x589abc))){
        uint32_t command[4]={0x21d,328,640,338};
        uint32_t image[2]={640u|(480u<<16),get(a,0x5960e8)};
        if(!game_level_frame_004d8b50(a,command,command,image,1.0f))return 0;
    }
    stage="timed message";if(!game_level_frame_00548c70(a))goto failed;
    stage="mode HUD";
    mode=get(a,0x604760);
    switch(mode){
    case 1:case 2:if(!game_level_frame_004afcc0(a))goto failed;break;
    case 3:{int32_t left,top,wide,high;
        if(!game_level_frame_004d91f0(a,0x6032e0) || !game_level_frame_00532780(a,246))return 0;
        left=(int16_t)game_memory_read_u16(&a->memory,0x6032e0);
        top=(int16_t)game_memory_read_u16(&a->memory,0x6032e2);
        wide=(int16_t)game_memory_read_u16(&a->memory,0x6032e4);
        high=(int16_t)game_memory_read_u16(&a->memory,0x6032e6)-5;
        high=high>=0?high/2:-1-((-high-1)/2);
        if(!game_level_frame_00532870(a,left+wide+1,top+high,0x583d04,get(a,get(a,0x5f0208)+0x588)))return 0;
        break;}
    case 5:if(!game_level_frame_004afcc0(a) || !game_level_frame_004b28d0(a))goto failed;break;
    case 6:if(!game_level_frame_004afcc0(a) || !game_level_frame_004b26a0(a))goto failed;break;
    default:break;
    }
    stage="chat overlay";if(!game_level_chat_frame(a))goto failed;
    if(get(a,0x5910a4)){
        int result;
        if(!game_memory_read_u8(&a->memory,0x594d60) && !game_level_frame_004c6eb0(a))return 0;
        for(i=0;i<4;i++)put(a,saved_fields[i],get(a,clip_fields[i]));
        put(a,clip_fields[0],0);put(a,clip_fields[1],0);
        put(a,clip_fields[2],get(a,0x626d6c));put(a,clip_fields[3],get(a,0x626d68));
        result=game_level_frame_00515f40(a,0);
        for(i=0;i<4;i++)put(a,clip_fields[i],get(a,saved_fields[i]));
        if(!result || a->memory.fault)return 0;
    }
    stage="frame present";if(!game_application_frame_present(a))goto failed;
    tick=game_menu_milliseconds(a);put(a,0x583b24,tick);
    if(!get(a,0x583b20))put(a,0x583b20,tick);
    word=get(a,0x583b1c)+(tick-get(a,0x583b20));put(a,0x583b1c,word);
    put(a,0x583b20,tick);put(a,0x583b18,get(a,0x583b18)+1);
    if(signed_word(word)>1000){put(a,0x583b14,get(a,0x583b18));put(a,0x583b18,0);put(a,0x583b1c,0);}
    /*The final debug chord targets a literal ret; retain input polling order. */
    if(game_application_key_state(a,0x20,1)&0x8000)
        if(game_application_key_state(a,0x11,1)&0x8000)(void)game_application_key_state(a,0x10,1);
    return !a->memory.fault;
failed:
    game_application_log(a,"native presentation failed: %s\n",stage);return 0;
}
