#include "application_run.h"
#include <string.h>

static ApplicationRunState run;

void application_run_begin(void) {
    memset(&run, 0, sizeof(run));
    run.phase = APPLICATION_RUN_STARTUP;
}

ApplicationRunState *application_run_current(void) {
    return &run;
}

int application_run_prepare(const ApplicationStartupServices *services) {
    uint32_t network, requested, active, count = 0u;
    unsigned index;
    if (!services || !services->replay_flags || !services->open_replay ||
        !services->clear_replay_request || !services->publish_replay ||
        !services->reset_player_count || !services->player_controller ||
        !services->publish_player_count || !services->finalize_player_count ||
        !services->close_single_player_network || !services->prepare_players ||
        !services->read_settings || !services->publish_settings ||
        !services->load_source || !services->player_active ||
        !services->activate_player || !services->bootstrap_services ||
        !services->load_level || !services->current_replay ||
        !services->finish_replay || !services->replay_slot ||
        !services->apply_replay_slot || run.phase != APPLICATION_RUN_STARTUP)
        return 0;
    if (!services->replay_flags(services->context, &network,
            &requested)) return 0;
    if (!network && requested) {
        if (!services->open_replay(services->context,
                &run.replay_stream) ||
            !services->clear_replay_request(services->context)) return 0;
    } else run.replay_stream = 0u;
    if (!services->publish_replay(services->context,
            run.replay_stream)) return 0;
    if (!services->reset_player_count(services->context)) return 0;
    for (index = 0u; index < 4u; ++index) {
        if (!services->player_controller(services->context,
                index, &active)) return 0;
        if (active) {
            ++count;
            if (!services->publish_player_count(services->context,
                    count)) return 0;
        }
    }
    run.player_count = count;
    if (!services->finalize_player_count(services->context, count)) return 0;
    if (count == 1u) {
        if (!services->replay_flags(services->context,
                &network, &requested)) return 0;
        if (network &&
            !services->close_single_player_network(services->context))
            return 0;
    }
    if (!services->prepare_players(services->context) ||
        !services->read_settings(services->context, &run.settings_a,
            &run.settings_b, &run.settings_c) ||
        !services->publish_settings(services->context,
            run.settings_a, run.settings_b, run.settings_c) ||
        !services->load_source(services->context,
            run.replay_stream)) return 0;
    for (index = 0u; index < 6u; ++index) {
        if (!services->player_active(services->context,
                index, &active)) return 0;
        if (active && !services->activate_player(services->context,
                index)) return 0;
    }
    if (!services->bootstrap_services(services->context) ||
        !services->load_level(services->context) ||
        !services->current_replay(services->context,
            &run.replay_stream)) return 0;
    if (run.replay_stream) {
        if (!services->finish_replay(services->context,
                run.replay_stream)) return 0;
        for (index = 0u; index < 9u; ++index) {
            if (!services->replay_slot(services->context,
                    index, &active)) return 0;
            if (active && !services->apply_replay_slot(services->context,
                    index)) return 0;
        }
    }
    return 1;
}

int application_run_execute(const ApplicationRunServices *services) {
    uint32_t proceed, present, enabled, now, base, delay;
    if (!services || !services->prepare_root || !services->open_root ||
        !services->open_menu || !services->resource_present ||
        !services->load_resource || !services->enable_ui ||
        !services->close_replay || !services->replay_tick ||
        !services->network_enabled ||
        !services->network_begin || !services->network_prime ||
        !services->network_poll || !services->network_publish ||
        !services->local_prime || !services->start_audio ||
        !services->enter_game || !services->shutdown ||
        run.phase != APPLICATION_RUN_STARTUP) return 0;

    run.phase = APPLICATION_RUN_ROOT;
    if (!services->prepare_root(services->context, &proceed)) return 0;
    if (!proceed) goto done;
    if (!services->open_root(services->context, &proceed)) return 0;
    if (!proceed) goto done;
    run.phase = APPLICATION_RUN_MENU;
    if (!services->open_menu(services->context, &proceed)) return 0;
    if (!proceed) goto done;
    if (!services->resource_present(services->context, &present) ||
        !services->load_resource(services->context, present) ||
        !services->enable_ui(services->context)) return 0;

    run.phase = APPLICATION_RUN_SESSION;
    if (run.replay_stream) {
        if (!services->close_replay(services->context,
                run.replay_stream) ||
            !services->replay_tick(services->context,
                &run.initial_tick)) return 0;
        run.replay_stream = 0u;
    } else {
        if (!services->network_enabled(services->context,
                &enabled)) return 0;
        run.networked = enabled != 0u;
        if (run.networked) {
            if (!services->network_begin(services->context,
                    &proceed)) return 0;
            if (!proceed) goto shutdown;
            if (!services->network_prime(services->context,
                    &run.clock, &run.requested_delay)) return 0;
            for (;;) {
                if (!services->network_poll(services->context,
                        run.clock, &now, &base, &delay)) return 0;
                if (now - base >= delay) {
                    run.initial_tick = 1u;
                    if (!services->network_publish(services->context,
                            delay + base - 64u)) return 0;
                    break;
                }
            }
        } else if (!services->local_prime(services->context,
                &run.clock, &run.initial_tick)) return 0;
        if (!services->start_audio(services->context)) return 0;
    }
    run.phase = APPLICATION_RUN_GAME;
    if (!services->enter_game(services->context,
            run.initial_tick)) return 0;
shutdown:
    run.phase = APPLICATION_RUN_SHUTDOWN;
    if (!services->shutdown(services->context)) return 0;
done:
    run.phase = APPLICATION_RUN_DONE;
    return 1;
}
