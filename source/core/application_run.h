#ifndef UPRISING_APPLICATION_RUN_H
#define UPRISING_APPLICATION_RUN_H

#include <stdint.h>

typedef enum ApplicationRunPhase {
    APPLICATION_RUN_IDLE,
    APPLICATION_RUN_STARTUP,
    APPLICATION_RUN_ROOT,
    APPLICATION_RUN_MENU,
    APPLICATION_RUN_SESSION,
    APPLICATION_RUN_GAME,
    APPLICATION_RUN_SHUTDOWN,
    APPLICATION_RUN_DONE
} ApplicationRunPhase;

typedef struct ApplicationRunState {
    uint32_t replay_stream;
    uint32_t player_count;
    uint32_t clock;
    uint32_t requested_delay;
    uint32_t initial_tick;
    uint32_t settings_a, settings_b, settings_c;
    ApplicationRunPhase phase;
    uint8_t networked;
} ApplicationRunState;

typedef struct ApplicationStartupServices {
    void *context;
    int (*replay_flags)(void *, uint32_t *network, uint32_t *requested);
    int (*open_replay)(void *, uint32_t *stream);
    int (*clear_replay_request)(void *);
    int (*publish_replay)(void *, uint32_t stream);
    int (*reset_player_count)(void *);
    int (*player_controller)(void *, unsigned index, uint32_t *active);
    int (*publish_player_count)(void *, uint32_t count);
    int (*finalize_player_count)(void *, uint32_t count);
    int (*close_single_player_network)(void *);
    int (*prepare_players)(void *);
    int (*read_settings)(void *, uint32_t *a, uint32_t *b, uint32_t *c);
    int (*publish_settings)(void *, uint32_t a, uint32_t b, uint32_t c);
    int (*load_source)(void *, uint32_t replay_stream);
    int (*player_active)(void *, unsigned index, uint32_t *active);
    int (*activate_player)(void *, unsigned index);
    int (*bootstrap_services)(void *);
    int (*load_level)(void *);
    int (*current_replay)(void *, uint32_t *stream);
    int (*finish_replay)(void *, uint32_t stream);
    int (*replay_slot)(void *, unsigned index, uint32_t *present);
    int (*apply_replay_slot)(void *, unsigned index);
} ApplicationStartupServices;

/* These are boundaries to the still-external presentation, replay, network,
 * audio and platform services. Decisions and run lifetime stay native. */
typedef struct ApplicationRunServices {
    void *context;
    int (*prepare_root)(void *, uint32_t *proceed);
    int (*open_root)(void *, uint32_t *proceed);
    int (*open_menu)(void *, uint32_t *proceed);
    int (*resource_present)(void *, uint32_t *present);
    int (*load_resource)(void *, uint32_t present);
    int (*enable_ui)(void *);
    int (*close_replay)(void *, uint32_t stream);
    int (*replay_tick)(void *, uint32_t *tick);
    int (*network_enabled)(void *, uint32_t *enabled);
    int (*network_begin)(void *, uint32_t *proceed);
    int (*network_prime)(void *, uint32_t *clock, uint32_t *delay);
    int (*network_poll)(void *, uint32_t clock, uint32_t *now,
        uint32_t *base, uint32_t *delay);
    int (*network_publish)(void *, uint32_t base);
    int (*local_prime)(void *, uint32_t *clock, uint32_t *tick);
    int (*start_audio)(void *);
    int (*enter_game)(void *, uint32_t tick);
    int (*shutdown)(void *);
} ApplicationRunServices;

void application_run_begin(void);
ApplicationRunState *application_run_current(void);
int application_run_prepare(const ApplicationStartupServices *);
int application_run_execute(const ApplicationRunServices *);

#endif
