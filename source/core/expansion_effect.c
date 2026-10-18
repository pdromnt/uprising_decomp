#include "expansion_effect.h"
#include <string.h>
static float read_float(GameMemory *m,GameAddress a){uint32_t v=game_memory_read_u32(m,a);float f;memcpy(&f,&v,4u);return f;}
static uint32_t word(ExpansionEffect *s,unsigned offset){
    uint32_t v;
    if(offset==0xb4u)return s->phase;
    if(offset==0xb8u)return s->step;
    if(offset==0xc8u)return (uint32_t)s->duration;
    if(offset==0xbcu)memcpy(&v,&s->width,4u);
    else if(offset==0xc0u)memcpy(&v,&s->height,4u);
    else memcpy(&v,&s->top,4u);
    return v;
}
static void set_word(ExpansionEffect *s,unsigned offset,uint32_t v){
    if(offset==0xb4u)s->phase=v;
    else if(offset==0xb8u)s->step=v;
    else if(offset==0xc8u)s->duration=(int32_t)v;
    else if(offset==0xbcu)memcpy(&s->width,&v,4u);
    else if(offset==0xc0u)memcpy(&s->height,&v,4u);
    else memcpy(&s->top,&v,4u);
}
static int transfer(ExpansionEffect *s,uint32_t offset,void *data,size_t size,int writing){
    uint8_t *bytes=data;size_t i;
    if(!s || !data || offset<0xb4u || offset>0xccu || size>0xccu-offset)return 0;
    for(i=0;i<size;++i){unsigned o=(offset+(unsigned)i)&~3u,shift=((offset+(unsigned)i)&3u)*8u;uint32_t v=word(s,o);
        if(writing)set_word(s,o,(v&~(255u<<shift))|((uint32_t)bytes[i]<<shift));else bytes[i]=(uint8_t)(v>>shift);
    }return 1;
}
int expansion_effect_projection_read(ExpansionEffect *s,uint32_t o,void *b,size_t n){return transfer(s,o,b,n,0);}
int expansion_effect_projection_write(ExpansionEffect *s,uint32_t o,const void *b,size_t n){return transfer(s,o,(void *)b,n,1);}
int expansion_effect_import(GameMemory *m,GameAddress object,ExpansionEffect *s){
    unsigned o;for(o=0xb4u;o<0xccu;o+=4u)set_word(s,o,game_memory_read_u32(m,object+o));return !m->fault;
}
void expansion_effect_shrink(ExpansionEffect *s){s->phase=2u;s->step=20u;}
void expansion_effect_remove(ExpansionEffect *s){s->phase=3u;}
int expansion_effect_initialize(GameMemory *m,ExpansionEffect *s,PresentationPose *pose,EffectModel *model,
        uint32_t identity,const float position[3],float width,float height,int32_t duration,const ExpansionEffectServices *services){
    GameAddress profile=game_memory_read_u32(m,0x6020d8u);(void)pose;
    if(!effect_model_configure(m,model,profile,0.0f,0))return 0;
    model->start_tick=game_memory_read_u32(m,0x5ac6acu);
    if(m->fault || !services->place(services->context,identity,position))return 0;
    model->quad.width=model->quad.height=0.0f;
    s->phase=0u;s->step=1u;s->width=width;s->height=height;s->duration=duration;
    s->top=(float)((long double)height*read_float(m,0x57427cu)+position[1]);return !m->fault;
}
int expansion_effect_update(GameMemory *m,ExpansionEffect *s,PresentationPose *pose,EffectModel *model,const ExpansionEffectServices *services){
    float height;long double factor;
    if(s->phase==3u)return services->deactivate(services->context);
    if(s->phase>3u)return !m->fault;
    if(s->phase==1u){
        if(s->duration!=-1){s->duration=(int32_t)((uint32_t)s->duration-1u);if(s->duration==20)expansion_effect_shrink(s);}
        return !m->fault;
    }
    if(s->phase==2u && !s->step)return services->deactivate(services->context);
    if(s->phase==0u && s->step==20u){
        s->phase=1u;model->quad.width=s->width;model->quad.height=s->height;
        if(m->fault || !services->detach(services->context))return 0;
        pose->position[1]=s->top;
        if(!services->insert(services->context))return 0;
        if(s->duration!=-1)s->duration=(int32_t)((uint32_t)s->duration-20u);
        height=(float)((long double)s->height*read_float(m,0x57427cu));
    }else{
        factor=(long double)(int32_t)(s->step+1u)*read_float(m,0x574280u);
        height=(float)((long double)s->height*factor);model->quad.width=(float)(factor*s->width);model->quad.height=height;
        if(m->fault || !services->detach(services->context))return 0;
        pose->position[1]=(float)((long double)s->top-((long double)s->height-height)*read_float(m,0x57427cu));
        if(!services->insert(services->context))return 0;
        if(s->phase==0u)++s->step;else --s->step;
        height=(float)((long double)height*read_float(m,0x57427cu));
    }
    pose->dimensions[0]=pose->dimensions[1]=pose->dimensions[2]=height;return !m->fault;
}
