#include "gameplay_input.h"
#include "level_run.h"
#include "menu_pause.h"
#include "menu_settings.h"
#include "gameplay_messages.h"
#include "display_transition.h"
extern int game_level_default_viewport(GameApplication *);
/* Real spatial queries and camera/UI controllers are still native link
 * dependencies. No address/argument-word adapter or CPU fallback sits here. */
extern int game_input_nearby_target(GameApplication *,GameAddress,uint32_t *,uint32_t *,int *); /*4b3310*/
extern int game_input_nearby_destination(GameApplication *,GameAddress,uint32_t *,uint32_t *,int *); /*4b3290*/
extern int game_input_screen_target(GameApplication *,uint32_t *,uint32_t *,int *); /*4d6e60*/
extern int game_input_target_coordinates(GameApplication *,uint16_t[4],int *); /*4d6e60+4d71c0*/
extern int game_input_root_select(GameApplication *,GameAddress); /*47bf00*/
extern int game_input_selection_previous(GameApplication *); /*4b2ea0*/
extern int game_input_selection_next(GameApplication *); /*4b2ed0*/
extern int game_input_display_cycle(GameApplication *); /*4afb10+548ab0*/
extern int game_input_selector_begin(GameApplication *); /*4b2650/2cf0/2d50+mode6+cursor*/
extern int game_input_target_cycle(GameApplication *); /*4b3230*/
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
static GameAddress root(GameApplication *a){return get(a,0x5f0208);}
static GameAddress controller(GameApplication *a){return get(a,root(a)+0x3bc);}
static int button(GameApplication *a,const GameKeyboardServices *s,uint8_t code){return s->queue_button(a,controller(a)+0x13c,code);}
static int signal(GameApplication *a,const GameKeyboardServices *s,uint8_t code){return game_root_signal_publish(&a->memory,&s->events,root(a),code);}
static int release_strafe(GameApplication *a) {
    if(get(a,controller(a)+0x4a0))return !a->memory.fault;
    if(!game_input_direction(&a->memory,controller(a)+0x13c,4,0,0))return 0;
    return game_input_direction(&a->memory,controller(a)+0x13c,5,0,0);
}
/*4d3be5: tracking camera selection, restoration, binding and feedback reach
 * the already-native camera and viewport algorithms directly. */
int game_input_tracking_camera(GameApplication *a){
    GameMemory *m=&a->memory;GameAddress tracking=get(a,0x58f8ac),label;
    if(get(a,0x58f890)==tracking){
        if(!game_camera_restore(m,NULL) || !game_level_default_viewport(a))return 0;
        label=0x595d44;
    }else{
        GameAddress selected=get(a,0x60387c);
        if(!selected)label=0x595d5c;
        else{
            if(!game_camera_activate(m,tracking,NULL))return 0;
            put(a,get(a,0x58f8ac)+0x28,get(a,0x60387c));
            game_display_sample_scalar(m,0);label=0x595d50;
        }
    }
    return !m->fault && game_message_enqueue(a,get(a,0x628a88),label,1);
}
/*4d3410: ordinary vehicle keyboard control flow. Movement and event records
 * retain the existing player/pool owners; all key releases reach those owners. */
