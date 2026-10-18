#include "vehicle_presentation.h"
#include "gameplay_leaves.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static uint32_t get(GameMemory *m,GameAddress p){return game_memory_read_u32(m,p);}
static void put(GameMemory *m,GameAddress p,uint32_t v){game_memory_write_u32(m,p,v);}
static float real(GameMemory *m,GameAddress p){uint32_t bits=get(m,p);float v;memcpy(&v,&bits,4);return v;}
static void store(GameMemory *m,GameAddress p,float v){uint32_t bits;memcpy(&bits,&v,4);put(m,p,bits);}
static int32_t signed_word(uint32_t bits){int32_t v;memcpy(&v,&bits,4);return v;}
static int16_t signed_short(uint16_t bits){int16_t v;memcpy(&v,&bits,2);return v;}
static uint32_t truncate_word(long double v){return (uint32_t)(uint64_t)(!isfinite(v) || v< -0x1p63L || v>=0x1p63L?INT64_MIN:(int64_t)v);}
static int less_or_unordered(long double a,long double b){return a<b || isnan(a) || isnan(b);}
static void vector(GameMemory *m,GameAddress p,float out[3]){unsigned i;for(i=0;i<3;i++)out[i]=real(m,p+4*i);}
static float random_offset(GameMemory *m){
    uint32_t index=get(m,0x6308c0),sample;
    if(index==1024)index=0;
    if(index>1023){m->fault=1;return 0;}
    put(m,0x6308c0,index+1);
    sample=truncate_word((long double)real(m,0x6308c8+4*index)*real(m,0x5778f4));
    return (float)((long double)signed_word(sample)-real(m,0x5778f8));
}
static int deployment_particles(GameMemory *m,GameAddress o,const VehicleUpdateServices *s){
    float position[3],velocity[3],mirrored[3],bound,height=real(m,o+0x30),old_y=real(m,o+0x1c);
    uint16_t angle;int rising=get(m,o+0x29c)!=0;long double low;
    velocity[0]=random_offset(m);velocity[2]=random_offset(m);
    angle=(uint16_t)(game_memory_read_u16(m,o+0x2ac)+64u);
    if(signed_short(angle)>2048)angle=0;
    game_memory_write_u16(m,o+0x2ac,angle);
    bound=(float)((long double)old_y+(rising?height:-height));
    store(m,o+0x2a4,(float)((long double)real(m,o+0x2a4)+(rising?real(m,0x5778fc):-real(m,0x5778fc))));
    if(rising?real(m,o+0x2a4)>bound:!(real(m,o+0x2a4)>=bound)){
        put(m,o+0x29c,rising?0:1);store(m,o+0x2a4,bound);
    }
    vector(m,o+0x2a0,position);velocity[1]=rising?10.80000019073486328125f:-10.80000019073486328125f;
    if(m->fault || !s->particles || !s->particles(s->context,o,position,velocity,1200,0x5a0450,angle,0,50))return 0;
    /*The first emission may change the record. Reread its live position. */
    low=(long double)real(m,o+0x1c)-real(m,o+0x30);
    mirrored[0]=real(m,o+0x2a0);mirrored[2]=real(m,o+0x2a8);
    mirrored[1]=(float)(low+((long double)real(m,o+0x30)*real(m,0x577908)-((long double)real(m,o+0x2a4)-low)));
    velocity[1]=-velocity[1];angle=(uint16_t)(2048u-game_memory_read_u16(m,o+0x2ac));
    return !m->fault && s->particles(s->context,o,mirrored,velocity,1200,0x5a0450,angle,0,50);
}
static int damage_effect(GameMemory *m,GameAddress o,uint32_t slot,uint32_t multiplier,uint32_t maximum,const VehicleUpdateServices *s){
    GameAddress effect=get(m,o+slot),root;uint32_t identity;float position[3];
    if(signed_word(get(m,o+0x58)*multiplier)<signed_word(maximum)){
        if(effect)return !m->fault;
        if(!s->take_expansion || !(effect=s->take_expansion(s->context)))return 0;
        put(m,o+slot,effect);root=get(m,0x58973c);
        if(!s->next_identity || !s->next_identity(s->context,root,&identity))return 0;
        vector(m,o+slot+4,position);
        if(m->fault || !s->start_expansion || !s->start_expansion(s->context,effect,identity,position,slot==0x2b8?800:900,slot==0x2b8?1000:600,-1))return 0;
        return s->publish_expansion && s->publish_expansion(s->context,get(m,0x58973c),get(m,o+slot));
    }
    if(effect){if(!s->stop_expansion || !s->stop_expansion(s->context,effect))return 0;put(m,o+slot,0);}
    return !m->fault;
}
static int move_effect(GameMemory *m,GameAddress o,uint32_t slot,const VehicleUpdateServices *s){
    float position[3];GameAddress effect=get(m,o+slot);
    if(!effect)return !m->fault;
    vector(m,o+slot+4,position);
    return !m->fault && s->move_expansion && s->move_expansion(s->context,effect,position);
}
/*Two affine products with the original stored binary32 intermediate. These
 *local matrices are call arguments, not persistent copies of model state. */
