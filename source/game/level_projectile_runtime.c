#include "level_effect_runtime.h"
#include "level_factory.h"
#include "level_objects.h"
#include "level_player_slots.h"
#include "projectile.h"
#include "projectile_factory.h"
#include "presentation_collision.h"
#include "gameplay_spatial.h"
#include "gameplay_geometry.h"
#include "game_intrusive_list.h"
#include "effect_resource_pool.h"
#include "terrain_asset.h"
#include "menu_sound.h"
#include "vehicle_targeting.h"
#include "level_run.h"
#include "player_contact.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

extern int game_input_world_line(GameApplication *,const float[3],const float[3],float[3],int *);
extern int game_level_contact_response(GameApplication *,GameAddress,GameAddress,GameAddress,const uint32_t[3],const float[3]);
extern int game_level_particle_emit(GameApplication *,GameAddress,const float[3],const float[3],float,GameAddress,uint16_t,uint32_t,uint32_t);
typedef struct Shot {GameApplication *app;GameAddress object;Projectile *state;PresentationPose *pose;} Shot;
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static float real(GameApplication *a,GameAddress p){uint32_t bits=get(a,p);float value;memcpy(&value,&bits,4);return value;}
static int visible(void *c,const float from[3],const float to[3],int *clear){Shot *s=c;float hit[3];return game_input_world_line(s->app,from,to,hit,clear);}
static int sequence(void *c,GameAddress catalog,uint32_t *out){GameApplication *a=c;uint32_t next;
    if(level_player_slots_owns_player(catalog)){if(!level_player_slots_next_owner_sequence(&a->memory,catalog,out,&next))return 0;}
    else{uint32_t n=get(a,catalog+24);next=n+1;*out=(n<<6)|game_memory_read_u16(&a->memory,catalog+8);}
    game_memory_write_u32(&a->memory,catalog+24,next);return !a->memory.fault;
}
static int launch(void *c,GameAddress child,uint32_t id,const PresentationPose *pose,GameAddress recipe,GameAddress shooter){
    return game_level_projectile_launch(c,child,id,pose->position,pose->angles,recipe,shooter,0x431d00u,1);
}
static int publish(void *c,GameAddress catalog,GameAddress child){GameApplication *a=c;uint32_t flags=get(a,child+16),slot=(flags&1)?((flags&64)?0x80:0x1c):0xe4;
    if(a->memory.fault || !level_player_slots_attach_scene_member(catalog,child,slot))return 0;
    game_list_insert_after(&a->memory,child,catalog+slot,0x48,0x4c);
    if(a->memory.fault){level_player_slots_forget_scene_member(child);return 0;}return 1;
}
static int split(Shot *s){ProjectileSplitServices services={0};game_level_projectile_factory_services(s->app,&services.factory);
    services.sequence=sequence;services.launch=launch;services.publish=publish;
    return projectile_split(&s->app->memory,s->object,s->state,s->pose,&services);
}
static int collision(void *c,GameAddress candidate,GameAddress source,float point[3],int *hit){Shot *s=c;GameApplication *a=s->app;uint32_t method=get(a,get(a,candidate)+0x24u);
    if(method==0x419e60u)return presentation_spawn_collision(&a->memory,candidate,source,point,hit);
    if(method==0x482bb0u)return presentation_record_collision(&a->memory,candidate,source,point,hit);
    if(method==0x42dfd0u)return presentation_collision(&a->memory,candidate,source,0,NULL,point,hit);
    a->memory.fault=1;a->memory.fault_address=method;return 0;
}
static int gather(Shot *s){GameApplication *a=s->app;GameAddress heads[9],candidate,record;EffectResourcePool *pool;EffectResource *contact;float point[3];ProjectileContactServices services={s,collision};
    if(!gameplay_spatial_query_heads(gameplay_spatial_grid(0x5f0800),s->pose->position[0],s->pose->position[2],heads))return 0;
    pool=effect_resource_pool_find(0x631910);if(!pool)return 0;
    candidate=projectile_find_contact(&a->memory,s->object,s->state,s->pose,heads,&services,point);
    if(a->memory.fault)return 0;
    if(!candidate)return 1;
    record=effect_resource_pool_acquire_kind(&a->memory,pool,1);contact=effect_resource_find(record);if(!contact)return 0;
    contact->contact=candidate;memcpy(contact->contact_vector,point,12);
    game_memory_write_u32(&a->memory,s->object+0xb0,record);return !a->memory.fault;
}
static int scaled_expire(Shot *s);
static int reset(Shot *s){GameApplication *a=s->app;
    if(get(a,get(a,s->object)+0x20)==0x48e960)return game_level_mole_reset(a,s->object);
    if(get(a,get(a,s->object)+0x20)==0x4fc9d0 || get(a,get(a,s->object)+0x20)==0x50d720)return scaled_expire(s);
    return get(a,get(a,s->object)+0x20u)==0x5362d0u && game_level_presentation_reset(a,s->object,1);}
