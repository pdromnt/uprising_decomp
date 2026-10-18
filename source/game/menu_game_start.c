#include "level_run.h"
#include "menu_widgets.h"
#include "menu_network.h"
#include <string.h>
extern int game_menu_unregister_tick_method(GameApplication *,GameAddress,uint32_t);
extern int game_menu_register_tick(GameApplication *,GameAddress,GameAddress,uint32_t,uint32_t);
extern int game_menu_event_release_assets(GameApplication *,GameAddress);
extern int game_menu_selection_begin(GameApplication *);
extern int game_menu_input_begin(GameApplication *);
extern int game_menu_input_reset(GameApplication *);
extern int game_menu_window_clear(GameApplication *);
extern int game_menu_cursor_mode(GameApplication *,int);
extern int game_menu_palette_load_named(GameApplication *,GameAddress);
extern int game_menu_initial_widgets_construct(GameApplication *,GameAddress);
extern int game_menu_request(GameApplication *,GameAddress,uint32_t,uint32_t,uint32_t);
extern int game_menu_window_mode_restore(GameApplication *);
static uint32_t get(GameApplication *a,GameAddress p,unsigned o){return game_memory_read_u32(&a->memory,p+o);}
static void put(GameApplication *a,GameAddress p,unsigned o,uint32_t v){game_memory_write_u32(&a->memory,p+o,v);}
static int selected_level(GameApplication *a) {
    char name[60];unsigned i;
    for(i=0;i<sizeof(name);++i){name[i]=(char)game_memory_read_u8(&a->memory,0x629808+i);if(!name[i])return game_memory_write(&a->memory,0x5efe80,name,i+1);}
    return 0;
}
/*005232f0/state17: the real menu/game boundary, including repeat-level requests
 * and restoring the actual menu owners. The native world dependencies are still
 * strong link blockers, never calls back through the preserved hybrid/oracle. */
int game_menu_game_start(GameApplication *a) {
    GameAddress event=get(a,0x59d4c4,0),controller=get(a,0x59d4c0,0);uint32_t next;int ok;
    game_application_log(a,"native game start: %s\n","retire menu assets and initialize input");
    if(!game_menu_unregister_tick_method(a,controller,0x5232f0) || !game_menu_event_release_assets(a,event) || !game_menu_input_begin(a))return 0;
    game_application_log(a,"native game start: %s\n","selection");
    if(get(a,0x58abdc,0) && !game_menu_selection_begin(a))return 0;
    game_application_log(a,"native game start: %s\n","clear surface and palette");
    if(get(a,0x59d4bc,0) && !game_menu_window_clear(a))return 0;
    if(!game_menu_palette_load_named(a,0x598144) || !game_menu_cursor_mode(a,0))return 0;
    put(a,0x58abd8,0,get(a,event,0x10));if(!selected_level(a))return 0;
    game_application_log(a,"native game start: %s\n","level run");
    ok=game_level_run(a);
    while(ok && get(a,0x58360c,0)) {
        put(a,0x58360c,0,0);
        if(!game_level_end(a) || !game_menu_window_mode_restore(a) || !game_menu_input_begin(a) ||
           !game_menu_window_clear(a) || !game_menu_palette_load_named(a,0x598150) || !game_menu_selection_begin(a) || !selected_level(a))return 0;
        ok=game_level_run(a);
    }
    if(!game_level_end(a))ok=0;
    if(!get(a,0x59d4c8,0)) {
        next=get(a,0x58abdc,0)?1:get(a,0x5910b0,0)?18:19;
        if(next==18)put(a,0x5910b0,0,0);
        if(!game_menu_request(a,controller,69,1000,next))ok=0;
        if(!get(a,0x59d4c8,0) && (!game_menu_window_mode_restore(a) || !game_menu_palette_load_named(a,0x59815c) ||
           !game_menu_initial_widgets_construct(a,event) || !game_menu_cursor_mode(a,1)))ok=0;
    }
    return ok && !a->memory.fault;
}
int game_menu_game_state(GameApplication *a,GameAddress event,uint32_t phase) {
    (void)event;
    if(!phase)return game_menu_register_tick(a,get(a,0x59d4c0,0),0,0x5232f0,0);
    return !a->memory.fault;
}
/*005234c0: the multiplayer start observer shares actual input/window/level
 * owners, then closes the session and rebuilds the real menu on return. Keep
 * cleanup running after a failed level instead of stranding acquired devices. */
int game_menu_network_game_start(GameApplication *a) {
    GameAddress event=get(a,0x59d4c4,0),controller=get(a,0x59d4c0,0);int ok;
    if(!game_menu_unregister_tick_method(a,controller,0x5234c0) || !game_menu_window_clear(a) ||
       !game_menu_palette_load_named(a,0x598178))return 0;
    put(a,0x624f8c,0,6);
    if(!game_menu_event_release_assets(a,event) || !game_menu_cursor_mode(a,0) || !game_menu_input_begin(a))return 0;
    ok=game_level_run(a);put(a,0x5eff04,0,1);
    if(!game_network_session_reset(a))ok=0;
    if(!game_menu_input_reset(a))ok=0;
    if(!game_level_display_end(a))ok=0;
    if(!game_menu_window_mode_restore(a) || !game_menu_palette_load_named(a,0x598184) ||
       !game_menu_initial_widgets_construct(a,event) || !game_menu_cursor_mode(a,1))ok=0;
    put(a,0x58abd8,0,14);
    if(!game_menu_request(a,controller,69,1000,19))ok=0;
    return ok && !a->memory.fault;
}
