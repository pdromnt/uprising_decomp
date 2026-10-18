#include "application_entry.h"
#include "input_controller.h"
#include "scene_projection_storage.h"
#include "menu_widgets.h"
#include "menu_network.h"
#include <stdio.h>

extern int game_menu_register_delivery(GameApplication *, GameAddress, GameAddress, uint32_t, uint32_t);
extern int game_menu_register_tick(GameApplication *, GameAddress, GameAddress, uint32_t, uint32_t);
extern int game_menu_unregister(GameApplication *, GameAddress, GameAddress, int);
extern int game_menu_controller_tick(GameApplication *,GameAddress);
extern int game_menu_state(GameApplication *, GameAddress, uint32_t, uint32_t);
extern int game_menu_transition_notify(GameApplication *);
extern int game_menu_event_release_assets(GameApplication *, GameAddress);
extern int game_menu_profiles_release(GameApplication *);

static int change_state(GameApplication *app, GameAddress event, uint32_t requested) {
    GameMemory *m = &app->memory;
    uint32_t current = game_memory_read_u32(m, event + 0x3cu);
    if (current != UINT32_MAX) {
        if (game_memory_read_u32(m, event + 0x28u) == 3u)
            game_memory_write_u32(m, event + 0x44u, 1u);
        if (!game_menu_state(app, event, current, 1u)) {
            fprintf(stderr,"native menu state exit failed: %lu -> %lu\n",(unsigned long)current,(unsigned long)requested);
            return 0;
        }
    }
    game_memory_write_u32(m, event + 0x10u, game_memory_read_u32(m, event + 0x3cu));
    if (!game_menu_state(app, event, requested, 0u)) {
        fprintf(stderr,"native menu state entry failed: %lu -> %lu\n",(unsigned long)current,(unsigned long)requested);
        return 0;
    }
    game_memory_write_u32(m, event + 0x3cu, requested);
    if (game_memory_read_u32(m, event + 0x24u) == 2u)
        game_memory_write_u32(m, event + 0x44u, 1u);
    return !m->fault;
}

/* 004901b0/00490150: immediate state-zero, otherwise enqueue the transition. */
static int request_state(GameApplication *app, GameAddress event, uint32_t state, int immediate) {
    if (!state || immediate) return change_state(app, event, state);
    game_memory_write_u32(&app->memory, event + 0x40u, 1u);
    game_memory_write_u32(&app->memory, event + 0x14u, state);
    return !app->memory.fault;
}

/* 00490220: native delay, exit/enter and repeated presentation phases. */
static int advance_event(GameApplication *app, GameAddress event) {
    GameMemory *m = &app->memory;
    uint32_t delay = game_memory_read_u32(m, event + 0x44u);
    uint32_t current;
    if (delay && delay < 0x80000000u) {
        game_memory_write_u32(m, event + 0x44u, delay - 1u); return !m->fault;
    }
    current = game_memory_read_u32(m, event + 0x3cu);
    if (current == UINT32_MAX) return !m->fault;
    if (game_memory_read_u32(m, event + 0x40u)) {
        if (game_memory_read_u32(m, event + 0x28u)) {
            uint32_t remaining;
            if (!game_menu_state(app, event, current, 3u)) return 0;
            remaining = game_memory_read_u32(m, event + 0x20u) - 1u;
            game_memory_write_u32(m, event + 0x20u, remaining);
            if (!remaining) game_memory_write_u32(m, event + 0x28u, 0u);
        } else {
            uint32_t pending = game_memory_read_u32(m, event + 0x40u) - 1u;
            game_memory_write_u32(m, event + 0x40u, pending);
            if (!pending) {
                if (!change_state(app, event, game_memory_read_u32(m, event + 0x14u))) return 0;
                game_memory_write_u32(m, event + 0x14u, UINT32_MAX);
                if (!game_menu_transition_notify(app)) return 0;
            }
        }
    }
    if (game_memory_read_u32(m, event + 0x24u)) {
        uint32_t remaining;
        if (!game_menu_state(app, event, game_memory_read_u32(m, event + 0x3cu), 2u)) return 0;
        remaining = game_memory_read_u32(m, event + 0x1cu) - 1u;
        game_memory_write_u32(m, event + 0x1cu, remaining);
        if (!remaining) game_memory_write_u32(m, event + 0x24u, 0u);
    }
    return !m->fault;
}

