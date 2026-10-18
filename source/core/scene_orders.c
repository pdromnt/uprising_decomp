#include "scene_orders.h"
#include "recyclable_records.h"
#include "game_state_ops.h"
#include "vehicle_targeting.h"
#include "gameplay_geometry.h"
#include "entity_queries.h"
#include "gameplay_state.h"
#include <math.h>
#include <string.h>

static uint32_t get(GameMemory *m,GameAddress p){return game_memory_read_u32(m,p);}
static void put(GameMemory *m,GameAddress p,uint32_t v){game_memory_write_u32(m,p,v);}
static int32_t integer(uint32_t v){int32_t n;memcpy(&n,&v,4);return n;}
static float real(GameMemory *m,GameAddress p){uint32_t bits=get(m,p);float f;memcpy(&f,&bits,4);return f;}
static uint32_t bits(float f){uint32_t n;memcpy(&n,&f,4);return n;}
static void store(GameMemory *m,GameAddress p,float f){put(m,p,bits(f));}
static uint32_t ftol(long double f){return (uint32_t)(uint64_t)(!isfinite(f)||f< -0x1p63L||f>=0x1p63L?INT64_MIN:(int64_t)f);}
static int32_t shift(uint32_t n,unsigned count){return integer((n>>count)|((n&0x80000000u)?(~0u<<(32u-count)):0u));}
static uint8_t cell(float f){return (uint8_t)shift(ftol(f),9);}
static void vector(GameMemory *m,GameAddress p,float v[3]){unsigned i;for(i=0;i<3;i++)v[i]=real(m,p+i*4);}
static int blocked(GameMemory *m,unsigned x,unsigned z){return game_memory_read_u8(m,0x610520u+z*32u+x/8u)&(1u<<(x&7u));}

/*40ff40: the original byte-cell walk. Equal positive diagonals also test
 *both corner neighbors; unequal slopes retain binary32 stepping/stores. */
int scene_order_segment_clear(GameMemory *m,const float from[3],const float to[3]){
    uint8_t x=cell(from[0]),z=cell(from[2]),tx=cell(to[0]),tz=cell(to[2]);
    int dx=(int)tx-x,dz=(int)tz-z,i,count;float px=from[0],pz=from[2],step,fx,fz;
    if(blocked(m,x,z))return 0;
    if(!dx && !dz)return !m->fault;
    if(blocked(m,tx,tz))return 0;
    if(!dx){int direction=dz<0?-1:1;count=dz<0?-dz:dz;
        for(i=0;i<count;i++){z=(uint8_t)(z+direction);if(blocked(m,x,z))return 0;}
    }else if(!dz){int direction=dx<0?-1:1;count=dx<0?-dx:dx;
        for(i=0;i<count;i++){x=(uint8_t)(x+direction);if(blocked(m,x,z))return 0;}
    }else if(dx==dz){int sx=dx<0?-1:1,sz=dz<0?-1:1;count=dz<0?-dz:dz;
        for(i=0;i<count;i++){uint8_t oldx=x,oldz=z;x=(uint8_t)(x+sx);z=(uint8_t)(z+sz);
            if(blocked(m,x,z)||blocked(m,x,oldz)||blocked(m,oldx,z))return 0;}
    }else{
        int ax=dx<0?-dx:dx,az=dz<0?-dz:dz,along_x=ax>az;
        fx=(float)((long double)to[0]-from[0]);fz=(float)((long double)to[2]-from[2]);
        count=along_x?ax:az;step=(float)((bits(along_x?fx:fz)>0x80000000u?-1.0L:1.0L)*real(m,0x571f24));
        for(i=1;i<=count;i++){uint8_t oldx=x,oldz=z;
            if(along_x){px=(float)((long double)px+step);pz=(float)((long double)i/count*fz+from[2]);}
            else{pz=(float)((long double)pz+step);px=(float)((long double)i/count*fx+from[0]);}
            x=cell(px);z=cell(pz);
            if(blocked(m,x,z) || (x!=oldx && z!=oldz && (blocked(m,oldx,z)||blocked(m,x,oldz))))return 0;
        }
    }
    return !m->fault;
}
int scene_order_position_clear(GameMemory *m,GameAddress o,float x,float z,int *clear){
    float from[3],to[3]={x,0,z};
    if(get(m,get(m,o)+0x188)!=0x4cb750)return 0;
    game_memory_write_u8(m,o+0xd8,game_memory_read_u8(m,o+0xd8)|16u);
    if(bits(x)>0x80000000u || integer(bits(x))>0x48000000 || bits(z)>0x80000000u || integer(bits(z))>0x48000000){*clear=0;return !m->fault;}
    vector(m,o+24,from);*clear=scene_order_segment_clear(m,from,to);return !m->fault;
}
static int advance(GameMemory *m,GameAddress o,const SceneOrderServices *s){
    return get(m,get(m,o)+0x168)==0x53c000 && s->advance && s->advance(s->context,o,0) && !m->fault;
}
/*4ca1e0: the shared three-axis destination writer. Its original callers also
 *consume the incidental nonzero return: only a changed positive-zero X clears
 *it. Preserve that decision as an ordinary result, not register execution. */
int scene_order_destination(GameMemory *m,GameAddress o,const float p[3],int *result){
    unsigned i;int same=1;uint32_t x=bits(p[0]),z=bits(p[2]);
    if(get(m,get(m,o)+0x150)!=0x4ca1e0)return 0;
    game_memory_write_u8(m,o+0xd8,game_memory_read_u8(m,o+0xd8)&~16u);*result=1;
    if(x>0x80000000u || integer(x)>0x48000000 || z>0x80000000u || integer(z)>0x48000000)return !m->fault;
    for(i=0;i<3;i++){float current=real(m,o+24+i*4);if(!(isnan(current)||isnan(p[i])||current==p[i]))same=0;}
    if(!same){for(i=0;i<3;i++)store(m,o+0xf8+i*4,p[i]);put(m,o+0xe0,0);*result=x!=0;}
    return !m->fault;
}
/*53a8a0: probe a short step toward the requested point, then the original
 *cell-edge alternatives, in order. Byte wrap is deliberate. */
