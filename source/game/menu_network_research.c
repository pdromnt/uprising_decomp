#include "menu_choice_controls.h"
#include "menu_network.h"
extern int game_menu_research_controls_position(GameApplication *,GameAddress,uint32_t,uint32_t,uint32_t);
extern int game_menu_research_controls_visible(GameApplication *,GameAddress,int);
extern int game_menu_research_controls_enable(GameApplication *,GameAddress,int);
extern int game_menu_register_observer(GameApplication *,GameAddress,uint32_t,uint32_t);
extern int game_menu_unregister_tick_method(GameApplication *,GameAddress,uint32_t);
extern int game_menu_sound_loop_begin(GameApplication *,uint32_t,GameAddress);
extern int game_menu_sound_loop_end(GameApplication *,uint32_t,uint32_t);
static uint32_t get(GameApplication *a,GameAddress p,unsigned o){return game_memory_read_u32(&a->memory,p+o);}
static void put(GameApplication *a,GameAddress p,unsigned o,uint32_t v){game_memory_write_u32(&a->memory,p+o,v);}
static int control(GameApplication *a,GameAddress p,int visible,int enabled) {
    return !p || (game_menu_widget_visible(a,p,visible) && game_menu_widget_enable(a,p,enabled));
}
/*4972b7: network research borrows the event's existing research and lobby
 * controls. The two page families keep one roster and selection owner through
 * entry, slide presentation, input changes and return/departure destruction. */
int game_menu_network_research_state(GameApplication *a,GameAddress e,uint32_t phase) {
    GameAddress p,animation=get(a,e,0x60),controller=get(a,0x59d4c0,0);unsigned i;uint32_t frame;
    if(!phase) {
        p=get(a,e,0x120);if(p && !game_menu_widget_enable(a,p,0))return 0;
        if(!control(a,get(a,e,0xf0),0,0) || !control(a,get(a,e,0x11c),0,0) || !control(a,get(a,e,0x118),0,0))return 0;
        for(i=0;i<4;++i) {
            p=get(a,e,0x124+i*4);if(p && !game_menu_widget_visible(a,p,0))return 0;
            if(!control(a,get(a,e,0xf4+i*4),0,0) || !control(a,get(a,e,0x144+i*4),0,0) || !control(a,get(a,e,0x134+i*4),0,0))return 0;
        }
        if(!game_menu_widget_visible(a,get(a,0x58abe8,0),1))return 0;
        for(i=0;i<4;++i)if(!game_menu_widget_base_state(a,get(a,e,0x78+i*4),0))return 0;
        p=get(a,e,0x78);
        if(!game_menu_button_input(a,p,0x201,0,0) || !game_menu_button_input(a,p,0x202,0,0) ||
           !game_menu_research_catalog_build(a,get(a,e,0x74),0) || !game_menu_research_controls_position(a,e,659,236,11) ||
           !game_menu_research_controls_visible(a,e,1) || !game_menu_widget_position(a,animation,544,383,12,0) ||
           !game_menu_widget_visible(a,animation,1))return 0;
        put(a,0x58fd3c,0,1);put(a,e,0x24,2);put(a,e,0x1c,100);put(a,e,0x28,3);put(a,e,0x20,100);return !a->memory.fault;
    }
    if(phase==2 || phase==3) {
        unsigned progress=phase==2?0x1c:0x20;
        if(get(a,e,progress)==100) {
            put(a,animation,0x308,1);put(a,animation,0x304,phase==2?1:UINT32_MAX);put(a,e,0x48,1);
            if(phase==3 && (!game_menu_research_controls_enable(a,e,0) || !game_menu_unregister_tick_method(a,controller,0x474eb0)))return 0;
            return game_menu_sound_loop_begin(a,151,0x5f06ac);
        }
        if(!game_menu_animation_advance(a,animation,&frame))return 0;
        if(frame==UINT32_MAX) {
            put(a,e,progress,1);put(a,e,0x48,0);put(a,0x58fd3c,0,1);
            if(phase==2 && (!game_menu_research_controls_enable(a,e,1) || !game_menu_register_observer(a,controller,0x474eb0,0)))return 0;
            if(!game_menu_sound_loop_end(a,151,get(a,0x5f06ac,0)))return 0;
        } else {put(a,e,progress,2);put(a,e,0x48,1);}
        p=get(a,e,0x70);return game_menu_research_controls_position(a,e,get(a,p,0x20)+(phase==2?0u-18u:18u),236,get(a,p,0x40));
    }
    if(phase!=1)return !a->memory.fault;
    if(!game_menu_selector_clear(a,get(a,e,0x74)) || !game_menu_research_controls_visible(a,e,0) ||
       !game_menu_widget_visible(a,get(a,0x58abe8,0),0) || !game_menu_widget_visible(a,animation,0))return 0;
    if(get(a,e,0x14)==16)return game_menu_network_page_release(a,e);
    if(get(a,e,0x14)!=15)return game_network_session_reset(a) && game_menu_network_page_release(a,e);
    p=get(a,e,0x120);if(p && !game_menu_widget_enable(a,p,1))return 0;
    if(!control(a,get(a,e,0x11c),1,1) || !control(a,get(a,e,0x118),1,1) || !control(a,get(a,e,0xf0),1,!!get(a,0x5f0150,0)))return 0;
    for(i=0;i<4;++i) {
        GameAddress local=get(a,0x5f0208,0);int enabled=local && game_memory_read_u16(&a->memory,local+8)==i;
        if(!control(a,get(a,e,0x144+i*4),1,enabled) || !control(a,get(a,e,0x134+i*4),1,enabled))return 0;
        p=get(a,e,0x134+i*4);if(p && enabled && !game_menu_text_entry_activate(a,p))return 0;
        p=get(a,e,0x124+i*4);if(p && !game_menu_widget_visible(a,p,1))return 0;
        if(!control(a,get(a,e,0xf4+i*4),1,1))return 0;
    }
    return !a->memory.fault;
}
