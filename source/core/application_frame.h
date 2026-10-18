#ifndef UPRISING_APPLICATION_FRAME_H
#define UPRISING_APPLICATION_FRAME_H

#include <stdint.h>

typedef enum ApplicationFrameAction {
    APPLICATION_FRAME_SUSPEND,
    APPLICATION_FRAME_NETWORK_POLL,
    APPLICATION_FRAME_NETWORK_ADVANCE,
    APPLICATION_FRAME_NETWORK_FLUSH,
    APPLICATION_FRAME_PREPARE_TICKS,
    APPLICATION_FRAME_WORLD_BEGIN,
    APPLICATION_FRAME_SCENE_BEGIN,
    APPLICATION_FRAME_WORLD_STEP,
    APPLICATION_FRAME_SCENE_STEP,
    APPLICATION_FRAME_NETWORK_SYNC,
    APPLICATION_FRAME_GAMEPLAY_STEP,
    APPLICATION_FRAME_WORLD_END,
    APPLICATION_FRAME_SCENE_END,
    APPLICATION_FRAME_RENDER_GAME,
    APPLICATION_FRAME_RENDER_MODE_SEVEN,
    APPLICATION_FRAME_FINISH
} ApplicationFrameAction;

typedef struct ApplicationFrameServices {
    void *context;
    int (*clock_now)(void *, uint32_t *);
    int (*read_mode)(void *, uint32_t *);
    /* result is used by NETWORK_SYNC; other actions may ignore it. */
    int (*perform)(void *, ApplicationFrameAction, uint32_t, uint32_t *);
    int (*publish_tick)(void *, uint32_t);
} ApplicationFrameServices;

typedef struct ApplicationFrameInput {
    uint32_t time_base;
    uint32_t previous_tick;
    int suspended;
    int networked;
} ApplicationFrameInput;

typedef struct ApplicationFrameStats {
    uint32_t frames, suspended_frames, ticks, gameplay_ticks;
    uint32_t last_tick;
} ApplicationFrameStats;

/* Level startup and unload delimit the authoritative frame tick. Guest tick
 * storage remains a projection while external world/scene readers use it. */
void application_frame_begin(uint32_t previous_tick);
void application_frame_end(void);
int application_frame_run(const ApplicationFrameInput *,
    const ApplicationFrameServices *);
void application_frame_stats(ApplicationFrameStats *);

#endif