int scene_order_steer(GameMemory *m,GameAddress o,const float point[3],const SceneOrderServices *s,int *moved){
    float dx=(float)((long double)point[0]-real(m,o+24)),dz=(float)((long double)point[2]-real(m,o+32)),ratio,x,z;
    uint16_t angle;int sign=1,clear,xd,zd;uint8_t cx,cz;long double speed,wide_ratio;
    if(!(bits(dx)&0x7fffffffu))dx=0.001f;
    if(bits(dx)>0x80000000u)sign=-1;
    wide_ratio=(long double)dz/dx;ratio=(float)wide_ratio;
    if(!(fabsl(wide_ratio)>=real(m,0x57691c)))angle=game_memory_read_u16(m,0x62e8b0+ftol((long double)ratio*real(m,0x57693c)+real(m,0x57693c))*2)&2047u;
    else{angle=integer(bits(ratio))>0?512:1536;angle=(uint16_t)(angle-game_memory_read_u16(m,0x62e8b0+ftol((long double)dx/dz*real(m,0x57693c)+real(m,0x57693c))*2));}
    if(!s->speed || !s->speed(s->context,o,&speed))return 0;
    x=(float)(speed*real(m,0x6298a8+(((uint16_t)(angle+512))&2047)*4)*sign*real(m,0x576940)+real(m,o+24));
    if(!s->speed(s->context,o,&speed))return 0;
    z=(float)(speed*real(m,0x6298a8+(angle&2047)*4)*sign*real(m,0x576940)+real(m,o+32));
    if(!scene_order_position_clear(m,o,x,z,&clear))return 0;
    if(!clear){cx=cell(x);cz=cell(z);xd=(int)cx-cell(real(m,o+24));zd=(int)cz-cell(real(m,o+32));
        if(!xd){x=(float)((uint8_t)(cx-1u)*512u);if(!scene_order_position_clear(m,o,x,z,&clear))return 0;
            if(!clear){x=(float)((uint8_t)(cx+1u)*512u);if(!scene_order_position_clear(m,o,x,z,&clear))return 0;}}
        else if(!zd){z=(float)((uint8_t)(cz-1u)*512u);if(!scene_order_position_clear(m,o,x,z,&clear))return 0;
            /*53aaf7 stores into X after pushing the previous Z. Preserve
             *this cross-axis second probe instead of regularizing the edges. */
            if(!clear){x=(float)((uint8_t)(cz+1u)*512u);if(!scene_order_position_clear(m,o,x,z,&clear))return 0;}}
        else{float original_x=x;x=(float)((uint8_t)(cx+(xd>0?-1:1))*512u);if(!scene_order_position_clear(m,o,x,z,&clear))return 0;
            if(!clear){x=original_x;z=(float)((uint8_t)(cz+(zd>0?-1:1))*512u);if(!scene_order_position_clear(m,o,x,z,&clear))return 0;}}
    }
    *moved=clear;
    return !m->fault && (!clear || (s->move && s->move(s->context,o,x,z)));
}
/*4ca140/405a90 consume the same scene destination/velocity fields. */
int scene_order_move(GameMemory *m,GameAddress o,GameAddress method,float x,float z,const SceneOrderServices *s){
    float dx,dz,point[3],distance,threshold;uint16_t yaw;int16_t pitch;long double scale,speed;
    if(method!=0x4ca140 && method!=0x405a90)return 0;
    game_memory_write_u8(m,o+0xd8,game_memory_read_u8(m,o+0xd8)&~16u);
    if(!(bits(x)>0x80000000u || integer(bits(x))>0x48000000 || bits(z)>0x80000000u || integer(bits(z))>0x48000000) &&
        !((isnan(real(m,o+24)) || isnan(x) || real(m,o+24)==x) && (isnan(real(m,o+32)) || isnan(z) || real(m,o+32)==z))){
        store(m,o+0xf8,x);store(m,o+0x100,z);put(m,o+0xe0,0);put(m,o+0xfc,0x47000000);
    }
    if(method==0x4ca140)return !m->fault;
    dx=(float)((long double)real(m,o+0xf8)-real(m,o+24));dz=(float)((long double)real(m,o+0x100)-real(m,o+32));
    if(!((bits(dx)|bits(dz))&0x7fffffffu)){put(m,o+0xe0,5);return !m->fault;}
    distance=(float)((long double)dz*dz+(long double)dx*dx);scale=(long double)real(m,o+0x128)/sqrtl(distance);
    store(m,o+0xb8,(float)((long double)dz*scale));store(m,o+0xb4,(float)((long double)dx*scale));
    vector(m,o+0xf8,point);if(!vehicle_targeting_point_angles(m,o,point,&yaw,&pitch))return 0;
    game_memory_write_u16(m,o+0x224,yaw);
    if(!s->speed || !s->speed(s->context,o,&speed))return 0;
    threshold=(float)(speed*real(m,0x571060));
    dx=(float)((long double)real(m,o+0xf8)-real(m,o+24));dz=(float)((long double)real(m,o+0x100)-real(m,o+32));
    if(bits(dx)>0x80000000u)dx=-dx;
    if(bits(dz)>0x80000000u)dz=-dz;
    distance=(float)((long double)(dx>dz?dx:dz)+(long double)(dx>dz?dz:dx)*real(m,0x571f18));
    if(!(distance>=threshold)){int difference=(int)(yaw&1023)-(int)(game_memory_read_u16(m,o+36)&1023);
        if(difference<0)difference=-difference;
        if(difference<28)game_memory_write_u16(m,o+0x224,game_memory_read_u16(m,o+36));}
    return !m->fault;
}
static int reacquire(GameMemory *m,GameAddress o,const SceneOrderServices *s,int following,int *issued){
    GameAddress target;PresentationCommand command={0};*issued=0;
    if(integer(get(m,o+0xd4)+16u)>=integer(get(m,0x5ac6ac)))return !m->fault;
    if(!s->acquire || !s->acquire(s->context,o,&target))return 0;
    put(m,o+0xd4,get(m,0x5ac6ac));
    if(target && (!following || target!=get(m,o+0xf0))){command.kind=5;command.target=target;command.has_target=command.has_position=1;
        command.target_identity=get(m,target+20);vector(m,0x594470,command.position);
        if(!s->issue || !s->issue(s->context,o,&command))return 0;
        *issued=1;}
    return !m->fault;
}
static int route_orders(GameMemory *m,GameAddress o,const float destination[3],int following,int first_only,const SceneOrderServices *s){
    GameAddress route,point,followed=following?get(m,o+0xf0):0;uint32_t pending;unsigned attempts=1,visits=0;int ok=0,moved,initialized=0;PresentationCommand command={0};
    if(get(m,0x60c4cc))return !m->fault;
    route=recyclable_record_allocate(0x24);if(!route)return 0;
    if(!recyclable_outer_initialize(m,route,NULL))goto cleanup;
    initialized=1;
    pending=recyclable_route_begin(m,0x60a4c8,cell(destination[0]),cell(destination[2]),cell(real(m,o+24)),cell(real(m,o+32)),route,0xbf800000,NULL);
    while(pending && !m->fault){++attempts;pending=recyclable_route_continue(m,0x60a4c8,route,NULL);
        if(attempts>20){if(pending==get(m,0x60c4cc))put(m,0x60c4cc,0);break;}}
    if(m->fault)goto cleanup;
    if(attempts>20 || integer(get(m,route+24))>0x3f800000){
        if(first_only==2){ok=advance(m,o,s);goto cleanup;}
        if(!scene_order_steer(m,o,destination,s,&moved))goto cleanup;
        if(first_only){if(!moved){GameAddress current=get(m,o+0xdc);unsigned i;put(m,current+16,0);for(i=0;i<3;i++)put(m,current+24+i*4,0);}ok=!m->fault;}
        else ok=moved || advance(m,o,s);
        goto cleanup;
    }
    if(first_only==2){if(!recyclable_route_simplify(m,route,0,NULL) || !s->advance || !s->advance(s->context,o,1))goto cleanup;}
    else if(!s->advance || !s->advance(s->context,o,1) || !recyclable_route_simplify(m,route,0,NULL))goto cleanup;
    point=get(m,route+12);if(!point)goto cleanup;
    game_memory_write_u8(m,point+12,1);
    while(point && !m->fault){uint8_t cursor=game_memory_read_u8(m,point+12),next=(uint8_t)(cursor+1);
        if(++visits>65536)goto cleanup;
        command.position[0]=(float)((unsigned)game_memory_read_u8(m,get(m,point+16)+cursor)*512u);
        command.position[1]=32768;command.position[2]=(float)((unsigned)game_memory_read_u8(m,get(m,point+20)+cursor)*512u);
        game_memory_write_u8(m,point+12,next);
        if(next==32){point=get(m,point+4);if(point)game_memory_write_u8(m,point+12,0);}
        else if(next>=game_memory_read_u8(m,point+13))point=0;
        command.position[0]=(float)((long double)command.position[0]+real(m,first_only==1?0x574970:first_only==2?0x571028:0x576934));
        command.position[2]=(float)((long double)command.position[2]+real(m,first_only==1?0x574970:first_only==2?0x571028:0x576934));
        if(!point)break;
        command.priority=get(m,get(m,o+0xdc)+40);command.kind=0;command.target=first_only==1?0:get(m,o+0xf0);
        command.has_position=1;command.has_target=command.target!=0;command.continue_chain=1;command.flagged=0;command.parameter=0;
        if(command.target)command.target_identity=get(m,command.target+20);
        if(!s->issue || !s->issue(s->context,o,&command))goto cleanup;
        if(first_only==1){command.priority=get(m,get(m,o+0xdc)+40);command.kind=9;command.parameter=32;
            command.continue_chain=0;command.flagged=1;if(!s->issue(s->context,o,&command))goto cleanup;break;}
        if(following){GameAddress world=get(m,0x58973c);
            uint32_t owner=game_memory_read_u16(m,world+8)&63,other=get(m,followed+20)&63;
            if(get(m,0x5f0160+owner*24u+other*4u)){
                command.priority=get(m,get(m,o+0xdc)+40);command.kind=9;command.target=0;command.has_target=0;command.flagged=1;command.continue_chain=0;command.parameter=32;
                if(!s->issue(s->context,o,&command))goto cleanup;
            }
        }
    }
    ok=!m->fault;
cleanup:{GameMemory cleanup=*m;cleanup.fault=0;
    if(initialized){if(!recyclable_record_destroy(&cleanup,route,1,NULL))ok=0;}
    else if(!recyclable_record_release(route))ok=0;}
    return ok && !m->fault;
}
/*4107a0 stores its deltas and final octagonal metric as binary32. */
static float planar(GameMemory *m,const float a[3],const float b[3]){
    float x=(float)((long double)a[0]-b[0]),z=(float)((long double)a[2]-b[2]);
    if(bits(x)>0x80000000u)x=-x;
    if(bits(z)>0x80000000u)z=-z;
    return (float)((long double)(x>z?x:z)+(long double)(x>z?z:x)*real(m,0x571f18));
}
static int sight(GameMemory *m,const SceneOrderServices *s,const float from[3],const float to[3],int *clear){
    return s->line && s->line(s->context,from,to,clear) && !m->fault;
}
static void raised(GameMemory *m,GameAddress o,float p[3]){
    vector(m,o+24,p);p[1]=(float)((long double)p[1]+real(m,o+0x30));
}
static int random_choice(GameMemory *m,long double scale,int32_t *out){
    uint32_t index=get(m,0x6308c0);
    if(index==1024)index=0;
    if(index>1024)return 0;
    *out=integer(ftol((long double)real(m,0x6308c8+index*4)*scale));
    put(m,0x6308c0,index+1);return !m->fault;
}
/*4cb7c0's fallback consumes the existing recyclable route owner. It publishes
 *only interior waypoints, with the original squared-distance budget. */
