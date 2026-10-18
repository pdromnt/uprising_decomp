#include "vehicle_targeting.h"
#include "gameplay_geometry.h"
#include "gameplay_leaves.h"
#include "game_matrix.h"
#include "presentation_pose.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
static uint32_t get(GameMemory *m,GameAddress p){return game_memory_read_u32(m,p);}
static void put(GameMemory *m,GameAddress p,uint32_t v){game_memory_write_u32(m,p,v);}
static int32_t integer(uint32_t v){int32_t s;memcpy(&s,&v,4);return s;}
static int16_t short_value(uint16_t v){int16_t s;memcpy(&s,&v,2);return s;}
static float real(GameMemory *m,GameAddress p){uint32_t v=get(m,p);float f;memcpy(&f,&v,4);return f;}
static void store(GameMemory *m,GameAddress p,float f){uint32_t v;memcpy(&v,&f,4);put(m,p,v);}
static uint32_t trunc_word(long double v){return (uint32_t)(uint64_t)(!isfinite(v)||v< -0x1p63L||v>=0x1p63L?INT64_MIN:(int64_t)v);}
static uint32_t bits(float f){uint32_t v;memcpy(&v,&f,4);return v;}
static int octant_less(long double a,long double b){return isnan(a) || isnan(b) || a<b;}
uint16_t vehicle_targeting_octant(GameMemory *m,float x,float z){
    long double factor=real(m,0x571f28);
    if(integer(bits(x))>0){
        if(integer(bits(z))>0){
            if(octant_less((long double)x*factor,z))return 0;
            if(octant_less((long double)z*factor,x))return 512;
            return 256;
        }
        z=-z;
        if(octant_less((long double)z*factor,x))return 512;
        if(octant_less((long double)x*factor,z))return 1024;
        return 768;
    }
    if(bits(z)>0x80000000u){
        x=-x;z=-z;
        if(octant_less((long double)x*factor,z))return 1024;
        if(octant_less((long double)z*factor,x))return 1536;
        return 1280;
    }
    x=-x;
    if(octant_less((long double)z*factor,x))return 1536;
    if(octant_less((long double)x*factor,z))return 0;
    return 1792;
}
static void vector(GameMemory *m,GameAddress p,float out[3]){unsigned i;for(i=0;i<3;i++)out[i]=real(m,p+i*4);}
static uint32_t random_word(GameMemory *m,long double scale){
    uint32_t index=get(m,0x6308c0);if(index==1024)index=0;
    if(index>1023){m->fault=1;return 0;}put(m,0x6308c0,index+1);
    return trunc_word((long double)real(m,0x6308c8+index*4)*scale);
}
static long double weapon_range(GameMemory *m,GameAddress weapon){GameAddress motion=get(m,weapon+20);
    return (long double)integer(get(m,motion+0x24))*real(m,motion+0x1c);
}
static int friendly(GameMemory *m,GameAddress o,GameAddress target){
    return get(m,0x5f0160+(get(m,o+20)&63u)*24+(get(m,target+20)&63u)*4)!=0;
}
/*4107a0: the original octagonal planar distance, not Euclidean distance. */
static float planar(GameMemory *m,const float a[3],const float b[3]){
    long double dx=(long double)a[0]-b[0],dz=(long double)a[2]-b[2];float x=(float)dx,z=(float)dz;
    if(dx<real(m,0x571f14)||isnan(dx))x=-x;
    if(dz<real(m,0x571f14)||isnan(dz))z=-z;
    return (float)((z<x||isnan(z)||isnan(x))?(long double)z*real(m,0x571f18)+x:(long double)x*real(m,0x571f18)+z);
}
uint16_t vehicle_targeting_vector_angle(GameMemory *m,float x,float z,float scale){
    uint32_t index;uint16_t angle;
    if(fabsl(x)<fabsl(z)){
        index=trunc_word((long double)x*scale/z+scale);
        angle=game_memory_read_u16(m,0x62e8b0+index*2u);
        return integer(bits(z))>0?(angle&2047u):(uint16_t)(angle+1024u);
    }
    angle=integer(bits(x))>0?512u:1536u;
    index=trunc_word((long double)z*scale/x+scale);
    return (uint16_t)(angle-game_memory_read_u16(m,0x62e8b0+index*2u));
}
/*410d10: preserve the binary32 deltas, zero substitutions and lookup angles. */
int vehicle_targeting_pitch_limit(GameAddress method,int16_t *out){
    if(!out)return 0;
    switch(method){
    case 0x4073f0:case 0x4e8460:case 0x5521b0:case 0x408fe0:case 0x409300:case 0x408ca0:*out=-512;return 1;
    case 0x407400:case 0x4e8470:case 0x5521c0:case 0x408ff0:case 0x408cb0:*out=256;return 1;
    case 0x409310:*out=28;return 1;
    default:return 0;
    }
}
int vehicle_targeting_point_angles(GameMemory *m,GameAddress o,const float point[3],uint16_t *yaw,int16_t *pitch){
    float x=(float)((long double)point[0]-real(m,o+24)),z=(float)((long double)point[2]-real(m,o+32));
    float y=(float)((long double)point[1]-((get(m,o+16)&512u)?(long double)real(m,o+28)+real(m,o+0x1cc):real(m,o+28)));
    float horizontal=(float)sqrtl((float)((long double)x*x+(long double)z*z));uint16_t angle;
    if(!(bits(z)&0x7fffffffu))z=0.01f;
    if(!(bits(horizontal)&0x7fffffffu))horizontal=0.01f;
    *yaw=vehicle_targeting_vector_angle(m,x,z,real(m,0x571f34))&2047u;
    angle=vehicle_targeting_vector_angle(m,y,horizontal,real(m,0x571f34))&2047u;
    *pitch=short_value(angle>1024u?(uint16_t)(2048u-angle):(uint16_t)(0u-angle));return !m->fault;
}
/*410560/4105e0 signed-short circular interval and midpoint. */
static int in_arc(uint16_t value,uint16_t left,uint16_t right){
    int16_t lo=short_value(left),hi=short_value(right),at=short_value(value);int span;
    if(hi<lo){int16_t tmp=lo;lo=hi;hi=tmp;}
    span=short_value((uint16_t)(hi-lo));if(span<0)span=-span;
    if(short_value((uint16_t)span)>1024)span=short_value((uint16_t)(2048-span));
    return (int)lo-hi+2048<=span?!(lo<at&&hi>at):(lo<=at&&hi>=at);
}
static uint16_t midpoint(uint16_t first,uint16_t second){
    int a=short_value(first),b=short_value(second),span=abs(a-b),sign;
    if(short_value((uint16_t)span)>1024)span=short_value((uint16_t)(2048-span));
    if(b<a)sign=b+1024>=a?-1:1;else sign=a+1024>=b?1:-1;
    return (uint16_t)(b+(sign==1?short_value((uint16_t)span)/2:-short_value((uint16_t)span)/2))&2047u;
}
int vehicle_targeting_aim(GameMemory *m,GameAddress o,uint16_t yaw,int16_t pitch,const VehicleTargetServices *s){
    GameAddress table=get(m,o);int16_t yaw_rate,pitch_rate;int cy=short_value(game_memory_read_u16(m,o+0x148)),cp=short_value(game_memory_read_u16(m,o+0x14a));
    int dy=abs(short_value(yaw)-cy),dp=abs(pitch-cp);float turn=0,tilt=0;
    if(short_value((uint16_t)dy)>1024)dy=short_value((uint16_t)(2048-dy));
    if(!s->rate || !s->rate(s->context,o,get(m,table+0x1d8),&pitch_rate) || !s->rate(s->context,o,get(m,table+0x1d4),&yaw_rate))return 0;
    if(pitch<cp && pitch_rate<short_value((uint16_t)dp))tilt=-1;
    else if(cp<pitch && pitch_rate<short_value((uint16_t)dp))tilt=1;
    if(yaw_rate<short_value((uint16_t)dy)){
        if(cy<=1024)turn=short_value(yaw)>cy && short_value(yaw)-cy<1024?-1:1;
        else turn=short_value(yaw)<cy && cy-short_value(yaw)<1024?1:-1;
    }
    if(real(m,o+0x1f0)!=tilt || real(m,o+0x1f4)!=turn){
        if(!s->directions || !s->directions(s->context,o,get(m,table+0x1e4),turn,tilt))return 0;
    }
    if(turn==0 && tilt==0){
        if(game_memory_read_u16(m,o+0x148)!=yaw || short_value(game_memory_read_u16(m,o+0x14a))!=pitch){
            if(get(m,table+0x1e8)!=0x4e84d0)return 0;
            game_memory_write_u16(m,o+0x148,yaw);game_memory_write_u16(m,o+0x14a,(uint16_t)pitch);
            put(m,o+0x1ec,1);
            game_memory_write_u8(m,o+0xd8,game_memory_read_u8(m,o+0xd8)|8u);
        }
    }
    return !m->fault;
}
int vehicle_targeting_idle(GameMemory *m,GameAddress o,const VehicleTargetServices *s){
    int16_t low,pitch;uint16_t yaw;int renew;
    if(!game_terrain_cell_presence(m,o,0))return !m->fault;
    if(!s->limit || !s->limit(s->context,o,get(m,get(m,o)+0x1cc),&low))return 0;
    low=short_value((uint16_t)trunc_word((long double)low*real(m,0x571048)));
    renew=random_word(m,real(m,0x57104c))==1;
    yaw=game_memory_read_u16(m,o+0x210);pitch=short_value(game_memory_read_u16(m,o+0x212));
    if(renew || short_value(yaw)<0 || short_value(yaw)>2048){yaw=(uint16_t)random_word(m,real(m,0x571050));game_memory_write_u16(m,o+0x210,yaw);}
    if(renew || pitch<low || pitch>5){pitch=short_value((uint16_t)(random_word(m,5-low)+(uint16_t)low));game_memory_write_u16(m,o+0x212,(uint16_t)pitch);}
    return !m->fault && vehicle_targeting_aim(m,o,yaw,pitch,s);
}
static float distance(GameMemory *m,GameAddress o,const float target[3]){
    long double dx=(long double)real(m,o+24)-target[0],dy=(long double)target[1]-real(m,o+28),dz=(long double)real(m,o+32)-target[2];
    return (float)sqrtl((float)((dz*dz+dx*dx)+dy*dy));
}
static int secondary(GameMemory *m,GameAddress o,const VehicleTargetServices *s){
    GameAddress target=get(m,o+0xf0),weapon=get(m,o+0x220);uint32_t roll;uint8_t type;float body[12],head[12],mount[12],middle[12],world[12];uint16_t angles[3];unsigned i;
    if(game_memory_read_u8(m,o+12)!=0x21 || !get(m,weapon+12))return !m->fault;
    roll=random_word(m,real(m,0x57105c));type=game_memory_read_u8(m,target+12);
    if(!(roll==0 && type!=0x40) && !(type==0x27 && !(roll&~7u)) && !(type==0x26 && !(roll&~15u)))return !m->fault;
    for(i=0;i<12;i++){body[i]=real(m,o+0x80+i*4);head[i]=real(m,o+0x180+i*4);mount[i]=real(m,o+0x150+i*4);}
    game_matrix_compose_values(middle,head,mount);game_matrix_compose_values(world,body,middle);
    angles[0]=game_memory_read_u16(m,o+0x148);angles[1]=game_memory_read_u16(m,o+0x14a);angles[2]=game_memory_read_u16(m,o+0x28);
    if(m->fault || !s->auxiliary || !s->auxiliary(s->context,o,weapon,angles,world))return 0;
    put(m,o+0x1e4,get(m,0x5acbe8));return !m->fault;
}
int vehicle_targeting_frame(GameMemory *m,GameAddress o,const VehicleTargetServices *s){
    GameAddress target,previous,table=get(m,o),command=get(m,o+0xdc),weapon;uint32_t tick;int32_t command_kind;int blocked,clear,yaw_ready,pitch_ready,cooldown,eligible;unsigned i;
    float point[3],raw[3],origin[3],actual[3],range_distance;uint16_t yaw,actual_yaw;int16_t pitch,actual_pitch,low,high;
    if(m->fault || !s)return 0;
    command_kind=integer(get(m,command+12));if(game_memory_read_u8(m,o+12)==0x21 && command_kind==3)return !m->fault;
    tick=get(m,0x5ac6ac);if(!get(m,o+0x1ec))return !m->fault;
    target=get(m,o+0xf0);if(target && get(m,target+20)!=get(m,o+0xf4))put(m,o+0xf0,0);
    target=get(m,o+0xc8);if(target && get(m,target+20)==get(m,o+0xcc)){put(m,o+0xf0,target);put(m,o+0xf4,get(m,target+20));}
    target=get(m,o+0xf0);weapon=get(m,o+0x21c);vector(m,o+24,origin);
    if(target && (command_kind==5 || command_kind==6)){
        vector(m,target+24,raw);if(!(weapon_range(m,weapon)<planar(m,origin,raw)))goto aim;
    }
    previous=target;
    if(integer(get(m,o+0xd4)+16u)>integer(get(m,0x5ac6ac))){if(target)goto aim;return vehicle_targeting_idle(m,o,s);}
    put(m,o+0xd4,get(m,0x5ac6ac));
    {uint32_t query=get(m,table+0x1b4),flags;
        if(query==0x4cc1c0)flags=0x8040;
        else if(query==0x409320)flags=0x4000;
        else if(query==0x40eb80)flags=0x8000;
        else return 0;
        if(!s->acquire || !s->acquire(s->context,o,get(m,table+0x1ac),flags,&target))return 0;
    }
    put(m,o+0xf0,target);
    if(target){put(m,o+0xf4,get(m,target+20));game_memory_write_u16(m,o+0x210,0);game_memory_write_u16(m,o+0x212,0);put(m,o+0x1fc,0);}
    else if(previous && !friendly(m,o,previous)){target=previous;put(m,o+0xf0,target);put(m,o+0xf4,get(m,target+20));game_memory_write_u16(m,o+0x210,0);game_memory_write_u16(m,o+0x212,0);}
    else return vehicle_targeting_idle(m,o,s);
aim:
    target=get(m,o+0xf0);if(friendly(m,o,target))return !m->fault;
    /*404b33/404ba7 pass the shooter's position to the target's slot38.
     *Generators select one of their transformed aim points from this source. */
    vector(m,o+24,origin);
    if(!presentation_target_position(m,target,origin,point))return 0;
    vector(m,target+24,raw);range_distance=distance(m,o,get(m,o+0x1fc)?point:raw);
    if(get(m,o+0x1fc)){
        float time=(float)((long double)range_distance/real(m,get(m,weapon+20)+0x1c));
        for(i=0;i<3;i++){
            float velocity=get(m,target+16)&0x8000u?0:(float)(((long double)raw[i]-real(m,o+0x200+i*4))/integer(tick-get(m,o+0x20c)));
            point[i]=(float)((long double)velocity*time+point[i]);
        }
    }
    put(m,o+0x1fc,target);for(i=0;i<3;i++)store(m,o+0x200+i*4,raw[i]);put(m,o+0x20c,tick);
    if(!vehicle_targeting_point_angles(m,o,point,&yaw,&pitch) || !vehicle_targeting_point_angles(m,o,raw,&actual_yaw,&actual_pitch))return 0;
    yaw=(yaw+game_memory_read_u16(m,o+0x210))&2047u;pitch=short_value((uint16_t)(pitch+game_memory_read_u16(m,o+0x212)));
    if(!s->limit || !s->limit(s->context,o,get(m,table+0x1cc),&low) || !s->limit(s->context,o,get(m,table+0x1d0),&high))return 0;
    if(pitch<low)pitch=low;else if(pitch>high)pitch=high;
    if(!vehicle_targeting_aim(m,o,yaw,pitch,s))return 0;
    if(integer(bits(range_distance))<0x453b8000){
        /*404d58 keeps the unpredicted bearing as the interval endpoint.
         *Origin-only targets hid the swapped half-arc when both bearings agreed. */
        yaw_ready=in_arc(game_memory_read_u16(m,o+0x148),actual_yaw,midpoint(actual_yaw,yaw));
        if(!yaw_ready)yaw_ready=in_arc(game_memory_read_u16(m,o+0x148),(yaw-18u)&2047u,(yaw+18u)&2047u);
        i=(unsigned)abs((int)pitch-actual_pitch);if(i<18)i=18;
        pitch_ready=i>=(unsigned)abs((int)pitch-short_value(game_memory_read_u16(m,o+0x14a)));
    }else{yaw_ready=in_arc(game_memory_read_u16(m,o+0x148),(yaw-6u)&2047u,(yaw+6u)&2047u);pitch_ready=abs((int)pitch-short_value(game_memory_read_u16(m,o+0x14a)))<=6;}
    cooldown=integer(get(m,weapon+8)+get(m,get(m,weapon+20)+0x20))<=integer(tick);
    if(!cooldown && integer(get(m,weapon+0xb0))<=0){uint32_t total=get(m,weapon+0xac);put(m,weapon+0xb0,get(m,weapon+0x20)?total-random_word(m,integer(total-1u)>1?integer(total-1u):1):total);}
    eligible=!(weapon_range(m,weapon)<range_distance);
    if(!(yaw_ready && pitch_ready && cooldown && eligible))return secondary(m,o,s);
    vector(m,o+24,origin);origin[1]=(float)((long double)origin[1]+real(m,o+0x30));
    vector(m,target+24,actual);actual[1]=(float)((long double)actual[1]+real(m,target+0x30));
    if(!s->line || !s->line(s->context,origin,actual,&clear))return 0;
    if(!clear)return secondary(m,o,s);
    if(!s->blocked || !s->blocked(s->context,o,get(m,table+0x1f0),&blocked))return 0;
    if(blocked)return secondary(m,o,s);
    if(!s->fire)return 0;
    if(game_terrain_cell_presence(m,o,target)){
        if(!s->fire(s->context,o,weapon,target,1))return 0;
    }else{
        int16_t angle=short_value(vehicle_targeting_vector_angle(m,real(m,target+0x38),range_distance,real(m,0x571058)));
        int bias=abs(short_value(game_memory_read_u16(m,o+0x210)))*(game_memory_read_u8(m,o+12)==0x24?4:2);
        if(!s->fire(s->context,o,weapon,target,angle-bias>0))return 0;
    }
    if(integer(bits(range_distance))<0x453b8000){
        low=short_value(game_memory_read_u16(m,o+0x218));high=short_value(game_memory_read_u16(m,o+0x21a));
        game_memory_write_u16(m,o+0x210,(uint16_t)(random_word(m,low)-(uint16_t)(low>=0?low/2:-((-(int)low+1)/2))));
        game_memory_write_u16(m,o+0x212,(uint16_t)(random_word(m,high)-(uint16_t)(high>=0?high/4:-((-(int)high+3)/4))));
    }else{
        float scale=(float)((long double)real(m,0x57100c)-(long double)range_distance/weapon_range(m,weapon));int32_t value;
        low=short_value(game_memory_read_u16(m,o+0x214));high=short_value(game_memory_read_u16(m,o+0x216));
        value=integer(random_word(m,low))-(low>=0?low/2:-((-(int)low+1)/2));game_memory_write_u16(m,o+0x210,(uint16_t)trunc_word((long double)value*scale));
        value=integer(random_word(m,high))-(high>=0?high/4:-((-(int)high+3)/4));game_memory_write_u16(m,o+0x212,(uint16_t)trunc_word((long double)value*scale));
    }
    return !m->fault;
}
/*407410: turret/base targeting has no articulated-deployment gate. Borrowed
 *targets retain the original nonzero-generation test before invalidation. */
