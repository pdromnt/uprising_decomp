#include "sprite_fragment.h"
#include "effect_initialization.h"
#include "terrain_asset.h"
#include "game_intrusive_list.h"
#include "gameplay_spatial.h"
#include "gameplay_geometry.h"
#include <math.h>
#include <string.h>
static float scalar(GameMemory *m,GameAddress a){uint32_t b=game_memory_read_u32(m,a);float f;memcpy(&f,&b,4u);return f;}
static void put_scalar(GameMemory *m,GameAddress a,float f){uint32_t b;memcpy(&b,&f,4u);game_memory_write_u32(m,a,b);}
static int32_t signed_word(uint32_t b){int32_t n;memcpy(&n,&b,4u);return n;}
static uint32_t arithmetic_shift(uint32_t value,unsigned bits){return (value>>bits)|((value&0x80000000u)?(UINT32_MAX<<(32u-bits)):0u);}
static uint32_t truncate_word(long double value){
    int64_t n=!isfinite(value) || value< -0x1p63L || value>=0x1p63L?INT64_MIN:(int64_t)value;
    return (uint32_t)(uint64_t)n;
}
static int terrain_flags(GameMemory *m,const float point[3],uint8_t *flags){
    uint32_t x=truncate_word(point[0]),z=truncate_word(point[2]);
    uint32_t offset=(z&0xfffffe00u)*4u+arithmetic_shift(x&0xfffffe3fu,6u);
    if(m->terrain && m->terrain->cells){uint32_t i=offset/8u;if(i>=65536u){m->fault=1;return 0;}*flags=m->terrain->cells[i].flags;}
    else *flags=game_memory_read_u8(m,game_memory_read_u32(m,0x597d90u)+offset+5u);
    return !m->fault;
}
GameAddress fragment_profile(GameMemory *m,int16_t selector){
    GameAddress table;float scale;
    if(selector==0){table=game_memory_read_u32(m,0x5ac908u)==1u?0x5ad8a0u:0x5ad880u;scale=scalar(m,0x572f8cu);}
    else if(selector==1){table=0x5ad890u;scale=scalar(m,0x572f8cu);}
    else if(selector==2){table=0x5ad8b0u;scale=scalar(m,0x572f90u);}
    else return 0;
    return game_memory_read_u32(m,table+effect_initialization_random(m,scale)*4u);
}
int fragment_begin(GameMemory *m,GameAddress object,const FragmentRequest *r,const FragmentServices *ports){
    PresentationPose *pose=presentation_instance_pose(object);EffectModel *model;unsigned i;float radius;
    if(!pose || !(model=effect_model_find(pose->model)))return 0;
    if(!effect_model_configure(m,model,r->profile,r->scale,(r->scale!=0.0f)))return 0;
    if(effect_initialization_random(m,scalar(m,0x572f84u)))effect_quad_set_flags(&model->quad,0x80000000u,0,0,0);
    else effect_quad_clear_flags(&model->quad,0x80000000u);
    model->start_tick=game_memory_read_u32(m,0x5ac6acu);
    radius=(float)((long double)model->quad.width*scalar(m,0x572f88u));
    pose->radius=radius;for(i=0;i<3u;i++)pose->dimensions[i]=radius;
    game_memory_write_u32(m,object+0xd4u,(uint32_t)r->lifetime);
    game_memory_write_u32(m,object+0xdcu,r->source_identity);
    game_memory_write_u8(m,object+0xe0u,r->flags);game_memory_write_u32(m,object+0xe4u,0u);
    for(i=0;i<3u;i++){put_scalar(m,object+0xb4u+i*4u,r->velocity[i]);game_memory_write_u16(m,object+0xccu+i*2u,0u);}
    if(m->fault || !ports->place(ports->context,object,r))return 0;
    game_memory_write_u16(m,object+0xd8u,0xffffu);return !m->fault;
}
int fragment_prepare(GameMemory *m,GameAddress object,const FragmentServices *ports){
    PresentationPose *pose=presentation_instance_pose(object);uint8_t flags;uint32_t delta;unsigned i;long double h;
    if(!pose)return 0;
    flags=game_memory_read_u8(m,object+0xe0u);
    memcpy(pose->local_position,pose->position,sizeof(pose->position));
    for(i=0;i<3u;i++)pose->local_angles[i]=pose->angles[2u-i];
    if(flags&2u){if(!ports->height(ports->context,pose->local_position[0],pose->local_position[2],&h))return 0;
        pose->local_position[1]=(float)(h+pose->dimensions[1]);
    }else if(game_memory_read_u32(m,0x5eff04u)){
        delta=game_memory_read_u32(m,0x5acbe8u)-game_memory_read_u32(m,0x5acc80u);
        for(i=0;i<3u;i++){pose->local_position[i]=(float)((long double)scalar(m,object+0xb4u+i*4u)*signed_word(delta)+pose->local_position[i]);
            pose->local_angles[i]=(uint16_t)(pose->local_angles[i]+(uint32_t)game_memory_read_u16(m,object+(i==0u?0xceu:i==1u?0xd0u:0xccu))*(uint16_t)delta);}
    }
    return !m->fault && fragment_build_model(m,object);
}
int fragment_build_world(GameMemory *m,GameAddress object){
    PresentationPose *pose=presentation_instance_pose(object);unsigned i;
    if(!pose)return 0;
    for(i=0;i<12u;i++)pose->world_matrix[i]=0.0f;
    pose->world_matrix[0]=pose->world_matrix[5]=pose->world_matrix[10]=1.0f;
    if(game_memory_read_u8(m,object+0xe0u)&8u){
        uint16_t a=pose->angles[0]&2047u;
        float s=scalar(m,0x6298a8u+(uint32_t)a*4u),c=scalar(m,0x6298a8u+((a+512u)&2047u)*4u);
        pose->world_matrix[0]=pose->world_matrix[10]=c;
        pose->world_matrix[2]=s;pose->world_matrix[8]=-s;
    }
    for(i=0;i<3u;i++)pose->world_matrix[i*4u+3u]=pose->position[i];
    return !m->fault;
}
int fragment_build_model(GameMemory *m,GameAddress object){
    PresentationPose *pose=presentation_instance_pose(object);EffectModel *model;unsigned i;uint8_t flags;
    if(!pose || !(model=effect_model_find(pose->model)))return 0;
    flags=game_memory_read_u8(m,object+0xe0u);
    if(flags&8u){uint16_t a=pose->local_angles[2]&2047u;float s=scalar(m,0x6298a8u+(uint32_t)a*4u),c=scalar(m,0x6298a8u+((a+512u)&2047u)*4u);
        for(i=0;i<12u;i++)model->local[i]=0.0f;
        model->local[0]=model->local[10]=c;model->local[2]=s;model->local[8]=-s;model->local[5]=1.0f;
        for(i=0;i<3u;i++)model->local[i*4u+3u]=pose->local_position[i];
    }else{
        PresentationPose view=*pose;memcpy(view.position,pose->local_position,sizeof(view.position));
        presentation_pose_build_billboard(&view);memcpy(model->local,view.world_matrix,sizeof(model->local));}
    return !m->fault;
}
int fragment_move(GameMemory *m,GameAddress object){
    PresentationPose *pose=presentation_instance_pose(object);unsigned i;
    if(!pose)return 0;
    for(i=0;i<3u;i++){pose->position[i]=(float)(pose->position[i]+(long double)scalar(m,object+0xb4u+i*4u));
        pose->angles[i]=(uint16_t)(pose->angles[i]+game_memory_read_u16(m,object+(i==0u?0xccu:i==1u?0xd0u:0xceu)));}
    pose->angles[0]&=2047u;
    for(i=1u;i<3u;i++){uint16_t angle=pose->angles[i]&2047u;
        pose->angles[i]=(uint16_t)(angle>1024u?angle-2048u:angle);}
    return !m->fault;
}
int fragment_terrain_response(GameMemory *m,GameAddress object,const FragmentServices *ports){
    PresentationPose *pose=presentation_instance_pose(object);uint8_t flags,terrain;int32_t lifetime;long double h;float multiplier;
    if(!pose || !terrain_flags(m,pose->position,&terrain))return 0;
    flags=game_memory_read_u8(m,object+0xe0u);
    if(terrain&0x50u){if((flags&4u) && !ports->terrain_event(ports->context,object,terrain))return 0;
        return ports->deactivate(ports->context,object);}
    if(!ports->sound(ports->context,object,(int16_t)game_memory_read_u16(m,object+0xd8u),pose->position))return 0;
    lifetime=signed_word(game_memory_read_u32(m,object+0xd4u));
    if(lifetime<=0){if(!(flags&1u))return ports->deactivate(ports->context,object);
        game_memory_write_u8(m,object+0xe0u,flags|2u);game_memory_write_u32(m,object+0xe4u,game_memory_read_u32(m,0x5ac6acu));
        if(!ports->height(ports->context,pose->position[0],pose->position[2],&h))return 0;
        pose->position[1]=(float)(h+scalar(m,0x572f9cu));return !m->fault;}
    game_memory_write_u32(m,object+0xd4u,(uint32_t)lifetime-1u);
    {unsigned category=(pose->identity&0xc0u)>>6u;
        multiplier=category==2u?1.0f:scalar(m,category==0u?0x572f98u:category==1u?0x572f88u:0x572f94u);}
    put_scalar(m,object+0xb8u,(float)(fabsl(scalar(m,object+0xb8u))*multiplier));
    {uint32_t phase=game_memory_read_u32(m,0x5ac6acu)&3u;float step=scalar(m,0x572f84u);
        put_scalar(m,object+0xb4u,(float)((long double)scalar(m,object+0xb4u)+(phase==0u||phase==2u?step:-step)));
        put_scalar(m,object+0xbcu,(float)((long double)scalar(m,object+0xbcu)+(phase==1u||phase==2u?step:-step)));}
    return !m->fault;
}
int fragment_expire(GameMemory *m,GameAddress object,const FragmentServices *ports){
    uint8_t flags=game_memory_read_u8(m,object+0xe0u);
    if((flags&2u) && signed_word(game_memory_read_u32(m,object+0xe4u)+90u)<signed_word(game_memory_read_u32(m,0x5ac6acu)))
        return ports->deactivate(ports->context,object);
    return !m->fault;
}
int fragment_terrain_contact(GameMemory *m,const PresentationPose *pose,int *hit){
    float normal[3];return presentation_terrain_contact(m,pose,normal,hit);
}
/*43d820: motion/contact/spatial publication. The43d530 frame wrapper owns
 *the settled-fragment expiry gate; a direct motion call must not apply it. */
