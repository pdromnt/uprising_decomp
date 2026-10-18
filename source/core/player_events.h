#ifndef UPRISING_PLAYER_EVENTS_H
#define UPRISING_PLAYER_EVENTS_H
#include "presentation_pose.h"
#include "effect_resource_pool.h"

typedef struct PlayerControls {
    uint32_t queue_methods, input_flags, previous_input_flags;
    uint16_t input_axes[2];
    GameAddress event_head, event_tail, weapons[15], selected[2];
    uint32_t selection[2];
    GameAddress feedback[2];
    uint16_t aim, previous_aim, heading, previous_heading;
    float vertical_scale, height_scale, remaining_scale;
    uint32_t mode_flags, last_packet_tick;
} PlayerControls;

typedef struct PlayerEventServices {
    void *context;
    int (*catalog)(void *, unsigned method, unsigned item, unsigned variant, int32_t *);
    int (*sound)(void *, unsigned enabled);
    int (*refresh)(void *, unsigned method);
    int (*spatial)(void *, unsigned operation, int *alive);
    int (*interact)(void *, unsigned operation);
} PlayerEventServices;
int player_controls_import(GameMemory *, GameAddress, PlayerControls *);
int player_controls_projection_read(PlayerControls *, uint32_t, void *, size_t);
int player_controls_projection_write(PlayerControls *, uint32_t, const void *, size_t);
int player_event_apply(GameMemory *, GameAddress, PresentationPose *, PlayerControls *,
    const EffectResource *, const PlayerEventServices *);
int player_packet_apply(GameMemory *, GameAddress, PresentationPose *, PlayerControls *,
    const EffectResource *, const PlayerEventServices *);
#endif
