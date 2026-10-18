#include "level_run.h"
#include "level_load.h"
#include "level_factory.h"
#include "level_objects.h"
#include "level_player_resources.h"
#include "level_player_slots.h"
#include "level_effect_runtime.h"
#include "gameplay_geometry.h"
#include "gameplay_spatial.h"
#include "gameplay_catalog.h"
#include "gameplay_state.h"
#include "game_intrusive_list.h"
#include "recyclable_records.h"
#include "presentation_commands.h"
#include "presentation_instance.h"
#include "scene_orders.h"
#include "state_helper_plans.h"
#include "portable_crt.h"
#include "menu_sound.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

/*Distinct unrecovered executable programs keep their actual domain inputs.
 *They must be implemented before this game target links, never CPU fallback. */
extern int game_level_imperial_unit_activate(GameApplication *,GameAddress,uint32_t,const float[3]); /*class26 +1bc*/
extern int game_level_scene_order_issue(GameApplication *,GameAddress,const PresentationCommand *,int);

extern int game_level_vehicle_death_detach(GameApplication *,GameAddress,GameAddress,GameAddress);
extern int game_level_vehicle_reset(GameApplication *,GameAddress);
extern int game_level_vehicle_command(GameApplication *,GameAddress,GameAddress,uint32_t);

static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
static int32_t integer(uint32_t b){int32_t v;memcpy(&v,&b,4);return v;}
static float real(GameApplication *a,GameAddress p){uint32_t b=get(a,p);float v;memcpy(&v,&b,4);return v;}
static uint32_t bits(float f){uint32_t b;memcpy(&b,&f,4);return b;}
static uint32_t truncate_word(long double f){return (uint32_t)(uint64_t)(!isfinite(f)||f< -0x1p63L||f>=0x1p63L?INT64_MIN:(int64_t)f);}
static int random_scaled(GameApplication *a,float scale,uint32_t *out){uint32_t i=get(a,0x6308c0);
    if(i==1024)i=0;if(i>=1024)return 0;put(a,0x6308c0,i+1);
    *out=truncate_word((long double)real(a,0x6308c8+i*4)*scale);return !a->memory.fault;}
static int sequence(GameApplication *a,GameAddress root,uint32_t *id){uint32_t next;
    if(level_player_slots_owns_player(root)){if(!level_player_slots_next_owner_sequence(&a->memory,root,id,&next))return 0;}
    else{next=get(a,root+24);*id=(next<<6)|game_memory_read_u16(&a->memory,root+8);next++;}
    put(a,root+24,next);return !a->memory.fault;}
static int publish(GameApplication *a,GameAddress root,GameAddress object){unsigned head=state_helper_flagged_member_offset(get(a,object+16));
    if(level_player_slots_owns_player(root) && !level_player_slots_attach_scene_member(root,object,head))return 0;
    game_list_insert_after(&a->memory,object,root+head,0x48,0x4c);
    if(a->memory.fault){level_player_slots_forget_scene_member(object);return 0;}return 1;}
static int unused(GameApplication *a,GameAddress object){GameAddress head=get(a,object+0x50);
    if(!head || a->memory.fault)return 0;game_list_insert_after(&a->memory,object,head,4,8);return !a->memory.fault;}
static int type_price(GameApplication *a,GameAddress root,uint8_t kind,uint32_t *price){
    if(get(a,get(a,root)+0x24)!=0x479450)return 0;
    if(kind==11){*price=0;return !a->memory.fault;}
    return game_level_player_catalog_value(a,root,kind,8,0,price);
}
static int spend(GameApplication *a,GameAddress root,uint32_t cost){
    if(get(a,get(a,root)+0x30)!=0x479490)return 0;
    put(a,root+0x22c,get(a,root+0x22c)-cost);return !a->memory.fault;
}
static int blocked(GameApplication *a,GameAddress mask,int x,int z){
    return x<0 || x>=256 || z<0 || z>=256 || (game_memory_read_u8(&a->memory,mask+(unsigned)z*32+(unsigned)x/8)&(1u<<(x&7)));
}
/*4bc820: search real collision/terrain masks, widening after each five trials.
 *A successful fifth candidate still widens in the original. Failed searches
 *return the unused instance to its existing factory list, not a new allocator. */
int game_level_imperial_produce(GameApplication *a,GameAddress strategy,uint8_t kind,const float center[3],int require_route,float radius,GameAddress *out){
    GameAddress root=get(a,0x58973c),object,arrival;uint32_t sample,cost=0,variant,id;float p[3]={0};int tries,clear=0,accessible=0,route=1,paid;
    (void)strategy;*out=0;
    if(!random_scaled(a,real(a,0x5746c0),&sample))return 0;paid=sample<1u;
    if(paid){uint32_t allowed;
        if(!type_price(a,root,kind,&cost) || gameplay_catalog_unit_affordable(&a->memory,root,cost,&allowed)!=1)return 0;
        if(!allowed)return !a->memory.fault;
    }
    if(!game_level_player_catalog_value(a,root,kind,3,0,&variant) || variant>UINT16_MAX)return 0;
    object=game_level_factory_take(a,kind,(uint16_t)variant);if(!object)return 0;
    if(bits(radius)>0x80000000u)radius=(float)((long double)real(a,object+0x38)*real(a,0x57465c));
    if(integer(bits(center[0]))<=0 || integer(bits(center[0]))>=0x48000000 ||
       integer(bits(center[2]))<=0 || integer(bits(center[2]))>=0x48000000 || integer(bits(radius))>=0x45abe800)goto no_site;
    for(;;){
        tries=5;
        do{
            if(!(bits(radius)&0x7fffffffu))memcpy(p,center,12);
            else{float xz[2]={0};(void)gameplay_geometry_nearby_location(&a->memory,center[0],center[2],radius,xz);
                if(a->memory.fault)goto failure;p[0]=xz[0];p[2]=xz[1];}
            p[0]=(float)integer(truncate_word(p[0])&0xfffffe00u);p[2]=(float)integer(truncate_word(p[2])&0xfffffe00u);
            p[1]=(float)(gameplay_geometry_terrain_height(&a->memory,p[0],p[2])+real(a,object+0x30));
            if(kind==0x26 || kind==0x27){
                p[1]=(float)((long double)p[1]+real(a,0x59d8a8));accessible=route=1;
                if(!gameplay_geometry_position_clear(&a->memory,object,p,&clear))goto failure;
            }else{
                uint8_t x=(uint8_t)(integer(truncate_word(p[0]))>>9),z=(uint8_t)(integer(truncate_word(p[2]))>>9);
                if(!gameplay_geometry_position_clear(&a->memory,object,p,&clear))goto failure;
                accessible=!blocked(a,0x610520,x,z) &&
                    (!blocked(a,0x610520,(int)x-1,z) || !blocked(a,0x610520,(int)x+1,z) ||
                     !blocked(a,0x610520,x,(int)z-1) || !blocked(a,0x610520,x,(int)z+1));
                route=1;if(clear && accessible && require_route)route=scene_order_segment_clear(&a->memory,p,center);
            }
            if(a->memory.fault)goto failure;
            tries--;
        }while(tries>0 && (!clear || !accessible || !route));
        if(tries>=1)break;
        {long double wider=(long double)real(a,object+0x38)*real(a,0x57465c)+radius;
            radius=(float)wider;if(!(wider<real(a,0x5746d0)))goto no_site;}
    }
    if(kind==0x26){
        if(!sequence(a,root,&id) || !game_level_imperial_unit_activate(a,object,id,p) || !publish(a,root,object))goto failure;
    }else{
        arrival=game_level_factory_take(a,0x45,0);if(!arrival)goto failure;
        if(!sequence(a,root,&id) || !game_level_arrival_produce(a,arrival,id,p,object) || !publish(a,root,arrival)){
            (void)game_level_factory_destroy(a,arrival);goto failure;}
    }
    if(!spend(a,root,cost))return 0;*out=object;return !a->memory.fault;
no_site:return unused(a,object);
failure:(void)game_level_factory_destroy(a,object);return 0;
}
static int phase_advance(GameApplication *a,GameAddress p){uint32_t i=get(a,p+0x1c4);if(i>=14)return 0;
    game_memory_write_u8(&a->memory,p+0x1b6+i,0);game_memory_write_u8(&a->memory,p+0x1a8+i,game_memory_read_u8(&a->memory,p+0x1a8+i)+1);return !a->memory.fault;}
static int cancel(GameApplication *a,GameAddress p){uint32_t i=get(a,p+0x1c4);if(i>=14)return 0;
    put(a,p+0xda0+i*4,get(a,0x5ac6ac));game_memory_write_u8(&a->memory,p+0x1b6+i,0);
    game_memory_write_u8(&a->memory,p+0x1a8+i,0);put(a,p+0x1a4,get(a,p+0x1a4)&~(1u<<i));return !a->memory.fault;}
/*4b5820: budget preflight, real source-site position, two produced units,
 *and per-slot formation publication. No serialized duplicate work state. */
static int produce_pair(GameApplication *a,GameAddress p){
    GameAddress root=get(a,0x58973c),site,source;uint32_t cost,extra,index=get(a,p+0x1c4);float center[3];unsigned i;
    if(index>=14 || !type_price(a,root,0x27,&cost) || !game_level_player_catalog_unit_price(a,root,0,&extra))return 0;
    if(integer(get(a,root+0x22c))<integer(cost*2u+extra))return phase_advance(a,p);
    site=get(a,p+0x340+index*4);
    if(get(a,site+0x54) && !(get(a,p+0x1a4)&0x100))return cancel(a,p);
    source=get(a,get(a,p+0x234)+get(a,p+0x378+index*4)*4);
    source=source?get(a,source+0x1c):p+0xd2c;
    for(i=0;i<3;i++)center[i]=real(a,source+i*4);
    for(i=0;i<8;i++)put(a,p+0x7ec+(index+i*14)*4,0);
    if(!game_memory_read_u8(&a->memory,p+0x1b6+index))for(i=0;i<2;i++){
        GameAddress object;if(!game_level_imperial_produce(a,p,0x27,center,0,-1.0f,&object))return 0;
        put(a,p+0x7ec+(get(a,p+0x1c4)+i*14)*4,object);
    }
    return phase_advance(a,p);
}
static uint32_t work_get(GameApplication *a,GameAddress p,unsigned field,unsigned i){return get(a,get(a,p+field)+i*4);}
static void work_put(GameApplication *a,GameAddress p,unsigned field,unsigned i,uint32_t v){put(a,get(a,p+field)+i*4,v);}
static int pair_link(GameApplication *a,GameAddress object,unsigned slot,GameAddress argument){
    GameplayPeerServices s={a,NULL};return gameplay_state_peer_method(&a->memory,object,argument,get(a,get(a,object)+slot),&s);}
static int point_command(GameApplication *a,GameAddress object,unsigned priority,unsigned kind,const float p[3]){
    PresentationCommand command={0};command.priority=priority;command.kind=kind;command.has_position=1;memcpy(command.position,p,12);
    return game_level_scene_order_issue(a,object,&command,0);
}
static int follow_command(GameApplication *a,GameAddress object,GameAddress target){
    PresentationCommand command={0};unsigned i;command.priority=2;command.kind=3;command.target=target;
    command.has_target=command.has_position=1;command.target_identity=get(a,target+0x14);
    for(i=0;i<3;i++)command.position[i]=real(a,0x594470+i*4);
    return game_level_scene_order_issue(a,object,&command,1);
}
static int refund_old(GameApplication *a,GameAddress root,GameAddress object){
    uint32_t maximum,price,amount;float ratio;GameAddress method;
    if(!game_level_scene_property(a,object,get(a,get(a,object)+0x44),&maximum) ||
       !type_price(a,root,game_memory_read_u8(&a->memory,object+12),&price))return 0;
    ratio=(float)((long double)integer(get(a,object+0x58))/(long double)integer(maximum));
    amount=truncate_word((long double)integer(price)*ratio);method=get(a,get(a,root)+0x38);
    if(method!=0x4794b0 && method!=0x4794e0)return 0;
    put(a,root+0x22c,get(a,root+0x22c)+amount);put(a,root+0x150,get(a,root+0x150)+amount);
    method=get(a,get(a,object)+0x20);
    return game_level_scene_reset(a,object,method);
}
static int defenses(GameApplication *a,GameAddress p,unsigned site_index){
    GameAddress site=get(a,get(a,0x639a70)+site_index*4),root=get(a,0x58973c);uint32_t random,extent;
    long double width=(long double)real(a,site+12)-real(a,site+4),depth=(long double)real(a,site+16)-real(a,site+8);
    float center[3]={(float)(((long double)real(a,site+12)+real(a,site+4))*real(a,0x57464c)),0,
        (float)(((long double)real(a,site+8)+real(a,site+16))*real(a,0x57464c))};int iterations;unsigned i;
    extent=truncate_word(width<depth || isnan(width)||isnan(depth)?depth:width);
    {uint32_t x=truncate_word(center[0]),z=truncate_word(center[2]);
        if(game_memory_read_u8(&a->memory,0x6020f0+(GameAddress)(integer(z&0xfffff07f)>>7)+(GameAddress)(integer(x)>>12)))return !a->memory.fault;}
    if(!random_scaled(a,real(a,0x57465c),&random))return 0;
    if(integer(random)>=2)return !a->memory.fault;
    iterations=integer(extent)>>8;
    for(i=0;iterations>=0;iterations--,i++){
        float position[3]={0};GameAddress object;uint32_t x,z,variant,id;int clear;
        if(i>65536)return 0;
        if(!random_scaled(a,(float)integer(extent),&x) || !random_scaled(a,(float)integer(extent),&z))return 0;
        position[0]=(float)((long double)integer(x-(uint32_t)(integer(extent)>>1))+center[0]);
        position[2]=(float)((long double)integer(z-(uint32_t)(integer(extent)>>1))+center[2]);
        site=get(a,get(a,0x639a70)+site_index*4);
        if(real(a,site+4)>position[0] || !(real(a,site+12)>=position[0]) || real(a,site+8)>position[2] || !(real(a,site+16)>=position[2]))continue;
        position[1]=(float)gameplay_geometry_terrain_height(&a->memory,position[0],position[2]);
        if(!game_level_player_catalog_mount(a,root,9,0x20,5,&variant) || variant>UINT16_MAX)return 0;
        object=game_level_factory_take(a,9,(uint16_t)variant);if(!object)return 0;
        if(!gameplay_geometry_position_clear(&a->memory,object,position,&clear))return 0;
        if(!clear){if(!unused(a,object))return 0;continue;}
        {uint16_t angles[3]={0};GameAddress gun=get(a,p+0x13c);
            if(get(a,get(a,object)+0x78)!=0x434780 || !sequence(a,root,&id) ||
               !game_level_position_shot_place(a,object,id,position,angles,get(a,gun+0x14),10) || !publish(a,root,object)){
                (void)game_level_factory_destroy(a,object);return 0;}}
    }
    return !a->memory.fault;
}
/*4b59c0: install the produced pair, link its real formation, issue the
 *selected route, observe capture/timeout and publish defenses after capture. */
