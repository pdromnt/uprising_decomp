#include "gameplay_input.h"
#include "gameplay_chat.h"
#include "level_run.h"
#include "menu_pause.h"
/* Remaining distinct native consumers. The candidate link fails until their
 * actual text/network/camera behavior is reconstructed. */
extern int game_input_message(GameApplication *,GameAddress);
extern int game_menu_strategic_depart(GameApplication *,uint32_t,uint32_t);
extern int game_menu_ingame_4c3110(GameApplication *,uint32_t);
extern int game_menu_ingame_4c54c0(GameApplication *,uint32_t);
extern int game_menu_ingame_4c5cb0(GameApplication *,uint32_t);
extern int game_input_screenshot(GameApplication *,uint32_t); /*format595c30+4d9290*/
extern int game_input_overhead_keyboard(GameApplication *,const GameInputEvent *); /*4d4b40*/
extern int game_input_placement_keyboard(GameApplication *,const GameInputEvent *); /*4d55d0*/
extern int game_menu_request(GameApplication *,GameAddress,uint32_t,uint32_t,uint32_t);
extern int game_level_display_visibility(GameApplication *,int);
extern int game_menu_present_frame(GameApplication *,uint32_t);
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
/*4d2a30: shared modifiers, shutdown/pause/chat gates and actual mode routing.
 * The supported modes select native functions with ordinary event arguments. */
int game_input_game_keyboard(GameApplication *a,const GameInputEvent *e) {
    GameMemory *m=&a->memory;uint32_t action,mode;int pressed=!!(e->value&128);
    if(e->offset==29 || e->offset==157)game_memory_write_u8(m,0x594d60,(uint8_t)pressed);
    if(e->offset==42 || e->offset==54)game_memory_write_u8(m,0x594d5c,(uint8_t)pressed);
    mode=get(a,0x604760);
    if(m->fault)return 0;
    if(mode==8)return !pressed || e->offset==57 || game_menu_request(a,get(a,0x59d4c0),200,1006,e->offset);
    if(e->offset>=256 || m->fault)return !m->fault;
    action=get(a,0x6047f0+e->offset*4);
    if(get(a,0x5910a4) && get(a,0x5eff04)) {
        if(!pressed || action!=47)return !m->fault;
        /*The repeated departure is present in the preserved executable. */
        return game_menu_ingame_pause_state(a,4) && game_menu_ingame_pause_state(a,4) &&
            game_menu_strategic_depart(a,0,4) && game_menu_strategic_depart(a,1,4) &&
            game_menu_ingame_4c3110(a,4) && game_menu_ingame_4c54c0(a,4) && game_menu_ingame_4c5cb0(a,4);
    }
    if(get(a,0x5acbe4))return !pressed || (action!=46 && action!=47) || game_input_pause(a,1);
    if(pressed && action==48 && !get(a,0x594d6c))return game_input_chat_begin(a);
    if(get(a,0x594d6c))return game_input_chat_prompt(a) && game_input_chat_keyboard(a,e);
    if(pressed) {
        if(action==35) {
            uint32_t number=get(a,0x594d34);game_memory_write_u32(m,0x594d34,number+1);
            if(!game_input_screenshot(a,number))return 0;
        } else if(action==44) {
            if(!game_level_display_visibility(a,get(a,0x597f80)!=1))return 0;
        } else if(action==45) {
            uint32_t interval=get(a,0x597f84)<1;game_memory_write_u32(m,0x597f84,interval);
            if(!game_menu_present_frame(a,interval))return 0;
        } else if(action==46) {
            int network=get(a,0x5eff04)!=0;
            if(!game_input_pause(a,0))return 0;
            if(!network)return !m->fault;
        }
    }
    /*Read again: display/pause consumers may have changed the current mode. */
    mode=get(a,0x604760);
    switch(mode) {
    case 1:return game_input_vehicle_keyboard(a,e->offset,e->value);
    case 2:return game_input_overhead_keyboard(a,e);
    case 3: {
        uint32_t selector;GameAddress root;GameRootEventServices services;
        if(!pressed || e->offset!=57)return !m->fault;
        selector=get(a,0x60464c);root=get(a,0x5f0208);services=game_input_event_services(a);
        if(selector==2)return game_root_signal_publish(m,&services,root,38);
        if(selector>1)return !m->fault;
        return game_root_scene_publish(m,&services,root,(uint8_t)(selector?40:39),game_memory_read_u16(m,get(a,0x58f890)+6));
    }
    case 5:return game_input_placement_keyboard(a,e);
    case 7: {
        GameKeyboardServices services=game_input_keyboard_services(a);
        return game_player_keyboard(m,&services,e->offset,e->value);
    }
    default:return !m->fault;
    }
}