static int32_t signed_word(uint32_t bits){int32_t value;memcpy(&value,&bits,4);return value;}
static void increment_word(int32_t *value){uint32_t bits;memcpy(&bits,value,4);++bits;memcpy(value,&bits,4);}
/*4fc9d0: retire the linked children from the tail first. Each is returned to
 *its existing presentation pool; target and predecessor links are borrowed.
 *An iterative walk avoids using the host stack for a long gameplay chain. */
int game_level_scaled_effect_reset(GameApplication *a,GameAddress object){
    GameAddress node=object,next,previous;unsigned visits=0;uint8_t kind;
    if(!a || a->memory.fault)return 0;
    kind=game_memory_read_u8(&a->memory,object+12);if(kind!=3 && kind!=0x0f)return 0;
    while((next=get(a,node+0xe8)) && game_memory_read_u8(&a->memory,next+12)==kind){
        if(++visits>65536)return 0;node=next;
    }
    if(a->memory.fault)return 0;
    for(;;){
        previous=get(a,node+0xe4);
        if(kind==0x0f && get(a,0x5f0210+(get(a,node+20)&63)*4)==get(a,0x5f0208) &&
           (!previous || game_memory_read_u8(&a->memory,previous+12)!=0x0f) &&
           !game_menu_sound_stop(a,0x13,0))return 0;
        game_memory_write_u32(&a->memory,node+0xec,0);game_memory_write_u32(&a->memory,node+0xf0,0);
        game_memory_write_u32(&a->memory,node+0xe4,0);game_memory_write_u32(&a->memory,node+0xe8,0);
        if(!game_level_presentation_reset(a,node,1))return 0;
        if(node==object)return !a->memory.fault;
        if(!previous || game_memory_read_u8(&a->memory,previous+12)!=kind || ++visits>65536)return 0;
        node=previous;
    }
}
static int shot_sound(GameApplication *,const float[3],int16_t);
static int scaled_sound(GameApplication *a,GameAddress o,int update){
    GameAddress bank=get(a,0x628a90),listener=get(a,0x58f890);SoundPlaybackRequest r={0};int playing;
    long double x=(long double)real(a,o+24)-real(a,listener+12),z=(long double)real(a,o+32)-real(a,listener+20);
    r.sound=19;r.distance_squared=(float)(z*z+x*x);r.volume=r.pitch=1;r.flags=1;r.force=1;
    if(update){
        if(!game_menu_sound_playing(a,19,0,&playing))return 0;
        if(playing)return game_menu_sound_volume(a,19,0,sound_playback_attenuation(&a->memory,get(a,bank+0x58),r.distance_squared));
    }
    return !a->memory.fault && game_menu_sound_play(a,&r);
}
int game_level_scaled_effect_launch(GameApplication *a,GameAddress o,uint32_t id,const float p[3],const uint16_t angles[3],GameAddress recipe,GameAddress shooter){
    GameAddress next;unsigned i;uint8_t kind=game_memory_read_u8(&a->memory,o+12);
    uint32_t head=kind==3?0x21c:0x220,random_base=kind==3?0x5752c0:0x575118;
    if(kind!=3 && kind!=0x0f)return 0;
    if(!game_level_projectile_launch(a,o,id,p,angles,recipe,shooter,0x42f970,0))return 0;
    for(i=0;i<3;i++)game_memory_write_u32(&a->memory,o+0xf4+i*4,get(a,o+0x88+i*16));
    game_memory_write_u32(&a->memory,o+0xe4,shooter);next=get(a,shooter+head);
    game_memory_write_u32(&a->memory,o+0xe8,next);game_memory_write_u32(&a->memory,shooter+head,o);
    if(next){game_memory_write_u32(&a->memory,next+0xe4,o);
        game_memory_write_u32(&a->memory,o+0xec,get(a,next+0xec));game_memory_write_u32(&a->memory,o+0xf0,get(a,next+0xf0));
    }else if(kind==0x0f && get(a,0x5f0210+(id&63)*4)==get(a,0x5f0208) && !scaled_sound(a,o,0))return 0;
    game_memory_write_u32(&a->memory,o+0x110,1);
    {uint32_t word=effect_initialization_random(&a->memory,real(a,random_base))-700u;
        float value=(float)signed_word(word);uint32_t bits;memcpy(&bits,&value,4);game_memory_write_u32(&a->memory,o+0x100,bits);
        word=effect_initialization_random(&a->memory,real(a,random_base+4))-200u;value=(float)signed_word(word);memcpy(&bits,&value,4);game_memory_write_u32(&a->memory,o+0x104,bits);
    }
    game_memory_write_u32(&a->memory,o+0x108,0);
    game_memory_write_u16(&a->memory,o+0x10c,(uint16_t)(effect_initialization_random(&a->memory,real(a,random_base+8))+4));
    return !a->memory.fault;
}
int game_level_scaled_effect_contact(GameApplication *a,GameAddress o,GameAddress other,const float p[3]){
    GameAddress previous,node;unsigned visits=0;int retire;
    uint8_t kind=game_memory_read_u8(&a->memory,o+12);
    if(kind!=3 && kind!=0x0f)return 0;
    if(!get(a,other+0x58) || !get(a,other+20) || signed_word(get(a,o+0x58))<=0)return !a->memory.fault;
    /*50d5a0 heals the shooter before publishing the new contact target. The
     *same recipe, resistance table and owned health storage serve damage. */
    if(kind==3 && signed_word(get(a,other+0x58))>0){
        GameAddress recipe=get(a,o+0xe0),shooter=get(a,o+0xd4);
        uint32_t resistance,damage=get(a,recipe+0x14),damage_kind=get(a,recipe+0x18),heal,maximum,total;
        float factor;
        if(!game_level_scene_property(a,other,get(a,get(a,other)+0x4c),&resistance))return 0;
        if(damage_kind>=18 || resistance>=13)return 0;
        factor=real(a,get(a,0x59dae4)+(damage_kind*13+resistance)*4);
        if(get(a,0x594d3c) && (get(a,shooter+20)&63u)==game_memory_read_u16(&a->memory,get(a,0x5f0208)+8))damage<<=2;
        heal=(uint32_t)player_contact_scaled_damage(signed_word(damage),factor)-get(a,other+0x5c);
        if(signed_word(heal)>0 && get(a,shooter+0x58)){
            total=get(a,shooter+0x58)+heal;
            if(!game_level_scene_property(a,shooter,get(a,get(a,shooter)+0x44),&maximum))return 0;
            game_memory_write_u32(&a->memory,shooter+0x58,signed_word(total)>signed_word(maximum)?maximum:total);
        }
    }
    previous=get(a,o+0xe4);
    if(get(a,o+0xec)!=other){
        for(node=previous;node && game_memory_read_u8(&a->memory,node+12)==kind;node=get(a,node+0xe4)){
            if(++visits>65536)return 0;
            game_memory_write_u32(&a->memory,node+0xec,other);game_memory_write_u32(&a->memory,node+0xf0,get(a,other+20));
        }
        if(kind==3){float point[3];unsigned i;
            for(i=0;i<3;i++)point[i]=real(a,o+24+i*4);
            if(!shot_sound(a,point,11))return 0;
        }
    }
    retire=!previous || game_memory_read_u8(&a->memory,previous+12)==kind;
    if(previous && retire)game_memory_write_u32(&a->memory,previous+0xe8,other);
    if(!game_level_projectile_impact(a,o,p,other,0,0x42fa00))return 0;
    return retire?game_level_scaled_effect_reset(a,o):!a->memory.fault;
}
static int scaled_expire(Shot *s){GameApplication *a=s->app;GameAddress previous=get(a,s->object+0xe4),node;unsigned visits=0;
    uint8_t kind=game_memory_read_u8(&a->memory,s->object+12);
    if(previous){game_memory_write_u32(&a->memory,previous+(game_memory_read_u8(&a->memory,previous+12)==kind?0xe8:(kind==3?0x21c:0x220)),0);
        for(node=previous;node && game_memory_read_u8(&a->memory,node+12)==kind;node=get(a,node+0xe4)){
            if(++visits>65536)return 0;game_memory_write_u32(&a->memory,node+0xec,0);game_memory_write_u32(&a->memory,node+0xf0,0);
        }}
    return !a->memory.fault && game_level_scaled_effect_reset(a,s->object);
}
static int scaled_track(Shot *s){GameApplication *a=s->app;GameAddress o=s->object,target=get(a,o+0xec),node,last=o;unsigned visits=0;
    uint8_t kind=game_memory_read_u8(&a->memory,o+12);
    if(!target || get(a,target+20)==get(a,o+0xf0))return !a->memory.fault;
    for(node=o;node && game_memory_read_u8(&a->memory,node+12)==kind;node=get(a,node+0xe4)){
        if(++visits>65536)return 0;game_memory_write_u32(&a->memory,node+0xec,0);game_memory_write_u32(&a->memory,node+0xf0,0);
    }
    node=get(a,o+0xe8);if(!node)return 0; /* Retail assumes this link exists. */
    while(game_memory_read_u8(&a->memory,node+12)==kind){
        if(++visits>65536)return 0;last=node;
        game_memory_write_u32(&a->memory,node+0xec,0);game_memory_write_u32(&a->memory,node+0xf0,0);node=get(a,node+0xe8);
        if(!node)return 0;
    }
    game_memory_write_u32(&a->memory,last+0xe8,0);return !a->memory.fault;
}
/*4fc6d0: seek a live target, normalize the stored direction and move using
 *the recipe speed. The terrain floor is tested in extended precision. */
