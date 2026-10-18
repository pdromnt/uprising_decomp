#include "menu_widgets.h"
#include "menu_music.h"
extern uint32_t game_menu_milliseconds(GameApplication *);
extern int game_menu_palette_load_named(GameApplication *,GameAddress);
extern int game_menu_screen_mask(GameApplication *,int);
extern int game_menu_present_frame(GameApplication *,uint32_t);
extern int game_menu_register_tick(GameApplication *,GameAddress,GameAddress,uint32_t,uint32_t);
extern int game_menu_unregister_tick_method(GameApplication *,GameAddress,uint32_t);
extern int game_menu_request(GameApplication *,GameAddress,uint32_t,uint32_t,uint32_t);
static uint32_t get(GameApplication *a,GameAddress p,unsigned o){return game_memory_read_u32(&a->memory,p+o);}
static void put(GameApplication *a,GameAddress p,unsigned o,uint32_t v){game_memory_write_u32(&a->memory,p+o,v);}
static int clear(GameApplication *a) {return game_menu_screen_mask(a,0) && game_menu_present_frame(a,0);}
/*0052c270: the global timed callback advances the existing credit image and
 * requests main-menu return only after its bottom leaves the viewport. */
int game_menu_credits_tick(GameApplication *a) {
    uint32_t now=game_menu_milliseconds(a);GameAddress e,p;
    if((int32_t)(get(a,0x59812c,0)+get(a,0x598130,0))>(int32_t)now)return !a->memory.fault;
    put(a,0x598130,0,now);e=get(a,0x59d4c4,0);p=get(a,e,0xc);
    if(!p || !game_menu_widget_position(a,p,get(a,p,0x20),get(a,p,0x24)-1,get(a,p,0x40),0))return 0;
    put(a,p,0x18,1);
    return (int32_t)get(a,p,0x2c)>140? !a->memory.fault:
        game_menu_request(a,get(a,0x59d4c0,0),69,1000,2);
}
/*004929e0 state8: background/click owner, one composed text surface, shared
 * palette/music and timed scroll. Existing storage is the authoritative owner. */
int game_menu_credits_state(GameApplication *a,GameAddress e,uint32_t phase) {
    GameAddress p,controller=get(a,0x59d4c0,0);unsigned i,n;char text[1024];int ok=1;
    if(!phase) {
        put(a,e,0xc,0);put(a,0x598130,0,0);
        if(!clear(a))goto fail;
        (void)game_menu_palette_load_named(a,0x58d2c0);
        p=game_menu_image_create(a,UINT32_MAX,0x58d2e0);if(!p)goto fail;put(a,0x58b0e8,0,p);
        if(!game_menu_widget_position(a,p,0,0,0,0) || !game_menu_widget_visible(a,p,1))goto fail;
        p=game_menu_click_region_create(a,362,69,1000,2,0);if(!p)goto fail;put(a,0x58b0ec,0,p);
        put(a,p,0xc,0);put(a,p,0x10,0);put(a,p,0x14,640);put(a,p,0x18,480);
        if(!game_menu_widget_enable(a,p,1))goto fail;
        n=get(a,0x58b028,0);if(!n || n>4096)goto fail;
        p=game_menu_image_surface_create(a,400,n*15,0);if(!p)goto fail;put(a,e,0xc,p);
        for(i=0;i<n;++i) {
            GameAddress first=get(a,0x58ac10+i*8,0),label=game_memory_read_u8(&a->memory,first)?first:get(a,0x58ac14+i*8,0);
            unsigned j;
            for(j=0;j<sizeof(text);++j){text[j]=(char)game_memory_read_u8(&a->memory,label+j);if(!text[j])break;}
            if(j==sizeof(text) || a->memory.fault ||
               !game_menu_text_surface_paint(a,p,4,text,label==first?0x021c532bu:0x02529c94u,label==first?0:60,(int32_t)(i*15),0,0))goto fail;
        }
        put(a,p,0x154,116);put(a,p,0x158,147);put(a,p,0x15c,525);put(a,p,0x160,395);put(a,p,0x164,1);
        if(!game_menu_image_mode(a,p,101) || !game_menu_widget_position(a,p,116,400,5,0) ||
           !game_menu_widget_visible(a,p,1) || !game_menu_register_tick(a,controller,0,0x52c270,0))goto fail;
        if(get(a,0x59d828,0))(void)game_menu_music_play(a,get(a,0x59d828,0),1);
        return !a->memory.fault;
    }
    if(phase!=1)return !a->memory.fault;
    if(!game_menu_unregister_tick_method(a,controller,0x52c270))ok=0;
    p=get(a,e,0xc);if(p && !game_menu_widget_destroy(a,p))ok=0;else put(a,e,0xc,0);
    for(i=0;i<2;++i){GameAddress slot=0x58b0e8+i*4;p=get(a,slot,0);if(p && !game_menu_widget_destroy(a,p))ok=0;else put(a,slot,0,0);}
    if(!clear(a))ok=0;
    (void)game_menu_palette_load_named(a,0x58d304);
    if(get(a,0x59d828,0))(void)game_menu_music_play(a,get(a,0x59d828,0),0);
    return ok && !a->memory.fault;
fail:(void)game_menu_credits_state(a,e,1);return 0;
}
