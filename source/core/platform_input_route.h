#ifndef UPRISING_PLATFORM_INPUT_ROUTE_H
#define UPRISING_PLATFORM_INPUT_ROUTE_H

#include "platform_events.h"

typedef enum PlatformInputActionKind {
    PLATFORM_INPUT_NONE,
    PLATFORM_INPUT_POSITION,
    PLATFORM_INPUT_KEY,
    PLATFORM_INPUT_POINTER
} PlatformInputActionKind;

typedef struct PlatformInputAction {
    PlatformInputActionKind kind;
    uint32_t code, wparam, lparam;
    int32_t x, y;
    uint32_t button_down, clear_drag;
} PlatformInputAction;

typedef struct PlatformInputRouteState {
    int32_t pointer_x, pointer_y;
    uint32_t button_down;
    uint64_t routed, ignored;
} PlatformInputRouteState;

/* Application-owned input state. Host messages enter here before the
 * remaining controller callback bridge. */
void platform_input_route(const PlatformEventMessage *message,
    PlatformInputAction *action);
void platform_input_route_state(PlatformInputRouteState *output);

#endif
