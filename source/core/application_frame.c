#include "application_frame.h"
#include <string.h>

typedef struct ApplicationFrameState {
    uint32_t last_tick;
    int active;
} ApplicationFrameState;

static ApplicationFrameState frame;
static ApplicationFrameStats statistics;

void application_frame_begin(uint32_t previous_tick) {
    frame.last_tick = previous_tick;
    frame.active = 1;
}

void application_frame_end(void) {
    memset(&frame, 0, sizeof(frame));
}

void application_frame_stats(ApplicationFrameStats *output) {
    if (output) *output = statistics;
}

static int perform(const ApplicationFrameServices *services,
        ApplicationFrameAction action, uint32_t value, uint32_t *result) {
    return services->perform(services->context, action, value, result);
}

static int render(const ApplicationFrameInput *input,
        const ApplicationFrameServices *services) {
    uint32_t mode;
    (void)input;
    if (!services->read_mode(services->context, &mode)) return 0;
    return perform(services, mode == 7u ?
        APPLICATION_FRAME_RENDER_MODE_SEVEN :
        APPLICATION_FRAME_RENDER_GAME, 0u, NULL);
}

int application_frame_run(const ApplicationFrameInput *input,
        const ApplicationFrameServices *services) {
    uint32_t now, tick, step, result = 0u;
    if (!input || !services || !services->clock_now || !services->read_mode ||
        !services->perform ||
        !services->publish_tick) return 0;
    if (!frame.active) application_frame_begin(input->previous_tick);
    if (input->suspended) {
        if (!perform(services, APPLICATION_FRAME_SUSPEND, 0u, NULL)) return 0;
        if (input->networked &&
            (!perform(services, APPLICATION_FRAME_NETWORK_POLL, 0u, NULL) ||
             !perform(services, APPLICATION_FRAME_NETWORK_ADVANCE, 0u, NULL) ||
             !perform(services, APPLICATION_FRAME_NETWORK_FLUSH, 0u, NULL)))
            return 0;
        if (!render(input, services) ||
            !perform(services, APPLICATION_FRAME_FINISH, 0u, NULL)) return 0;
        ++statistics.frames;
        ++statistics.suspended_frames;
        statistics.last_tick = frame.last_tick;
        return 1;
    }
    if (!services->clock_now(services->context, &now) ||
        !perform(services, APPLICATION_FRAME_PREPARE_TICKS,
            (now - input->time_base) >> 5u, NULL) ||
        !services->clock_now(services->context, &now)) return 0;
    tick = (now - input->time_base) >> 5u;
    if ((int32_t)(tick - frame.last_tick) > 0) {
        if (!perform(services, APPLICATION_FRAME_WORLD_BEGIN,
                tick - frame.last_tick, NULL) ||
            !perform(services, APPLICATION_FRAME_SCENE_BEGIN,
                tick - frame.last_tick, NULL)) return 0;
        for (step = frame.last_tick; step < tick; ++step)
            if (!perform(services, APPLICATION_FRAME_WORLD_STEP, step, NULL) ||
                !perform(services, APPLICATION_FRAME_SCENE_STEP, step, NULL))
                return 0;
        if (input->networked &&
            !perform(services, APPLICATION_FRAME_NETWORK_SYNC, 0u,
                &result)) return 0;
        if (!input->networked || result) {
            if (!perform(services, APPLICATION_FRAME_GAMEPLAY_STEP,
                    tick, NULL)) return 0;
            ++statistics.gameplay_ticks;
            if (input->networked &&
                !perform(services, APPLICATION_FRAME_NETWORK_FLUSH,
                    0u, NULL)) return 0;
        }
        if (!perform(services, APPLICATION_FRAME_WORLD_END, 0u, NULL) ||
            !perform(services, APPLICATION_FRAME_SCENE_END, 0u, NULL)) return 0;
        statistics.ticks += tick - frame.last_tick;
    }
    frame.last_tick = tick;
    if (!services->publish_tick(services->context, tick) ||
        !render(input, services) ||
        !perform(services, APPLICATION_FRAME_FINISH, 0u, NULL)) return 0;
    ++statistics.frames;
    statistics.last_tick = tick;
    return 1;
}
