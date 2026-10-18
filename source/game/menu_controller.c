#include "application_entry.h"
#include "game_intrusive_list.h"
#include "gameplay_state.h"
#include "scene_projection_storage.h"
#include <stdio.h>

/* These dispatch only native one-argument menu callbacks. The ID is a stable
 * retail identity, not executable memory or a register/stack calling convention. */
extern int game_menu_deliver_native(GameApplication *, uint32_t method,
    GameAddress receiver, uint32_t argument, uint32_t *result);
extern int game_menu_deliver_message(GameApplication *, uint32_t method,
    GameAddress receiver, uint32_t message, uint32_t argument);
extern int game_menu_keyboard_native(GameApplication *,uint32_t,GameAddress,
    uint32_t,uint32_t,uint32_t);

static GameAddress allocate_record(GameMemory *m, size_t size) {
    GameAddress record = scene_projection_storage_allocate(size, "portable menu record");
    if (record && !game_memory_fill_u8(m, record, (uint32_t)size, 0u, 0)) {
        scene_projection_storage_free(record);
        return 0;
    }
    return record;
}

GameAddress game_menu_controller_construct(GameApplication *app) {
    GameMemory *m = &app->memory;
    GameAddress controller = allocate_record(m, 0x24u);
    unsigned offset;
    if (!controller) return 0;
    for (offset = 4u; offset <= 12u; offset += 4u) {
        GameAddress list = allocate_record(m, 8u);
        if (!list) {
            unsigned previous;
            for (previous = 4u; previous < offset; previous += 4u)
                scene_projection_storage_free(game_memory_read_u32(m, controller + previous));
            scene_projection_storage_free(controller);
            return 0;
        }
        game_memory_write_u32(m, controller + offset, list);
    }
    return m->fault ? 0u : controller;
}

static GameAddress pop_payload(GameMemory *m, GameAddress list) {
    GameListResult popped = game_list_queue_pop(m, list);
    GameAddress payload;
    if (!popped.first || m->fault) return 0;
    payload = game_memory_read_u32(m, popped.first + 8u);
    scene_projection_storage_free(popped.first);
    return payload;
}

static int prepend(GameMemory *m, GameAddress list, GameAddress payload) {
    GameAddress link = allocate_record(m, 12u);
    if (!link) return 0;
    game_list_queue_prepend(m, list, link, payload);
    return !m->fault;
}

/* 004cd630: positioning changes the authoritative hit rectangle directly. */
int game_menu_register_region(GameApplication *app, GameAddress controller,
    GameAddress receiver, GameAddress rectangle, uint32_t message,
    uint32_t method, uint32_t argument) {
    GameMemory *m = &app->memory;
    GameAddress callback;
    if (!method) return 0;
    callback = allocate_record(m, 24u);
    if (!callback) return 0;
    game_memory_write_u32(m, callback, rectangle);
    game_memory_write_u32(m, callback + 4u, message);
    game_memory_write_u32(m, callback + 8u, receiver);
    game_memory_write_u32(m, callback + 12u, method);
    game_memory_write_u32(m, callback + 16u, argument);
    if (!m->fault && prepend(m, game_memory_read_u32(m, controller + 8u), callback)) return 1;
    scene_projection_storage_free(callback); return 0;
}

/* 004cd110: register a resource owner and its recipient callback, replacing
 * an existing recipient in place. Publish only fully owned records. */