int game_input_vehicle_keyboard(GameApplication *a,uint32_t key,uint32_t flags) {
    GameMemory *m=&a->memory;GameKeyboardServices s=game_input_keyboard_services(a);
    uint32_t action,first=0,second=0;unsigned direction=8;int pressed=!!(flags&128),found;
    if(key>=256 || m->fault)return !m->fault;
    action=get(a,0x6047f0+key*4);
    switch(action){case 1:direction=0;break;case 2:direction=1;break;case 3:direction=2;break;
    case 4:direction=3;break;case 5:direction=4;break;case 6:direction=5;break;
    case 32:direction=6;break;case 33:direction=7;break;}
    if(direction<8)return game_input_direction(m,controller(a)+0x13c,direction,pressed,0x3f800000);
    if(!pressed){if(action==7 || action==8)return button(a,&s,(uint8_t)(action-3));return !m->fault;}
    if(action>=58 && action<=72)return game_input_byte_publish(a,controller(a)+0x13c,9,(uint8_t)(action-58));
    if(action>=52 && action<=57) {
        uint8_t unit=action==56?10:action==57?5:(uint8_t)((game_memory_read_u8(m,0x594d5c)?6:1)+action-52);
        GameAddress chosen=gameplay_player_choose_collection(m,&s.units,unit);
        if(m->fault)return 0;
        if(unit==5)return (int32_t)get(a,root(a)+0x2d4)>=0 || s.feedback(a,123,0x596098);
        if(!chosen)return s.feedback(a,123,0x5960b8);
        if(!s.feedback(a,121,0) || !game_root_command_publish(m,&s.events,root(a),26,unit,game_memory_read_u8(m,chosen)))return 0;
        return s.feedback(a,0,gameplay_collection_message(unit));
    }
    switch(action) {
    case 7:return button(a,&s,2);case 8:return button(a,&s,3);
    case 9:return game_root_count_publish(m,&s.events,root(a),27,4);
    case 10:return signal(a,&s,28);case 11:return signal(a,&s,29);
    case 12:case 15: {
        GameAddress position=get(a,root(a)+0x448)+0x18;
        if(action==12){if(!game_input_nearby_target(a,position,&first,&second,&found))return 0;}
        else if(!game_input_nearby_destination(a,position,&first,&second,&found))return 0;
        if(found)return game_root_pair_publish(m,&s.events,root(a),(uint8_t)(action==12?30:31),first,second);
        return s.feedback(a,0,action==12?0x595ce0:0x595cf0);
    }
    case 13:case 14:
        if(!game_input_screen_target(a,&first,&second,&found))return 0;
        return !found || game_root_pair_publish(m,&s.events,root(a),(uint8_t)(action==13?43:44),first,second);
    case 16: {
        uint16_t coordinates[4];if(!game_input_target_coordinates(a,coordinates,&found))return 0;
        return !found || game_root_coordinates_publish(m,&s.events,root(a),32,coordinates);
    }
    case 17:return signal(a,&s,49);case 18:return signal(a,&s,33);
    case 19:
        if(get(a,0x58f890)==get(a,0x58f898)) {
            if(!game_camera_activate(m,get(a,0x58f89c),NULL) || !button(a,&s,get(a,controller(a)+0x49c)?15:16))return 0;
            put(a,controller(a)+0x4a0,get(a,controller(a)+0x49c));
        } else {
            if(!game_camera_activate(m,get(a,0x58f898),NULL))return 0;
            put(a,controller(a)+0x49c,get(a,controller(a)+0x4a0));put(a,controller(a)+0x4a0,0);
            if(!button(a,&s,16))return 0;
        }
        return game_menu_render_quality(a,get(a,0x597f88)) && release_strafe(a);
    case 20:
        put(a,controller(a)+0x4a0,get(a,controller(a)+0x4a0)<1);
        return button(a,&s,get(a,controller(a)+0x4a0)?15:16) && release_strafe(a);
    case 22:return button(a,&s,17);case 23:return button(a,&s,10);case 24:return button(a,&s,11);
    case 25:return button(a,&s,7);case 26:return button(a,&s,8);
    case 27:return game_input_root_select(a,root(a));
    case 28:return game_input_selection_previous(a);case 29:return game_input_selection_next(a);
    case 30:return game_input_display_cycle(a);
    case 34:return button(a,&s,20);
    case 36:game_input_queue_clear(m,controller(a)+0x13c);return button(a,&s,6) && game_input_selector_begin(a);
    case 37: {
        static const uint32_t weights[4][3]={{0,0x3f800000,0},{0x3f800000,0,0},{0,0,0x3f800000},{0x3eaaaa3b,0x3eaaaa3b,0x3eaaab8a}};
        static const GameAddress labels[4]={0x595d00,0x595d0c,0x595d18,0x595d24};
        int cycle=((int32_t)(int16_t)game_memory_read_u16(m,0x603744)+1)%4;
        game_memory_write_u16(m,0x603744,(uint16_t)cycle);
        if(cycle>=0){put(a,0x6040a4,weights[cycle][0]);put(a,0x6040fc,weights[cycle][1]);put(a,0x604114,weights[cycle][2]);
            if(!s.feedback(a,0,labels[cycle]))return 0;}
        return s.queue_float_pair(a,controller(a)+0x13c,get(a,0x604114),get(a,0x6040a4));
    }
    case 38:return game_input_target_cycle(a);case 39:return signal(a,&s,48);
    case 40:return game_input_command_panel_toggle(a,0);case 41:return game_input_tracking_camera(a);
    case 42:return game_input_command_panel_toggle(a,1);
    case 47:
        if(get(a,0x5eff04))return game_menu_ingame_pause_state(a,0) && game_menu_ingame_pause_state(a,1);
        return game_input_pause(a,0);
    case 51:put(a,0x594d48,get(a,0x594d48)^1);break;
    /*The original explicitly ignores these actions and out-of-range bindings. */
    default:break;
    }
    return !m->fault;
}
/*4d4b40: overhead controls share owned command queues with the vehicle,
 * but have separate movement axes, fire events and camera-exit commands. */