static int settle_pair(GameApplication *a,GameAddress p){
    uint32_t index=get(a,p+0x1c4),state;GameAddress root=get(a,0x58973c),site;unsigned site_index,head_index,i,visits;
    float center[3];GameAddress first,second,node;
    if(index>=14)return 0;site=get(a,p+0x340+index*4);site_index=get(a,site);head_index=site_index*8;
    if(site_index>=(unsigned)(int16_t)game_memory_read_u16(&a->memory,0x639a74))return 0;
    state=game_memory_read_u8(&a->memory,p+0x1b6+index);
    if(state>2)return phase_advance(a,p);
    if(!state){
        for(i=0;i<2;i++){
            GameAddress object=get(a,p+0x7ec+(index+i*14)*4);
            if(!object)return phase_advance(a,p);if(!get(a,object+0xdc))return !a->memory.fault;
        }
        for(i=0;i<2;i++){
            GameAddress old=work_get(a,p,0x2c8,head_index+i);
            if(old && !refund_old(a,root,old))return 0;
            work_put(a,p,0x2c8,head_index+i,get(a,p+0x7ec+(index+i*14)*4));put(a,p+0x7ec+(index+i*14)*4,0);
        }
        game_memory_write_u8(&a->memory,p+0x1b6+index,1);state=1;
    }
    if(state==1){
        center[0]=(float)(((long double)real(a,site+4)+real(a,site+12))*real(a,0x57464c));center[1]=32768.0f;
        center[2]=(float)(((long double)real(a,site+8)+real(a,site+16))*real(a,0x57464c));
        first=work_get(a,p,0x2c8,head_index);second=work_get(a,p,0x2c8,head_index+1);
        if(!first || !second || !pair_link(a,first,0x104,p) || !pair_link(a,second,0x104,first) ||
           !pair_link(a,first,0x108,second) || !pair_link(a,second,0x108,0))return 0;
        work_put(a,p,0x2c8,head_index+1,0);
        if(!point_command(a,first,2,14,center) || !point_command(a,first,0,0,center))return 0;
        node=get(a,get(a,p+0x430+index*4)+12);visits=0;
        if(node)game_memory_write_u8(&a->memory,node+12,0);
        while(node){unsigned cursor=game_memory_read_u8(&a->memory,node+12),length=game_memory_read_u8(&a->memory,node+13);
            if(++visits>65536)return 0;
            /*The retail boundary step reads outside its32-byte arrays before
             *advancing. Keep bounded native storage; no such candidate is
             *published. Cross-node route parity remains an explicit gap. */
            if(cursor>=32){node=get(a,node+4);if(node)game_memory_write_u8(&a->memory,node+12,0);continue;}
            if(cursor>=length)break;
            center[0]=(float)(game_memory_read_u8(&a->memory,get(a,node+16)+cursor)*1024u);
            center[2]=(float)(game_memory_read_u8(&a->memory,get(a,node+20)+cursor)*1024u);
            game_memory_write_u8(&a->memory,node+12,(uint8_t)(cursor+1));
            if(!point_command(a,first,2,0,center))return 0;
        }
        if(!point_command(a,second,0,14,center) || !point_command(a,second,0,0,center) || !follow_command(a,second,first))return 0;
        work_put(a,p,0x30c,head_index,1);game_memory_write_u8(&a->memory,p+0x1b6+index,2);return !a->memory.fault;
    }
    if(get(a,site+0x54)==root && get(a,get(a,site+0x18))){
        put(a,get(a,p+0x278),UINT32_MAX);put(a,get(a,p+0x27c),0);put(a,p+0x5f0,get(a,0x5ac6ac)+160);
        work_put(a,p,0x328,site_index,get(a,0x5ac6ac));work_put(a,p,0x30c,head_index,0);
        node=work_get(a,p,0x2c8,head_index);visits=0;
        while(node){if(++visits>65536 || !game_level_vehicle_command(a,node,get(a,get(a,node)+0x158),2))return 0;node=get(a,node+0x10c);}
        first=work_get(a,p,0x2c8,head_index);
        if(first){GameAddress descriptor=get(a,get(a,site+0x18));float point[3];for(i=0;i<3;i++)point[i]=real(a,p+0xd2c+i*4);
            if(!game_level_scene_point_order(a,first,0,3,get(a,descriptor+0x48),point))return 0;}
        if(get(a,p+0x13c)){put(a,p+0x32c,get(a,p+0x32c)|(1u<<(site_index&31)));if(!defenses(a,p,site_index))return 0;}
        return phase_advance(a,p);
    }
    if(work_get(a,p,0x30c,head_index) && integer(get(a,0x5ac6ac)-get(a,p+0xda0+index*4))<=5760)return !a->memory.fault;
    if(get(a,get(a,p+0x278))!=site_index)put(a,get(a,p+0x27c),0);
    put(a,get(a,p+0x27c),get(a,get(a,p+0x27c))+1);put(a,get(a,p+0x278),site_index);
    return cancel(a,p);
}
extern int game_level_imperial_census(GameApplication *,GameAddress,int);
extern int game_level_imperial_site_refresh(GameApplication *,GameAddress,unsigned,int);
static void site_center(GameApplication *a,GameAddress site,float point[3]){
    point[0]=(float)(((long double)real(a,site+12)+real(a,site+4))*real(a,0x57464c));
    point[1]=32768.0f;
    point[2]=(float)(((long double)real(a,site+8)+real(a,site+16))*real(a,0x57464c));
}
static int site_move(GameApplication *a,GameAddress unit,const float point[3],int target_stop){
    if(target_stop){PresentationCommand command={0};unsigned i;
        command.priority=2;command.kind=14;command.has_position=command.has_target=1;
        for(i=0;i<3;i++)command.position[i]=real(a,0x594470+i*4);
        if(!game_level_scene_order_issue(a,unit,&command,1))return 0;
    }else if(!point_command(a,unit,2,14,point))return 0;
    return point_command(a,unit,2,0,point);
}
/*4bb2f0: borrow an idle formation from the first eligible site, choosing
 *the nearest head in its eight slots. A busy live head remains the fallback,
 *matching the original candidate assignment before its reservation test. */
static int claim_site(GameApplication *a,GameAddress p){
    unsigned index=get(a,p+0x1c4),state,i;int count,last=-1,chosen=-1;
    GameAddress site,unit;float point[3],best;uint32_t bits=0x7f7fffff;
    if(index>=14)return 0;state=game_memory_read_u8(&a->memory,p+0x1b6+index);
    if(state>1)return !a->memory.fault;
    if(!state){
        if(!game_level_imperial_census(a,p,1))return 0;
        put(a,p+0xd38,UINT32_MAX);
        if(!get(a,p+0x290)){game_memory_write_u8(&a->memory,p+0x1b6+index,1);return !a->memory.fault;}
        count=(int16_t)game_memory_read_u16(&a->memory,0x639a74);
        for(i=0;i<(unsigned)(count>0?count:0);i++){
            int slot;site=work_get(a,p,0x270,i);
            if(!site || !work_get(a,p,0x310,i))continue;
            put(a,p+0x340+index*4,site);site_center(a,site,point);memcpy(&best,&bits,4);
            for(slot=(int)i*8+7;slot>=(int)i*8;slot--){
                long double x,z,distance;
                unit=work_get(a,p,0x2c8,(unsigned)slot);
                if(!unit || !get(a,unit+0xdc))continue;
                last=slot;if(work_get(a,p,0x30c,(unsigned)slot))continue;
                x=(long double)point[0]-real(a,unit+24);z=(long double)point[2]-real(a,unit+32);
                distance=z*z+x*x;
                if(distance<best){best=(float)distance;chosen=slot;put(a,p+0xd38,(uint32_t)slot);}
            }
            if(chosen<0){chosen=last;put(a,p+0xd38,(uint32_t)chosen);}
            if(chosen<0)return 0; /* Retail dereferences the invalid head here. */
            unit=work_get(a,p,0x2c8,(unsigned)chosen);
            if(!site_move(a,unit,point,1))return 0;
            work_put(a,p,0x30c,(unsigned)chosen,1);
            game_memory_write_u8(&a->memory,p+0x1b6+index,1);return !a->memory.fault;
        }
        put(a,p+0x340+index*4,0);return phase_advance(a,p);
    }
    site=get(a,p+0x340+index*4);if(!site)return 0;
    if(get(a,site+0x54)==get(a,0x58973c)){
        work_put(a,p,0x328,get(a,site),get(a,0x5ac6ac));
        if(get(a,p+0x13c))put(a,p+0x32c,get(a,p+0x32c)|(1u<<(get(a,site)&31)));
    }
    chosen=integer(get(a,p+0xd38));
    if(integer(get(a,0x5ac6ac)-get(a,p+0xda0+index*4))>3840 || chosen<0 ||
       !work_get(a,p,0x2c8,(unsigned)chosen) || get(a,site+0x54))return phase_advance(a,p);
    return !a->memory.fault;
}
/*4bb630: retain the first expired empty site as this slot's work item. */
static int select_rebuild_site(GameApplication *a,GameAddress p){
    unsigned index=get(a,p+0x1c4),i;int count=(int16_t)game_memory_read_u16(&a->memory,0x639a74);
    if(index>=14)return 0;
    for(i=0;i<(unsigned)(count>0?count:0);i++){
        GameAddress site=work_get(a,p,0x26c,i);
        if(site && !get(a,get(a,site+24)) &&
           integer(work_get(a,p,0x328,i)+get(a,0x60419c))<integer(get(a,0x5ac6ac))){
            put(a,p+0x340+index*4,site);return phase_advance(a,p);
        }
    }
    put(a,p+0x340+index*4,0);return cancel(a,p);
}
/*4bb700: move an existing formation, or produce a replacement at the nearest
 *occupied site, wait for activation, link it into the actual peer chain and
 *publish its commands. Work arrays borrow heads; factories own the units. */
static int rebuild_site(GameApplication *a,GameAddress p){
    unsigned index=get(a,p+0x1c4),state,site_index,base,i;GameAddress site,unit,member,source;float point[3];
    if(index>=14)return 0;site=get(a,p+0x340+index*4);if(!site)return 0;
    site_index=get(a,site);if(site_index>UINT32_MAX/8u-1u)return 0;base=site_index*8;
    site_center(a,site,point);state=game_memory_read_u8(&a->memory,p+0x1b6+index);
    if(state>2)return phase_advance(a,p);
    if(!state){
        if(!game_level_imperial_census(a,p,1))return 0;
        if(work_get(a,p,0x310,site_index)){
            unsigned selected=base+7,empty;unsigned visits=0;
            for(;;){unit=work_get(a,p,0x2c8,selected);if(unit && get(a,unit+0xdc))break;
                if(selected==base)return !a->memory.fault;--selected;}
            for(member=unit;member;member=get(a,member+0x10c)){
                float x=real(a,member+24),z=real(a,member+32);
                if(++visits>65536)return 0;
                if(x>=real(a,site+4) && x<=real(a,site+12) && z>=real(a,site+8) && z<=real(a,site+16)){
                    return site_move(a,member,point,0) && phase_advance(a,p);
                }
            }
            if(!site_move(a,unit,point,0))return 0;
            if(work_get(a,p,0x2c8,base))for(empty=base+7;empty>=base;empty--){
                if(!work_get(a,p,0x2c8,empty)){
                    work_put(a,p,0x30c,empty,work_get(a,p,0x30c,base));work_put(a,p,0x30c,base,0);
                    work_put(a,p,0x2c8,empty,work_get(a,p,0x2c8,base));work_put(a,p,0x2c8,base,0);break;
                }
                if(empty==base)break;
            }
            work_put(a,p,0x2c8,base,work_get(a,p,0x2c8,selected));work_put(a,p,0x2c8,selected,0);
            work_put(a,p,0x30c,base,work_get(a,p,0x30c,selected));work_put(a,p,0x30c,selected,0);
            return phase_advance(a,p);
        }else{
            int count=(int16_t)game_memory_read_u16(&a->memory,0x639a74),found=0;float best;uint32_t b=0x7f7fffff;
            memcpy(&best,&b,4);
            for(i=0;i<(unsigned)(count>0?count:0);i++){
                float candidate[3],distance;long double x,z;source=work_get(a,p,0x26c,i);
                if(!source || !get(a,get(a,source+24)))continue;
                site_center(a,source,candidate);x=fabsl((float)((long double)candidate[0]-point[0]));z=fabsl((float)((long double)candidate[2]-point[2]));
                distance=(float)(x>z?z*real(a,0x571f18)+x:x*real(a,0x571f18)+z);
                if(distance<best){best=distance;found=1;put(a,p+0x378+index*4,i);}
            }
            if(!found)return cancel(a,p);
            source=work_get(a,p,0x26c,get(a,p+0x378+index*4));site_center(a,source,point);
            if(!game_level_imperial_produce(a,p,0x27,point,0,-1.0f,&unit))return 0;
            put(a,p+0x7ec+index*4,unit);game_memory_write_u8(&a->memory,p+0x1b6+index,1);return !a->memory.fault;
        }
    }
    unit=get(a,p+0x7ec+index*4);
    if(state==1){
        if(!unit)return phase_advance(a,p);
        if(get(a,unit+0xdc))game_memory_write_u8(&a->memory,p+0x1b6+index,2);
        return !a->memory.fault;
    }
    member=work_get(a,p,0x2c8,base);
    if((member && !pair_link(a,member,0x104,unit)) || !pair_link(a,unit,0x108,member) || !pair_link(a,unit,0x104,p))return 0;
    work_put(a,p,0x2c8,base,unit);
    if(!site_move(a,unit,point,0))return 0;
    work_put(a,p,0x30c,base,1);return phase_advance(a,p);
}
/*4bbc40: settle the rebuilt descriptor and retry only while a formation lives. */
static int settle_rebuild_site(GameApplication *a,GameAddress p){
    unsigned index=get(a,p+0x1c4),site_index;GameAddress site;
    if(index>=14)return 0;site=get(a,p+0x340+index*4);if(!site)return 0;site_index=get(a,site);
    if(get(a,get(a,site+24))){
        work_put(a,p,0x280,site_index,3);work_put(a,p,0x284,site_index,10);work_put(a,p,0x288,site_index,2);
        work_put(a,p,0x328,site_index,get(a,0x5ac6ac));return phase_advance(a,p);
    }
    if(!game_level_imperial_census(a,p,1))return 0;
    return work_get(a,p,0x310,site_index)?!a->memory.fault:phase_advance(a,p);
}
extern int game_application_timed_message(GameApplication *,GameAddress,int32_t);
static void strike_deadline(GameApplication *a,GameAddress p){
    put(a,p+0xd98,get(a,0x604194)?get(a,0x5ac6ac)+get(a,0x6041a4):0x7fffffffu);
}
/*The three actual5909e0 templates contain only%c and%d. Keep formatting in
 *bounded native bytes rather than borrowing a writable retail scratch buffer. */
static int strike_warning(GameApplication *a,GameAddress label,GameAddress position){
    uint8_t text[256];char number[16];size_t input=0,length=0;int n,column,row;
    column=integer(74u-truncate_word((long double)real(a,position)*real(a,0x5746b8)*real(a,0x5746bc)));
    row=integer(truncate_word((long double)real(a,position+8)*real(a,0x5746b8)*real(a,0x5746bc))+1u);
    n=snprintf(number,sizeof(number),"%d",row);if(n<0 || n>=(int)sizeof(number))return 0;
    while(input<256){
        uint8_t ch=game_memory_read_u8(&a->memory,label+(uint32_t)input++);
        if(!ch){if(length+1>=sizeof(text))return 0;text[length++]='\n';return game_application_timed_message_bytes(a,text,length,5);}
        if(ch=='%'){
            ch=game_memory_read_u8(&a->memory,label+(uint32_t)input++);
            if(ch=='c')ch=(uint8_t)column;
            else if(ch=='d'){if(length+(size_t)n>=sizeof(text))return 0;memcpy(text+length,number,(size_t)n);length+=(size_t)n;continue;}
            else if(ch!='%')return 0;
        }
        if(length+1>=sizeof(text))return 0;text[length++]=ch;
    }
    return 0;
}
/*4dfdd0 and4b90d0 share the actual strike factory and faction owner. */
int game_level_satellite_launch(GameApplication *a,GameAddress root,float point[3]){
    GameAddress object=game_level_factory_take(a,11,0),listener;
    uint32_t id;uint16_t angles[3]={0};SoundPlaybackRequest sound={0};long double x,z;
    if(!object)return 0;
    point[1]=(float)(gameplay_geometry_terrain_height(&a->memory,point[0],point[2])+real(a,0x57468c));
    /*471a90 is the direct-call alias of536250; the real kind11 vtable
     *selects536250. Both launch callers place that same native payload. */
    if((get(a,get(a,object)+4)!=0x536250 && get(a,get(a,object)+4)!=0x471a90) || !sequence(a,root,&id) ||
       !game_level_object_place_pose(a,object,id,point,angles,0) || !publish(a,root,object)){
        int fault=a->memory.fault;a->memory.fault=0;(void)game_level_factory_destroy(a,object);if(fault)a->memory.fault=fault;return 0;
    }
    listener=get(a,0x58f890);x=(long double)point[0]-real(a,listener+12);z=(long double)point[2]-real(a,listener+20);
    sound.sound=7;sound.volume=1;sound.distance_squared=(float)(z*z+x*x);
    return !a->memory.fault && game_menu_sound_play(a,&sound);
}
/*4b90d0: prefer a visible hostile site beside its owner's player; otherwise
 *select among real hostile sites using the gameplay RNG, then warn locally. */
