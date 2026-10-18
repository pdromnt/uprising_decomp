#include "effect_initialization.h"
#include "presentation_lifecycle.h"
#include "presentation_instance.h"
#include "sprite_fragment.h"
#include "game_matrix.h"
#include "gameplay_state.h"
#include <string.h>
static float read_float(GameMemory *,GameAddress);

int effect_burst(GameMemory *memory,GameAddress source,uint16_t selector,int32_t count,
        int16_t sound,const EffectBurstServices *services){
    PresentationPose borrowed,*pose=presentation_instance_pose(source);GameAddress manager,child;
    EffectInitialization request={0};int32_t index;unsigned axis;
    if(!services || !services->acquire || !services->emit)return 0;
    /*Gate/scenery death shares42d9a0 but owns only a bounded legacy base,
     *not a second presentation instance. Emission borrows its immutable values. */
    if(!pose){if(!presentation_pose_import_prefix(memory,source,&borrowed,0x64u))return 0;pose=&borrowed;}
    child=services->acquire(services->context);if(!child)return 0;
    manager=game_memory_read_u32(memory,0x5f0210u+(pose->identity&63u)*4u);
    request.profile=effect_initialization_profile(memory,selector);
    request.subtype=game_memory_read_u8(memory,source+12u);request.mark_kind=-1;
    request.scale[0]=request.scale[1]=(float)((long double)pose->radius*read_float(memory,0x57217cu));
    memcpy(request.position,pose->position,12u);
    if(memory->fault || !services->emit(services->context,manager,child,&request))goto failed;
    if(sound!=-1 && (!services->sound || !services->sound(services->context,child,sound)))return 0;
    for(index=0;index<count;index++){
        child=services->acquire(services->context);if(!child)return 0;
        memset(&request,0,sizeof(request));
        request.subtype=game_memory_read_u8(memory,source+12u);request.mark_kind=-1;
        for(axis=0;axis<3u;axis++){
            uint32_t positive=effect_initialization_random(memory,read_float(memory,0x57217cu));
            long double displacement=(long double)pose->radius*read_float(memory,
                axis==0u?(positive?0x572198u:0x57219cu):0x572198u);
            request.position[axis]=(float)(axis && !positive?
                (long double)pose->position[axis]-displacement:displacement+pose->position[axis]);
        }
        request.scale[0]=request.scale[1]=(float)((long double)pose->radius*read_float(memory,0x5721a0u));
        request.profile=effect_initialization_profile(memory,selector);
        if(memory->fault || !services->emit(services->context,manager,child,&request))goto failed;
    }
    return !memory->fault;
failed:
    if(services->discard)services->discard(services->context,child);
    return 0;
}

int fragment_mesh_spawn(GameMemory *memory,GameAddress source,GameAddress descriptor,
        const float matrix[12],const float inherited[3],int32_t stride,int16_t sound,
        const FragmentSpawnServices *services){
    GameAddress manager,points,child;uint32_t identity;
    int visible;uint32_t index,count;float multiplier,origin[3];unsigned axis;
    if(!services || !matrix || !inherited)return 0;
    if(!game_memory_read_u32(memory,0x5ad8acu))return !memory->fault;
    if(!services->visible(services->fragment.context,source,&visible))return 0;
    if(!visible)return 1;
    /*Small scenery/generator records share this emitter without owning a
     *PresentationPose. These bounded reads also resolve existing pose owners. */
    multiplier=(float)((long double)read_float(memory,0x572188u)/read_float(memory,source+0x38u));
    identity=game_memory_read_u32(memory,source+0x14u);
    for(axis=0;axis<3u;axis++)origin[axis]=read_float(memory,source+0x18u+axis*4u);
    count=game_memory_read_u32(memory,descriptor+0x14u);points=game_memory_read_u32(memory,descriptor);
    manager=game_memory_read_u32(memory,0x5f0210u+(identity&63u)*4u);
    if(memory->fault)return 0;
    if(!count || count>INT32_MAX)return 1;
    if(stride<=0 || (uint64_t)points+(uint64_t)count*12u>UINT32_MAX)return 0;
    for(index=0;index<count;index+=(uint32_t)stride){
        FragmentRequest request={0};float local[3],delta[3];uint32_t ordinal=index/(uint32_t)stride;
        for(axis=0;axis<3u;axis++)local[axis]=read_float(memory,points+index*12u+axis*4u);
        game_matrix_transform_point_values(request.position,matrix,local);
        for(axis=0;axis<3u;axis++){
            delta[axis]=(float)((long double)request.position[axis]-origin[axis]);
            request.velocity[axis]=(float)((long double)multiplier*delta[axis]+inherited[axis]);
        }
        child=services->acquire(services->fragment.context);if(!child)return 0;
        request.profile=fragment_profile(memory,1);request.source_identity=identity;
        request.lifetime=(int32_t)(ordinal%3u);
        request.scale=(float)((long double)(index&3u)*read_float(memory,0x57218cu)+read_float(memory,0x572190u));
        if(memory->fault || !services->sequence(services->fragment.context,manager,&request.identity) ||
           !fragment_begin(memory,child,&request,&services->fragment))goto failed;
        if(sound!=-1 && (ordinal&1u))game_memory_write_u16(memory,child+0xd8u,(uint16_t)sound);
        if(memory->fault || !services->publish(services->fragment.context,manager,child))goto failed;
        if(count-index<=(uint32_t)stride)break;
        continue;
failed:
        services->destroy(services->fragment.context,child);return 0;
    }
    return !memory->fault;
}
static float read_float(GameMemory *memory,GameAddress address){
    uint32_t bits=game_memory_read_u32(memory,address);
    float value;memcpy(&value,&bits,sizeof(value));return value;
}
static uint32_t float_bits(float value){
    uint32_t bits;memcpy(&bits,&value,sizeof(bits));return bits;
}

