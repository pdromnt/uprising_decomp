#include "scene_contacts.h"
#include "gameplay_spatial.h"
#include "gameplay_state.h"
#include "entity_queries.h"
#include <math.h>
#include <string.h>

static uint32_t get(GameMemory *m,GameAddress p){return game_memory_read_u32(m,p);}
static void put(GameMemory *m,GameAddress p,uint32_t v){game_memory_write_u32(m,p,v);}
static float real(GameMemory *m,GameAddress p){uint32_t v=get(m,p);float f;memcpy(&f,&v,4);return f;}
static int32_t signed_word(uint32_t v){int32_t s;memcpy(&s,&v,4);return s;}
static uint32_t truncate_word(long double v){return (uint32_t)(uint64_t)(!isfinite(v) || v< -0x1p63L || v>=0x1p63L?INT64_MIN:(int64_t)v);}
/*534f8c/53502c classify these type bytes in the original code section. Keep
 *the actual predicates here; shipping never needs that executable image. */
static int excluded(GameMemory *m,GameAddress o,GameAddress candidate,int *out){
    uint32_t type=game_memory_read_u8(m,o+12),other=game_memory_read_u8(m,candidate+12);int active,second;
    *out=1;
    switch(type){
    case 4:case 14:case 0x26:if(other==4 || other==0x27 || other==0x26 || other==14)return !m->fault;break;
    case 10:if(get(m,candidate+20)==get(m,o+0xe0) || (other==10 && get(m,candidate+0xe0)==get(m,o+0xe0)))return !m->fault;break;
    case 0x23:case 0x88:case 0x89:
        if(!native_entity_type_activity(m,o,&active))return 0;
        if(active){if(!native_entity_type_activity(m,candidate,&second))return 0;if(second)return !m->fault;}break;
    case 0x41:
        if((game_memory_read_u8(m,o+0xc2)&2) && !((get(m,candidate+20)^get(m,o+20))&63))return !m->fault;
        if(other==4)return !m->fault;
        break;
    default:break;
    }
    switch(other){
    case 9:if(get(m,0x5f0160+(get(m,candidate+20)&63)*24+(get(m,o+20)&63)*4) && type!=0x41)return !m->fault;break;
    case 10:if(get(m,candidate+0xe0)==get(m,o+20))return !m->fault;break;
    case 12:if(get(m,candidate+0xd4)==o)return !m->fault;break;
    case 0xa0:if(type!=0x20)return !m->fault;break;
    case 0xa2:if(get(m,candidate+0xb8)==1)return !m->fault;break;
    default:break;
    }
    *out=0;return !m->fault;
}
/*543f90 shares collision allocation/filter semantics but never requests
 *rollback. Its transformed probe separately asks the command layer to evade. */