static int select_strike_site(GameApplication *a,GameAddress p){
    unsigned index=get(a,p+0x1c4),i;int count=(int16_t)game_memory_read_u16(&a->memory,0x639a74);
    GameAddress root=get(a,0x58973c),site,position,player;uint32_t cursor,choice;int remaining;
    if(index>=14)return 0;put(a,p+0x340+index*4,0);
    if(integer(get(a,root+0x214))<=0)return cancel(a,p);
    if(integer(get(a,p+0x294))<1)return phase_advance(a,p);
    for(i=0;i<(unsigned)(count>0?count:0);i++){
        uint32_t x,z;site=work_get(a,p,0x274,i);if(!site)continue;
        position=get(a,get(a,get(a,0x639a70)+i*4)+0x1c);
        x=truncate_word(real(a,position));z=truncate_word(real(a,position+8));
        if(!game_memory_read_u8(&a->memory,0x6020f0+(uint32_t)(integer(z&0xfffff07fu)>>7)+(uint32_t)(integer(x)>>12)))continue;
        player=get(a,get(a,site+0x54)+0x450);
        if(fabsl((long double)real(a,player+24)-real(a,position))<real(a,0x5746b4) &&
           fabsl((long double)real(a,player+32)-real(a,position+8))<real(a,0x5746b4)){
            put(a,p+0x2b0,0);put(a,p+0x340+index*4,site);return phase_advance(a,p);
        }
    }
    cursor=get(a,0x6308c0);if(cursor==1024)cursor=0;if(cursor>=1024)return 0;put(a,0x6308c0,cursor+1);
    remaining=integer(truncate_word((long double)integer(get(a,p+0x294))*real(a,0x6308c8+cursor*4))+1u);
    for(i=0;remaining>0 && i<(unsigned)(count>0?count:0);i++)if(work_get(a,p,0x274,i))--remaining;
    if(!i)return 0;site=work_get(a,p,0x274,i-1);if(!site)return 0;
    if(get(a,site+0x54)==get(a,0x5f0208)){
        choice=portable_crt_rand_step(&a->memory)*3u/32768u;
        if(!strike_warning(a,get(a,0x5909e0+choice*4),get(a,site+0x1c)))return 0;
    }
    put(a,p+0x2b0,get(a,0x5ac6ac)+160u);put(a,p+0x340+index*4,site);put(a,p+0xf58,0);
    return phase_advance(a,p);
}
/*4b93a0: select the next real site descriptor according to its recorded age,
 *or the first live human player when no site was selected, then emit KSAT. */
static int advance_strike(GameApplication *a,GameAddress p){
    unsigned index=get(a,p+0x1c4),i;GameAddress site,head,record,position,root=get(a,0x58973c);
    uint32_t tick=get(a,0x5ac6ac),mode,choice,variant=1;int count;float point[3];
    if(index>=14)return 0;if(integer(get(a,root+0x214))<=0)return cancel(a,p);
    site=get(a,p+0x340+index*4);
    if(!site){
        for(i=0;i<4;i++){
            GameAddress owner=get(a,0x5f0210+i*4),player=get(a,owner+0x3bc);
            if(!get(a,owner+20) || !player || integer(get(a,player+0x58))<=0)continue;
            if(owner==get(a,0x5f0208)){
                choice=portable_crt_rand_step(&a->memory)/32768u;
                if(!game_application_timed_message(a,get(a,0x5909ec+choice*4),2))return 0;
            }
            for(i=0;i<3;i++)point[i]=real(a,player+24+i*4);
            if(!game_level_satellite_launch(a,get(a,0x58973c),point))return 0;strike_deadline(a,p);return phase_advance(a,p);
        }
        return phase_advance(a,p);
    }
    i=get(a,site);site=work_get(a,p,0x274,i);
    if(!site){strike_deadline(a,p);return phase_advance(a,p);}
    if(integer(get(a,p+0x2b0))>integer(tick) || integer(get(a,p+0xd98))>integer(tick))return !a->memory.fault;
    mode=get(a,0x604178);if(mode==2 && !random_scaled(a,real(a,0x5746c0),&mode))return 0;
    if(mode>1)return 0; /*Other values use an uninitialized retail index. */
    {uint16_t b=game_memory_read_u16(&a->memory,site+0x24);int16_t n;memcpy(&n,&b,2);count=n;}
    head=get(a,site+24);record=get(a,head);
    if(!mode && record && integer(get(a,record+12)+160u)<integer(tick))variant=0;
    else{
        for(variant=1;(int)variant<count;variant++){
            record=get(a,head+variant*4);
            if(record && integer(get(a,record+12)+160u)<=integer(tick))break;
        }
        if(mode && (int)variant==count){record=get(a,head);
            if(record && integer(get(a,record+12)+160u)<integer(tick))variant=0;}
    }
    if((int)variant==count)return !a->memory.fault;
    if(!get(a,p+0x2b0) && get(a,site+0x54)==get(a,0x5f0208)){
        choice=portable_crt_rand_step(&a->memory)*5u/32768u;
        if(!game_application_timed_message(a,get(a,0x5909c8+choice*4),2))return 0;
    }
    position=get(a,get(a,get(a,0x639a70)+i*4)+0x1c)+variant*12;
    for(i=0;i<3;i++)point[i]=real(a,position+i*4);
    if(!game_level_satellite_launch(a,get(a,0x58973c),point))return 0;
    if(get(a,0x604130))put(a,p+0xd98,tick+64u);else strike_deadline(a,p);
    return !a->memory.fault;
}
int game_level_assault_search_begin(GameApplication *a,int32_t x,int32_t z,float radius,GameAddress output,uint32_t *query){
    if(!a || !query || a->memory.fault || x<0 || x>=128 || z<0 || z>=128)return 0;
    *query=recyclable_grid_flood_begin(&a->memory,0x5cf038,(uint8_t)x,(uint8_t)z,bits(radius),output,NULL);
    return !a->memory.fault;
}
/*4ba425..4ba504 with456460/456400: a private temporary path owns its copied
 *chain, while the grid continues owning all accepted candidates. Selection
 *budget is the first angular-pick result, not the number of sampled points. */
int game_level_assault_search_points(GameApplication *a,GameAddress route,unsigned count,float points[20][3],int32_t *budget){
    GameAddress temporary,candidate,tail;int32_t selected;unsigned i,x,z,length;int found,ok=0,initialized=0;
    if(!a || !points || !budget || count>20 || a->memory.fault)return 0;
    temporary=recyclable_record_allocate(0x24);if(!temporary)return 0;
    if(!recyclable_outer_initialize(&a->memory,temporary,NULL))goto done;initialized=1;
    found=recyclable_grid_pick_route(&a->memory,0x5cf038,route,768,&selected);*budget=found;
    if(a->memory.fault)goto done;
    if(found && !recyclable_outer_copy(&a->memory,temporary,get(a,0x5cf038+0x1203c+(uint32_t)selected*4),NULL))goto done;
    for(i=0;i<count;i++){
        found=recyclable_grid_pick_route(&a->memory,0x5cf038,temporary,768,&selected);if(a->memory.fault)goto done;
        candidate=found?get(a,0x5cf038+0x1203c+(uint32_t)selected*4):recyclable_grid_random_route(&a->memory,0x5cf038);
        if(!candidate || !recyclable_outer_copy(&a->memory,temporary,candidate,NULL))goto done;
        tail=get(a,temporary+0x10);length=game_memory_read_u8(&a->memory,tail+0x0d);if(!length || length>32)goto done;
        x=game_memory_read_u8(&a->memory,get(a,tail+0x10)+length-1);z=game_memory_read_u8(&a->memory,get(a,tail+0x14)+length-1);
        points[i][0]=(float)(x*1024u);points[i][1]=0;points[i][2]=(float)(z*1024u);
    }
    ok=!a->memory.fault;
done:{GameMemory cleanup=a->memory;cleanup.fault=0;
    if(initialized){if(!recyclable_record_destroy(&cleanup,temporary,1,NULL))ok=0;}
    else (void)recyclable_record_release(temporary);
    return ok;}
}
extern int game_level_scene_target_order(void *,GameAddress,uint32_t,GameAddress);
static float assault_distance(GameApplication *a,float x,float z,float tx,float tz){
    float ax=(float)((long double)x-tx),az=(float)((long double)z-tz);
    ax=fabsf(ax);az=fabsf(az);
    return (float)(az<ax?(long double)az*real(a,0x571f18)+ax:(long double)ax*real(a,0x571f18)+az);
}
/*4bf580: identity/health gates, weighted defender counts, and exclusion by
 *occupied descriptors and the established flagged-object list. */
static int assault_eligible(GameApplication *a,GameAddress p,unsigned owner,int *allowed){
    GameAddress root,target=get(a,p+0x508),site,node;unsigned i,j,visits=0;int count;
    long double strength;*allowed=0;if(owner>=4)return 0;root=get(a,0x5f0210+owner*4);
    if(!root)return !a->memory.fault;
    if(target && (get(a,target+20)!=get(a,p+0x50c) || integer(get(a,target+0x58))<=0 ||
                  !get(a,root+20) || !get(a,root+0x3bc)))return !a->memory.fault;
    strength=(long double)integer(get(a,root+0x500))*real(a,0x574708);
    strength+=(long double)integer(get(a,root+0x508))*real(a,0x57464c);
    strength+=(long double)integer(get(a,root+0x50c))*real(a,0x574708);
    strength+=(long double)integer(get(a,root+0x504))*real(a,0x57464c);
    strength+=integer(get(a,root+0x514));strength+=integer(get(a,root+0x510));
    if(strength>real(a,0x574654))return !a->memory.fault;
    count=(int16_t)game_memory_read_u16(&a->memory,0x639a74);
    for(i=0;i<(unsigned)(count>0?count:0);i++){
        int length;site=get(a,get(a,0x639a70)+i*4);if(!get(a,site+0x54))continue;
        length=(int16_t)game_memory_read_u16(&a->memory,site+0x24);
        for(j=0;j<(unsigned)(length>0?length:0);j++){
            GameAddress point=get(a,site+0x1c)+j*12;
            if(!get(a,get(a,site+24)+j*4))continue;
            if(!target)return 0;
            if(assault_distance(a,real(a,target+24),real(a,target+32),real(a,point),real(a,point+8))<real(a,0x57470c))return !a->memory.fault;
        }
    }
    for(node=get(a,get(a,0x5f0224)+0x12c);node;node=get(a,node+0x48)){
        if(++visits>65536)return 0;
        if(!(get(a,node+16)&128u) || game_memory_read_u8(&a->memory,node+12)==100)continue;
        if(!target)return 0;
        if(assault_distance(a,real(a,target+24),real(a,target+32),real(a,node+24),real(a,node+32))<real(a,0x57470c))return !a->memory.fault;
    }
    *allowed=1;return !a->memory.fault;
}
static void assault_step(GameApplication *a,GameAddress p,unsigned index,unsigned step){game_memory_write_u8(&a->memory,p+0x1b6+index,(uint8_t)step);}
static void assault_clear_search(GameApplication *a,GameAddress p,unsigned index){
    if(get(a,p+0x4a0+index*4)==get(a,0x5d083c))put(a,0x5d083c,0);
    put(a,p+0x4a0+index*4,0);
}
static void assault_snapshot(GameApplication *a,GameAddress p,unsigned owner,GameAddress target){
    put(a,p+0x4d8+owner*12,get(a,target+24));put(a,p+0x4e0+owner*12,get(a,target+32));
    put(a,p+0x508,target);put(a,p+0x50c,get(a,target+20));
}
static int assault_select(GameApplication *a,GameAddress p,unsigned index){
    uint32_t forced=get(a,p+0xf5c),chosen=UINT32_MAX,random;unsigned i,visits=0;int allowed;
    GameAddress root,target=0;
    put(a,p+0x508,0);if(integer(get(a,p+0xe48))>integer(get(a,0x5ac6ac)))return !a->memory.fault;
    put(a,p+0x2c0,UINT32_MAX);
    if(forced==UINT32_MAX){
        for(i=0;i<4;i++){
            root=get(a,0x5f0210+i*4);if(!root)continue;target=get(a,root+0x3bc);
            if(!get(a,root+20) || !target || integer(get(a,target+0x58))<=0)continue;
            put(a,p+0x508,target);put(a,p+0x50c,get(a,target+20));
            if(!assault_eligible(a,p,i,&allowed))return 0;if(!allowed)continue;
            if(chosen!=UINT32_MAX){if(!random_scaled(a,real(a,0x5746c0),&random))return 0;if(!random)continue;}
            chosen=i;put(a,p+0x2c0,i);
        }
        if(chosen==UINT32_MAX){put(a,p+0xe48,get(a,0x5ac6ac)+160u);return !a->memory.fault;}
        root=get(a,0x5f0210+chosen*4);target=get(a,root+0x3bc);
    }else{
        if(forced>=4)return 0;chosen=forced;put(a,p+0x2c0,chosen);root=get(a,0x5f0210+chosen*4);if(!root)return 0;
        if(!random_scaled(a,(float)integer(get(a,root+0x1f8)),&random))return 0;random++;
        if(!random)return cancel(a,p);
        target=get(a,root+0xc8);
        while(target && integer(random)>0){
            if(++visits>65536)return 0;
            if(game_memory_read_u8(&a->memory,target+12)==0x29)--random;
            if(integer(random)>0)target=get(a,target+0x48);
        }
        if(!target)return 0;
    }
    if(!target)return 0;assault_snapshot(a,p,chosen,target);assault_step(a,p,index,1);put(a,p+0xe48,0);
    return !a->memory.fault;
}
static int assault_moved(GameApplication *a,GameAddress p,unsigned owner,GameAddress target){
    return assault_distance(a,real(a,p+0x4d8+owner*12),real(a,p+0x4e0+owner*12),real(a,target+24),real(a,target+32))>real(a,0x5746c4);
}
static int assault_prepare(GameApplication *a,GameAddress p,unsigned index,unsigned owner){
    int allowed;uint32_t query=get(a,p+0x4a0+index*4),count=get(a,0x5e106c);float points[20][3];int32_t budget;unsigned i,j;
    static const unsigned counter[3]={0x580,0x548,0x510},rates[3]={0x60417c,0x6041b4,0x604184},enabled[3]={0x204,0x208,0x20c};
    if(integer(count)<0 || count>20){
        if(integer(get(a,p+0xf44+owner*4)+9600u)<integer(get(a,0x5ac6ac))){assault_step(a,p,index,1);return !a->memory.fault;}
        return cancel(a,p);
    }
    if(!assault_eligible(a,p,owner,&allowed))return 0;
    if(!allowed){assault_step(a,p,index,1);put(a,p+0xe48,get(a,0x5ac6ac)+32u);return !a->memory.fault;}
    if(!game_level_assault_search_points(a,get(a,p+0x430+index*4),count,points,&budget))return 0;
    (void)query;put(a,p+0xf3c,count);
    for(i=0;i<count;i++)for(j=0;j<3;j+=2)put(a,p+0xe4c+i*12+j*4,bits(points[i][j]));
    for(i=0;i<3;i++)put(a,p+counter[i]+index*4,get(a,get(a,0x58973c)+enabled[i])?
        truncate_word(((long double)budget+real(a,0x574654))*real(a,rates[i])):0);
    put(a,p+0xf40,0);put(a,p+0xe48,get(a,0x5ac6ac));assault_step(a,p,index,4);return !a->memory.fault;
}
static int assault_produce(GameApplication *a,GameAddress p,unsigned index,unsigned owner){
    static const unsigned counter[3]={0x580,0x548,0x510},rates[3]={0x60417c,0x6041b4,0x604184},heads[3]={0x2cc,0x2dc,0x2ec};
    static const uint8_t kinds[3]={0x40,0x21,0x27};unsigned family,attempt,i;GameAddress target=get(a,p+0x508),object,previous;float point[3];
    put(a,p+0xf60,0);
    if(!target || get(a,target+20)!=get(a,p+0x50c) || integer(get(a,target+0x58))<=0){put(a,p+0xf60,1);assault_step(a,p,index,5);return !a->memory.fault;}
    if(integer(get(a,p+0xe48))>integer(get(a,0x5ac6ac)))return !a->memory.fault;
    put(a,p+0xe48,get(a,0x5ac6ac)+16u);
    for(family=0;family<3;family++){
        if(integer(get(a,rates[family]))<=0)continue;
        for(attempt=0;;){
            unsigned cursor=get(a,p+0xf40);if(integer(get(a,p+counter[family]+index*4))<=0)break;
            if(cursor>=20)return 0;
            for(i=0;i<3;i++)point[i]=real(a,p+0xe4c+cursor*12+i*4);
            if(!game_level_imperial_produce(a,p,kinds[family],point,1,-1.0f,&object))return 0;
            if(object){
                previous=get(a,p+heads[family]+owner*4);
                if((previous && !pair_link(a,previous,0x104,object)) || !pair_link(a,object,0x104,p) || !pair_link(a,object,0x108,previous))return 0;
                put(a,p+heads[family]+owner*4,object);
                if(!game_level_scene_target_order(a,object,2,target))return 0;
                put(a,p+counter[family]+index*4,get(a,p+counter[family]+index*4)-1u);
                ++cursor;put(a,p+0xf40,cursor==get(a,p+0xf3c)?0:cursor);
            }
            if(++attempt>65536)return 0;
            if(!((long double)(int32_t)attempt<real(a,rates[family])))break;
        }
    }
    if(!get(a,p+0x580+index*4) && !get(a,p+0x548+index*4) && !get(a,p+0x510+index*4))assault_step(a,p,index,5);
    return !a->memory.fault;
}
static int assault_retire(GameApplication *a,GameAddress p,unsigned index,unsigned owner){
    unsigned family,visits=0;GameAddress target=get(a,p+0x508),unit,next;int any=0;
    if(!target || !get(a,target+0x58) || get(a,target+20)!=get(a,p+0x50c))put(a,p+0xf60,1);
    for(family=0;family<4;family++)for(unit=get(a,p+0x2cc+(owner+family*4)*4);unit;unit=get(a,unit+0x10c)){
        uint32_t kind=get(a,get(a,unit+0xdc)+12),x,z;unsigned visibility;
        if(++visits>65536)return 0;if(kind>=5 && kind<=7)return !a->memory.fault;
        x=truncate_word(real(a,unit+24));z=truncate_word(real(a,unit+32));
        visibility=game_memory_read_u8(&a->memory,0x6020f0+(uint32_t)(integer(z&0xfffff07fu)>>7)+(uint32_t)(integer(x)>>12));
        if(visibility&(1u<<owner))return !a->memory.fault;
    }
    put(a,p+0xf44+owner*4,get(a,0x5ac6ac));visits=0;
    for(family=0;family<4;family++)for(unit=get(a,p+0x2cc+(owner+family*4)*4);unit;unit=next){
        if(++visits>65536)return 0;any=1;next=get(a,unit+0x10c);if(!refund_old(a,get(a,0x58973c),unit))return 0;
    }
    if(any && integer(get(a,p+0xe48))>0 && integer(get(a,p+0xe48)+1920u)>integer(get(a,0x5ac6ac)) &&
       target && game_memory_read_u8(&a->memory,target+12)==0x20 && !get(a,p+0xf60)){
        assault_step(a,p,index,1);return !a->memory.fault;
    }
    return phase_advance(a,p);
}
/*4b9880: six-phase ordinary control flow around the real target, owned
 *weighted search, formation factories, orders and refund retirement. */