static void attachment_matrix(GameMemory *m,GameAddress o,float out[12]){
    float first[12],second[12],middle[12];unsigned i,r,c;
    for(i=0;i<12;i++){first[i]=real(m,o+0x180+4*i);second[i]=real(m,o+0x150+4*i);}
    for(r=0;r<3;r++)for(c=0;c<4;c++){
        long double value=(long double)first[4*r]*second[c];
        value+=(long double)first[4*r+1]*second[4+c];
        value+=(long double)first[4*r+2]*second[8+c];
        if(c==3)value+=first[4*r+3];
        middle[4*r+c]=(float)value;
    }
    for(i=0;i<12;i++)first[i]=real(m,o+0x80+4*i);
    for(r=0;r<3;r++)for(c=0;c<4;c++){
        long double value=(long double)first[4*r]*middle[c];
        value+=(long double)first[4*r+1]*middle[4+c];
        value+=(long double)first[4*r+2]*middle[8+c];
        if(c==3)value+=first[4*r+3];
        out[4*r+c]=(float)value;
    }
}
int vehicle_presentation_update(GameMemory *m,GameAddress o,const VehicleUpdateServices *s){
    GameAddress target,table,commander,effect;uint32_t maximum,property,tick,old_stage,stage,lost;float position[3],matrix[12];
    if(!m || m->fault || !s)return 0;
    target=get(m,o+0xc8);if(target && get(m,target+0x14)!=get(m,o+0xcc))put(m,o+0xc8,0);
    if(game_terrain_cell_presence(m,o,0)){
        commander=get(m,o+0x294);
        if(signed_word(get(m,o+0x2b4))<30 && commander && get(m,commander+0x10)==1 && !deployment_particles(m,o,s))return 0;
        tick=get(m,0x5ac6ac);
        if(!(tick&15)){
            if(!s->property || !s->property(s->context,o,get(m,get(m,o)+0x44),&maximum))return 0;
            if(!damage_effect(m,o,0x2b8,2,maximum,s) || !damage_effect(m,o,0x2c8,4,maximum,s))return 0;
        }
        effect=get(m,o+0x2b8);
        if(effect){
            tick=get(m,0x5ac6ac)&3;
            if(!tick){vector(m,effect+0x18,position);if(!s->surface_particles || !s->surface_particles(s->context,o,position,10,0x6297f0))return 0;}
            effect=get(m,o+0x2c8);
            if(effect && tick==2){vector(m,effect+0x18,position);if(!s->surface_particles || !s->surface_particles(s->context,o,position,10,0x6297f0))return 0;}
        }
    }
    target=get(m,o+0xf0);
    if(target && (get(m,target+0x14)!=get(m,o+0xf4) || get(m,0x5f0160+(get(m,o+0x14)&63)*24+(get(m,target+0x14)&63)*4))){put(m,o+0xf0,0);put(m,o+0xf4,0);}
    if(m->fault || !s->expire_input || !s->expire_input(s->context,o+0x264))return 0;
    old_stage=get(m,o+0x2b4);
    if(signed_word(old_stage)<30){
        stage=(uint32_t)(signed_word((get(m,0x5ac6ac)-get(m,o+0x2b0))*30u)/937);
        if(stage==old_stage)return !m->fault;
        if(!s->property || !s->property(s->context,o,0x53c560,&maximum))return 0;
        lost=truncate_word((long double)signed_word((maximum-100u)*old_stage)*real(m,0x577920))-get(m,o+0x58)+100u;
        put(m,o+0x2b4,stage);table=get(m,o);
        if(!s->property(s->context,o,get(m,table+0x44),&maximum))return 0;
        put(m,o+0x58,maximum-lost);
        if(!s->property(s->context,o,get(m,table+0x48),&property))return 0;
        put(m,o+0x5c,property);
        if(!s->property(s->context,o,get(m,table+0x4c),&property))return 0;
        put(m,o+0x60,property);
        if(get(m,o+0x2b4)!=30)return !m->fault;
        return !m->fault && s->command && s->command(s->context,o,get(m,table+0x58),3);
    }
    if(!s->refresh || !s->refresh(s->context,o,get(m,get(m,o)+0x88)))return 0;
    /*The second effect follows the first's ownership gate in the original. */
    if(get(m,o+0x2b8) && (!move_effect(m,o,0x2b8,s) || !move_effect(m,o,0x2c8,s)))return 0;
    if(!(get(m,o+0x268)&0x10000))return !m->fault;
    attachment_matrix(m,o,matrix);target=get(m,o+0x21c);
    if(m->fault || !s->attachment || !s->attachment(s->context,target,get(m,get(m,target)),matrix))return 0;
    put(m,o+0x1e4,get(m,0x5acbe8));return !m->fault;
}

