#include "application_shutdown.h"
#include <string.h>

typedef struct ApplicationShutdownState {
    uint32_t players[6];
    ApplicationShutdownStep step;
    uint8_t cursor, active_mask;
    uint32_t starts, completes;
} ApplicationShutdownState;

static ApplicationShutdownState shutdown_state;

void application_shutdown_begin(void) {
    uint32_t starts = shutdown_state.starts;
    uint32_t completes = shutdown_state.completes;
    if (shutdown_state.step != APPLICATION_SHUTDOWN_DONE &&
        shutdown_state.starts != shutdown_state.completes) return;
    memset(&shutdown_state, 0, sizeof(shutdown_state));
    shutdown_state.starts = starts + 1u;
    shutdown_state.completes = completes;
}

void application_shutdown_stats(ApplicationShutdownStats *output) {
    if (!output) return;
    output->starts = shutdown_state.starts;
    output->completes = shutdown_state.completes;
    output->step = shutdown_state.step;
    output->cursor = shutdown_state.cursor;
    output->active_mask = shutdown_state.active_mask;
}

int application_shutdown_execute(const ApplicationShutdownServices *services) {
    ApplicationShutdownState *state = &shutdown_state;
    unsigned limit;
    if (!services || !services->perform ||
        state->starts == state->completes) return 0;
    while (state->step < APPLICATION_SHUTDOWN_DONE) {
        limit = state->step == APPLICATION_SHUTDOWN_RELEASE_POOL ? 5u :
            state->step == APPLICATION_SHUTDOWN_NOTIFY_PLAYER ? 6u :
            state->step == APPLICATION_SHUTDOWN_RELEASE_SERVICE ? 12u : 1u;
        while (state->cursor < limit) {
            if (!services->perform(services->context, state->step,
                    state->cursor, state->players,
                    &state->active_mask)) return 0;
            ++state->cursor;
        }
        state->cursor = 0u;
        state->step = (ApplicationShutdownStep)(state->step + 1);
    }
    ++state->completes;
    return 1;
}
