#include "projectile.h"
#include "gameplay_geometry.h"
#include "game_matrix.h"
#include "gameplay_state.h"
#include "gameplay_spatial.h"
#include "game_intrusive_list.h"
#include "effect_resource_pool.h"
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static float read_float(GameMemory *m, GameAddress a) {
    uint32_t v=game_memory_read_u32(m,a); float f; memcpy(&f,&v,4u); return f;
}
static uint32_t float_bits(float f) { uint32_t v; memcpy(&v,&f,4u); return v; }
/* These conversions reproduce the stored binary32 intermediates and retail
 * truncation, including its indefinite integer on invalid input. */
static int32_t truncate_value(long double f) {
    return !isfinite(f) || f>=2147483648.0L || f< -2147483648.0L ? INT32_MIN : (int32_t)f;
}
static void *field(Projectile *s,unsigned o) {
    if(o>=0xb4u && o<=0xbcu)return &s->velocity[(o-0xb4u)/4u];
    if(o>=0xc0u && o<=0xc8u)return &s->auxiliary_c0[(o-0xc0u)/4u];
    if(o==0xccu)return &s->angular_velocity[0];
    if(o==0xd0u)return &s->angular_velocity[2];
    switch(o){
    case 0xd4u:return &s->shooter;case 0xd8u:return &s->shooter_identity;
    case 0xdcu:return &s->birth_tick;case 0xe0u:return &s->recipe;
    case 0xe4u:return &s->cell_x;case 0xe8u:return &s->cell_z;
    case 0xecu:return &s->target;case 0x108u:return &s->target_identity;
    case 0x10cu:return &s->search_tick;case 0x110u:return &s->lifetime;
    case 0x114u:return &s->steering;case 0x118u:return &s->split;
    default:break;
    }
    if(o>=0xf0u && o<=0xf8u)return &s->target_position[(o-0xf0u)/4u];
    if(o>=0xfcu && o<=0x104u)return &s->previous_position[(o-0xfcu)/4u];
    return NULL;
}
static int transfer(Projectile *s,uint32_t o,void *b,size_t n,int write) {
    size_t i; unsigned char *bytes=b;
    if(!s || !b || o<0xb4u || o>s->projection_size || n>s->projection_size-o)return 0;
    for(i=0;i<n;++i)if(!field(s,(o+(unsigned)i)&~3u))return 0;
    for(i=0;i<n;++i){unsigned char *p=field(s,(o+(unsigned)i)&~3u);p+=(o+(unsigned)i)&3u;
        if(write)*p=bytes[i];else bytes[i]=*p;}
    return 1;
}
int projectile_projection_read(Projectile *s,uint32_t o,void *b,size_t n){return transfer(s,o,b,n,0);}
int projectile_projection_write(Projectile *s,uint32_t o,const void *b,size_t n){return transfer(s,o,(void *)b,n,1);}
static int32_t fragment_random(GameMemory *m,GameAddress scale){
    uint32_t word=effect_initialization_random(m,read_float(m,scale));int32_t value;
    memcpy(&value,&word,4);return value;
}
int projectile_fragment_begin(GameMemory *m,Projectile *s,PresentationPose *p,
        GameAddress recipe,GameAddress shooter,int primary,uint32_t generation){
    unsigned i;uint16_t heading=p->angles[0];long double speed,scale;uint32_t word;
    s->birth_tick=game_memory_read_u32(m,0x5ac6ac);s->recipe=recipe;s->shooter=shooter;
    s->shooter_identity=game_memory_read_u32(m,shooter+20);
    /*5366d0 has already placed/refreshed this pose. Consume that matrix;
     *rebuilding it here would change the original placement ordering. */
    memset(s->angular_velocity,0,3u*sizeof(uint16_t));
    speed=read_float(m,recipe+0x1c);
    for(i=0;i<3;i++)s->velocity[i]=(float)((long double)p->world_matrix[i*4+2]*speed);
    if(primary){
        uint16_t pitch=p->angles[1]&2047u;
        word=(uint32_t)fragment_random(m,0x57239c)+75u;
        {int32_t jitter;memcpy(&jitter,&word,4);scale=(long double)jitter*speed;}
        s->velocity[1]=(float)(scale*read_float(m,0x5723a0)+s->velocity[1]);
        scale=read_float(m,0x5722fc)-read_float(m,0x6298a8+pitch*4u);
        s->velocity[0]=(float)(scale*s->velocity[0]);s->velocity[2]=(float)(scale*s->velocity[2]);
        s->angular_velocity[0]=(uint16_t)fragment_random(m,0x5723a4);
        generation=2;
    }else{
        uint64_t bits=(uint64_t)game_memory_read_u32(m,0x5723b0)|((uint64_t)game_memory_read_u32(m,0x5723b4)<<32);
        double base;int32_t exponent;word=generation+1;memcpy(&exponent,&word,4);memcpy(&base,&bits,8);
        scale=powl((long double)base,(long double)exponent*read_float(m,0x572358));
        word=(uint32_t)fragment_random(m,0x572320)+10u;
        {int32_t jitter;memcpy(&jitter,&word,4);scale*=jitter;}
        scale*=speed;
        scale*=read_float(m,0x5723b8);
        s->velocity[0]=(float)((long double)read_float(m,0x6298a8+(heading&2047u)*4u)*scale);
        s->velocity[1]=(float)((long double)read_float(m,0x572364)*scale);
        s->velocity[2]=(float)(scale*read_float(m,0x6298a8+((heading+512u)&2047u)*4u));
        s->angular_velocity[1]=(uint16_t)fragment_random(m,0x5723a4);
    }
    s->angular_velocity[2]=(uint16_t)fragment_random(m,0x5723a8);
    memcpy(&s->cell_x,&generation,4);p->angles[1]=256;
    return !m->fault;
}
int projectile_fragment_force(GameMemory *m,Projectile *s){
    s->velocity[1]=(float)((long double)read_float(m,s->recipe+0x1c)*read_float(m,0x5723bc)+s->velocity[1]);
    return !m->fault;
}
int projectile_import(GameMemory *m,GameAddress a,Projectile *s){
    unsigned o;uint8_t type=game_memory_read_u8(m,a+12u);memset(s,0,sizeof(*s));
    /*Mole mines share only the ordinary projectile base. Their e4..10c
     *state and mutable-mesh handle remain in the owned legacy record. */
    s->projection_size=(type==2u || type==0x0cu)?0xe4u:(type==3u || type==0x0fu)?0x114u:type==0x0du?0xe8u:type==0x12u?0xfcu:(type==0x11u || type==0x13u)?0xecu:
        type==0x16u?0xf0u:(type==7u || type==8u || type==0x14u)?0x11cu:0x118u;
    for(o=0xb4u;o<s->projection_size;o+=4u){void *p=field(s,o);if(p){uint32_t v=game_memory_read_u32(m,a+o);memcpy(p,&v,4u);}}
    return !m->fault;
}
/*5483c0/5485a0: the e4..ec words are the launch point in this family.
 *Keep that role in the existing owner, using explicit bit transfers. */