int vehicle_presentation_remove_input(GameMemory *m,GameAddress o,GameAddress event){
    uint8_t flags=game_memory_read_u8(m,o+0xd8);
    switch(game_memory_read_u8(m,event+0x10)){
    case 0x15:if(flags&1)put(m,o+0x268,get(m,o+0x268)|0x10000u);break;
    case 0x16:put(m,o+0x268,get(m,o+0x268)&~0x10000u);break;
    case 0x17:
        game_memory_write_u16(m,o+0x14a,game_memory_read_u16(m,event+0x18));
        game_memory_write_u16(m,o+0x14c,game_memory_read_u16(m,event+0x1a));
        game_memory_write_u8(m,o+0xd8,flags|8);put(m,o+0x1ec,1);break;
    case 0x18:put(m,o+0x268,0);put(m,o+0x26c,0);break;
    default:break; /*5532f0 explicitly ignores every other event code. */
    }
    return !m->fault;
}

/*53b3e0: shared physical movement transaction. Automatic snapshots are rollback
 *locals, not another writable owner. Class programs use native service calls. */
int vehicle_base_motion(GameMemory *m,GameAddress o,const VehicleMotionServices *s){
    static const uint16_t slots[8]={0xe4,0xe8,0xec,0,0xf4,0,0,0xfc};
    uint32_t position[3],world[12],velocity[3],mode;uint16_t angles[3];
    GameAddress table,node,other,chain;float point[3];unsigned i,visits=0;int hit;uint8_t flags;
    if(!m || m->fault || !s || !s->control || !s->refresh)return 0;
    for(i=0;i<3;i++){position[i]=get(m,o+24+i*4);angles[i]=game_memory_read_u16(m,o+36+i*2);}
    for(i=0;i<12;i++)world[i]=get(m,o+0x80+i*4);
    mode=get(m,o+0xe0);table=get(m,o);
    if(mode<8 && slots[mode] && !s->control(s->context,o,get(m,table+slots[mode])))return 0;
    flags=game_memory_read_u8(m,o+0xd8);if(!(flags&8))return !m->fault;
    game_memory_write_u8(m,o+0xd8,flags&~8u);
    if(!s->spatial || !s->spatial(s->context,o,0))return 0;
    table=get(m,o);
    if(!s->refresh(s->context,o,get(m,table+8)) || !s->refresh(s->context,o,get(m,table+12)))return 0;
    for(i=0;i<3;i++)memcpy(point+i,position+i,4);
    if(!s->gather || !s->gather(s->context,o,get(m,table+0x78),point,&hit))return 0;
    if(hit && get(m,get(m,o+0xdc)+12)!=11){
        for(i=0;i<3;i++){put(m,o+24+i*4,position[i]);game_memory_write_u16(m,o+36+i*2,angles[i]);}
        for(i=0;i<12;i++)put(m,o+0x80+i*4,world[i]);
        if(!s->refresh(s->context,o,get(m,table+12)))return 0;
    }
    node=get(m,o+0xb0);
    while(node){
        if(++visits>65536){m->fault=1;return 0;}
        other=get(m,node+12);vector(m,node+16,point);
        for(i=0;i<3;i++)velocity[i]=(get(m,other+16)&2)?get(m,other+0xb4+i*4):0;
        if(!s->contact || !s->contact(s->context,o,other,get(m,table+0x2c),velocity,point))return 0;
        velocity[0]=velocity[1]=velocity[2]=0;
        if(!s->contact(s->context,other,o,get(m,get(m,other)+0x2c),velocity,point))return 0;
        if(!get(m,o+0x58))return !m->fault;
        node=get(m,node+4);
    }
    chain=get(m,o+0xb0);
    if(chain){if(!s->release_contacts || !s->release_contacts(s->context,chain))return 0;put(m,o+0xb0,0);}
    if(!s->control(s->context,o,get(m,table+0x118)))return 0;
    return !m->fault && s->spatial(s->context,o,1);
}
int vehicle_presentation_motion(GameMemory *m,GameAddress o,const VehicleMotionServices *s){
    uint32_t saved[24],velocity[3];uint16_t pitch,yaw;GameAddress command,table,node,other,chain;float point[3];
    long double height;uint8_t flags;unsigned i,visits=0;int hit;
    if(!m || m->fault || !s)return 0;
    pitch=game_memory_read_u16(m,o+0x14a);yaw=game_memory_read_u16(m,o+0x14c);
    for(i=0;i<24;i++)saved[i]=get(m,o+0x150+4*i);
    command=get(m,o+0xdc);
    if(get(m,command+12)==10){
        store(m,o+0x1c,(float)((long double)real(m,o+0x1c)-real(m,0x571484)));
        game_memory_write_u8(m,o+0xd8,game_memory_read_u8(m,o+0xd8)|8);
        if(m->fault || !s->height || !s->height(s->context,o,real(m,o+0x18),real(m,o+0x20),&height))return 0;
        if(!((long double)real(m,o+0x30)*real(m,0x571488)+real(m,o+0x1c)>height)){
            table=get(m,o);
            if(!s->control || !s->control(s->context,o,get(m,table+0x168)))return 0;
            if(!s->height(s->context,o,real(m,o+0x18),real(m,o+0x20),&height))return 0;
            store(m,o+0x1c,(float)(height+(long double)real(m,o+0x30)*real(m,0x571488)));
        }
    }else{
        table=get(m,o);
        if(!s->control || !s->control(s->context,o,get(m,table+0x1c4)) || !s->control(s->context,o,get(m,table+0x1ec)))return 0;
    }
    flags=game_memory_read_u8(m,o+0xd8);if(!(flags&8))return !m->fault;
    game_memory_write_u8(m,o+0xd8,flags&~8);table=get(m,o);
    if(!s->refresh || !s->refresh(s->context,o,get(m,table+8)) || !s->refresh(s->context,o,get(m,table+12)))return 0;
    vector(m,o+0x18,point);
    if(!s->gather || !s->gather(s->context,o,get(m,table+0x78),point,&hit))return 0;
    if(hit){
        game_memory_write_u16(m,o+0x14a,pitch);game_memory_write_u16(m,o+0x14c,yaw);
        for(i=0;i<24;i++)put(m,o+0x150+4*i,saved[i]);
        if(!s->refresh(s->context,o,get(m,table+12)))return 0;
    }
    node=get(m,o+0xb0);if(!node)return !m->fault;
    do{
        if(++visits>65536){m->fault=1;return 0;}
        other=get(m,node+12);vector(m,node+16,point);
        for(i=0;i<3;i++)velocity[i]=(get(m,other+16)&2)?get(m,other+0xb4+4*i):0;
        if(m->fault || !s->contact || !s->contact(s->context,o,other,get(m,table+0x2c),velocity,point))return 0;
        velocity[0]=velocity[1]=velocity[2]=0;
        if(!s->contact(s->context,other,o,get(m,get(m,other)+0x2c),velocity,point))return 0;
        if(!get(m,o+0x58))return !m->fault; /*Death leaves the chain for teardown. */
        node=get(m,node+4);
    }while(node && !m->fault);
    chain=get(m,o+0xb0);
    if(m->fault || !s->release_contacts || !s->release_contacts(s->context,chain))return 0;
    put(m,o+0xb0,0);return !m->fault;
}

