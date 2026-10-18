#ifndef UPRISING_NATIVE_DISPLAY_TRANSITION_H
#define UPRISING_NATIVE_DISPLAY_TRANSITION_H

#include "game_memory.h"

typedef enum GameDisplayScalarFlags {
    GAME_DISPLAY_SCALAR_COMPARE,
    GAME_DISPLAY_SCALAR_TEST
} GameDisplayScalarFlags;

typedef struct GameDisplayScalarPlan {
    uint32_t scalar;
    uint32_t eax;
    uint32_t ecx;
    uint32_t left;
    uint32_t right;
    GameDisplayScalarFlags flags;
    int bypass;
    int has_ecx;
} GameDisplayScalarPlan;

int game_display_select_transition_scalar(GameMemory *memory,
    GameDisplayScalarPlan *plan);

/* Retail death-camera record: owned storage remains in the existing manager.
 * Arguments are values/record IDs, never register or guest-stack state. */
int game_camera_transition_begin(GameMemory *,GameAddress camera,
    const float start[3],const float target[3],uint16_t heading,float clearance);
int game_camera_transition_position(GameMemory *,GameAddress camera);
int game_camera_transition_aim(GameMemory *,GameAddress camera);
int game_camera_activate(GameMemory *,GameAddress camera,GameAddress *request);
int game_camera_restore(GameMemory *,GameAddress *request);
int game_camera_bind_scene(GameMemory *,GameAddress camera,GameAddress scene);
int game_display_layout(GameMemory *,int32_t x,int32_t y,int alternate);
int game_display_button_rectangles(GameMemory *);
int game_display_circle_template(GameMemory *,GameAddress output,uint32_t diameter);
typedef struct GameDisplayMaskServices {
    void *context;
    int (*resize)(void *,uint32_t diameter);
} GameDisplayMaskServices;
int game_display_configure_control(GameMemory *,uint32_t mode,const GameDisplayMaskServices *);
void game_display_sample_scalar(GameMemory *,uint32_t bits);
int game_display_change_runtime_mode(GameMemory *,uint32_t mode,GameAddress *external_refresh);

#endif