static int interaction_route(GameMemory *m,GameAddress o,const float target[3],float distance,const SceneOrderServices *s,int *found){
    GameAddress route,point;uint32_t pending;unsigned attempts=1,visits=0;
    float budget,previous[3];int initialized=0,ok=0,clear;PresentationCommand command={0};
    *found=0;if(get(m,0x60c4cc))return !m->fault;
    route=recyclable_record_allocate(0x24);if(!route)return 0;
    if(!recyclable_outer_initialize(m,route,NULL))goto cleanup;
    initialized=1;
    pending=recyclable_route_begin(m,0x60a4c8,cell(real(m,get(m,o+0xf0)+24)),cell(real(m,get(m,o+0xf0)+32)),
        cell(real(m,o+24)),cell(real(m,o+32)),route,0xbf800000,NULL);
    while(pending && !m->fault){++attempts;pending=recyclable_route_continue(m,0x60a4c8,route,NULL);
        if(attempts>20){if(pending==get(m,0x60c4cc))put(m,0x60c4cc,0);break;}}
    if(m->fault)goto cleanup;
    if(attempts>20 || integer(get(m,route+24))>0x3f800000){ok=1;goto cleanup;}
    if(!recyclable_route_simplify(m,route,0,NULL))goto cleanup;
    point=get(m,route+12);if(!point)goto cleanup;
    game_memory_write_u8(m,point+12,1);
    if(!s->advance || !s->advance(s->context,o,1))goto cleanup;
    budget=(float)((long double)distance*real(m,0x57499c));vector(m,get(m,o+0xf0)+24,previous);
    while(point && integer(bits(budget))>0 && !m->fault){
        uint8_t cursor=game_memory_read_u8(m,point+12),next=(uint8_t)(cursor+1);
        float raw[3];long double dx,dz;
        if(++visits>65536)goto cleanup;
        raw[0]=(float)(game_memory_read_u8(m,get(m,point+16)+cursor)*512u);raw[1]=32768;
        raw[2]=(float)(game_memory_read_u8(m,get(m,point+20)+cursor)*512u);
        game_memory_write_u8(m,point+12,next);
        if(next==32){point=get(m,point+4);if(point)game_memory_write_u8(m,point+12,0);}
        else if(next>=game_memory_read_u8(m,point+13))point=0;
        dx=(long double)previous[0]-raw[0];dz=(long double)previous[2]-raw[2];
        budget=(float)((long double)budget-(dx*dx+dz*dz));memcpy(previous,raw,12);
        command.position[0]=(float)((long double)raw[0]+real(m,0x574970));command.position[1]=32768;
        command.position[2]=(float)((long double)raw[2]+real(m,0x574970));
        if(point){command.priority=get(m,get(m,o+0xdc)+40);command.kind=0;command.target=get(m,o+0xf0);
            command.has_target=command.target!=0;command.has_position=1;command.continue_chain=1;
            if(command.target)command.target_identity=get(m,command.target+20);
            if(!s->issue || !s->issue(s->context,o,&command))goto cleanup;}
        if(!sight(m,s,command.position,target,&clear))goto cleanup;
        if(!clear)break;
    }
    *found=1;ok=!m->fault;
cleanup:{GameMemory cleanup=*m;cleanup.fault=0;
    if(initialized){if(!recyclable_record_destroy(&cleanup,route,1,NULL))ok=0;}
    else if(!recyclable_record_release(route))ok=0;}
    return ok && !m->fault;
}
/*4cb410/4cbc00/4cb7c0 share eight candidate directions but have distinct line
 *predicates, target heights, consumers and route fallback. No state mirror. */