int game_input_overhead_keyboard(GameApplication *a,const GameInputEvent *e) {
    GameMemory *m=&a->memory;GameKeyboardServices s=game_input_keyboard_services(a);
    GameAddress active=get(a,root(a)+0x448);uint32_t action,first,second;
    unsigned direction=10;int pressed=!!(e->value&128),found;
    if(e->offset>=256 || m->fault)return !m->fault;
    action=get(a,0x6047f0+e->offset*4);
    switch(action){case 1:case 32:direction=6;break;case 2:case 33:direction=7;break;
    case 5:direction=8;break;case 6:direction=9;break;}
    if(direction<10)return game_input_direction(m,active+0x264,direction,pressed,0x3f800000);
    if(action==7 || action==8)return s.queue_button(a,active+0x264,(uint8_t)(pressed?21:22));
    if(!pressed)return !m->fault;
    if(action>=52 && action<=57) {
        uint8_t unit=action==56?10:action==57?5:(uint8_t)((game_memory_read_u8(m,0x594d5c)?6:1)+action-52);
        GameAddress chosen=gameplay_player_choose_collection(m,&s.units,unit);
        if(m->fault)return 0;
        if(unit==5)return (int32_t)get(a,root(a)+0x2d4)>=0 || s.feedback(a,123,0x596098);
        if(!chosen)return s.feedback(a,123,0x5960b8);
        return s.feedback(a,121,0) && game_root_command_publish(m,&s.events,root(a),26,unit,game_memory_read_u8(m,chosen)) &&
            s.feedback(a,0,gameplay_collection_message(unit));
    }
    switch(action) {
    case 9:return game_root_count_publish(m,&s.events,root(a),27,4);
    case 10:return signal(a,&s,28);case 11:return signal(a,&s,29);
    case 12:case 15:
        if(action==12){if(!game_input_nearby_target(a,active+0x18,&first,&second,&found))return 0;}
        else if(!game_input_nearby_destination(a,active+0x18,&first,&second,&found))return 0;
        return found?game_root_pair_publish(m,&s.events,root(a),(uint8_t)(action==12?30:31),first,second):
            s.feedback(a,0,action==12?0x595d70:0x595d80);
    case 13:case 14:
        if(!game_input_screen_target(a,&first,&second,&found))return 0;
        return !found || game_root_pair_publish(m,&s.events,root(a),(uint8_t)(action==13?43:44),first,second);
    case 16: {
        uint16_t coordinates[4];if(!game_input_target_coordinates(a,coordinates,&found))return 0;
        return !found || game_root_coordinates_publish(m,&s.events,root(a),32,coordinates);
    }
    case 17:return signal(a,&s,49);case 18:return signal(a,&s,35);
    case 27:return game_input_root_select(a,root(a));
    case 28:return game_input_selection_previous(a);case 29:return game_input_selection_next(a);
    case 30:return game_input_display_cycle(a);
    case 38:return game_input_target_cycle(a);case 39:return signal(a,&s,48);
    case 47:return signal(a,&s,34);
    case 51:put(a,0x594d48,get(a,0x594d48)^1);break;
    default:break;
    }
    return !m->fault;
}