int effect_initialize_transition(GameMemory *memory,EffectAnimation *animation,
        PresentationPose *pose,EffectModel *model,uint32_t sequence,
        const float position[3],uint16_t status,GameAddress target,
        uint32_t mode,uint32_t request,const EffectTransitionServices *services){
    EffectInitialization initialization={0};GameAddress attachment=0u,world,catalog=0u;
    uint32_t flags,identity;float radius,coefficient;unsigned axis;
    void *context=services->initialization.context;
    animation->arrival_status=(animation->arrival_status&0xffff0000u)|status;animation->arrival.target=target;
    animation->arrival.mode=mode;animation->arrival_request=request;
    for(axis=0u;axis<3u;++axis)initialization.position[axis]=position[axis];
    world=mode==2u?0u:game_memory_read_u32(memory,0x5f0210u+(sequence&63u)*4u);
    if(memory->fault)return 0;
    if(mode==0u){
        pose->flags&=~0x400u;
        if(request && !services->lookup(context,world,request,&catalog))return 0;
        if(request && (!catalog || (game_memory_read_u32(memory,catalog+0x10u)&0x20000u))){
            GameAddress head=game_memory_read_u32(memory,target+0x50u);
            GameAddress next=game_memory_read_u32(memory,head+4u);
            game_memory_write_u32(memory,target+0x14u,0u);
            game_memory_write_u32(memory,target+8u,head);
            game_memory_write_u32(memory,target+4u,next);
            if(next)game_memory_write_u32(memory,next+8u,target);
            game_memory_write_u32(memory,head+4u,target);
            animation->arrival.target=0u;
        }else{
            long double height;
            if(!services->initialization.terrain_height(context,position[0],position[2],&height))return 0;
            initialization.position[1]=(float)(height+read_float(memory,target+0x30u));
            flags=game_memory_read_u32(memory,target+0x10u);
            if(flags&0x4000u)initialization.position[1]=(float)((long double)initialization.position[1]+read_float(memory,0x59d8a8u));
            if(memory->fault || !services->sequence(context,world,initialization.position,status,&identity) ||
               !services->bind(context,target,identity))return 0;
            animation->arrival.identity=game_memory_read_u32(memory,target+0x14u);
            if(request){
                game_memory_write_u32(memory,target+0x260u,request);
                if(memory->fault || !services->register_target(context,catalog,target))return 0;
            }
            if(!services->publish(context,world,target))return 0;
            flags=game_memory_read_u32(memory,target+0x10u);
            if(flags&1u)attachment=target;
            if((flags&0x100000u) && !services->delegate(context,world,target))return 0;
            /* Delegate constructors can change the target's flags. */
            flags=game_memory_read_u32(memory,target+0x10u);
            game_memory_write_u32(memory,target+0x10u,flags|0x200000u);
        }
    }else if(mode==1u){
        pose->flags|=0x400u;
        animation->arrival.identity=game_memory_read_u32(memory,target+0x14u);
        flags=game_memory_read_u32(memory,target+0x10u);
        if(flags&1u)attachment=target;
        game_memory_write_u32(memory,target+0x10u,flags|0x20000u);
    }else if(mode==2u){
        animation->arrival.identity=game_memory_read_u32(memory,target+0x14u);
    }
    radius=read_float(memory,target+0x38u);
    flags=game_memory_read_u32(memory,target+0x10u);
    coefficient=read_float(memory,mode==2u?0x5730d0u:(flags&0x180u)?0x5730c8u:0x5730ccu);
    initialization.scale[0]=(float)((long double)radius*coefficient);
    initialization.scale[1]=initialization.scale[0];initialization.radius=radius;
    /* 5511b0 consumes selector 5, fixes the angle at zero and forwards its
     * third argument as the attachment to 550e40. */
    initialization.identity=sequence;initialization.attachment=attachment;
    initialization.mark_kind=-1;
    initialization.profile=effect_initialization_profile(memory,5u);
    return !memory->fault && effect_initialize(memory,animation,pose,model,&initialization,&services->initialization);
}
uint32_t effect_initialization_random(GameMemory *memory,float scale){
    GameplaySubmissionSample sample=gameplay_state_prepare_submission_sample(memory,0);
    float value=read_float(memory,sample.scalar);
    gameplay_state_commit_submission_sample(memory,&sample);
    return 0u-presentation_lifecycle_phase(value,scale);
}
GameAddress effect_initialization_profile(GameMemory *memory,unsigned selector){
    uint32_t random;
    float scale=selector==0u?read_float(memory,0x57769cu):1.0f;
    if(selector>6u)return 0;
    random=effect_initialization_random(memory,scale);
    return gameplay_state_submission_table_value(memory,selector,random);
}