static int interaction_fan(GameMemory *m,GameAddress o,GameAddress method,const SceneOrderServices *s,int *found){
    static const int dx[8]={-1,0,1,1,1,0,-1,-1},dz[8]={1,1,1,0,-1,-1,-1,0};
    float origin[3],target[3],best[3]={0},radius=32,distance=0;GameAddress t;unsigned sweep=0,i;
    int clear,pass;long double rx,rz;
    *found=0;vector(m,o+24,origin);
    t=get(m,o+(method==0x4cb410?0xc8:0xf0));if(!t)return 0;
    raised(m,t,target);
    if(method==0x4cb410){float from[3];raised(m,o,from);
        if(!sight(m,s,from,target,&clear))return 0;
        if(clear){for(i=0;i<3;i++)store(m,o+0xbc+i*4,origin[i]);
            store(m,o+0xbc,(float)((long double)origin[0]+real(m,0x574980)));*found=1;return !m->fault;}
        target[1]=real(m,t+0x30); /*This phase replaces Y rather than adding. */
    }
    if(method==0x4cb7c0){uint32_t range=get(m,o+0x120);
        rx=(long double)target[0]-origin[0];rz=(long double)target[2]-origin[2];distance=(float)(rx*rx+rz*rz);
        if(!((long double)integer(range*range)*real(m,0x574998)>=distance))return !m->fault;
    }
    for(;;){float scale=(float)((long double)real(m,o+0x128)*radius),best_distance=0;
        if(++sweep>65536)return 0;
        for(i=0;i<8;i++){float p[3],d;
            p[0]=(float)((long double)dx[i]*scale+origin[0]);p[2]=(float)((long double)dz[i]*scale+origin[2]);
            p[1]=(float)(gameplay_geometry_terrain_height(m,p[0],p[2])+(long double)real(m,o+0x30)*(method==0x4cb410?real(m,0x574988):1.0L));
            if(!sight(m,s,p,target,&clear))return 0;
            if(method==0x4cb7c0 ? clear : !clear)continue;
            if(method==0x4cbc00){float farther[3];long double multiplier=real(m,0x57499c);
                /*Original comparison retains the lesser coordinate, then adds
                 *the scaled absolute delta. The farther line must be blocked. */
                farther[0]=(float)((p[0]>origin[0]?origin[0]:p[0])+fabsl((long double)p[0]-origin[0])*multiplier);
                farther[2]=(float)((p[2]>origin[2]?origin[2]:p[2])+fabsl((long double)p[2]-origin[2])*multiplier);
                farther[1]=(float)(gameplay_geometry_terrain_height(m,farther[0],farther[2])+real(m,o+0x30));
                if(!sight(m,s,farther,target,&clear))return 0;
                if(clear)continue;
            }
            if(!scene_order_position_clear(m,o,p[0],p[2],&pass))return 0;
            if(!pass)continue;
            d=planar(m,p,origin);
            if((bits(best_distance)&0x7fffffffu) && d>=best_distance)continue;
            if(method==0x4cbc00 && !(planar(m,target,origin)>=d))continue;
            best_distance=d;memcpy(best,p,12);
        }
        if(bits(best_distance)&0x7fffffffu){*found=1;
            if(method==0x4cb410){for(i=0;i<3;i++)store(m,o+0xbc+i*4,best[i]);return !m->fault;}
            return s->move && s->move(s->context,o,best[0],best[2]) && !m->fault;
        }
        {long double next=(long double)radius+real(m,0x57498c);radius=(float)next;
            if(next>real(m,0x574990))break;}
    }
    return method==0x4cb7c0?interaction_route(m,o,target,distance,s,found):!m->fault;
}
int scene_order_interaction(GameMemory *m,GameAddress o,GameAddress method,const SceneOrderServices *s,int *result){
    float from[3],to[3];GameAddress target;int clear,found;int32_t choice;unsigned retries=0;PresentationCommand command={0};
    *result=0;
    if(method==0x4cb410 || method==0x4cbc00 || method==0x4cb7c0)return interaction_fan(m,o,method,s,result);
    if(method==0x4cb670){vector(m,o+0xbc,from);vector(m,o+24,to);
        if(!((bits(from[0])|bits(from[2]))&0x7fffffffu) || planar(m,from,to)>real(m,0x574994))return !m->fault;
        target=get(m,o+0xc8);if(!target)return 0;raised(m,target,to);
        if(!sight(m,s,from,to,&clear))return 0;
        return !clear?!m->fault:scene_order_position_clear(m,o,from[0],from[2],result);
    }
    if(method!=0x4cb010 && method!=0x4cb2b0)return 0;
    target=get(m,o+(method==0x4cb010?0xc8:0xf0));
    if(!get(m,o+0xf0) || !target || get(m,target+20)!=get(m,o+(method==0x4cb010?0xcc:0xf4)))return advance(m,o,s);
    if(method==0x4cb2b0 && get(m,o+0xe0)!=5)return !m->fault;
    raised(m,o,from);raised(m,target,to);if(!sight(m,s,from,to,&clear))return 0;
    if(method==0x4cb2b0){
        if(clear){if(!random_choice(m,real(m,0x574954),&choice))return 0;
            if(choice>integer(get(m,o+0x118)))return !m->fault;
            if(!random_choice(m,real(m,0x57497c),&choice))return 0;
            if(choice!=1)return !m->fault;}
        if(get(m,get(m,o)+0x190)!=0x4cbc00 || !interaction_fan(m,o,0x4cbc00,s,&found))return 0;
        if(found)return !m->fault;
        if(!clear){put(m,get(m,o+0xdc)+12,5);return !m->fault;}
        if(get(m,get(m,o)+0x18c)!=0x4cb7c0 || !interaction_fan(m,o,0x4cb7c0,s,&found))return 0;
        return found?!m->fault:advance(m,o,s);
    }
    if(!clear){
        if(!random_choice(m,real(m,0x574954),&choice))return 0;
        if(choice<integer(get(m,o+0x118)) && integer(get(m,o+0x12c))>32)return s->attack && s->attack(s->context,o,target);
        if(!random_choice(m,real(m,0x574974),&choice))return 0;
        if(choice>=integer(get(m,o+0x114)) && integer(get(m,o+0x12c))>32)return !m->fault;
        if(get(m,get(m,o)+0x144)!=0x4cb410 || !interaction_fan(m,o,0x4cb410,s,&found))return 0;
        if(found)return s->move && s->move(s->context,o,real(m,o+0xbc),real(m,o+0xc4));
        command.kind=6;command.target=get(m,o+0xc8);command.has_target=command.has_position=1;
        command.target_identity=get(m,command.target+20);vector(m,0x594470,command.position);
        return s->issue && s->issue(s->context,o,&command) && !m->fault;
    }
    if(!s->acquire || !s->acquire(s->context,o,&target))return 0;
    while(target){ /*The original never advances this candidate. Preserve RNG
                   *retries; reject its nonterminating/corrupt cases explicitly. */
        if(++retries>65536)return 0;
        if(target!=get(m,o+0xc8)){vector(m,target+24,to);vector(m,o+24,from);
            if(!(planar(m,from,to)>=real(m,0x574978))){
                if(!random_choice(m,real(m,0x574954),&choice))return 0;
                if(choice<integer(get(m,o+0x118)))return s->attack && s->attack(s->context,o,target);
            }else return 0; /*Unchanging candidate: original infinite loop. */
        }else return 0;
    }
    if(!random_choice(m,integer(get(m,o+0x114)<<8),&choice))return 0;
    if(choice<integer(get(m,o+0x118)))return s->target_order && s->target_order(s->context,o,0,get(m,o+0xc8));
    return !m->fault;
}
static int travel(GameMemory *m,GameAddress o,int following,const SceneOrderServices *s){
    int issued,clear,moved;float destination[3];long double speed;
    if(!reacquire(m,o,s,following,&issued) || issued)return issued && !m->fault;
    if(get(m,o+0xe0)!=5)return !m->fault;
    if(following){GameAddress target=get(m,o+0xf0);if(!target || get(m,target+20)!=get(m,o+0xf4))return advance(m,o,s);
        unsigned i;for(i=0;i<3;i++)put(m,o+0xe4+i*4,get(m,target+24+i*4));vector(m,o+0xe4,destination);
        if(fabsl((long double)real(m,o+24)-destination[0])<real(m,0x57692c) && fabsl((long double)real(m,o+32)-destination[2])<real(m,0x57692c))return advance(m,o,s);
    }else{vector(m,o+0xe4,destination);
        if(!s->speed || !s->speed(s->context,o,&speed))return 0;
        if(fabsl((long double)real(m,o+24)-destination[0])<speed+real(m,0x57691c)){
            if(!s->speed(s->context,o,&speed))return 0;
            if(fabsl((long double)real(m,o+32)-destination[2])<speed+real(m,0x57691c))return advance(m,o,s);}
    }
    if(!scene_order_position_clear(m,o,destination[0],destination[2],&clear))return 0;
    if(clear){if(!following)return s->move && s->move(s->context,o,destination[0],destination[2]);
        return scene_order_steer(m,o,destination,s,&moved) && (moved || advance(m,o,s));}
    if(!following && blocked(m,cell(destination[0]),cell(destination[2]))){
        if(!scene_order_steer(m,o,destination,s,&moved))return 0;
        if(!moved)return advance(m,o,s);}
    return route_orders(m,o,destination,following,0,s);
}
/*4cad20: command target/point following. It queues the first interior route
 *point and a wait, unlike the full travel route and the LOS-budget route. */