GameAddress game_menu_event_construct(GameApplication *app, unsigned kind) {
    GameMemory *m = &app->memory;
    GameAddress event = scene_projection_storage_allocate(0x178u, "portable menu event");
    static const uint32_t globals[] = {0x58abe8u,0x58abfcu,0x58ac00u,0x58abecu,0x58abf0u};
    unsigned index;
    if (!event) return 0;
    game_memory_write_u32(m, event, 0x574124u);
    game_memory_write_u32(m, event + 4u, kind);
    if (!game_input_controller_clear_initial(m, event) ||
        !game_input_controller_clear_matrix(m, event)) goto fail;
    for (index = 0; index < sizeof(globals)/sizeof(globals[0]); ++index)
        game_memory_write_u32(m, globals[index], 0u);
    for (index = 0x1cu; index <= 0x28u; index += 4u) game_memory_write_u32(m, event + index, 0u);
    game_memory_write_u32(m, event + 0x2cu, 0u);
    game_memory_write_u32(m, event + 0x30u, 0u);
    game_memory_write_u32(m, event + 0x34u, 640u);
    game_memory_write_u32(m, event + 0x38u, 480u);
    game_memory_write_u32(m, event + 0x3cu, UINT32_MAX);
    game_memory_write_u32(m, event + 0x40u, 0u);
    game_memory_write_u32(m, event + 0x44u, 0u);
    game_memory_write_u32(m, event + 0x48u, 0u);
    game_memory_write_u32(m, event + 0x154u, 1u);
    if (!game_menu_state(app, event, 0u, 0u)) goto fail;
    game_memory_write_u32(m, event + 0x3cu, 0u);
    return event;
fail:
    (void)game_menu_unregister(app, game_memory_read_u32(m, 0x59d4c0u), event, 0);
    (void)game_menu_event_release_assets(app, event);
    scene_projection_storage_free(event);
    return 0;
}

int game_menu_event_destroy(GameApplication *app, GameAddress event) {
    GameMemory *m = &app->memory;
    uint32_t current;
    int ok=game_menu_slider_cancel(app);
    if(!game_menu_unregister(app, game_memory_read_u32(m, 0x59d4c0u), event, 0))ok=0;
    current = game_memory_read_u32(m, event + 0x3cu);
    if (current != UINT32_MAX && !game_menu_state(app, event, current, 1u))ok=0;
    if(!game_menu_event_release_assets(app, event))ok=0;
    if(!game_menu_profiles_release(app))ok=0;
    game_memory_write_u32(m, event + 0x3cu, UINT32_MAX);
    game_memory_write_u32(m, event, 0x573180u);
    if(!scene_projection_storage_free(event))ok=0;
    return ok && !m->fault;
}