int vehicle_targeting_tick(GameMemory *m,GameAddress o,const VehicleTargetServices *s){
    GameAddress target,table;int32_t command;
    if(m->fault || !s)return 0;
    put(m,o+0x12c,get(m,o+0x12c)+1u);
    target=get(m,o+0xc8);if(target && get(m,target+20) && get(m,target+20)!=get(m,o+0xcc))put(m,o+0xc8,0);
    target=get(m,o+0xf0);if(target && get(m,target+20) && get(m,target+20)!=get(m,o+0xf4))put(m,o+0xf0,0);
    command=integer(get(m,get(m,o+0xdc)+12));table=get(m,o);
    if(command<0 || command>1)return get(m,table+0x1c0)==0x404950 && vehicle_targeting_frame(m,o,s);
    return s->settle && s->settle(s->context,o,get(m,table+0x168));
}
/*553730 gates only the articulated caller before its shared407410 tick. */
int vehicle_targeting_transient_reset(GameMemory *m,GameAddress o,const VehicleTargetServices *s){
    if(m->fault || !s)return 0;
    if((game_memory_read_u8(m,o+0xd8)&1u) || get(m,o+0x2b4)!=30u)return !m->fault;
    return vehicle_targeting_tick(m,o,s);
}
int vehicle_targeting_obstructed_arc(GameMemory *m,GameAddress o,int *blocked){
    GameAddress descriptor=get(m,get(m,o+0x294)+4),distance_table=get(m,descriptor+0x28),arcs=get(m,descriptor+0x2c);int count=short_value(game_memory_read_u16(m,descriptor+0x26)),i;
    float origin[3],target[3],d;int16_t pitch=short_value(game_memory_read_u16(m,o+0x14a));uint16_t yaw=(game_memory_read_u16(m,o+0x24)+game_memory_read_u16(m,o+0x14c))&2047u;
    *blocked=0;if(count>32767 || count<0)return 0;
    vector(m,get(m,descriptor+0x1c),origin);vector(m,get(m,o+0xf0)+24,target);d=planar(m,target,origin);
    for(i=count-1;i>=1;i--){
        GameAddress row=arcs+(uint32_t)i*8;
        if(game_memory_read_u16(m,row)==0xffffu || d<real(m,distance_table+(uint32_t)i*4))continue;
        if(short_value(game_memory_read_u16(m,row+2))>=pitch || short_value(game_memory_read_u16(m,row))<=pitch)continue;
        {int start=short_value(game_memory_read_u16(m,row+4)),end=short_value(game_memory_read_u16(m,row+6));
            if(start>end?(yaw<=end || yaw>=start):(yaw>=start && yaw<=end)){*blocked=1;break;}
        }
    }
    return !m->fault;
}
