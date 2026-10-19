

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "application_entry.h"
#include "portable_input_devices.h"
#include "portable_sdl.h"

/* Shipping strings use the original byte encoding and C-locale case mapping.
 * No live writer selects the obsolete Win32 LCID route. */
int game_menu_locale_uppercase(GameApplication *a,char *value,size_t capacity){
    size_t i;
    if(a->memory.fault || !value || !capacity || !memchr(value,0,capacity))return 0;
    for(i=0;value[i];++i)if(value[i]>='a' && value[i]<='z')value[i]=(char)(value[i]-'a'+'A');
    return 1;
}/* Native constructor/consumer/destructor contracts for 004aa780/004ccbf0/
 * 0048fe60, presentation, script selection and the level/game route. They are
 * intentionally link blockers until their real algorithms are connected. */
extern GameAddress game_menu_window_construct(GameApplication *, unsigned, unsigned);
extern GameAddress game_menu_controller_construct(GameApplication *);
extern GameAddress game_menu_event_construct(GameApplication *, unsigned);
extern int game_menu_resources_begin(GameApplication *);
extern void game_menu_resources_end(GameApplication *);
extern int game_menu_demo_available(GameApplication *);
extern int game_menu_demo_start(GameApplication *, unsigned);
extern int game_menu_request(GameApplication *, GameAddress, unsigned, unsigned, unsigned);
/*538c52..538cad: normalize the command-selected level and use its original
 * player identity. The strategic-screen marker is distinct from the suffix. */
int game_menu_command_route(GameApplication *app,const char *level) {
    char name[60];size_t length=strlen(level);
    /* Process argument parsing accepts either case. Preserve the actual name
     * here: appending again turned SCINTILLAE.LFL into SCINTILLAE.LFL.lfl. */
    int has_suffix=length>=4 && level[length-4]=='.' &&
        (level[length-3]=='l' || level[length-3]=='L') &&
        (level[length-2]=='f' || level[length-2]=='F') &&
        (level[length-1]=='l' || level[length-1]=='L');
    if(length+(has_suffix?0u:4u)>=sizeof(name))return 0;
    memcpy(name,level,length);
    if(!has_suffix){memcpy(name+length,".lfl",4);length+=4;}
    name[length]=0;
    return game_memory_write(&app->memory,0x5efec0,"Hacker",7) &&
        game_memory_write(&app->memory,0x629808,name,length+1);
}
extern int game_menu_command_start(GameApplication *);
extern int game_menu_controller_pointer(GameApplication *,GameAddress,uint32_t,int32_t,int32_t,uint32_t);
extern int game_menu_controller_key(GameApplication *,GameAddress,uint32_t,uint32_t);
extern int game_menu_input_poll(GameApplication *,GameAddress window); /* Actual004d0220. */
extern int game_menu_controller_tick(GameApplication *, GameAddress);
extern int game_menu_window_present(GameApplication *, GameAddress);
extern int game_menu_event_destroy(GameApplication *, GameAddress);
extern void game_menu_controller_destroy(GameApplication *, GameAddress);
extern void game_menu_window_close(GameApplication *, GameAddress);
extern void game_menu_window_destroy(GameApplication *, GameAddress);

/* 00515820: real cursor service, with the original published state. */
int game_menu_cursor_mode(GameApplication *app,int visible) {
    if(game_application_isolated_input(app)){
        game_memory_write_u32(&app->memory,0x597f8cu,visible!=0);return !app->memory.fault;
    }
    game_memory_write_u32(&app->memory,0x597f8cu,visible!=0);
    return (visible?SDL_ShowCursor():SDL_HideCursor()) && !app->memory.fault;
}
int game_menu_command_start(GameApplication *app) {
    return game_menu_cursor_mode(app,1);
}

/* Menu controllers keep their original scalar key/message IDs. The event
 * source and positions are SDL values; no MSG, LPARAM or host pointer crosses
 * into a game record. Text input and navigation are delivered separately. */