int game_menu_deliver_native(GameApplication *app, uint32_t method,
    GameAddress receiver, uint32_t argument, uint32_t *result) {
    *result = 1u;
    switch (method) {
    case 0x5313a0u: {
        extern int game_campaign_purchase_tick(GameApplication *);
        return game_campaign_purchase_tick(app);
    }
    case 0x44db20u:return game_menu_research_preview_tick(app,receiver);
    case 0x5232f0u: {
        extern int game_menu_game_start(GameApplication *);
        return game_menu_game_start(app);
    }
    case 0x524730u:case 0x52c1b0u: {
        extern int game_menu_video_action(GameApplication *,uint32_t,uint32_t);
        return game_menu_video_action(app,method,argument);
    }
    case 0x44c220u: {
        extern int game_campaign_scene_tick(GameApplication *,GameAddress);
        return game_campaign_scene_tick(app,receiver);
    }
    case 0x44cab0u: {
        extern int game_campaign_preview_tick(GameApplication *,GameAddress);
        return game_campaign_preview_tick(app,receiver);
    }
    case 0x44d400u: {
        extern int game_campaign_caption_update(GameApplication *,GameAddress);
        return game_campaign_caption_update(app,receiver);
    }
    case 0x40a8e0u: {
        extern int game_menu_movie_tick(GameApplication *,GameAddress);
        return game_menu_movie_tick(app,receiver);
    }
    case 0x52c270u: {
        extern int game_menu_credits_tick(GameApplication *);
        return game_menu_credits_tick(app);
    }
    case 0x520960u: {
        uint32_t callback=game_memory_read_u32(&app->memory,receiver+0xa0);
        if(!callback){*result=0;return !app->memory.fault;}
        if(callback==0x54b7d0u)return game_menu_bindings_build(app,receiver);
        return game_menu_button_callback(app,callback,receiver,1001,argument);
    }
    case 0x5203b0u:return game_menu_selector_capture_tick(app,receiver);
    case 0x520eb0u:return game_menu_controller_tick(app,game_memory_read_u32(&app->memory,receiver+0xa0));
    case 0x521ef0u:return game_menu_text_entry_tick(app,receiver);
    case 0x521040u:return game_menu_combo_close(app,receiver,argument);
    case 0x54bd30u:return game_menu_binding_remap(app,receiver,argument,result);
    case 0x52c940u: {
        extern int game_menu_music_label_tick(GameApplication *);
        return game_menu_music_label_tick(app);
    }
    case 0x51ea30u:return game_menu_toggle_group(app,receiver,argument);
    case 0x522880u:return game_menu_controller_tick(app,game_memory_read_u32(&app->memory,0x627ab4));
    case 0x521320u:return game_menu_edit_scroll_request(app,receiver,argument,0,result);
    case 0x521540u:return game_menu_edit_scroll_request(app,receiver,argument,1,result);
    case 0x54e020u:return game_menu_value_change(app,receiver,argument,result);
    case 0x5232e0u:
        if(game_memory_read_u32(&app->memory,receiver)!=0x575b08u)break;
        return game_menu_edit_scroll_tick(app,receiver,0);
    case 0x523280u:
        if(game_memory_read_u32(&app->memory,receiver)!=0x575b08u)break;
        return game_menu_edit_scroll_tick(app,receiver,1);
    case 0x4901b0u: return request_state(app, receiver, argument, 0);
    case 0x490150u: return request_state(app, receiver, argument, 1);
    case 0x490220u: return advance_event(app, receiver);
    case 0x51d6d0u: {
        uint32_t callback=game_memory_read_u32(&app->memory,receiver+0x74u);
        return !app->memory.fault && (!callback || game_menu_button_callback(app,callback,receiver,0x201u,argument));
    }
    case 0x490130u: game_memory_write_u32(&app->memory, receiver + 0x24u, 2u); return !app->memory.fault;
    case 0x490140u: game_memory_write_u32(&app->memory, receiver + 0x28u, 3u); *result = 0u; return !app->memory.fault;
    default: break;
    }
    return game_menu_missing_route(app,"delivery callback",method);
}
int game_menu_deliver_message(GameApplication *app, uint32_t method,
    GameAddress receiver, uint32_t message, uint32_t argument) {
    if(method==0x5313a0u){
        extern int game_campaign_purchase_tick(GameApplication *);
        return game_campaign_purchase_tick(app);
    }
    if(method==0x52c1b0 || method==0x524730) {
        extern int game_menu_video_action(GameApplication *,uint32_t,uint32_t);
        return game_menu_video_action(app,method,argument);
    }
    if(method==0x5234c0) {
        extern int game_menu_network_game_start(GameApplication *);
        return game_menu_network_game_start(app);
    }
    if(method==0x474a60)return game_menu_network_session_tick(app);
    if(method==0x474d40 || method==0x474eb0 || method==0x475750)return game_menu_network_page_tick(app,method);
    if(method==0x512000u) {
        extern int game_menu_research_popup_tick(GameApplication *);
        return game_menu_research_popup_tick(app);
    }
    (void)receiver;(void)message;
    return game_menu_missing_route(app,"observer callback",method);
}
