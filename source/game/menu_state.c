#include "menu_widgets.h"
#include "menu_settings.h"
#include "menu_network.h"

extern int game_menu_register_tick(GameApplication *, GameAddress, GameAddress, uint32_t, uint32_t);
extern int game_menu_register_delivery(GameApplication *, GameAddress, GameAddress, uint32_t, uint32_t);
extern int game_menu_bootstrap(GameApplication *, GameAddress);
extern int game_menu_initial_widgets_construct(GameApplication *, GameAddress);
extern int game_menu_shutdown_systems(GameApplication *); /* Actual 00541230. */
extern int game_menu_audio_restore(GameApplication *);
extern int game_menu_command_start(GameApplication *);
extern int game_menu_music_start(GameApplication *, uint32_t);
extern int game_menu_strategic_state(GameApplication *,uint32_t);
extern int game_menu_graphics_state(GameApplication *,GameAddress,uint32_t);
extern int game_menu_options_state(GameApplication *,GameAddress,uint32_t);
extern int game_menu_audio_state(GameApplication *,GameAddress,uint32_t);
extern int game_menu_credits_state(GameApplication *,GameAddress,uint32_t);
extern int game_menu_quick_state(GameApplication *,GameAddress,uint32_t);

/* 004929e0 state 2. Other routes remain genuine unresolved native services. */
static int main_menu(GameApplication *a,GameAddress event,uint32_t phase) {
    static const GameAddress names[5][4]={
        {0x58c754u,0x58c730u,0x58c70cu,0},
        {0x58c7bcu,0x58c798u,0x58c778u,0},
        {0x58c828u,0x58c804u,0x58c7e0u,0},
        {0x58c894u,0x58c870u,0x58c84cu,0},
        {0x58c8fcu,0x58c8d8u,0x58c8b8u,0}};
    static const uint32_t commands[]={0,13,4,8,1};
    GameMemory *m=&a->memory;
    GameAddress background,extra;
    unsigned i;
    if (phase==0u) {
        background=game_menu_image_create(a,UINT32_MAX,0x58c6e8u);
        if (!background) return 0;
        game_memory_write_u32(m,0x58b064u,background);
        if (!game_menu_widget_position(a,background,0,0,0,0) ||
            !game_menu_widget_visible(a,background,1) || !game_menu_command_start(a)) goto fail;
        for (i=0;i<5u;++i) {
            GameAddress button=game_menu_button_create(a,0x78u+i,names[i],
                i?0x45u:UINT32_MAX,i?1000u:UINT32_MAX,commands[i],i?0:0x52cb00u);
            if (!button) goto fail;
            game_memory_write_u32(m,0x58b068u+i*4u,button);
            if (!game_menu_widget_position(a,button,129u,137u+i*54u,10u,0) ||
                !game_menu_widget_visible(a,button,1) || !game_menu_widget_enable(a,button,1)) goto fail;
            game_memory_write_u32(m,button+0x50u,0x90u);
            game_memory_write_u32(m,button+0x54u,i?0x91u:UINT32_MAX);
        }
        extra=game_memory_read_u32(m,event+0x60u);
        if (!extra || !game_menu_widget_position(a,extra,544u,383u,12u,0) ||
            !game_menu_widget_visible(a,extra,1)) goto fail;
        i=game_memory_read_u32(m,event+0x10u);
        /* The original menu ignores unavailable CD audio; retain its error
         * owner for the audio page and shutdown instead of failing the menu. */
        if(i==20u || i==0u)(void)game_menu_music_start(a,0);
        return !m->fault;
    }
    if (phase!=1u) return !m->fault; /* This state has no transition animation. */
    extra=game_memory_read_u32(m,event+0x60u);
    if (extra && !game_menu_widget_visible(a,extra,0)) return 0;
    for (i=0;i<6u;++i) {
        GameAddress slot=0x58b064u+i*4u,object=game_memory_read_u32(m,slot);
        if (object && !game_menu_widget_destroy(a,object)) return 0;
        game_memory_write_u32(m,slot,0);
    }
    return !m->fault;
fail:
    (void)main_menu(a,event,1u); return 0;
}
/*004929e0 state3: the real single-player route picker. Its commands connect
 * quick/campaign/training/load selection; those destination states remain work. */
