#include "player_contact.h"
#include <math.h>
#include <limits.h>

int32_t player_contact_scaled_damage(int32_t base,long double coefficient){
    volatile long double value=(long double)base*coefficient;
    if(!isfinite(value) || value<(long double)INT64_MIN || value>=9223372036854775808.0L)return 0;
    return (int32_t)(uint32_t)(uint64_t)(int64_t)value;
}
int32_t player_contact_resisted_damage(int32_t damage,int32_t armor,uint8_t type,uint32_t flags){
    if(damage>armor)return (int32_t)((uint32_t)damage-(uint32_t)armor);
    if(damage>0 || type==0x0bu || (flags&4u))return 1;
    return 0;
}

int player_apply_contact_damage(PresentationPose *pose,PlayerControls *controls,uint32_t damage,
        const PlayerContactEnvironment *environment,const PlayerContactServices *services){
    unsigned threshold;
    if(environment->damage_immune || (int32_t)damage<=0)return 1;
    if(controls->mode_flags&8u){damage>>=2u;if(damage<1u)damage=1u;}
    if(environment->controlled){
        if(environment->primary && !services->hit_timer(services->context))return 0;
        /* Catalog callbacks can mutate health. Query and read each side in the
         * original order, rather than caching maximum health across thresholds. */
        for(threshold=0u;threshold<3u;++threshold){
            int32_t maximum;volatile long double boundary;int32_t after;
            if(!services->maximum_health(services->context,0,&maximum))return 0;
            boundary=(long double)maximum*environment->thresholds[threshold];
            after=(int32_t)(pose->properties[0]-damage);
            if((long double)after>boundary)continue;
            if(!services->maximum_health(services->context,0,&maximum))return 0;
            boundary=(long double)maximum*environment->thresholds[threshold];
            if(!((long double)(int32_t)pose->properties[0]<=boundary)){
                if(!services->sound(services->context,0x16u-threshold))return 0;
                break;
            }
        }
    }
    pose->properties[0]-=damage;return 1;
}
static int refill(const PlayerContactServices *services,unsigned slot,int required){
    GameAddress weapon;int32_t count,maximum;
    if(!services->inventory(services->context,slot,&weapon,&count,&maximum))return 0;
    if(!weapon)return !required;
    if(count==-1 || maximum<=count)return 1;
    return services->set_inventory(services->context,weapon,maximum);
}
static int notify(const PlayerContactEnvironment *environment,const PlayerContactServices *services,
        PlayerContactMessage message,int32_t amount,unsigned slot){
    return !environment->local || services->message(services->context,message,amount,slot);
}
static void knockback(PlayerRig *rig,const PlayerContactEvent *event,int32_t damage,float scale){
    volatile long double squared=(long double)event->velocity[0]*event->velocity[0];
    volatile float length_squared;
    long double root,reciprocal,x,z;float length;
    squared+=(long double)event->velocity[2]*event->velocity[2];length_squared=(float)squared;
    root=sqrtl((long double)length_squared);length=(float)root;
    if(!(root>0.0L))return;
    reciprocal=1.0L/(long double)length;
    x=(long double)event->velocity[0]*reciprocal;
    z=(long double)event->velocity[2]*reciprocal;
    x*=damage;z*=damage;x*=scale;z*=scale;
    rig->controller_velocity[0]=(float)(x+(long double)rig->controller_velocity[0]);
    rig->controller_velocity[2]=(float)(z+(long double)rig->controller_velocity[2]);
}
static int pickup(PresentationPose *pose,PlayerContactEvent *event,
        const PlayerContactEnvironment *environment,const PlayerContactServices *services){
    unsigned slot;uint32_t amount=(uint32_t)event->amount;
    if(environment->local && (!services->sound(services->context,0x7bu) ||
        !services->acknowledge(services->context)))return 0;
    switch(event->kind){
    case 0u:
        return services->reward(services->context,0u,amount) && notify(environment,services,CONTACT_CREDITS,event->amount,0u);
    case 1u:{
        int32_t maximum,increment,health;
        if(event->amount<1)event->amount=1;else if(event->amount>=100)event->amount=100;
        if(!services->maximum_health(services->context,1,&maximum))return 0;
        increment=(int32_t)((uint32_t)event->amount*(uint32_t)maximum)/100;
        health=(int32_t)(pose->properties[0]+(uint32_t)increment);
        if(health>=maximum)health=maximum;
        pose->properties[0]=(uint32_t)health;
        return notify(environment,services,CONTACT_HEALTH,event->amount,0u);
    }
    case 2u:
        return services->reward(services->context,2u,amount) && notify(environment,services,CONTACT_SCORE,event->amount,0u);
    case 3u:
        for(slot=0u;slot<15u;++slot)if(!refill(services,slot,0))return 0;
        return notify(environment,services,CONTACT_REFILL,0,0u);
    case 4u:{
        int32_t start,step,candidate,count,maximum;GameAddress weapon;uint32_t scale,increment;
        if(!services->random(services->context,0u,&start) || !services->random(services->context,1u,&step))return 0;
        candidate=start;
        do{
            candidate=(int32_t)((uint32_t)candidate+(uint32_t)step+1u)%13;
            slot=(unsigned)candidate+2u;
            if(!services->inventory(services->context,slot,&weapon,&count,&maximum))return 0;
        }while(!weapon && candidate!=start);
        if(!weapon){
            if(!refill(services,0u,1) || !refill(services,1u,1))return 0;
            return notify(environment,services,CONTACT_DEFAULT_AMMO,0,0u);
        }
        if(!services->ammo_scale(services->context,slot,&scale))return 0;
        increment=scale*amount;
        if(!services->set_inventory(services->context,weapon,(int32_t)((uint32_t)count+increment)))return 0;
        return notify(environment,services,CONTACT_AMMO,(int32_t)increment,slot);
    }
    case 5u:case 6u:case 7u:case 8u:
        return services->reward(services->context,event->kind,amount) &&
            notify(environment,services,(PlayerContactMessage)(CONTACT_REWARD5+event->kind-5u),event->amount,0u);
    default:return notify(environment,services,CONTACT_UNKNOWN_PICKUP,(int32_t)event->kind,0u);
    }
}
int player_handle_contact(PresentationPose *pose,PlayerControls *controls,PlayerRig *rig,
        PlayerContactEvent *event,PlayerContactEnvironment *environment,const PlayerContactServices *services){
    int accepted;int32_t damage;
    if((int32_t)pose->properties[0]<=0)return 1;
    if(!services->accept(services->context,event,&accepted))return 0;
    accepted=accepted || (event->flags&4u) || event->type==0x41u || event->type==0x0bu || event->type==0x0au;
    if(accepted && !environment->dead){
        if(!services->damage(services->context,event,&damage) ||
            !player_apply_contact_damage(pose,controls,(uint32_t)damage,environment,services))return 0;
        if(environment->local){
            rig->motion.lean=(int16_t)((uint16_t)rig->motion.lean+(uint16_t)environment->impulse);
            environment->impulse=(int16_t)(0u-(uint16_t)environment->impulse);
            if(!services->publish_impulse(services->context,environment->impulse))return 0;
            if(((event->flags&4u) || event->type==0x41u || event->type==0x0bu) &&
                !services->effect(services->context,event,damage))return 0;
            if(damage>1)knockback(rig,event,damage,environment->knockback_scale);
        }
        if(environment->health_cheat){
            int32_t maximum;if(!services->maximum_health(services->context,0,&maximum))return 0;
            pose->properties[0]=(uint32_t)maximum;
        }
        if((int32_t)pose->properties[0]<=0)return services->remove_player(services->context,event);
    }
    if(event->type==0x10u)return pickup(pose,event,environment,services);
    if(event->type==0xa0u && !(controls->mode_flags&1u) && (!event->owner || event->owner==environment->owner)){
        if(event->relocate && !services->relocate(services->context,event->destination))return 0;
        if(environment->owner_index==(pose->identity&63u) && !services->acknowledge(services->context))return 0;
    }
    return 1;
}
