#include "effect_animation.h"
#include <string.h>
static uint32_t word(EffectAnimation *s,unsigned o){
    uint32_t value=0;
    switch(o){
    case 0xb4:return s->birth_tick;case 0xb8:return s->contact_armed;case 0xbc:return s->loop_control;
    case 0xc0:return s->variant|((uint32_t)s->placement_flags<<16);
    case 0xc4:return s->attachment;case 0xc8:return s->attachment_identity;case 0xcc:return s->subtype;
    case 0xd0:case 0xd4:case 0xd8:memcpy(&value,&s->attachment_offset[(o-0xd0)/4],4);return value;
    case 0xdc:return (uint32_t)s->emitted;case 0xe0:return s->emitter;
    case 0xe4:return (uint16_t)s->mark_kind|((uint32_t)(uint16_t)s->mark_delay<<16);
    case 0xe8:return s->mark_radius;case 0xec:return (uint16_t)s->last_mark|((uint32_t)s->mark_padding<<16);
    case 0xf0:return s->arrival_status;case 0xf4:return s->arrival.target;case 0xf8:return s->arrival.identity;
    case 0xfc:return s->arrival_request;case 0x100:return s->arrival.mode;
    }return value;
}
static void set_word(EffectAnimation *s,unsigned o,uint32_t value){
    switch(o){
    case 0xb4:s->birth_tick=value;break;case 0xb8:s->contact_armed=value;break;case 0xbc:s->loop_control=value;break;
    case 0xc0:s->variant=(uint16_t)value;s->placement_flags=(uint16_t)(value>>16);break;
    case 0xc4:s->attachment=value;break;case 0xc8:s->attachment_identity=value;break;case 0xcc:s->subtype=value;break;
    case 0xd0:case 0xd4:case 0xd8:memcpy(&s->attachment_offset[(o-0xd0)/4],&value,4);break;
    case 0xdc:s->emitted=(int32_t)value;break;case 0xe0:s->emitter=value;break;
    case 0xe4:s->mark_kind=(int16_t)value;s->mark_delay=(int16_t)(value>>16);break;
    case 0xe8:s->mark_radius=value;break;case 0xec:s->last_mark=(int16_t)value;s->mark_padding=(uint16_t)(value>>16);break;
    case 0xf0:s->arrival_status=value;break;case 0xf4:s->arrival.target=value;break;case 0xf8:s->arrival.identity=value;break;
    case 0xfc:s->arrival_request=value;break;case 0x100:s->arrival.mode=value;break;
    }
}
static int transfer(EffectAnimation *s,uint32_t offset,void *data,size_t size,int writing){
    uint8_t *bytes=data;size_t i;
    if(!s || !data || offset<0xb4u || offset>0x104u || size>0x104u-offset)return 0;
    for(i=0;i<size;++i){unsigned o=(offset+(unsigned)i)&~3u,shift=((offset+(unsigned)i)&3u)*8u;uint32_t v=word(s,o);
        if(writing)set_word(s,o,(v&~(255u<<shift))|((uint32_t)bytes[i]<<shift));else bytes[i]=(uint8_t)(v>>shift);
    }return 1;
}
int effect_animation_projection_read(EffectAnimation *s,uint32_t o,void *b,size_t n){return transfer(s,o,b,n,0);}
int effect_animation_projection_write(EffectAnimation *s,uint32_t o,const void *b,size_t n){return transfer(s,o,(void *)b,n,1);}
int effect_animation_import(GameMemory *m,GameAddress a,EffectAnimation *s){
    unsigned o;for(o=0xb4;o<0x104;o+=4)set_word(s,o,game_memory_read_u32(m,a+o));return !m->fault;
}
void effect_animation_attached_position(const EffectAnimation *s,const float matrix[12],float position[3]){
    unsigned row;
    for(row=0;row<3;++row){
        volatile long double x=(long double)matrix[row*4]*s->attachment_offset[0];
        volatile long double y=(long double)matrix[row*4+1]*s->attachment_offset[1];
        volatile long double z=(long double)matrix[row*4+2]*s->attachment_offset[2];
        /* Y/Z partials are stored before the final X add in the original. */
        volatile long double sum=y+z;sum+=x;sum+=matrix[row*4+3];position[row]=(float)sum;
    }
}
static int mark_coverage(EffectAnimation *s,uint32_t elapsed,const EffectAnimationServices *p){
    if(s->mark_kind>=0 && elapsed-(uint32_t)(int32_t)s->mark_delay==(uint32_t)(int32_t)s->last_mark)
        return p->mark_coverage(p->context,s->mark_kind,s->mark_radius);
    return 1;
}
static int emit_until(EffectAnimation *s,int32_t goal,const EffectAnimationServices *p){
    while(s->emitted<goal){if(!p->emit(p->context))return 0;s->emitted=(int32_t)((uint32_t)s->emitted+1u);}return 1;
}
int effect_animation_update(EffectAnimation *s,PresentationPose *pose,uint32_t tick,const EffectAnimationServices *p){
    uint32_t elapsed;int32_t duration;int visible;
    if((int32_t)tick<(int32_t)s->birth_tick)return 1;
    elapsed=tick-s->birth_tick;
    if(!p->duration(p->context,&duration))return 0;
    if(duration<=(int32_t)elapsed){
        if(!mark_coverage(s,elapsed,p))return 0;
        if(s->emitter && !emit_until(s,16,p))return 0;
        return p->deactivate(p->context);
    }
    if(s->attachment){uint32_t identity;float matrix[12];
        if(!p->attachment(p->context,s->attachment,&identity,matrix))return 0;
        if(identity==s->attachment_identity){int inside;
            effect_animation_attached_position(s,matrix,pose->position);
            if(!p->place(p->context,&inside))return 0;
            if(!inside)return p->deactivate(p->context);
        }
    }
    if(s->birth_tick-p->tick(p->context)==UINT32_MAX && s->contact_armed){
        pose->flags&=~0x20u;if(!p->contacts(p->context))return 0;s->contact_armed=0;pose->flags|=0x20u;
    }
    if(s->mark_kind>=0 && elapsed-(uint32_t)(int32_t)s->mark_delay==(uint32_t)(int32_t)s->last_mark){
        if(!p->mark_coverage(p->context,s->mark_kind,s->mark_radius))return 0;
        s->last_mark=(int16_t)elapsed;
    }
    if(!s->emitter)return 1;
    if(!p->visible(p->context,&visible))return 0;
    if(visible){int32_t numerator=(int32_t)(elapsed<<4);
        if(!duration || (numerator==INT32_MIN && duration==-1))return 0;
        if(!emit_until(s,numerator/duration,p))return 0;
    }return 1;
}