int game_menu_register_delivery(GameApplication *app, GameAddress controller,
    GameAddress receiver, uint32_t recipient, uint32_t method) {
    GameMemory *m = &app->memory;
    GameAddress list = game_memory_read_u32(m, controller + 4u), node;
    GameAddress bundle = 0, nested = 0, callback = 0;
    unsigned visits = 0;
    int fresh = 0;
    if (!method) return 0;
    node = game_memory_read_u32(m, list);
    while (node && !m->fault) {
        GameplayIndexedDeliveryNode view = gameplay_state_indexed_delivery_outer(m, node);
        if (++visits > 65536u) return 0;
        if (view.nested == receiver && view.key == game_memory_read_u32(m, receiver + 4u)) {
            bundle = view.payload; break;
        }
        node = view.next;
    }
    if (!bundle) {
        bundle = allocate_record(m, 8u); nested = allocate_record(m, 8u);
        if (!bundle || !nested) goto fail;
        game_memory_write_u32(m, bundle, nested);
        game_memory_write_u32(m, bundle + 4u, receiver);
        fresh = 1;
    } else nested = game_memory_read_u32(m, bundle);
    node = game_memory_read_u32(m, nested);
    while (node && !m->fault) {
        GameplayIndexedDeliveryNode view = gameplay_state_indexed_delivery_inner(m, node);
        if (++visits > 65536u) goto fail;
        if (view.key == recipient) {
            game_memory_write_u32(m, view.payload + 4u, method);
            return !m->fault;
        }
        node = view.next;
    }
    callback = allocate_record(m, 12u);
    if (!callback) goto fail;
    game_memory_write_u32(m, callback, recipient);
    game_memory_write_u32(m, callback + 4u, method);
    if (!prepend(m, nested, callback)) goto fail;
    callback = 0;
    if (fresh && !prepend(m, list, bundle)) goto fail;
    return !m->fault;
fail:
    if (callback) scene_projection_storage_free(callback);
    if (fresh || (bundle && !nested)) {
        if (nested) {
            while (!m->fault && game_memory_read_u32(m, nested))
                scene_projection_storage_free(pop_payload(m, nested));
            scene_projection_storage_free(nested);
        }
        scene_projection_storage_free(bundle);
    } else if (!bundle && nested) scene_projection_storage_free(nested);
    return 0;
}

/* 004cd6d0: a duplicate receiver/method/argument is not enqueued twice. */
int game_menu_register_tick(GameApplication *app, GameAddress controller,
    GameAddress receiver, uint32_t method, uint32_t argument) {
    GameMemory *m = &app->memory;
    GameAddress list = game_memory_read_u32(m, controller + 12u);
    GameAddress node = game_memory_read_u32(m, list), callback;
    unsigned visits = 0;
    if (!method) return 0;
    while (node && !m->fault) {
        GameAddress payload = game_memory_read_u32(m, node + 8u);
        if (++visits > 65536u) return 0;
        if (game_memory_read_u32(m, payload) == receiver &&
            game_memory_read_u32(m, payload + 4u) == method &&
            game_memory_read_u32(m, payload + 12u) == argument) return 0;
        node = game_memory_read_u32(m, node + 4u);
    }
    callback = allocate_record(m, 16u);
    if (!callback) return 0;
    game_memory_write_u32(m, callback, receiver);
    game_memory_write_u32(m, callback + 4u, method);
    game_memory_write_u32(m, callback + 12u, argument);
    if (!prepend(m, list, callback)) {
        scene_projection_storage_free(callback); return 0;
    }
    return !m->fault;
}

/*4cd7a0: message observers share the existing tick owner but use a native
 * message callback, not the single-argument method slot. */
int game_menu_register_observer(GameApplication *app,GameAddress controller,uint32_t method,uint32_t argument) {
    GameMemory *m=&app->memory;GameAddress list=game_memory_read_u32(m,controller+12),record;
    if(!method)return 0;
    record=allocate_record(m,16);if(!record)return 0;
    game_memory_write_u32(m,record+8,method);game_memory_write_u32(m,record+12,argument);
    if(m->fault || !prepend(m,list,record)){scene_projection_storage_free(record);return 0;}
    return 1;
}
/* 004ccd00/004ce340/004ce3d0: free payloads as well as list links. */
void game_menu_controller_destroy(GameApplication *app, GameAddress controller) {
    GameMemory *m = &app->memory;
    unsigned offset, visits = 0;
    for (offset = 4u; offset <= 12u && !m->fault; offset += 4u) {
        GameAddress list = game_memory_read_u32(m, controller + offset);
        while (list && game_memory_read_u32(m, list) && !m->fault) {
            GameAddress payload;
            if (++visits > 65536u) { m->fault = 1; break; }
            payload = pop_payload(m, list);
            if (offset == 4u && payload) {
                GameAddress nested = game_memory_read_u32(m, payload);
                while (nested && game_memory_read_u32(m, nested) && !m->fault) {
                    if (++visits > 65536u) { m->fault = 1; break; }
                    scene_projection_storage_free(pop_payload(m, nested));
                }
                if (nested) scene_projection_storage_free(nested);
            }
            if (payload) scene_projection_storage_free(payload);
        }
        if (list) scene_projection_storage_free(list);
    }
    scene_projection_storage_free(controller);
}

