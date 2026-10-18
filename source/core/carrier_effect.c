#include "carrier_effect.h"
#include "effect_state.h"
#include "gameplay_leaves.h"
#include <math.h>
#include <string.h>
static uint32_t get(GameMemory *m,GameAddress p){return game_memory_read_u32(m,p);}
static void put(GameMemory *m,GameAddress p,uint32_t v){game_memory_write_u32(m,p,v);}
static float real(GameMemory *m,GameAddress p){uint32_t b=get(m,p);float f;memcpy(&f,&b,4);return f;}
static void store(GameMemory *m,GameAddress p,float f){uint32_t b;memcpy(&b,&f,4);put(m,p,b);}
static int32_t signed_word(uint32_t b){int32_t v;memcpy(&v,&b,4);return v;}
static int16_t short_at(GameMemory *m,GameAddress p){uint16_t b=game_memory_read_u16(m,p);int16_t v;memcpy(&v,&b,2);return v;}
static uint32_t ftol(long double v){return (uint32_t)(uint64_t)(!isfinite(v) || v< -0x1p63L || v>=0x1p63L?INT64_MIN:(int64_t)v);}
static float random_sample(GameMemory *m){uint32_t cursor=get(m,0x6308c0);
    if(cursor==1024)cursor=0;
    if(cursor>1024){m->fault=1;return 0;}
    put(m,0x6308c0,cursor+1);return real(m,0x6308c8+cursor*4);
}
static float distance(GameMemory *m,GameAddress o,int placement){GameAddress camera=get(m,0x58f890);
    long double x=(long double)real(m,o+0x18)-real(m,camera+12),y=(long double)real(m,o+0x1c)-real(m,camera+16),z=(long double)real(m,o+0x20)-real(m,camera+20);
    long double scale=real(m,0x573430);
    return (float)(placement?(y*y*scale+(x*x*scale+z*z*scale)):(x*x*scale+(y*y*scale+z*z*scale)));
}
int carrier_effect_place(GameMemory *m,GameAddress o,uint32_t id,const float p[3],const uint16_t angles[3],const CarrierServices *s){
    unsigned i;uint16_t bank;SoundPlaybackRequest sound={24,0,1,0.55f,1,1};
    if(!m || m->fault || !s || !s->place || !s->play)return 0;
    if(!game_random_effect_begin(m,o))return 0;
    store(m,o+0xb8,p[1]);game_memory_write_u16(m,o+0xbc,angles[0]);put(m,o+0xcc,get(m,0x5ac6ac));
    game_memory_write_u16(m,o+0xbe,0);put(m,o+0xc4,(get(m,0x5ac6ac)&1)+2);
    bank=(uint16_t)(ftol((long double)random_sample(m)*real(m,0x57341c))+50u);
    if(!(get(m,o+0xc4)&1))bank=(uint16_t)(0u-bank);
    game_memory_write_u16(m,o+0xc0,bank);
    for(i=0;i<4;i++)put(m,o+(i==0?0xd8:i==1?0xe0:i==2?0xfc:0x104),0);
    for(i=0;i<2;i++){
        int32_t x=signed_word(ftol((long double)random_sample(m)*real(m,0x573420)));
        int32_t z=signed_word(ftol((long double)random_sample(m)*real(m,0x573420)));
        store(m,o+0xe4+i*12,(float)(((long double)x-real(m,0x573424))*real(m,0x573428)));
        store(m,o+0xec+i*12,(float)(((long double)z-real(m,0x573424))*real(m,0x573428)));
    }
    store(m,o+0xb4,0);sound.distance_squared=distance(m,o,1);
    if(m->fault || !s->play(s->context,&sound))return 0;
    put(m,o+0xd0,get(m,get(m,0x628a90)));
    return !m->fault && s->place(s->context,o,id,p,angles);
}
int carrier_effect_reset(GameMemory *m,GameAddress o,const CarrierServices *s){
    return m && !m->fault && s && s->stop && s->reset && s->stop(s->context,24,get(m,o+0xd0)) && s->reset(s->context,o);
}
/*The four controls are ordinary cubic Bezier coefficients. Original float
 *stores between terms and integer-bit clamps retain signed-zero/NaN behavior. */