static int group_travel(GameMemory *m,GameAddress o,const SceneOrderServices *s){
    GameAddress command=get(m,o+0xdc),target=get(m,command+16);float destination[3];unsigned i;int clear,moved;
    if(target && get(m,target+20)!=get(m,command+20))return advance(m,o,s);
    vector(m,target?target+24:command+24,destination);
    for(i=0;i<3;i++)store(m,o+0xe4+i*4,destination[i]);
    if(!(fabsl((long double)real(m,o+24)-destination[0])>=real(m,0x574964)) &&
       !(fabsl((long double)real(m,o+32)-destination[2])>=real(m,0x574964)))return !m->fault;
    if(!scene_order_position_clear(m,o,destination[0],destination[2],&clear))return 0;
    if(!clear)return route_orders(m,o,destination,0,1,s);
    if(get(m,get(m,o)+0x150)!=0x53a8a0 || !scene_order_steer(m,o,destination,s,&moved))return 0;
    if(!moved){command=get(m,o+0xdc);put(m,command+16,0);for(i=0;i<3;i++)put(m,command+24+i*4,0);}
    return !m->fault;
}
static uint32_t priority(GameMemory *m,GameAddress o){return get(m,get(m,o+0xdc)+40);}
static int restore_speed(GameMemory *m,GameAddress o,const SceneOrderServices *s){long double value;
    if(!s->speed || !s->speed(s->context,o,&value))return 0;
    store(m,o+0x128,(float)value);return !m->fault;
}
static int territory_contains(GameMemory *m,GameAddress group,GameAddress o,long double margin){
    long double x=real(m,o+24),z=real(m,o+32);
    return !(real(m,group+4)-margin>x) && real(m,group+12)+margin>=x &&
        !(real(m,group+8)-margin>z) && real(m,group+16)+margin>=z;
}
static int territory_target(GameMemory *m,GameAddress group,GameAddress *target,int *found){
    int count=(int16_t)game_memory_read_u16(m,group+0x26),i;GameAddress records=get(m,group+24),record=0,method,payload;
    *found=0;*target=0;
    for(i=0;i<count;i++){record=get(m,records+(uint32_t)i*4);if(record)break;}
    if(!record)return !m->fault;
    method=get(m,get(m,record)+16);
    if(method!=0x53e470 && method!=0x53f170 && method!=0x53fc40)return 0;
    payload=get(m,record+0x48); /*Shared descriptor getters return a live ID only. */
    if(get(m,payload+20))*target=payload;
    *found=1;return !m->fault;
}
static int territory_attack(GameMemory *m,GameAddress o,GameAddress group,const SceneOrderServices *s){
    GameAddress target;int found;PresentationCommand request={0};
    if(!territory_target(m,group,&target,&found))return 0;
    if(!found)return !m->fault;
    request.priority=priority(m,o);request.kind=5;request.target=target;
    request.has_target=target!=0;request.has_position=1;request.target_identity=target?get(m,target+20):0;
    vector(m,0x594470,request.position);return s->issue && s->issue(s->context,o,&request) && !m->fault;
}
/*53abd0: find a territory, descend into its rectangle, capture or target the
 *existing descriptor payload. Capturing uses the real native citadel owner. */
static int territory_order(GameMemory *m,GameAddress o,const SceneOrderServices *s){
    GameAddress group=get(m,o+0x104),owner;int count,i,expanded,found;uint16_t status;
    float point[3];long double speed,height;PresentationCommand request={0};
    if(!get(m,o+0xe0))return !m->fault;
    if(!group){
        for(expanded=0;expanded<2 && !group;expanded++){
            count=(int16_t)game_memory_read_u16(m,0x639a74);
            for(i=0;i<count;i++){GameAddress candidate=get(m,get(m,0x639a70)+(uint32_t)i*4);
                if(territory_contains(m,candidate,o,expanded?real(m,o+0x128):0)){group=candidate;put(m,o+0x104,group);break;}}
        }
        if(!group)return restore_speed(m,o,s) && advance(m,o,s);
        owner=get(m,group+0x54);
        if(owner==get(m,0x58973c) && get(m,get(m,group+24)))return restore_speed(m,o,s) && advance(m,o,s);
        if(owner && owner!=get(m,0x58973c)){
            GameAddress target;
            if(!restore_speed(m,o,s) || !territory_target(m,group,&target,&found))return 0;
            if(!found)return !m->fault;
            store(m,o+0xe4,(float)(((long double)real(m,group+4)+real(m,group+12))*real(m,0x576944)));
            store(m,o+0xec,(float)(((long double)real(m,group+8)+real(m,group+16))*real(m,0x576944)));
            request.priority=priority(m,o);request.kind=0;request.has_position=1;vector(m,o+0xe4,request.position);
            if(!s->issue || !s->issue(s->context,o,&request))return 0;
            return territory_attack(m,o,get(m,o+0x104),s);
        }
    }
    height=(long double)real(m,o+28)-gameplay_geometry_terrain_height(m,real(m,o+24),real(m,o+32));
    if(height>real(m,0x576948)){
        if(territory_contains(m,group,o,0)){point[0]=real(m,o+24);point[2]=real(m,o+32);}
        else{point[0]=(float)(((long double)real(m,group+4)+real(m,group+12))*real(m,0x576944));
            point[2]=(float)(((long double)real(m,group+8)+real(m,group+16))*real(m,0x576944));}
        point[1]=(float)((long double)real(m,o+28)-real(m,0x57694c));
        speed=(long double)real(m,o+0x128)*real(m,0x576950);
        store(m,o+0x128,(float)(speed>real(m,0x576954)?speed:real(m,0x576954)));
        request.priority=priority(m,o);request.kind=0;request.has_position=1;memcpy(request.position,point,12);
        return s->issue && s->issue(s->context,o,&request) && !m->fault;
    }
    if(!restore_speed(m,o,s) || !s->capture || !s->capture(s->context,get(m,o+0x104),get(m,0x58973c),&status))return 0;
    return status==2?territory_attack(m,o,get(m,o+0x104),s):advance(m,o,s);
}
/*53bb50: the timed four-phase collision detour uses actual command coordinates,
 *heading/table arithmetic and existing velocity fields, not a CPU callback. */
