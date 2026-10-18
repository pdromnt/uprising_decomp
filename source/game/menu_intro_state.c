#include "menu_movie.h"
extern int game_menu_window_clear(GameApplication *);
extern int game_menu_palette_load_named(GameApplication *,GameAddress);
extern int game_menu_cursor_mode(GameApplication *,int);
extern int game_menu_register_observer(GameApplication *,GameAddress,uint32_t,uint32_t);
extern int game_menu_unregister_tick_method(GameApplication *,GameAddress,uint32_t);
extern int game_menu_controller_keyboard_handler(GameApplication *,GameAddress,uint32_t,uint32_t);
extern int game_menu_unregister_keyboard_method(GameApplication *,GameAddress,uint32_t);
extern int game_menu_initial_widgets_construct(GameApplication *,GameAddress);
extern uint32_t game_menu_milliseconds(GameApplication *);
extern int game_menu_movie_volume(GameApplication *,GameAddress,uint32_t);
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
static int show(GameApplication *a,GameAddress p,uint32_t y,uint32_t z) {
    return game_menu_widget_position(a,p,0,y,z,0) && game_menu_widget_visible(a,p,1);
}
static int click(GameApplication *a,GameAddress slot,uint32_t next) {
    GameAddress p=game_menu_click_region_create(a,362,69,1000,next,0);if(!p)return 0;put(a,slot,p);
    put(a,p+0xc,0);put(a,p+0x10,0);put(a,p+0x14,640);put(a,p+0x18,480);
    return game_menu_widget_enable(a,p,1);
}
static int timed(GameApplication *a,GameAddress controller,uint32_t next) {
    put(a,0x628790,next);put(a,0x598128,game_menu_milliseconds(a));
    return game_menu_register_observer(a,controller,0x52c1b0,0);
}
/*Real startup chain: static publisher logo25 -> optional publisher movie24 ->
 * optional game movie26 -> title20 -> main menu2. Existing native images and
 * decoders own presentation; missing files follow original transitions. */
int game_menu_intro_state(GameApplication *a,GameAddress event,uint32_t state,uint32_t phase) {
    GameAddress controller=get(a,0x59d4c0),image_slot=0,movie_slot=0,click_slot=0,p;
    GameAddress palette=0,background=0;uint32_t next;char path[128];unsigned i;
    int ok=1;
    if(state==25){image_slot=0x58b03c;click_slot=0x58b040;palette=0x58c5c8;background=0x58c5e8;next=24;}
    else if(state==24){image_slot=0x58b048;click_slot=0x58b04c;movie_slot=0x58b050;next=26;}
    else if(state==26){click_slot=0x58b054;movie_slot=0x58b058;next=20;}
    else if(state==20){image_slot=0x58b05c;palette=0x58c68c;background=0x58c6a8;next=2;}
    else return 0;
    if(phase==1) {
        GameAddress slots[4]={click_slot,image_slot,movie_slot,state==25?0x58b044:state==20?0x58b060:0};
        if(!game_menu_unregister_tick_method(a,controller,0x52c1b0) ||
           !game_menu_unregister_tick_method(a,controller,0x524730) ||
           !game_menu_unregister_keyboard_method(a,controller,0x5246f0))ok=0;
        for(i=0;i<4;++i)if(slots[i] && (p=get(a,slots[i]))!=0) {
            if(!game_menu_widget_destroy(a,p))ok=0;else put(a,slots[i],0);
        }
        if(!game_menu_window_clear(a))ok=0;
        if(state==20 && (!game_menu_palette_load_named(a,0x58c6cc) || !game_menu_initial_widgets_construct(a,event)))ok=0;
        return ok && !a->memory.fault;
    }
    if(phase)return !a->memory.fault;
    if(state==20 && !game_menu_cursor_mode(a,0))goto fail;
    if(!game_menu_window_clear(a))goto fail;
    if(state==24 || state==26) {
        if(game_menu_movie_path(a,state==24?"cyclonea.smk":"cinem.smk",path,sizeof(path))) {
            p=game_menu_movie_create(a,path,state==24?640:UINT32_MAX,state==24?480:UINT32_MAX);
            if(!p)goto fail;put(a,movie_slot,p);put(a,p+0x48,0x524710);put(a,p+0x4c,next);
            if(!game_menu_movie_volume(a,p,get(a,0x58ac0c)) || !show(a,p,state==24?0:40,6) || !game_menu_movie_play(a,p))goto fail;
        } else if(state==26) {
            if(!game_menu_register_observer(a,controller,0x524730,next))goto fail;
            return !a->memory.fault;
        } else {palette=0x58c628;background=0x58c64c;}
    }
    if(background) {
        if(!game_menu_palette_load_named(a,palette))goto fail;
        p=game_menu_image_create(a,UINT32_MAX,background);if(!p)goto fail;put(a,image_slot,p);
        if(!show(a,p,0,0) || !timed(a,controller,next))goto fail;
    }
    if(click_slot && !click(a,click_slot,next))goto fail;
    if(state!=20 && !game_menu_controller_keyboard_handler(a,controller,0x5246f0,next))goto fail;
    return !a->memory.fault;
fail:(void)game_menu_intro_state(a,event,state,1);return 0;
}
