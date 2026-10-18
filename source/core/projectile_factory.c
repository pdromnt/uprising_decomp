#include "projectile_factory.h"
#include "game_initializers.h"
#include "game_intrusive_list.h"
#include "mesh_buffer.h"
#include "scene_projection_storage.h"
#include <string.h>

/* Neighboring entries in the verified name/publication tables delimit these
 * configured families. Other kinds/slots retain their explicit producer port. */
static const ProjectileFamily families[]={
    {2u,4u,0x5ada60u,0x584688u,0x572548u,0xe4u},
    {3u,1u,0x5ada20u,0x5846c4u,0x5752e0u,0x114u},
    {6u,2u,0x5adda8u,0x584680u,0x572608u,0x118u},
    {7u,1u,0x5adb50u,0x5846b0u,0x5726c8u,0x120u},
    {8u,1u,0x5adaf8u,0x5846b4u,0x572788u,0x128u},
    {0x0cu,1u,0x5adcfcu,0x5846a4u,0x573fb8u,0x10cu},
    {0x0du,1u,0x5adaf4u,0x5846bcu,0x572a88u,0xe8u},
    {0x0fu,1u,0x5adafcu,0x5846c8u,0x575138u,0x114u},
    {0x11u,1u,0x5adbf4u,0x5846ccu,0x577220u,0xecu},
    {0x12u,1u,0x5adcdcu,0x5846d0u,0x577310u,0xfcu},
    {0x13u,1u,0x5adce0u,0x584698u,0x5723c8u,0xecu},
    {0x14u,1u,0x5adb40u,0x5846b8u,0x572848u,0x11cu},
    {0x15u,1u,0x5add80u,0x58469cu,0x572908u,0x118u},
    {0x16u,1u,0x5adcd8u,0x5846a0u,0x5729c8u,0xf0u}
};
const ProjectileFamily *projectile_factory_family(unsigned kind){
    unsigned i;
    for(i=0;i<sizeof(families)/sizeof(families[0]);++i)
        if(families[i].kind==kind)return families+i;
    return NULL;
}
static int finish(GameMemory *memory,GameAddress object,const ProjectileFamily *family){
    Projectile *state;
    game_initializer_object_identity(memory,object,family->methods,family->kind,1);
    if(memory->fault || !presentation_instance_enable_projectile(memory,object))return 0;
    state=presentation_instance_projectile(object);
    memset(state->velocity,0,sizeof(state->velocity));
    memset(state->angular_velocity,0,3u*sizeof(uint16_t));
    if(family->kind==0x0cu){GameAddress source=game_memory_read_u32(memory,object+0x50u),mesh=0u;unsigned i;
        /*48db10/48da30: the template has no mutable mesh. Each actual copy
         *owns its64h holder and buffers; the hump model is a shared source. */
        for(i=0xe4u;i<=0xecu;i+=4u)game_memory_write_u32(memory,object+i,0u);
        game_memory_write_u32(memory,object+0xfcu,0u);
        if(source){
            mesh=scene_projection_storage_allocate(0x64u,"mole mutable mesh holder");if(!mesh)return 0;
            if(!mesh_buffer_construct(memory,mesh,(int16_t)game_memory_read_u32(memory,0x5f06a4u),
               (int16_t)game_memory_read_u32(memory,0x5f06a0u),0x59d810u,0x59d818u,mesh_buffer_owned_services())){
                scene_projection_storage_free(mesh);return 0;}
            game_memory_write_u32(memory,object+0xfcu,mesh);
            game_initializer_00413690(memory,mesh,game_memory_read_u32(memory,0x5f0210u)+0x10u);
        }
    }
    return 1;
}
GameAddress projectile_factory_template(GameMemory *memory,const ProjectileFamily *family,
        uint16_t slot,const ProjectileFactoryServices *services){
    GameAddress publication,object;PresentationInstanceDefinition definition={0};
    if(!family || slot>=family->slots || !services)return 0;
    publication=family->publication+(uint32_t)slot*4u;
    object=game_memory_read_u32(memory,publication);
    if(object || memory->fault)return object;
    if(!services->describe(services->context,family,slot,&definition))return 0;
    definition.type=family->kind;definition.flags=family->kind==0x11u?0x803u:
        (family->kind==0x0du || family->kind==0x12u || family->kind==0x14u)?0xc07u:0xc06u;
    object=services->allocate(services->context,family->size);
    if(!object)return 0;
    if(!presentation_instance_create(memory,object,&definition,0) || !finish(memory,object,family)){
        services->destroy(services->context,object);return 0;
    }
    game_memory_write_u32(memory,publication,object);
    return memory->fault?0u:object;
}
int projectile_factory_fill(GameMemory *memory,const ProjectileFamily *family,uint16_t slot,
        int32_t count,const ProjectileFactoryServices *services){
    GameAddress source=projectile_factory_template(memory,family,slot,services);
    PresentationConstructionServices construction={services->context,services->clone_model};
    if(!source || memory->fault)return 0;
    while(count>0){
        GameAddress object=services->allocate(services->context,family->size);
        if(!object)return 0;
        if(!presentation_instance_create_copy(memory,object,source,0,&construction) ||
           !finish(memory,object,family)){
            services->destroy(services->context,object);return 0;
        }
        game_list_insert_after(memory,object,source,4u,8u);
        if(memory->fault)return 0;
        --count;
    }
    return 1;
}
GameAddress projectile_factory_take(GameMemory *memory,const ProjectileFamily *family,
        uint16_t slot,const ProjectileFactoryServices *services){
    GameAddress source=projectile_factory_template(memory,family,slot,services),object;
    if(!source || memory->fault)return 0;
    object=game_memory_read_u32(memory,source+4u);
    if(!object){
        if(!projectile_factory_fill(memory,family,slot,1,services))return 0;
        object=game_memory_read_u32(memory,source+4u);
    }
    game_list_detach_node(memory,object,4u,8u);
    game_memory_write_u32(memory,object+4u,0u);
    game_memory_write_u32(memory,object+8u,0u);
    return memory->fault?0u:object;
}
int projectile_split(GameMemory *memory,GameAddress source,Projectile *state,
        PresentationPose *pose,const ProjectileSplitServices *services){
    GameAddress children[2]={0,0},manager;Projectile *child[2];uint32_t identity,tick;
    unsigned i,published=0;uint16_t slot;float speed,positive,negative;uint32_t bits;
    void *context=services->factory.context;
    (void)source;
    if(state->split){
        projectile_move(state,pose,1.0f);
        return presentation_pose_build_world(memory,pose) &&
            presentation_pose_transform_vertices(memory,pose,0);
    }
    manager=game_memory_read_u32(memory,0x5f0210u+(pose->identity&63u)*4u);
    slot=(uint16_t)game_memory_read_u32(memory,state->recipe+16u);
    if(memory->fault)return 0;
    for(i=0;i<2u;++i){
        children[i]=projectile_factory_take(memory,projectile_factory_family(7u),slot,&services->factory);
        if(!children[i])goto failed;
    }
    for(i=0;i<2u;++i){
        if(!services->sequence(context,manager,&identity) ||
           !services->launch(context,children[i],identity,pose,state->recipe,state->shooter))goto failed;
        child[i]=presentation_instance_projectile(children[i]);
        if(!child[i])goto failed;
    }
    for(i=0;i<2u;++i){
        if(!services->publish(context,manager,children[i]))goto failed;
        ++published;
    }
    tick=game_memory_read_u32(memory,0x5ac6acu);
    state->birth_tick=child[0]->birth_tick=child[1]->birth_tick=tick;
    state->split=child[0]->split=child[1]->split=1u;
    bits=game_memory_read_u32(memory,state->recipe+0x1cu);memcpy(&speed,&bits,4u);
    bits=game_memory_read_u32(memory,0x57235cu);memcpy(&positive,&bits,4u);
    bits=game_memory_read_u32(memory,0x572360u);memcpy(&negative,&bits,4u);
    for(i=0;i<2u;++i){
        child[i]->velocity[0]=state->velocity[0];child[i]->velocity[2]=state->velocity[2];
        child[i]->velocity[1]=(float)((long double)speed*(i?negative:positive)+state->velocity[1]);
    }
    return !memory->fault;
failed:
    for(i=published;i<2u;++i)if(children[i])services->factory.destroy(context,children[i]);
    return 0;
}
