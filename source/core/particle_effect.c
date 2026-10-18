#include "particle_effect.h"
#include <string.h>
#include <math.h>
#include "gameplay_state.h"
static float read_float(GameMemory *m,GameAddress a){uint32_t bits=game_memory_read_u32(m,a);float f;memcpy(&f,&bits,4u);return f;}
static uint32_t word(ParticleEffect *s,unsigned offset){
    uint32_t bits;
    if(offset>=0xb8u && offset<=0xc0u){memcpy(&bits,&s->initial_position[(offset-0xb8u)/4u],4u);return bits;}
    if(offset==0xb4u)return s->birth_tick;
    if(offset==0xc4u)return (uint16_t)s->lifetime|((uint32_t)s->restarts<<16);
    return offset==0xc8u?s->recipe:0u;
}
static void set_word(ParticleEffect *s,unsigned offset,uint32_t bits){
    if(offset>=0xb8u && offset<=0xc0u){memcpy(&s->initial_position[(offset-0xb8u)/4u],&bits,4u);return;}
    if(offset==0xb4u)s->birth_tick=bits;
    else if(offset==0xc4u){s->lifetime=(int16_t)bits;s->restarts=(uint16_t)(bits>>16);}
    else if(offset==0xc8u)s->recipe=bits;
}
static int transfer(ParticleEffect *s,uint32_t offset,void *data,size_t size,int writing){
    uint8_t *bytes=data;size_t i;
    if(!s || !data || offset<0xb4u || offset>0xccu || size>0xccu-offset)return 0;
    for(i=0;i<size;++i){unsigned o=(offset+(unsigned)i)&~3u,shift=((offset+(unsigned)i)&3u)*8u;uint32_t v=word(s,o);
        if(writing)set_word(s,o,(v&~(255u<<shift))|((uint32_t)bytes[i]<<shift));else bytes[i]=(uint8_t)(v>>shift);
    }return 1;
}
int particle_effect_projection_read(ParticleEffect *s,uint32_t o,void *b,size_t n){return transfer(s,o,b,n,0);}
int particle_effect_projection_write(ParticleEffect *s,uint32_t o,const void *b,size_t n){return transfer(s,o,(void *)b,n,1);}
int particle_effect_import(GameMemory *m,GameAddress object,ParticleEffect *s){
    unsigned o;for(o=0xb4u;o<0xccu;o+=4u)set_word(s,o,game_memory_read_u32(m,object+o));return !m->fault;
}
int particle_effect_initialize(GameMemory *m,ParticleEffect *s,PresentationPose *pose,EffectModel *model,
        uint32_t identity,const float position[3],GameAddress recipe,const ParticleEffectServices *services){
    int32_t kind=(int16_t)game_memory_read_u16(m,recipe),size=(int16_t)game_memory_read_u16(m,recipe+12u);
    int alternate=game_memory_read_u32(m,0x6297b8u)==2u && kind==0;
    GameAddress profile=game_memory_read_u32(m,alternate?0x62976cu:0x629768u+(uint32_t)kind*4u);
    int32_t minimum,maximum;float dimension;
    if(!effect_model_configure(m,model,profile,(float)size,1))return 0;
    if(alternate)effect_quad_set_flags(&model->quad,0x800u,0,0,0);
    else effect_quad_clear_flags(&model->quad,0x800u);
    model->start_tick=game_memory_read_u32(m,0x5ac6acu);
    minimum=(int16_t)game_memory_read_u16(m,recipe+2u);maximum=(int16_t)game_memory_read_u16(m,recipe+4u);
    s->lifetime=maximum?(int16_t)((uint32_t)minimum+effect_initialization_random(m,(float)(maximum-minimum))):(int16_t)minimum;
    s->birth_tick=game_memory_read_u32(m,0x5ac6acu);s->restarts=game_memory_read_u16(m,recipe+6u);s->recipe=recipe;
    dimension=(float)((long double)read_float(m,profile+24u)*read_float(m,0x57668cu));
    pose->dimensions[0]=pose->dimensions[1]=pose->dimensions[2]=dimension;
    if(m->fault || !services || !services->place)return 0;
    return services->place(services->context,identity,position);
}
int particle_effect_update(GameMemory *m,ParticleEffect *s,PresentationPose *pose,EffectModel *model,
        uint32_t tick,const ParticleEffectServices *services){
    int inside;
    if(tick&1u)return 1;
    if((int32_t)(s->birth_tick+(uint32_t)(int32_t)s->lifetime)<=(int32_t)tick){
        if(!s->restarts)return services->deactivate(services->context);
        memcpy(pose->position,s->initial_position,sizeof(pose->position));--s->restarts;s->birth_tick=tick;
    }else{
        int32_t vertical=(int16_t)game_memory_read_u16(m,s->recipe+10u);
        int32_t horizontal=(int16_t)game_memory_read_u16(m,s->recipe+8u);
        if(vertical>0){
            int32_t step=(int32_t)effect_initialization_random(m,(float)vertical);
            pose->position[1]=(float)((long double)pose->position[1]+step);
        }
        if(horizontal>0){
            int32_t x=(int32_t)(effect_initialization_random(m,(float)horizontal)-(uint32_t)(horizontal/2));
            int32_t z;
            pose->position[0]=(float)((long double)pose->position[0]+x);
            z=(int32_t)(effect_initialization_random(m,(float)horizontal)-(uint32_t)(horizontal/2));
            pose->position[2]=(float)((long double)pose->position[2]+z);
        }
        model->quad.width=(float)((long double)model->quad.width+read_float(m,s->recipe+16u));
        model->quad.height=(float)((long double)model->quad.height+read_float(m,s->recipe+20u));
    }
    if(m->fault || !services->inside(services->context,&inside))return 0;
    return inside?services->reinsert(services->context):services->deactivate(services->context);
}

static int emission_bounds(GameMemory *m,float radius,const float position[3],GameAddress constants,unsigned upper_offset){
    uint32_t y;long double lower,upper;float stored;
    memcpy(&y,position+1,4u);
    if((int32_t)y>(int32_t)0x46ea6000u || y>0xc6ea6000u)return 0;
    lower=(long double)radius*read_float(m,constants);stored=(float)lower;
    if(lower>position[0] || !(position[2]>=stored))return 0;
    upper=(long double)radius*read_float(m,constants+upper_offset)+read_float(m,constants+upper_offset+4u);stored=(float)upper;
    return upper>=position[0] && !(position[2]>stored) && !m->fault;
}

int particle_effect_emission_allowed(GameMemory *m,float radius,const float position[3]){
    return emission_bounds(m,radius,position,0x57769cu,8u);
}
int particle_effect_surface_allowed(GameMemory *m,float radius,const float position[3],int32_t chance){
    GameplaySubmissionSample sample;long double product;int64_t value;
    if(!emission_bounds(m,radius,position,0x57217cu,4u))return 0;
    /* Surface constants are contiguous, unlike the shared effect constants. */
    if(!chance)return 1;
    sample=gameplay_state_prepare_submission_sample(m,0u);
    product=(long double)read_float(m,sample.scalar)*chance;
    gameplay_state_commit_submission_sample(m,&sample);
    value=!isfinite(product) || product < -0x1p63L || product >= 0x1p63L?INT64_MIN:(int64_t)product;
    return (uint32_t)(uint64_t)value==0u && !m->fault;
}