/*4035f0 owns the transaction on the live record. Contact callbacks use real
 *arguments; saved values are automatic rollback storage, never a second owner. */
int vehicle_group_motion(GameMemory *m,GameAddress o,const VehicleMotionServices *s){
    uint32_t position[3],world[12],parts[24],velocity[3],state;uint16_t angles[5];
    GameAddress table,node,other,chain;float point[3];unsigned i,visits=0;int hit;uint8_t flags;
    static const uint32_t controls[8]={0xe4,0xe8,0xec,0,0xf4,0,0,0xfc};
    if(!m || m->fault || !s || !s->control || !s->refresh)return 0;
    for(i=0;i<3;i++){position[i]=get(m,o+0x18+4*i);angles[i]=game_memory_read_u16(m,o+0x24+2*i);}
    angles[3]=game_memory_read_u16(m,o+0x14a);angles[4]=game_memory_read_u16(m,o+0x14c);
    for(i=0;i<12;i++)world[i]=get(m,o+0x80+4*i);
    for(i=0;i<24;i++)parts[i]=get(m,o+0x150+4*i);
    state=get(m,o+0xe0);table=get(m,o);
    if(state<8 && controls[state]){
        if(!s->control(s->context,o,get(m,table+controls[state])))return 0;
        if(!state && game_memory_read_u16(m,o+0x24)!=game_memory_read_u16(m,o+0x224) &&
           !s->control(s->context,o,get(m,table+0x1c8)))return 0;
    }
    table=get(m,o);
    if(!s->control(s->context,o,get(m,table+0x1c4)) || !s->control(s->context,o,get(m,table+0x1ec)))return 0;
    flags=game_memory_read_u8(m,o+0xd8);if(!(flags&8))return !m->fault;
    game_memory_write_u8(m,o+0xd8,flags&~8u);
    if(!s->spatial || !s->spatial(s->context,o,0) || !s->refresh(s->context,o,get(m,table+8)) ||
       !s->refresh(s->context,o,get(m,table+12)))return 0;
    memcpy(point,position,12);
    if(!s->gather || !s->gather(s->context,o,get(m,table+0x78),point,&hit))return 0;
    if(hit && !(game_memory_read_u8(m,o+0xd8)&1)){
        for(i=0;i<3;i++){put(m,o+0x18+4*i,position[i]);game_memory_write_u16(m,o+0x24+2*i,angles[i]);}
        game_memory_write_u16(m,o+0x14a,angles[3]);game_memory_write_u16(m,o+0x14c,angles[4]);
        for(i=0;i<12;i++)put(m,o+0x80+4*i,world[i]);
        for(i=0;i<24;i++)put(m,o+0x150+4*i,parts[i]);
        if(get(m,o+0xe0)==7 && signed_word(get(m,o+0x144))>=0){
            if(!s->stop_voice || !s->stop_voice(s->context,get(m,o+0x144)))return 0;
            put(m,o+0x144,UINT32_MAX);
        }
        if(!s->refresh(s->context,o,get(m,table+12)))return 0;
    }
    node=get(m,o+0xb0);
    while(node && !m->fault){
        if(++visits>65536){m->fault=1;return 0;}
        other=get(m,node+12);vector(m,node+16,point);
        for(i=0;i<3;i++)velocity[i]=(get(m,other+16)&2)?get(m,other+0xb4+4*i):0;
        if(!s->contact || !s->contact(s->context,o,other,get(m,table+0x2c),velocity,point))return 0;
        velocity[0]=velocity[1]=velocity[2]=0;
        if(!s->contact(s->context,other,o,get(m,get(m,other)+0x2c),velocity,point))return 0;
        if(!get(m,o+0x58))return !m->fault;
        node=get(m,node+4);
    }
    chain=get(m,o+0xb0);
    if(chain){
        if(!s->release_contacts || !s->release_contacts(s->context,chain))return 0;
        put(m,o+0xb0,0);
    }
    return !m->fault && s->spatial(s->context,o,1);
}