int projectile_line_begin(GameMemory *m,Projectile *s,const PresentationPose *p,
        const float target[3],float strength){
    long double dx=(long double)target[0]-p->position[0],dz=(long double)target[2]-p->position[2];
    long double dy=(long double)target[1]-p->position[1],squared=dx*dx+dz*dz;
    long double duration=(long double)strength*read_float(m,0x577304);
    memcpy(&s->cell_x,p->position,4);memcpy(&s->cell_z,p->position+1,4);memcpy(&s->target,p->position+2,4);
    memcpy(s->target_position,target,12);
    s->velocity[1]=(float)((dy+squared*read_float(m,0x577300))/(dx+dz));
    s->velocity[2]=(float)(dz/duration);s->velocity[0]=(float)(dx/duration);
    s->angular_velocity[0]=(uint16_t)fragment_random(m,0x5772f8);
    s->angular_velocity[2]=(uint16_t)fragment_random(m,0x5772f8);
    s->angular_velocity[1]=(uint16_t)fragment_random(m,0x5772f8);
    return !m->fault;
}
int projectile_line_force(GameMemory *m,Projectile *s){
    s->velocity[0]=(float)((long double)s->velocity[0]*read_float(m,0x577308));
    s->velocity[2]=(float)((long double)s->velocity[2]*read_float(m,0x577308));return !m->fault;
}
int projectile_line_move(GameMemory *m,Projectile *s,PresentationPose *p){
    float origin[3];long double dx,dz;unsigned i;static const unsigned spin[3]={0,2,1};
    memcpy(origin,&s->cell_x,4);memcpy(origin+1,&s->cell_z,4);memcpy(origin+2,&s->target,4);
    p->position[0]=(float)((long double)p->position[0]+s->velocity[0]);
    p->position[2]=(float)((long double)p->position[2]+s->velocity[2]);
    dx=(long double)p->position[0]-origin[0];dz=(long double)p->position[2]-origin[2];
    p->position[1]=(float)((dx+dz)*s->velocity[1]-(dx*dx+dz*dz)*read_float(m,0x577300)+origin[1]);
    for(i=0;i<3;i++)p->angles[i]=(uint16_t)(p->angles[i]+s->angular_velocity[spin[i]])&2047u;
    return !m->fault && presentation_pose_build_world(m,p) && presentation_pose_transform_vertices(m,p,0);
}
int projectile_motion_blocked(GameMemory *m,const float position[3]){
    /*Retail deliberately wraps signed cell coordinates to eight bits. */
    uint32_t x=((uint32_t)truncate_value(position[0])>>9)&255u,z=((uint32_t)truncate_value(position[2])>>9)&255u;
    return (game_memory_read_u8(m,0x610520u+z*32u+(x>>3))&(1u<<(x&7u)))!=0;
}
int projectile_motion_target(GameMemory *m,const PresentationPose *p,float from[3],float to[3],float *strength,int *allowed){
    uint32_t bits;int32_t limit,jitter;uint16_t angle;
    long double value=(long double)read_float(m,0x5772d4)-p->radius;
    bits=(uint32_t)(uint64_t)(!isfinite(value) || value< -0x1p63L || value>=0x1p63L?INT64_MIN:(int64_t)value);
    memcpy(&limit,&bits,4);bits=effect_initialization_random(m,(float)limit);memcpy(&jitter,&bits,4);
    *strength=(float)((long double)jitter+p->radius);
    angle=(uint16_t)fragment_random(m,0x5772d8);
    to[0]=(float)((long double)read_float(m,0x6298a8u+((angle+512u)&2047u)*4u)*(*strength)+p->position[0]);
    to[2]=(float)((long double)read_float(m,0x6298a8u+(angle&2047u)*4u)*(*strength)+p->position[2]);
    to[1]=0;memcpy(from,p->position,12);
    from[1]=(float)((long double)p->position[1]+p->radius+read_float(m,0x5772dc));
    *allowed=!projectile_motion_blocked(m,to);return !m->fault;
}
int projectile_begin(GameMemory *m,Projectile *s,const PresentationPose *p,uint32_t tick,
        GameAddress recipe,GameAddress shooter,const ProjectileServices *services){
    unsigned i;uint8_t type=game_memory_read_u8(m,shooter+12u);(void)services;
    s->cell_x=s->cell_z=-1;s->steering=1.0f;s->birth_tick=tick;
    s->recipe=recipe;s->shooter=shooter;s->shooter_identity=game_memory_read_u32(m,shooter+20u);
    for(i=0;i<3u;++i){s->velocity[i]=(float)((long double)p->world_matrix[i*4u+2u]*read_float(m,recipe+0x1cu));s->angular_velocity[i]=0u;}
    if(type==0x27u || type==0x24u){
        s->target=game_memory_read_u32(m,shooter+0xf0u);
        s->target_identity=game_memory_read_u32(m,s->target+20u);
        s->lifetime=type==0x27u?0x00ffffffu:0x0fffffffu;
        s->search_tick=type==0x27u?0x00ffffffu:tick+32u;
        if(!presentation_target_position(m,s->target,p->position,s->target_position))return 0;
    }else{s->lifetime=96u;s->target=0;s->target_identity=0;s->search_tick=0;}
    memcpy(s->previous_position,p->position,12u);return !m->fault;
}

