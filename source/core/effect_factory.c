#include "effect_factory.h"
#include "effect_model.h"
#include "trail_model.h"
#include "mesh_buffer.h"
#include "attachment_effect.h"
#include "scene_projection_storage.h"
#include "game_initializers.h"
#include <string.h>
static const EffectFamily families[]={
    {5u,0x5adce4u,0x624f48u,0x572ed0u,0x43d890u,0xc10u,0xe8u,0u},
    {10u,0x5adb18u,0x624f48u,0x577980u,0u,0x800u,0x124u,0u},
    {10u,0x5adb1cu,0x624f48u,0x577980u,0u,0x800u,0x124u,1u},
    {10u,0x5adb20u,0x624f48u,0x577980u,0u,0x800u,0x124u,2u},
    {0x41u,0x5adcf4u,0x639ac0u,0x5775f0u,0x551530u,0xc10u,0xf0u,1u},
    {0x42u,0x5adc74u,0x5961d8u,0x574a60u,0u,0xc20u,0xd0u,0u},
    {0x43u,0x5adcecu,0x629778u,0x576690u,0x5379d0u,0xc30u,0xccu,0u},
    {0x44u,0x5adaf0u,0x6020e0u,0x5741e0u,0x49aae0u,0xc30u,0xccu,1u},
    {0x45u,0x5adc0cu,0x639ac0u,0x573028u,0u,0xc10u,0x104u,1u}
};
const EffectFamily *effect_factory_family(unsigned kind){
    unsigned i;for(i=0;i<sizeof(families)/sizeof(families[0]);++i)if(families[i].kind==kind)return &families[i];return NULL;
}
const EffectFamily *effect_factory_family_slot(unsigned kind,uint16_t slot){
    unsigned i;if(kind!=10u)return effect_factory_family(kind);
    if(slot>=3u)return NULL;
    for(i=0;i<sizeof(families)/sizeof(families[0]);i++)
        if(families[i].kind==10u && families[i].model_mode==slot)return &families[i];
    return NULL;
}
static int release_model(GameMemory *memory,const EffectFamily *family,GameAddress model){
    if(family->kind==10u)return mesh_buffer_release(memory,model,mesh_buffer_owned_services()) &&
        scene_projection_storage_free(model);
    return family->kind==0x42u?trail_model_release(model):effect_model_release(model);
}
static GameAddress create_model(GameMemory *memory,const EffectFamily *family){
    GameAddress model;int16_t vertices,triangles;
    if(family->kind!=10u)return family->kind==0x42u?trail_model_create(300u):effect_model_create(family->model_mode);
    vertices=(int16_t)game_memory_read_u32(memory,0x5a0488u+family->model_mode*8u);
    triangles=(int16_t)game_memory_read_u32(memory,0x5a048cu+family->model_mode*8u);
    if(memory->fault)return 0;
    model=scene_projection_storage_allocate(0x64u,"attachment model holder");if(!model)return 0;
    if(!mesh_buffer_construct(memory,model,vertices,triangles,0x59d810u,0x59d818u,mesh_buffer_owned_services())){
        scene_projection_storage_free(model);return 0;
    }
    return model;
}
static int finish(GameMemory *m,GameAddress object,const EffectFamily *f){
    PresentationPose *pose=presentation_instance_pose(object);
    if(!pose)return 0;
    game_initializer_linked_object(m,object,f->methods,f->kind,pose->flags|f->flags,
        0u,0u,(f->kind==0x41u || f->kind==0x45u)?0xccu:0u,0u);
    if(m->fault)return 0;
    if(f->kind==10u)return attachment_effect_initialize(m,object);
    if(f->kind==5u){
        game_memory_write_u32(m,object+0xd4u,0u);game_memory_write_u32(m,object+0xdcu,0u);
        game_memory_write_u8(m,object+0xe0u,0u);
        {EffectModel *model=effect_model_find(pose->model);if(!model)return 0;model->parent=object;}
        return !m->fault && presentation_instance_enable_fragment(m,object);
    }
    if(f->kind==0x41u)return presentation_instance_enable_animation(m,object,0);
    if(f->kind==0x45u){
        game_initializer_type45_runtime(m,object);
        return !m->fault && presentation_instance_enable_animation(m,object,1);
    }
    if(f->kind==0x43u)return presentation_instance_enable_particle(m,object);
    if(f->kind==0x42u)return 1;
    return presentation_instance_enable_expansion(m,object);
}
GameAddress effect_factory_template(GameMemory *m,const EffectFamily *f,const EffectFactoryServices *s){
    GameAddress object=game_memory_read_u32(m,f->publication),model;PresentationInstanceDefinition d={0};unsigned i;
    if(m->fault || object)return object;
    if(game_memory_read_u32(m,0x624f8cu)==10u || m->fault)return 0;
    if(f->load_assets && !s->load_assets(s->context,f->load_assets))return 0;
    model=create_model(m,f);if(!model)return 0;
    object=s->allocate(s->context,f->size);
    if(!object){release_model(m,f,model);return 0;}
    for(i=0;i<3u;++i){uint32_t v=game_memory_read_u32(m,f->dimensions+i*4u);memcpy(&d.dimensions[i],&v,4u);}
    d.model=model;
    if(f->kind==5u || f->kind==10u)d.radius=0x3f800000u;
    if(!presentation_instance_create(m,object,&d,0) || !finish(m,object,f)){
        if(!presentation_instance_find(object))release_model(m,f,model);
        s->destroy(s->context,object);return 0;
    }
    game_memory_write_u32(m,f->publication,object);return m->fault?0:object;
}
int effect_factory_fill(GameMemory *m,const EffectFamily *f,int32_t count,const EffectFactoryServices *s){
    GameAddress source=effect_factory_template(m,f,s);PresentationLifecycle *owner;
    if(!source || m->fault || !(owner=presentation_instance_lifecycle(source)))return 0;
    while(count > 0){
        GameAddress object=s->allocate(s->context,f->size);PresentationLifecycle *instance,*first;
        if(!object)return 0;
        if(!presentation_instance_create_copy(m,object,source,0,NULL) || !finish(m,object,f)){
            s->destroy(s->context,object);return 0;
        }
        instance=presentation_instance_lifecycle(object);
        instance->membership.next=source;instance->membership.previous=owner->membership.previous;
        if(instance->membership.previous){
            first=presentation_instance_lifecycle(instance->membership.previous);if(!first)return 0;
            first->membership.next=object;
        }
        owner->membership.previous=object;
        --count;
    }
    return !m->fault;
}
GameAddress effect_factory_take(GameMemory *m,const EffectFamily *f,const EffectFactoryServices *s){
    GameAddress source=effect_factory_template(m,f,s),object;PresentationLifecycle *owner,*instance,*next,*previous;
    if(!source || m->fault || !(owner=presentation_instance_lifecycle(source)))return 0;
    if(!owner->membership.previous && !effect_factory_fill(m,f,1,s))return 0;
    object=owner->membership.previous;instance=presentation_instance_lifecycle(object);if(!instance)return 0;
    next=presentation_instance_lifecycle(instance->membership.next);
    previous=instance->membership.previous?presentation_instance_lifecycle(instance->membership.previous):NULL;
    if(!next || (instance->membership.previous && !previous))return 0;
    next->membership.previous=instance->membership.previous;
    if(previous)previous->membership.next=instance->membership.next;
    instance->membership.previous=instance->membership.next=0u;return object;
}
int effect_factory_shutdown(GameMemory *m,const EffectFamily *f,const EffectFactoryServices *s){
    GameAddress object=game_memory_read_u32(m,f->publication);PresentationLifecycle *owner;
    if(m->fault)return 0;
    if(!object)return 1;
    owner=presentation_instance_lifecycle(object);if(!owner)return 0;
    while(owner->membership.previous)if(!s->destroy(s->context,owner->membership.previous))return 0;
    if(!s->destroy(s->context,object))return 0;
    game_memory_write_u32(m,f->publication,0u);return !m->fault;
}