static int collision_detour(GameMemory *m,GameAddress o,const SceneOrderServices *s){
    GameAddress target,command;uint32_t phase,tick=get(m,0x5ac6ac);int pass;float x,z,sx,sz,scale;long double speed,wide,offset;
    if((get(m,o+16)&0x4000u) || integer(get(m,o+0x134))>=integer(tick))return !m->fault;
    target=get(m,o+0x138);command=get(m,o+0xdc);
    if(!target || !get(m,command+16) || get(m,target+20)!=get(m,get(m,command+16)+20))return advance(m,o,s);
    phase=get(m,o+0x130);if(phase>3)return !m->fault;if(phase==3)return advance(m,o,s);
    if(!phase){uint16_t heading;int16_t signed_heading,current;int difference;long double delta;
        sz=(float)((long double)real(m,o+32)-real(m,command+32));
        delta=(long double)real(m,o+24)-real(m,command+24);sx=(float)delta;
        if((delta==real(m,0x576914) || isnan(delta) || isnan(real(m,0x576914))) && !(bits(sz)&0x7fffffffu))return advance(m,o,s);
        if(!(fabsl((long double)sx)>=fabsl((long double)sz))){
            heading=game_memory_read_u16(m,0x62e8b0+ftol((long double)sx*real(m,0x57693c)/sz+real(m,0x57693c))*2);
            heading=integer(bits(sz))>0?heading&2047u:(uint16_t)(heading+1024u);
        }else{heading=integer(bits(sx))>0?512:1536;
            heading=(uint16_t)(heading-game_memory_read_u16(m,0x62e8b0+ftol((long double)sz*real(m,0x57693c)/sx+real(m,0x57693c))*2));}
        memcpy(&signed_heading,&heading,2);heading=game_memory_read_u16(m,o+36);memcpy(&current,&heading,2);
        difference=(int)current-signed_heading;
        if(difference<512 && difference> -512)return advance(m,o,s);
        game_memory_write_u8(m,o+0xd8,(game_memory_read_u8(m,o+0xd8)&~2u)|(difference>1024?2u:0u));
        if(!s->speed || !s->speed(s->context,o,&speed))return 0;
        speed*=real(m,0x576940);
        z=(float)((long double)real(m,o+32)-speed*real(m,0x6298a8+(((uint16_t)(heading+512))&2047u)*4));
        x=(float)((long double)real(m,o+24)-speed*real(m,0x6298a8+(heading&2047u)*4));
        put(m,o+0x134,tick+32u);put(m,o+0x130,1);
        if(!scene_order_position_clear(m,o,x,z,&pass))return 0;
        return !pass?!m->fault:(s->move && s->move(s->context,o,x,z));
    }
    if(game_memory_read_u8(m,o+0xd8)&2u){sz=-real(m,o+0xb4);sx=real(m,o+0xb8);}
    else{sz=real(m,o+0xb4);sx=-real(m,o+0xb8);}
    wide=((long double)real(m,target+0x38)+(phase==1?real(m,o+0x38):0.0L))/real(m,o+0x128);
    if(!(wide>real(m,0x576940)))wide=real(m,0x576940);
    scale=(float)wide;offset=wide*sz;sz=(float)offset;
    z=(float)((long double)real(m,o+32)+offset);
    sx=(float)((long double)sx*scale);
    /*The X product is stored with FSTS, then its extended value is added. */
    if(game_memory_read_u8(m,o+0xd8)&2u)offset=(long double)real(m,o+0xb8)*scale;
    else offset=-(long double)real(m,o+0xb8)*scale;
    x=(float)((long double)real(m,o+24)+offset);
    if(!scene_order_position_clear(m,o,x,z,&pass))return 0;
    if(!pass){x=(float)((long double)real(m,o+24)-sx);z=(float)((long double)real(m,o+32)-sz);
        if(phase==1){if(!scene_order_position_clear(m,o,x,z,&pass))return 0;if(!pass)return advance(m,o,s);}}
    put(m,o+0x134,tick+ftol(scale));put(m,o+0x130,phase+1);
    if(phase==2){if(!scene_order_position_clear(m,o,x,z,&pass))return 0;if(!pass)return !m->fault;}
    return s->move && s->move(s->context,o,x,z) && !m->fault;
}
static int change_kind(GameMemory *m,GameAddress o,uint32_t kind,const SceneOrderServices *s){
    put(m,get(m,o+0xdc)+12,kind);return !m->fault && s->publish && s->publish(s->context,o);
}
static int vehicle_order_fan(GameMemory *m,GameAddress o,unsigned slot,GameAddress expected,const SceneOrderServices *s,int *found){
    return get(m,get(m,o)+slot)==expected && interaction_fan(m,o,expected,s,found);
}
static int vehicle_order_step(GameMemory *m,GameAddress o,float x,float z,const SceneOrderServices *s,int *moved){
    if(!scene_order_position_clear(m,o,x,z,moved))return 0;
    return !*moved || (s->move && s->move(s->context,o,x,z));
}
/*403cd0/404410/4046c0: attack, sidestep and secondary-target orders consume
 *the same target generations, RNG, command owner and recyclable routes. */
