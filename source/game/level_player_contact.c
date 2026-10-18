#include "level_player_scene.h"
#include "level_player_resources.h"
#include "level_effect_runtime.h"
#include "gameplay_messages.h"
#include "player_contact.h"
#include "player_feedback.h"
#include "entity_queries.h"
#include "presentation_instance.h"
#include "menu_sound.h"
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

extern uint32_t game_menu_milliseconds(GameApplication *);
extern int game_level_scene_property(GameApplication *,GameAddress,GameAddress,uint32_t *);
extern int game_level_scene_killed(GameApplication *,GameAddress,GameAddress,GameAddress);
extern int game_level_player_finalize(GameApplication *,GameAddress);
extern int game_level_player_relocate(GameApplication *,GameAddress,const float[2]);
extern int game_level_player_hit_marker(GameApplication *,const float[3],uint32_t,int32_t);
typedef struct Contact {GameApplication *app;GameAddress player;PresentationPose *pose;PlayerControls *controls;PlayerContactEvent *event;PlayerContactEnvironment *environment;} Contact;
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
static float real(GameApplication *a,GameAddress p){uint32_t bits=get(a,p);float value;memcpy(&value,&bits,4);return value;}
static int accept(void *c,const PlayerContactEvent *event,int *out){Contact *e=c;return native_entity_type_activity(&e->app->memory,event->entity,out);}
static int damage(void *c,const PlayerContactEvent *event,int32_t *out){Contact *e=c;return game_level_contact_damage(e->app,e->player,event->entity,out);}
static int maximum(void *c,int owner,int32_t *out){Contact *e=c;GameApplication *a=e->app;uint32_t value;int ok;
    if(e->event->type==0x10)put(a,e->event->entity+0xd8,(uint32_t)e->event->amount);
    if(owner)ok=game_level_player_catalog_value(a,e->environment->owner,0x20,0,0,&value);
    else ok=game_level_scene_property(a,e->player,get(a,get(a,e->player)+0x44),&value);
    if(e->event->type==0x10)e->event->amount=(int32_t)get(a,e->event->entity+0xd8);
    if(!ok || a->memory.fault)return 0;
    *out=(int32_t)value;return 1;
}
static int tick(void *c,GameAddress method,uint32_t *out){Contact *e=c;(void)method;*out=game_menu_milliseconds(e->app);return !e->app->memory.fault;}
static uint32_t elapsed(void *c,uint32_t now,uint32_t start){(void)c;return (now-start)>>5;}
static int hit_timer(void *c){Contact *e=c;GamePlayerHitTimerOps services={e,tick,elapsed};return game_player_update_hit_timer(&e->app->memory,&services,NULL);}
static int acknowledge(void *c){Contact *e=c;if(!hit_timer(e))return 0;put(e->app,0x627900,1);return !e->app->memory.fault;}
static int sound(void *c,unsigned id){Contact *e=c;SoundPlaybackRequest request={(int16_t)id,0,1,0,0,0};return game_menu_sound_play(e->app,&request);}
static int effect(void *c,const PlayerContactEvent *event,int32_t amount){Contact *e=c;uint32_t kind=event->effect_type;
    if(event->flags&4){uint32_t payload=get(e->app,event->entity+0xd4);kind=(payload&0xffffff00u)|game_memory_read_u8(&e->app->memory,payload+12);}
    return !e->app->memory.fault && game_level_player_hit_marker(e->app,event->position,kind,amount);
}
static int remove_player(void *c,const PlayerContactEvent *event){Contact *e=c;GameApplication *a=e->app;
    return game_level_scene_killed(a,e->player,event->entity,get(a,get(a,e->player)+0x10)) && game_level_player_finalize(a,e->player);
}
static int reward(void *c,unsigned kind,uint32_t amount){Contact *e=c;GameApplication *a=e->app;GameAddress owner=e->environment->owner;uint32_t live,total,value;
    if(kind==0 || kind==2){live=kind==0?0x588:0x22c;put(a,owner+live,get(a,owner+live)+amount);}
    else{live=0x4b4+(kind-5)*4;total=0x4cc+(kind-5)*4;value=get(a,owner+live);
        if(kind!=6 || (int32_t)value<10)put(a,owner+live,value+amount);
        amount=get(a,e->event->entity+0xd8);put(a,owner+total,get(a,owner+total)+amount);}
    e->event->amount=(int32_t)get(a,e->event->entity+0xd8);return !a->memory.fault;
}
static int inventory(void *c,unsigned slot,GameAddress *weapon,int32_t *count,int32_t *limit){Contact *e=c;GameApplication *a=e->app;GameAddress descriptor;
    if(slot>=15)return 0;
    *weapon=e->controls->weapons[slot];*count=*limit=0;if(!*weapon)return 1;
    *count=(int32_t)get(a,*weapon+12);if(*count==-1)return !a->memory.fault;
    descriptor=get(a,*weapon+20);*limit=(int32_t)get(a,descriptor+0x28);return !a->memory.fault;
}
static int set_inventory(void *c,GameAddress weapon,int32_t count){Contact *e=c;put(e->app,weapon+12,(uint32_t)count);return !e->app->memory.fault;}
static int random_value(void *c,unsigned scale,int32_t *out){Contact *e=c;GameApplication *a=e->app;uint32_t index=get(a,0x6308c0);long double value;
    if(index==1024)index=0;
    if(index>1024)return 0;
    put(a,0x6308c0,index+1);value=(long double)real(a,0x6308c8+index*4)*real(a,scale?0x574dd4:0x574e14);
    *out=!isfinite(value) || value< -0x1p63L || value>=0x1p63L?0:(int32_t)(uint32_t)(uint64_t)(int64_t)value;
    return !a->memory.fault;
}
static int ammo_scale(void *c,unsigned slot,uint32_t *out){Contact *e=c;if(slot>=15)return 0;*out=get(e->app,0x596a00+slot*4);return !e->app->memory.fault;}
static int text(GameApplication *a,GameAddress source,char *out,size_t size){size_t i;
    for(i=0;i<size;i++){out[i]=(char)game_memory_read_u8(&a->memory,source+(uint32_t)i);if(a->memory.fault)return 0;if(!out[i])return 1;}
    out[size-1]=0;return 1;
}
static int message(void *c,PlayerContactMessage kind,int32_t amount,unsigned slot){
    static const GameAddress plural[4]={0x596b04,0x596b2c,0x596b4c,0x596b6c},singular[4]={0x596b18,0x596b3c,0x596b5c,0x596b80};
    Contact *e=c;GameApplication *a=e->app;GameAddress format;char pattern[128],buffer[128],name[64];unsigned index;int mode=1;
    switch(kind){
    case CONTACT_CREDITS:format=amount==1?0x596a90:0x596a80;mode=amount==1?0:1;break;
    case CONTACT_HEALTH:format=amount==100?0x596b90:amount==50?0x596ba0:amount==25?0x596bac:0x596bbc;mode=amount==100 || amount==50 || amount==25?0:1;break;
    case CONTACT_SCORE:format=0x596aa0;break;
    case CONTACT_REFILL:format=0x596ab0;mode=0;break;
    case CONTACT_DEFAULT_AMMO:format=0x596ac4;mode=0;break;
    case CONTACT_AMMO:if(slot>=15)return 0;format=slot==2?0x596ad8:0x596af4;mode=2;break;
    case CONTACT_UNKNOWN_PICKUP:format=0x596bc8;break;
    default:index=(unsigned)kind-CONTACT_REWARD5;if(index>=4)return 0;format=amount==1?singular[index]:plural[index];mode=amount==1?0:1;break;
    }
    if(!text(a,format,pattern,sizeof(pattern)))return 0;
    if(mode==2){if(!text(a,0x596938+slot*13,name,sizeof(name)))return 0;snprintf(buffer,sizeof(buffer),pattern,amount,name);}
    else if(mode==1)snprintf(buffer,sizeof(buffer),pattern,amount);
    else snprintf(buffer,sizeof(buffer),"%s",pattern);
    return game_message_enqueue_bytes(a,get(a,0x628a88),buffer,1);
}
static int relocate(void *c,const float p[2]){Contact *e=c;return game_level_player_relocate(e->app,e->player,p);}
static int impulse(void *c,int16_t value){Contact *e=c;game_memory_write_u16(&e->app->memory,0x596a3c,(uint16_t)value);return !e->app->memory.fault;}
static PlayerContactServices services(Contact *e){PlayerContactServices s={e,accept,damage,maximum,hit_timer,sound,effect,remove_player,acknowledge,reward,inventory,set_inventory,random_value,ammo_scale,message,relocate,impulse};return s;}
static void environment(Contact *e){GameApplication *a=e->app;PlayerContactEnvironment *out=e->environment;GameAddress world=get(a,0x5f0208);
    out->owner=get(a,0x5f0210+(e->pose->identity&63)*4);out->local=out->owner==world;
    out->dead=get(a,e->player+0x498)!=UINT32_MAX;out->health_cheat=get(a,0x594d38)!=0;out->damage_immune=get(a,out->owner+0x148)==1;
    out->controlled=get(a,world+0x3bc)==e->player;out->primary=get(a,world+0x448)==e->player;
    out->owner_index=game_memory_read_u16(&a->memory,world+8);out->impulse=(int16_t)game_memory_read_u16(&a->memory,0x596a3c);
    out->knockback_scale=real(a,0x574e00);out->thresholds[0]=real(a,0x574e40);out->thresholds[1]=real(a,0x574d9c);out->thresholds[2]=real(a,0x574d4c);
}
int game_level_player_apply_damage(GameApplication *a,GameAddress player,uint32_t amount){PlayerContactEvent event={0};PlayerContactEnvironment env={0};
    Contact e={a,player,presentation_instance_pose(player),presentation_instance_player_controls(player),&event,&env};PlayerContactServices s;
    if(!e.pose || !e.controls)return 0;
    env.owner=get(a,0x5f0210+(e.pose->identity&63)*4);env.damage_immune=get(a,env.owner+0x148)==1;
    if(a->memory.fault)return 0;
    if(env.damage_immune || (int32_t)amount<=0)return 1;
    environment(&e);s=services(&e);return !a->memory.fault && player_apply_contact_damage(e.pose,e.controls,amount,&env,&s);
}
int game_level_player_contact(GameApplication *a,GameAddress player,GameAddress other,const float velocity[3],const float point[3]){PlayerContactEvent event={0};PlayerContactEnvironment env={0};PlayerRig *rig=presentation_instance_player_rig(player);
    Contact e={a,player,presentation_instance_pose(player),presentation_instance_player_controls(player),&event,&env};PlayerContactServices s;int result;
    if(!e.pose || !e.controls || !rig)return 0;
    if((int32_t)e.pose->properties[0]<=0)return 1;
    event.entity=other;event.type=game_memory_read_u8(&a->memory,other+12);event.flags=get(a,other+16);event.effect_type=event.type;
    if(event.type==0x10){event.kind=get(a,other+0xd4);event.amount=(int32_t)get(a,other+0xd8);}
    if(event.type==0xa0){event.owner=get(a,other+0x78);event.relocate=get(a,other+0x70)!=0;event.destination[0]=real(a,other+0x64);event.destination[1]=real(a,other+0x6c);}
    memcpy(event.velocity,velocity,12);memcpy(event.position,point,12);environment(&e);s=services(&e);
    result=!a->memory.fault && player_handle_contact(e.pose,e.controls,rig,&event,&env,&s);
    if(event.type==0x10 && event.kind==1)put(a,other+0xd8,(uint32_t)event.amount);
    return result && !a->memory.fault;
}