static int assault(GameApplication *a,GameAddress p){
    unsigned index=get(a,p+0x1c4),state,owner;GameAddress target;uint32_t query;int allowed;
    if(index>=14)return 0;state=game_memory_read_u8(&a->memory,p+0x1b6+index);
    if(state>5)return phase_advance(a,p);if(!state)return assault_select(a,p,index);
    owner=get(a,p+0x2c0);if(owner>=4)return 0;target=get(a,p+0x508);
    if(state==4)return assault_produce(a,p,index,owner);
    if(state==5)return assault_retire(a,p,index,owner);
    if(state==3)return assault_prepare(a,p,index,owner);
    if(!target || get(a,target+20)!=get(a,p+0x50c) || (state==2 && integer(get(a,target+0x58))<0)){
        assault_clear_search(a,p,index);assault_step(a,p,index,0);return !a->memory.fault;
    }
    if(state==1){
        put(a,p+0xf60,0);if(integer(get(a,p+0xe48))>integer(get(a,0x5ac6ac)))return !a->memory.fault;
        if(!assault_eligible(a,p,owner,&allowed))return 0;
        if(!allowed){put(a,p+0xe48,get(a,0x5ac6ac)+32u);return !a->memory.fault;}
        if(game_memory_read_u8(&a->memory,target+12)==0x20){
            GameAddress root=get(a,0x5f0210+owner*4),player=get(a,root+0x3bc);
            if(!get(a,root+20) || !player || integer(get(a,player+0x58))<0){put(a,p+0x4a0+index*4,0);assault_step(a,p,index,0);return !a->memory.fault;}
        }
    }
    if(assault_moved(a,p,owner,target)){
        assault_snapshot(a,p,owner,target);assault_clear_search(a,p,index);assault_step(a,p,index,1);
        if(state==2)put(a,p+0xe48,0);return !a->memory.fault;
    }
    if(state==1){
        if(get(a,0x5cf038+0x1804)){put(a,p+0xe48,get(a,0x5ac6ac)+32u);return !a->memory.fault;}
        if(!game_level_assault_search_begin(a,integer(truncate_word(real(a,p+0x4d8+owner*12)))>>10,
            integer(truncate_word(real(a,p+0x4e0+owner*12)))>>10,10.0f,get(a,p+0x430+index*4),&query))return 0;
        put(a,p+0x4a0+index*4,query);assault_step(a,p,index,2);
    }else{
        query=get(a,p+0x4a0+index*4);
        if(query)query=recyclable_grid_route_continue(&a->memory,0x5cf038,get(a,p+0x430+index*4),NULL);
        if(a->memory.fault)return 0;
        put(a,p+0x4a0+index*4,query);
    }
    if(query)return !a->memory.fault;assault_step(a,p,index,3);return assault_prepare(a,p,index,owner);
}
extern int game_level_group_descriptor_payload(GameApplication *,GameAddress,GameAddress *);
extern int game_level_group_repair_quote(GameApplication *,GameAddress,GameAddress,uint32_t *,uint32_t *,uint32_t *);
extern int game_level_group_repair_apply(GameApplication *,GameAddress,uint32_t);
/*4bf7e0: keep the original unsigned smoothing for an existing sample, and
 *signed half-tick initialization for an unsampled site. */
static int target_history(GameApplication *a,GameAddress p,unsigned index){
    GameAddress site=get(a,p+0x340+index*4);unsigned id;uint32_t old,tick=get(a,0x5ac6ac),next;
    if(!site)return !a->memory.fault;id=get(a,site);old=work_get(a,p,0xe40,id);
    next=integer(old)<0?(tick>>1)|(tick&0x80000000u):(tick-work_get(a,p,0xe44,id)+old)>>1;
    work_put(a,p,0xe40,id,next);work_put(a,p,0xe44,id,tick);return !a->memory.fault;
}
static int target_descriptor(GameApplication *a,GameAddress p,unsigned index,GameAddress site,unsigned slot){
    GameAddress descriptor,payload;int length=(int16_t)game_memory_read_u16(&a->memory,site+0x24);
    if(slot>=(unsigned)(length>0?length:0))return 0;descriptor=get(a,get(a,site+24)+slot*4);
    if(!descriptor || !game_level_group_descriptor_payload(a,descriptor,&payload) || !payload)return 0;
    put(a,p+0x3c0+index*4,payload);put(a,p+0x3f8+index*4,get(a,payload+20));
    return target_history(a,p,index) && phase_advance(a,p);
}
/*4b6e00: choose a hostile site other than slot5's current target, then choose
 *among its populated descriptor slots using the distinct original RNG draw. */
static int choose_hostile_descriptor(GameApplication *a,GameAddress p){
    unsigned index=get(a,p+0x1c4),i,slot;uint32_t choice;int count,remaining,length;GameAddress site=0;
    if(index>=14)return 0;
    if(integer(get(a,p+0x294))<2){put(a,p+0x340+index*4,0);put(a,p+0x3c0+index*4,0);return cancel(a,p);}
    if(!random_scaled(a,(float)integer(get(a,p+0x294)-1u),&choice))return 0;remaining=integer(choice+1u);
    count=(int16_t)game_memory_read_u16(&a->memory,0x639a74);
    for(i=0;remaining && i<(unsigned)(count>0?count:0);i++){
        site=work_get(a,p,0x274,i);
        if(site && site!=get(a,p+0x354))--remaining;
    }
    if(remaining || !i){put(a,p+0x340+index*4,0);return phase_advance(a,p);}
    site=work_get(a,p,0x274,i-1);put(a,p+0x340+index*4,site);
    if(!random_scaled(a,(float)(int16_t)game_memory_read_u16(&a->memory,site+0x26),&choice))return 0;
    remaining=integer(choice);length=(int16_t)game_memory_read_u16(&a->memory,site+0x24);
    for(slot=0;remaining>=0 && slot<(unsigned)(length>0?length:0);slot++)if(get(a,get(a,site+24)+slot*4))--remaining;
    if(!slot)return 0;return target_descriptor(a,p,index,site,slot-1);
}
/*4b7010: borrow the first hostile site lacking a primary descriptor, then
 *its first available secondary. Retail's missing-secondary read is rejected. */
static int choose_secondary_descriptor(GameApplication *a,GameAddress p){
    unsigned index=get(a,p+0x1c4),i,slot;int count,length;GameAddress site;
    if(index>=14)return 0;count=(int16_t)game_memory_read_u16(&a->memory,0x639a74);
    for(i=0;i<(unsigned)(count>0?count:0);i++){
        site=work_get(a,p,0x274,i);if(!site || get(a,get(a,site+24)))continue;
        put(a,p+0x340+index*4,site);length=(int16_t)game_memory_read_u16(&a->memory,site+0x24);
        for(slot=1;slot<(unsigned)(length>0?length:0);slot++)if(get(a,get(a,site+24)+slot*4))return target_descriptor(a,p,index,site,slot);
        return 0;
    }
    return !a->memory.fault;
}
static uint32_t repair_round(GameApplication *a,long double value){
    float stored=(float)value;
    return truncate_word((long double)stored+(value>real(a,0x57466c)?real(a,0x57464c):-real(a,0x57464c)));
}
/*4bb0f0: the AI shares the UI's descriptor quote/apply and actual wallet.
 *Preserve binary32 affordability/fee stores, signed wrapping and capacity
 *consumption before applying the capped repair. No alternate health owner. */
static int repair_descriptors(GameApplication *a,GameAddress p){
    unsigned i,index=get(a,p+0x1c4);int count;GameAddress root=get(a,0x58973c),site,descriptor;
    if(index>=14)return 0;count=(int16_t)game_memory_read_u16(&a->memory,0x639a74);
    for(i=0;i<(unsigned)(count>0?count:0);i++){
        int32_t slot=integer(work_get(a,p,0x320,i));uint32_t price,health,maximum,need,cap,amount,fee;
        long double ratio;float rate;int length;
        if(slot<0)continue;site=work_get(a,p,0x26c,i);if(!site)return 0;
        length=(int16_t)game_memory_read_u16(&a->memory,site+0x24);if(slot>=length)return 0;
        descriptor=get(a,get(a,site+24)+(uint32_t)slot*4);
        if(!descriptor || integer(work_get(a,p,0x31c,i))<=0 || integer(work_get(a,p,0x288,i))<=0)continue;
        if(!game_level_group_repair_quote(a,descriptor,root,&price,&health,&maximum))return 0;
        ratio=(long double)(float)integer(health+get(a,descriptor+24))/integer(maximum);
        if(!(isnan(ratio) || ratio<real(a,0x57464c)))continue;
        work_put(a,p,0x288,i,work_get(a,p,0x288,i)-1u);
        if(!game_level_group_repair_quote(a,descriptor,root,&price,&health,&maximum))return 0;
        need=maximum-health-get(a,descriptor+24);
        cap=truncate_word((long double)integer(get(a,root+0x2d0))*real(a,0x5746c8)*integer(need));
        if(!game_level_group_repair_quote(a,descriptor,root,&price,&health,&maximum))return 0;
        rate=(float)(((long double)(float)integer(price)/integer(maximum))*real(a,0x574680));
        amount=repair_round(a,(long double)integer(get(a,root+0x22c))/rate);
        if(integer(amount)>=integer(cap))amount=cap;
        fee=repair_round(a,(long double)integer(amount)*rate);
        if(!game_level_group_repair_apply(a,descriptor,amount) || !spend(a,root,fee))return 0;
    }
    return phase_advance(a,p);
}
extern uint32_t game_level_imperial_missing_class(GameApplication *,GameAddress);
extern int game_level_citadel_upgrade(GameApplication *,GameAddress,uint8_t,uint16_t *);
extern int game_level_group_remove_member(GameApplication *,GameAddress,unsigned);
static int site_count(GameApplication *a){return (int16_t)game_memory_read_u16(&a->memory,0x639a74);}
static GameAddress economy_site(GameApplication *a,unsigned i){return get(a,get(a,0x639a70)+i*4);}
static int16_t short_at(GameApplication *a,GameAddress p){return (int16_t)game_memory_read_u16(&a->memory,p);}
/*4bd310: recent threats suppress production near living human players. */
static int economy_safe(GameApplication *a,GameAddress p,unsigned index){
    uint32_t stamp=work_get(a,p,0x31c,index);float minimum=20000.0f;unsigned i;
    if(integer(stamp)<=0 || integer(get(a,0x5ac6ac)-stamp)>=640)return 1;
    for(i=0;i<4;i++){
        GameAddress owner=get(a,0x5f0210+i*4),player,point;float distance;
        if(!owner || !get(a,owner+20))continue;player=get(a,owner+0x3bc);
        if(!player || integer(get(a,player+88))<=0)continue;
        point=get(a,economy_site(a,index)+28);
        distance=assault_distance(a,real(a,player+24),real(a,player+32),real(a,point),real(a,point+8));
        if(isnan(distance) || distance<=minimum)minimum=distance;
        if(integer(bits(minimum))<0x466a6000)return 0;
    }
    return 1;
}
/*4bd410: affordability preflight, real bucket refresh, then the actual group
 *upgrade transaction. Kind10 needs an active site; others prefer inactive sites
 *and otherwise the last active candidate. A rejected upgrade returns -1. */