static int single_player(GameApplication *a,GameAddress event,uint32_t phase) {
    static const struct {GameAddress slot,normal,hover,pressed;uint32_t id,state,y;} buttons[]={
        {0x58b084,0x58c980,0x58c960,0x58c940,160,9,137},
        {0x58b088,0x58c9dc,0x58c9bc,0x58c9a0,161,29,191},
        {0x58b080,0x58ca3c,0x58ca1c,0x58c9fc,162,23,245},
        {0x58b08c,0x58ca9c,0x58ca7c,0x58ca5c,162,12,299},
        {0x58b090,0x58cafc,0x58cadc,0x58cabc,163,2,353}};
    static const GameAddress retire[]={0x58b080,0x58b090,0x58b084,0x58b088,0x58b08c,0x58b07c};
    GameMemory *m=&a->memory;GameAddress p;unsigned i;
    if(!phase) {
        p=game_menu_image_create(a,UINT32_MAX,0x58c91c);if(!p)goto fail;
        game_memory_write_u32(m,0x58b07c,p);
        if(!game_menu_widget_position(a,p,0,0,0,0) || !game_menu_widget_visible(a,p,1))goto fail;
        for(i=0;i<5;++i) {
            GameAddress names[4]={buttons[i].normal,buttons[i].hover,buttons[i].pressed,0};
            p=game_menu_button_create(a,buttons[i].id,names,69,1000,buttons[i].state,0);if(!p)goto fail;
            game_memory_write_u32(m,buttons[i].slot,p);
            if(!game_menu_widget_position(a,p,129,buttons[i].y,10,0) ||
               !game_menu_widget_visible(a,p,1) || !game_menu_widget_enable(a,p,1))goto fail;
            game_memory_write_u32(m,p+0x50,144);game_memory_write_u32(m,p+0x54,145);
        }
        p=game_memory_read_u32(m,event+0x60);
        if(!game_menu_widget_position(a,p,544,383,12,0) || !game_menu_widget_visible(a,p,1))goto fail;
        return !m->fault;
    }
    if(phase!=1)return !m->fault;
    if(!game_menu_widget_visible(a,game_memory_read_u32(m,event+0x60),0))return 0;
    for(i=0;i<6;++i) {
        p=game_memory_read_u32(m,retire[i]);if(p && !game_menu_widget_destroy(a,p))return 0;
        game_memory_write_u32(m,retire[i],0);
    }
    return !m->fault;
fail:(void)single_player(a,event,1);return 0;
}
int game_menu_state(GameApplication *a,GameAddress event,uint32_t state,uint32_t phase) {
    GameMemory *m=&a->memory;
    if(state==18u)return game_menu_strategic_state(a,phase);
    if(state==19u) {
        extern int game_menu_results_state(GameApplication *,GameAddress,uint32_t);
        return game_menu_results_state(a,event,phase);
    }
    if(state==30u || state==31u) {
        extern int game_menu_campaign_route(GameApplication *,uint32_t,uint32_t);
        return game_menu_campaign_route(a,state,phase);
    }
    if(state==20u || state==24u || state==25u || state==26u) {
        extern int game_menu_intro_state(GameApplication *,GameAddress,uint32_t,uint32_t);
        return game_menu_intro_state(a,event,state,phase);
    }
    if(state==13u)return game_menu_network_provider_state(a,event,phase);
    if(state==14u)return game_menu_network_session_state(a,event,phase);
    if(state==15u)return game_menu_network_page_state(a,event,phase);
    if(state==16u) {
        extern int game_menu_register_observer(GameApplication *,GameAddress,uint32_t,uint32_t);
        extern int game_menu_unregister_tick_method(GameApplication *,GameAddress,uint32_t);
        GameAddress controller=game_memory_read_u32(m,0x59d4c0);
        if(!phase)return game_menu_register_observer(a,controller,0x5234c0,0);
        if(phase==1)return game_menu_unregister_tick_method(a,controller,0x5234c0);
        return !m->fault;
    }
    if(state==22u) {
        extern int game_menu_network_research_state(GameApplication *,GameAddress,uint32_t);
        return game_menu_network_research_state(a,event,phase);
    }
    if(state==32u) {
        extern int game_menu_research_popup_state(GameApplication *,GameAddress,uint32_t);
        return game_menu_research_popup_state(a,event,phase);
    }
    if(state==11u) {
        extern int game_menu_research_state(GameApplication *,GameAddress,uint32_t);
        return game_menu_research_state(a,event,phase);
    }
    if(state==17u) {
        extern int game_menu_game_state(GameApplication *,GameAddress,uint32_t);
        return game_menu_game_state(a,event,phase);
    }
    if(state==27u || state==29u) {
        extern int game_menu_video_state(GameApplication *,GameAddress,uint32_t,uint32_t);
        return game_menu_video_state(a,event,state,phase);
    }
    if (state==12u) {
        extern int game_menu_save_state(GameApplication *,GameAddress,uint32_t);
        return game_menu_save_state(a,event,phase);
    }
    if (state==23u) {
        extern int game_menu_training_state(GameApplication *,GameAddress,uint32_t);
        return game_menu_training_state(a,event,phase);
    }
    if (state==2u) return main_menu(a,event,phase);
    if (state==3u) return single_player(a,event,phase);
    if (state==6u) return game_menu_graphics_state(a,event,phase);
    if (state==4u) return game_menu_options_state(a,event,phase);
    if (state==7u) return game_menu_audio_state(a,event,phase);
    if (state==5u) return game_menu_keyboard_state(a,event,phase);
    if (state==8u) return game_menu_credits_state(a,event,phase);
    if (state==9u) return game_menu_quick_state(a,event,phase);
    if (state==0u) {
        if (!phase) {
            GameAddress controller=game_memory_read_u32(m,0x59d4c0u);
            return game_menu_register_tick(a,controller,event,0x490220u,0) &&
                game_menu_register_delivery(a,controller,event,1000u,0x4901b0u) &&
                game_menu_bootstrap(a,event);
        }
        if (phase==1u && game_memory_read_u32(m,event+0x14u)!=25u)
            return game_menu_initial_widgets_construct(a,event);
        return !m->fault;
    }
    if (state==1u) {
        if (!phase) {
            if (!game_menu_settings_end(a)) return 0;
            game_memory_write_u32(m,0x59d4c8u,1u);
            if (!game_menu_shutdown_systems(a) || !game_menu_audio_restore(a)) return 0;
        }
        return !m->fault;
    }
    /*4929e0's jump table maps10/21/28 and out-of-range values straight to
     * its return. Every executable state above selects its real native family. */
    return !m->fault;
}