int projectile_begin_derived(GameMemory *m,Projectile *s,PresentationPose *p,
        const float position[3],uint16_t angle,float strength,const ProjectileServices *services){
    GameAddress world=game_memory_read_u32(m,0x58973cu),target;
    long double dx=(long double)position[0]-read_float(m,s->shooter+0x18u);
    long double dz=(long double)position[2]-read_float(m,s->shooter+0x20u);
    uint16_t shooter_angle=game_memory_read_u16(m,s->shooter+0x24u);
    long double side=dx*read_float(m,0x6298a8u+(((uint32_t)shooter_angle+512u)&2047u)*4u)+
        dz*read_float(m,0x6298a8u+(((uint32_t)shooter_angle+1024u)&2047u)*4u);
    long double scale=((long double)strength-read_float(m,0x572358u))*read_float(m,0x572364u);
    long double span=read_float(m,0x5722fcu)-fabsl(scale);
    long double lateral;float speed;unsigned axis;
    (void)services;s->split=0u;
    angle=(uint16_t)(angle+((!isnan(side) && side>read_float(m,0x5722f8u))?512u:(uint16_t)-512));
    s->velocity[1]=(float)(scale*read_float(m,0x572368u));
    lateral=read_float(m,0x572368u)-fabsl(s->velocity[1]);
    s->velocity[0]=(float)((read_float(m,0x6298a8u+((uint32_t)angle&2047u)*4u)*lateral)*span);
    s->velocity[2]=(float)((lateral*read_float(m,0x6298a8u+(((uint32_t)angle+512u)&2047u)*4u))*span);
    for(axis=0;axis<3u;axis++)p->position[axis]=(float)((long double)p->position[axis]+
        (long double)s->velocity[axis]*read_float(m,0x57236cu));
    if(m->fault || !presentation_pose_build_world(m,p) ||
       !presentation_pose_transform_vertices(m,p,0))return 0;
    speed=read_float(m,s->recipe+0x1cu);
    for(axis=0;axis<3u;axis++)s->velocity[axis]=(float)((long double)p->world_matrix[axis*4u+2u]*speed);
    s->birth_tick=game_memory_read_u32(m,0x5ac6acu);
    if(gameplay_state_entry_matches(m,world)){
        uint32_t identity,relation;
        target=game_memory_read_u32(m,world+0x3c4u);identity=game_memory_read_u32(m,target+20u);
        relation=game_memory_read_u32(m,0x5f0160u+(p->identity&63u)*24u+(identity&63u)*4u);
        if(!relation){
            s->target=target;s->target_identity=identity;s->search_tick=s->lifetime=0x00ffffffu;
            if(!presentation_target_position(m,target,p->position,s->target_position))return 0;
        }
    }
    s->steering=6.0f;return !m->fault;
}

int projectile_begin_motion(GameMemory *m,Projectile *s,PresentationPose *p,uint32_t tick,
        GameAddress recipe,GameAddress shooter,uint16_t pitch){
    uint32_t index=game_memory_read_u32(m,0x6308c0u);unsigned i;int32_t jitter;
    long double factor;float speed=read_float(m,recipe+0x1cu);
    s->birth_tick=tick;s->recipe=recipe;s->shooter=shooter;
    s->shooter_identity=game_memory_read_u32(m,shooter+20u);s->cell_x=s->cell_z=0;
    for(i=0;i<3u;i++)s->velocity[i]=(float)((long double)p->world_matrix[i*4u+2u]*speed);
    if(index==1024u)index=0u;
    jitter=truncate_value((long double)read_float(m,0x6308c8u+index*4u)*read_float(m,0x5772ccu));
    game_memory_write_u32(m,0x6308c0u,index+1u);
    s->velocity[1]=(float)(((long double)jitter*speed)*read_float(m,0x5772d0u)+s->velocity[1]);
    factor=(long double)read_float(m,0x6298a8u+((uint32_t)pitch&2047u)*4u)*read_float(m,0x5772ccu)+read_float(m,0x5772ccu);
    s->velocity[0]=(float)(factor*s->velocity[0]);s->velocity[2]=(float)(factor*s->velocity[2]);
    s->angular_velocity[0]=s->angular_velocity[1]=s->angular_velocity[2]=0u;
    memset(p->angles,0,sizeof(p->angles));return !m->fault;
}
int projectile_motion_update(GameMemory *m,Projectile *s,PresentationPose *p){
    int16_t angular,roll;long double level;float rounded_level;
    s->velocity[0]=(float)((long double)s->velocity[0]*read_float(m,0x5772e0u));
    s->velocity[2]=(float)((long double)s->velocity[2]*read_float(m,0x5772e0u));
    memcpy(&angular,&s->angular_velocity[0],2u);memcpy(&roll,&p->angles[2],2u);
    if(angular<120)s->angular_velocity[0]=(uint16_t)(s->angular_velocity[0]+2u);
    else s->angular_velocity[1]=1u;
    if(roll>64)s->angular_velocity[1]=0u;
    level=gameplay_geometry_terrain_height(m,p->position[0],p->position[2])+read_float(m,0x5772e4u);
    rounded_level=(float)level;
    if(isnan(level) || fabsl((long double)p->position[1]-level)<read_float(m,0x5772e8u))s->velocity[1]=0.0f;
    else s->velocity[1]=(float)((long double)s->velocity[1]+(long double)read_float(m,s->recipe+0x1cu)*
        read_float(m,!(p->position[1]>rounded_level)?0x5772f0u:0x5772f4u));
    return !m->fault;
}
int projectile_motion_post(GameMemory *m,Projectile *s,PresentationPose *p){
    float previous[3];unsigned i;uint32_t x,z,mask;
    memcpy(previous,p->position,sizeof(previous));
    for(i=0;i<3u;i++)p->position[i]=(float)((long double)p->position[i]+s->velocity[i]);
    /* Retail deliberately narrows each signed cell coordinate to eight bits. */
    x=((uint32_t)truncate_value(p->position[0])>>9u)&255u;
    z=((uint32_t)truncate_value(p->position[2])>>9u)&255u;
    mask=game_memory_read_u8(m,0x610520u+z*32u+(x>>3u))&(1u<<(x&7u));
    if(mask)memcpy(p->position,previous,sizeof(previous));
    p->angles[0]=(uint16_t)((p->angles[0]+s->angular_velocity[0])&2047u);
    p->angles[1]=(uint16_t)((p->angles[1]+s->angular_velocity[2])&2047u);
    p->angles[2]=(uint16_t)((p->angles[2]+s->angular_velocity[1])&2047u);
    return !m->fault && presentation_pose_build_world(m,p) && presentation_pose_transform_vertices(m,p,0);
}
int projectile_motion_settle(GameMemory *m,Projectile *s,PresentationPose *p){
    p->position[1]=(float)(gameplay_geometry_terrain_height(m,p->position[0],p->position[2])+read_float(m,0x5772e4u));
    s->velocity[1]=0.0f;return !m->fault;
}
/*430110: class13's rolling/bouncing force. Its legacy e4/e8 words are
 *bounce strength and count here; reuse their existing owner without aliasing
 *an integer as a float or introducing another synchronized record. */
