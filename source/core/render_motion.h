#ifndef UPRISING_RENDER_MOTION_H
#define UPRISING_RENDER_MOTION_H
#include <stdint.h>
typedef struct RenderMotionEntry {
    uint64_t key,frame;
    float current[3],previous[3];
    int valid,ambiguous;
} RenderMotionEntry;
/* Bounded four-way cache, owned by the renderer. These are previous rendered
 * positions, never another authoritative simulation/object state. */
typedef struct RenderMotionHistory {RenderMotionEntry *entries;} RenderMotionHistory;
void render_motion_release(RenderMotionHistory *);
void render_motion_reset(RenderMotionHistory *);
/* Halton[2,3], pixel offsets in [-.5,.5], matching the FSR jitter convention.
 * Sequence length belongs to the render/display ratio; no simulation state. */
int render_motion_jitter(uint64_t frame,uint32_t phases,float output[2]);
/* Previous screen positions of an affine textured rectangle's four corners.
 * Used for scrolling backgrounds, not geometry depth. Gaps/resource changes,
 * degenerate mappings and shifts beyond half a viewport reject correspondence. */
typedef struct RenderMotionPlane {uint64_t frame,identity;float bounds[4],uv[4];} RenderMotionPlane;
int render_motion_plane(RenderMotionPlane *,uint64_t frame,uint64_t identity,
    const float bounds[4],const float uv[4],float previous[4][4]);
/* Rendered world-to-view pose, independent of camera allocation identity.
 * Cuts invalidate geometry and sky correspondence before either is submitted.
 * Rotation uses matrices (no Euler wrap); translation uses inverse rigid pose. */
typedef struct RenderMotionCamera {uint64_t frame,time_ns;float view[12];} RenderMotionCamera;
enum {RENDER_CAMERA_INITIAL=1,RENDER_CAMERA_GAP=2,RENDER_CAMERA_TIME=4,
    RENDER_CAMERA_POSE=8,RENDER_CAMERA_INVALID=16};
unsigned render_motion_camera(RenderMotionCamera *,uint64_t frame,uint64_t time_ns,
    const float view[12],float max_translation);
int render_motion_sample(RenderMotionHistory *,uint64_t frame,uint64_t key,
    const float current[3],float previous[3]);
/* Pack previous screen coordinates for correct projective interpolation.
 * Output is {previous_x*q/q_previous, previous_y*q/q_previous, q/q_previous, 1}. */
int render_motion_projective(float output[4],const float current[3],const float previous[3]);
typedef struct RenderMotionSource {
    uint64_t key;float current[3];
    uint32_t parents[2];float fraction;int screen_space;
} RenderMotionSource;
typedef int (*RenderMotionLookup)(void *,uint32_t,RenderMotionSource *);
/* Resolve clipped points from this frame's edge provenance. Bounds/cycle and
 * missing-endpoint failures return invalid motion, never a scratch-address ID. */
int render_motion_resolve(RenderMotionHistory *,uint64_t,uint32_t,
    RenderMotionLookup,void *,float output[4]);
#endif