int fragment_motion(GameMemory *m,GameAddress object,const FragmentServices *ports){
    PresentationPose *pose=presentation_instance_pose(object);int hit;
    if(!pose)return 0;
    put_scalar(m,object+0xb8u,(float)((long double)scalar(m,object+0xb8u)-scalar(m,0x5765b8u)));
    if(m->fault || !fragment_move(m,object) || !fragment_build_world(m,object) ||
       !presentation_pose_transform_vertices(m,pose,0))return 0;
    if(!gameplay_geometry_world_bounds_values(pose->radius,pose->position))return ports->deactivate(ports->context,object);
    game_list_detach_node(m,object,4u,8u);
    if(m->fault || !fragment_terrain_contact(m,pose,&hit))return 0;
    if(hit && !fragment_terrain_response(m,object,ports))return 0;
    if(game_memory_read_u32(m,object+0x58u))gameplay_spatial_insert(m,0x5f0800u,object,
        signed_word(arithmetic_shift(truncate_word(pose->position[0]),12u)),signed_word(arithmetic_shift(truncate_word(pose->position[2]),12u)),(pose->flags&0x400u)!=0u);
    return !m->fault;
}
int fragment_update(GameMemory *m,GameAddress object,const FragmentServices *ports){
    if(game_memory_read_u8(m,object+0xe0u)&2u)return fragment_expire(m,object,ports);
    return !m->fault && fragment_motion(m,object,ports);
}
int fragment_spawn(GameMemory *m,GameAddress source,int32_t count,const float inherited[3],const FragmentSpawnServices *ports){
    PresentationPose borrowed,*pose=presentation_instance_pose(source);uint8_t terrain;int visible;int32_t i,total;
    GameAddress world;long double height;void *context=ports->fragment.context;
    /*Small ground/scenery sources own only the64h base record. Borrow that
     *bounded prefix for emission; never install another writable pose owner. */
    if(!pose){if(!presentation_pose_import_prefix(m,source,&borrowed,0x64u))return 0;pose=&borrowed;}
    if(!ports->visible(context,source,&visible))return 0;
    if(!visible)return 1;
    if(!terrain_flags(m,pose->position,&terrain))return 0;
    if(terrain&0x50u)return ports->fragment.terrain_event(context,source,terrain);
    if(!ports->fragment.height(context,pose->position[0],pose->position[2],&height))return 0;
    /* Height is rounded to a stored float before the fragment radius is added. */
    {uint32_t cursor=game_memory_read_u32(m,0x6308c0u);long double random;int64_t n;
        if(cursor==1024u)cursor=0u;
        random=(long double)count*scalar(m,0x6308c8u+cursor*4u);
        game_memory_write_u32(m,0x6308c0u,cursor+1u);
        n=!isfinite(random) || random< -0x1p63L || random>=0x1p63L?INT64_MIN:(int64_t)random;
        total=signed_word((uint32_t)(uint64_t)n);}
    world=game_memory_read_u32(m,0x5f0210u+(pose->identity&63u)*4u);
    if(m->fault)return 0;
    for(i=0;i<total;i++){
        FragmentRequest request={0};GameAddress object=ports->acquire(context);PresentationPose *child;
        float vx,vy,vz;unsigned axis;
        if(!object)return 0;
        request.scale=(float)signed_word(effect_initialization_random(m,scalar(m,0x572fa0u))+20u);
        vx=(float)signed_word(effect_initialization_random(m,scalar(m,0x572fa4u)));
        vy=(float)signed_word(effect_initialization_random(m,scalar(m,0x572fa8u))+10u);
        vz=(float)signed_word(effect_initialization_random(m,scalar(m,0x572fa4u)));
        if(effect_initialization_random(m,scalar(m,0x572f84u)))vx=-vx;
        if(effect_initialization_random(m,scalar(m,0x572f84u)))vz=-vz;
        request.velocity[0]=(float)((long double)vx+inherited[0]);
        request.velocity[1]=(float)((long double)vy+inherited[1]);
        request.velocity[2]=(float)((long double)vz+inherited[2]);
        child=presentation_instance_pose(object);if(!child)goto failed;
        for(axis=0;axis<3u;axis++)request.position[axis]=pose->position[axis];
        request.position[1]=(float)((long double)(float)height+(long double)child->radius*scalar(m,0x572f84u));
        request.source_identity=pose->identity;request.lifetime=i%3+2;
        if(!ports->sequence(context,world,&request.identity))goto failed;
        request.profile=fragment_profile(m,0);
        if(m->fault || !fragment_begin(m,object,&request,&ports->fragment) ||
           !ports->publish(context,world,object))goto failed;
        continue;
failed:
        ports->destroy(context,object);return 0;
    }
    return !m->fault;
}