static int scaled_move(Shot *s){GameApplication *a=s->app;GameAddress o=s->object,target=get(a,o+0xec);float direction[3];unsigned i;
    int curved=game_memory_read_u8(&a->memory,o+12)==3;
    float unit=real(a,curved?0x575288:0x5750d0);
    if(target && get(a,target+20)==get(a,o+0xf0)){
        float point[3],square,length;long double norm;
        {float origin[3];GameAddress shooter=get(a,o+0xd4);
            for(i=0;i<3;i++)origin[i]=real(a,shooter+24+i*4);
            if(!presentation_target_position(&a->memory,target,origin,point))return 0;}
        for(i=0;i<3;i++)direction[i]=(float)((long double)point[i]-s->pose->position[i]);
        square=(float)(((long double)direction[0]*direction[0]+(long double)direction[1]*direction[1])+(long double)direction[2]*direction[2]);
        norm=gameplay_geometry_square_root(square);length=(float)norm;
        if(!isnan(norm) && norm!=unit && (length!=0.0f)){
            long double inverse=(long double)unit/length;
            for(i=0;i<3;i++)direction[i]=(float)(inverse*direction[i]);
        }
        for(i=0;i<3;i++){uint32_t bits;memcpy(&bits,&direction[i],4);game_memory_write_u32(&a->memory,o+0xf4+i*4,bits);}
        s->pose->angles[0]=vehicle_targeting_vector_angle(&a->memory,direction[0],direction[2],real(a,curved?0x5752b8:0x575114));
    }else for(i=0;i<3;i++)direction[i]=real(a,o+0xf4+i*4);
    if(curved){
        float velocity[3],offset[3];
        for(i=0;i<3;i++)velocity[i]=(float)((long double)direction[i]*real(a,s->state->recipe+0x1c));
        if(get(a,o+0xe8)){
            uint16_t bits=game_memory_read_u16(&a->memory,o+0x10c);int16_t period;int half;long double inverse;
            uint32_t angle=s->pose->angles[0]&2047u;
            float sine=real(a,0x6298a8+angle*4),cosine=real(a,0x6298a8+((angle+512)&2047u)*4);
            memcpy(&period,&bits,2);half=period>=0?period/2:-((-(int)period+1)/2);
            /* Retail divides by the signed half-period. Reject a corrupt zero
             * divisor instead of invoking undefined host arithmetic. */
            if(!half || (half==-1 && signed_word(get(a,0x5ac6ac)-s->state->birth_tick)==INT32_MIN))return 0;
            if(signed_word(get(a,0x5ac6ac)-s->state->birth_tick)%half==0)
                game_memory_write_u32(&a->memory,o+0x110,get(a,o+0x110)<1u?1u:0u);
            offset[0]=(float)((long double)real(a,o+0x108)*sine+(long double)real(a,o+0x100)*cosine);
            offset[1]=real(a,o+0x104);
            offset[2]=(float)((long double)real(a,o+0x100)*(-sine)+(long double)real(a,o+0x108)*cosine);
            inverse=(long double)unit/period;if(!get(a,o+0x110))inverse=-inverse;
            for(i=0;i<3;i++){offset[i]=(float)(offset[i]*inverse);velocity[i]=(float)((long double)velocity[i]+offset[i]);}
        }
        for(i=0;i<3;i++)s->pose->position[i]=(float)((long double)s->pose->position[i]+velocity[i]);
    }else for(i=0;i<3;i++)s->pose->position[i]=(float)((long double)direction[i]*real(a,s->state->recipe+0x1c)+s->pose->position[i]);
    {long double floor=gameplay_geometry_terrain_height(&a->memory,s->pose->position[0],s->pose->position[2])+real(a,curved?0x5752bc:0x5750fc);
        if(floor>=s->pose->position[1])s->pose->position[1]=(float)floor;}
    return !a->memory.fault && presentation_pose_build_world(&a->memory,s->pose) &&
        presentation_pose_transform_vertices(&a->memory,s->pose,0) && presentation_pose_export(&a->memory,o,s->pose);
}
static int shot_sound(GameApplication *a,const float p[3],int16_t sound){
    GameAddress listener=get(a,0x58f890);SoundPlaybackRequest request={0};
    long double x=(long double)p[0]-real(a,listener+12),z=(long double)p[2]-real(a,listener+20);
    request.sound=sound;request.volume=1;request.distance_squared=(float)(z*z+x*x);
    return !a->memory.fault && game_menu_sound_play(a,&request);
}
int game_level_line_projectile_launch(GameApplication *a,GameAddress object,uint32_t id,
        const float from[3],const float to[3],float strength,GameAddress recipe,GameAddress shooter){
    const uint16_t angles[3]={0,0,0};Projectile *state=presentation_instance_projectile(object);
    PresentationPose *pose=presentation_instance_pose(object);
    return state && pose && game_level_projectile_launch(a,object,id,from,angles,recipe,shooter,0x42f970,1) &&
        projectile_line_begin(&a->memory,state,pose,to,strength) && shot_sound(a,from,26);
}
static int unused_shot(GameApplication *a,GameAddress object){
    GameAddress source=get(a,object+0x50);if(!source || a->memory.fault)return 0;
    game_list_insert_after(&a->memory,object,source,4,8);return !a->memory.fault;
}
/*5479d0/548670: retain actual pooled owners through rejection or publication.
 *The root's4b0 recipe and3bc shooter are borrowed by the emitted projectile. */
