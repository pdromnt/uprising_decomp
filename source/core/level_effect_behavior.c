#include "level_effects.h"
#include "trail_model.h"
#include "gameplay_geometry.h"
#include "gameplay_leaves.h"
#include "game_state_ops.h"
#include "level_player_slots.h"
#include "state_helper_plans.h"
#include "game_intrusive_list.h"
#include "gameplay_spatial.h"
#include "effect_initialization.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

static float effect_real(GameMemory *m,GameAddress address) {
    uint32_t bits=game_memory_read_u32(m,address);float value;memcpy(&value,&bits,4u);return value;
}
static void effect_write_real(GameMemory *m,GameAddress address,float value) {
    uint32_t bits;memcpy(&bits,&value,4u);game_memory_write_u32(m,address,bits);
}
int level_effect_initialize(GameMemory *m,GameAddress object,const LevelEffectRequest *r,const LevelEffectServices *s) {
    LevelEffect effect={0};PresentationPose *pose=presentation_instance_pose(object);TrailModel *model;
    uint16_t word;unsigned axis;
    if(!pose || !(model=trail_model_find(pose->model)))return 0;
    effect.object=object;effect.resource=pose->model;effect.birth_tick=game_memory_read_u32(m,0x5ac6acu);
    effect.flags=game_memory_read_u32(m,r->recipe);
    word=game_memory_read_u16(m,r->recipe+0x28u);
    effect.lifetime=word<32768u?word:(uint32_t)((int32_t)word-65536);
    memcpy(effect.velocity,r->velocity,sizeof(effect.velocity));
    game_memory_write_u32(m,object+0xc0u,effect.birth_tick);
    game_memory_write_u32(m,object+0xc4u,effect.flags);game_memory_write_u32(m,object+0xc8u,effect.lifetime);
    word=game_memory_read_u16(m,r->recipe+0x2cu);
    game_memory_write_u32(m,object+0xccu,word<32768u?word:(uint32_t)((int32_t)word-65536));
    pose->radius=r->scale;
    for(axis=0;axis<3u;axis++) {
        pose->dimensions[axis]=r->scale;effect_write_real(m,object+0xb4u+axis*4u,r->velocity[axis]);
    }
    if(m->fault || !trail_model_initialize(m,model,r->recipe) || !s->place(s->factory.context,object,r))return 0;
    memcpy(effect.position,pose->position,sizeof(effect.position));
    return !m->fault && level_effects_begin(&effect);
}
int level_effect_emit(GameMemory *m,GameAddress owner,const LevelEffectRequest *input,const LevelEffectServices *s) {
    LevelEffectRequest request=*input;GameAddress object,manager;uint32_t next,offset;long double ceiling;
    if(!game_terrain_cell_presence(m,owner,0u))return m->fault?-1:0;
    if((long double)request.scale*effect_real(m,0x57217cu)>effect_real(m,0x5721a8u))request.scale=2048.0f;
    if(!game_memory_read_u32(m,owner+0x58u))return m->fault?-1:0;
    ceiling=(long double)effect_real(m,0x572184u)-request.scale;
    if(m->fault)return -1;
    if(!(request.position[0]>=request.scale && request.position[2]>=request.scale &&
         request.position[0]<=ceiling && request.position[2]<=(float)ceiling))return 0;
    object=effect_factory_take(m,effect_factory_family(0x42u),&s->factory);if(!object)return -1;
    manager=game_memory_read_u32(m,0x5f0210u+(game_memory_read_u32(m,owner+0x14u)&63u)*4u);
    if(level_player_slots_owns_player(manager)) {
        if(!level_player_slots_next_owner_sequence(m,manager,&request.identity,&next))goto failed;
        game_memory_write_u32(m,manager+24u,next);
    }else request.identity=game_state_next_owner_sequence(m,manager);
    if(m->fault || !level_effect_initialize(m,object,&request,s))goto failed;
    offset=state_helper_flagged_member_offset(game_memory_read_u32(m,object+16u));
    if(level_player_slots_owns_player(manager) && !level_player_slots_attach_scene_member(manager,object,offset))goto failed;
    game_list_insert_after(m,object,manager+offset,0x48u,0x4cu);
    if(m->fault){level_player_slots_forget_scene_member(object);goto failed;}
    return 1;
failed:
    level_effects_retire(object);
    s->factory.destroy(s->factory.context,object);
    return -1;
}
static int32_t effect_bucket(float value) {
    uint32_t low;
    int64_t integer;
    if(!isfinite(value) || (long double)value < -0x1p63L || (long double)value >= 0x1p63L)low=0u;
    else low=(uint32_t)(uint64_t)(int64_t)value;
    integer=low<=INT32_MAX?(int64_t)low:(int64_t)low-4294967296LL;
    return (int32_t)((integer<0?integer-4095:integer)/4096);
}
int level_effect_update(GameMemory *m,GameAddress object,const LevelEffectServices *s) {
    PresentationPose *pose=presentation_instance_pose(object);TrailModel *model;LevelEffect effect;
    LevelEffectStep step;uint32_t tick;unsigned axis;
    if(!pose || !(model=trail_model_find(pose->model)))return 0;
    tick=game_memory_read_u32(m,0x5ac6acu);
    step=level_effects_step(object,tick,effect_real(m,0x574afcu),effect_real(m,0x574b00u),&effect);
    if(m->fault || step==LEVEL_EFFECT_MISSING)return -1;
    if(step==LEVEL_EFFECT_EXPIRED)return s->reset(s->factory.context,object)?1:-1;
    memcpy(pose->position,effect.position,sizeof(effect.position));
    for(axis=0;axis<3u;axis++)effect_write_real(m,object+0xb4u+axis*4u,effect.velocity[axis]);
    trail_model_update(model,effect.flags,effect.birth_tick,tick,effect_real(m,0x574afcu),effect_real(m,0x574b08u));
    if(m->fault)return -1;
    if(!gameplay_geometry_world_bounds_values(pose->radius,pose->local_position))return s->reset(s->factory.context,object)?1:-1;
    game_list_detach_node(m,object,4u,8u);
    gameplay_spatial_insert(m,0x5f0800u,object,effect_bucket(pose->position[0]),effect_bucket(pose->position[2]),(pose->flags&0x400u)!=0u);
    return m->fault?-1:1;
}
int level_effect_fragment_event(GameMemory *m,GameAddress owner,uint8_t flags,const LevelEffectServices *s) {
    LevelEffectRequest r={0};int32_t sound;unsigned axis;int emitted;
    if(!owner || !game_terrain_cell_presence(m,owner,0u))return m->fault?-1:1;
    if(flags&0x10u) {
        r.recipe=0x584350u;r.velocity[1]=8.0f;
        sound=game_memory_read_u8(m,owner+12u)==2u?6:133;
    }else {
        r.recipe=game_memory_read_u32(m,0x5ac908u)==4u?0x584380u:0x5843b0u;
        r.velocity[1]=11.0f;sound=4;
    }
    for(axis=0;axis<3u;axis++)r.position[axis]=effect_real(m,owner+24u+axis*4u);
    r.position[1]=(float)gameplay_geometry_terrain_height(m,r.position[0],r.position[2]);
    r.angles[0]=(uint16_t)effect_initialization_random(m,effect_real(m,0x572fb4u));
    r.scale=(float)((long double)effect_real(m,owner+0x38u)*effect_real(m,0x572f84u));
    if(m->fault)return -1;
    emitted=level_effect_emit(m,owner,&r,s);if(emitted<0)return -1;
    return s->sound(s->factory.context,sound,r.position)?1:-1;
}
