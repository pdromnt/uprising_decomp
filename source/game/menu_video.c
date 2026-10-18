#include "menu_movie.h"
#include "menu_music.h"
#include <string.h>
extern int game_menu_request(GameApplication *,GameAddress,uint32_t,uint32_t,uint32_t);
extern int game_menu_register_tick(GameApplication *,GameAddress,GameAddress,uint32_t,uint32_t);
extern int game_menu_unregister_tick_method(GameApplication *,GameAddress,uint32_t);
extern int game_menu_controller_keyboard_handler(GameApplication *,GameAddress,uint32_t,uint32_t);
extern int game_menu_window_clear(GameApplication *);
extern int game_menu_palette_load_named(GameApplication *,GameAddress);
extern uint32_t game_menu_milliseconds(GameApplication *);
static uint32_t get(GameApplication *a,GameAddress p,unsigned o){return game_memory_read_u32(&a->memory,p+o);}
static void put(GameApplication *a,GameAddress p,unsigned o,uint32_t v){game_memory_write_u32(&a->memory,p+o,v);}
static int name_read(GameApplication *a,char name[128]) {
    unsigned i;for(i=0;i<128;++i){name[i]=(char)game_memory_read_u8(&a->memory,0x629808+i);if(!name[i])return !a->memory.fault;}return 0;
}
/*0049838e/00498964: Training and campaign introduction share actual movie,
 * click-region and keyboard consumers. Missing media takes the original queued
 * next-state route; an unavailable codec for an existing file is a failure. */
int game_menu_video_state(GameApplication *a,GameAddress event,uint32_t state,uint32_t phase) {
    static const GameAddress palettes[]={0x58dc70,0x58dcc0,0x58dd10},backgrounds[]={0x58dc90,0x58dce0,0x58dd30};
    static const char *levels[]={"Training1","Training2","Training3"},*movies[]={"train1.smk","train2.smk","train3.smk"};
    GameAddress movie_slot=state==29?0x58b154:0x58b148,bg_slot=state==29?0x58b150:0x58b144,region_slot=state==29?0x58b158:0x58b14c;
    GameAddress controller=get(a,0x59d4c0,0),music=get(a,0x59d828,0),p;uint32_t next=state==29?18:17;char level[128],path[128];unsigned i=3;int ok=1;(void)event;
    if(state!=27 && state!=29)return 0;
    if(!phase) {
        if(music && !game_menu_music_stop(a,music))goto fail;
        if(!game_menu_window_clear(a))goto fail;
        if(state!=29) {
            if(!name_read(a,level))goto fail;
            for(i=0;i<3;++i)if(strstr(level,levels[i]))break;
        }
        if((state!=29 && i==3) || !game_menu_movie_path(a,state==29?"campintr.smk":movies[i],path,sizeof(path))) {
            if(!game_menu_register_tick(a,controller,0,0x524730,next))goto fail;
        }else {
            if(state==29)p=game_menu_image_surface_create(a,640,480,0);
            else {
                if(!game_menu_palette_load_named(a,palettes[i]))goto fail;
                p=game_menu_image_create(a,UINT32_MAX,backgrounds[i]);
            }
            if(!p)goto fail;
            put(a,bg_slot,0,p);
            if(!game_menu_widget_position(a,p,0,0,0,0) || !game_menu_widget_visible(a,p,1))goto fail;
            p=game_menu_movie_create(a,path,UINT32_MAX,UINT32_MAX);if(!p)goto fail;put(a,movie_slot,0,p);
            put(a,p,0x48,0x524710);put(a,p,0x4c,next);
            if(!game_menu_widget_position(a,p,state==29?0:160,state==29?50:120,6,0) || !game_menu_movie_play(a,p) || !game_menu_widget_visible(a,p,1))goto fail;
            p=game_menu_click_region_create(a,0x16a,69,1000,next,0);if(!p)goto fail;put(a,region_slot,0,p);
            put(a,p,0xc,0);put(a,p,0x10,0);put(a,p,0x14,640);put(a,p,0x18,480);
            if(!game_menu_widget_enable(a,p,1) || !game_menu_controller_keyboard_handler(a,controller,0x5246f0,next))goto fail;
        }
        if(state==29 && music && !game_menu_music_stop(a,music))goto fail;
        return !a->memory.fault;
    }
    if(phase!=1)return !a->memory.fault;
    if(get(a,region_slot,0)) {
        if(!game_menu_unregister_tick_method(a,controller,0x52c1b0))ok=0;
        put(a,controller,24,0);
    }
    /*Also cancel an unconsumed missing-media tick when interrupted externally. */
    if(!game_menu_unregister_tick_method(a,controller,0x524730))ok=0;
    for(i=0;i<3;++i) {
        GameAddress slot=i==0?movie_slot:i==1?bg_slot:region_slot;p=get(a,slot,0);
        if(p && !game_menu_widget_destroy(a,p))ok=0;else put(a,slot,0,0);
    }
    if(music)(void)game_menu_music_play(a,music,0);
    return ok && !a->memory.fault;
fail:(void)game_menu_video_state(a,event,state,1);return 0;
}
/*005246f0/00524710 and00524730: ordinary skip/completion/delayed delivery.
 * The keyboard callback ignores its key, including mouse-independent skips. */
int game_menu_video_action(GameApplication *a,uint32_t method,uint32_t next) {
    GameAddress controller=get(a,0x59d4c0,0);
    if(method==0x524730 && !game_menu_unregister_tick_method(a,controller,method))return 0;
    if(method==0x52c1b0) {
        if((int32_t)(get(a,0x598128,0)+5000)>(int32_t)game_menu_milliseconds(a))return !a->memory.fault;
        next=get(a,0x628790,0);
    }else if(method!=0x5246f0 && method!=0x524710 && method!=0x524730)return 0;
    return game_menu_request(a,controller,69,1000,next);
}