static int economy_produce(GameApplication *a,GameAddress p,uint32_t kind,uint32_t *result){
    GameAddress root=get(a,0x58973c),chosen=0;uint32_t price,allowed;int i;
    *result=UINT32_MAX;if(kind>UINT8_MAX)return 0;
    if(!game_level_player_catalog_unit_price(a,root,(int16_t)kind,&price) ||
       gameplay_catalog_unit_affordable(&a->memory,root,price,&allowed)!=1)return 0;
    if(!allowed)return !a->memory.fault;
    (void)gameplay_state_refresh_entity_buckets(&a->memory,p+0x1c8,0,0);
    if(a->memory.fault)return 0;
    for(i=0;i<site_count(a);i++){
        GameAddress site=economy_site(a,(unsigned)i);
        if(get(a,site+84)!=root || short_at(a,site+36)<=short_at(a,site+38) || !economy_safe(a,p,(unsigned)i))continue;
        if(kind==10){if(integer(get(a,site+20))>0){chosen=site;break;}}
        else{chosen=site;if(!get(a,site+20))break;}
    }
    if(chosen){uint16_t status;if(!game_level_citadel_upgrade(a,chosen,(uint8_t)kind,&status))return 0;
        if(!status)*result=kind;}
    return !a->memory.fault;
}
/*4bae70: unmet prerequisites return the prerequisite, not the desired class. */
static uint32_t economy_required(GameApplication *a,GameAddress p){
    uint32_t scenario=get(a,p+0xd94),desired,prerequisite;GameAddress root=get(a,0x58973c);int i,eligible=0;
    if(scenario>=6)return 0;desired=get(a,0x590a20+scenario*4);prerequisite=get(a,0x590a08+scenario*4);
    if(desired==5){
        for(i=0;i<site_count(a);i++){GameAddress site=economy_site(a,(unsigned)i);
            if(get(a,site+84)==get(a,0x5f0220) && get(a,get(a,site+24)) && get(a,site+20) &&
               short_at(a,site+36)>short_at(a,site+38)){eligible=1;break;}}
        if(!eligible)return 0;
    }
    if(integer(get(a,root+0x200+prerequisite*4))<integer(get(a,0x590a38+scenario*4)))return prerequisite;
    return integer(get(a,root+0x200+desired*4))<1?desired:0;
}
/*4baf60 and the inlined replacement choice in4ba6d0. */
static uint32_t economy_replacement(GameApplication *a,GameAddress p){
    uint32_t scenario=get(a,p+0xd94);GameAddress root=get(a,0x58973c);int i;
    if(scenario>=6)return UINT32_MAX;
    for(i=10;i>=0;i--){uint32_t kind=get(a,p+0xd68+(unsigned)i*4);
        if(integer(kind)>0 && kind!=get(a,0x590a08+scenario*4) && kind!=get(a,0x590a20+scenario*4) &&
           integer(get(a,root+0x200+kind*4))>1)return kind;}
    return UINT32_MAX;
}
/*4bafd0 deliberately walks d64 downward, a different priority window from
 *d68..d90. Keep this original storage choice; do not normalize the two lists. */
static uint32_t economy_surplus(GameApplication *a,GameAddress p,uint32_t wanted){
    uint32_t best=UINT32_MAX,fallback=0;GameAddress root=get(a,0x58973c);unsigned i;
    for(i=0;i<11;i++){uint32_t kind=get(a,p+0xd64-i*4),quantity=get(a,root+0x200+kind*4);
        if(kind==wanted && !fallback)fallback=UINT32_MAX;
        if(integer(quantity)<=0)continue;
        if(!fallback && (kind!=5 || get(a,p+0xf58)))fallback=kind;
        if(integer(quantity)>1 && (integer(best)<0 || integer(quantity)>integer(get(a,root+0x200+best*4))) &&
           (kind!=5 || get(a,p+0xf58)))best=kind;
    }
    return integer(best)>=0?best:fallback;
}
/*4bdce0: randomized unmet scenario, then least numerous configured class. */
static int economy_balanced(GameApplication *a,GameAddress p,uint32_t *out){
    GameAddress root=get(a,0x58973c);uint32_t cursor,best=UINT32_MAX;unsigned i;int has_active=0;
    for(i=0;(int)i<site_count(a);i++){GameAddress site=work_get(a,p,0x26c,i);
        if(site && short_at(a,site+36)>short_at(a,site+38) && integer(get(a,site+20))>0){has_active=1;break;}}
    if(!random_scaled(a,real(a,0x5746a0),&cursor))return 0;
    for(i=0;i<5;i++){cursor++;if(cursor==5)cursor=0;if(cursor>=6)return 0;
        if(cursor!=get(a,p+0xd94)){
            uint32_t prerequisite=get(a,0x590a08+cursor*4),desired=get(a,0x590a20+cursor*4);
            if(integer(get(a,root+0x200+prerequisite*4))>=integer(get(a,0x590a38+cursor*4)) &&
               integer(get(a,root+0x200+desired*4))<1){*out=desired;return !a->memory.fault;}
        }
    }
    if(!random_scaled(a,real(a,0x5746e8),&cursor))return 0;
    for(i=0;i<11;i++){uint32_t kind;cursor++;if(cursor==11)cursor=0;if(cursor>=11)return 0;
        kind=get(a,p+0xd68+cursor*4);if(integer(kind)<=0 || (kind==10 && !has_active))continue;
        if(integer(best)<0 || integer(get(a,root+0x200+best*4))>integer(get(a,root+0x200+kind*4)))best=kind;
    }
    *out=best;return !a->memory.fault;
}
/*Remove a borrowed secondary through its existing recyclable owner. The
 *forward kind10 swap preserves the displaced class and production ordering. */
static int economy_swap_ten(GameApplication *a,GameAddress p){
    int i,j;uint32_t result;
    for(i=0;i<site_count(a);i++){GameAddress site=economy_site(a,(unsigned)i);
        if(get(a,site+84)!=get(a,0x5f0220) || !get(a,site+20) || !economy_safe(a,p,(unsigned)i))continue;
        for(j=1;j<short_at(a,site+36);j++){GameAddress descriptor=get(a,get(a,site+24)+(unsigned)j*4);
            uint32_t kind;if(!descriptor || short_at(a,descriptor+20)==10)continue;
            kind=(uint32_t)(int32_t)short_at(a,descriptor+20);
            return game_level_group_remove_member(a,site,(unsigned)j) && economy_produce(a,p,10,&result) && economy_produce(a,p,kind,&result);
        }
    }
    return !a->memory.fault;
}
static int economy_remove_surplus(GameApplication *a,GameAddress p,uint32_t kind,int active,int owned_slots,int *removed){
    int i,j;*removed=0;
    for(i=site_count(a)-1;i>=0;i--){GameAddress site=work_get(a,p,0x26c,(unsigned)i),descriptors;
        if(!site || (active && integer(get(a,site+20))<=0) || short_at(a,site+38)<=1 || !economy_safe(a,p,(unsigned)i))continue;
        descriptors=get(a,(owned_slots?site:economy_site(a,(unsigned)i))+24);
        for(j=1;j<short_at(a,site+36);j++){GameAddress d=get(a,descriptors+(unsigned)j*4);
            if(d && (uint32_t)(int32_t)short_at(a,d+20)==kind){
                if(!game_level_group_remove_member(a,site,(unsigned)j))return 0;*removed=1;return !a->memory.fault;}}
    }
    return !a->memory.fault;
}
/*4ba6d0: complete scheduled descriptor economy, including replacement and
 *class5 cooldown. Production status and application failure stay distinct. */
static int descriptor_economy(GameApplication *a,GameAddress p){
    uint32_t tick=get(a,0x5ac6ac),kind,replacement,result;int removed,required;
    if(integer(get(a,p+0x5f0))>integer(tick))return !a->memory.fault;
    put(a,p+0x5f0,tick+160);if(integer(get(a,p+0x28c))<1)return cancel(a,p);
    kind=game_level_imperial_missing_class(a,p);required=!kind;
    if(required)kind=economy_required(a,p);
    if(kind){
        if(kind==10 && get(a,p+0x2a4))return economy_produce(a,p,kind,&result);
        if(kind==10 && get(a,p+0x2a0)){
            /*The first missing-class branch tests site14==0; the later
             *required/balanced branches test nonzero. Keep both below. */
            if(required)return economy_swap_ten(a,p);
            {int i,j;for(i=0;i<site_count(a);i++){GameAddress site=economy_site(a,(unsigned)i);
                if(get(a,site+84)!=get(a,0x5f0220) || get(a,site+20) || !economy_safe(a,p,(unsigned)i))continue;
                for(j=1;j<short_at(a,site+36);j++){GameAddress d=get(a,get(a,site+24)+(unsigned)j*4);uint32_t displaced;
                    if(!d || short_at(a,d+20)==10)continue;displaced=(uint32_t)(int32_t)short_at(a,d+20);
                    return game_level_group_remove_member(a,site,(unsigned)j) && economy_produce(a,p,10,&result) && economy_produce(a,p,displaced,&result);}}}
            return !a->memory.fault;
        }
        if(!get(a,p+0x2a0)){
            replacement=required?economy_replacement(a,p):economy_surplus(a,p,kind);
            if(integer(replacement)<=0)return !a->memory.fault;
            if(!economy_remove_surplus(a,p,replacement,kind==10,required && kind!=10,&removed))return 0;
            /*Required non10 tries production even when no replacement found. */
            if(!removed && (!required || kind==10))return !a->memory.fault;
        }
    }else{
        if(!get(a,p+0x2a0))return !a->memory.fault;
        if(!economy_balanced(a,p,&kind))return 0;
        if(kind==10){if(get(a,p+0x2a4))return economy_produce(a,p,10,&result);return economy_swap_ten(a,p);}
        if(integer(kind)<=0)return !a->memory.fault;
    }
    if(!economy_produce(a,p,kind,&result))return 0;
    if(required && result==kind && kind==5)put(a,p+0xd98,get(a,0x5ac6ac)+get(a,0x6041a4));
    return !a->memory.fault;
}
/*4bb070: inactive owned sites cannot retain class10 secondary defenders.
 *Reread the slot limit after retirement through the shared group owner. */
static int economy_retire_inactive(GameApplication *a,GameAddress p){
    int i,j;
    for(i=0;i<site_count(a);i++){
        GameAddress site=work_get(a,p,0x26c,(unsigned)i);
        if(!site || integer(get(a,site+20))>0)continue;
        for(j=1;j<short_at(a,site+36);j++){
            GameAddress descriptor=get(a,get(a,site+24)+(unsigned)j*4);
            if(descriptor && short_at(a,descriptor+20)==10 && !game_level_group_remove_member(a,site,(unsigned)j))return 0;
        }
    }
    return !a->memory.fault;
}
extern int game_level_imperial_descriptor_health(GameApplication *,GameAddress,uint32_t *,uint32_t *);
/*4be9e0: four human defense mixes, binary presence of each descriptor class.
 *Each addition stores binary32; the aggregate has the original summation order. */
static int hostile_mix(GameApplication *a,GameAddress p,float mix[4][6]){
    static const unsigned lanes[5]={0,2,1,3,4};unsigned owner,kind;
    memset(mix,0,24*sizeof(float));
    for(owner=0;owner<4;owner++){
        GameAddress root=get(a,0x5f0210+owner*4);if(!root)return 0;
        for(kind=1;kind<=10;kind++){
            unsigned family=(kind-1)%5,lane=lanes[family];long double value;
            if(!get(a,root+0x200+kind*4))continue;
            value=integer(get(a,p+0x184+owner*4+(family==1?-0x30:family==2?-0x20:family==3?-0x10:family==4?0x10:0)));
            if(family==0 || family==3)value*=real(a,0x5746b8);else if(family==1)value*=real(a,0x57464c);
            mix[owner][lane]=(float)(value+mix[owner][lane]);
        }
        mix[owner][5]=(float)((long double)mix[owner][1]+mix[owner][2]+mix[owner][3]+mix[owner][4]+mix[owner][0]);
    }
    return !a->memory.fault;
}
/*4beee0: the original walks the first20 floats of the24-float mix in flat
 *order, interpreting them as five groups of four. Do not transpose or skip
 *the per-human aggregate fields. Class6..10 contribute a signed half score. */
static int hostile_mix_scores(GameApplication *a,GameAddress p,const float mix[4][6]){
    uint32_t weights[5][4];unsigned owner,lane,i,j;static const unsigned lanes[5]={0,2,1,3,4};float maximum[4]={0};
    for(lane=0;lane<5;lane++)for(owner=0;owner<4;owner++){
        unsigned offset=lane*4+owner;float value=mix[offset/6][offset%6];
        if(isnan(maximum[owner]) || isnan(value) || maximum[owner]<value)maximum[owner]=value;
    }
    for(lane=0;lane<5;lane++)for(owner=0;owner<4;owner++){
        unsigned offset=lane*4+owner;
        weights[lane][owner]=!(bits(maximum[owner])&0x7fffffffu)?0:
            truncate_word((long double)mix[offset/6][offset%6]/maximum[owner]*real(a,0x574664));
    }
    for(i=0;(int)i<site_count(a);i++){
        GameAddress site=work_get(a,p,0x274,i);uint32_t sum=0,value=0;
        if(site && short_at(a,site+38)>=2){
            owner=game_memory_read_u16(&a->memory,get(a,site+84)+8);if(owner>=4)return 0;
            for(j=1;(int)j<short_at(a,site+36);j++){
                GameAddress descriptor=get(a,get(a,site+24)+j*4);int kind;if(!descriptor)continue;
                kind=short_at(a,descriptor+20);if(kind<1 || kind>10)continue;
                value=weights[lanes[(kind-1)%5]][owner];
                if(kind>=6)value=(value>>1)|(value&0x80000000u);sum+=value;
            }
            value=truncate_word((long double)integer(sum)*real(a,0x5746fc))+1;
        }else value=0;
        work_put(a,p,0xe20,i,value);
    }
    return !a->memory.fault;
}
/*4bed30/4bee10/4bf100/4bf260/4bf380/4bf4f0: score arrays and shared
 *scratch remain strategy-owned. Ranking is the original nonstable selection
 *sort, including inactive zero entries and tie-rank reuse. */