int vehicle_heading_step(GameMemory *m,GameAddress o){
    int current=signed_short(game_memory_read_u16(m,o+0x24)),target=signed_short(game_memory_read_u16(m,o+0x224));
    int difference=current-target,step;int16_t distance;
    if(difference<0)difference=-difference;
    distance=signed_short((uint16_t)difference);
    game_memory_write_u8(m,o+0xd8,game_memory_read_u8(m,o+0xd8)|8);
    if(distance>1024)distance=signed_short((uint16_t)(2048-distance));
    if(distance<=28)game_memory_write_u16(m,o+0x24,(uint16_t)target);
    else{
        step=current>target?(target+1024>=current?-1:1):(current+1024>=target?1:-1);
        game_memory_write_u16(m,o+0x24,(uint16_t)(current+28*step)&2047u);
    }
    return !m->fault;
}
/*The original truncates to signed32, shifts by9, then keeps the low byte.
 *Preserve that wrapping selection, including the ineffective bounds branch. */
static int destination_blocked(GameMemory *m,GameAddress o){
    uint32_t x=(truncate_word(real(m,o+0x18))>>9)&255u,z=(truncate_word(real(m,o+0x20))>>9)&255u;
    return (game_memory_read_u8(m,0x610520+z*32+x/8)&(1u<<(x&7)))!=0;
}
int vehicle_destination_step(GameMemory *m,GameAddress o,const VehicleNavigationServices *s){
    uint32_t old[3];long double speed,height;unsigned i;
    if(!m || m->fault || !s || !s->speed || !s->height)return 0;
    for(i=0;i<3;i++)old[i]=get(m,o+0x18+4*i);
    for(i=0;i<2;i++){
        uint32_t axis=i?0x20:0x18,destination=i?0x100:0xf8,velocity=i?0xb8:0xb4;
        if(!s->speed(s->context,o,&speed))return 0;
        if(!less_or_unordered(fabsl((long double)real(m,o+axis)-real(m,o+destination)),speed+real(m,0x57691c)))
            store(m,o+axis,(float)((long double)real(m,o+velocity)+real(m,o+axis)));
    }
    if(destination_blocked(m,o)){
        for(i=0;i<3;i++)put(m,o+0x18+4*i,old[i]);
        put(m,o+0xe0,5);return !m->fault;
    }
    if(!s->height(s->context,o,real(m,o+0x18),real(m,o+0x20),&height))return 0;
    store(m,o+0x1c,(float)(height+real(m,o+0x30)));
    game_memory_write_u8(m,o+0xd8,game_memory_read_u8(m,o+0xd8)|8);
    if(!s->speed(s->context,o,&speed))return 0;
    if(less_or_unordered(fabsl((long double)real(m,o+0x18)-real(m,o+0xf8)),speed+real(m,0x57691c))){
        if(!s->speed(s->context,o,&speed))return 0;
        if(less_or_unordered(fabsl((long double)real(m,o+0x20)-real(m,o+0x100)),speed+real(m,0x57691c)))put(m,o+0xe0,5);
    }
    return !m->fault;
}
static int16_t navigation_asin(GameMemory *m,float value,int pitch){
    uint32_t bits,index;long double scaled;uint16_t angle;memcpy(&bits,&value,4);
    if(pitch?signed_word(bits)>0x3f666666:value>real(m,0x574cf8)){
        scaled=((long double)value-real(m,0x574cf8))*real(m,0x574cfc);
        index=truncate_word(scaled);angle=game_memory_read_u16(m,0x62b8a8+2*index);
    }else if(bits>0xbf666666u){
        scaled=((long double)real(m,0x574d00)-value)*real(m,0x574cfc);
        index=truncate_word(scaled);angle=(uint16_t)(0u-game_memory_read_u16(m,0x62b8a8+2*index));
    }else{
        scaled=(long double)value*real(m,0x574cc8)+real(m,0x574cc8);
        index=truncate_word(scaled);angle=game_memory_read_u16(m,0x62f8b8+2*index);
    }
    return signed_short(angle);
}
static int navigation_attitude(GameMemory *m,GameAddress o,const VehicleNavigationServices *s,int ascent){
    float normal[3],sum[3],sample[3],x=real(m,o+0x18),z=real(m,o+0x20),sin,cos,squared,length,slope,ratio,pitch_cos;
    uint16_t heading=game_memory_read_u16(m,o+0x24);int16_t roll,pitch;unsigned i,axis;uint32_t bits;
    sin=real(m,0x6298a8+4*(heading&2047u));cos=real(m,0x6298a8+4*((heading+512u)&2047u));
    if(!s->normal || !s->normal(s->context,normal,x,z))return 0;
    memcpy(sum,normal,12);
    for(i=0;i<4;i++){
        unsigned corner=ascent==2?0x140:0x228;
        float px=(float)(((long double)real(m,o+corner+12*i)*cos+(long double)real(m,o+corner+8+12*i)*sin)+x);
        float pz=(float)(((long double)real(m,o+corner+8+12*i)*cos+(long double)real(m,o+corner+12*i)*(-sin))+z);
        if(ascent){
            /*4068b0's full affine product also evaluates the zero Y terms. */
            long double zero_y=(long double)real(m,o+corner+4+12*i)*0.0f;
            if(ascent==2 && i<2){ /*4511a0's first two affine products are inline. */
                px=(float)((((long double)real(m,o+corner+8+12*i)*sin+(long double)real(m,o+corner+12*i)*cos)+zero_y)+x);
                pz=(float)((((long double)real(m,o+corner+12*i)*(-sin)+zero_y)+(long double)real(m,o+corner+8+12*i)*cos)+z);
            }else{
                px=(float)(((zero_y+(long double)real(m,o+corner+8+12*i)*sin)+(long double)real(m,o+corner+12*i)*cos)+x);
                pz=(float)(((zero_y+(long double)real(m,o+corner+8+12*i)*cos)+(long double)real(m,o+corner+12*i)*(-sin))+z);
            }
        }
        if(!s->normal(s->context,sample,px,pz))return 0;
        for(axis=0;axis<3;axis++)sum[axis]=(float)((long double)sum[axis]+sample[axis]);
    }
    squared=(float)(((long double)sum[1]*sum[1]+(long double)sum[2]*sum[2])+(long double)sum[0]*sum[0]);
    {long double root=sqrtl(squared);length=(float)root;memcpy(&bits,&length,4);
        /*Both originals compare the extended sqrt before storing it. */
        if(root!=real(m,0x574cc4) && !isnan(root) && (bits&0x7fffffff)){
            long double reciprocal=(long double)real(m,0x574cc4)/length;
            sum[0]=(float)((long double)sum[0]*reciprocal);sum[2]=(float)((long double)sum[2]*reciprocal);
        }
    }
    slope=(float)((long double)sin*sum[2]-(long double)cos*sum[0]);roll=navigation_asin(m,slope,0);
    pitch_cos=real(m,0x6298a8+4*(((uint16_t)roll+512u)&2047u));memcpy(&bits,&sin,4);
    ratio=(float)((bits&0x7fffffff)?((long double)cos*slope+sum[0])/((long double)pitch_cos*sin):
        ((long double)sum[2]-(long double)sin*slope)/((long double)pitch_cos*cos));
    pitch=navigation_asin(m,ratio,1);
    game_memory_write_u16(m,o+0x28,(uint16_t)((signed_short(game_memory_read_u16(m,o+0x28))*5+roll)/6));
    game_memory_write_u16(m,o+0x26,(uint16_t)((signed_short(game_memory_read_u16(m,o+0x26))*5+pitch)/6));
    return !m->fault;
}
/*4516d0: fixed five-unit steering toward the stored class29 heading. */
int vehicle_class29_steer(GameMemory *m,GameAddress o){
    int current=signed_short(game_memory_read_u16(m,o+36)),target=signed_short(game_memory_read_u16(m,o+0x13c));
    int difference=target-current,magnitude=abs(difference);int16_t distance=signed_short((uint16_t)magnitude);
    if(distance>1024)distance=signed_short((uint16_t)(2048-distance));
    if(distance<=5)current=target;
    else if(current<=1024)current+=(current<target && difference<1024)?5:-5;
    else current+=(current>target && current-target<1024)?-5:5;
    game_memory_write_u16(m,o+36,(uint16_t)current);return !m->fault;
}
/*4511a0 shares destination stepping and the existing terrain-normal algorithm,
 *with class29's own four corners and the original inline arithmetic order. */