int scene_contacts_class27_gather(GameMemory *m,GameAddress o,const SceneContactQueryServices *s,int *rollback){
    GameplaySpatialGrid *grid;GameAddress heads[9],candidate,node;unsigned bucket,i,visits=0;int skip,hit;float point[3];
    if(!m || m->fault || !s || !rollback)return 0;
    *rollback=0;put(m,o+0x17c,0);
    if(get(m,o+16)&0x200000)return !m->fault;
    grid=gameplay_spatial_grid(0x5f0800);
    if(!grid || !gameplay_spatial_query_heads(grid,real(m,o+24),real(m,o+32),heads))return 0;
    for(bucket=0;bucket<9;bucket++)for(candidate=heads[bucket];candidate && !m->fault;candidate=get(m,candidate+4)){
        if(++visits>65536){m->fault=1;return 0;}
        if(candidate==o || candidate==get(m,o+0xf0) || (get(m,candidate+16)&0x200000))continue;
        if(!excluded(m,o,candidate,&skip))return 0;
        if(skip)continue;
        if(!s->intersect || !s->intersect(s->context,candidate,o,get(m,get(m,candidate)+0x24),point,&hit))return 0;
        if(hit){
            if(!s->allocate || !(node=s->allocate(s->context)))return 0;
            put(m,node+12,candidate);for(i=0;i<3;i++){uint32_t word;memcpy(&word,point+i,4);put(m,node+16+i*4,word);}
            put(m,node+4,get(m,o+0xb0));put(m,o+0xb0,node);
        }
        if(!get(m,o+0x17c) && !(get(m,candidate+16)&0x400) &&
           signed_word(get(m,o+0x180)+16u)<signed_word(get(m,0x5ac6ac))){
            long double x=(long double)real(m,o+0x184)-real(m,candidate+24),y=(long double)real(m,o+0x188)-real(m,candidate+28),z=(long double)real(m,o+0x18c)-real(m,candidate+32);
            long double radius=(long double)real(m,o+0x38)*real(m,0x576fa8)+real(m,candidate+0x38);
            if(!(radius*radius<(x*x+z*z)+y*y)){put(m,o+0x17c,1);put(m,o+0x180,get(m,0x5ac6ac));}
        }
    }
    return !m->fault;
}
int scene_contacts_gather(GameMemory *m,GameAddress o,const float previous[3],const SceneContactQueryServices *s,int *rollback){
    GameplaySpatialGrid *grid;GameAddress heads[9],candidate,node;unsigned bucket,i,visits=0;int skip,hit;float point[3];
    if(!m || m->fault || !s || !previous || !rollback)return 0;
    *rollback=0;
    if(get(m,o+16)&0x200000)return !m->fault;
    grid=gameplay_spatial_grid(0x5f0800);
    if(!grid || !gameplay_spatial_query_heads(grid,real(m,o+0x18),real(m,o+0x20),heads))return 0;
    for(bucket=0;bucket<9;bucket++)for(candidate=heads[bucket];candidate && !m->fault;candidate=get(m,candidate+4)){
        if(++visits>65536){m->fault=1;return 0;}
        if(candidate==o || (get(m,candidate+16)&0x200000))continue;
        if(!excluded(m,o,candidate,&skip))return 0;
        if(skip)continue;
        if(!s->intersect || !s->intersect(s->context,candidate,o,get(m,get(m,candidate)+0x24),point,&hit))return 0;
        if(!hit)continue;
        if(!s->allocate || !(node=s->allocate(s->context)))return 0;
        put(m,node+12,candidate);for(i=0;i<3;i++){uint32_t bits;memcpy(&bits,point+i,4);put(m,node+16+4*i,bits);}
        put(m,node+4,get(m,o+0xb0));put(m,o+0xb0,node);
        if(!(get(m,candidate+16)&0x400)){
            long double dx=(long double)real(m,candidate+0x18)-real(m,o+0x18),dy=(long double)real(m,candidate+0x1c)-real(m,o+0x1c),dz=(long double)real(m,candidate+0x20)-real(m,o+0x20);
            float px=(float)((long double)real(m,candidate+0x18)-previous[0]),py=(float)((long double)real(m,candidate+0x1c)-previous[1]);
            long double pz=(long double)real(m,candidate+0x20)-previous[2];
            long double now=(dx*dx+dz*dz)+dy*dy,old=(pz*pz+(long double)py*py)+(long double)px*px;
            if(old>now)*rollback=1;
        }
    }
    return !m->fault;
}
static int inside_axis(GameMemory *m,GameAddress o,GameAddress other,uint32_t position,uint32_t desired){
    long double center=real(m,other+position),margin=real(m,0x576924),goal=real(m,o+desired),low=center-margin;
    return center+margin>=goal && (low<=goal || isnan(low) || isnan(goal));
}
int scene_contact_response(GameMemory *m,GameAddress o,GameAddress other,const float point[3],const SceneContactResponseServices *s){
    uint32_t flags,command,kind,tick,health;GameAddress table,owner,binding,attacker;int active;int32_t amount;
    if(!m || m->fault || !s || !point)return 0;
    if(signed_word(get(m,o+0x58))<=0)return !m->fault;
    if(!native_entity_type_activity(m,other,&active))return 0;
    flags=get(m,other+16);kind=game_memory_read_u8(m,other+12);
    if(!active && !(flags&4) && kind!=0x41 && kind!=11 && kind!=10){
        if(get(m,o+16)&0x4000)return !m->fault;
        command=get(m,get(m,o+0xdc)+12);if(command==11)return !m->fault;
        tick=get(m,0x5ac6ac);
        if(signed_word(get(m,o+0x134))>signed_word(tick) && get(m,o+0x130)==1)return !m->fault;
        if(get(m,o+0xe0))return !m->fault;
        if(game_memory_read_u8(m,o+12)==0x40 && (command==5 || command==6)){
            uint32_t cursor=get(m,0x6308c0),choice;if(cursor==1024)cursor=0;if(cursor>1023){m->fault=1;return 0;}
            choice=truncate_word((long double)real(m,0x6308c8+4*cursor)*real(m,0x576920));put(m,0x6308c0,cursor+1);
            if(!choice)return !m->fault;
        }
        command=get(m,get(m,o+0xdc)+12);
        if((command==0 || command==1) && inside_axis(m,o,other,0x18,0xf8) && inside_axis(m,o,other,0x20,0x100) &&
           (get(m,o+0xfc)==0x47000000 || inside_axis(m,o,other,0x1c,0xfc))){
            if(!s->settle || !s->settle(s->context,o,get(m,get(m,o)+0x168)))return 0;
        }
        table=get(m,o);
        return !m->fault && s->avoid && s->avoid(s->context,o,other,get(m,table+0x160),point);
    }
    if(signed_word(get(m,o+0x58))<0)return !m->fault;
    if(!s->damage || !s->damage(s->context,o,other,&amount))return 0;
    health=get(m,o+0x58)-(uint32_t)amount;put(m,o+0x58,health);
    if(signed_word(health)<=0){
        put(m,o+0x58,0);table=get(m,o);
        if(!s->killed || !s->killed(s->context,o,other,get(m,table+16)) || !s->reset || !s->reset(s->context,o,get(m,table+0x20)))return 0;
        if(game_memory_read_u8(m,o+12)!=0x23 || (get(m,o+20)&63)!=game_memory_read_u16(m,get(m,0x5f0208)+8))return !m->fault;
        kind=game_memory_read_u8(m,other+12);
        if(kind==11 || (kind==0x41 && game_memory_read_u8(m,other+0xcc)==0x26))return s->notice && s->notice(s->context,o,get(m,table+0x58),kind==11?4:5);
        return !m->fault;
    }
    if(get(m,o+16)&0x100){
        owner=get(m,0x5f0210+(get(m,o+20)&63)*4);
        if(game_memory_read_u16(m,owner+10)==4){binding=get(m,o+0x294);if(binding && !gameplay_group_note_attack(m,get(m,owner+0x2e0),get(m,binding+4),other))return 0;}
    }
    table=get(m,o);
    if(!s->notice || !s->notice(s->context,o,get(m,table+0x58),0))return 0;
    if(get(m,other+16)&4){
        attacker=get(m,other+0xd4);
        if(get(m,attacker+20)==get(m,other+0xd8))return s->credit && s->credit(s->context,o,attacker,get(m,table+0x11c),amount);
    }
    return !m->fault;
}
