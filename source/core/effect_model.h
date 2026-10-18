#ifndef UPRISING_EFFECT_MODEL_H
#define UPRISING_EFFECT_MODEL_H
#include "game_memory.h"
/* Runtime billboard instances own their quad, frame state and sizing. Asset
 * programs and shade inputs remain borrowed until their producers migrate. */
typedef struct EffectQuadVertex {float x,y,reciprocal_depth,w;} EffectQuadVertex;
typedef struct EffectQuad {
    EffectQuadVertex vertices[4];
    GameAddress vertex_refs[4];
    float uv[4][2],shading[4][4];
    int16_t texture_width,texture_height;
    GameAddress texture;
    uint32_t count,flags;GameAddress vertex_array,next,uv_ref,material_ref;
    uint32_t material_padding,tint;
    GameAddress shade_source,shade_alternate;
    float width,height,bound_radius;
} EffectQuad;
typedef struct EffectModel {
    GameAddress projection,methods,parent,profile;
    float local[12];GameAddress descriptor;uint32_t mode;GameAddress lighting;
    EffectQuad quad;uint32_t start_tick;
    struct EffectProfileBank *bank;
    struct EffectProfileBank *profile_owner;
    uint32_t bank_owner;
} EffectModel;
EffectModel *effect_model_find(GameAddress);
GameAddress effect_model_create(unsigned mode);
/*49ada0/49ac90: source owns profiles; copies retain the same immutable bank.
 *On success ownership of the supplied profile records transfers to the model. */
GameAddress effect_model_create_bank(const GameAddress *,uint16_t count);
int effect_model_select_profile(GameMemory *,EffectModel *,uint16_t,uint32_t tick);
GameAddress effect_model_clone(GameMemory *,GameAddress source);
int effect_model_construct(GameMemory *,GameAddress object,GameAddress source);
int effect_model_release(GameAddress);
int effect_model_direction(GameMemory *,const EffectModel *,uint16_t *);
void effect_quad_set_flags(EffectQuad *,uint32_t flags,uint32_t tint,GameAddress shade,GameAddress alternate);
void effect_quad_clear_flags(EffectQuad *,uint32_t flags);
int effect_model_configure(GameMemory *,EffectModel *,GameAddress profile,float scale,int explicit_scale);
int effect_model_sample_frame(GameMemory *,EffectModel *,uint16_t direction,uint32_t tick_delta,int *visible);
typedef struct EffectModelView {
    float world_to_view[12],focal,vertical_scale;
    float center[2],roll_sine,roll_cosine,near_depth,far_bias,unit,half;
    int32_t far_distance;
} EffectModelView;
int effect_quad_project(EffectQuad *,const float position[3],const EffectModelView *);
#endif