int vehicle_class29_destination(GameMemory *m,GameAddress o,const VehicleNavigationServices *s){
    return vehicle_destination_step(m,o,s) && navigation_attitude(m,o,s,2) && vehicle_class29_steer(m,o);
}
int vehicle_group_destination(GameMemory *m,GameAddress o,const VehicleNavigationServices *s){
    uint32_t old[3];float speed,vx,vz;long double value,height;GameAddress command,target,root;unsigned i;
    if(!vehicle_destination_step(m,o,s))return 0;
    for(i=0;i<3;i++)old[i]=get(m,o+0x18+4*i);
    if(!s->speed(s->context,o,&value))return 0;
    speed=(float)value;vx=real(m,o+0xb4);vz=real(m,o+0xb8);
    command=get(m,o+0xdc);target=get(m,command+16);root=get(m,0x58973c);
    if(target && get(m,target+20) && get(m,command+20)==get(m,target+20) && game_memory_read_u8(m,target+12)==0x20 &&
       !get(m,0x5f0160+(game_memory_read_u16(m,root+8)&63u)*24+(get(m,target+20)&63u)*4)){
        float dx=(float)((long double)real(m,o+0x18)-real(m,target+0x18)),dz=(float)((long double)real(m,o+0x20)-real(m,target+0x20)),distance,other_speed;
        if(!s->speed(s->context,target,&value))return 0;
        other_speed=(float)value;
        if(dx<real(m,0x571f14) || isnan(dx))dx=-dx;
        if(dz<real(m,0x571f14) || isnan(dz))dz=-dz;
        distance=(float)(dz<dx || isnan(dz) || isnan(dx)?(long double)dz*real(m,0x571f18)+dx:(long double)dx*real(m,0x571f18)+dz);
        if(!less_or_unordered(distance,real(m,0x574cf4))){
            value=(long double)other_speed/speed;speed=other_speed;vx=(float)((long double)vx*value);vz=(float)((long double)vz*value);
        }
    }
    speed=(float)((long double)speed+real(m,0x574cc4));
    if(!less_or_unordered(fabsl((long double)real(m,o+0x18)-real(m,o+0xf8)),speed))store(m,o+0x18,(float)((long double)real(m,o+0x18)+vx));
    if(!less_or_unordered(fabsl((long double)real(m,o+0x20)-real(m,o+0x100)),speed))store(m,o+0x20,(float)((long double)real(m,o+0x20)+vz));
    if(destination_blocked(m,o)){for(i=0;i<3;i++)put(m,o+0x18+4*i,old[i]);put(m,o+0xe0,5);return !m->fault;}
    if(!s->height(s->context,o,real(m,o+0x18),real(m,o+0x20),&height))return 0;
    store(m,o+0x1c,(float)(height+real(m,o+0x30)));game_memory_write_u8(m,o+0xd8,game_memory_read_u8(m,o+0xd8)|8);
    if(less_or_unordered(fabsl((long double)real(m,o+0x18)-real(m,o+0xf8)),speed) && less_or_unordered(fabsl((long double)real(m,o+0x20)-real(m,o+0x100)),speed)){
        put(m,o+0x18,get(m,o+0xf8));put(m,o+0x20,get(m,o+0x100));put(m,o+0xe0,5);
    }
    return navigation_attitude(m,o,s,0);
}
int vehicle_group_ascent(GameMemory *m,GameAddress o,const VehicleNavigationServices *s){
    long double height;
    if(!m || m->fault || !s || !s->height || !s->ascent_voice)return 0;
    game_memory_write_u8(m,o+0xd8,game_memory_read_u8(m,o+0xd8)|8);
    if(!s->ascent_voice(s->context,o))return 0;
    store(m,o+0x1c,(float)((long double)real(m,o+0x1c)+real(m,0x571064)));
    if(!s->height(s->context,o,real(m,o+0x18),real(m,o+0x20),&height))return 0;
    if(!(height+real(m,o+0x30)>real(m,o+0x1c)))put(m,o+0xe0,5);
    return navigation_attitude(m,o,s,1);
}