/* 004cddc0: selection and delivery retain the original last-outer fallback. */
int game_menu_request(GameApplication *app, GameAddress controller,
    unsigned resource, unsigned recipient, unsigned argument) {
    GameMemory *m = &app->memory;
    GameAddress node = gameplay_state_indexed_delivery_head(m, controller);
    GameAddress payload = 0;
    unsigned visits = 0;
    while (node && !m->fault) {
        GameplayIndexedDeliveryNode view = gameplay_state_indexed_delivery_outer(m, node);
        if (++visits > 65536u) return 0;
        payload = view.payload;
        node = view.next;
        if (view.key == resource) break;
    }
    if (!payload) return !m->fault;
    node = game_memory_read_u32(m, gameplay_state_indexed_delivery_inner_list(m, payload));
    while (node && !m->fault) {
        GameplayIndexedDeliveryNode view = gameplay_state_indexed_delivery_inner(m, node);
        if (++visits > 65536u) return 0;
        if (view.key == recipient) {
            uint32_t result;
            GameAddress receiver = game_memory_read_u32(m, payload + 4u);
            return !m->fault && game_menu_deliver_native(app, view.callback,
                receiver, argument, &result);
        }
        node = view.next;
    }
    return !m->fault;
}

/* 004cde60: one matching delivery per receiver bundle, across every bundle.
 * A callback may mutate registration; capture the outer successor first. */
int game_menu_broadcast(GameApplication *app,GameAddress controller,uint32_t recipient,uint32_t argument) {
    GameMemory *m=&app->memory;
    GameAddress node=gameplay_state_indexed_delivery_head(m,controller);
    unsigned visits=0;
    while(node && !m->fault) {
        GameplayIndexedDeliveryNode outer=gameplay_state_indexed_delivery_outer(m,node);
        GameAddress inner=game_memory_read_u32(m,gameplay_state_indexed_delivery_inner_list(m,outer.payload));
        node=outer.next;
        if(++visits>65536u)return 0;
        while(inner && !m->fault) {
            GameplayIndexedDeliveryNode delivery=gameplay_state_indexed_delivery_inner(m,inner);
            if(++visits>65536u)return 0;
            if(delivery.key==recipient) {
                uint32_t result;
                GameAddress receiver=game_memory_read_u32(m,outer.payload+4u);
                if(!game_menu_deliver_native(app,delivery.callback,receiver,argument,&result))return 0;
                break;
            }
            inner=delivery.next;
        }
    }
    return !m->fault;
}

/* 004ce220: save next before invoking callbacks, just as the original cursor. */
int game_menu_controller_tick(GameApplication *app, GameAddress controller) {
    GameMemory *m = &app->memory;
    GameAddress list = game_memory_read_u32(m, controller + 12u);
    GameAddress node = game_memory_read_u32(m, list);
    unsigned visits = 0;
    while (node && !m->fault) {
        GameAddress next = game_memory_read_u32(m, node + 4u);
        GameAddress record = game_memory_read_u32(m, node + 8u);
        GameAddress receiver = game_memory_read_u32(m, record);
        uint32_t method = game_memory_read_u32(m, record + 4u);
        uint32_t argument = game_memory_read_u32(m, record + 12u), result;
        if (++visits > 65536u || m->fault) return 0;
        if (method) {
            if (!game_menu_deliver_native(app, method, receiver, argument, &result)) {
                fprintf(stderr,"native menu tick failed: method=%08lx receiver=%08lx argument=%08lx\n",
                    (unsigned long)method,(unsigned long)receiver,(unsigned long)argument);
                return 0;
            }
        } else {
            method = game_memory_read_u32(m, record + 8u);
            if (!game_menu_deliver_message(app, method, receiver, 0x3f2u, argument)) return 0;
        }
        node = next;
    }
    return !m->fault;
}

