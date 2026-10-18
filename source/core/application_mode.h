#ifndef UPRISING_APPLICATION_MODE_H
#define UPRISING_APPLICATION_MODE_H

#include <stdint.h>

typedef enum ApplicationModeField {
    APPLICATION_MODE_CURRENT,
    APPLICATION_MODE_ROOT_PRESENTATION,
    APPLICATION_MODE_ROOT_FLAG,
    APPLICATION_MODE_SELECTOR,
    APPLICATION_MODE_UI_ACTIVE,
    APPLICATION_MODE_RESOURCE_SET,
    APPLICATION_MODE_SOUND_ACTIVE,
    APPLICATION_MODE_INPUT_ACTIVE,
    APPLICATION_MODE_NOTIFY_OBJECT,
    APPLICATION_MODE_SCALE,
    APPLICATION_MODE_CLEAR_A,
    APPLICATION_MODE_CLEAR_B,
    APPLICATION_MODE_SAVED_A,
    APPLICATION_MODE_SAVED_B,
    APPLICATION_MODE_SAVED_SCALE
} ApplicationModeField;

typedef enum ApplicationModeAction {
    APPLICATION_MODE_LOAD_FALLBACK,
    APPLICATION_MODE_LOAD_SELECTOR,
    APPLICATION_MODE_LOAD_ROOT,
    APPLICATION_MODE_LOAD_ALTERNATE,
    APPLICATION_MODE_CLOSE_THREE,
    APPLICATION_MODE_START_SCENE,
    APPLICATION_MODE_START_DEFAULT,
    APPLICATION_MODE_RESOURCE_EMPTY,
    APPLICATION_MODE_RESOURCE_ONE,
    APPLICATION_MODE_RESOURCE_TWO,
    APPLICATION_MODE_RESOURCE_GAME,
    APPLICATION_MODE_RESOURCE_SEVEN,
    APPLICATION_MODE_STOP_SOUND,
    APPLICATION_MODE_STOP_INPUT,
    APPLICATION_MODE_NOTIFY
} ApplicationModeAction;

typedef struct ApplicationModeServices {
    void *context;
    int (*read)(void *, ApplicationModeField, uint32_t *);
    int (*write)(void *, ApplicationModeField, uint32_t);
    int (*call)(void *, ApplicationModeAction);
} ApplicationModeServices;

typedef struct ApplicationModeStats {
    uint32_t current, transitions, service_calls, entered[8];
} ApplicationModeStats;

/* Owns the mode transition order across application startup, gameplay and
 * shutdown. Services remain explicit external callbacks until migrated. */
int application_mode_change(uint32_t requested,
    const ApplicationModeServices *services);
int application_mode_capture_presentation(const ApplicationModeServices *);
int application_mode_restore_presentation(const ApplicationModeServices *);
uint32_t application_mode_current(void);
void application_mode_stats(ApplicationModeStats *);

#endif
