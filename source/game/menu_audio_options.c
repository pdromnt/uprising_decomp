#include "menu_widgets.h"
#include "menu_sound.h"
#include "menu_music.h"
extern uint32_t game_menu_milliseconds(GameApplication *);
extern int game_menu_register_tick(GameApplication *,GameAddress,GameAddress,uint32_t,uint32_t);
extern int game_menu_unregister_tick_method(GameApplication *,GameAddress,uint32_t);
static uint32_t get(GameApplication *a,GameAddress p,unsigned o){return game_memory_read_u32(&a->memory,p+o);}
static void put(GameApplication *a,GameAddress p,unsigned o,uint32_t v){game_memory_write_u32(&a->memory,p+o,v);}
static const unsigned slots[]={0xbc,0xc0,0xc4,0xc8,0xcc,0xec,0xd0,0xd4,0xdc,0xd8};
/* 00492690: move existing audio controls with their common panel. */
static int layout(GameApplication *a,GameAddress e,uint32_t x,uint32_t y,uint32_t z) {
    static const unsigned dx[]={409,202,202,202,465,498,498,137,43},dy[]={112,41,77,113,82,72,91,33,33};unsigned i;
    if(!game_menu_widget_position(a,get(a,e,0xbc),x,y,z,0))return 0;
    x=get(a,get(a,e,0xbc),0x20);y=get(a,get(a,e,0xbc),0x24);
    for(i=0;i<9;++i)if(!game_menu_widget_position(a,get(a,e,slots[i+1]),x+dx[i],y+dy[i],z+1,0))return 0;
    return !a->memory.fault;
}
static int enable(GameApplication *a,GameAddress e,int enabled) {
    static const unsigned indices[]={1,2,3,4,6,7,8,9};unsigned i;
    for(i=0;i<8;++i)if(!game_menu_widget_enable(a,get(a,e,slots[indices[i]]),enabled))return 0;
    return !a->memory.fault;
}
/* 0052c940: refresh the existing owned text image at the original cadence. */
int game_menu_music_label_tick(GameApplication *a) {
    uint32_t now=game_menu_milliseconds(a),last=get(a,0x598140,0),period=get(a,0x59813c,0);char label[32];
    if((int32_t)(last+period)>=(int32_t)now)return !a->memory.fault;
    put(a,0x598140,0,now);
    return game_menu_music_title(a,label,sizeof(label)) &&
        game_menu_text_image_set_bytes(a,get(a,get(a,0x59d4c4,0),0xec),label,1);
}
/* 004929e0 state7, including 00492790 show/selection and exit ownership. */
int game_menu_audio_state(GameApplication *a,GameAddress e,uint32_t phase) {
    GameAddress p,animation=get(a,e,0x60),controller=get(a,0x59d4c0,0);uint32_t frame;unsigned i;
    if(!phase) {
        p=game_menu_image_create(a,UINT32_MAX,0x58d250);if(!p)return 0;put(a,0x58b0e0,0,p);
        if(!game_menu_widget_position(a,p,0,0,0,0) || !game_menu_widget_visible(a,p,1) ||
           !game_menu_widget_position(a,animation,544,383,12,0) || !game_menu_widget_visible(a,animation,1) || !layout(a,e,659,236,11))goto fail;
        for(i=0;i<10;++i)if(!game_menu_widget_visible(a,get(a,e,slots[i]),1))goto fail;
        p=get(a,0x59d828,0);if(!p)goto fail;
        i=!get(a,p,0xc) && !get(a,p,0x10)?0xc8:get(a,p,0x10)?0xcc:0xc4;
        if(!game_menu_toggle_state(a,get(a,e,i),2))goto fail;
        put(a,e,0x24,2);put(a,e,0x1c,100);put(a,e,0x28,3);put(a,e,0x20,100);return !a->memory.fault;
    }
    if(phase==1) {
        if(!game_menu_slider_cancel(a))return 0;
        controller=get(a,0x59d4c0,0);
        game_music_worker_join(a);
        if(!game_menu_unregister_tick_method(a,controller,0x52c940) || !enable(a,e,0) || !game_menu_widget_visible(a,animation,0))return 0;
        p=get(a,0x58b0e0,0);if(p && !game_menu_widget_destroy(a,p))return 0;put(a,0x58b0e0,0,0);
        for(i=0;i<10;++i)if(!game_menu_widget_visible(a,get(a,e,slots[i]),0))return 0;
        return !a->memory.fault;
    }
    if(phase!=2 && phase!=3)return !a->memory.fault;
    i=phase==2?0x1c:0x20;
    if(get(a,e,i)==100) {
        put(a,animation,0x308,1);put(a,animation,0x304,phase==2?1:UINT32_MAX);
        if(phase==3) {
            if(!game_menu_slider_cancel(a))return 0;
            controller=get(a,0x59d4c0,0);
            if(!game_menu_unregister_tick_method(a,controller,0x52c940) || !enable(a,e,0))return 0;
            game_music_worker_join(a);
        }
        return game_menu_sound_loop_begin(a,151,0x5f06ac);
    }
    if(!game_menu_animation_advance(a,animation,&frame))return 0;
    if(frame==UINT32_MAX) {
        put(a,e,i,1);
        if(phase==2) {
            if(!enable(a,e,1) || !game_menu_register_tick(a,controller,get(a,e,0xec),0x52c940,UINT32_MAX) ||
               !game_menu_music_label_tick(a) || !game_music_worker_start(a))return 0;
        }
        if(!game_menu_sound_loop_end(a,151,get(a,0x5f06ac,0)))return 0;
        if(phase==3)return !a->memory.fault;
    } else put(a,e,i,2);
    p=get(a,e,0xbc);
    if(!layout(a,e,get(a,p,0x20)+(phase==2?0xffffffeeu:18),236,get(a,p,0x40)))return 0;
    put(a,0x58fd3c,0,1);return !a->memory.fault;
fail:(void)game_menu_audio_state(a,e,1);return 0;
}
