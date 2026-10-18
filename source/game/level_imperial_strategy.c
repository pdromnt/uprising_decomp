#include "level_load.h"
#include "level_run.h"
#include "level_factory.h"
#include "level_objects.h"
#include "level_player_resources.h"
#include "gameplay_geometry.h"
#include "gameplay_spatial.h"
#include "game_intrusive_list.h"
#include "state_helper_plans.h"
#include "gameplay_state.h"
#include "level_player_slots.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

/*Mandatory ordinary-C programs still being recovered. Their arguments are
 *actual domain inputs, never registers, a guest stack or instruction state. */
extern int game_level_imperial_operation(GameApplication *,GameAddress,GameAddress); /*actual native method ID*/
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
static float real(GameApplication *a,GameAddress p){uint32_t b=get(a,p);float v;memcpy(&v,&b,4);return v;}
static int32_t integer(uint32_t b){int32_t v;memcpy(&v,&b,4);return v;}
static uint32_t truncate_word(long double v){return (uint32_t)(uint64_t)(!isfinite(v)||v< -0x1p63L||v>=0x1p63L?INT64_MIN:(int64_t)v);}
static GameAddress site_at(GameApplication *a,unsigned i){return get(a,get(a,0x639a70)+i*4);}
static uint32_t array_get(GameApplication *a,GameAddress p,unsigned field,unsigned i){return get(a,get(a,p+field)+i*4);}
static void array_put(GameApplication *a,GameAddress p,unsigned field,unsigned i,uint32_t v){put(a,get(a,p+field)+i*4,v);}
/*53f170/53fc40,53f1a0/53fc70 and53f1b0/53fc80: the descriptor
 *borrows its actual payload. Health and maximum never use a second mirror. */
extern int game_level_group_descriptor_payload(GameApplication *,GameAddress,GameAddress *);
int game_level_imperial_descriptor_health(GameApplication *a,GameAddress d,uint32_t *health,uint32_t *maximum){
    GameAddress table=get(a,d),payload=get(a,d+0x48);uint32_t h=get(a,table+0x20),m=get(a,table+0x24);
    if((h!=0x53e4b0 && h!=0x53f1a0 && h!=0x53fc70) || (m!=0x53e4c0 && m!=0x53f1b0 && m!=0x53fc80))return 0;
    *health=get(a,payload+0x58);
    return game_level_scene_property(a,payload,get(a,get(a,payload)+0x44),maximum);
}
static int descriptor_ratio(GameApplication *a,GameAddress d,GameAddress bias,long double *out){
    uint32_t h,m;if(!game_level_imperial_descriptor_health(a,d,&h,&m))return 0;
    /*Original stores the signed numerator as float before division. */
    *out=(long double)(float)integer(h+get(a,bias+0x18))/(long double)integer(m);return !a->memory.fault;
}
/*4bc090: update controller buckets, count owned formations and choose the
 *weakest real site descriptor. Site arrays remain owned by the strategy. */
