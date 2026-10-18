#ifndef UPRISING_PLATFORM_EVENTS_H
#define UPRISING_PLATFORM_EVENTS_H

#include <stdint.h>

/* Host message values stay native here. Guest MSG bytes are published only by
 * the temporary application callback adapter. A later backend can implement
 * this interface without changing the application event loop. */
typedef struct PlatformEventMessage {
    uintptr_t window;
    uint32_t code;
    uintptr_t wparam;
    intptr_t lparam;
    uint32_t time;
    int32_t x, y;
} PlatformEventMessage;

typedef struct PlatformEventStats {
    uint64_t waits, polls, delivered, translated, dispatched, synthetic;
} PlatformEventStats;

uint32_t platform_events_wait(const uintptr_t handles[2], uint32_t timeout_ms);
int platform_events_poll(PlatformEventMessage *message);
void platform_events_translate(const PlatformEventMessage *message);
intptr_t platform_events_dispatch(const PlatformEventMessage *message,
    void *callback_context);
void platform_events_stats(PlatformEventStats *stats);

#endif
