#ifndef UPRISING_SCENE_SUBMISSION_H
#define UPRISING_SCENE_SUBMISSION_H
#include "gameplay_geometry.h"

/* Per-submission values; geometry and packet storage keep their existing owners. */
typedef struct SceneSubmissionState {
    GameplayRenderFrame frame;
    GameplayClipCursors *shadow_sources; /* Borrowed emission buffers; clipping has separate cursors. */
    GameMemory *palette_memory; /* Borrowed external palette view, if separate. */
    GameAddress *visible_queue; /* Ordinary caller-owned queue head, when enabled. */
    float visible_numerator,visible_offset;
    float camera[12],far_depth;
    uint32_t presentation_policy,depth;
    float matrices[10][12];
    uint8_t dirty[10];
    uint32_t submitted,nodes,render_nodes,uv_updates,reject;
    GameAddress *shadow_queue; /*5037b0's projected plain queue, frame-owned. */
} SceneSubmissionState;
/* 1 submitted, 0 unsupported before execution, -1 allocation/storage failure. */
int scene_submission_owned(GameMemory *,GameAddress root,SceneSubmissionState *);
int scene_submission_bounds_shadow(GameMemory *,GameAddress descriptor,const float world[12],SceneSubmissionState *);
int scene_submission_ground_shadow(GameMemory *,const float position[3],float diameter,SceneSubmissionState *);
#endif