int game_level_imperial_census(GameApplication *a,GameAddress p,int forced){
    static const unsigned cleared[]={0x2a8,0x2a4,0x2a0,0x298,0x29c,0x2b8,0x2bc,0x2c4};
    int count;unsigned i,j;
    if(!forced || integer(get(a,p+0x140))>integer(get(a,0x5ac6ac)))return !a->memory.fault;
    count=(int16_t)game_memory_read_u16(&a->memory,0x639a74);if(count<0)return 0;
    if(game_memory_copy_elements(&a->memory,get(a,p+0xf64),get(a,p+0x26c),(unsigned)count,4,0)!=(unsigned)count)return 0;
    put(a,p+0x140,get(a,0x5ac6ac));
    (void)gameplay_state_refresh_entity_buckets(&a->memory,p+0x1c8,forced,0);
    put(a,p+0x28c,get(a,p+0x240));put(a,p+0x290,get(a,p+0x244));put(a,p+0x294,get(a,p+0x248));
    for(i=0;i<sizeof(cleared)/sizeof(cleared[0]);i++)put(a,p+cleared[i],0);
    put(a,p+0x33c,0x3f800000);
    game_memory_fill_u32(&a->memory,get(a,p+0x314),(unsigned)count*3,0,0);
    for(i=0;i<4;i++){GameAddress root=get(a,0x5f0210+i*4);
        if(get(a,root+0x14) && get(a,root+0x448)!=get(a,root+0x3bc))put(a,p+0x2bc,get(a,p+0x2bc)|(1u<<i));}
    for(i=0;i<(unsigned)count;i++){
        GameAddress site=array_get(a,p,0x26c,i),previous=array_get(a,p,0xf64,i);uint32_t chosen;
        array_put(a,p,0x318,i,0);array_put(a,p,0x310,i,0);
        for(j=0;j<8;j++){
            GameAddress headslot=get(a,p+0x2c8)+(i*8+j)*4,node=get(a,headslot);unsigned visits=0;
            if(!node)continue;if(!get(a,node+0x14)){put(a,headslot,0);continue;}
            array_put(a,p,0x318,i,array_get(a,p,0x318,i)+1);
            while(node){uint8_t kind=game_memory_read_u8(&a->memory,node+12);unsigned lane;
                if(++visits>65536)return 0;
                array_put(a,p,0x310,i,array_get(a,p,0x310,i)+1);
                if(kind==0x21 || kind==0x27 || kind==0x40){lane=kind==0x21?2:kind==0x27?1:0;
                    GameAddress counter=get(a,p+0x314)+(i*3+lane)*4;put(a,counter,get(a,counter)+1);}
                node=get(a,node+0x10c);
            }
        }
        if(site){
            if(!previous){array_put(a,p,0x280,i,6);array_put(a,p,0x284,i,20);array_put(a,p,0x288,i,2);
                put(a,p+0x5f0,get(a,0x5ac6ac)+320);put(a,p+0x258,get(a,0x5ac6ac)+160);}
            if(!get(a,get(a,site+0x18)))put(a,p+0x298,1);
            if((int16_t)game_memory_read_u16(&a->memory,site+0x24)>(int16_t)game_memory_read_u16(&a->memory,site+0x26) && integer(get(a,site+0x14))>0)put(a,p+0x2a4,get(a,p+0x2a4)+1);
            for(j=0;(int32_t)j<(int16_t)game_memory_read_u16(&a->memory,site+0x24);j++){
                GameAddress d=get(a,get(a,site+0x18)+j*4),object;long double candidate,old;uint32_t x,z;
                if(!d){put(a,p+0x2a0,get(a,p+0x2a0)+1);continue;}
                if(!game_level_group_descriptor_payload(a,d,&object))return 0;
                z=truncate_word(real(a,object+0x20));x=truncate_word(real(a,object+0x18));
                if(game_memory_read_u8(&a->memory,0x6020f0+(GameAddress)(integer(z&0xfffff07f)>>7)+(GameAddress)(integer(x)>>12))){
                    put(a,p+0x2b8,get(a,p+0x2b8)|(1u<<(i&31)));array_put(a,p,0x324,i,get(a,0x5ac6ac));}
                chosen=array_get(a,p,0x320,i);
                if(chosen==UINT32_MAX || !get(a,get(a,site+0x18)+chosen*4))array_put(a,p,0x320,i,j);
                else{
                    GameAddress incumbent=get(a,get(a,site+0x18)+chosen*4);
                    if(!descriptor_ratio(a,incumbent,incumbent,&old) || !descriptor_ratio(a,d,incumbent,&candidate))return 0;
                    if(candidate< (long double)(float)old || isnan(candidate) || isnan(old))array_put(a,p,0x320,i,j);
                }
                if(game_memory_read_u16(&a->memory,d+0x14)==10 && integer(get(a,site+0x14))<=0)put(a,p+0x2a8,get(a,p+0x2a8)+1);
                site=array_get(a,p,0x26c,i);
            }
            chosen=array_get(a,p,0x31c,i);
            if(integer(chosen)>0 && integer(chosen+1920)>integer(get(a,0x5ac6ac)))put(a,p+0x2c4,1);
        }else{
            if(previous)array_put(a,p,0x328,i,get(a,0x5ac6ac));
            array_put(a,p,0x31c,i,UINT32_MAX);array_put(a,p,0x320,i,UINT32_MAX);array_put(a,p,0xe44,i,get(a,0x5ac6ac));
        }
        {GameAddress enemy=array_get(a,p,0x274,i);if(enemy && !get(a,get(a,enemy+0x18)))put(a,p+0x29c,1);}
        chosen=array_get(a,p,0x320,i);
        if(integer(chosen)>-1){long double ratio;GameAddress d=get(a,get(a,array_get(a,p,0x26c,i)+0x18)+chosen*4);
            if(!descriptor_ratio(a,d,d,&ratio))return 0;
            if(ratio<real(a,p+0x33c) || isnan(ratio) || isnan(real(a,p+0x33c))){float stored=(float)ratio;uint32_t bits;memcpy(&bits,&stored,4);put(a,p+0x33c,bits);}}
    }
    return !a->memory.fault;
}
/*4be7d0: threat-history decay and timestamp publication for one real site. */
int game_level_imperial_site_refresh(GameApplication *a,GameAddress p,unsigned i,int forced){
    uint32_t health=0,maximum=0,age=array_get(a,p,0x3bc,i),old,next;GameAddress site,owner;unsigned j;double decay;
    if(i>=(unsigned)(int16_t)game_memory_read_u16(&a->memory,0x639a74))return 0;
    if(forced && integer(age)>=0)return !a->memory.fault;
    site=site_at(a,i);owner=get(a,site+0x54);
    if(owner && !get(a,0x5f0160+((get(a,p+0x14)&63)*6+(game_memory_read_u16(&a->memory,owner+8)&63))*4)){
        for(j=0;(int32_t)j<(int16_t)game_memory_read_u16(&a->memory,site+0x24);j++){
            GameAddress d=get(a,get(a,site+0x18)+j*4);uint32_t h,m;
            if(d){if(!game_level_imperial_descriptor_health(a,d,&h,&m))return 0;health+=h;maximum+=m;}site=site_at(a,i);
        }
    }else maximum=1;
    if(integer(age)>=0){
        health=truncate_word((long double)integer(health*100u)/(long double)integer(array_get(a,p,0x3b8,i)));
        next=array_get(a,p,0x3b4,i)-health;if(integer(next)<0)next=0;old=array_get(a,p,0x3b0,i);
        if((integer(old)<0 && integer(next)>0) || integer(next)>integer(old))array_put(a,p,0x3b0,i,next);
        else{uint64_t bits=(uint64_t)get(a,0x5746f0)|((uint64_t)get(a,0x5746f4)<<32);memcpy(&decay,&bits,8);
            array_put(a,p,0x3b0,i,truncate_word((long double)integer(old)*decay));}
    }
    if(forced){array_put(a,p,0x3b4,i,truncate_word((long double)integer(health*100u)/(long double)integer(maximum)));
        array_put(a,p,0x3b8,i,maximum);array_put(a,p,0x3bc,i,get(a,0x5ac6ac));}
    else{array_put(a,p,0x3b4,i,UINT32_MAX);array_put(a,p,0x3b8,i,UINT32_MAX);array_put(a,p,0x3bc,i,UINT32_MAX);}
    return !a->memory.fault;
}
static int random_scaled(GameApplication *a,float scale,uint32_t *out){uint32_t index=get(a,0x6308c0);
    if(index==1024)index=0;if(index>=1024)return 0;
    put(a,0x6308c0,index+1);*out=truncate_word((long double)real(a,0x6308c8+index*4)*scale);return !a->memory.fault;}