static float curve(GameMemory *m,GameAddress o,unsigned axis,float t){
    static const unsigned factorial[4]={1,1,2,6};float result=0,complement=(float)((long double)real(m,0x5733f0)-t);unsigned i,j;
    for(i=0;i<4;i++){
        long double left=1,right=1,term;
        for(j=0;j<3-i;j++)left*=complement;
        for(j=0;j<i;j++)right*=t;
        term=(long double)real(m,0x573454)/(float)(factorial[i]*factorial[3-i]);
        term*=real(m,o+0xd8+(axis*2+i*3)*4);
        term*=(float)right;term*=(float)left;
        result=(float)(term+result);
    }
    return result;
}
static void bank_step(GameMemory *m,GameAddress o){uint32_t count=get(m,o+0xc4);int16_t roll=short_at(m,o+0x28),target=short_at(m,o+0xc0);
    if(count){
        if(count&1){roll=(int16_t)(uint16_t)((uint16_t)roll+2u);game_memory_write_u16(m,o+0x28,(uint16_t)roll);if(target>roll)return;}
        else{roll=(int16_t)(uint16_t)((uint16_t)roll-2u);game_memory_write_u16(m,o+0x28,(uint16_t)roll);if(target<roll)return;}
        game_memory_write_u16(m,o+0x28,(uint16_t)target);put(m,o+0xc4,count-1);
        game_memory_write_u16(m,o+0xc0,(uint16_t)(0u-(uint16_t)target));return;
    }
    if(roll>0){roll=(int16_t)(uint16_t)((uint16_t)roll-2u);game_memory_write_u16(m,o+0x28,(uint16_t)(roll<0?0:roll));}
    else if(roll<0){roll=(int16_t)(uint16_t)((uint16_t)roll+2u);game_memory_write_u16(m,o+0x28,(uint16_t)(roll>0?0:roll));}
    else{int16_t pitch=short_at(m,o+0x26);if(pitch>-426)game_memory_write_u16(m,o+0x26,(uint16_t)(pitch-3));}
}
int carrier_effect_update(GameMemory *m,GameAddress o,const CarrierServices *s){
    GameAddress bank,descriptor;uint32_t bits;float speed;int playing=0;
    if(!m || m->fault || !s)return 0;
    bank=get(m,0x628a90);
    if(get(m,bank+0x468) && (get(m,0x5ac6ac)&3)){
        if(!s->playing || !s->playing(s->context,24,get(m,o+0xd0),&playing))return 0;
        if(playing){
            bits=get(m,o+0xb4);speed=bits>0x80000000u?0.55f:(float)((long double)real(m,o+0xb4)*real(m,0x573434)+real(m,0x57342c));
            if(!s->frequency || !s->frequency(s->context,24,get(m,o+0xd0),speed) ||
               !s->distance_volume || !s->distance_volume(s->context,24,get(m,o+0xd0),distance(m,o,0)))return 0;
        }
    }
    if(!get(m,o+0xc8)){
        descriptor=get(m,o+0xd4);
        if(!descriptor || !get(m,descriptor+0x3c)){
            int32_t degrees=signed_word(ftol((long double)random_sample(m)*real(m,0x573438)));
            uint32_t turn=ftol(((long double)degrees-real(m,0x57343c))*real(m,0x573440));
            put(m,o+0xc8,1);game_memory_write_u16(m,o+0xbe,0);
            game_memory_write_u16(m,o+0xbc,(game_memory_read_u16(m,o+0x24)+(uint16_t)turn)&2047u);
            if(descriptor){SoundPlaybackRequest sound={134,(float)game_leaf_horizontal_distance_squared(m,o+0x18,get(m,0x58f890)),1,0,0,0};
                if(!s->play || !s->play(s->context,&sound))return 0;
            }
            if(!descriptor)return !m->fault;
        }
        {GameAddress group=get(m,descriptor+4),source;int32_t slot=short_at(m,descriptor+8);float t,out[2];long double progress;
            if(slot<0 || slot>=(int16_t)game_memory_read_u16(m,group+0x24)){m->fault=1;return 0;}
            progress=((long double)real(m,o+0xb8)-real(m,o+0x1c))*real(m,0x573444);t=(float)progress;
            store(m,o+0xb4,(float)(((long double)real(m,0x5733f0)-progress)*real(m,0x573448)));
            bits=get(m,o+0xb4);
            speed=bits>0xc3e10000u?-450.0f:(real(m,o+0xb4)<real(m,0x573450)?real(m,o+0xb4):real(m,0x573450));
            store(m,o+0xb4,speed);
            memcpy(&bits,&t,4);
            if(bits>0x80000000u)t=0;else if(signed_word(bits)>0x3f800000)t=1;
            out[0]=curve(m,o,0,t);out[1]=curve(m,o,1,t);
            store(m,o+0x1c,(float)((long double)real(m,o+0x1c)+speed));
            source=get(m,group+0x1c)+(uint32_t)slot*12;
            store(m,o+0x18,(float)((long double)real(m,source)+out[0]));
            store(m,o+0x20,(float)((long double)real(m,source+8)+out[1]));
            if(signed_word(get(m,o+0x18))<0x447a0000)put(m,o+0x18,0x447a0000);
            else if(signed_word(get(m,o+0x18))>0x47fe0c00)put(m,o+0x18,0x47fe0c00);
            if(signed_word(get(m,o+0x20))<0x447a0000)put(m,o+0x20,0x447a0000);
            else if(signed_word(get(m,o+0x20))>0x47fe0c00)put(m,o+0x20,0x47fe0c00);
        }
    }else{
        int16_t acceleration=short_at(m,o+0xbe),yaw,target;float updated;
        if(!ftol((long double)random_sample(m)*real(m,0x573464)))acceleration=(int16_t)(uint16_t)((uint16_t)acceleration+1u);
        if(acceleration<0)acceleration=0;else if(acceleration>=3)acceleration=3;
        game_memory_write_u16(m,o+0xbe,(uint16_t)acceleration);
        yaw=short_at(m,o+0x24);target=short_at(m,o+0xbc);
        if(yaw>target){yaw=(int16_t)(uint16_t)((uint16_t)yaw-(uint16_t)acceleration);game_memory_write_u16(m,o+0x24,(uint16_t)yaw);if(yaw<target)game_memory_write_u16(m,o+0xbc,(uint16_t)yaw);}
        else if(yaw<target){yaw=(int16_t)(uint16_t)((uint16_t)yaw+(uint16_t)acceleration);game_memory_write_u16(m,o+0x24,(uint16_t)yaw);if(yaw>target)game_memory_write_u16(m,o+0xbc,(uint16_t)yaw);}
        bits=get(m,o+0xb4);updated=(float)((long double)real(m,o+0xb4)+real(m,bits>0x80000000u?0x573468:0x57346c));store(m,o+0xb4,updated);
        if(bits<=0x80000000u){
            bits=get(m,o+0xb4);
            updated=bits>0x80000000u?0:(updated<real(m,0x573470)?updated:real(m,0x573470));store(m,o+0xb4,updated);
        }
        store(m,o+0x1c,(float)((long double)real(m,o+0x1c)+real(m,o+0xb4)));
        if(!(real(m,o+0x1c)<real(m,o+0xb8)))return carrier_effect_reset(m,o,s);
    }
    if(get(m,o+0xb4)<0xc3480000u)bank_step(m,o);
    return !m->fault;
}