int game_menu_event_forward(GameApplication *app,const SDL_Event *event){
    GameMemory *m=&app->memory;
    GameAddress controller=game_memory_read_u32(m,0x59d4c0u);
    uint32_t code=0,buttons=0;int32_t point[2];
    if(game_application_isolated_input(app))return !m->fault;
    switch(event->type){
    case SDL_EVENT_KEY_DOWN:{
        uint32_t key=0,flags=0x1000000u;
        switch(event->key.scancode){
        case SDL_SCANCODE_LEFT:key=0x25;break;case SDL_SCANCODE_UP:key=0x26;break;
        case SDL_SCANCODE_RIGHT:key=0x27;break;case SDL_SCANCODE_DOWN:key=0x28;break;
        case SDL_SCANCODE_HOME:key=0x24;break;case SDL_SCANCODE_END:key=0x23;break;
        case SDL_SCANCODE_PAGEUP:key=0x21;break;case SDL_SCANCODE_PAGEDOWN:key=0x22;break;
        case SDL_SCANCODE_INSERT:key=0x2d;break;case SDL_SCANCODE_DELETE:key=0x2e;break;
        case SDL_SCANCODE_BACKSPACE:key=8;flags=0;break;
        case SDL_SCANCODE_TAB:key=9;flags=0;break;
        case SDL_SCANCODE_RETURN:case SDL_SCANCODE_KP_ENTER:key=13;flags=0;break;
        case SDL_SCANCODE_ESCAPE:key=27;flags=0;break;
        default:return !m->fault;
        }
        return game_menu_controller_key(app,controller,key,flags);
    }
    case SDL_EVENT_TEXT_INPUT:{
        const unsigned char *text=(const unsigned char *)event->text.text;
        /* The game's font/edit records use bytes, not UTF-8. Reject unsupported
         * characters instead of inserting individual UTF-8 continuation bytes. */
        for(;*text;++text)if(*text<128 && !game_menu_controller_key(app,controller,*text,0))return 0;
        return !m->fault;
    }
    case SDL_EVENT_MOUSE_MOTION:
        code=0x200;point[0]=(int32_t)event->motion.x;point[1]=(int32_t)event->motion.y;
        buttons=event->motion.state;break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:case SDL_EVENT_MOUSE_BUTTON_UP:{
        int down=event->type==SDL_EVENT_MOUSE_BUTTON_DOWN;
        if(event->button.button==SDL_BUTTON_LEFT){
            /* Menus register down/up regions. Gameplay owns its separate
             * buffered double-click handling (including citadel selection). */
            code=down?0x201:0x202;
            game_memory_write_u32(m,0x59d4ccu,down);
            if(!down)game_memory_write_u32(m,0x627ab8u,0);
        }else if(event->button.button==SDL_BUTTON_RIGHT)code=down?0x204:0x205;
        else return !m->fault;
        point[0]=(int32_t)event->button.x;point[1]=(int32_t)event->button.y;
        buttons=SDL_GetMouseState(NULL,NULL);break;
    }
    default:return !m->fault;
    }
    /* Internal flags retain left=1/right=2/middle=16 rather than SDL's order. */
    buttons=((buttons&SDL_BUTTON_LMASK)?1u:0u)|((buttons&SDL_BUTTON_RMASK)?2u:0u)|
        ((buttons&SDL_BUTTON_MMASK)?16u:0u)|((SDL_GetModState()&SDL_KMOD_SHIFT)?4u:0u)|
        ((SDL_GetModState()&SDL_KMOD_CTRL)?8u:0u);
    return game_menu_pointer_from_client(app,point) &&
        game_menu_controller_pointer(app,controller,code,point[0],point[1],buttons);
}
/* State changes discard pending physical input, while servicing close/focus. */
int game_menu_transition_notify(GameApplication *app){
    int ok=game_menu_pump_events(app,0);game_input_devices_discard(app);return ok;
}
/* 00538b40: pump SDL events and drive the existing menu/game owners. */
int game_application_menu_loop(GameApplication *app,const char *level) {
    GameMemory *m = &app->memory;
    GameAddress window = 0, controller = 0, event = 0;
    unsigned request;
    uint32_t last_presented = UINT32_MAX;
    GameAddress paced_drawer = 0;
    uint32_t drawer_frame = UINT32_MAX, drawer_deadline = 0;
    int resources = 0, ok = 0;
    const char *windowed_setting=getenv("UPRISING_RECOMP_WINDOWED");
    int background_windowed=windowed_setting && !strcmp(windowed_setting,"1");
    const char *stage = "window construction";

    window = game_menu_window_construct(app,640u,480u);
    if (!window) goto done;
    game_memory_write_u32(m, 0x59d4bcu, window);
    stage = "controller construction";
    controller = game_menu_controller_construct(app);
    if (!controller) goto done;
    game_memory_write_u32(m, 0x59d4c0u, controller);
    stage = "event construction";
    event = game_menu_event_construct(app, 0x45u);
    if (!event) goto done;
    game_memory_write_u32(m, 0x59d4c4u, event);
    stage = "menu resources";
    if (!game_menu_resources_begin(app)) goto done;
    resources = 1;
    stage = "initial route";
    if (game_menu_demo_available(app) && game_menu_demo_start(app, 1u)) request = 15u;
    else if (!game_memory_read_u32(m, 0x58abe4u)) request = 2u;
    else if (!*level) request = 25u;
    else {
        game_memory_write_u32(m, 0x58abdcu, 1u);
        if (!game_menu_command_route(app, level)) goto done;
        if (strstr(level, "gotocampaign")) {
            if (!game_menu_request(app, controller, 0x45u, 1000u, 18u) ||
                !game_menu_command_start(app)) goto done;
            SDL_ShowCursor();
            goto pump;
        }
        request = 17u;
    }
    if (!game_menu_request(app, controller, 0x45u, 1000u, request)) goto done;
pump:
    stage = "message/frame pump";
    while (!m->fault && !game_memory_read_u32(m, 0x59d4c8u)) {
        uint32_t frame_started=(uint32_t)SDL_GetTicks();

        if(!game_menu_pump_events(app,1)){stage="event delivery";goto done;}
        /* Network handshakes and the guarded windowed profile keep their menu
         * owners running without stealing desktop focus. Physical events and
         * device polling remain focus-gated; activation still releases devices.
         * The ordinary inactive profile sleeps until a message instead of
         * busy-spinning while its controller is deliberately suspended. */
        if (game_memory_read_u32(m, 0x6270f4u) ||
            game_memory_read_u32(m, 0x5eff04u) || background_windowed) {
            if (game_memory_read_u32(m, 0x6270f4u) &&
                game_memory_read_u32(m, event + 0x3cu) == 5u &&
                !game_menu_input_poll(app, window)) {stage="input poll";goto done;}
            /* Captured sliders/selectors publish their temporary controller.
             * Tick that owner, which forwards its parent's tick itself. */
            if (!game_menu_controller_tick(app, game_memory_read_u32(m,0x59d4c0u)))
                {stage="controller tick";goto done;}
            /* Windowed presentation lacks retail's fullscreen refresh wait.
             * Preserve the overlay's 17 ms cadence only while the shared
             * one-shot drawer selects new frames; movies and steady menus
             * retain their own clocks. Never catch up in a burst after a stall. */
            {
                GameAddress animation=game_memory_read_u32(m,event+0x60u);
                uint32_t frame=animation?game_memory_read_u32(m,animation+0x300u):0;
                if(animation && game_memory_read_u32(m,animation+0x308u) &&
                   (animation!=paced_drawer || frame!=drawer_frame)) {
                    uint32_t now=(uint32_t)SDL_GetTicks();
                    int32_t wait;
                    if(animation!=paced_drawer)drawer_deadline=now+17u;
                    wait=(int32_t)(drawer_deadline-now);
                    if(wait>0)SDL_Delay((uint32_t)wait);
                    now=(uint32_t)SDL_GetTicks();
                    if((int32_t)(now-drawer_deadline)>17)drawer_deadline=now;
                    drawer_deadline+=17u;
                    paced_drawer=animation;drawer_frame=frame;
                }else {
                    paced_drawer=animation;drawer_frame=frame;
                    drawer_deadline=(uint32_t)SDL_GetTicks()+17u;
                }
            }
            if (!game_menu_window_present(app, window)) {stage="window presentation";goto done;}
            {
                uint32_t state=game_memory_read_u32(m,event+0x3cu);
                if(state!=last_presented) {
                    char number[16];snprintf(number,sizeof(number),"%lu",(unsigned long)state);
                    game_application_log(app,"native menu presented state %s\n",number);
                    last_presented=state;
                }
            }
            /* Software/windowed presentation has no fullscreen refresh wait.
             * Steady menus used to spin at full CPU between input messages.
             * Include controller/presentation time, including nested gameplay,
             * and never accumulate missed frames after a slow operation. */
            if(!m->fault && !game_memory_read_u32(m,0x59d4c8u)) {
                uint32_t elapsed=(uint32_t)SDL_GetTicks()-frame_started;
                if(elapsed<17u)SDL_Delay(17u-elapsed);
            }
        }else if(!m->fault && !game_memory_read_u32(m,0x59d4c8u))
            SDL_Delay(10);
    }
    ok = !m->fault;
done:
    if (!ok) {
        fprintf(stderr,"native menu failed: %s; memory fault=%d at %08lx size=%lu\n",
            stage,m->fault,(unsigned long)m->fault_address,(unsigned long)m->fault_size);
        fflush(stderr);
    }
    if (event && !game_menu_event_destroy(app, event))ok=0;
    game_memory_write_u32(m, 0x59d4c4u, 0u);
    if (controller) game_menu_controller_destroy(app, controller);
    game_memory_write_u32(m, 0x59d4c0u, 0u);
    if (resources) game_menu_resources_end(app);
    if (window) {
        game_menu_window_close(app, window);
        game_menu_window_destroy(app, window);
    }
    game_memory_write_u32(m, 0x59d4bcu, 0u);
    return ok && !m->fault;
}