static void enable(GameApplication *a,GameAddress p,unsigned slot){uint32_t flags=get(a,p+0x1a4),bit=1u<<slot;
    if(!(flags&bit) && !get(a,0x590a6c)){
        put(a,p+0xda0+slot*4,get(a,0x5ac6ac));game_memory_write_u8(&a->memory,p+0x1a8+slot,0);
        game_memory_write_u8(&a->memory,p+0x1b6+slot,0);put(a,p+0x1a4,flags|bit);
    }
}
/*4bdba0: first missing configured class, with the class10 site/defender rule. */
uint32_t game_level_imperial_missing_class(GameApplication *a,GameAddress p){
    GameAddress root=get(a,0x58973c);int count=(int16_t)game_memory_read_u16(&a->memory,0x639a74);unsigned i,j;
    for(i=0;i<11;i++){
        uint32_t kind=get(a,p+0xd68+i*4);if(integer(kind)<=0 || get(a,root+0x200+kind*4))continue;
        if(kind!=10)return kind;
        for(j=0;(int)j<count;j++){
            GameAddress site=get(a,get(a,0x639a70)+j*4);
            if(get(a,site+0x54)==get(a,0x5f0220) && get(a,get(a,site+0x18)) && get(a,site+0x14) &&
                (!get(a,p+0x2a0) || (int16_t)game_memory_read_u16(&a->memory,site+0x24)>(int16_t)game_memory_read_u16(&a->memory,site+0x26)))return kind;
        }
    }
    return 0;
}
static int budget_for_restart(GameApplication *a,GameAddress root,uint32_t *cost){uint32_t price,extra;
    if(get(a,get(a,root)+0x24)!=0x479450 || get(a,get(a,root)+0x28)!=0x479590)return 0;
    if(!game_level_player_catalog_value(a,root,0x27,8,0,&price) || !game_level_player_catalog_value(a,root,0x23,8,0,&extra))return 0;
    *cost=price+extra;return !a->memory.fault;
}
/*4b4610: choose actions on the real32-tick cadence. Wrapping low words and
 *float-to-integer arithmetic preserve the original numeric decisions. */