/* 004cdb40: unregister all three kinds of receiver links before destruction. */
/* 004cda90: remove only this native tick method, retaining the event pump. */
int game_menu_unregister_tick_method(GameApplication *app,GameAddress controller,uint32_t method) {
    GameMemory *m=&app->memory;GameAddress list=game_memory_read_u32(m,controller+12),node=game_memory_read_u32(m,list);unsigned visits=0;
    while(node && !m->fault) {
        GameAddress next=game_memory_read_u32(m,node+4),payload=game_memory_read_u32(m,node+8);
        if(++visits>65536)return 0;
        if(game_memory_read_u32(m,payload+4)==method || game_memory_read_u32(m,payload+8)==method) {
            game_list_detach_linked(m,list,node);scene_projection_storage_free(node);scene_projection_storage_free(payload);
        }
        node=next;
    }
    return !m->fault;
}
int game_menu_unregister(GameApplication *app, GameAddress controller,
    GameAddress receiver, int finish_active) {
    GameMemory *m = &app->memory;
    unsigned offset, visits = 0;
    for (offset = 4u; offset <= 12u && !m->fault; offset += 4u) {
        GameAddress list = game_memory_read_u32(m, controller + offset);
        GameAddress node = game_memory_read_u32(m, list);
        while (node && !m->fault) {
            GameAddress next = game_memory_read_u32(m, node + 4u);
            GameAddress payload = game_memory_read_u32(m, node + 8u);
            GameAddress target = game_memory_read_u32(m, payload +
                (offset == 4u ? 4u : offset == 8u ? 8u : 0u));
            if (++visits > 65536u) return 0;
            if (target == receiver) {
                game_list_detach_linked(m, list, node);
                scene_projection_storage_free(node);
                if (offset == 4u) {
                    GameAddress nested = game_memory_read_u32(m, payload);
                    while (nested && game_memory_read_u32(m, nested) && !m->fault) {
                        if (++visits > 65536u) return 0;
                        scene_projection_storage_free(pop_payload(m, nested));
                    }
                    if (nested) scene_projection_storage_free(nested);
                }
                scene_projection_storage_free(payload);
            }
            node = next;
        }
    }
    if (game_memory_read_u32(m, controller + 16u) == receiver) {
        if (finish_active) {
            uint32_t direct = game_memory_read_u32(m, controller + 20u);
            uint32_t fallback = game_memory_read_u32(m, controller + 24u);
            uint32_t argument = game_memory_read_u32(m, controller + 28u);
            game_memory_write_u32(m, controller, 0u);
            if (receiver && direct) {
                if (!game_menu_keyboard_native(app,direct,receiver,0,UINT32_MAX,argument)) return 0;
            } else {
                if (fallback) {
                    if (!game_menu_keyboard_native(app,fallback,0,0,UINT32_MAX,argument)) return 0;
                } else game_memory_write_u32(m, controller, 1u);
                if (game_memory_read_u32(m, controller) &&
                    !game_menu_keyboard_native(app,0x52c2e0u,0,0,UINT32_MAX,argument)) return 0;
            }
        }
        game_memory_write_u32(m, controller + 16u, 0u);
        game_memory_write_u32(m, controller + 20u, 0u);
        game_memory_write_u32(m, controller + 28u, 0u);
    }
    return !m->fault;
}
