#ifndef UPRISING_PROJECTILE_H
#define UPRISING_PROJECTILE_H
#include "presentation_pose.h"
#include "effect_initialization.h"

typedef struct Projectile {
    unsigned projection_size;
    float velocity[3];
    uint32_t auxiliary_c0[3]; /* Shared flight/contact fields: role still varies. */
    uint16_t angular_velocity[4]; /* Fourth word preserves retail's unused padding. */
    GameAddress shooter, recipe, target;
    uint32_t shooter_identity, birth_tick, target_identity, search_tick, lifetime;
    int32_t cell_x, cell_z;
    float target_position[3], previous_position[3], steering;
    uint32_t split;
} Projectile;

typedef struct ProjectileServices {
    void *context;
    int (*visible)(void *, const float[3], const float[3], int *);
} ProjectileServices;
int projectile_import(GameMemory *, GameAddress, Projectile *);
int projectile_begin(GameMemory *,Projectile *,const PresentationPose *,uint32_t,
    GameAddress,GameAddress,const ProjectileServices *);
int projectile_begin_derived(GameMemory *,Projectile *,PresentationPose *,
    const float position[3],uint16_t heading,float strength,const ProjectileServices *);
int projectile_begin_delayed(GameMemory *,Projectile *,const PresentationPose *,const ProjectileServices *);
int projectile_delayed_force(GameMemory *,Projectile *,PresentationPose *,uint32_t,const ProjectileServices *);
int projectile_delayed_move(GameMemory *,Projectile *,PresentationPose *,uint32_t);
int projectile_begin_motion(GameMemory *,Projectile *,PresentationPose *,uint32_t,
    GameAddress recipe,GameAddress shooter,uint16_t pitch);
int projectile_motion_update(GameMemory *,Projectile *,PresentationPose *);
int projectile_motion_post(GameMemory *,Projectile *,PresentationPose *);
int projectile_motion_settle(GameMemory *,Projectile *,PresentationPose *);
int projectile_bounce_force(GameMemory *,Projectile *,PresentationPose *);
/*433190/433330: physical fragments share the projectile owner. The e4
 *word is a split generation here, not a second spatial-state mirror. */
int projectile_fragment_begin(GameMemory *,Projectile *,PresentationPose *,
    GameAddress recipe,GameAddress shooter,int primary,uint32_t generation);
int projectile_fragment_force(GameMemory *,Projectile *);
int projectile_line_begin(GameMemory *,Projectile *,const PresentationPose *,const float target[3],float strength);
int projectile_line_force(GameMemory *,Projectile *);
int projectile_line_move(GameMemory *,Projectile *,PresentationPose *);
int projectile_motion_target(GameMemory *,const PresentationPose *,float from[3],float to[3],float *strength,int *allowed);
int projectile_motion_blocked(GameMemory *,const float position[3]);
typedef struct ProjectileMotionServices {
    void *context;
    int (*reset)(void *);
    int (*gather)(void *,const float previous[3]);
    int (*peer_contact)(void *,GameAddress,const float velocity[3],GameAddress point);
    int (*impact)(void *,GameAddress other,GameAddress point);
} ProjectileMotionServices;
int projectile_motion_contact(GameMemory *,PresentationPose *,GameAddress other,GameAddress point,
    const ProjectileMotionServices *);
int projectile_motion_advance(GameMemory *,GameAddress,Projectile *,PresentationPose *,
    const ProjectileMotionServices *);
int projectile_projection_read(Projectile *, uint32_t, void *, size_t);
int projectile_projection_write(Projectile *, uint32_t, const void *, size_t);
GameAddress projectile_search_cell(GameMemory *, const Projectile *, const PresentationPose *,
    int32_t, int32_t, GameAddress, float *, const ProjectileServices *);
int projectile_home(GameMemory *, Projectile *, const PresentationPose *, uint32_t,
    const ProjectileServices *);
void projectile_move(Projectile *, PresentationPose *, float);
/*5368b0 dispatches the family's world/vertex refresh after moving. Lasers
 *retain rotation but must refresh their hull vertices on every tick. */
int projectile_move_program(GameMemory *,Projectile *,PresentationPose *,GameAddress method,int laser);
int projectile_orient(GameMemory *,const Projectile *,PresentationPose *);
typedef struct ProjectileContactServices {
    void *context;
    int (*collision)(void *,GameAddress candidate,GameAddress source,
        float contact[3],int *hit);
} ProjectileContactServices;
GameAddress projectile_find_contact(GameMemory *,GameAddress,const Projectile *,
    const PresentationPose *,const GameAddress heads[9],
    const ProjectileContactServices *,float contact[3]);
int projectile_expired(GameMemory *,const Projectile *,uint32_t tick);
int projectile_terrain_contact(GameMemory *,const PresentationPose *,float normal[3],int *hit);
int presentation_terrain_contact(GameMemory *,const PresentationPose *,float normal[3],int *hit);
/* Build the impact's existing effect request; sequence/profile publication is
 * performed after factory acquisition by the owning caller. */
int projectile_impact_request(GameMemory *,const Projectile *,const float point[3],
    GameAddress attachment,int terrain,EffectInitialization *,uint16_t *selector);
#endif
