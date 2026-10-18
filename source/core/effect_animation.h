#ifndef UPRISING_EFFECT_ANIMATION_H
#define UPRISING_EFFECT_ANIMATION_H
#include "player_relocation.h"
typedef struct EffectAnimation {
    uint32_t birth_tick,contact_armed,loop_control;
    uint16_t variant,placement_flags;
    GameAddress attachment;uint32_t attachment_identity,subtype;
    float attachment_offset[3];
    int32_t emitted;GameAddress emitter;
    int16_t mark_kind,mark_delay;uint32_t mark_radius;
    int16_t last_mark;uint16_t mark_padding;
    uint32_t arrival_status,arrival_request;EntityArrivalAction arrival;
} EffectAnimation;
int effect_animation_import(GameMemory *,GameAddress,EffectAnimation *);
int effect_animation_projection_read(EffectAnimation *,uint32_t,void *,size_t);
int effect_animation_projection_write(EffectAnimation *,uint32_t,const void *,size_t);
/* Attached position uses the same stored-float stages as the original x87
 * path. The update services expose concrete resource and world boundaries. */
void effect_animation_attached_position(const EffectAnimation *,const float matrix[12],float position[3]);
typedef struct EffectAnimationServices {
    void *context;
    int (*duration)(void *,int32_t *);
    int (*attachment)(void *,GameAddress,uint32_t *,float matrix[12]);
    int (*place)(void *,int *inside);
    int (*contacts)(void *);
    int (*mark_coverage)(void *,int32_t,uint32_t);
    int (*visible)(void *,int *);
    int (*emit)(void *);
    int (*deactivate)(void *);
    uint32_t (*tick)(void *);
} EffectAnimationServices;
int effect_animation_update(EffectAnimation *,PresentationPose *,uint32_t tick,const EffectAnimationServices *);
#endif