static int ground_shot(Shot *s,const float point[3],int probe,int sound,int *accepted){
    GameApplication *a=s->app;GameAddress root=get(a,0x58973c),recipe=get(a,root+0x4b0),child;
    const uint16_t angles[3]={0,0,0};uint32_t id;int clear=1;
    if(accepted)*accepted=0;
    child=game_level_factory_take(a,9,(uint16_t)get(a,recipe+16));if(!child)return 0;
    if(probe && !gameplay_geometry_position_clear(&a->memory,child,point,&clear))goto failure;
    if(!clear)return unused_shot(a,child);
    if(get(a,get(a,child)+0x78)!=0x434780 || !sequence(a,root,&id) ||
       !game_level_position_shot_place(a,child,id,point,angles,recipe,0x20))goto failure;
    if(sound && !shot_sound(a,s->pose->position,3))goto failure;
    if(!publish(a,root,child))goto failure;
    if(accepted)*accepted=1;return !a->memory.fault;
failure:{int fault=a->memory.fault;a->memory.fault=0;(void)game_level_factory_destroy(a,child);if(fault)a->memory.fault=fault;return 0;}
}
static int motion_seed(Shot *s){
    GameApplication *a=s->app;GameAddress root,recipe,child;float from[3],to[3],strength;uint32_t id,tick;int allowed,clear;
    if(!projectile_motion_target(&a->memory,s->pose,from,to,&strength,&allowed))return 0;
    if(!allowed)return 1;
    root=get(a,0x58973c);recipe=get(a,root+0x4b0);
    child=game_level_factory_take(a,0x12,(uint16_t)get(a,recipe+16));if(!child)return 0;
    to[1]=gameplay_geometry_terrain_height(&a->memory,to[0],to[2]);
    if(!gameplay_geometry_position_clear(&a->memory,child,to,&clear))goto failure;
    if(!clear)return unused_shot(a,child);
    if(!sequence(a,root,&id) || !game_level_line_projectile_launch(a,child,id,from,to,strength,recipe,get(a,root+0x3bc)) ||
       !publish(a,root,child))goto failure;
    tick=get(a,0x5ac6ac);memcpy(&s->state->cell_x,&tick,4);increment_word(&s->state->cell_z);return !a->memory.fault;
failure:{int fault=a->memory.fault;a->memory.fault=0;(void)game_level_factory_destroy(a,child);if(fault)a->memory.fault=fault;return 0;}
}
/*547dd0 advances its motion through the34 update callback. It
 *probes blocked ground, emits parabolic shots, then completes its ground shot.
 *No renderer wrapper or alternate simulation state is introduced. */
