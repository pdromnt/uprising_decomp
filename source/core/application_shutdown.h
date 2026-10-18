#ifndef UPRISING_APPLICATION_SHUTDOWN_H
#define UPRISING_APPLICATION_SHUTDOWN_H

#include <stdint.h>

typedef enum ApplicationShutdownStep {
    APPLICATION_SHUTDOWN_WAIT_PRESENTATION,
    APPLICATION_SHUTDOWN_STOP_AUDIO_INPUT,
    APPLICATION_SHUTDOWN_DETACH_PLAYERS,
    APPLICATION_SHUTDOWN_RELEASE_LEVEL,
    APPLICATION_SHUTDOWN_FINISH_FRAME,
    APPLICATION_SHUTDOWN_RELEASE_POOL,
    APPLICATION_SHUTDOWN_FINISH_POOLS,
    APPLICATION_SHUTDOWN_UNLOAD_LEVEL,
    APPLICATION_SHUTDOWN_FLUSH_PRESENTATION,
    APPLICATION_SHUTDOWN_CLOSE_PRESENTATION,
    APPLICATION_SHUTDOWN_CLEAR_ACTIVE,
    APPLICATION_SHUTDOWN_NOTIFY_PLAYER,
    APPLICATION_SHUTDOWN_MODE_ZERO,
    APPLICATION_SHUTDOWN_RELEASE_SERVICE,
    APPLICATION_SHUTDOWN_FINISH_CONTROLLER,
    APPLICATION_SHUTDOWN_CLOSE_ROOT,
    APPLICATION_SHUTDOWN_DONE
} ApplicationShutdownStep;

typedef struct ApplicationShutdownServices {
    void *context;
    int (*perform)(void *, ApplicationShutdownStep, unsigned,
        uint32_t players[6], uint8_t *active_mask);
} ApplicationShutdownServices;

typedef struct ApplicationShutdownStats {
    uint32_t starts, completes;
    ApplicationShutdownStep step;
    uint8_t cursor, active_mask;
} ApplicationShutdownStats;

void application_shutdown_begin(void);
int application_shutdown_execute(const ApplicationShutdownServices *);
void application_shutdown_stats(ApplicationShutdownStats *);

#endif