static int decisions(GameApplication *a,GameAddress p){
    uint32_t tick=get(a,0x5ac6ac),flags,sites,needed,limit,period,first,second;int count;
    GameAddress root=get(a,0x58973c);unsigned i;int pending;
    if(tick&31u)return !a->memory.fault;
    if(!game_level_imperial_census(a,p,1))return 0;
    count=(int16_t)game_memory_read_u16(&a->memory,0x639a74);if(count<0)return 0;
    for(i=0;i<(unsigned)count;i++){uint32_t age=get(a,get(a,p+0x3bc)+i*4);
        if(integer(age)>=0 && integer(age+960)<integer(tick) && !game_level_imperial_site_refresh(a,p,i,0))return 0;}
    flags=get(a,p+0x1a4);first=get(a,p+0xd98)-tick;
    put(a,p+0xf58,!(flags&0x1000) && integer(first)>=1920 && integer(get(a,0x6041a4))>>1<=integer(first));
    if(get(a,p+0x2a8))enable(a,p,11);
    sites=get(a,p+0x28c);
    if(sites && (game_level_imperial_missing_class(a,p) || get(a,p+0x2a0)))enable(a,p,1);
    put(a,p+0x268,truncate_word((long double)integer(sites-1)/((long double)count-real(a,0x574654))*real(a,0x574658)));
    if(get(a,p+0x1a4)&0x100)return !a->memory.fault;
    pending=(get(a,p+0x1a4)&0x208)!=0;
    if(!pending && get(a,p+0x290))for(i=0;i<(unsigned)count;i++){
        if(get(a,get(a,p+0x270)+i*4) && get(a,get(a,p+0x310)+i*4) &&
            integer(get(a,get(a,p+0x328)+i*4)+get(a,0x60419c))<integer(tick)){enable(a,p,9);pending=1;}}
    if(!sites){
        uint32_t sum=0,cost;for(i=0;i<(unsigned)count;i++)sum+=get(a,get(a,p+0x310)+i*4);
        if(get(a,p+0x1a4))return !a->memory.fault;
        if(!budget_for_restart(a,root,&cost))return 0;
        if(integer(sum)>0)return !a->memory.fault;
        if(integer(get(a,root+0x22c))>=integer(cost) && integer(get(a,p+0xd9c))<integer(get(a,0x60414c))){
            enable(a,p,8);put(a,p+0xd9c,get(a,p+0xd9c)+1);
        }else enable(a,p,13);
        return !a->memory.fault;
    }
    if(!pending && (integer(get(a,p+0x294))>integer(sites) ||
        ((!game_level_imperial_missing_class(a,p) || !get(a,p+0x2a0)) && integer(get(a,p+0xdac)+get(a,0x604138))<integer(tick))) &&
        get(a,p+0x290) && get(a,0x604138)!=999999999)enable(a,p,3);
    limit=get(a,0x604140)+truncate_word((long double)integer(get(a,0x604140)*(sites-1))/(count-1)*real(a,0x57465c));
    if(count-integer(get(a,p+0x294))>integer(get(a,0x6041a0)))put(a,p+0xe14,0);
    if(!(get(a,p+0x1a4)&0x20)){
        if(get(a,p+0xe14)==1){put(a,p+0xe18,get(a,p+0xe18)+1);enable(a,p,5);}
        if(integer(get(a,p+0xe18))>integer(get(a,0x60418c)))put(a,p+0xe14,2);
    }
    first=get(a,p+0xdec);
    if(!(get(a,p+0x1a4)&0x20)){
        if(integer(first)>=integer(limit)){put(a,p+0xdec,0);first=0;}
        if(integer(first)>0 && integer(first)<integer(limit)){enable(a,p,5);put(a,p+0xdec,first+1);}
    }
    if(!(get(a,p+0x1a4)&0x20)){
        period=get(a,0x6041c4)-truncate_word((long double)integer(get(a,0x6041c4)*(sites-1))/(count-1)*real(a,0x574660));
        if(get(a,p+0x2c4))period+=480;
        if(integer(get(a,p+0xe14))<1 && count-integer(get(a,p+0x294))<=integer(get(a,0x6041a0))){period=0;put(a,p+0xe18,0);put(a,p+0xe14,1);}
        put(a,p+0x2ac,period);
        if(get(a,p+0x29c) && get(a,0x604134)){
            if(get(a,p+0xe10)==get(a,0x604134) && integer(get(a,p+0xdb4)+period)<integer(tick))put(a,p+0xe10,0);
            if(integer(get(a,p+0xe10))<integer(get(a,0x604134))){put(a,p+0xe10,get(a,p+0xe10)+1);enable(a,p,5);}
        }
        if(!get(a,p+0xdec) && integer(get(a,p+0xdb4)+period)<integer(tick) && get(a,p+0x294)){
            put(a,p+0xdec,1);enable(a,p,5);
            if(integer(get(a,p+0x294))>1){
                if(!random_scaled(a,real(a,0x574664),&first))return 0;
                if(integer(first)<integer(get(a,0x604164)) && !(get(a,p+0x1a4)&0x40)){put(a,p+0xdf0,1);enable(a,p,6);}
            }
        }
    }
    first=get(a,p+0xdf0);
    if(!(get(a,p+0x1a4)&0x40)){
        if(integer(first)>=integer(limit)){put(a,p+0xdf0,0);first=0;}
        if(integer(first)>0 && integer(first)<integer(limit)){enable(a,p,6);put(a,p+0xdf0,first+1);}
    }
    period=1920-truncate_word((long double)integer(sites-1)/(count-1)*real(a,0x574668));
    if(!(get(a,p+0x1a4)&4) && integer(get(a,p+0xda8)+period)<integer(tick)){
        enable(a,p,2);
        if(!random_scaled(a,real(a,0x574664),&first) || !random_scaled(a,real(a,0x574664),&second))return 0;
        if(integer(first)<integer(get(a,0x60412c))){
            unsigned available=0,selected=0,slot;
            for(i=0;i<4;i++)if(get(a,get(a,0x5f0210+i*4)+0x1f8))available++;
            if(available){
                if(!random_scaled(a,(float)available,&first))return 0;selected=first+1;slot=0;
                while(integer(selected)>0 && slot<4){if(get(a,get(a,0x5f0210+slot*4)+0x1f8))selected--;slot++;}
                if(integer(selected)>0)return 0;put(a,p+0xf5c,slot-1);
            }else if(integer(second)>=integer(get(a,0x604160)))goto cancel;
            else put(a,p+0xf5c,UINT32_MAX);
        }else if(integer(second)>=integer(get(a,0x604160))){
cancel:     put(a,p+0xda8,tick);put(a,p+0x1a4,get(a,p+0x1a4)&~4u);
            game_memory_write_u8(&a->memory,p+0x1aa,0);game_memory_write_u8(&a->memory,p+0x1b8,0);
        }else put(a,p+0xf5c,UINT32_MAX);
    }
    if(!get(a,p+0xd94) && integer(get(a,p+0xd98))<integer(tick) && integer(get(a,root+0x214))>0)enable(a,p,12);
    if(integer(get(a,p+0x28c))>0)enable(a,p,4);
    if(integer(get(a,p+0x33c))<0x3f000000)enable(a,p,7); /*Retail compares float bits as signed words. */
    if(get(a,p+0x298))enable(a,p,10);
    return !a->memory.fault;
}
static int duplicate(GameApplication *a,const float position[3],int *found){
    GameAddress bucket,node;unsigned visits=0;*found=0;
    if(!gameplay_spatial_query_group(0x5f0800,position[0],position[2],&bucket))return 0;
    node=get(a,bucket+4);
    while(node){
        float x=real(a,node+0x18),z=real(a,node+0x20);
        if(++visits>65536)return 0;
        if(game_memory_read_u8(&a->memory,node+12)==9 && (x==position[0] || isnan(x)) && (z==position[2] || isnan(z))){*found=1;break;}
        node=get(a,node+4);
    }
    return !a->memory.fault;
}
/*4bd770: incremental80x80 placement scan, bit masks, deterministic jitter,
 *visibility/occupancy/duplicate rejection and real factory publication. */