int game_level_motion_seed_update(GameApplication *a,GameAddress object){
    Shot s={a,object,presentation_instance_projectile(object),presentation_instance_pose(object)};
    float ground,point[3];long double minimum;uint32_t tick,last;int blocked,clear;int16_t roll;
    if(!s.state || !s.pose || get(a,get(a,object)+0x88)!=0x536790 || !game_level_presentation_flight(a,object))return 0;
    ground=gameplay_geometry_terrain_height(&a->memory,s.pose->position[0],s.pose->position[2]);
    blocked=projectile_motion_blocked(&a->memory,s.pose->position);
    if(blocked && (s.state->velocity[0]==0.0f || s.state->velocity[2]==0.0f)){
        GameAddress root=get(a,0x58973c),recipe=get(a,root+0x4b0);
        GameAddress child=game_level_factory_take(a,9,(uint16_t)get(a,recipe+16));if(!child)return 0;
        s.state->velocity[0]=s.state->velocity[2]=0;
        memcpy(point,s.pose->position,12);point[1]=ground;
        if(!gameplay_geometry_position_clear(&a->memory,child,point,&clear)){
            int fault=a->memory.fault;a->memory.fault=0;(void)unused_shot(a,child);if(fault)a->memory.fault=fault;return 0;
        }
        if(clear)increment_word(&s.state->cell_z);
        if(!unused_shot(a,child))return 0;
    }
    minimum=(long double)ground+real(a,0x5772f8);
    if(minimum>s.pose->position[1])s.pose->position[1]=(float)minimum;
    tick=get(a,0x5ac6ac);memcpy(&roll,s.pose->angles+2,2);memcpy(&last,&s.state->cell_x,4);
    if(roll>=64 && signed_word(last+16u)<signed_word(tick) && !motion_seed(&s))return 0;
    if(signed_word(s.state->birth_tick+1960u)<signed_word(tick) ||
       signed_word(get(a,s.state->recipe+0x24))<=s.state->cell_z){
        if(!game_level_projectile_impact(a,object,s.pose->position,0,0,0x5480b0))return 0;
        s.pose->position[1]=ground;
        if(!ground_shot(&s,s.pose->position,1,0,NULL) || !reset(&s))return 0;
    }
    return !a->memory.fault;
}
/*4334e0: children are real pooled projectiles, published to the same catalog.
 *The borrowed recipe/emitter survive in the existing projectile record. */