static int vehicle_combat_order(GameMemory *m,GameAddress o,GameAddress method,const SceneOrderServices *s){
    GameAddress target=get(m,o+(method==0x4046c0?0xc8:0xf0)),command=get(m,o+0xdc);
    float from[3],to[3],dx,dz,distance,x,z;uint32_t range;int clear,found,moved;int32_t choice;
    if(method!=0x4046c0 && get(m,command+12)!=(method==0x403cd0?5u:6u))return !m->fault;
    if(!target || get(m,target+20)!=get(m,o+(method==0x4046c0?0xcc:0xf4)))return advance(m,o,s);
    if(get(m,o+0xe0)!=5)return !m->fault;
    if(method==0x403cd0){
        if(!random_choice(m,real(m,0x571014),&choice))return 0;
        if(choice==13){if(!vehicle_order_fan(m,o,0x190,0x4cbc00,s,&found))return 0;
            if(found)return change_kind(m,o,7,s);}
    }
    raised(m,o,from);raised(m,target,to);
    if(method==0x404410){dx=(float)((long double)real(m,o+24)-real(m,target+24));dz=(float)((long double)real(m,o+32)-real(m,target+32));
        range=get(m,o+0x120);
        if(!((long double)integer(range*range)*real(m,game_memory_read_u8(m,target+12)==0x20?0x571018:0x57101c)>=
            (long double)dx*dx+(long double)dz*dz))return advance(m,o,s);
    }
    if(!sight(m,s,from,to,&clear))return 0;
    if(method==0x4046c0){PresentationCommand request={0};
        if(!clear){request.kind=6;request.target=target;request.has_target=request.has_position=1;
            request.target_identity=get(m,target+20);vector(m,0x594470,request.position);
            return s->issue && s->issue(s->context,o,&request);}
        if(!random_choice(m,integer(get(m,o+0x12c)),&choice))return 0;
        if(choice<=integer(get(m,o+0x118)))return !m->fault;
        put(m,o+0xf0,get(m,o+0xc8));put(m,o+0xf4,get(m,get(m,o+0xc8)+20));
        if(!vehicle_order_fan(m,o,0x18c,0x4cb7c0,s,&found))return 0;
        return found?!m->fault:advance(m,o,s);
    }
    if(clear){if(!vehicle_order_fan(m,o,0x18c,0x4cb7c0,s,&found))return 0;if(found)return !m->fault;}
    target=get(m,o+0xf0);
    dx=(float)((long double)real(m,target+24)-real(m,o+24));dz=(float)((long double)real(m,target+32)-real(m,o+32));
    if(method==0x404410){
        if(!random_choice(m,real(m,0x571034),&choice))return 0;
        if(choice<integer(get(m,o+0x118)) || (long double)dx*dx+(long double)dz*dz>real(m,0x57103c))return change_kind(m,o,5,s);
        dx=(float)((long double)dx*real(m,0x571040));dz=(float)((long double)dz*real(m,0x571040));
        if(!random_choice(m,real(m,0x571044),&choice))return 0;
        x=choice?dz:-dz;z=choice?-dx:dx;
        x=(float)((long double)x+real(m,o+24));z=(float)((long double)z+real(m,o+32));
        return vehicle_order_step(m,o,x,z,s,&moved);
    }
    if(clear){range=get(m,o+0x120);
        if(!((long double)integer(range*range)*real(m,game_memory_read_u8(m,target+12)==0x20?0x571018:0x57101c)>=
            (long double)dx*dx+(long double)dz*dz))return advance(m,o,s);
        if(!random_choice(m,real(m,0x571020),&choice))return 0;
        choice=integer((uint32_t)choice+5u);
        x=(float)((long double)dx/choice+real(m,o+24));z=(float)((long double)dz/choice+real(m,o+32));
        if(!vehicle_order_step(m,o,x,z,s,&moved))return 0;
        if(moved)return !m->fault;
    }else{vector(m,o+24,from);vector(m,target+24,to);distance=planar(m,from,to);
        if(integer(bits(distance))<=0x44fa0000){int32_t truncated=integer(ftol(distance));float as_float=(float)truncated;
            if(!random_choice(m,truncated,&choice))return 0;
            if(choice<integer(get(m,o+0x114)))return change_kind(m,o,6,s);
            if(integer(bits(distance))>=0x42c80000){if(!random_choice(m,as_float,&choice))return 0;if(choice>=10)return !m->fault;}
            x=(float)(((long double)real(m,o+24)-real(m,target+24))*real(m,0x571038)+real(m,o+24));
            z=(float)(((long double)real(m,o+32)-real(m,target+32))*real(m,0x571038)+real(m,o+32));
            return vehicle_order_step(m,o,x,z,s,&moved);
        }
        x=(float)((long double)real(m,o+24)-((long double)real(m,o+24)-real(m,target+24))*real(m,0x571030));
        z=(float)((long double)real(m,o+32)-((long double)real(m,o+32)-real(m,target+32))*real(m,0x571030));
        if(!vehicle_order_step(m,o,x,z,s,&moved))return 0;
        if(moved)return !m->fault;
        if(get(m,0x60c4cc))return !m->fault;
        if(!recyclable_route_reachable(m,0x60a4c8,cell(to[0]),cell(to[2]),cell(from[0]),cell(from[2]),&found))return 0;
        if(!found)return advance(m,o,s);
    }
    vector(m,get(m,o+0xf0)+24,to);return route_orders(m,o,to,0,2,s);
}
/*4061e0: landing sound lifetime and terrain-height gate. */
static int vehicle_landing_order(GameMemory *m,GameAddress o,const SceneOrderServices *s){
    long double ground;
    if(get(m,o+0xe0)==7)return !m->fault;
    if(integer(get(m,o+0x144))<0 && (!s->landing_sound || !s->landing_sound(s->context,o,0)))return 0;
    ground=gameplay_geometry_terrain_height(m,real(m,o+24),real(m,o+32))+real(m,o+0x30);
    if(!(ground>real(m,o+28))){
        if(!s->landing_sound || !s->landing_sound(s->context,o,1))return 0;
        put(m,o+0x144,UINT32_MAX);return advance(m,o,s);
    }
    put(m,o+0xe0,7);return !m->fault;
}
static int group_target(GameMemory *m,GameAddress o,const SceneOrderServices *s){
    GameAddress command=get(m,o+0xdc),target=get(m,command+16),leader,follower,root;unsigned visits=0;
    float from[3],to[3];int active;int32_t score;NativeEntityRelationshipResult relation;PresentationCommand request={0};
    if(target && get(m,target+20)!=get(m,command+20))return advance(m,o,s);
    if(integer(get(m,o+0xd4)+16u)>integer(get(m,0x5ac6ac)))return !m->fault;
    leader=get(m,o+0x108);
    if(leader && game_memory_read_u8(m,leader+12)!=0xfe){GameAddress next;
        while((next=get(m,leader+0x108)) && game_memory_read_u8(m,next+12)!=0xfe){if(++visits>65536)return 0;leader=next;}
        command=get(m,leader+0xdc);
        if(get(m,command+12)!=12){vector(m,o+24,from);vector(m,leader+24,to);
            /*The original compares range/4 against distance; it enters this
             *branch when the leader is closer than that threshold. */
            if((long double)shift(get(m,o+0x120),2)>planar(m,from,to)){
                if(!native_entity_type_activity(m,leader,&active))return 0;
                if(!active && get(m,command+12)!=13)return !m->fault;
                target=get(m,command+12)==13?get(m,command+16):get(m,leader+0xf0);
                if(!target || get(m,target+20)!=get(m,leader+0xf4))return !m->fault;
                return s->target_order && s->target_order(s->context,o,priority(m,o),get(m,leader+0xf0));
            }
        }
    }
    command=get(m,o+0xdc);target=get(m,command+16);
    if(target && (get(m,target+16)&0x40u)){
        if(!native_entity_type_activity(m,target,&active))return 0;
        if(active){GameAddress followed=get(m,get(m,target+0xdc)+16);
            if(followed)return s->target_order && s->target_order(s->context,o,priority(m,o),followed);}
    }
    put(m,o+0xd4,get(m,0x5ac6ac));
    if(!s->acquire || !s->acquire(s->context,o,&target))return 0;
    if(target){if(!s->notice || !s->notice(s->context,o,2))return 0;
        return s->identity_order && s->identity_order(s->context,o,0,get(m,target+20));}
    root=get(m,0x58973c);
    if(game_memory_read_u16(m,root+8)<4 && gameplay_state_entry_matches(m,root)){
        target=get(m,root+0x3c4);
        if(!native_entity_relationship_query(m,target,o,&relation))return 0;
        if(relation.decision>=NATIVE_ENTITY_RELATION_ACCEPT_NULL_SECOND){
            if(!s->target_priority || !s->target_priority(s->context,o,root,target,&score))return 0;
            if(score>0){long double dx=(long double)real(m,o+24)-real(m,target+24),dz=(long double)real(m,o+32)-real(m,target+32);
                if(!(dx*dx+dz*dz>=real(m,0x574960)))return s->identity_order && s->identity_order(s->context,o,priority(m,o),get(m,root+0x3c8));}
        }
    }
    command=get(m,o+0xdc);target=get(m,o+0x108);
    if(target && game_memory_read_u8(m,target+12)!=0xfe){
        if(!get(m,command+16) && !(integer(get(m,command+24))>0 && integer(get(m,command+32))>0)){
            request.kind=1;request.priority=priority(m,o);request.target=leader;request.has_target=leader!=0;
            request.target_identity=leader?get(m,leader+20):0;request.has_position=1;vector(m,0x594470,request.position);
            return s->issue && s->issue(s->context,o,&request);}
        return scene_order_execute(m,o,get(m,get(m,o)+0xac),s);
    }
    follower=get(m,o+0x10c);visits=0;
    while(follower){if(++visits>65536 || !native_entity_type_activity(m,follower,&active))return 0;
        if(active)break;
        follower=get(m,follower+0x10c);}
    if(follower && (!s->target_order || !s->target_order(s->context,o,priority(m,o),get(m,get(m,follower+0xdc)+16))))return 0;
    command=get(m,o+0xdc);
    if(get(m,command+16) || (integer(get(m,command+24))>0 && integer(get(m,command+32))>0))return scene_order_execute(m,o,get(m,get(m,o)+0xac),s);
    leader=get(m,o+0x108);
    if(m->fault || !leader)return !m->fault;
    {GameAddress method=get(m,get(m,leader)+0x154);
        if(method==0x53c540)return !m->fault; /*Verified empty peer method. */
        /*4cacda notifies the actual formation owner. Imperial4be000 clears
         *its reservation through the same native program used by infantry. */
        return gameplay_state_peer_method(m,leader,o,method,NULL);
    }
}
static int reaction_issue(GameMemory *m,GameAddress o,GameAddress target,uint32_t kind,const SceneReactionServices *services,int positioned){
    const SceneOrderServices *s=&services->orders;
    PresentationCommand command={0};command.kind=kind;command.target=target;
    command.has_position=1;command.has_target=target!=0;
    if(target)command.target_identity=get(m,target+20);
    vector(m,positioned?o+0xbc:0x594470,command.position);
    return !m->fault && services->issue && services->issue(s->context,o,&command,!positioned);
}
static int reaction_interaction(GameMemory *m,GameAddress o,unsigned slot,const SceneOrderServices *s,int *found){
    GameAddress method=get(m,get(m,o)+slot);
    if(method!=(slot==0x144?0x4cb410u:0x4cb670u))return 0;
    return scene_order_interaction(m,o,method,s,found);
}
/*4ca320: an incoming hit changes target/order choices according to the current
 *command, relationships, distances, cooldown and the shared sampled RNG. */