int projectile_bounce_force(GameMemory *m,Projectile *s,PresentationPose *p){
    float x,z,stored_height,bounce;long double height,absolute,strength,random;
    uint32_t index,word;int32_t jitter;
    s->velocity[0]=(float)((long double)s->velocity[0]*read_float(m,0x572318));
    s->velocity[2]=(float)((long double)s->velocity[2]*read_float(m,0x572318));
    p->angles[0]=(uint16_t)((p->angles[0]+184u)&2047u);p->angles[1]=p->angles[2]=0;
    x=(float)((long double)p->position[0]+s->velocity[0]);z=(float)((long double)p->position[2]+s->velocity[2]);
    height=gameplay_geometry_terrain_height(m,x,z)+read_float(m,0x57231c);stored_height=(float)height;
    if(height>p->position[1]){
        index=game_memory_read_u32(m,0x6308c0);if(index==1024)index=0;
        if(index>1024 || m->fault)return 0;
        game_memory_write_u32(m,0x6308c0,index+1);
        random=(long double)read_float(m,0x6308c8+index*4)*read_float(m,0x572320);
        /*Original ftol converts to64 bits and returns its low32, including
         *the indefinite result. Bound the conversion before using C casts. */
        word=(uint32_t)(uint64_t)(!isfinite(random) || random< -0x1p63L || random>=0x1p63L?INT64_MIN:(int64_t)random);
        memcpy(&jitter,&word,4);memcpy(&bounce,&s->cell_x,4);
        absolute=fabsl(s->velocity[1]);strength=bounce>absolute?absolute:bounce;
        s->velocity[1]=(float)(strength+jitter);
        bounce=(float)((long double)bounce*read_float(m,0x572324));memcpy(&s->cell_x,&bounce,4);
        word=(uint32_t)s->cell_z+1;memcpy(&s->cell_z,&word,4);p->position[1]=stored_height;
    }else s->velocity[1]=(float)((long double)s->velocity[1]-read_float(m,0x572328));
    return !m->fault;
}
int projectile_motion_contact(GameMemory *m,PresentationPose *pose,GameAddress other,GameAddress point,
        const ProjectileMotionServices *services){
    int32_t health;memcpy(&health,&pose->properties[0],4u);
    if(health<=0 || game_memory_read_u8(m,other+12u)==0x12u)return !m->fault;
    return !m->fault && services && services->impact && services->reset &&
        services->impact(services->context,other,point) && services->reset(services->context);
}
int projectile_motion_advance(GameMemory *m,GameAddress object,Projectile *state,PresentationPose *pose,
        const ProjectileMotionServices *services){
    float previous[3],normal[3];GameAddress record;unsigned visits=0;int hit;
    if(!services || !services->reset || !services->gather || !services->peer_contact)return 0;
    memcpy(previous,pose->position,sizeof(previous));
    if(!projectile_motion_update(m,state,pose) || !projectile_motion_post(m,state,pose))return 0;
    if(!gameplay_geometry_world_bounds_values(pose->radius,pose->position))return services->reset(services->context);
    game_list_detach_node(m,object,4u,8u);
    if(m->fault || !services->gather(services->context,previous))return 0;
    for(record=game_memory_read_u32(m,object+0xb0u);record && !m->fault;record=game_memory_read_u32(m,record+4u)){
        GameAddress other=game_memory_read_u32(m,record+12u);
        if(++visits>65536u || !services->peer_contact(services->context,other,state->velocity,record+16u) ||
           !projectile_motion_contact(m,pose,other,record+16u,services))return 0;
        if(!pose->properties[0])return !m->fault;
    }
    record=game_memory_read_u32(m,object+0xb0u);
    if(record){EffectResourcePool *pool=effect_resource_pool_find(0x631910u);
        if(!pool || effect_resource_pool_return_chain(m,pool,record)!=1)return 0;
        game_memory_write_u32(m,object+0xb0u,0u);
    }
    if(!presentation_terrain_contact(m,pose,normal,&hit))return 0;
    if(hit){if(!projectile_motion_settle(m,state,pose))return 0;if(!pose->properties[0])return !m->fault;}
    gameplay_spatial_insert(m,0x5f0800u,object,
        (int32_t)floor((double)truncate_value(pose->position[0])/4096.0),
        (int32_t)floor((double)truncate_value(pose->position[2])/4096.0),(pose->flags&0x400u)!=0u);
    return !m->fault;
}
static int16_t heading(GameMemory *m,float x,float z){
    long double scale=read_float(m,0x572310u),index; int32_t at; uint16_t angle;
    if(fabsl(x)<fabsl(z)){
        index=(long double)x*scale/z+scale;at=truncate_value(index);
        angle=game_memory_read_u16(m,0x62e8b0u+(uint32_t)at*2u);
        return (int16_t)(((int32_t)float_bits(z)>0)?(angle&2047u):(uint16_t)(angle+1024u));
    }
    angle=(int32_t)float_bits(x)>0?512u:1536u;
    index=(long double)z*scale/x+scale;at=truncate_value(index);
    return (int16_t)(angle-game_memory_read_u16(m,0x62e8b0u+(uint32_t)at*2u));
}
GameAddress projectile_search_cell(GameMemory *m,const Projectile *s,const PresentationPose *p,
        int32_t x,int32_t z,GameAddress best,float *distance,const ProjectileServices *services){
    GameAddress candidate;float fallback=FLT_MAX;
    if(x<0 || z<0 || x>=32 || z>=32)return best;
    if(!distance)distance=&fallback;
    candidate=game_memory_read_u32(m,0x5f0804u+(uint32_t)(x*32+z)*12u);
    while(candidate && !m->fault){
        uint32_t flags=game_memory_read_u32(m,candidate+16u),mask;
        float delta[3],square_x,squared,tolerance=7.0f,point[3];unsigned i;int visible;
        if(best){mask=game_memory_read_u32(m,s->recipe+0x40u);
            if((game_memory_read_u32(m,best+16u)&mask)>(flags&mask))goto next;}
        /* At least one of these target categories must be present. */
        if(!(flags&0x18040u))goto next;
        if(game_memory_read_u32(m,0x5f0160u+(p->identity&63u)*24u+
            (game_memory_read_u32(m,candidate+20u)&63u)*4u))goto next;
        for(i=0;i<3u;++i){point[i]=read_float(m,candidate+24u+i*4u);delta[i]=(float)((long double)point[i]-p->position[i]);}
        square_x=(float)((long double)delta[0]*delta[0]);
        squared=(float)(((long double)delta[1]*delta[1]+(long double)delta[2]*delta[2])+square_x);
        if(!(squared<*distance))goto next;
        if((float_bits(delta[2])&0x7fffffffu)==0u)delta[2]=0.1f;
        if(!(flags&0x10000u)){
            int difference=(int)heading(m,delta[0],delta[2])-(int)(int16_t)p->angles[0];
            if((int32_t)float_bits(squared)<(int32_t)0x4b095440u)
                tolerance=(float)((long double)tolerance*read_float(m,0x572350u));
            if(abs(difference)>(int16_t)truncate_value((long double)tolerance*read_float(m,0x572354u)))goto next;
            {
                float horizontal=(float)gameplay_geometry_square_root((float)((long double)delta[2]*delta[2]+square_x));
                int16_t pitch=heading(m,delta[1],horizontal);
                if(pitch>512)pitch=(int16_t)-(int16_t)(pitch-2048);
                if(abs((int)pitch-(int)(int16_t)p->angles[1])>102)goto next;
            }
        }
        point[1]=(float)((long double)point[1]+read_float(m,candidate+0x30u));
        if(!services || !services->visible || !services->visible(services->context,p->position,point,&visible))return best;
        if(visible){best=candidate;*distance=squared;}
next: candidate=game_memory_read_u32(m,candidate+4u);
    }
    return best;
}

