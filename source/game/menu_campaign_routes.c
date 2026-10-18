#include "menu_movie.h"
#include "menu_music.h"
extern int game_menu_window_clear(GameApplication *);
extern int game_menu_palette_load_named(GameApplication *,GameAddress);
extern int game_menu_register_observer(GameApplication *,GameAddress,uint32_t,uint32_t);
extern int game_menu_unregister_tick_method(GameApplication *,GameAddress,uint32_t);
extern int game_menu_controller_keyboard_handler(GameApplication *,GameAddress,uint32_t,uint32_t);
extern int game_menu_unregister_keyboard_method(GameApplication *,GameAddress,uint32_t);
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
static int show(GameApplication *a,GameAddress p,uint32_t x,uint32_t y,uint32_t z) {
    return game_menu_widget_position(a,p,x,y,z,0) && game_menu_widget_visible(a,p,1);
}
/*498cb9/499008: campaign victory movie and briefing use the existing media,
 * image/text and input owners. Their globals are the sole published ownership. */
int game_menu_campaign_route(GameApplication *a,uint32_t state,uint32_t phase) {
    GameAddress controller=get(a,0x59d4c0),music=get(a,0x59d828),p;
    GameAddress first=state==30?0x58b15c:0x58b0fc;uint32_t next=state==30?18:17;
    unsigned i;char path[128];int ok=1;
    if(state!=30 && state!=31)return 0;
    if(phase==1) {
        if(!game_menu_unregister_keyboard_method(a,controller,0x5246f0) ||
           !game_menu_unregister_tick_method(a,controller,0x524730) ||
           !game_menu_unregister_tick_method(a,controller,0x52c1b0))ok=0;
        for(i=0;i<3;++i){GameAddress slot=first+i*4;p=get(a,slot);
            if(p && !game_menu_widget_destroy(a,p))ok=0;else put(a,slot,0);}
        /*498ffe resumes optional CD music and ignores its result. A missing
         * native CD device must not abort the ending-to-campaign transition;
         * the music owner retains its real status/error for retry and UI. */
        if(state==30 && music)(void)game_menu_music_play(a,music,0);
        return ok && !a->memory.fault;
    }
    if(phase)return !a->memory.fault;
    if(!game_menu_window_clear(a))goto fail;
    if(state==30) {
        if(game_menu_movie_path(a,"wincamp.smk",path,sizeof(path))) {
            p=game_menu_image_surface_create(a,640,480,0);if(!p)goto fail;put(a,first,p);if(!show(a,p,0,0,0))goto fail;
            p=game_menu_movie_create(a,path,UINT32_MAX,UINT32_MAX);if(!p)goto fail;put(a,first+4,p);
            put(a,p+0x48,0x524710);put(a,p+0x4c,next);
            if(!show(a,p,0,50,6) || !game_menu_movie_play(a,p))goto fail;
        }else {
            if(!game_menu_register_observer(a,controller,0x524730,next))goto fail;
            if(music && !game_menu_music_stop(a,music))goto fail;
            return !a->memory.fault;
        }
    }else {
        GameMenuTextStyle style={0,255,0,3,430,220,0,0,0};
        if(!game_menu_palette_load_named(a,0x58d454))goto fail;
        p=game_menu_image_create(a,UINT32_MAX,0x58d474);if(!p)goto fail;put(a,first,p);if(!show(a,p,0,0,0))goto fail;
        p=game_menu_text_image_create(a,0x628c00,&style);if(!p)goto fail;put(a,first+4,p);if(!show(a,p,105,182,5))goto fail;
    }
    p=game_menu_click_region_create(a,362,69,1000,next,0);if(!p)goto fail;put(a,first+8,p);
    put(a,p+0xc,0);put(a,p+0x10,0);put(a,p+0x14,640);put(a,p+0x18,480);
    if(!game_menu_widget_enable(a,p,1) || !game_menu_controller_keyboard_handler(a,controller,0x5246f0,next))goto fail;
    if(state==30 && music && !game_menu_music_stop(a,music))goto fail;
    return !a->memory.fault;
fail:(void)game_menu_campaign_route(a,state,1);return 0;
}
