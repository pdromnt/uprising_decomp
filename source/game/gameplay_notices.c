#include "gameplay_notices.h"
#include "gameplay_messages.h"
#include "display_transition.h"
#include "event_queue.h"
#include "effect_resource_pool.h"
#include "menu_sound.h"
#include <string.h>

extern int game_application_options_load(GameApplication *,uint32_t);
extern uint32_t game_menu_milliseconds(GameApplication *);
extern int game_level_clock_observe_event(GameApplication *,GameAddress);
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
static float real(GameApplication *a,GameAddress p){uint32_t bits=get(a,p);float value;memcpy(&value,&bits,4);return value;}
/*4f97f0 and shared4a2230/4a4ee0/4a63f0 consumers. Camera coefficients,
 *terrain/RNG and activation reuse actual native algorithms and existing owners. */
int game_level_vehicle_end_camera(GameApplication *a,GameAddress vehicle){
    GameAddress player,active,source,camera,target;uint32_t slot;uint16_t heading;float from[3],to[3],clearance;unsigned i;
    if(!a || !vehicle || a->memory.fault)return 0;
    put(a,vehicle+0x498,get(a,0x5ac6ac));slot=get(a,vehicle+20)&63u;if(slot>=6)return 0;
    player=get(a,0x5f0210+slot*4);if(!player)return 0;put(a,player+0x5a0,0);
    if(player!=get(a,0x5f0208))return !a->memory.fault;
    if(get(a,0x604760)==7 && !game_root_selector_close(&a->memory,NULL))return 0;
    if(!game_application_options_load(a,4))return 0;
    active=get(a,0x58f890);source=vehicle+0x64;target=vehicle+0x18;
    if(active==get(a,0x58f898) || active==get(a,0x58f89c)){
        if(!active)return 0;
        source=active+12;heading=game_memory_read_u16(&a->memory,active+6);
    }else heading=game_memory_read_u16(&a->memory,vehicle+0x78);
    for(i=0;i<3;++i){from[i]=real(a,source+i*4);to[i]=real(a,target+i*4);}
    clearance=real(a,vehicle+0x30);camera=get(a,0x58f8b0);if(!camera || a->memory.fault)return 0;
    return game_camera_transition_begin(&a->memory,camera,from,to,heading,clearance) && game_camera_activate(&a->memory,camera,NULL);
}
/*47f0b0: schedule once, publish the real local notice, or allocate the existing
 *kind3 game-over event and transfer it into the player's sorted owned queue. */
int game_script_notice_publish(GameApplication *a,GameAddress player,uint32_t kind,GameAddress label,uint32_t delay,uint32_t extra){
    GameAddress vehicle,event;EffectResourcePool *pool;uint8_t bytes[256];size_t length;int inserted=0;
    if(!a || !player || a->memory.fault)return 0;
    if(get(a,player+0x57c)!=UINT32_MAX)return !a->memory.fault;
    put(a,player+0x57c,get(a,0x5ac6ac)+delay);put(a,player+0x148,kind);put(a,player+0x14c,extra);
    vehicle=get(a,player+0x3bc);if(!vehicle)return 0;
    if(get(a,vehicle+0x58) && !game_level_vehicle_end_camera(a,vehicle))return 0;
    if(player!=get(a,0x5f0208))return !a->memory.fault;
    if(!label)return 0;
    for(length=0;length<sizeof(bytes);++length){bytes[length]=game_memory_read_u8(&a->memory,label+(GameAddress)length);if(!bytes[length])break;}
    if(length==sizeof(bytes) || a->memory.fault || !game_memory_write(&a->memory,0x5ac6b8,bytes,length+1))return 0;
    if(delay && delay<0x80000000u)
        return game_menu_sound(a,kind?139:140) && game_message_enqueue(a,get(a,0x628a88),kind?0x58993c:0x589950,1);
    pool=effect_resource_pool_find(0x5cee40);if(!pool)return 0;
    event=effect_resource_pool_acquire_kind(&a->memory,pool,3);if(!event)return 0;
    put(a,event+12,(game_menu_milliseconds(a)-get(a,0x5ac90c))>>5);
    game_memory_write_u8(&a->memory,event+16,25);put(a,event+20,game_memory_read_u16(&a->memory,player+8));put(a,player+0x580,1);
    if(a->memory.fault || !game_event_queue_insert(&a->memory,player,event))goto failed;
    inserted=1;
    if(get(a,0x5eff04) && !game_level_clock_observe_event(a,event))return 0;
    return !a->memory.fault;
failed:
    if(!inserted)(void)effect_resource_pool_return_chain(&a->memory,pool,event);
    return 0;
}