static int hostile_scores(GameApplication *a,GameAddress p,unsigned phase){
    unsigned i,j;int count=site_count(a);uint32_t maximum_history=0;
    if(phase==0){for(i=0;(int)i<count;i++)if(work_get(a,p,0x274,i) && integer(work_get(a,p,0xe40,i))>integer(maximum_history))maximum_history=work_get(a,p,0xe40,i);}
    for(i=0;(int)i<count;i++){
        GameAddress site=work_get(a,p,0x274,i);uint32_t score=0,scratch=0;
        if(phase==0){if(site){uint32_t history=work_get(a,p,0xe40,i);
            score=integer(history)<0?5:10u-truncate_word((long double)integer(history*10u)/integer(maximum_history));}work_put(a,p,0xe34,i,score);continue;}
        if(phase==1){scratch=site?work_get(a,p,0x3b0,i):0;
            if(integer(scratch)<10)score=truncate_word((long double)integer(scratch)*real(a,0x57464c));
            else if(integer(scratch)<50)score=truncate_word((long double)integer(scratch*3u-30u)*real(a,0x5746f8))+5;
            else score=9;work_put(a,p,0xe38,i,i);work_put(a,p,0xe3c,i,scratch);work_put(a,p,0xe1c,i,score);continue;}
        if(phase==2){work_put(a,p,0xe3c,i,i);work_put(a,p,0xe38,i,site?get(a,economy_site(a,i)+20):0);continue;}
        if(phase==3){scratch=0x7fffffffu;
            if(site)for(j=0;(int)j<count;j++){
                GameAddress own=work_get(a,p,0x26c,j),from,to;uint32_t distance;if(!own)continue;
                from=get(a,own+28);to=get(a,site+28);
                distance=truncate_word(assault_distance(a,real(a,from),real(a,from+8),real(a,to),real(a,to+8)));
                if(integer(distance)<integer(scratch))scratch=distance;
            }
            if(site)score=10u+(uint32_t)((int64_t)integer(scratch*10u)/-149630);
            work_put(a,p,0xe38,i,i);if(site)work_put(a,p,0xe3c,i,scratch);work_put(a,p,0xe28,i,score);continue;
        }
        if(phase==4){uint32_t health=0,maximum=0;
            work_put(a,p,0xe38,i,i);work_put(a,p,0xe3c,i,0);
            if(site){GameAddress actual=economy_site(a,i);
                for(j=0;(int)j<short_at(a,actual+36);j++){
                    GameAddress d=get(a,get(a,actual+24)+j*4);uint32_t h,m;
                    if(d){if(!game_level_imperial_descriptor_health(a,d,&h,&m))return 0;health+=h;maximum+=m;}
                }
                scratch=truncate_word((long double)integer(health*100u)/integer(maximum));work_put(a,p,0xe3c,i,scratch);
                if(scratch!=100)score=truncate_word((long double)integer((99u-scratch)*2u)*real(a,0x574700))+3;
            }
            work_put(a,p,0xe2c,i,score);continue;
        }
        if(phase==5 && site){GameAddress point=get(a,site+28),player=get(a,get(a,site+84)+0x3bc);
            if(!player)return 0;score=9u-truncate_word((long double)assault_distance(a,real(a,player+24),real(a,player+32),real(a,point),real(a,point+8))*real(a,0x574704));}
        work_put(a,p,0xe30,i,score);
    }
    if(phase==2){uint32_t rank=0;
        for(i=0;(int)i<count;i++){
            for(j=i+1;(int)j<count;j++){
                uint32_t first=work_get(a,p,0xe3c,i),second=work_get(a,p,0xe3c,j);
                if(first>=(unsigned)count || second>=(unsigned)count)return 0;
                if(integer(work_get(a,p,0xe38,first))<integer(work_get(a,p,0xe38,second))){work_put(a,p,0xe3c,i,second);work_put(a,p,0xe3c,j,first);}
            }
            j=work_get(a,p,0xe3c,i);
            if(work_get(a,p,0x274,j)){
                if(i){unsigned previous=work_get(a,p,0xe3c,i-1);
                    if(work_get(a,p,0xe38,previous)!=work_get(a,p,0xe38,j) && work_get(a,p,0x274,previous))rank++;}
                work_put(a,p,0xe24,j,(uint32_t)(((int64_t)count-rank)*9/count));
            }else work_put(a,p,0xe24,j,0);
        }
    }
    return !a->memory.fault;
}
static int hostile_cache_target(GameApplication *a,GameAddress p,unsigned index,GameAddress site,int history){
    int j;GameAddress payload=0;put(a,p+0x340+index*4,site);
    for(j=0;j<short_at(a,site+36);j++){GameAddress descriptor=get(a,get(a,site+24)+(unsigned)j*4);
        if(descriptor){if(!game_level_group_descriptor_payload(a,descriptor,&payload))return 0;break;}}
    if(!payload)return 0;put(a,p+0x3c0+index*4,payload);put(a,p+0x3f8+index*4,get(a,payload+20));
    return (!history || target_history(a,p,index)) && phase_advance(a,p);
}
/*4b70f0: real hostile selection cadence and phased shared scoring. Native
 *address IDs for the two alternate selectors dispatch ordinary programs. */
static int plan_hostile_site(GameApplication *a,GameAddress p){
    static const unsigned fields[7]={0xe28,0xe34,0xe24,0xe2c,0xe30,0xe1c,0xe20};
    static const GameAddress coefficients[7]={0x6041b0,0x6041c0,0x604154,0x604198,0x604150,0x60415c,0x6041a8};
    uint32_t index=get(a,p+0x1c4),hostiles,selected=UINT32_MAX;unsigned i,k,phase;int count;
    if(index>=14 || !game_level_imperial_census(a,p,1))return 0;hostiles=get(a,p+0x294);count=site_count(a);
    if(integer(hostiles)<1 || (integer(hostiles)<2 && index==6))return cancel(a,p);
    if(integer(hostiles)<2)for(i=0;(int)i<count;i++){GameAddress site=work_get(a,p,0x274,i);if(site)return hostile_cache_target(a,p,index,site,1);}
    if(integer(get(a,p+0xdd8+index*4))>1 || (get(a,p+0xe14)==1 && integer(get(a,p+0xe18))>1)){
        GameAddress site=get(a,p+0x340+index*4);int j;
        if(site && work_get(a,p,0x274,get(a,site)))for(j=0;j<short_at(a,site+36);j++)
            if(get(a,get(a,site+24)+(unsigned)j*4))return hostile_cache_target(a,p,index,site,0);
        put(a,p+0xdd8+index*4,0);return cancel(a,p);
    }
    phase=game_memory_read_u8(&a->memory,p+0x1b6+index);
    if(phase==0){
        GameAddress method=get(a,get(a,p)+(index==6?0x1d0:0x1e4));
        if(index==6)return method==0x4b6e00 && choose_hostile_descriptor(a,p);
        if(get(a,p+0x29c))return method==0x4b7010 && choose_secondary_descriptor(a,p);
        if(!hostile_scores(a,p,4) || !hostile_scores(a,p,5) || !hostile_scores(a,p,2) || !hostile_scores(a,p,1) || !hostile_scores(a,p,0))return 0;
    }else if(phase==1){float mix[4][6];if(!hostile_mix(a,p,mix) || !hostile_mix_scores(a,p,mix))return 0;}
    else if(phase==2){if(!hostile_scores(a,p,3))return 0;}
    else if(phase==3){uint32_t high=0x80000001u,low=0x7fffffffu;
        for(i=0;(int)i<count;i++)if(work_get(a,p,0x274,i)){
            uint32_t score=0;for(k=0;k<7;k++)score+=work_get(a,p,fields[k],i)*get(a,coefficients[k]);
            if(integer(high)<integer(score)){high=score;selected=i;}if(integer(low)>=integer(score))low=score;
        }
        if(integer(selected)<0 || high==low){uint32_t nth;
            if(!random_scaled(a,(float)integer(hostiles),&nth))return 0;nth++;
            for(i=0;(int)i<count && nth;i++)if(work_get(a,p,0x274,i) && !--nth){selected=i;break;}
            if(nth || selected>=(unsigned)count)return 0;
        }
        return hostile_cache_target(a,p,index,work_get(a,p,0x274,selected),1);
    }else return phase_advance(a,p);
    game_memory_write_u8(&a->memory,p+0x1b6+index,(uint8_t)(phase+1));return !a->memory.fault;
}
static void hostile_count_flags(uint32_t flags,uint32_t *first,uint32_t *second,uint32_t *both){
    if(flags&0x2000u){if(flags&0x1000u){(*both)++;return;}(*second)++;}
    if(flags&0x1000u)(*first)++;
}
/*4b7670: size the actual assault against nearby hostile scene members and
 *the primary's three attached units. Reuse the native world index, alliance
 *matrix, visibility mask and authoritative formation counters. */
static int size_hostile_assault(GameApplication *a,GameAddress p){
    uint32_t index=get(a,p+0x1c4),first=0,second=0,both=0,identity=get(a,p+20)&63u,quantity;
    GameAddress site,primary,root=get(a,0x58973c),buckets[9];unsigned i,visits;
    static const unsigned fields[]={0x510,0x548,0x580,0x5b8};
    if(index>=14)return 0;site=get(a,p+0x340+index*4);if(!site)return phase_advance(a,p);
    i=get(a,site);
    if(integer(work_get(a,p,0x310,i))>20 || integer(get(a,get(a,p+0x314)+i*12+8))>5){
        if(!phase_advance(a,p))return 0;game_memory_write_u8(&a->memory,p+0x1b6+index,8);return !a->memory.fault;
    }
    primary=get(a,get(a,site+24));
    if(!gameplay_spatial_query_values(0x5f0800,real(a,site+0x48),real(a,site+0x50),buckets))return 0;
    for(i=0;i<9;i++){
        GameAddress object=get(a,buckets[i]+4);visits=0;
        while(object){uint8_t kind=game_memory_read_u8(&a->memory,object+12);
            if(++visits>65536)return 0;
            if(kind!=0x24 && kind!=0x22 && !get(a,0x5f0160+identity*24+(get(a,object+20)&63u)*4))
                hostile_count_flags(get(a,object+16),&first,&second,&both);
            object=get(a,object+4);
        }
    }
    if(primary)for(i=0;i<3;i++){GameAddress object=get(a,primary+0x50+i*4);
        if(object)hostile_count_flags(get(a,object+16),&first,&second,&both);}
    for(i=0;i<4;i++)put(a,p+fields[i]+index*4,i==2?4:0);
    quantity=get(a,root+0x210);
    if(!primary){put(a,p+0x580+index*4,8);put(a,p+0x5b8+index*4,integer(quantity)>0);}
    else{
        if(get(a,p+0x1a4)&64u){uint32_t half=(quantity>>1)|(quantity&0x80000000u);
            put(a,p+0x5b8+index*4,index==5?quantity-half:half);}
        else put(a,p+0x5b8+index*4,quantity);
        put(a,p+0x510+index*4,2);
    }
    if(first || both){put(a,p+0x548+index*4,2);put(a,p+0x580+index*4,get(a,p+0x580+index*4)+4);}
    if(second)put(a,p+0x510+index*4,get(a,p+0x510+index*4)+2);
    if(!get(a,p+0x548+index*4)){
        GameAddress target=get(a,p+0x3c0+index*4);uint32_t x,z;
        if(!target)return 0;x=truncate_word(real(a,target+24));z=truncate_word(real(a,target+32));
        if(game_memory_read_u8(&a->memory,0x6020f0+(GameAddress)(integer(z&0xfffff07fu)>>7)+(GameAddress)(integer(x)>>12)))put(a,p+0x548+index*4,2);
    }
    if(get(a,p+0xe14)==1){put(a,p+0x548+index*4,3);put(a,p+0x580+index*4,truncate_word((long double)integer(get(a,p+0x580+index*4))*1.5L));}
    if(get(a,p+0xdd8+index*4)&1u)put(a,p+0x5b8+index*4,0);
    for(i=0;i<4;i++)if(integer(get(a,p+fields[i]+index*4))>=8)put(a,p+fields[i]+index*4,8);
    return phase_advance(a,p);
}
static void hostile_point(GameApplication *a,GameAddress p,float point[3]){unsigned i;for(i=0;i<3;i++)point[i]=real(a,p+i*4);}
static int hostile_route_endpoint(GameApplication *a,GameAddress route,float point[3]){
    GameAddress tail=get(a,route+16);unsigned length=game_memory_read_u8(&a->memory,tail+13);
    if(!length || length>32)return 0;
    point[0]=(float)(game_memory_read_u8(&a->memory,get(a,tail+16)+length-1)*1024u);
    point[1]=0;point[2]=(float)(game_memory_read_u8(&a->memory,get(a,tail+20)+length-1)*1024u);return !a->memory.fault;
}
static int hostile_source(GameApplication *a,GameAddress p,uint32_t needed,GameAddress *site,unsigned *slot){
    unsigned i,j,count=0;*site=0;*slot=0;
    for(i=0;(int)i<site_count(a) && count<needed;i++){
        GameAddress candidate=economy_site(a,i),owner=get(a,candidate+84);if(!owner || game_memory_read_u16(&a->memory,owner+8)!=(get(a,p+20)&63u))continue;
        for(j=1;(int)j<short_at(a,candidate+36) && count<needed;j++){
            GameAddress descriptor=get(a,get(a,candidate+24)+j*4);
            if(descriptor && short_at(a,descriptor+20)==4){count++;*site=candidate;*slot=j;}
        }
    }
    return count>=needed && needed!=0;
}
static int hostile_route_alternate(GameApplication *a,GameAddress route,GameAddress alternate){
    GameAddress temporary=recyclable_record_allocate(0x24),candidate;int32_t selected;int found,ok=0,initialized=0;
    if(!temporary)return 0;if(!recyclable_outer_initialize(&a->memory,temporary,NULL))goto done;initialized=1;
    found=recyclable_grid_pick_route(&a->memory,0x5cf038,route,256,&selected);if(a->memory.fault)goto done;
    candidate=found?get(a,0x5cf038+0x1203c+(uint32_t)selected*4):route;
    if(found && !recyclable_outer_copy(&a->memory,temporary,candidate,NULL))goto done;
    if(!recyclable_outer_copy(&a->memory,alternate,found?temporary:route,NULL))goto done;
    ok=1;
done:{GameMemory cleanup=a->memory;cleanup.fault=0;
    if(initialized){if(!recyclable_record_destroy(&cleanup,temporary,1,NULL))ok=0;}else recyclable_record_release(temporary);}
    return ok && !a->memory.fault;
}
static int hostile_site_warning(GameApplication *a,GameAddress site){
    GameAddress player=get(a,0x5f0208),point;uint32_t tick=get(a,0x5ac6ac),age,id=get(a,site),x,z;int started;
    int16_t words[8]={0x57,0x5d,0,0,0x5f,-1,-1,-1};
    if(get(a,site+84)!=player)return !a->memory.fault;
    age=tick-get(a,player+0x590);
    if(!((integer(age)>160 && get(a,player+0x594)!=id) || integer(age)>320))return !a->memory.fault;
    point=get(a,site+28);
    z=29u+truncate_word((long double)real(a,point+8)*real(a,0x571f64)*real(a,0x571f68));
    x=48u-truncate_word((long double)real(a,point)*real(a,0x571f64)*real(a,0x571f68));
    words[2]=(int16_t)(uint16_t)x;words[3]=(int16_t)(uint16_t)z;
    if(!game_menu_sound_sequence(a,words,&started))return 0;
    if(started){game_memory_write_u16(&a->memory,player+0x598,0x57);put(a,player+0x590,tick);put(a,player+0x58c,tick);put(a,player+0x594,id);}
    return !a->memory.fault;
}
static int hostile_route_orders(GameApplication *a,GameAddress head,GameAddress route,unsigned priority){
    GameAddress node=get(a,route+12);unsigned visits=0;float point[3]={0,32768,0};
    if(!head)return 1;if(!node)return 0;game_memory_write_u8(&a->memory,node+12,0);
    while(node){unsigned cursor=game_memory_read_u8(&a->memory,node+12),length=game_memory_read_u8(&a->memory,node+13);
        if(++visits>65536 || cursor>=length || length>32)return 0;
        point[0]=(float)(game_memory_read_u8(&a->memory,get(a,node+16)+cursor)*1024u);
        point[2]=(float)(game_memory_read_u8(&a->memory,get(a,node+20)+cursor)*1024u);
        game_memory_write_u8(&a->memory,node+12,(uint8_t)(cursor+1));
        if(cursor+1==32){node=get(a,node+4);if(node)game_memory_write_u8(&a->memory,node+12,0);}
        else if(cursor+1>=length)node=0;
        /*The final point is already represented by the target command. */
        if(node && !game_level_scene_point_order(a,head,priority,0,0,point))return 0;
    }
    return !a->memory.fault;
}
static int hostile_publish_formation(GameApplication *a,GameAddress p,unsigned index,GameAddress site){
    static const unsigned counters[3]={0x580,0x548,0x510},arrays[3]={0x62c,0x9ac,0x7ec};
    unsigned family,j,site_id=get(a,site),base=site_id*8;GameAddress merged=work_get(a,p,0x2c8,base+7),tail=merged;
    unsigned visits=0;float zero[3]={0};GameAddress target=get(a,p+0x3c0+index*4);
    if(!hostile_site_warning(a,site))return 0;
    while(tail && get(a,tail+0x10c)){if(++visits>65536)return 0;tail=get(a,tail+0x10c);}
    for(family=0;family<3;family++){
        GameAddress head=work_get(a,p,0x2c8,base+family);
        if(!head)continue;
        if(tail){if(!pair_link(a,head,0x104,tail) || !pair_link(a,tail,0x108,head))return 0;}
        else{work_put(a,p,0x2c8,base+7,head);tail=head;}
        while(get(a,tail+0x10c)){if(++visits>65536)return 0;tail=get(a,tail+0x10c);}
        work_put(a,p,0x30c,base+family,0);work_put(a,p,0x2c8,base+family,0);
    }
    put(a,p+0x5f4+index*4,0);
    for(family=0;family<3;family++){
        uint32_t count=get(a,p+counters[family]+index*4);if(integer(count)<0 || count>8)return 0;
        for(j=0;j<count;j++){
            GameAddress slot=p+arrays[family]+index*4+j*56,object=get(a,slot),head;
            if(object || family==2){put(a,p+0x5f4+index*4,get(a,p+0x5f4+index*4)+1);work_put(a,p,0x310,site_id,work_get(a,p,0x310,site_id)+1);}
            if(!object)continue;head=work_get(a,p,0x2c8,base+family);
            if((head && !pair_link(a,head,0x104,object)) || !pair_link(a,object,0x108,head) || !pair_link(a,object,0x104,p))return 0;
            work_put(a,p,0x2c8,base+family,object);put(a,slot,0);
        }
    }
    for(family=0;family<3;family++){
        GameAddress head=work_get(a,p,0x2c8,base+family);unsigned priority=family==1?2:0;
        if(head && !game_level_scene_point_order(a,head,priority,5,target,zero))return 0;
        if(family<2 && !hostile_route_orders(a,head,get(a,p+(family?0x430:0x468)+index*4),priority))return 0;
    }
    game_memory_write_u8(&a->memory,p+0x1b6+index,8);return !a->memory.fault;
}
/*4b7ab0: complete site assault lifecycle, from existing descriptor sources,
 *owned weighted routes and factory production to command publication and
 *settlement. Phases are gameplay states; no guest registers or stack survive. */