/* Preserve the x87 stored-float stages and unordered comparison behavior of
 * the original size-dependent world bounds. */
static float bounded_coordinate(float value,float radius,float multiplier,
        float upper_multiplier,float world_extent){
    long double lower=(long double)radius*multiplier;
    if(lower>value)return (float)lower;
    {
        long double upper=(long double)radius*upper_multiplier+world_extent;
        return (float)((long double)value>upper?upper:(long double)value);
    }
}

int effect_initialize(GameMemory *memory,EffectAnimation *animation,
        PresentationPose *pose,EffectModel *model,EffectInitialization *request,
        const EffectInitializationServices *services){
    uint32_t tick=game_memory_read_u32(memory,0x5ac6acu);
    unsigned axis;
    float multiplier,upper_multiplier,world_extent;
    animation->birth_tick=tick;
    animation->subtype=(animation->subtype&0xffffff00u)|request->subtype;
    animation->placement_flags=(uint16_t)((animation->placement_flags&0xff00u)|request->placement_flags);

    if(!effect_model_configure(memory,model,request->profile,request->scale[0],
        ((float_bits(request->scale[0])|float_bits(request->scale[1]))&0x7fffffffu)!=0u))return 0;
    if(effect_initialization_random(memory,read_float(memory,0x57769cu)))
        effect_quad_set_flags(&model->quad,0x80000000u,0,0,0);
    else effect_quad_clear_flags(&model->quad,0x80000000u);
    model->start_tick=game_memory_read_u32(memory,0x5ac6acu);
    pose->flags|=0x420u;
    animation->contact_armed=request->contact_armed;
    animation->loop_control=request->loop_control;

    pose->radius=(float_bits(request->radius)&0x7fffffffu)?request->radius:
        (float)((long double)model->quad.width*read_float(memory,0x5776a0u));
    for(axis=0;axis<3u;++axis)pose->dimensions[axis]=pose->radius;
    multiplier=read_float(memory,0x57769cu);
    upper_multiplier=read_float(memory,0x5776a4u);
    world_extent=read_float(memory,0x5776a8u);
    request->position[0]=bounded_coordinate(request->position[0],pose->radius,multiplier,upper_multiplier,world_extent);
    request->position[2]=bounded_coordinate(request->position[2],pose->radius,multiplier,upper_multiplier,world_extent);
    if(animation->placement_flags&1u){
        long double height;
        if(!services || !services->terrain_height ||
           !services->terrain_height(services->context,request->position[0],request->position[2],&height))return 0;
        request->position[1]=(float)(height+(long double)model->quad.height*read_float(memory,0x5776acu));
    }
    animation->emitter=request->emitter;
    animation->mark_delay=request->mark_delay;
    animation->mark_kind=request->mark_kind;
    animation->mark_radius=request->mark_radius;
    animation->emitted=0;animation->last_mark=0;

    if(request->attachment && (int32_t)game_memory_read_u32(memory,request->attachment+0x58u)>0){
        float delta[3];long double unrounded_z;
        animation->attachment=request->attachment;
        animation->attachment_identity=game_memory_read_u32(memory,request->attachment+0x14u);
        for(axis=0;axis<2u;++axis)delta[axis]=(float)((long double)request->position[axis]-
            read_float(memory,request->attachment+0x18u+axis*4u));
        unrounded_z=(long double)request->position[2]-read_float(memory,request->attachment+0x20u);
        delta[2]=(float)unrounded_z;
        for(axis=0;axis<3u;++axis){
            /* The first dot product retains Z in x87 after its float store;
             * subsequent columns reload the rounded value. */
            volatile long double z=(axis==0u?unrounded_z:(long double)delta[2])*read_float(memory,request->attachment+0xa0u+axis*4u);
            volatile long double y=(long double)delta[1]*read_float(memory,request->attachment+0x90u+axis*4u);
            volatile long double x=(long double)delta[0]*read_float(memory,request->attachment+0x80u+axis*4u);
            volatile long double sum=z+y;sum+=x;
            animation->attachment_offset[axis]=(float)sum;
        }
    }else animation->attachment=0;
    if(memory->fault || !services || !services->place)return 0;
    return services->place(services->context,request->identity,request->position,request->angle);
}
