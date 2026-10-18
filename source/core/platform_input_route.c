#include "platform_input_route.h"

#include <string.h>

static PlatformInputRouteState route;

void platform_input_route(const PlatformEventMessage *message,
    PlatformInputAction *action) {
    uint32_t code, packed;
    if (!action) return;
    memset(action, 0, sizeof(*action));
    if (!message) return;
    code = message->code;
    packed = (uint32_t)message->lparam;
    action->code = code;
    action->wparam = (uint32_t)message->wparam;
    action->lparam = packed;
    if (code == 3u || code == 5u || code == 0x7eu) {
        route.pointer_x = (int16_t)packed;
        route.pointer_y = (int16_t)(packed >> 16u);
        action->kind = PLATFORM_INPUT_POSITION;
    } else if (code == 0x100u) {
        if (packed & 0x01000000u) action->kind = PLATFORM_INPUT_KEY;
    } else if (code == 0x102u) {
        action->kind = PLATFORM_INPUT_KEY;
    } else if (code >= 0x200u && code <= 0x205u) {
        action->kind = PLATFORM_INPUT_POINTER;
        action->x = message->x;
        action->y = message->y;
        route.pointer_x = action->x;
        route.pointer_y = action->y;
        if (code == 0x201u) route.button_down = 1u;
        if (code == 0x202u) {
            route.button_down = 0u;
            action->clear_drag = 1u;
        }
    }
    action->x = action->kind == PLATFORM_INPUT_POSITION ?
        route.pointer_x : action->x;
    action->y = action->kind == PLATFORM_INPUT_POSITION ?
        route.pointer_y : action->y;
    action->button_down = route.button_down;
    if (action->kind == PLATFORM_INPUT_NONE) route.ignored++;
    else route.routed++;
}

void platform_input_route_state(PlatformInputRouteState *output) {
    if (output) *output = route;
}