int projectile_home(GameMemory *m,Projectile *s,const PresentationPose *p,uint32_t tick,const ProjectileServices *services){
    float delta[3],normal,step,speed,squared;unsigned i;
    if((int32_t)(tick-s->birth_tick)>(int32_t)s->lifetime)return 1;
    if(s->target && game_memory_read_u32(m,s->target+20u)!=s->target_identity){s->target=0;s->target_identity=0;}
    if((int32_t)(s->search_tick+8u)<=(int32_t)tick &&
            (!s->target || !(game_memory_read_u8(m,s->target+18u)&1u))){
        int32_t x=truncate_value(p->position[0])>>12,z=truncate_value(p->position[2])>>12;
        int angle=(int16_t)p->angles[0],dx=0,dz=1,diagonal=0,n;
        GameAddress best=0;float distance=FLT_MAX;
        s->search_tick=tick;s->cell_x=x;s->cell_z=z;
        if(angle>128 && angle<=384){dx=1;diagonal=1;}
        else if(angle>384 && angle<=640){dx=1;dz=0;}
        else if(angle>640 && angle<=896){dx=1;dz=-1;diagonal=1;}
        else if(angle>896 && angle<=1152){dz=-1;}
        else if(angle>1152 && angle<=1408){dx=-1;dz=-1;diagonal=1;}
        else if(angle>1408 && angle<=1664){dx=-1;dz=0;}
        else if(angle>1664 && angle<=1920){dx=-1;diagonal=1;}
        for(n=0;n<4 && !m->fault;++n){
            best=projectile_search_cell(m,s,p,x,z,best,&distance,services);
            if(diagonal){
                best=projectile_search_cell(m,s,p,x+dx,z,best,&distance,services);
                best=projectile_search_cell(m,s,p,x,z+dz,best,&distance,services);
                best=projectile_search_cell(m,s,p,x,z,best,&distance,services);
                best=projectile_search_cell(m,s,p,x+dx,z,best,&distance,services);
            }else if(!dx){
                best=projectile_search_cell(m,s,p,x+1,z,best,&distance,services);
                best=projectile_search_cell(m,s,p,x-1,z,best,&distance,services);
            }else if(!dz){
                best=projectile_search_cell(m,s,p,x,z+1,best,&distance,services);
                best=projectile_search_cell(m,s,p,x,z-1,best,&distance,services);
            }
            x+=dx;z+=dz;
        }
        s->target=best;s->target_identity=best?game_memory_read_u32(m,best+20u):0u;
        if(best && !presentation_target_position(m,best,p->position,s->target_position))return 0;
    }
    if(!s->target)return !m->fault;
    if(!presentation_target_position(m,s->target,p->position,s->target_position))return 0;
    for(i=0;i<3u;++i)delta[i]=(float)((long double)s->target_position[i]-p->position[i]);
    normal=(float)((long double)read_float(m,0x5722fcu)/(fabsl(delta[0])+fabsl(delta[1])+fabsl(delta[2])));
    squared=(float)(((long double)delta[0]*delta[0]+(long double)delta[2]*delta[2])+(long double)delta[1]*delta[1]);
    step=s->steering;
    if(squared<read_float(m,0x572340u))step=(float)((long double)step*read_float(m,0x572344u));
    speed=read_float(m,s->recipe+0x1cu);
    for(i=0;i<3u;++i){
        long double desired=(long double)delta[i]*normal*speed;
        float stored=(float)desired;
        if(desired>s->velocity[i])s->velocity[i]=(float)((long double)s->velocity[i]+step);
        else if(s->velocity[i]>stored)s->velocity[i]=(float)((long double)s->velocity[i]-step);
    }
    return !m->fault;
}
void projectile_move(Projectile *s,PresentationPose *p,float multiplier){
    unsigned i;for(i=0;i<3u;++i)p->position[i]=(float)((long double)s->velocity[i]*multiplier+p->position[i]);
}
int projectile_orient(GameMemory *m,const Projectile *s,PresentationPose *p){
    float squared=(float)(((long double)s->velocity[2]*s->velocity[2]+(long double)s->velocity[1]*s->velocity[1])+
        (long double)s->velocity[0]*s->velocity[0]);
    long double length=gameplay_geometry_square_root(squared),ratio;
    float speed=(float)length,value;uint16_t pitch,yaw;int32_t index;
    if(length==read_float(m,0x5722f8u) || isnan(length))return !m->fault;
    ratio=(long double)s->velocity[1]/speed;value=(float)ratio;
    if(ratio<read_float(m,0x572300u)){
        index=truncate_value(((long double)read_float(m,0x572300u)-value)*read_float(m,0x572304u));
        pitch=game_memory_read_u16(m,0x62b8a8u+(uint32_t)index*2u);
    }else if((int32_t)float_bits(value)>(int32_t)0x3f666666u){
        index=truncate_value(((long double)value-read_float(m,0x572308u))*read_float(m,0x572304u));
        pitch=(uint16_t)-game_memory_read_u16(m,0x62b8a8u+(uint32_t)index*2u);
    }else{
        index=truncate_value((long double)value*read_float(m,0x57230cu)+read_float(m,0x572310u));
        pitch=game_memory_read_u16(m,0x62f8b8u+(uint32_t)index*2u);
    }
    if(pitch==512u || pitch==(uint16_t)-512){p->angles[0]=0;p->angles[1]=512;return !m->fault;}
    ratio=(long double)s->velocity[0]/((long double)read_float(m,0x6298a8u+(((uint32_t)(int16_t)pitch+512u)&2047u)*4u)*speed);
    value=(float)ratio;
    if(ratio>=read_float(m,0x572314u))value=ratio>read_float(m,0x5722fcu)?read_float(m,0x5722fcu):value;
    else value=-1.0f;
    if((int32_t)float_bits(value)>(int32_t)0x3f666666u){
        index=truncate_value(((long double)value-read_float(m,0x572308u))*read_float(m,0x572304u));
        yaw=game_memory_read_u16(m,0x62b8a8u+(uint32_t)index*2u);
    }else if(float_bits(value)>0xbf666666u){
        index=truncate_value(((long double)read_float(m,0x572300u)-value)*read_float(m,0x572304u));
        yaw=(uint16_t)-game_memory_read_u16(m,0x62b8a8u+(uint32_t)index*2u);
    }else{
        index=truncate_value((long double)value*read_float(m,0x572310u)+read_float(m,0x572310u));
        yaw=game_memory_read_u16(m,0x62f8b8u+(uint32_t)index*2u);
    }
    p->angles[0]=float_bits(s->velocity[2])>0x80000000u?(uint16_t)(1024u-yaw):(yaw&2047u);
    p->angles[1]=pitch;return !m->fault;
}

