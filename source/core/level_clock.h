#ifndef UPRISING_LEVEL_CLOCK_H
#define UPRISING_LEVEL_CLOCK_H

#include <stdint.h>

/* Level lifetime owns the network clock handle and its two frame filters.
 * The x86 network service remains an external caller until that service is
 * reconstructed; its handle is projected into guest memory by the bridge. */
typedef struct LevelClock {
    uint32_t object;
    uint32_t current_tick;
    uint32_t last_sample;
    float smooth_a;
    float smooth_b;
    int active;
    uint8_t stop_phase;
} LevelClock;

typedef struct LevelClockStopServices {
    void *context;
    int (*sample_time)(void *, uint32_t *);
    int (*pending)(void *, uint32_t object, uint32_t *);
    int (*refresh)(void *, uint32_t object);
    int (*release)(void *, uint32_t object);
    int (*unpublish)(void *);
} LevelClockStopServices;

typedef struct LevelClockStats {
    uint32_t starts, stops, wait_samples, refreshes;
} LevelClockStats;

/* The clock's constructor owns the initial pool, channel and four timing
 * lanes. The x86 projection remains while the external clock methods still
 * consume the retail object layout. */
typedef struct LevelClockCreateServices {
    void *context;
    int (*allocate)(void *, uint32_t bytes, uint32_t *object);
    int (*initialize_pool)(void *, uint32_t pool);
    int (*configure_pool)(void *, uint32_t pool, uint32_t count,
        uint32_t stride, uint32_t lanes, uint32_t capacity);
    int (*project)(void *, uint32_t object, uint32_t pool,
        uint32_t channel);
} LevelClockCreateServices;

LevelClock *level_clock_current(void);
int level_clock_create(uint32_t channel,
    const LevelClockCreateServices *, uint32_t *object);
void level_clock_begin(uint32_t object, int32_t interval);
void level_clock_set_current(uint32_t tick);
void level_clock_sample(uint32_t requested_tick, uint32_t current_tick,
    float sample_weight, float delta_weight, float blend);
uint32_t level_clock_end(void);
/* Drain pending presentation, then release and unpublish the clock. A failed
 * external callback leaves the native owner available for shutdown retry. */
int level_clock_stop_pending(uint32_t time_base,
    const LevelClockStopServices *);
void level_clock_stats(LevelClockStats *);

#endif