int game_level_fragment_projectile_split(GameApplication *a,GameAddress object){
    Projectile *state=presentation_instance_projectile(object);PresentationPose *pose=presentation_instance_pose(object);
    GameAddress catalog,recipe,shooter,child;uint32_t generation,id;unsigned count,i;
    if(!a || !state || !pose || a->memory.fault)return 0;
    memcpy(&generation,&state->cell_x,4);if(!generation)return 1;
    catalog=get(a,0x5f0210+(pose->identity&63u)*4);recipe=state->recipe;shooter=state->shooter;
    count=generation-2u<1u?3u:1u;--generation;memcpy(&state->cell_x,&generation,4);
    for(i=0;i<count;i++){
        uint16_t angles[3]={(uint16_t)((pose->angles[0]+i*682u)&2047u),0,0};
        Projectile *part;PresentationPose *part_pose;
        child=game_level_factory_take(a,0x0d,(uint16_t)get(a,recipe+16));if(!child)return 0;
        part=presentation_instance_projectile(child);part_pose=presentation_instance_pose(child);
        if(!part || !part_pose || !sequence(a,catalog,&id) ||
           !game_level_object_place_angles(a,child,id,pose->position,angles) ||
           !projectile_fragment_begin(&a->memory,part,part_pose,recipe,shooter,0,generation) ||
           !publish(a,catalog,child)){
            int fault=a->memory.fault;a->memory.fault=0;(void)game_level_factory_destroy(a,child);
            if(fault)a->memory.fault=fault;return 0;
        }
    }
    return !a->memory.fault;
}
static int terrain(Shot *s,const float normal[3]){GameApplication *a=s->app;uint32_t method=get(a,get(a,s->object)+0x84u);uint8_t flags;uint32_t x,z,index;const float zero[3]={0,0,0};
    (void)normal;
    if(method==0x432d70u)return game_level_projectile_attachment_impact(a,s->object);
    if(method==0x547fe0u)return projectile_motion_settle(&a->memory,s->state,s->pose);
    if(method==0x548670u)return ground_shot(s,s->state->target_position,0,1,NULL) && reset(s);
    if(method!=0x430680u && method!=0x42f900u && method!=0x433660u)return 0;
    if((method==0x430680u || method==0x433660u) && !game_level_fragments_spawn(a,s->object,8,zero))return 0;
    if(!isfinite(s->pose->position[0]) || !isfinite(s->pose->position[2]) ||
       s->pose->position[0]<0 || s->pose->position[0]>=131072 || s->pose->position[2]<0 || s->pose->position[2]>=131072)return 0;
    x=(uint32_t)s->pose->position[0];z=(uint32_t)s->pose->position[2];index=(z>>9)*256+(x>>9);
    if(a->memory.terrain && a->memory.terrain->cells)flags=a->memory.terrain->cells[index].flags;
    else flags=game_memory_read_u8(&a->memory,get(a,0x597d90)+(z&0xfffffe00u)*4+((x&0xfffffe3fu)>>6)+5);
    if(a->memory.fault)return 0;
    if(flags&0x50u){if(method==0x42f900u && !game_level_fragment_impact(a,s->object,flags)){
        fprintf(stderr,"native terrain impact failed: surface effect flags=%02x\n",flags);return 0;}}
    else {
        if(!game_level_projectile_impact(a,s->object,s->pose->position,0,1,get(a,get(a,s->object)+0xb0u))){
            fprintf(stderr,"native terrain impact failed: animation method=%08x\n",get(a,get(a,s->object)+0xb0u));return 0;}
        if(method==0x433660u){unsigned axis;
            s->pose->position[1]=gameplay_geometry_terrain_height(&a->memory,s->pose->position[0],s->pose->position[2]);
            for(axis=0;axis<3;axis++)s->pose->position[axis]=(float)((long double)normal[axis]*real(a,0x5723c0)+s->pose->position[axis]);
            if(!game_level_fragment_projectile_split(a,s->object))return 0;
        }
    }
    return reset(s);
}
static int force_and_move(Shot *s,uint32_t tick){GameApplication *a=s->app;uint32_t table=get(a,s->object),force=get(a,table+0xa8u),move=get(a,table+0xa4u);ProjectileServices services={s,visible};
    if(force==0x48ddf0 && move==0x48e1d0)return game_level_mole_steer(a,s->object) && game_level_mole_move(a,s->object);
    if(force==0x431120u || force==0x4320f0u || force==0x432960u || (force==0x431b30u && s->state->split==1)){
        if(!projectile_home(&a->memory,s->state,s->pose,tick,&services)){
            fprintf(stderr,"native projectile homing failed: object=%08x target=%08x\n",s->object,s->state->target);return 0;}
    }else if(force==0x430110u){if(!projectile_bounce_force(&a->memory,s->state,s->pose))return 0;}
    else if(force==0x432410u){if(!projectile_delayed_force(&a->memory,s->state,s->pose,tick,&services))return 0;}
    else if(force==0x4335c0u){if(!projectile_fragment_force(&a->memory,s->state))return 0;}
    else if(force==0x548570u){if(!projectile_line_force(&a->memory,s->state))return 0;}
    else if(force==0x547bf0u){if(!projectile_motion_update(&a->memory,s->state,s->pose))return 0;}
    else if(force!=0x42f410u && force!=0x431b30u)return 0;
    if(move==0x431b50u)return split(s);
    if(move==0x432440u)return projectile_delayed_move(&a->memory,s->state,s->pose,tick);
    if(move==0x5485a0u)return projectile_line_move(&a->memory,s->state,s->pose);
    if(move!=0x5368b0u && move!=0x4335e0u && move!=0x4318b0u && move!=0x432100u && move!=0x432970u)
        return move==0x547cd0u && projectile_motion_post(&a->memory,s->state,s->pose);
    if(move!=0x5368b0u && move!=0x4335e0u && get(a,table+0xb8u)!=0x42fd70u)return 0;
    {int ok=projectile_move_program(&a->memory,s->state,s->pose,move,get(a,table+8)==0x430b10);
        if(!ok)fprintf(stderr,"native projectile motion program failed: object=%08x method=%08x\n",s->object,move);
        return ok;}
}
static int flight(Shot *s,int follows_anchor){GameApplication *a=s->app;uint32_t tick=get(a,0x5ac6ac),table=get(a,s->object);GameAddress record;float normal[3];int hit;unsigned i;uint8_t kind=game_memory_read_u8(&a->memory,s->object+12);int scaled=kind==3 || kind==0x0f;
    if(scaled){if(!scaled_move(s))return 0;}
    else if(follows_anchor){
        GameAddress anchor=(uint32_t)s->state->cell_x;
        if(anchor && get(a,anchor+0x14)==(uint32_t)s->state->cell_z){float p[3];
            if(!presentation_target_position(&a->memory,anchor,s->pose->position,p))return 0;
            s->pose->position[0]=p[0];s->pose->position[2]=p[2];
        }
        if(get(a,table+0xa4)!=0x5368b0 || !projectile_move_program(&a->memory,s->state,s->pose,0x5368b0,0))return 0;
    }else if(!force_and_move(s,tick)){fprintf(stderr,"native projectile failed: movement object=%08x\n",s->object);return 0;}
    if(!gameplay_geometry_world_bounds_values(s->pose->radius,s->pose->position))return reset(s);
    if(get(a,table+0xb4u)!=0x42f9d0u && get(a,table+0xb4u)!=0x548660u && get(a,table+0xb4u)!=0x48ee20u)return 0;
    if(get(a,table+0xb4u)==0x42f9d0u && projectile_expired(&a->memory,s->state,tick))return reset(s);
    if(get(a,table+0xb4u)==0x48ee20u && signed_word(get(a,s->object+0xec)+get(a,s->object+0xdc))<signed_word(tick))return reset(s);
    game_list_detach_node(&a->memory,s->object,4,8);
    if(a->memory.fault || get(a,table+0x78u)!=0x42fc10u || !gather(s)){fprintf(stderr,"native projectile failed: gather object=%08x\n",s->object);return 0;}
    record=get(a,s->object+0xb0);
    while(record && !a->memory.fault){GameAddress other=get(a,record+12);float point[3];uint32_t velocity[3],zero[3]={0,0,0};
        for(i=0;i<3;i++)point[i]=real(a,record+16+i*4);
        if(scaled)memset(velocity,0,12);else memcpy(velocity,s->state->velocity,12);
        if(!game_level_contact_response(a,other,s->object,get(a,get(a,other)+0x2c),velocity,point)){
            fprintf(stderr,"native projectile target contact failed: object=%08x other=%08x kind=%02x method=%08x\n",
                s->object,other,game_memory_read_u8(&a->memory,other+12),get(a,get(a,other)+0x2c));return 0;}
        if(!game_level_contact_response(a,s->object,other,get(a,table+0x2c),zero,point)){
            fprintf(stderr,"native projectile source contact failed: object=%08x other=%08x method=%08x\n",s->object,other,get(a,table+0x2c));return 0;}
        if(!s->pose->properties[0])return !a->memory.fault;
        record=get(a,record+4);
    }
    record=get(a,s->object+0xb0);if(record){if(!game_level_factory_release_pool_member(a,record))return 0;game_memory_write_u32(&a->memory,s->object+0xb0,0);}
    if(scaled){
        gameplay_spatial_insert(&a->memory,0x5f0800,s->object,(int32_t)s->pose->position[0]>>12,(int32_t)s->pose->position[2]>>12,(s->pose->flags&0x400u)!=0);
        if(kind==0x0f && get(a,0x5f0210+(s->pose->identity&63)*4)==get(a,0x5f0208) && !get(a,s->object+0xe8))return scaled_sound(a,s->object,1);
        return !a->memory.fault;
    }
    if(get(a,table+0xa0u)==0x48e680u){if(!game_level_mole_terrain_contact(a,s->object,normal,&hit))return 0;}
    else if(get(a,table+0xa0u)!=0x42f420u || !projectile_terrain_contact(&a->memory,s->pose,normal,&hit))return 0;
    if(hit){int ok=terrain(s,normal);if(!ok)fprintf(stderr,"native projectile failed: terrain object=%08x method=%08x\n",s->object,get(a,table+0x84));return ok;}
    if(follows_anchor){
        uint32_t deadline=s->state->target+32;int32_t signed_deadline,signed_tick;
        memcpy(&signed_deadline,&deadline,4);memcpy(&signed_tick,&tick,4);
        /*432e50's original comparison emits only while this deadline is
         *greater than the tick. Preserve that unusual direction exactly. */
        if(signed_deadline>signed_tick){const float zero[3]={0,0,0};
            if(!game_level_particle_emit(a,s->object,s->pose->position,zero,512.0f,0x583fe8,0,0,0))return 0;
            s->state->target=tick;
        }
    }
    gameplay_spatial_insert(&a->memory,0x5f0800,s->object,(int32_t)s->pose->position[0]>>12,(int32_t)s->pose->position[2]>>12,(s->pose->flags&0x400u)!=0);
    return !a->memory.fault;
}
int game_level_projectile_update(GameApplication *a,GameAddress object,GameAddress method){
    Shot s={a,object,presentation_instance_projectile(object),presentation_instance_pose(object)};float midpoint[3];GameAddress recipe;unsigned i;
    if(!s.state || !s.pose)return 0;
    if(method==0x4fc460 || method==0x4fc510 || method==0x50d050 || method==0x50d100){
        if(get(a,object)!=(method==0x50d050 || method==0x50d100?0x5752e0u:0x575138u) ||
           ((method==0x4fc460 || method==0x50d050) && !scaled_track(&s)))return 0;
        return flight(&s,0);
    }
    /*433650 is the fragment projectile's tail call to its motion method.
     *430820 runs ordinary flight, then emits only for a living shot whose
     *borrowed shooter is class28. Both keep the same projectile/pose owner. */
    if(method==0x433650u){
        GameAddress motion=get(a,get(a,object)+0x88u);
        return motion==0x42f300u && game_level_projectile_update(a,object,motion);
    }
    if(method!=0x42f300u && method!=0x430820u && method!=0x4318f0u && method!=0x431d40u && method!=0x432190u && method!=0x4329b0u && method!=0x432e50u && method!=0x4326b0u)return 0;
    if(method!=0x42f300u && method!=0x430820u && method!=0x432e50u && get(a,get(a,object)+0x88u)!=0x42f300u)return 0;
    if(!flight(&s,method==0x432e50u))return 0;
    if(method==0x430820u){
        const float zero[3]={0,0,0};
        if(!s.pose->properties[0] || !s.state->shooter)return !a->memory.fault;
        if(game_memory_read_u8(&a->memory,s.state->shooter+12)!=0x28)return !a->memory.fault;
        return !a->memory.fault && game_level_particle_emit(a,object,s.pose->position,zero,70.0f,
            0x584018,s.pose->angles[0],(uint16_t)(s.pose->angles[1]+512u),0);
    }
    if(method==0x4326b0u)return !s.pose->properties[0] || s.state->split!=1 || (get(a,0x5ac6ac)&3u)!=0 ||
        game_level_surface_particles(a,object,s.pose->position,0,0x629788);
    if(method==0x432190u)return !s.pose->properties[0] || !s.state->target || game_level_surface_particles(a,object,s.pose->position,0,0x6297c0);
    if(method==0x4318f0u || method==0x4329b0u){
        recipe=method==0x4318f0u?0x629788:0x6297d8;
        if(s.pose->properties[0] && (int32_t)(get(a,0x5ac6ac)-s.state->birth_tick)>1){
            if(!game_level_surface_particles(a,object,s.pose->position,0,recipe))return 0;
            for(i=0;i<3;i++)midpoint[i]=(float)(((long double)s.pose->position[i]+s.state->previous_position[i])*real(a,0x572358));
            if(!game_level_surface_particles(a,object,midpoint,0,recipe))return 0;
        }
        memcpy(s.state->previous_position,s.pose->position,12);
    }
    return !a->memory.fault;
}
