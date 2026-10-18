#ifndef UPRISING_NATIVE_PLAYER_FEEDBACK_H
#define UPRISING_NATIVE_PLAYER_FEEDBACK_H

#include "game_memory.h"

typedef struct GamePlayerHitTimerOps {
    void *context;
    int (*read_tick)(void *context, GameAddress target, uint32_t *tick);
    uint32_t (*scale_elapsed)(void *context, uint32_t tick,
        uint32_t start_tick);
} GamePlayerHitTimerOps;

int game_player_update_hit_timer(GameMemory *memory,
    const GamePlayerHitTimerOps *ops, uint32_t *value);
int game_player_hit_marker(GameMemory *,const float position[3],uint8_t,int32_t);
int game_camera_hit_begin(GameMemory *,GameAddress,const float impulse[3]);
int game_camera_hit_sample(GameMemory *,GameAddress,float output[3],int *active);

typedef struct GamePresentationFeedbackServices {
    void *context;
    int (*clear_queue)(void *,GameAddress bank);
    int (*submit_queue)(void *,GameAddress bank,const int16_t sounds[8],int *accepted);
} GamePresentationFeedbackServices;
uint32_t game_presentation_location_token(GameMemory *,float coordinate,int horizontal);
int game_presentation_deactivation_feedback(GameMemory *,GameAddress object,uint32_t reason,int16_t announcement,
    const GamePresentationFeedbackServices *);

#endif
