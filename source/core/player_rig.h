#ifndef UPRISING_PLAYER_RIG_H
#define UPRISING_PLAYER_RIG_H
#include "player_events.h"

/* One player owns the inline simulation matrices and the mount descriptions.
 * Rendered matrices live in its owned model graph; identities expose old views. */
typedef struct PlayerPoseChannel {
    uint32_t ticks[2];
    int32_t active;
    float samples[2],coefficients[2];
} PlayerPoseChannel;
/* Suspension and control integration share this state for the player's lifetime. */
typedef struct PlayerDynamics {
    float forward_scale,strafe_scale,turn_scale,turret_scale;
    float surface_normal[3],engine_scalar,slope_drag,speed_drag;
    int16_t steering,yaw_step,lean,filtered_roll,idle_ticks,steer_ticks;
    uint16_t steering_padding,yaw_padding;
} PlayerDynamics;
typedef struct PlayerRig {
    GameAddress turret_model, mount_model;
    struct SceneInstance *turret_instance,*mount_instance;
    uint16_t turret_part, turret_padding, mount_part, mount_padding;
    float inline_turret[12], inline_mount[12];
    float turret_origin[3], mount_origin[3], pivot[3];
    float controller_velocity[3], aim_offset;
    float ground_corners[4][3], terrain_ring[8][3];
    PlayerPoseChannel channels[8];
    PlayerDynamics motion;
    uint16_t smoothed_heading, smoothing_padding;
} PlayerRig;
int player_rig_import(GameMemory *,GameAddress,PlayerRig *);
int player_rig_projection_read(PlayerRig *,uint32_t,void *,size_t);
int player_rig_projection_write(PlayerRig *,uint32_t,const void *,size_t);
int player_rig_build(GameMemory *,PresentationPose *,PlayerControls *,PlayerRig *,int rendered);
typedef struct PlayerRigDefinition {
    uint16_t mount_part,turret_part;
    float mount_origin[3],turret_origin[3],pivot[3];
} PlayerRigDefinition;
int player_rig_construct(GameMemory *,GameAddress object,GameAddress source,
    const PlayerRigDefinition *);
int player_rig_record_pose(GameMemory *,PresentationPose *,PlayerControls *,PlayerRig *,int reset);
int player_rig_sample_pose(GameMemory *,PresentationPose *,PlayerControls *,const PlayerRig *);
#endif
