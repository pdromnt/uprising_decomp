#include "player_contact.h"
#include <math.h>
#include <string.h>

static uint32_t get(GameMemory *m,GameAddress p){return game_memory_read_u32(m,p);}
static float real(GameMemory *m,GameAddress p){uint32_t bits=get(m,p);float f;memcpy(&f,&bits,4);return f;}
static int32_t signed_word(uint32_t bits){int32_t v;memcpy(&v,&bits,4);return v;}
static int response(GameMemory *m,GameAddress o,const GameContactDamageServices *s,uint32_t *out){
    GameAddress method=get(m,get(m,o)+0x4c);
    return !m->fault && s->response && s->response(s->context,o,method,out);
}
static int coefficient(GameMemory *m,uint32_t kind,uint32_t resistance,float *out){
    /*54b130 loads eighteen damage rows, each with thirteen resistances. */
    if(kind>=18 || resistance>=13){m->fault=1;return 0;}
    *out=real(m,get(m,0x59dae4)+(kind*13+resistance)*4);return !m->fault;
}
int game_contact_damage(GameMemory *m,GameAddress o,GameAddress source,const GameContactDamageServices *s,int32_t *out){
    uint32_t flags,type,target_flags,relation=0,value=0,resistance,kind;GameAddress child=0,record,owner,method;float factor;
    int32_t base=0;long double amount;
    if(!m || m->fault || !s || !out)return 0;
    flags=get(m,source+0x10);type=game_memory_read_u8(m,source+12);
    if(flags&0x100){
        if(!(flags&0x20000)){
            switch(type){
            case 0x23:child=get(m,source+0x294);break;
            case 0x80:case 0x81:case 0x82:case 0x83:case 0x84:case 0x85:case 0x86:case 0x87:child=get(m,source+0xa0);break;
            case 0x88:case 0x89:child=get(m,source+0xb4);break;
            default:break;
            }
            if(child && !get(m,child+16))value=get(m,o+0x58)*2u;
        }
    }else if(type==10){
        method=get(m,get(m,o)+0x5c);
        if(method==0x42ec70){
            if(!(game_memory_read_u16(m,source+0xe4)&0x8000)){
                long double x=real(m,source+0xb4),y=real(m,source+0xb8),z=real(m,source+0xbc);
                float speed=(float)sqrtl((float)((x*x+y*y)+z*z));
                float ratio=(float)((long double)real(m,source+0x38)/real(m,o+0x38)),cap=real(m,0x5721b0);
                speed=cap>speed?speed:cap;
                if(speed>real(m,0x5721b4) && !isnan(speed)){
                    if(!response(m,o,s,&resistance) || !coefficient(m,2,resistance,&factor))return 0;
                    amount=(long double)factor*ratio*speed;
                    value=!isfinite(amount) || amount< -0x1p63L || amount>=0x1p63L?0u:(uint32_t)(uint64_t)(int64_t)amount;
                }
            }
        }else if(!s->impact || !s->impact(s->context,o,source,method,&value))return 0;
    }else{
        if(type!=11){
            target_flags=get(m,o+16);
            if(type!=0x41 || (target_flags&0x100))relation=get(m,0x5f0160+(get(m,source+20)&63)*24+(get(m,o+20)&63)*4);
            if(relation){
                if(type==0x41){if(game_memory_read_u8(m,source+0xcc)!=0x20)goto resisted;}
                else if(target_flags&0x100){child=get(m,source+0xd4);if(!child || game_memory_read_u8(m,child+12)!=0x20)goto resisted;}
            }
        }
        kind=0;
        if(type==11){
            owner=get(m,0x5f0210+(get(m,source+20)&63)*4);
            if(!s->catalog_record || !s->catalog_record(s->context,owner,0x89,&record))return 0;
            base=signed_word(get(m,record+0x28));kind=get(m,record+0x2c);
        }
        if(!response(m,o,s,&resistance))return 0;
        if(type==0x41)kind=get(m,source+0xbc);
        else if(type!=11)kind=get(m,get(m,source+0xe0)+0x18);
        if(!coefficient(m,kind,resistance,&factor))return 0;
        if(type!=11 && type!=0x41){
            if(relation && type==2)factor=(float)((long double)factor*real(m,0x572198));
            record=get(m,source+0xe0);base=signed_word(get(m,record+0x14));
            if(get(m,0x594d3c)){
                child=get(m,source+0xd4);
                if((get(m,child+20)&63)==game_memory_read_u16(m,get(m,0x5f0208)+8))base=signed_word((uint32_t)base*4u);
            }
        }else if(type==0x41)base=signed_word(get(m,source+0xb8));
        value=(uint32_t)player_contact_scaled_damage(base,(long double)factor);
    }
resisted:
    *out=player_contact_resisted_damage(signed_word(value),signed_word(get(m,o+0x5c)),game_memory_read_u8(m,source+12),get(m,source+16));
    return !m->fault;
}
