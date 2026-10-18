#include "application_mode.h"

typedef struct ApplicationModeState {
    uint32_t current;
    uint32_t saved_a, saved_b, saved_scale;
    int initialized;
    int presentation_saved;
} ApplicationModeState;

static ApplicationModeState application;
static ApplicationModeStats statistics;

uint32_t application_mode_current(void) {
    return application.current;
}

void application_mode_stats(ApplicationModeStats *output) {
    if (output) *output = statistics;
}

static int read_field(const ApplicationModeServices *services,
    ApplicationModeField field, uint32_t *value) {
    return services->read(services->context, field, value);
}

static int write_field(const ApplicationModeServices *services,
    ApplicationModeField field, uint32_t value) {
    return services->write(services->context, field, value);
}

static int call_action(const ApplicationModeServices *services,
    ApplicationModeAction action) {
    if (!services->call(services->context, action)) return 0;
    ++statistics.service_calls;
    return 1;
}

int application_mode_capture_presentation(
        const ApplicationModeServices *services) {
    uint32_t first, second, scale;
    if (!services || !services->read || !services->write ||
        !read_field(services, APPLICATION_MODE_CLEAR_B, &first) ||
        !read_field(services, APPLICATION_MODE_CLEAR_A, &second) ||
        !read_field(services, APPLICATION_MODE_SCALE, &scale) ||
        !write_field(services, APPLICATION_MODE_SAVED_A, first) ||
        !write_field(services, APPLICATION_MODE_SAVED_B, second) ||
        !write_field(services, APPLICATION_MODE_SAVED_SCALE, scale)) return 0;
    application.saved_a = first;
    application.saved_b = second;
    application.saved_scale = scale;
    application.presentation_saved = 1;
    return 1;
}

int application_mode_restore_presentation(
        const ApplicationModeServices *services) {
    uint32_t first, second, scale;
    if (!services || !services->read || !services->write) return 0;
    if (application.presentation_saved) {
        first = application.saved_a;
        second = application.saved_b;
        scale = application.saved_scale;
    } else if (!read_field(services, APPLICATION_MODE_SAVED_A, &first) ||
        !read_field(services, APPLICATION_MODE_SAVED_B, &second) ||
        !read_field(services, APPLICATION_MODE_SAVED_SCALE, &scale)) return 0;
    if (!write_field(services, APPLICATION_MODE_CLEAR_B, first) ||
        !write_field(services, APPLICATION_MODE_CLEAR_A, second) ||
        !write_field(services, APPLICATION_MODE_SCALE, scale)) return 0;
    application.saved_a = first;
    application.saved_b = second;
    application.saved_scale = scale;
    application.presentation_saved = 1;
    return 1;
}

int application_mode_change(uint32_t requested,
    const ApplicationModeServices *services) {
    uint32_t value, selector, current;
    if (!services || !services->read || !services->write || !services->call)
        return 0;
    if (!application.initialized) {
        if (!read_field(services, APPLICATION_MODE_CURRENT,
                &application.current)) return 0;
        application.initialized = 1;
    }
    current = application.current;

    if (requested == 3u) {
        if (!read_field(services, APPLICATION_MODE_ROOT_PRESENTATION, &value))
            return 0;
        if (!value) {
            if (!call_action(services, APPLICATION_MODE_LOAD_FALLBACK))
                return 0;
        } else {
            if (!read_field(services, APPLICATION_MODE_SELECTOR, &selector))
                return 0;
            if (selector != 0u && selector != 1u) {
                if (!call_action(services, APPLICATION_MODE_LOAD_SELECTOR))
                    return 0;
            } else {
                if (!read_field(services, APPLICATION_MODE_ROOT_FLAG, &value))
                    return 0;
                if (!call_action(services, value ?
                        APPLICATION_MODE_LOAD_ALTERNATE :
                        APPLICATION_MODE_LOAD_ROOT)) return 0;
            }
        }
    } else if (current == 3u &&
            !call_action(services, APPLICATION_MODE_CLOSE_THREE)) return 0;

    if (current == 1u && requested != 1u && requested != 6u) {
        if (!application_mode_capture_presentation(services) ||
            !write_field(services, APPLICATION_MODE_SCALE, 0x3f800000u) ||
            !write_field(services, APPLICATION_MODE_CLEAR_A, 0u) ||
            !write_field(services, APPLICATION_MODE_CLEAR_B, 0u)) return 0;
    } else if (current != 1u && current != 5u &&
            !application_mode_restore_presentation(services)) return 0;

    if (requested >= 1u && requested <= 6u) {
        if (!call_action(services, requested == 3u || requested == 4u ?
                APPLICATION_MODE_START_SCENE :
                APPLICATION_MODE_START_DEFAULT)) return 0;
    }

    if (!read_field(services, APPLICATION_MODE_UI_ACTIVE, &value)) return 0;
    if (value && requested >= 1u && requested <= 7u) {
        if (requested == 1u || requested == 2u) {
            if (!read_field(services, APPLICATION_MODE_RESOURCE_SET, &value))
                return 0;
            if (!call_action(services, !value ?
                    APPLICATION_MODE_RESOURCE_EMPTY :
                    requested == 1u ? APPLICATION_MODE_RESOURCE_ONE :
                        APPLICATION_MODE_RESOURCE_TWO)) return 0;
        } else if (requested == 3u || requested == 4u) {
            if (!call_action(services, APPLICATION_MODE_RESOURCE_GAME))
                return 0;
        } else if (requested == 7u &&
                !call_action(services, APPLICATION_MODE_RESOURCE_SEVEN))
            return 0;
    }

    if (requested != 1u) {
        if (!read_field(services, APPLICATION_MODE_SOUND_ACTIVE, &value))
            return 0;
        if (value && !call_action(services, APPLICATION_MODE_STOP_SOUND))
            return 0;
        if (!read_field(services, APPLICATION_MODE_INPUT_ACTIVE, &value))
            return 0;
        if (value && !call_action(services, APPLICATION_MODE_STOP_INPUT))
            return 0;
    }

    if (!write_field(services, APPLICATION_MODE_CURRENT, requested)) return 0;
    application.current = requested;
    statistics.current = requested;
    ++statistics.transitions;
    if (requested < 8u) ++statistics.entered[requested];
    if (requested && !call_action(services, APPLICATION_MODE_NOTIFY)) return 0;
    return 1;
}