int projectile_move_program(GameMemory *memory,Projectile *state,PresentationPose *pose,GameAddress method,int laser){
    unsigned i;
    if(method==0x5368b0u || method==0x4335e0u){
        static const unsigned axis[3]={0,2,1};
        projectile_move(state,pose,1);
        for(i=0;i<3;i++){
            uint16_t angle=(uint16_t)(pose->angles[i]+state->angular_velocity[axis[i]])&2047;
            if(method==0x5368b0u && i && angle>1024)angle=(uint16_t)(angle-2048);
            pose->angles[i]=angle;
        }
    }else if(method==0x4318b0u || method==0x432100u || method==0x432970u){
        if(!projectile_orient(memory,state,pose))return 0;
        for(i=0;i<3;i++){
            float scale=method==0x432100u?read_float(memory,
                state->target?0x572374u:0x572324u):1;
            pose->position[i]=(float)((long double)state->velocity[i]*scale+pose->position[i]);
        }
    }else return 0;
    return !memory->fault && (laser ?
        (presentation_pose_build_laser_world(memory,pose) && presentation_pose_transform_laser_vertices(memory,pose)) :
        (presentation_pose_build_world(memory,pose) && presentation_pose_transform_vertices(memory,pose,0)));
}
/*432350/432410/432440: the existing split word is the delayed activation
 *latch here. Target references remain borrowed; there is only one motion owner. */
