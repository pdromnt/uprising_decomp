#include "player_events.h"
#include "player_scene_children.h"
#include <math.h>
#include <limits.h>
#include <string.h>

static void *segment(PlayerControls *s, uint32_t offset, uint32_t *begin, size_t *length) {
#define FIELD(at, member) do { if (offset - (at) < sizeof(s->member)) { *begin=(at); *length=sizeof(s->member); return &s->member; } } while (0)
    FIELD(0x13cu,queue_methods); FIELD(0x140u,input_flags);
    FIELD(0x144u,previous_input_flags); FIELD(0x14au,input_axes);
    FIELD(0x164u,event_head); FIELD(0x168u,event_tail);
    FIELD(0x1ccu,selected); FIELD(0x1d4u,weapons); FIELD(0x214u,selection);
    FIELD(0x21cu,feedback); FIELD(0x224u,aim); FIELD(0x226u,previous_aim);
    FIELD(0x26cu,heading); FIELD(0x26eu,previous_heading);
    FIELD(0x2dcu,vertical_scale); FIELD(0x2e0u,height_scale);
    FIELD(0x2e4u,remaining_scale); FIELD(0x2f0u,last_packet_tick);
    FIELD(0x2f4u,mode_flags);
#undef FIELD
    return NULL;
}
static int transfer(PlayerControls *s,uint32_t offset,void *bytes,size_t size,int writing) {
    uint32_t check=offset;size_t remaining=size;uint8_t *cursor=bytes;
    if(!s || !bytes)return 0;
    while(remaining){uint32_t begin;size_t length,step;
        if(!segment(s,check,&begin,&length))return 0;
        step=length-(check-begin);if(step>remaining)step=remaining;
        check+=(uint32_t)step;remaining-=step;
    }
    while(size){uint32_t begin;size_t length,step;uint8_t *field=segment(s,offset,&begin,&length);
        step=length-(offset-begin);if(step>size)step=size;
        if(writing)memcpy(field+offset-begin,cursor,step);else memcpy(cursor,field+offset-begin,step);
        offset+=(uint32_t)step;cursor+=step;size-=step;
    }return 1;
}
int player_controls_projection_read(PlayerControls *s,uint32_t offset,void *out,size_t size){return transfer(s,offset,out,size,0);}
int player_controls_projection_write(PlayerControls *s,uint32_t offset,const void *in,size_t size){return transfer(s,offset,(void *)in,size,1);}
int player_controls_import(GameMemory *m,GameAddress scene,PlayerControls *s){
    uint32_t offset;memset(s,0,sizeof(*s));
    for(offset=0x13cu;offset<0x2f8u;){uint32_t begin;size_t length;void *field=segment(s,offset,&begin,&length);
        if(!field){++offset;continue;}
        {size_t done=0;while(done<length){size_t step=length-done;if(step>4u)step=4u;
            if(!game_memory_read(m,scene+offset+(uint32_t)done,(uint8_t *)field+done,step))return 0;
            done+=step;}}
        offset+=(uint32_t)length;
    }return !m->fault;
}
static uint32_t float_bits(float value){uint32_t bits;memcpy(&bits,&value,4);return bits;}
static float read_float(GameMemory *m,GameAddress at){uint32_t bits=game_memory_read_u32(m,at);float value;memcpy(&value,&bits,4);return value;}
static uint32_t truncate_word(long double value){
    if(!isfinite(value) || value<(long double)INT64_MIN || value>=(long double)INT64_MAX)return 0x80000000u;
    return (uint32_t)(int64_t)value;
}
static GameAddress weapon_at(GameMemory *m,GameAddress scene,PlayerControls *s,uint32_t index){
    /* Retail direct-selection packets contain a byte, without a bounds check.
     * Keep malformed/foreign slot access at the projection boundary. */
    return index<15u?s->weapons[index]:game_memory_read_u32(m,scene+0x1d4u+index*4u);
}
static uint32_t quantity(GameMemory *m,GameAddress child){
    PlayerAttachmentState *s=player_scene_children_attachment(child);
    return s?s->quantity:game_memory_read_u32(m,child+12u);
}
static void detach_feedback(GameMemory *m,PlayerControls *s,unsigned which){
    if(s->feedback[which]){game_memory_write_u32(m,s->feedback[which]+0xe4u,0u);s->feedback[which]=0u;}
}
static void detach_weapon(GameMemory *m,PlayerControls *s,unsigned which){
    PlayerAttachmentState *child=player_scene_children_attachment(s->selected[which]);
    GameAddress motion=child?child->motion:game_memory_read_u32(m,s->selected[which]+20u);
    unsigned kind=game_memory_read_u8(m,motion+12u);
    if(kind==3u)detach_feedback(m,s,0u);
    else if(kind==15u)detach_feedback(m,s,1u);
}
static void stop_weapon(GameMemory *m,PlayerControls *s,unsigned which){
    PlayerAttachmentState *child;
    s->input_flags&=~(0x10000u<<which);detach_weapon(m,s,which);
    child=player_scene_children_attachment(s->selected[which]);
    if(child)child->stream_cookie=0u;else game_memory_write_u32(m,s->selected[which]+24u,0u);
}
static void select_weapon(GameMemory *m,GameAddress scene,PlayerControls *s,unsigned which,unsigned direction,unsigned requested){
    int32_t index;GameAddress child;detach_weapon(m,s,which);
    if(direction==0u){index=(int32_t)requested;child=weapon_at(m,scene,s,(uint32_t)index);
        if(!child || !quantity(m,child))index=0;
    }else if(direction==1u){
        index=(int32_t)(s->selection[which]+1u);
        while(index<15){child=weapon_at(m,scene,s,(uint32_t)index);
            if(child && (int32_t)quantity(m,child)>0)break;
            index=(int32_t)((uint32_t)index+1u);
        }index%=15;
    }else{index=(int32_t)(s->selection[which]+14u)%15;
        while(index>0){child=weapon_at(m,scene,s,(uint32_t)index);
            if(child && (int32_t)quantity(m,child)>0)break;
            --index;
        }
    }
    s->selection[which]=(uint32_t)index;s->selected[which]=weapon_at(m,scene,s,(uint32_t)index);
}
int player_packet_apply(GameMemory *m,GameAddress scene,PresentationPose *pose,PlayerControls *s,
        const EffectResource *event,const PlayerEventServices *ports){
    unsigned mask=event->packet_mask,index=0,bit;int alive=0;
    s->last_packet_tick=game_memory_read_u32(m,0x005ac6acu);
    if(!ports->spatial || !ports->spatial(ports->context,0u,&alive))return 0;
    for(bit=0;bit<8u;++bit)if(mask&(1u<<bit)){
        uint16_t word=event->packet_data[index++];
        if(bit==0u)pose->position[0]=(float)((uint32_t)word*2u);
        else if(bit==1u)pose->position[1]=(float)(int16_t)word;
        else if(bit==2u)pose->position[2]=(float)((uint32_t)word*2u);
        else if(bit<6u)pose->angles[bit-3u]=word;
        else if(bit==6u)s->heading=word;else s->aim=word;
    }
    if(m->fault || !ports->spatial(ports->context,1u,&alive))return 0;
    if(alive){if(!ports->spatial(ports->context,2u,&alive))return 0;
        /*4f20b6 skips the history update offline. Network reconciliation
         *samples these channels on later ticks; leaving them at startup
         *would overwrite each correctly delivered movement packet. */
        if(game_memory_read_u32(m,0x005eff04u) && (!ports->refresh || !ports->refresh(ports->context,3u)))return 0;
    }
    (void)scene;return !m->fault;
}
static int rescale(GameMemory *m,GameAddress scene,PresentationPose *pose,PlayerControls *s,
        const EffectResource *event,const PlayerEventServices *ports){
    int32_t height,top,bottom,base;float next_height,next_vertical;
    GameAddress world,motion;long double upper,lower,health;
    float clearance;uint32_t bits=event->event_arguments[0];memcpy(&next_height,&bits,4);
    bits=event->event_arguments[1];memcpy(&next_vertical,&bits,4);
    if(float_bits(s->height_scale)!=0x3f800000u && float_bits(next_height)==0x3f800000u)s->mode_flags|=8u;
    else if(float_bits(s->height_scale)==0x3f800000u && float_bits(next_height)!=0x3f800000u)s->mode_flags&=~8u;
    s->height_scale=next_height;s->vertical_scale=next_vertical;
    s->remaining_scale=(float)((long double)read_float(m,0x00574d3cu)-((long double)next_vertical+next_height));
    if(!ports->catalog || !ports->catalog(ports->context,0u,5u,0u,&height) ||
       !ports->catalog(ports->context,1u,2u,1u,&top) || !ports->catalog(ports->context,1u,2u,0u,&bottom) ||
       !ports->catalog(ports->context,0u,1u,0u,&base))return 0;
    if(float_bits(s->vertical_scale)==0x3f800000u){
        clearance=(float)((float)height*(long double)read_float(m,0x00574d40u));
        upper=(long double)top*read_float(m,0x00574d44u);lower=0;
        health=(long double)base*read_float(m,0x00574d48u);
    }else if(float_bits(s->remaining_scale)==0x3f800000u){
        clearance=(float)((float)height*(long double)read_float(m,0x00574d4cu));
        upper=(long double)top*read_float(m,0x00574d50u);lower=(long double)bottom*read_float(m,0x00574d34u);
        health=-(long double)base;
    }else if(float_bits(s->height_scale)==0x3f800000u){
        clearance=(float)((float)height*(long double)read_float(m,0x00574d54u));
        upper=(long double)top*read_float(m,0x00574d44u);lower=0;health=0;
    }else{
        clearance=(float)(((long double)s->vertical_scale*read_float(m,0x00574d58u)+read_float(m,0x00574d3cu))*(float)height);
        upper=(long double)top*s->remaining_scale*read_float(m,0x00574d5cu);
        lower=(long double)bottom*s->remaining_scale*read_float(m,0x00574d5cu);
        health=(long double)base*s->height_scale*read_float(m,0x00574d60u);
    }
    world=game_memory_read_u32(m,0x0058973cu);motion=game_memory_read_u32(m,world+0x470u);
    game_memory_write_u32(m,scene+0x128u,float_bits(clearance));
    game_memory_write_u32(m,motion+0x20u,(uint32_t)top-truncate_word(upper));
    game_memory_write_u32(m,motion+0x14u,(uint32_t)bottom+truncate_word(lower));
    pose->properties[1]=(uint32_t)base+truncate_word(health);
    (void)scene;return !m->fault;
}
int player_event_apply(GameMemory *m,GameAddress scene,PresentationPose *pose,PlayerControls *s,
        const EffectResource *event,const PlayerEventServices *ports){
    unsigned code=event->event_code,i;int local;
    if((int32_t)pose->properties[0]<=0 || code>20u)return 1;
    switch(code){
    case 0:if(event->packet_mask && !player_packet_apply(m,scene,pose,s,event,ports))return 0;break;
    case 1:
        if(!player_packet_apply(m,scene,pose,s,event,ports))return 0;
        memcpy(pose->local_position,pose->position,sizeof(pose->position));
        for(i=0;i<3u;++i)pose->local_angles[2u-i]=pose->angles[i];
        s->previous_heading=s->heading;s->previous_aim=s->aim;
        if(game_memory_read_u32(m,0x005eff04u) && (!ports->refresh || !ports->refresh(ports->context,4u)))return 0;
        s->mode_flags&=~3u;break;
    case 2:s->input_flags|=0x10000u;break;
    case 3:s->input_flags|=0x20000u;break;
    case 4:stop_weapon(m,s,0u);break;
    case 5:stop_weapon(m,s,1u);break;
    case 6:stop_weapon(m,s,0u);stop_weapon(m,s,1u);break;
    case 7:case 8:case 9:select_weapon(m,scene,s,1u,code==7u?1u:code==8u?2u:0u,event->event_arguments[0]&255u);break;
    case 10:case 11:case 12:select_weapon(m,scene,s,0u,code==10u?1u:code==11u?2u:0u,event->event_arguments[0]&255u);break;
    case 13:if(!rescale(m,scene,pose,s,event,ports))return 0;break;
    case 15:
        local=game_memory_read_u32(m,0x0058973cu)==game_memory_read_u32(m,0x005f0208u);
        if(local && !(s->mode_flags&4u) && (!ports->sound || !ports->sound(ports->context,1u)))return 0;
        s->mode_flags|=4u;s->heading=s->previous_heading=0u;
        for(i=0;i<3u;++i)if(!ports->refresh || !ports->refresh(ports->context,i))return 0;
        break;
    case 16:
        local=game_memory_read_u32(m,0x0058973cu)==game_memory_read_u32(m,0x005f0208u);
        if(local && (s->mode_flags&4u) && (!ports->sound || !ports->sound(ports->context,0u)))return 0;
        s->mode_flags&=~4u;break;
    case 17:case 20:if(!ports->interact || !ports->interact(ports->context,code==17u?0u:1u))return 0;break;
    case 18:detach_feedback(m,s,0u);detach_feedback(m,s,1u);s->input_axes[0]=s->input_axes[1]=0u;s->input_flags=s->previous_input_flags=0u;break;
    default:break;
    }return !m->fault;
}
