#include "level_clock.h"
#include <string.h>

static LevelClock current_level_clock;
static LevelClockStats statistics;

void level_clock_stats(LevelClockStats *output) {
    if (output) *output = statistics;
}

LevelClock *level_clock_current(void) {
    return &current_level_clock;
}

int level_clock_create(uint32_t channel,
        const LevelClockCreateServices *services, uint32_t *object) {
    uint32_t clock, pool;
    if (!services || !services->allocate || !services->initialize_pool ||
        !services->configure_pool ||
        !services->project || !object) return 0;
    if (!services->allocate(services->context, 0xa8u, &clock) || !clock)
        return 0;
    if (!services->allocate(services->context, 0xa4u, &pool) || !pool)
        return 0;
    /* Retail's 0048f320 pool base initializes these two live fields. */
    if (!services->initialize_pool(services->context, pool)) return 0;
    if (!services->configure_pool(services->context, pool,
            20u, 4u, 10u, 64u)) return 0;
    if (!services->project(services->context, clock, pool, channel))
        return 0;
    *object = clock;
    return 1;
}

void level_clock_begin(uint32_t object, int32_t interval) {
    memset(&current_level_clock, 0, sizeof(current_level_clock));
    current_level_clock.object = object;
    current_level_clock.smooth_a = (float)interval;
    current_level_clock.active = object != 0u;
    if (object) ++statistics.starts;
}

void level_clock_set_current(uint32_t tick) {
    current_level_clock.current_tick = tick;
}

void level_clock_sample(uint32_t requested_tick, uint32_t current_tick,
    float sample_weight, float delta_weight, float blend) {
    LevelClock *clock = &current_level_clock;
    long double a = (long double)clock->smooth_a * sample_weight;
    long double b = (long double)clock->smooth_b * sample_weight;
    a += (long double)(int32_t)(requested_tick - clock->last_sample) *
        delta_weight;
    b += (long double)(int32_t)(requested_tick - current_tick);
    clock->smooth_a = (float)(a * blend);
    clock->smooth_b = (float)(b * blend);
    clock->last_sample = requested_tick;
}

uint32_t level_clock_end(void) {
    uint32_t object = current_level_clock.object;
    memset(&current_level_clock, 0, sizeof(current_level_clock));
    return object;
}

int level_clock_stop_pending(uint32_t time_base,
        const LevelClockStopServices *services) {
    LevelClock *clock = &current_level_clock;
    uint32_t start, deadline, now, pending;
    if (!services || !services->sample_time || !services->pending ||
        !services->refresh || !services->release || !services->unpublish ||
        !clock->active || !clock->object) return 0;
    if (clock->stop_phase == 0u) {
        if (!services->sample_time(services->context, &start)) return 0;
        ++statistics.wait_samples;
        start -= time_base;
        deadline = start + 2000u;
        if ((int32_t)deadline > (int32_t)start) for (;;) {
            if (!services->sample_time(services->context, &now)) return 0;
            ++statistics.wait_samples;
            now -= time_base;
            if (!services->pending(services->context, clock->object,
                    &pending)) return 0;
            if (pending) break;
            if ((int32_t)now > (int32_t)(start + 30u)) {
                start = now;
                if (!services->refresh(services->context,
                        clock->object)) return 0;
                ++statistics.refreshes;
            }
            if ((int32_t)now >= (int32_t)deadline) break;
        }
        clock->stop_phase = 1u;
    }
    if (clock->stop_phase == 1u) {
        if (!services->release(services->context, clock->object)) return 0;
        clock->stop_phase = 2u;
    }
    if (!services->unpublish(services->context)) return 0;
    ++statistics.stops;
    level_clock_end();
    return 1;
}