int projectile_begin_delayed(GameMemory *m,Projectile *s,const PresentationPose *p,const ProjectileServices *services){
    GameAddress root=game_memory_read_u32(m,0x58973c),target;uint32_t identity;(void)services;
    if(gameplay_state_entry_matches(m,root)){
        target=game_memory_read_u32(m,root+0x3c4);identity=game_memory_read_u32(m,target+20);
        if(!game_memory_read_u32(m,0x5f0160+(p->identity&63u)*24+(identity&63u)*4)){
            s->target=target;s->target_identity=identity;s->search_tick=s->lifetime=0x00ffffff;
            if(!presentation_target_position(m,target,p->position,s->target_position))return 0;
        }
    }
    s->steering=6;s->split=0;return !m->fault;
}
int projectile_delayed_force(GameMemory *m,Projectile *s,PresentationPose *p,uint32_t tick,const ProjectileServices *services){
    if(s->split)return projectile_home(m,s,p,tick,services);
    s->velocity[1]=(float)((long double)s->velocity[1]+read_float(m,0x572378));
    return !m->fault && projectile_orient(m,s,p);
}
int projectile_delayed_move(GameMemory *m,Projectile *s,PresentationPose *p,uint32_t tick){
    uint32_t deadline=s->birth_tick+32u;int32_t end,now;unsigned i;static const unsigned axis[3]={0,2,1};
    memcpy(&end,&deadline,4);memcpy(&now,&tick,4);
    if(end<now && !s->split){
        s->split=1;
        for(i=0;i<3;i++){
            float sign=read_float(m,fragment_random(m,0x572364)?0x572314:0x5722fc);
            uint32_t bits=(uint32_t)fragment_random(m,i==2?0x572380:0x57237c)+(i==2?25u:30u);int32_t amount;
            memcpy(&amount,&bits,4);
            s->velocity[axis[i]]=(float)((long double)amount*sign+s->velocity[axis[i]]);
        }
    }
    return projectile_move_program(m,s,p,s->split?0x4318b0:0x5368b0,0);
}
int projectile_expired(GameMemory *memory,const Projectile *state,uint32_t tick){
    uint32_t deadline=state->birth_tick+game_memory_read_u32(memory,state->recipe+0x24u);
    return (int32_t)deadline<(int32_t)tick;
}
int projectile_impact_request(GameMemory *memory,const Projectile *state,
        const float point[3],GameAddress attachment,int terrain,
        EffectInitialization *request,uint16_t *selector){
    unsigned axis;
    memset(request,0,sizeof(*request));
    memcpy(request->position,point,sizeof(request->position));
    if(terrain){
        long double height=gameplay_geometry_terrain_height(memory,point[0],point[2]);
        long double difference=height-(long double)point[1];
        float stored_difference=(float)difference,normal[3];
        attachment=0;
        if(difference>0.0L){
            long double dot;float stored_dot;
            if(!gameplay_geometry_terrain_normal_values(memory,normal,point[0],point[2]))return 0;
            dot=((long double)state->velocity[0]*normal[0]+
                 (long double)state->velocity[1]*normal[1])+
                 (long double)state->velocity[2]*normal[2];
            stored_dot=(float)dot;
            if(dot<0.0L || isnan(dot)){
                long double displacement=-((long double)normal[1]*stored_difference)/stored_dot;
                for(axis=0;axis<3u;++axis)
                    request->position[axis]=(float)((long double)point[axis]-
                        (long double)state->velocity[axis]*displacement);
            }
        }
    }else if(!attachment || !(game_memory_read_u32(memory,attachment+16u)&1u))attachment=0;
    request->attachment=attachment;
    request->subtype=game_memory_read_u8(memory,state->shooter+12u);
    *selector=game_memory_read_u16(memory,state->recipe+0x2cu);
    request->scale[0]=request->scale[1]=read_float(memory,state->recipe+0x3cu);
    request->contact_armed=game_memory_read_u32(memory,state->recipe+0x30u);
    request->loop_control=game_memory_read_u32(memory,state->recipe+0x34u);
    request->placement_flags=2u;request->mark_kind=-1;
    return !memory->fault;
}
int projectile_terrain_contact(GameMemory *memory,const PresentationPose *pose,
        float normal[3],int *hit){
    unsigned corner,axis;
    *hit=0;
    /* Bottom four, then top four: the first buried corner selects the normal. */
    for(corner=0;corner<8u && !memory->fault;++corner){
        float point[3],world[3];
        for(axis=0;axis<3u;++axis){
            unsigned positive=axis==0u?(corner&2u):axis==1u?(corner&4u):(corner&1u);
            point[axis]=positive?pose->dimensions[axis]:-pose->dimensions[axis];
        }
        if(corner<4u){
            for(axis=0;axis<3u;++axis){const float *row=pose->world_matrix+axis*4u;
                world[axis]=(float)((((long double)row[0]*point[0]+
                    (long double)row[1]*point[1])+(long double)row[2]*point[2])+row[3]);
            }
        }else game_matrix_transform_point_values(world,pose->world_matrix,point);
        if(gameplay_geometry_terrain_height(memory,world[0],world[2])>world[1]){
            if(memory->fault || !gameplay_geometry_terrain_normal_values(memory,normal,world[0],world[2]))return 0;
            *hit=1;return 1;
        }
    }
    return !memory->fault;
}
/* Projectiles stop at the first accepted contact, unlike the presentation
 * gatherer that prepends every contact. Preserve bucket and list order. */