int vehicle_presentation_command(GameMemory *m,GameAddress o,uint32_t code,const GamePresentationFeedbackServices *s){
    GameAddress root=get(m,0x5f0208),binding,record,camera;uint32_t index=0,elapsed,tick;int16_t sounds[8];
    uint32_t horizontal,vertical;unsigned i;int accepted;long double dx,dz;
    if(m->fault)return 0;
    if(code>5 || code==2)return 1; /*Actual jump table's empty branch. */
    if(code==0){
        binding=get(m,o+0x294);if(!binding)return !m->fault;
        record=get(m,binding+4);index=get(m,record);record=get(m,get(m,0x639a70)+4*index);
        if(get(m,record+0x54)!=root)return !m->fault;
        elapsed=get(m,0x5ac6ac)-get(m,root+0x590);
        if((signed_word(elapsed)<=160 || get(m,root+0x594)==index) && signed_word(elapsed)<=320)return !m->fault;
        camera=get(m,root+0x448);dx=(long double)real(m,o+0x18)-real(m,camera+0x18);dz=(long double)real(m,o+0x20)-real(m,camera+0x20);
        if(!(dx*dx+dz*dz>real(m,0x577924)))return !m->fault;
        vertical=game_presentation_location_token(m,real(m,o+0x20),0);horizontal=game_presentation_location_token(m,real(m,o+0x18),1);
    }else if((get(m,o+0x14)&63u)!=game_memory_read_u16(m,root+8))return !m->fault;
    if(!s || !s->clear_queue || !s->submit_queue || !s->clear_queue(s->context,get(m,0x628a90)))return 0;
    if(code){vertical=game_presentation_location_token(m,real(m,o+0x20),0);horizontal=game_presentation_location_token(m,real(m,o+0x18),1);}
    for(i=0;i<8;i++)sounds[i]=-1;
    if(code==3){sounds[0]=0x7f;sounds[1]=0x57;sounds[2]=0x5d;sounds[3]=(int16_t)horizontal;sounds[4]=(int16_t)vertical;sounds[5]=0x60;}
    else if(code==4 || code==5){sounds[0]=0x57;sounds[1]=code==4?0x89:0x8a;sounds[2]=0x5d;sounds[3]=(int16_t)horizontal;sounds[4]=(int16_t)vertical;}
    else{sounds[0]=0x57;sounds[1]=0x5d;sounds[2]=(int16_t)horizontal;sounds[3]=(int16_t)vertical;sounds[4]=code?0x5e:0x5f;}
    if(m->fault || !s->submit_queue(s->context,get(m,0x628a90),sounds,&accepted))return 0;
    if(!accepted)return !m->fault;
    root=get(m,0x5f0208);game_memory_write_u16(m,root+0x598,code==3?0x7f:0x57);
    tick=get(m,0x5ac6ac);root=get(m,0x5f0208);
    if(code==0){put(m,root+0x590,tick);root=get(m,0x5f0208);tick=get(m,root+0x590);}
    put(m,root+0x58c,tick);
    if(code==0)put(m,get(m,0x5f0208)+0x594,index);
    return !m->fault;
}
