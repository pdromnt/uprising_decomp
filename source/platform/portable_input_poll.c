#include "portable_input_devices.h"
#include "gameplay_input.h"

extern int game_application_options_load(GameApplication *,uint32_t);
extern int game_menu_request(GameApplication *,GameAddress,uint32_t,uint32_t,uint32_t);
/* These are real gameplay dependencies, not success placeholders. Their
 * implementations must consume ordinary event values before the target links. */

int game_menu_input_begin(GameApplication *a) {return game_input_devices_begin(a);}
int game_menu_input_reset(GameApplication *a) {game_input_devices_end(a);return !a->memory.fault;}
int game_menu_input_mode(GameApplication *a,unsigned mode) {return game_application_options_load(a,mode);}

static int keyboard(GameApplication *a,const GameInputEvent *event) {
    return game_input_game_keyboard(a,event);
}
static int mouse(GameApplication *a,const GameInputEvent *event) {
    GameMemory *m=&a->memory;uint32_t mode;
    if(game_memory_read_u32(m,0x5acbe4) || game_memory_read_u32(m,0x5910a4))return !m->fault;
    mode=game_memory_read_u32(m,0x604760);
    /* Original mode switch discards mouse samples in modes0/3/4/6/8. */
    if(mode!=1 && mode!=2 && mode!=5 && mode!=7)return !m->fault;
    return game_input_game_mouse(a,mode,event);
}
/* 004d0220: bounded buffered reads and sequence merge. Equal sequence numbers
 * consume mouse first. Window identity selects the owned SDL window. */
int game_menu_input_poll(GameApplication *a,GameAddress window) {
    GameInputEvent keys[64],moves[32];unsigned nk=64,nm=32,k=0,p=0;int32_t status;
    GameMemory *m=&a->memory;
    if(window!=game_memory_read_u32(m,0x627904) || !game_menu_sdl_window(a))return 0;
    status=game_input_device_events(a,0,keys,&nk);
    if(status<0)nk=0;
    status=game_input_device_events(a,1,moves,&nm);
    if(status<0)nm=0;
    if(game_memory_read_u32(m,0x604760)!=8 &&
       (!game_memory_read_u32(m,0x583604) || (int32_t)game_memory_read_u32(m,0x5ac6ac)<=1))return !m->fault;
    while(k<nk || p<nm) {
        if(k<nk && (p==nm || (int32_t)(keys[k].sequence-moves[p].sequence)<0)) {
            if(!keyboard(a,&keys[k++]))return 0;
        } else if(!mouse(a,&moves[p++]))return 0;
    }
    return !m->fault;
}