static int execute_hostile_assault(GameApplication *a,GameAddress p){
    unsigned index=get(a,p+0x1c4),phase,i,j,slot;GameAddress site,target,route,alternate,source,object,root=get(a,0x58973c);
    uint32_t query,needed,tick=get(a,0x5ac6ac);float target_point[3],point[3],sides[2][3];
    if(index>=14)return 0;site=get(a,p+0x340+index*4);if(!site)return phase_advance(a,p);
    target=get(a,p+0x3c0+index*4);if(!target)return 0;
    if((get(a,target+16)&0x20000u) || get(a,target+20)!=get(a,p+0x3f8+index*4)){assault_clear_search(a,p,index);return cancel(a,p);}
    hostile_point(a,target+24,target_point);route=get(a,p+0x430+index*4);alternate=get(a,p+0x468+index*4);
    if(!route || !alternate)return 0;phase=game_memory_read_u8(&a->memory,p+0x1b6+index);
    if(phase==0){static const unsigned arrays[4]={0x62c,0x9ac,0x7ec,0xb6c};
        for(i=0;i<8;i++)for(j=0;j<4;j++)put(a,p+arrays[j]+index*4+i*56,0);
        if(!game_level_imperial_site_refresh(a,p,get(a,site),1))return 0;phase=1;
    }
    if(phase==1){
        needed=get(a,p+0x5b8+index*4);
        if(!needed || !get(a,root+0x210)){phase=4;goto publish_phase;}
        if(get(a,0x5cf038+0x1804))goto publish_phase;
        if(!hostile_source(a,p,needed,&source,&slot)){phase=4;goto publish_phase;}
        hostile_point(a,get(a,source+28)+slot*12,point);put(a,p+0x4a0+index*4,0);
        /*4b7d33 calls453530: keep the route's owned first point for reuse. */
        if(!recyclable_outer_reset_payload(&a->memory,route,NULL) ||
           !recyclable_outer_append(&a->memory,route,(uint8_t)(integer(truncate_word(target_point[0]))>>10),(uint8_t)(integer(truncate_word(target_point[2]))>>10),0,0,NULL) ||
           !recyclable_outer_append(&a->memory,route,(uint8_t)(integer(truncate_word(point[0]))>>10),(uint8_t)(integer(truncate_word(point[2]))>>10),0,0,NULL))return 0;
        phase=2;goto publish_phase;
    }
    if(phase==2){
        query=get(a,p+0x4a0+index*4);if(query){query=recyclable_grid_route_continue(&a->memory,0x5cf038,route,NULL);put(a,p+0x4a0+index*4,query);}
        if(query)goto publish_phase;if(!recyclable_route_simplify(&a->memory,route,0x40a00000,NULL))return 0;
        needed=get(a,p+0x5b8+index*4);if(!needed || needed>8 || !hostile_source(a,p,needed,&source,&slot))return 0;
        hostile_point(a,get(a,source+28)+slot*12,point);
        if(!game_level_imperial_produce(a,p,0x26,point,0,-1.0f,&object))return 0;
        /*The original row8 overlaps later strategy scalars. It remains inside
         *the owned legacy record; preserve that verified storage address. */
        put(a,p+0xb6c+index*4+needed*56,object);phase=3;
    }
    if(phase==3){PresentationCommand command={0};needed=get(a,p+0x5b8+index*4);if(needed>8)return 0;
        object=get(a,p+0xb6c+index*4+needed*56);
        if(!object){put(a,p+0x5b8+index*4,needed-1);phase=1;goto publish_phase;}
        if(!get(a,object+0xdc))goto publish_phase;
        command.priority=2;command.kind=2;command.has_position=1;command.parameter=320;memcpy(command.position,target_point,12);
        if(!game_level_scene_order_issue(a,object,&command,0))return 0;
        command.kind=5;command.parameter=0;command.target=target;command.target_identity=get(a,target+20);command.has_target=1;
        if(!game_level_scene_order_issue(a,object,&command,0))return 0;
        put(a,p+0x5b8+index*4,needed-1);phase=1;goto publish_phase;
    }
    if(phase==4){uint32_t ignored=UINT32_MAX;GameAddress primary=get(a,get(a,site+24)),center=primary?get(a,site+28):target+24;float radius=get(a,p+0xe14)==1?10.5f:14.0f;
        if(!get(a,root+0x204) && !get(a,root+0x20c) && !get(a,root+0x208))return phase_advance(a,p);
        if(get(a,0x5cf038+0x1804))goto publish_phase;
        if(game_memory_read_u8(&a->memory,target+12)==0x23)ignored=get(a,get(a,get(a,target+0x294)+4));
        query=recyclable_grid_attack_begin(&a->memory,0x5cf038,(uint8_t)(integer(truncate_word(target_point[0]))>>10),(uint8_t)(integer(truncate_word(target_point[2]))>>10),
            (uint8_t)(integer(truncate_word(real(a,center)))>>10),(uint8_t)(integer(truncate_word(real(a,center+8)))>>10),bits(radius),route,get(a,p+20)&63u,ignored,0xbf800000u,UINT32_MAX,NULL);
        put(a,p+0x4a0+index*4,query);phase=5;if(query)goto publish_phase;
    }
    if(phase==5){
        query=get(a,p+0x4a0+index*4);if(query){query=recyclable_grid_route_continue(&a->memory,0x5cf038,route,NULL);put(a,p+0x4a0+index*4,query);}
        if(query)goto publish_phase;
        if(integer(get(a,route+24))>0x3f800000){
            int found=0;for(i=0;(int)i<short_at(a,site+36) && !found;i++){
                GameAddress descriptor=get(a,get(a,site+24)+i*4),payload;if(!descriptor)continue;
                if(!game_level_group_descriptor_payload(a,descriptor,&payload))return 0;if(payload!=target)continue;
                for(j=i+1;(int)j<short_at(a,site+36);j++){descriptor=get(a,get(a,site+24)+j*4);if(!descriptor)continue;
                    if(!game_level_group_descriptor_payload(a,descriptor,&payload) || !payload)return 0;
                    put(a,p+0x3c0+index*4,payload);put(a,p+0x3f8+index*4,get(a,payload+20));found=1;break;}
            }
            if(found){phase=4;goto publish_phase;}
            put(a,p+0x580+index*4,0);put(a,p+0x548+index*4,0);
            if(!recyclable_outer_reset_payload(&a->memory,alternate,NULL) || !recyclable_outer_reset_payload(&a->memory,route,NULL))return 0;
        }else if(!hostile_route_alternate(a,route,alternate))return 0;
        if(!recyclable_route_simplify(&a->memory,route,0,NULL) || !recyclable_route_simplify(&a->memory,alternate,0,NULL))return 0;phase=6;
    }
    if(phase==6){static const unsigned counters[3]={0x580,0x548,0x510},arrays[3]={0x62c,0x9ac,0x7ec},enabled[3]={0x204,0x208,0x20c};static const uint8_t kinds[3]={0x40,0x21,0x27};
        if(!get(a,p+0x580+index*4) && !get(a,p+0x548+index*4)){float xz[2]={0};
            (void)gameplay_geometry_nearby_location(&a->memory,target_point[0],target_point[2],15155.2001953125f,xz);if(a->memory.fault)return 0;
            sides[0][0]=sides[1][0]=xz[0];sides[0][1]=sides[1][1]=32768;sides[0][2]=sides[1][2]=xz[1];
        }else if(!hostile_route_endpoint(a,route,sides[0]) || !hostile_route_endpoint(a,alternate,sides[1]))return 0;
        for(i=0;i<3;i++){uint32_t count=get(a,p+counters[i]+index*4),success=0;if(integer(count)<0 || count>8)return 0;
            if(get(a,root+enabled[i]))for(j=0;j<count;j++){
                if(!game_level_imperial_produce(a,p,kinds[i],sides[j&1],1,-1.0f,&object))return 0;
                put(a,p+arrays[i]+index*4+success*56,object);if(object)success++;
            }
            put(a,p+counters[i]+index*4,success);
        }
        phase=7;
    }
    if(phase==7){if(integer(tick-get(a,p+0xda0+index*4))>1920)return cancel(a,p);
        if(!hostile_publish_formation(a,p,index,site))return 0;phase=8;goto publish_phase;}
    if(phase==8){
        for(i=0;i<8;i++){GameAddress address=p+0xb6c+index*4+i*56;object=get(a,address);
            if(object && (!get(a,object+88) || !get(a,object+0x158)))put(a,address,0);}
        if(get(a,site+84) && get(a,site+84)!=root){
            if(integer(tick-get(a,p+0xda0+index*4))>3840)return phase_advance(a,p);
            i=get(a,site);
            if((long double)integer(work_get(a,p,0x310,i))>(long double)integer(get(a,p+0x5f4+index*4))*real(a,0x5746b0))goto publish_phase;
            if(integer(get(a,get(a,p+0x314)+i*12+8))<=3 && integer(get(a,get(a,p+0x314)+i*12+4))<=3)return phase_advance(a,p);
        }else{
            for(i=0;i<8;i++){GameAddress address=p+0xb6c+index*4+i*56;object=get(a,address);if(!object)continue;
                hostile_point(a,object+24,point);if(!point_command(a,object,2,2,point))return 0;put(a,object+0x158,0);put(a,address,0);}
            return phase_advance(a,p);
        }
    }
publish_phase:game_memory_write_u8(&a->memory,p+0x1b6+index,(uint8_t)phase);return !a->memory.fault;
}
extern int game_level_scene_identity_replace(GameApplication *,GameAddress,uint32_t);
static void defensive_cooldown(GameApplication *a,GameAddress p,unsigned field,GameAddress period){
    uint32_t interval=get(a,period),scaled=get(a,p+0x268)*interval;
    put(a,p+field,truncate_word((long double)integer(get(a,0x5ac6ac)+interval)-(long double)integer(scaled)*real(a,0x574688)));
}
static uint8_t defensive_visibility(GameApplication *a,float x,float z){
    uint32_t ix=truncate_word(x),iz=truncate_word(z);
    return game_memory_read_u8(&a->memory,0x6020f0+(GameAddress)(integer(iz&0xfffff07fu)>>7)+(GameAddress)(integer(ix)>>12));
}
/*412380 uses signed short angles, one shared RNG draw and binary32 stores. */
static int sector_point(GameApplication *a,int start,int span,float radius,const float center[3],float point[3]){
    int16_t width=(int16_t)(uint16_t)span;uint32_t draw;unsigned angle;
    if(!random_scaled(a,(float)width,&draw))return 0;
    angle=((uint16_t)(start+span/2)+(uint16_t)draw-(uint16_t)(width>>1))&2047u;
    point[2]=(float)((long double)real(a,0x6298a8+((angle+512)&2047)*4)*radius+center[2]);
    point[0]=(float)((long double)real(a,0x6298a8+angle*4)*radius+center[0]);return !a->memory.fault;
}
/*4bcf00: real factory payload, collision search, placement/arrival publication
 *and payment. The virtual placement receives ID, position and zero angles;
 *the arguments remain on the original stack across sequence allocation. */
static int primary_deploy(GameApplication *a,GameAddress p,int start,int span,int arrival_mode,uint32_t *result){
    unsigned index=get(a,p+0x1c4),i,kind=0x22,n22=0,other=0;int tries,clear=0;
    GameAddress root=get(a,0x58973c),site,primary,unit,arrival=0,payload,human;
    uint32_t draw,cost,allowed,variant,id,old_capacity;float center[3]={0},point[3]={0},radius;
    long double dx,dz;static const uint16_t angles[3]={0};
    *result=UINT32_MAX;if(index>=14)return 0;site=get(a,p+0x340+index*4);
    if(!site || !(primary=get(a,get(a,site+24))))return 0;
    center[0]=(float)(((long double)real(a,site+0x40)+real(a,site+0x38))*real(a,0x57464c));
    center[2]=(float)(((long double)real(a,site+0x3c)+real(a,site+0x44))*real(a,0x57464c));
    dx=(long double)real(a,site+0x38)-real(a,site+0x40);dz=(long double)real(a,site+0x3c)-real(a,site+0x44);
    radius=(float)(sqrtl(dx*dx+dz*dz)*real(a,0x57464c));
    human=integer(get(a,p+0x2b4))>=0?get(a,0x5f0210+get(a,p+0x2b4)*4):0;
    if(!human || get(a,human+0x210) || get(a,human+0x20c)){
        for(i=0;i<3;i++){GameAddress object=get(a,primary+0x50+i*4);if(!object)continue;
            if(game_memory_read_u8(&a->memory,object+12)==0x22)n22++;else other++;}
        if(n22 && !other)kind=0x24;else if(!n22 && other)kind=0x22;
        else{if(!random_scaled(a,real(a,0x5746c0),&draw))return 0;kind=draw<1?0x24:0x22;}
    }
    if(!game_level_player_catalog_value(a,root,(uint8_t)kind,8,0,&cost) ||
       gameplay_catalog_unit_affordable(&a->memory,root,cost,&allowed)!=1)return 0;
    if(!allowed)return !a->memory.fault;
    if(!game_level_player_catalog_value(a,root,(uint8_t)kind,3,0,&variant) || variant>UINT16_MAX)return 0;
    unit=game_level_factory_take(a,(uint8_t)kind,(uint16_t)variant);if(!unit)return 0;
    for(tries=10;tries>0;){uint8_t x,z;
        if(!sector_point(a,start,span,radius,center,point))goto failure;
        point[0]=(float)((long double)integer(truncate_word(point[0])&0xfffffe00u)+real(a,0x5746d8));
        point[2]=(float)((long double)integer(truncate_word(point[2])&0xfffffe00u)+real(a,0x5746d8));
        point[1]=(float)(gameplay_geometry_terrain_height(&a->memory,point[0],point[2])+real(a,unit+0x30));
        if(!gameplay_geometry_position_clear(&a->memory,unit,point,&clear))goto failure;
        x=(uint8_t)(integer(truncate_word(point[0]))>>9);z=(uint8_t)(integer(truncate_word(point[2]))>>9);
        clear=clear && !blocked(a,0x610520,x,z);if(a->memory.fault)goto failure;
        if(--tries>0 && clear)break;
    }
    /*Retail rejects even a successful tenth trial. */
    if(!tries)return unused(a,unit);
    payload=get(a,primary+0x48);if(!payload)goto failure;
    old_capacity=get(a,primary+0x4c);put(a,primary+0x4c,old_capacity-1);
    if(arrival_mode){
        arrival=game_level_factory_take(a,0x45,0);if(!arrival)goto restore;
        if(!sequence(a,root,&id) || !game_level_arrival_produce_requested(a,arrival,id,point,unit,get(a,payload+20),0) ||
           !publish(a,root,arrival))goto restore;
    }else{
        if(!sequence(a,root,&id) || !game_level_object_place_angles(a,unit,id,point,angles))goto restore;
        put(a,unit+0x260,get(a,payload+20));
        for(i=0;i<3;i++)if(!get(a,primary+0x50+i*4))break;
        if(i==3)goto restore; /*Capacity must not overwrite another descriptor field. */
        put(a,primary+0x50+i*4,unit);if(!publish(a,root,unit)){put(a,primary+0x50+i*4,0);goto restore;}
    }
    defensive_cooldown(a,p,0x258,0x604144);
    if(!game_level_player_catalog_value(a,root,(uint8_t)kind,8,0,&cost) || !spend(a,root,cost))return 0;
    *result=1;return !a->memory.fault;
restore:put(a,primary+0x4c,old_capacity);if(arrival)(void)game_level_factory_destroy(a,arrival);
failure:(void)game_level_factory_destroy(a,unit);return 0;
}
/*4bcce0: choose the first unoccupied angular sector around primary defenses.
 *Keep the original quotient stores/table lookup rather than atan2 rounding. */