static int placements(GameApplication *a,GameAddress p){
    uint32_t census=get(a,p+0xf54),pending=get(a,p+0x32c),site=get(a,p+0x330),row;
    int count=(int16_t)game_memory_read_u16(&a->memory,0x639a74);GameAddress area,mask,root=get(a,0x58973c);
    float center[2];unsigned spawned=0,b;uint16_t angles[3]={0};
    if(integer(census)>=0){census++;put(a,p+0xf54,census==(uint32_t)count?UINT32_MAX:census);}
    if(!pending)return !a->memory.fault;
    if(site==UINT32_MAX){for(site=0;site<32 && !(pending&(1u<<site));site++);if(site==32)return 0;put(a,p+0x330,site);put(a,p+0x334,0);}
    if(count<0 || site>=(unsigned)count)return 0;
    area=get(a,get(a,0x639a70)+site*4);mask=get(a,get(a,0x604258)+site*4);
    center[0]=(float)(((truncate_word(((long double)real(a,area+4)+real(a,area+12))*real(a,0x57464c))>>9)&255u)*512u);
    center[1]=(float)(((truncate_word(((long double)real(a,area+8)+real(a,area+16))*real(a,0x57464c))>>9)&255u)*512u);
    for(row=get(a,p+0x334);row<80;row++){
        for(b=0;b<10;b++){
            uint8_t bits=game_memory_read_u8(&a->memory,mask+row*10+b);int col;
            if(!bits)continue;
            for(col=(int)b*8+7;col>=(int)b*8;col--){
                uint32_t jitter=212u*(row*(unsigned)col+1u),x,z,variant,id;int found;GameAddress object;
                float position[3]={0};
                /*Mask refresh/publication may change later candidates. */
                bits=game_memory_read_u8(&a->memory,mask+row*10+b);
                if(!(bits&(1u<<(col&7))))continue;
                position[0]=(float)((long double)(col*512)+center[0]+(jitter&511)-real(a,0x5746e4));
                position[2]=(float)((long double)(row*512)+center[1]+(jitter&511)-real(a,0x5746e4));
                /*4c9100 only prints the masks when a candidate is outside
                 *the world. It does not refresh or modify gameplay storage. */
                x=truncate_word(position[0]);z=truncate_word(position[2]);
                if(game_memory_read_u8(&a->memory,0x6020f0+(GameAddress)(integer(z&0xfffff07f)>>7)+(GameAddress)(integer(x)>>12)))continue;
                if(!duplicate(a,position,&found))return 0;if(found)continue;
                if(game_memory_read_u8(&a->memory,0x610520+((z>>9)&255)*32+((x>>9)&255)/8)&(1u<<((x>>9)&7)))continue;
                position[1]=gameplay_geometry_terrain_height(&a->memory,position[0],position[2]);
                if(!game_level_player_catalog_mount(a,root,9,0x20,5,&variant) || variant>UINT16_MAX)return 0;
                object=game_level_factory_take(a,9,(uint16_t)variant);if(!object)return 0;
                id=get(a,root+24);put(a,root+24,id+1);id=(id<<6)|game_memory_read_u16(&a->memory,root+8);
                if(get(a,get(a,object)+0x78)!=0x434780 || !game_level_position_shot_place(a,object,id,position,angles,get(a,get(a,p+0x13c)+0x14),10)){
                    (void)game_level_factory_destroy(a,object);return 0;}
                {uint32_t head=state_helper_flagged_member_offset(get(a,object+16));
                    if(level_player_slots_owns_player(root) && !level_player_slots_attach_scene_member(root,object,head)){
                        (void)game_level_factory_destroy(a,object);return 0;}
                    game_list_insert_after(&a->memory,object,root+head,0x48,0x4c);
                    if(a->memory.fault){level_player_slots_forget_scene_member(object);return 0;}}
                spawned++;
            }
        }
        if(spawned>10){put(a,p+0x334,row+1);return !a->memory.fault;}
    }
    put(a,p+0x330,UINT32_MAX);put(a,p+0x32c,pending&~(1u<<(site&31)));return !a->memory.fault;
}
/*4b4170: choose the actual native class operation by slot/stage, then advance
 *to the next enabled slot. No instruction-label dispatch or CPU re-entry. */
