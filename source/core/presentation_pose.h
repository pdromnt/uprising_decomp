#ifndef UPRISING_PRESENTATION_POSE_H
#define UPRISING_PRESENTATION_POSE_H
#include "game_memory.h"

/* Shared world/controller pose and vertex-cache state. No member ordering is
 * tied to the compatibility object layout. Model/hull identities remain
 * borrowed inputs while their owning families are reconstructed. */
typedef struct PresentationPose {
    uint32_t flags, identity;
    float position[3], local_position[3];
    float dimensions[3],radius;
    uint16_t angles[3], local_angles[3];
    uint16_t angle_padding, local_angle_padding;
    uint16_t vertex_count, count_padding;
    GameAddress hull, vertices, controller, model;
    struct SceneInstance *model_instance;
    uint32_t properties[3], local_phase;
    float world_matrix[12];
} PresentationPose;

int presentation_pose_import(GameMemory *, GameAddress, PresentationPose *);
int presentation_pose_import_prefix(GameMemory *,GameAddress,PresentationPose *,unsigned);
int presentation_pose_export(GameMemory *, GameAddress, PresentationPose *);
int presentation_pose_projection_read(PresentationPose *, uint32_t, void *, size_t);
int presentation_pose_projection_write(PresentationPose *, uint32_t, const void *, size_t);
int presentation_pose_build_world(GameMemory *, PresentationPose *);
int presentation_pose_build_controller(GameMemory *, PresentationPose *);
/*535fd0: presentation catches up to the authoritative simulation pose. */
int presentation_pose_interpolate(GameMemory *,PresentationPose *);
void presentation_pose_build_billboard(PresentationPose *);
int presentation_pose_build_billboard_controller(GameMemory *,PresentationPose *);
int presentation_pose_transform_vertices(GameMemory *, PresentationPose *, int controller);
/*544260: class27 world vertices and its separate local probe. */
int presentation_pose_transform_infantry_vertices(GameMemory *,GameAddress,PresentationPose *);
int presentation_pose_build_laser_world(GameMemory *,PresentationPose *);
int presentation_pose_build_laser_controller(GameMemory *,PresentationPose *);
int presentation_pose_build_spawn_world(GameMemory *,PresentationPose *,int falling);
int presentation_pose_build_spawn_controller(GameMemory *,PresentationPose *,int falling);
int presentation_pose_transform_laser_vertices(GameMemory *,PresentationPose *);
typedef struct PresentationPlacement {
    uint32_t identity;
    float position[3];
    uint16_t angles[3];
    GameAddress descriptor;
    int controller;
} PresentationPlacement;
typedef struct PresentationPlacementServices {
    void *context;
    unsigned override_mask;
    int (*refresh_override)(void *, unsigned operation);
    int (*property)(void *, unsigned index, uint32_t *value);
    int (*register_spatial)(void *);
    unsigned projection_extent; /* 0 full pose; 80h for foreign custom matrix owners. */
} PresentationPlacementServices;
/* Placement owns world/local pose publication and both cache pipelines.
 * Overrides and catalog properties remain explicit polymorphic boundaries. */
int presentation_pose_place(GameMemory *, GameAddress, PresentationPose *,
    const PresentationPlacement *, const PresentationPlacementServices *);
/*42eb60/483290: ordinary position or closest owned scenery aim point. */
int presentation_target_position(GameMemory *,GameAddress,const float from[3],float out[3]);
/*40a000: force override for physical presentation records; velocities stay owned. */
int presentation_physical_drag(GameMemory *,GameAddress);
#endif