static int primary_sector(GameApplication *a,GameAddress p,uint32_t *result){
    unsigned index=get(a,p+0x1c4),i,count=0,pass;int start=0,occupied=1,angles[3];GameAddress site,primary;
    float cx,cz;*result=0;if(index>=14)return 0;site=get(a,p+0x340+index*4);
    if(!site)return 0;primary=get(a,get(a,site+24));
    if(integer(get(a,p+0x258))>integer(get(a,0x5ac6ac)) || !primary)return !a->memory.fault;
    if(!get(a,primary+0x4c)){defensive_cooldown(a,p,0x258,0x604144);*result=UINT32_MAX;return !a->memory.fault;}
    cx=(float)(((long double)real(a,site+0x40)+real(a,site+0x38))*real(a,0x57464c));
    cz=(float)(((long double)real(a,site+0x3c)+real(a,site+0x44))*real(a,0x57464c));
    for(i=0;i<3;i++){GameAddress unit=get(a,primary+0x50+i*4);long double dx,quotient;float x,z,ratio;uint32_t lookup;uint16_t angle;
        if(!unit)continue;dx=(long double)real(a,unit+24)-cx;x=(float)dx;z=(float)((long double)real(a,unit+32)-cz);
        quotient=dx/z;ratio=(float)quotient;
        if(isnan(quotient) || fabsl(quotient)<real(a,0x574654)){
            lookup=truncate_word((long double)ratio*real(a,0x5746d4)+real(a,0x5746d4));
            angle=game_memory_read_u16(&a->memory,0x62e8b0+lookup*2)&2047;
        }else{
            lookup=truncate_word(((long double)z/x)*real(a,0x5746d4)+real(a,0x5746d4));
            angle=(uint16_t)((integer(bits(ratio))>0?512:1536)-game_memory_read_u16(&a->memory,0x62e8b0+lookup*2));
        }
        angles[count]=(int16_t)angle;if(bits(z)>0x80000000u)angles[count]=(angles[count]+1024)&2047;count++;
    }
    for(pass=0;pass<3 && occupied;pass++){occupied=0;
        for(i=0;i<count && !occupied;i++)if(angles[i]>=start && angles[i]<=start+682)occupied=1;
        if(occupied)start+=682;
    }
    return !a->memory.fault && primary_deploy(a,p,start,682,1,result);
}
/*4b6120: retire idle defenders, repair owned descriptors and replenish site
 *budgets; threatened sites receive real primary units around three sectors. */
static int maintain_formations(GameApplication *a,GameAddress p){
    unsigned index=get(a,p+0x1c4),i,j,k;int selected=-1,count=site_count(a);uint32_t tick=get(a,0x5ac6ac);GameAddress root=get(a,0x58973c);
    if(index>=14)return 0;if(!get(a,p+0x28c))return cancel(a,p);
    for(i=0;(int)i<count;i++){
        GameAddress site=work_get(a,p,0x26c,i),primary;uint32_t stamp=work_get(a,p,0x31c,i),other=work_get(a,p,0x324,i);int quiet;
        if(!site)continue;primary=get(a,get(a,site+24));
        quiet=primary && !(integer(stamp)>0 && integer(stamp+1920)>integer(tick)) &&
            !(integer(other)>=0 && integer(other+1920)>=integer(tick));
        if(quiet){int retired=0;
            for(j=0;j<8;j++){GameAddress object=work_get(a,p,0x2c8,i*8+j);
                while(object){GameAddress next=get(a,object+0x10c),command=get(a,object+0xdc);uint32_t kind=command?get(a,command+12):0;
                    if(get(a,object+20) && kind!=5 && kind!=6 && kind!=7 && !defensive_visibility(a,real(a,object+24),real(a,object+32))){
                        if(!refund_old(a,root,object))return 0;
                    }
                    object=next;
                }
            }
            site=work_get(a,p,0x26c,i);primary=get(a,get(a,site+24));
            if(primary)for(j=0;j<3;j++){GameAddress object=get(a,primary+0x50+j*4);if(object){if(!refund_old(a,root,object))return 0;retired=1;}}
            if(retired && get(a,p+0x13c))put(a,p+0x32c,get(a,p+0x32c)|(1u<<(i&31)));
            for(j=0;(int)j<short_at(a,site+36);j++){GameAddress descriptor=get(a,get(a,site+24)+j*4);uint32_t price,health,maximum,missing,amount,fee;float rate;
                if(!descriptor)continue;
                if(!game_level_group_repair_quote(a,descriptor,root,&price,&health,&maximum))return 0;
                missing=maximum-health-get(a,descriptor+24);if(!missing)continue;
                rate=(float)(((long double)(float)integer(price)/integer(maximum))*real(a,0x574680));
                amount=repair_round(a,(long double)integer(get(a,root+0x22c))/rate);
                if(integer(amount)>integer(missing))amount=missing;
                fee=repair_round(a,(long double)integer(amount)*rate);
                if(!game_level_group_repair_apply(a,descriptor,amount) || !spend(a,root,fee))return 0;
            }
            work_put(a,p,0x280,i,6);work_put(a,p,0x284,i,20);work_put(a,p,0x288,i,2);
        }else if(get(a,p+0x2b8)&(1u<<(i&31))){float minimum;
            /*The original final comparison is eax against itself: keep the
             *last flagged site, without inventing a nearest-site selection. */
            minimum=0x1.fffffep127f;selected=(int)i;put(a,p+0x340+index*4,site);
            for(j=0;j<4;j++){GameAddress owner=get(a,0x5f0210+j*4),player;
                if(!owner || !get(a,owner+20))continue;player=get(a,owner+0x3bc);if(!player)return 0;
                for(k=0;(int)k<short_at(a,site+36);k++)if(get(a,get(a,site+24)+k*4)){
                    GameAddress point=get(a,site+28)+k*12;float x=real(a,point),z=real(a,point+8),distance;
                    if(!(defensive_visibility(a,x,z)&(1u<<j)))continue;
                    distance=assault_distance(a,x,z,real(a,player+24),real(a,player+32));
                    if(isnan(distance) || distance<minimum)minimum=distance;
                }
            }
            if(integer(bits(minimum))>0x46400000 && primary && get(a,primary+0x4c)==3 &&
               !(integer(stamp)>0 && integer(stamp+1920)>integer(tick))){uint32_t result;
                for(j=0;j<3;j++)if(!primary_deploy(a,p,(int)j*682,682,0,&result))return 0;
            }
        }
    }
    put(a,p+0x340+index*4,selected<0?0:work_get(a,p,0x26c,(unsigned)selected));return phase_advance(a,p);
}
/*4bd5b0: bounded eight-slot production, existing per-site class caps and
 *reservation budget. Different classes intentionally share7ec temporary slots. */
static int defensive_produce(GameApplication *a,GameAddress p,unsigned base,uint32_t requested,uint8_t kind,const float point[3],float radius,int require_route,uint32_t *produced){
    unsigned index=get(a,p+0x1c4),site_index=base/8,j,lane;GameAddress site=get(a,p+0x340+index*4);
    uint32_t remaining=requested;int has_lane=kind==0x21 || kind==0x27 || kind==0x40;
    *produced=0;if(index>=14 || !site)return 0;lane=kind==0x21?2:kind==0x27?1:0;
    if(has_lane){GameAddress maximum=kind==0x21?0x6041b8:kind==0x27?0x6041c8:0x604148;
        uint32_t gap=get(a,maximum)-get(a,get(a,p+0x314)+site_index*12+lane*4);
        if(integer(gap)<integer(remaining))remaining=gap;
    }
    for(j=8;j>0;j--){GameAddress address=p+0x7ec+index*4+(j-1)*56,object;
        if(!work_get(a,p,0x284,get(a,site)) || integer(*produced)>=integer(remaining))break;
        if(get(a,address))continue;
        if(!game_level_imperial_produce(a,p,kind,point,require_route,radius,&object))return 0;put(a,address,object);
        if(object){(*produced)++;if(has_lane){GameAddress counter=get(a,p+0x314)+site_index*12+lane*4;put(a,counter,get(a,counter)+1);}}
    }
    return !a->memory.fault;
}
static void defensive_restart(GameApplication *a,GameAddress p,unsigned index){
    game_memory_write_u8(&a->memory,p+0x1a8+index,0);game_memory_write_u8(&a->memory,p+0x1b6+index,0);
}
/*4b67c0: produce defenders with the real site caps/cooldowns, then wait for
 *their command owner, publish group links and restore a descriptor identity.
 *The first argument of53bac0 is unused; reuse its existing native replacement. */
static int deploy_defenders(GameApplication *a,GameAddress p){
    static const uint8_t kinds[3]={0x40,0x27,0x21};
    static const unsigned enabled[3]={0x204,0x20c,0x208},timers[3]={0x25c,0x260,0x264};
    static const GameAddress amounts[3]={0x604170,0x604124,0x604180},periods[3]={0x604128,0x604190,0x60413c};
    unsigned index=get(a,p+0x1c4),phase,i,j,id,base;GameAddress site,root=get(a,0x58973c);uint32_t tick=get(a,0x5ac6ac);float point[3];
    if(index>=14)return 0;phase=game_memory_read_u8(&a->memory,p+0x1b6+index);if(phase>1)return !a->memory.fault;
    site=get(a,p+0x340+index*4);if(!site){defensive_restart(a,p,index);return !a->memory.fault;}id=get(a,site);base=id*8;
    if(phase==0){uint32_t total=0;int selected=-1;GameAddress primary;
        if(get(a,site+84)!=root){defensive_restart(a,p,index);return !a->memory.fault;}
        hostile_point(a,get(a,site+28),point);primary=get(a,get(a,site+24));
        if(!work_get(a,p,0x280,id) || !primary)defensive_cooldown(a,p,0x258,0x604144);
        else if(integer(get(a,p+0x258))<integer(tick)){
            for(j=0;(int)j<short_at(a,site+36);j++){GameAddress descriptor=get(a,get(a,site+24)+j*4);uint32_t result;
                if(!descriptor || short_at(a,descriptor+20)!=0)continue;
                if(get(a,descriptor+0x4c)){
                    if(!primary_sector(a,p,&result))return 0;
                    if(result==1)work_put(a,p,0x280,id,work_get(a,p,0x280,id)-1);
                }else defensive_cooldown(a,p,0x258,0x604144);
            }
        }
        if(!(get(a,p+0x2b8)&(1u<<(id&31))) || integer(work_get(a,p,0x310,id))>=integer(work_get(a,p,0x284,id))){defensive_restart(a,p,index);return !a->memory.fault;}
        /*Retail keeps the last qualifying human, not the closest one. */
        for(i=0;i<4;i++){GameAddress owner=get(a,0x5f0210+i*4),player;float distance;
            if(!owner || !get(a,owner+20) || !(player=get(a,owner+0x3bc)) || integer(get(a,player+88))<=0)continue;
            distance=assault_distance(a,real(a,player+24),real(a,player+32),point[0],point[2]);
            if((isnan(distance) || distance<real(a,0x57468c)) && integer(bits(distance))<0x7f7fffff)selected=(int)i;
        }
        if(selected>=0)hostile_point(a,get(a,get(a,0x5f0210+(unsigned)selected*4)+0x3bc)+24,point);
        for(i=0;i<8;i++)put(a,p+0x7ec+index*4+i*56,0);
        for(i=0;i<3;i++)if(get(a,root+enabled[i]) && integer(get(a,p+timers[i]))<integer(tick)){
            uint32_t count;if(!defensive_produce(a,p,base,get(a,amounts[i]),kinds[i],point,3500.0f,selected>=0,&count))return 0;
            total+=count;if(count)defensive_cooldown(a,p,timers[i],periods[i]);
        }
        if(!total){defensive_restart(a,p,index);return !a->memory.fault;}
        game_memory_write_u8(&a->memory,p+0x1b6+index,1);work_put(a,p,0x284,id,work_get(a,p,0x284,id)-1);return !a->memory.fault;
    }
    for(i=0;i<8;i++){
        GameAddress address=p+0x7ec+index*4+i*56,object=get(a,address),head,payload,descriptor;uint32_t ordinal;
        if(!object)continue;if(!get(a,object+0xdc))return !a->memory.fault;
        head=work_get(a,p,0x2c8,base+i);
        if((head && !pair_link(a,head,0x104,object)) || !pair_link(a,object,0x108,head))return 0;
        work_put(a,p,0x2c8,base+i,object);if(!pair_link(a,object,0x104,p))return 0;
        if(short_at(a,site+38)>0){
            if(!random_scaled(a,(float)short_at(a,site+38),&ordinal))return 0;
            for(j=0;(int)j<short_at(a,site+36);j++){descriptor=get(a,get(a,site+24)+j*4);
                if(descriptor && !ordinal--)break;}
            if((int)j>=short_at(a,site+36))return 0;descriptor=get(a,get(a,site+24)+j*4);
            if(!game_level_group_descriptor_payload(a,descriptor,&payload) || !payload || !game_level_scene_identity_replace(a,object,get(a,payload+20)))return 0;
        }
        put(a,address,0);
    }
    defensive_restart(a,p,index);return !a->memory.fault;
}
/*Every scheduled operation in574710's1bc..210 slots selects an ordinary
 *native program. Distinct downstream factories/orders retain their owners. */
int game_level_imperial_operation(GameApplication *a,GameAddress p,GameAddress method){
    if(!a || !p || a->memory.fault)return 0;
    if(method==0x4b5110)return recyclable_queued_transition(&a->memory,p,NULL);
    if(method==0x4b5330)return recyclable_queued_root_transition(&a->memory,p,NULL);
    if(method==0x4b5820)return produce_pair(a,p);
    if(method==0x4b59c0)return settle_pair(a,p);
    if(method==0x4ba6a0){put(a,p+0xe48,0);return phase_advance(a,p);}
    if(method==0x4bb2f0)return claim_site(a,p);
    if(method==0x4bb630)return select_rebuild_site(a,p);
    if(method==0x4bb700)return rebuild_site(a,p);
    if(method==0x4bbc40)return settle_rebuild_site(a,p);
    if(method==0x4b90d0)return select_strike_site(a,p);
    if(method==0x4b93a0)return advance_strike(a,p);
    if(method==0x4b9880)return assault(a,p);
    if(method==0x4b6e00)return choose_hostile_descriptor(a,p);
    if(method==0x4b7010)return choose_secondary_descriptor(a,p);
    if(method==0x4bb0f0)return repair_descriptors(a,p);
    if(method==0x4ba6d0)return descriptor_economy(a,p);
    if(method==0x4bb070)return economy_retire_inactive(a,p);
    if(method==0x4b70f0)return plan_hostile_site(a,p);
    if(method==0x4b7670)return size_hostile_assault(a,p);
    if(method==0x4b7ab0)return execute_hostile_assault(a,p);
    if(method==0x4b67c0)return deploy_defenders(a,p);
    if(method==0x4b6120)return maintain_formations(a,p);
    return 0; /*Unknown IDs are invalid; every actual scheduled operation is native. */
}