static int presentation_normalize_contact(GameMemory *memory,float normal[3]){
    float length=(float)sqrtl((float)(((long double)normal[1]*normal[1]+(long double)normal[2]*normal[2])+
        (long double)normal[0]*normal[0]));unsigned axis;long double scale;
    if(!isfinite(length) || length==0.0f)return 0;
    if(length==1.0f)return 1;
    scale=(long double)read_float(memory,0x576594u)/length;
    for(axis=0;axis<3u;axis++)normal[axis]=(float)((long double)normal[axis]*scale);
    return !memory->fault;
}
int presentation_terrain_contact(GameMemory *memory,const PresentationPose *pose,float normal[3],int *hit){
    /* Retail samples X+/Y-/Z+ twice and omits X+/Y+/Z-. */
    static const unsigned corners[8]={0u,1u,4u,5u,2u,3u,5u,7u};
    float world[3],sum[3]={0,0,0};unsigned axis,bone,corner,hits=0u;
    *hit=0;
    /*536a50 puts its bottom-center sample before the upper four corners.
     *Unlike projectile contact, output is not published until the marker
     *test succeeds; a rejected marker set leaves the caller's normal alone. */
    for(corner=0;corner<9 && !memory->fault;corner++){
        float local[3];unsigned box=corner<4?corner:corner-1;
        for(axis=0;axis<3;axis++){unsigned positive=axis==0?(box&2):axis==1?(box&4):(box&1);
            local[axis]=positive?pose->dimensions[axis]:-pose->dimensions[axis];}
        if(corner==4)for(axis=0;axis<3;axis++)world[axis]=(float)((long double)pose->world_matrix[axis*4+1]*
            -pose->dimensions[1]+pose->world_matrix[axis*4+3]);
        else if(corner<4)for(axis=0;axis<3;axis++){const float *row=pose->world_matrix+axis*4;
            world[axis]=(float)(((long double)row[0]*local[0]+(long double)row[1]*local[1])+
                (long double)row[2]*local[2]+row[3]);}
        else game_matrix_transform_point_values(world,pose->world_matrix,local);
        if(gameplay_geometry_terrain_height(memory,world[0],world[2])>world[1]){*hit=1;break;}
    }
    if(memory->fault || !*hit)return !memory->fault;
    if(pose->vertex_count==0u)return gameplay_geometry_terrain_normal_values(memory,normal,world[0],world[2]) && presentation_normalize_contact(memory,normal);
    *hit=0;
    if((int16_t)pose->vertex_count<0)return !memory->fault;
    if((uint64_t)pose->hull+(uint64_t)pose->vertex_count*28u>UINT32_MAX)return 0;
    for(bone=0;bone<pose->vertex_count;bone++){
        float low[3],high[3],local[3],sample_normal[3];GameAddress record=pose->hull+bone*28u;
        for(axis=0;axis<3u;axis++){
            float centre=read_float(memory,record+axis*4u),extent=read_float(memory,record+12u+axis*4u);
            low[axis]=(float)((long double)extent-centre);high[axis]=(float)((long double)extent+centre);
        }
        for(corner=0;corner<8u;corner++){
            unsigned mask=corners[corner];
            for(axis=0;axis<3u;axis++)local[axis]=mask&(4u>>axis)?high[axis]:low[axis];
            if(corner==0u){
                for(axis=0;axis<3u;axis++){const float *row=pose->world_matrix+axis*4u;
                    world[axis]=(float)((((long double)row[0]*local[0]+(long double)row[1]*local[1])+
                        (long double)row[2]*local[2])+row[3]);}
            }else game_matrix_transform_point_values(world,pose->world_matrix,local);
            if(gameplay_geometry_terrain_height(memory,world[0],world[2])>world[1]){
                if(!gameplay_geometry_terrain_normal_values(memory,sample_normal,world[0],world[2]))return 0;
                for(axis=0;axis<3u;axis++)sum[axis]=(float)((long double)sample_normal[axis]+sum[axis]);
                ++hits;
            }
            if(memory->fault)return 0;
        }
    }
    if(hits){
        memcpy(normal,sum,12u);if(!presentation_normalize_contact(memory,normal))return 0;
        *hit=1;
    }
    return !memory->fault;
}
GameAddress projectile_find_contact(GameMemory *memory,GameAddress source,
        const Projectile *state,const PresentationPose *pose,const GameAddress heads[9],
        const ProjectileContactServices *services,float contact[3]){
    unsigned bucket;
    if(!services || !services->collision){memory->fault=1;return 0;}
    for(bucket=0;bucket<9u && !memory->fault;++bucket){
        GameAddress candidate=heads[bucket];
        while(candidate && !memory->fault){
            uint32_t flags=game_memory_read_u32(memory,candidate+16u);
            uint8_t type=game_memory_read_u8(memory,candidate+12u);
            int hit=0,eligible=candidate!=source && candidate!=state->shooter &&
                !(flags&0x00200020u) && (type==9u || type==12u || !(flags&0x400u));
            if(eligible && type==9u && game_memory_read_u8(memory,source+12u)==0x12u)
                eligible=game_memory_read_u32(memory,0x5f0160u+(pose->identity&63u)*24u+
                    (game_memory_read_u32(memory,candidate+20u)&63u)*4u)==0u;
            if(eligible && type==0x40u && (game_memory_read_u8(memory,state->shooter+0xd8u)&1u))
                eligible=game_memory_read_u32(memory,0x5f0160u+(pose->identity&63u)*24u+
                    (game_memory_read_u32(memory,candidate+20u)&63u)*4u)==0u;
            if(eligible && type==0xa2u)
                eligible=game_memory_read_u32(memory,candidate+0xb8u)!=1u;
            if(memory->fault)return 0;
            if(eligible){
                if(!services->collision(services->context,candidate,source,contact,&hit)){
                    memory->fault=1;return 0;
                }
                if(hit)return candidate;
            }
            candidate=game_memory_read_u32(memory,candidate+4u);
        }
    }
    return 0;
}