int scene_order_reaction(GameMemory *m,GameAddress o,GameAddress attacker,int32_t damage,const SceneReactionServices *services){
    const SceneOrderServices *s;GameAddress node,following;uint32_t kind,maximum;
    int32_t previous,choice,first,second;int found;float from[3],to[3],current_distance,new_distance;
    if(!m || m->fault || !services)return 0;
    s=&services->orders;
    previous=integer(get(m,o+0x12c));put(m,o+0x12c,0);
    if(integer(get(m,attacker+0x58))<=0)return !m->fault;
    node=get(m,o+0xdc);kind=get(m,node+12);
    if(get(m,0x5f0160+(get(m,o+20)&63u)*24+(get(m,attacker+20)&63u)*4)){
        if(kind==5)put(m,node+12,6);
        return !m->fault;
    }
    put(m,o+0xc8,attacker);put(m,o+0xcc,get(m,attacker+20));
    switch(kind){
    case 0:case 1:case 8:
        if(!reaction_interaction(m,o,0x144,s,&found))return 0;
        if(found)return reaction_issue(m,o,get(m,o+0xc8),13,services,1);
        vector(m,o+24,from);vector(m,attacker+24,to);new_distance=planar(m,from,to);
        vector(m,o+0xe4,to);current_distance=planar(m,from,to);
        if(!random_choice(m,integer(get(m,o+0x114)),&first) ||
           !random_choice(m,integer(get(m,o+0x11c)),&second))return 0;
        if(second>=first && !(current_distance>new_distance))return !m->fault;
        if(!random_choice(m,real(m,0x574954),&choice))return 0;
        return reaction_issue(m,o,attacker,choice<integer(get(m,o+0x118))?5:6,services,0);
    case 3:case 4:case 9:case 12:return reaction_issue(m,o,attacker,5,services,0);
    case 5:case 6:
        if(!reaction_interaction(m,o,0x144,s,&found))return 0;
        if(found)return reaction_issue(m,o,get(m,o+0xc8),13,services,0);
        following=get(m,o+0xf0);
        if(following==attacker){
            if(previous>6 && previous<=16)return !m->fault;
            if(!random_choice(m,real(m,0x574958),&choice))return 0;
            if(choice<integer(get(m,o+0x114)))put(m,get(m,o+0xdc)+12,kind==5?6:5);
            return !m->fault;
        }
        if(!random_choice(m,real(m,0x574954),&choice))return 0;
        if(choice<integer(get(m,o+0x114)) && damage<=0)return !m->fault;
        vector(m,o+24,from);vector(m,attacker+24,to);new_distance=planar(m,from,to);
        current_distance=0x1.fffffep127f;
        if(following){vector(m,following+24,to);current_distance=planar(m,from,to);}
        if(!random_choice(m,real(m,0x574954),&choice))return 0;
        if(!(choice<integer(get(m,o+0x114)) && current_distance>new_distance)){
            if(!random_choice(m,integer(get(m,o+0x118)),&first) ||
               !random_choice(m,integer(get(m,o+0x11c)),&second))return 0;
            if(second>=first)return !m->fault;
        }
        if(integer(get(m,o+0x5c))<=integer(get(m,attacker+0x5c))){
            if(!services->maximum_health || !services->maximum_health(s->context,attacker,&maximum))return 0;
            if(integer(maximum)<=integer(get(m,attacker+0x58)))return reaction_issue(m,o,attacker,6,services,0);
        }
        return reaction_issue(m,o,attacker,5,services,0);
    case 13:
        if(!random_choice(m,real(m,0x574954),&choice))return 0;
        if(choice>=integer(get(m,o+0x114)))return !m->fault;
        if(!reaction_interaction(m,o,0x148,s,&found))return 0;
        if(found){
            if(!random_choice(m,real(m,0x574954),&choice))return 0;
            return reaction_issue(m,o,get(m,get(m,o+0xdc)+16),choice<integer(get(m,o+0x118))?5:6,services,0);
        }
        if(!reaction_interaction(m,o,0x144,s,&found))return 0;
        if(found){
            node=get(m,o+0xdc);for(kind=0;kind<3;kind++)put(m,node+24+kind*4,get(m,o+0xbc+kind*4));
            return !m->fault;
        }
        return reaction_issue(m,o,get(m,get(m,o+0xdc)+16),6,services,0);
    case 2:case 7:case 10:case 11:case 14:return !m->fault;
    default:return 0;
    }
}
int scene_order_execute(GameMemory *m,GameAddress o,GameAddress method,const SceneOrderServices *s){
    switch(method){
    case 0x4ca9b0:return group_target(m,o,s);
    case 0x4cad20:return group_travel(m,o,s);
    case 0x53abd0:return territory_order(m,o,s);
    case 0x451110:case 0x53bb50:return collision_detour(m,o,s);
    case 0x403cd0:case 0x404410:case 0x4046c0:return vehicle_combat_order(m,o,method,s);
    case 0x4061e0:return vehicle_landing_order(m,o,s);
    case 0x406130:return s->other && s->other(s->context,o,get(m,get(m,o)+0x1dc)) && group_target(m,o,s);
    case 0x4cb010:case 0x4cb2b0:{int result;return scene_order_interaction(m,o,method,s,&result);}
    case 0x451180:case 0x53afa0:return travel(m,o,0,s);
    case 0x451120:case 0x53a550:return travel(m,o,1,s);
    case 0x53b2f0:return get(m,o+0xd0)<=get(m,0x5ac6ac)?advance(m,o,s):scene_order_execute(m,o,get(m,get(m,o)+0xa8),s);
    case 0x53a050:case 0x53a060:case 0x53a070:case 0x53a080:case 0x53a090:
    case 0x53a0a0:case 0x53a0b0:case 0x53a0c0:case 0x53a0d0:case 0x53a0e0:
    case 0x53a0f0:case 0x53a100:case 0x53a110:case 0x53a120:return !m->fault; /*Verified empty retail methods. */
    default:return s->other && s->other(s->context,o,method);
    }
}
/*539de0: census, target generations, command selection. Stable IDs classify
 *immutable tables; ordinary functions consume actual command/scene owners. */
int scene_order_tick(GameMemory *m,GameAddress o,const SceneOrderServices *s){
    static const uint16_t slots[15]={0xa0,0x9c,0xa4,0xa8,0xb0,0xb8,0xbc,0xcc,0xc4,0xd0,0xd4,0xe0,0xd8,0xc8,0xdc};
    uint8_t type=game_memory_read_u8(m,o+12);GameAddress target;uint32_t identity,command;unsigned i;
    /*539fd8/539fec live inside .text. Preserve their actual type policy as
     *ordinary C; shipping data intentionally contains no instruction section. */
    if(type==0x21 || type==0x27 || type==0x40){uint32_t x=ftol(real(m,o+24)),z=ftol(real(m,o+32));
            uint8_t visible=game_memory_read_u8(m,0x6020f0+(uint32_t)shift(x,12)+(uint32_t)shift(z&0xfffff07fu,7));
            identity=get(m,o+20)&63u;
            if(visible&(1u<<(identity&31u))){if(!s->census || !s->census(s->context,get(m,0x58973c),type,0))return 0;}
            x=ftol(real(m,o+24));z=ftol(real(m,o+32));visible=game_memory_read_u8(m,0x6020f0+(uint32_t)shift(x,12)+(uint32_t)shift(z&0xfffff07fu,7));
            if(visible)for(i=0;i<4;i++)if(i!=(get(m,o+20)&63u) && (visible&(1u<<i)))
                if(!s->census || !s->census(s->context,get(m,0x5f0210+i*4),type,1))return 0;
    }else if(type==0x29){GameAddress root=get(m,0x58973c);put(m,root+0x1f8,get(m,root+0x1f8)+1u);}
    put(m,o+0x12c,get(m,o+0x12c)+1u);
    target=get(m,o+0xf0);if(target && (!get(m,target+20) || get(m,target+20)!=get(m,o+0xf4) || !get(m,target+0x58)))put(m,o+0xf0,0);
    target=get(m,o+0xc8);if(target && (!get(m,target+20) || get(m,target+20)!=get(m,o+0xcc) || !get(m,target+0x58)))put(m,o+0xc8,0);
    command=get(m,get(m,o+0xdc)+12);
    return !m->fault && (command>14 || scene_order_execute(m,o,get(m,get(m,o)+slots[command]),s));
}