int game_level_imperial_tick(GameApplication *a,GameAddress p){
    static const uint16_t operations[12][3]={{0x1e8,0,0},{0x1dc,0x1e0,0},{0x1f4,0x1c0,0x1c4},{0x1c8,0x1d4,0},
        {0x1cc,0x1d8,0x1f0},{0x1cc,0x1d8,0x1f0},{0x1ec,0,0},{0x1bc,0x1c0,0x1c4},{0x1f8,0,0},
        {0x1fc,0x200,0x204},{0x208,0,0},{0x20c,0x210,0}};
    uint32_t index,flags;unsigned stage,offset,visits=0;
    if(!a || !p || a->memory.fault)return 0;
    if(!decisions(a,p)){fprintf(stderr,"native imperial tick failed: decisions\n");return 0;}
    if(!placements(a,p)){fprintf(stderr,"native imperial tick failed: placements\n");return 0;}
    index=get(a,p+0x1c4);flags=get(a,p+0x1a4);
    if(index>=1 && index<=12 && (flags&(1u<<index))){
        stage=game_memory_read_u8(&a->memory,p+0x1a8+index);offset=stage<3?operations[index-1][stage]:0;
        if(offset){if(!game_level_imperial_operation(a,p,get(a,get(a,p)+offset))){
            fprintf(stderr,"native imperial tick failed: operation=%08x index=%u stage=%u\n",get(a,get(a,p)+offset),index,stage);return 0;}}
        else{put(a,p+0xda0+index*4,get(a,0x5ac6ac));game_memory_write_u8(&a->memory,p+0x1a8+index,0);
            game_memory_write_u8(&a->memory,p+0x1b6+index,0);put(a,p+0x1a4,flags&~(1u<<index));}
    }
    index=get(a,p+0x1c4)+1;if(index==14)index=0;put(a,p+0x1c4,index);flags=get(a,p+0x1a4);
    while(flags && !(flags&(1u<<(index&31)))){
        if(++visits>14){fprintf(stderr,"native imperial tick failed: flags=%08x\n",flags);return 0;}index++;if(index==14)index=0;put(a,p+0x1c4,index);
    }
    return !a->memory.fault;
}
