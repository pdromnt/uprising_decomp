#include "level_factory.h"
#include "level_objects.h"
#include "level_run.h"
#include "level_player_resources.h"
#include "level_player_slots.h"
#include "level_effect_runtime.h"
#include "gameplay_state.h"
#include "gameplay_catalog.h"
#include "gameplay_geometry.h"
#include "gameplay_leaves.h"
#include "game_intrusive_list.h"
#include "game_state_ops.h"
#include "state_helper_plans.h"
#include "vehicle_presentation.h"
#include "effect_resource_pool.h"
#include "presentation_collision.h"
#include "presentation_state_copy.h"
#include "gameplay_root.h"
#include "vehicle_targeting.h"
#include "scene_orders.h"
#include "entity_queries.h"
#include "menu_sound.h"
#include "gameplay_messages.h"
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>

/*Descriptor storage remains owned by the citadel/descriptor pool. Payloads
 *and descending carriers remain owned by their existing factories. These
 *remaining class programs are mandatory native dependencies, never fallback. */
extern int game_level_articulated_descriptor_place(GameApplication *,GameAddress,uint32_t,const float[3],uint16_t,GameAddress,int);
extern int game_level_articulated_descriptor_retire(GameApplication *,GameAddress);
extern int game_level_descriptor_transfer(GameApplication *,GameAddress,uint32_t,GameAddress);

extern int game_level_vehicle_death_detach(GameApplication *,GameAddress,GameAddress,GameAddress);
/*47f2d0/479520 distribute health fractions across the actual descriptor pages.
 *53f310/51ac70 returns unused production time; the payload owns its timer. */
int game_level_catalog_return_health(GameApplication *a,GameAddress owner,uint8_t kind,float fraction){
    GameAddress page,record,table,payload;unsigned visits=0;uint32_t bits,method;
    method=game_memory_read_u32(&a->memory,game_memory_read_u32(&a->memory,owner)+0x40);
    if(method==0x477780)return !a->memory.fault; /*Base AI's verified RET8; no descriptor refund. */
    if(method!=0x47f2d0 && method!=0x479520)return 0;
    for(page=game_memory_read_u32(&a->memory,owner+0x294);page;page=game_memory_read_u32(&a->memory,page+0x64)){
        int16_t count;unsigned i;uint16_t word=game_memory_read_u16(&a->memory,page+0x24);memcpy(&count,&word,2);
        if(++visits>65536 || a->memory.fault)return 0;
        for(i=0;count>0 && i<(unsigned)count;i++){
            uint8_t selected;long double unused;
            record=game_memory_read_u32(&a->memory,game_memory_read_u32(&a->memory,page+0x18)+i*4);
            if(!record)continue;
            table=game_memory_read_u32(&a->memory,record);method=game_memory_read_u32(&a->memory,table+0x44);
            if(method==0x53d8d0)selected=0;
            else if(method==0x53ff60)selected=11;
            else if(method==0x53f2c0){
                payload=game_memory_read_u32(&a->memory,record+0x48);
                switch(game_memory_read_u32(&a->memory,game_memory_read_u32(&a->memory,payload)+0x90)){
                case 0x51ba30:selected=0x40;break;case 0x51bb10:selected=0x21;break;
                case 0x51bba0:selected=0x27;break;case 0x51bc30:selected=0x26;break;default:return 0;
                }
            }else return 0;
            if(selected!=kind)continue;
            method=game_memory_read_u32(&a->memory,table+0x4c);
            if(method==0x53d8f0 || method==0x53f3d0){
                bits=game_memory_read_u32(&a->memory,0x576c2c);memcpy(&fraction,&bits,4);unused=fraction;
            }else if(method==0x53f310){
                uint32_t duration,start,tick,remaining,amount;int32_t signed_duration,signed_start,signed_remaining,signed_amount;float rounded_duration;
                long double product;
                payload=game_memory_read_u32(&a->memory,record+0x48);
                duration=game_memory_read_u32(&a->memory,payload+0xa8);start=game_memory_read_u32(&a->memory,payload+0xac);
                tick=game_memory_read_u32(&a->memory,0x5ac6ac);remaining=start-tick+duration;
                memcpy(&signed_remaining,&remaining,4);unused=fraction;
                if(signed_remaining>0){
                    memcpy(&signed_duration,&duration,4);rounded_duration=(float)signed_duration;
                    product=(long double)signed_duration*fraction;
                    amount=(uint32_t)(uint64_t)(!isfinite(product) || product< -0x1p63L || product>=0x1p63L?INT64_MIN:(int64_t)product);
                    memcpy(&signed_amount,&amount,4);
                    if(signed_amount>signed_remaining){
                        start-=remaining;amount-=remaining;memcpy(&signed_amount,&amount,4);
                        unused=(long double)signed_amount/rounded_duration;
                    }else{start-=amount;bits=game_memory_read_u32(&a->memory,0x575634);memcpy(&fraction,&bits,4);unused=fraction;}
                    memcpy(&signed_start,&start,4);if(signed_start<=0)start=0;
                    game_memory_write_u32(&a->memory,payload+0xac,start);
                }
                fraction=(float)unused;
            }else return 0;
            bits=game_memory_read_u32(&a->memory,0x573e2c);{float zero;memcpy(&zero,&bits,4);if(isnan(unused) || unused==zero)return !a->memory.fault;}
        }
    }
    return !a->memory.fault;
}
extern int game_level_scene_killed(GameApplication *,GameAddress,GameAddress,GameAddress);
extern int game_level_scene_update(GameApplication *,GameAddress,GameAddress);
extern int game_level_scene_transient_reset(GameApplication *,GameAddress,GameAddress);
extern int game_level_contact_response(GameApplication *,GameAddress,GameAddress,GameAddress,const uint32_t[3],const float[3]);
extern int game_level_catalog_refresh_selection(void *,GameAddress);
extern int game_level_group_member_disable(GameApplication *,GameAddress);
extern int game_level_root_reserve_produce(GameApplication *,GameAddress,uint8_t,uint32_t,uint32_t,float,float,uint32_t *);
extern int game_level_spatial_sample_produce(GameApplication *,GameAddress,uint32_t,uint32_t,uint32_t *);
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
static float real(GameApplication *a,GameAddress p){uint32_t b=get(a,p);float f;memcpy(&f,&b,4);return f;}
static int small_prepare(void *c,GameAddress object,float p[3],uint16_t heading){
    GameApplication *a=c;
    GameAddress method=get(a,get(a,object)+0x68);
    return (method==0x42ed30 || method==0x406ea0) && game_level_object_footprint(a,object,p,heading);
}
static int small_place(void *c,GameAddress object,uint32_t id,const float p[3],const uint16_t angles[3]){
    return game_level_scene_object_place(c,object,id,p,angles);
}
int game_level_secondary_descriptor_place(GameApplication *a,GameAddress object,unsigned f,uint32_t id,
        const float p[3],uint16_t heading,GameAddress descriptor,int restored){
    uint16_t angles[3]={heading,0,0};uint32_t interval,tick;GameAddress owner,method;
    GamePresentationBindingServices services={a,small_prepare,small_place};
    if(f==2)return game_level_articulated_descriptor_place(a,object,id,p,heading,descriptor,restored);
    method=get(a,get(a,object)+0x80);
    if(f!=1 || (method!=0x51ab30 && method!=0x4fa670) || a->memory.fault)return 0;
    game_memory_write_u8(&a->memory,object+0xa4,0);
    if(!game_presentation_bind(&a->memory,object,id,p,angles,restored,&services))return 0;
    put(a,object+0xa0,descriptor);
    if(method==0x4fa670)return !a->memory.fault;
    owner=get(a,0x5f0210+(get(a,object+20)&63)*4);
    if(!game_level_player_catalog_value(a,owner,game_memory_read_u8(&a->memory,object+12),5,0,&interval))return 0;
    put(a,object+0xa8,interval);tick=get(a,0x5ac6ac);
    put(a,object+0xac,get(a,0x624f8c)==10?tick+(restored?0:500000):0);
    return !a->memory.fault;
}
static int small_token(void *c,GameAddress object,uint32_t *token){GameApplication *a=c;
    switch(get(a,get(a,object)+0x90)){
    case 0x51ba30:*token=0x40;break;case 0x51bb10:*token=0x21;break;
    case 0x51bba0:*token=0x27;break;case 0x51bc30:*token=0x26;break;default:return 0;
    }return !a->memory.fault;
}
int game_level_secondary_descriptor_retire(GameApplication *a,GameAddress object,unsigned f){
    GameAddress method;
    if(f==2)return game_level_articulated_descriptor_retire(a,object);
    method=get(a,get(a,object)+0x84);
    if(f!=1 || a->memory.fault || (method!=0x4fa650 && method!=0x51ad90))return 0;
    return game_level_station_payload_reset(a,object,0);
}
int game_level_secondary_descriptor_relocate(GameApplication *a,GameAddress object,GameAddress point,uint16_t heading){
    GameAddress method=get(a,get(a,object)+0x68);float p[3];uint32_t bits;unsigned i;
    if(a->memory.fault)return 0;
    if(method==0x42ec30)return 1; /*Base footprint is empty. */
    if(method!=0x42ed30 && method!=0x406ea0)return 0;
    /*53ed86 adjusts the borrowed point through68h. Calling9ch placement
     *here inserts this still-settling payload twice and cycles its spatial list. */
    for(i=0;i<3;i++){bits=get(a,point+i*4);memcpy(p+i,&bits,4);}
    if(a->memory.fault || !game_level_object_footprint(a,object,p,heading))return 0;
    memcpy(&bits,p+1,4);put(a,point+4,bits);return !a->memory.fault;
}
int game_level_secondary_descriptor_settled(GameApplication *a,GameAddress record){
    GameAddress method=get(a,get(a,record)+0x54),payload=get(a,record+0x48);
    if(a->memory.fault)return 0;
    if(method==0x53d910)return 1; /*Original base method is a plain return. */
    if(method==0x53f320)put(a,payload+0xac,get(a,0x5ac6ac));
    else if(method==0x53f420)put(a,payload+0xc4,0);
    else return 0;
    return !a->memory.fault;
}
static int family(GameApplication *a,GameAddress record,unsigned *out){
    switch(get(a,record)){
    case 0x576c68:*out=0;break;
    case 0x576d38:*out=1;break;
    case 0x576da0:case 0x576e08:*out=2;break;
    default:return 0;
    }
    return !a->memory.fault;
}
static int catalog(void *c,GameAddress owner,uint8_t kind,unsigned item,uint32_t alternate,uint32_t *value){
    return game_level_player_catalog_value(c,owner,kind,item,alternate,value);
}
static int create(void *c,uint8_t kind,uint16_t variant,GameAddress *out){*out=game_level_factory_take(c,kind,variant);return *out!=0;}
static int sequence(void *c,GameAddress owner,uint32_t *out){GameApplication *a=c;uint32_t next;
    if(level_player_slots_owns_player(owner)){
        if(!level_player_slots_next_owner_sequence(&a->memory,owner,out,&next))return 0;
        put(a,owner+24,next);
    }else *out=game_state_next_owner_sequence(&a->memory,owner);
    return !a->memory.fault;
}
static int publish(void *c,GameAddress owner,GameAddress object){GameApplication *a=c;
    uint32_t head=state_helper_flagged_member_offset(get(a,object+16));
    if(level_player_slots_owns_player(owner) && !level_player_slots_attach_scene_member(owner,object,head))return 0;
    game_list_insert_after(&a->memory,object,owner+head,0x48,0x4c);
    if(a->memory.fault){level_player_slots_forget_scene_member(object);return 0;}return 1;
}
static int attach(void *c,GameAddress object,uint32_t id,const float p[3],uint16_t heading){
    uint16_t angles[3]={heading,0,0};return game_level_object_place_angles(c,object,id,p,angles);
}
static int bind(void *c,GameAddress object,unsigned f,uint32_t id,const float p[3],uint16_t heading,GameAddress descriptor,int restored){
    GameApplication *a=c;uint16_t angles[3]={heading,0,0};int ok;
    if(f)ok=game_level_secondary_descriptor_place(a,object,f,id,p,heading,descriptor,restored);
    else ok=game_level_vehicle_place(a,object,id,p,angles,descriptor,get(a,get(a,object)+0x1fc));
    if(!ok)fprintf(stderr,"native descriptor bind failed: family=%u kind=%02x table=%08x restored=%d fault=%d\n",
        f,game_memory_read_u8(&a->memory,object+12),(unsigned)get(a,object),restored,a->memory.fault);
    return ok;
}
static int duration(void *c,GameAddress object,int32_t *out){GameApplication *a=c;uint32_t value;
    if(!game_level_scene_property(a,object,get(a,get(a,object)+0x44),&value))return 0;
    memcpy(out,&value,4);return 1;
}
/*Descriptor repair quotes borrow the live payload and its catalog definition;
 *the payload remains the only health owner.53de00 prices the primary as23. */
int game_level_group_repair_quote(GameApplication *a,GameAddress descriptor,GameAddress owner,uint32_t *price,uint32_t *health,uint32_t *maximum){
    GameAddress table=get(a,descriptor),payload=get(a,descriptor+0x48);uint8_t kind;
    uint32_t p=get(a,table+0x18),h=get(a,table+0x20),m=get(a,table+0x24);
    if(p==0x53de00)kind=0x23;
    else if(p==0x53eca0 || p==0x53f760)kind=game_memory_read_u8(&a->memory,payload+12);
    else return 0;
    if((h!=0x53e4b0 && h!=0x53f1a0 && h!=0x53fc70) || (m!=0x53e4c0 && m!=0x53f1b0 && m!=0x53fc80))return 0;
    *health=get(a,payload+0x58);
    return game_level_player_catalog_value(a,owner,kind,8,0,price) &&
        game_level_scene_property(a,payload,get(a,get(a,payload)+0x44),maximum);
}
int game_level_group_repair_apply(GameApplication *a,GameAddress descriptor,uint32_t amount){
    GameAddress method=get(a,get(a,descriptor)+0x2c),payload=get(a,descriptor+0x48);
    if(method==0x53d8c0)put(a,descriptor+0x18,get(a,descriptor+0x18)+amount);
    else if(method==0x53e490 || method==0x53ecc0 || method==0x53f780)put(a,payload+0x58,get(a,payload+0x58)+amount);
    else return 0;
    return !a->memory.fault;
}
/*Primary/secondary descriptor10h callbacks borrow the same live payload.
 *A zero identity makes it unavailable; no duplicate presentation state exists. */
int game_level_group_descriptor_payload(GameApplication *a,GameAddress descriptor,GameAddress *out){
    GameAddress payload=get(a,descriptor+0x48),method=get(a,get(a,descriptor)+0x10);
    if(!out || (method!=0x53e470 && method!=0x53f170 && method!=0x53fc40))return 0;
    *out=payload && get(a,payload+20)?payload:0;return !a->memory.fault;
}
static int token(void *c,GameAddress object,uint32_t *out){GameApplication *a=c;
    if(get(a,get(a,object)+0x60)!=0x53c550)return 0;
    *out=(game_memory_read_u8(&a->memory,object+0xd8)&4)!=0;return !a->memory.fault;
}
static int stop(void *c,GameAddress object){GameApplication *a=c;return game_level_scene_killed(a,object,0,get(a,get(a,object)+0x10));}
static int retire(void *c,GameAddress object,unsigned f,int auxiliary){GameApplication *a=c;
    if(auxiliary){GameAddress method=get(a,get(a,object)+0x20);
        return game_level_scene_reset(a,object,method);
    }
    if(f)return game_level_secondary_descriptor_retire(a,object,f);
    if(get(a,get(a,object)+0x200)!=0x552040)return 0;
    return game_level_vehicle_descriptor_retire(a,object);
}
static int transition(void *c,GameAddress effect,uint32_t id,GameAddress payload){return game_level_descriptor_transfer(c,effect,id,payload);}
static int notify(void *c,GameAddress owner){return game_level_catalog_refresh_selection(c,owner);}
/*47e0a0 is the unscaled refund query at24h, not the unit-price query at28h.
 *Kind11 uses the spatial unit's item12 and its selected-variant sign gate. */
int game_level_descriptor_unit_refund(GameApplication *a,GameAddress owner,uint8_t kind,int32_t *out){
    uint32_t value;GameAddress table=get(a,owner);
    if(!out || a->memory.fault)return 0;
    if(get(a,table+0x24)==0x479450){
        if(kind==11)value=0;
        else if(!game_level_player_catalog_value(a,owner,kind,8,0,&value))return 0;
        memcpy(out,&value,4);return !a->memory.fault;
    }
    if(get(a,table+0x24)!=0x47e0a0)return 0;
    if(kind==11 && (get(a,owner+0x2d4)&0x80000000u))value=0;
    else if(!game_level_player_catalog_value(a,owner,kind==11?0x89:kind,kind==11?12:8,0,&value))return 0;
    memcpy(out,&value,4);return !a->memory.fault;
}
static int unit_value(void *c,GameAddress owner,uint8_t kind,int32_t *out){return game_level_descriptor_unit_refund(c,owner,kind,out);}
static int credit(void *c,GameAddress owner,int32_t amount){GameApplication *a=c;
    GameAddress method=get(a,get(a,owner)+0x38);
    if(method==0x47f290 || method==0x4794b0 || method==0x4794e0){put(a,owner+0x22c,get(a,owner+0x22c)+(uint32_t)amount);put(a,owner+0x150,get(a,owner+0x150)+(uint32_t)amount);return !a->memory.fault;}
    if(get(a,get(a,owner)+0x38)!=0x47f250)return 0;
    return gameplay_catalog_credit(&a->memory,owner,(uint32_t)amount);
}
static GameplayDescriptorServices lifecycle(GameApplication *a){GameplayDescriptorServices s={a,catalog,create,sequence,publish,attach,bind,duration,token,stop,retire,transition,notify,unit_value,credit};return s;}
static uint32_t production_integer(long double v){
    return (uint32_t)(uint64_t)(!isfinite(v)||v< -0x1p63L||v>=0x1p63L?INT64_MIN:(int64_t)v);
}
static float production_cell(GameApplication *a,float value){
    uint32_t bits=production_integer(value)&0xfffffe00u;int32_t cell;memcpy(&cell,&bits,4);
    return (float)((long double)cell+real(a,0x576c5c));
}
static int production_mask(GameApplication *a,const float p[3]){
    uint8_t x=(uint8_t)(production_integer(p[0])>>9),z=(uint8_t)(production_integer(p[2])>>9);
    return (game_memory_read_u8(&a->memory,0x610520+(uint32_t)z*32+x/8)&(1u<<(x&7)))!=0;
}
static int production_unused(GameApplication *a,GameAddress unit){
    GameAddress head=get(a,unit+0x50);if(!head)return 0;
    game_list_insert_after(&a->memory,unit,head,4,8);return !a->memory.fault;
}
/*Both producer paths retain their existing cluster/factory/packet owners. */
extern int game_level_ground_controller_worker(GameApplication *,GameAddress,GameAddress,int32_t *);
static int ground_base_update(void *c,GameAddress object){int ok=game_level_station_payload_update(c,object,0x482690);
    if(!ok)fprintf(stderr,"ground producer base failed: object=%08x\n",object);
    return ok;}
static int ground_controller_refresh(void *c,GameAddress controller){GameApplication *a=c;
    int ok=game_level_scene_update(a,controller,get(a,get(a,controller)+0x98));
    if(!ok)fprintf(stderr,"ground producer controller refresh failed: controller=%08x\n",controller);
    return ok;}
static int ground_worker(void *c,GameAddress controller,GameAddress producer,int32_t *result){
    int ok=game_level_ground_controller_worker(c,controller,producer,result);
    if(!ok){GameApplication *a=c;
        fprintf(stderr,"ground producer worker failed: object=%08x controller=%08x target=%08x lanes=%u cap=%u budget=%u/%u/%u\n",
            producer,controller,get(a,controller+0xf0),get(a,controller+0x184),get(a,controller+0x188),
            get(a,controller+0x178),get(a,controller+0x17c),get(a,controller+0x180));}
    return ok;}
static GameAddress ground_acquire(void *c,GameAddress producer){GameApplication *a=c;
    GameAddress method=get(a,get(a,producer)+0x80),root=get(a,0x58973c);uint8_t kind;uint32_t variant;
    switch(method){case 0x533cd0:kind=0x40;break;case 0x534580:kind=0x27;break;
        case 0x534780:kind=0x21;break;case 0x533f20:kind=0x26;break;default:return 0;}
    if(!game_level_player_catalog_value(a,root,kind,3,0,&variant) || variant>UINT16_MAX){
        fprintf(stderr,"ground producer catalog failed: object=%08x root=%08x kind=%02x\n",producer,root,(unsigned)kind);return 0;}
    {GameAddress unit=game_level_factory_take(a,kind,(uint16_t)variant);
     if(!unit)fprintf(stderr,"ground producer acquire failed: object=%08x kind=%02x variant=%u\n",producer,(unsigned)kind,variant);
     return unit;}
}
static int ground_clear(void *c,GameAddress unit,const float p[3],int *clear){
    int ok=gameplay_geometry_position_clear(&((GameApplication *)c)->memory,unit,p,clear);
    if(!ok)fprintf(stderr,"ground producer placement query failed: unit=%08x\n",unit);
    return ok;}
static int ground_deploy(void *c,GameAddress root,GameAddress unit,const float p[3]){uint32_t id;
    int ok=sequence(c,root,&id) && attach(c,unit,id,p,0) && publish(c,root,unit);
    if(!ok)fprintf(stderr,"ground producer deploy failed: root=%08x unit=%08x\n",root,unit);
    return ok;}
static int ground_recycle(void *c,GameAddress unit){GameApplication *a=c;
    return get(a,unit+0x58)?retire(a,unit,0,1):production_unused(a,unit);}
static int ground_aircraft_deploy(void *c,GameAddress root,GameAddress unit,const float p[3],const float destination[2]){
    GameApplication *a=c;uint32_t id;const uint16_t angles[3]={0};
    return get(a,get(a,unit)+0x1bc)==0x55ab20 && sequence(a,root,&id) &&
        game_level_aircraft_activate(a,unit,id,p,angles,destination,0) && publish(a,root,unit);
}
int game_level_ground_producer_update(GameApplication *a,GameAddress object,GameAddress method){
    GameplayGroundProducerServices s={a,ground_base_update,ground_controller_refresh,ground_worker,
        ground_acquire,ground_clear,ground_deploy,ground_recycle,ground_aircraft_deploy};
    if(method==0x533f40)return gameplay_ground_aircraft_update(&a->memory,object,&s);
    return gameplay_ground_producer_update(&a->memory,object,method,&s);
}
extern int game_level_scene_identity_replace(GameApplication *,GameAddress,uint32_t);
extern int game_level_scene_order_issue(GameApplication *,GameAddress,const PresentationCommand *,int);
static int ground_mask(GameApplication *a,int x,int z){
    return x<0 || x>=256 || z<0 || z>=256 || (game_memory_read_u8(&a->memory,0x610520u+(unsigned)z*32u+(unsigned)x/8u)&(1u<<(x&7)));
}
static int32_t ground_signed(uint32_t word){int32_t value;memcpy(&value,&word,4);return value;}
static int ground_accessible(GameApplication *a,const float p[3]){
    int32_t ix=ground_signed(production_integer(p[0])),iz=ground_signed(production_integer(p[2]));
    int x=(uint8_t)(ix/512-(ix%512<0)),z=(uint8_t)(iz/512-(iz%512<0));
    return !ground_mask(a,x,z) && (!ground_mask(a,x-1,z) || !ground_mask(a,x+1,z) || !ground_mask(a,x,z-1) || !ground_mask(a,x,z+1));
}
/*412380: one shared RNG draw, signed spread/heading and binary32 endpoint stores. */
static int ground_ray(GameApplication *a,unsigned spread,uint16_t heading,float radius,const float from[3],float out[3]){
    uint32_t offset=effect_initialization_random(&a->memory,(float)spread);
    unsigned angle=(heading+(uint16_t)offset-spread/2u)&2047u;
    out[2]=(float)((long double)real(a,0x6298a8u+((angle+512u)&2047u)*4u)*radius+from[2]);
    out[0]=(float)((long double)real(a,0x6298a8u+angle*4u)*radius+from[0]);return !a->memory.fault;
}
static int ground_peer(GameApplication *a,GameAddress object,unsigned slot,GameAddress argument){
    return gameplay_state_peer_method(&a->memory,object,argument,get(a,get(a,object)+slot),NULL);
}
static int ground_link_worker(GameApplication *a,GameAddress controller,unsigned lane,GameAddress unit){
    GameAddress head=get(a,controller+0x13cu+lane*4u);
    if(head && (!ground_peer(a,head,0x104,unit) || !ground_peer(a,unit,0x108,head)))return 0;
    if(!ground_peer(a,unit,0x104,controller))return 0;
    put(a,controller+0x13cu+lane*4u,unit);put(a,controller+0x150u+lane*4u,get(a,controller+0x150u+lane*4u)+1u);
    return !a->memory.fault;
}
static int ground_target_command(GameApplication *a,GameAddress unit,GameAddress producer){
    PresentationCommand command={0};command.kind=11;command.target=producer;command.has_target=1;
    command.target_identity=get(a,producer+20);return !a->memory.fault && game_level_scene_order_issue(a,unit,&command,1);
}
/*4589b0: the three actual producer classes share candidate search but have
 *distinct arrival/outlet, command and effect consumers. No guest stack state. */
int game_level_ground_controller_deploy(GameApplication *a,GameAddress controller,unsigned lane,uint8_t kind,GameAddress producer,float target[3],int *accepted){
    GameAddress unit=0,root=get(a,0x58973c),arrival=0,halo=0;float origin[3],outlet[3],margin,height;uint16_t heading,yaw,angles[3]={0};
    unsigned i,total_trials=0;int tries=25,clear=0,allowed=0;uint32_t id;uint8_t actual;
    const char *stage="placement";
    if(!accepted || lane>=5 || a->memory.fault)return 0;*accepted=0;
    unit=ground_acquire(a,producer);if(!unit)return 0;actual=game_memory_read_u8(&a->memory,unit+12);
    if(actual!=0x40 && actual!=0x21 && actual!=0x27)goto failed;
    for(i=0;i<3;i++)origin[i]=real(a,producer+24+i*4);
    heading=vehicle_targeting_vector_angle(&a->memory,(float)((long double)target[0]-origin[0]),
        (float)((long double)target[2]-origin[2]),real(a,0x571f34));
    margin=real(a,actual==0x40?0x573570:0x573574);
    height=(float)gameplay_geometry_terrain_height(&a->memory,origin[0],origin[2]);
    yaw=game_memory_read_u16(&a->memory,producer+0x24);
    if(actual==0x40){int16_t signed_heading,signed_yaw;int difference;
        memcpy(&signed_heading,&heading,2);memcpy(&signed_yaw,&yaw,2);difference=(int)signed_heading-signed_yaw;
        if(difference<0)difference=-difference;if(difference>512)yaw=(yaw+1024u)&2047u;heading=yaw;
    }
    for(;;){
        float radius=(float)((long double)real(a,producer+0x38)+margin);
        if(!ground_ray(a,actual==0x40?170u:512u,heading,radius,actual==0x40?origin:(float[3]){real(a,controller+24),real(a,controller+28),real(a,controller+32)},target))goto failed;
        target[0]=(float)((long double)ground_signed(production_integer(target[0])&0xfffffe00u)+real(a,0x573578));
        target[2]=(float)((long double)ground_signed(production_integer(target[2])&0xfffffe00u)+real(a,0x573578));
        target[1]=(float)(gameplay_geometry_terrain_height(&a->memory,target[0],target[2])+real(a,unit+0x30));
        if(!gameplay_geometry_position_clear(&a->memory,unit,target,&clear))goto failed;
        if(kind==0x27){target[1]=(float)((long double)target[1]+real(a,0x573574));allowed=1;}
        else{
            long double drop=(long double)height-target[1];allowed=ground_accessible(a,target) && (drop<real(a,0x573574) || isnan(drop));
            if(allowed && clear)allowed=scene_order_segment_clear(&a->memory,target,origin);
        }
        if(a->memory.fault)goto failed;
        if(++total_trials>1024){
            /* Exhausting this bounded outlet search means no production this
             * tick, just like the original tries<1 path below. */
            return ground_recycle(a,unit) && !a->memory.fault;
        }
        tries--;
        {uint32_t margin_bits;memcpy(&margin_bits,&margin,4);
            if(actual==0x40 && tries<1 && margin_bits>0x43fa0000u && margin_bits<0x80000000u){
                margin=(float)((long double)margin-real(a,unit+0x38));tries=25;
            }
        }
        if(!(tries>0 && (!clear || !allowed)))break;
    }
    if(tries<1){if(!ground_recycle(a,unit))return 0;return !a->memory.fault;}
    stage="arrival";
    if(actual==0x40){
        float position[3]={origin[0],(float)(gameplay_geometry_terrain_height(&a->memory,origin[0],origin[2])+real(a,unit+0x2c)),origin[2]};
        arrival=game_level_factory_take(a,0x45,0);
        if(!arrival || !sequence(a,root,&id) || !game_level_arrival_produce(a,arrival,id,position,unit) || !publish(a,root,arrival))goto failed;
    }else{
        unsigned angle=actual==0x27?(yaw-359u)&2047u:yaw&2047u;
        float distance=real(a,actual==0x27?0x573588:0x573590);
        outlet[0]=(float)((long double)real(a,0x6298a8u+angle*4u)*distance+origin[0]);
        outlet[2]=(float)((long double)real(a,0x6298a8u+((angle+512u)&2047u)*4u)*distance+origin[2]);
        outlet[1]=actual==0x27?(float)((long double)origin[1]-real(a,producer+0x30)):
            (float)(gameplay_geometry_terrain_height(&a->memory,outlet[0],outlet[2])+real(a,unit+0x30));
        if(!gameplay_geometry_position_clear_except(&a->memory,unit,outlet,producer,&clear))goto failed;
        if(!clear || !ground_accessible(a,outlet)){
            if(!ground_recycle(a,unit))return 0;*accepted=actual==0x27;return !a->memory.fault;
        }
        outlet[1]=(float)((long double)real(a,unit+(actual==0x27?0x2c:0x30))*real(a,0x57358c)+outlet[1]);
        if(!gameplay_geometry_position_clear_except(&a->memory,unit,outlet,producer,&clear))goto failed;
        if(!clear){if(!ground_recycle(a,unit))return 0;return !a->memory.fault;}
        angles[0]=actual==0x27?(yaw+512u)&2047u:yaw;angles[1]=actual==0x27?1536u:0;
        if(!sequence(a,root,&id) || !game_level_object_place_angles(a,unit,id,outlet,angles) || !publish(a,root,unit))goto failed;
    }
    stage="identity";
    if(!game_level_scene_identity_replace(a,unit,get(a,producer+20)))goto failed;
    stage="point order";
    if(actual==0x40 || actual==0x21){
        if(!game_level_scene_point_order(a,unit,0,actual==0x40?11:0,0,target))goto failed;
    }
    stage="target order";
    if(actual!=0x40 && !ground_target_command(a,unit,producer))goto failed;
    stage="worker link";
    if(!ground_link_worker(a,controller,lane,unit))goto failed;
    stage="halo";
    if(actual==0x40){EffectInitialization request={0};float scale=(float)((long double)real(a,unit+0x38)*real(a,0x573580));
        halo=game_level_factory_take(a,0x41,0);if(!halo)goto failed;
        request.profile=effect_initialization_profile(&a->memory,5);request.subtype=game_memory_read_u8(&a->memory,controller+12);
        request.scale[0]=request.scale[1]=scale;request.radius=real(a,unit+0x38);request.mark_kind=-1;
        memcpy(request.position,origin,12);if(!game_level_effect_emit(a,root,halo,&request))goto failed;
    }
    *accepted=1;return !a->memory.fault;
failed:
    fprintf(stderr,"ground worker deployment failed: stage=%s producer=%08x unit=%08x kind=%02x lane=%u trials=%u fault=%d\n",
        stage,producer,unit,(unsigned)actual,lane,total_trials,a->memory.fault);
    if(halo)(void)game_level_factory_destroy(a,halo);
    if(arrival)(void)game_level_factory_destroy(a,arrival);
    if(unit)(void)ground_recycle(a,unit);return 0;
}
/*53f2d0 dispatches the actual payload producer:51af90 retains the requested
 *infantry count and its tighter placement spread;51abc0 produces one tank/AAV;
 *51b7b0 launches a bomber from this station toward unsigned world coordinates.
 *All share the existing factory, presentation and membership owners. */
int game_level_station_produce(GameApplication *a,GameAddress descriptor,uint32_t x,uint32_t z,uint32_t *reason){
    GameAddress method=get(a,get(a,descriptor)+0x48),payload=get(a,descriptor+0x48);
    GameAddress root=get(a,0x58973c);uint32_t token,operation;
    if(method==0x53f3e0)return game_level_spatial_sample_produce(a,payload,x,z,reason);
    operation=get(a,get(a,payload)+0x8c);
    if(method!=0x53f2d0 || (operation!=0x51abc0 && operation!=0x51af90 && operation!=0x51b7b0)){
        fprintf(stderr,"native station production method unsupported: descriptor=%08x method=%08x payload=%08x call=%08x\n",descriptor,method,payload,operation);return 0;}
    if(!(game_memory_read_u8(&a->memory,payload+0xa4)&2) ||
       (operation!=0x51b7b0 && !get(a,payload+0xa0))){*reason=1;return !a->memory.fault;}
    if(operation==0x51b7b0){
        GameAddress unit;uint32_t variant;float origin[3],destination[2]={(float)x,(float)z};unsigned i;
        if(get(a,get(a,root)+0x60)!=0x47f420 ||
           !game_level_player_catalog_value(a,root,0x26,3,0,&variant) || variant>UINT16_MAX)return 0;
        for(i=0;i<3;i++)origin[i]=real(a,payload+24+i*4);
        unit=game_level_factory_take(a,0x26,(uint16_t)variant);if(!unit)return 0;
        if(!ground_aircraft_deploy(a,root,unit,origin,destination)){
            (void)game_level_factory_destroy(a,unit);return 0;
        }
        put(a,root+0x4d8,get(a,root+0x4d8)-1);*reason=0;
    }else{
        uint32_t count=1,spread=0xc7;float step=1000;
        if(operation==0x51af90){token=0x40;count=x;spread=0x80;step=500;}
        else if(!small_token(a,payload,&token))return 0;
        if(!game_level_root_reserve_produce(a,root,(uint8_t)token,count,spread,6000,step,reason)){
            fprintf(stderr,"native station reserve production failed: payload=%08x token=%02x fault=%d\n",payload,token,a->memory.fault);return 0;}
    }
    if(!*reason){
        game_memory_write_u8(&a->memory,payload+0xa4,game_memory_read_u8(&a->memory,payload+0xa4)&0xfdu);
        put(a,payload+0xac,get(a,0x5ac6ac));
    }
    return !a->memory.fault;
}
/*53e4d0: deploy through the descriptor's actual station identity and arrival
 *owner. The shared spatial grid, route mask and RNG remain authoritative. */
int game_level_group_produce(GameApplication *a,GameAddress descriptor,uint8_t kind,uint32_t x,uint32_t z,uint32_t *reason){
    GameAddress group=get(a,descriptor+4),owner=get(a,group+0x54),unit,arrival;
    uint32_t variant,cursor,id;float p[3],center[2],radius;unsigned trials=0;int clear;
    if(!reason || !game_level_player_catalog_value(a,owner,kind,3,0,&variant) || variant>UINT16_MAX)return 0;
    unit=game_level_factory_take(a,kind,(uint16_t)variant);if(!unit)return 0;
    /*The original advances the RNG once before its first collision query. */
    cursor=get(a,0x6308c0);if(cursor==1024)cursor=0;
    if(cursor>=1024)goto failure;put(a,0x6308c0,cursor+1);
    center[0]=(float)x;center[1]=(float)z;
    p[0]=production_cell(a,center[0]);p[2]=production_cell(a,center[1]);
    p[1]=(float)(gameplay_geometry_terrain_height(&a->memory,p[0],p[2])+real(a,unit+0x30));
    if(!gameplay_geometry_position_clear(&a->memory,unit,p,&clear))goto failure;
    radius=(float)((long double)real(a,unit+0x38)*real(a,0x576c60));
    while(!clear || production_mask(a,p)){
        uint32_t bx,bz;memcpy(&bx,&center[0],4);memcpy(&bz,&center[1],4);
        if(!(bx>0 && bx<0x48000000 && bz>0 && bz<0x48000000))goto no_site;
        /*410fc0 publishes the last candidate even when all three fail. */
        {float candidate[2];(void)gameplay_geometry_nearby_location(&a->memory,center[0],center[1],radius,candidate);
            if(a->memory.fault)goto failure;
            p[0]=production_cell(a,candidate[0]);p[2]=production_cell(a,candidate[1]);}
        p[1]=(float)(gameplay_geometry_terrain_height(&a->memory,p[0],p[2])+real(a,unit+0x30));
        if(trials++>=20)goto no_site;
        if(!(trials&7))radius=(float)((long double)real(a,unit+0x38)+radius);
        if(!gameplay_geometry_position_clear(&a->memory,unit,p,&clear))goto failure;
    }
    put(a,descriptor+0x4c,get(a,descriptor+0x4c)-1);
    arrival=game_level_factory_take(a,0x45,0);if(!arrival)goto failure;
    if(!sequence(a,owner,&id) || !game_level_arrival_produce_requested(a,arrival,id,p,unit,get(a,get(a,descriptor+0x48)+20),0) || !publish(a,owner,arrival)){
        (void)game_level_factory_destroy(a,arrival);return 0;
    }
    *reason=0;return !a->memory.fault;
no_site:*reason=6;return production_unused(a,unit);
failure:(void)production_unused(a,unit);return 0;
}
int game_level_group_member_activate(GameApplication *a,GameAddress record,uint32_t tick,int restored){
    unsigned f;GameplayDescriptorServices s=lifecycle(a);
    return family(a,record,&f) && gameplay_descriptor_bind(&a->memory,&s,record,f,tick,restored!=0);
}
int game_level_group_member_destroy(GameApplication *a,GameAddress record){
    unsigned f;GameplayDescriptorServices s=lifecycle(a);
    if(!family(a,record,&f))return 0;
    if(!gameplay_descriptor_destroy(&a->memory,&s,record,f,0)){
        GameAddress payload=get(a,record+0x48),table=payload?get(a,payload):0;
        fprintf(stderr,"native descriptor retire failed: record=%08x family=%u payload=%08x table=%08x method=%08x fault=%d\n",
            (unsigned)record,f,(unsigned)payload,(unsigned)table,(unsigned)(table?get(a,table+0x84):0),a->memory.fault);
        return 0;
    }
    return !a->memory.fault;
}
int game_level_descriptor_transient_reset(GameApplication *a,GameAddress record,GameAddress method){
    unsigned f;GameAddress payload;
    if(!family(a,record,&f))return 0;
    if(f)return method==0x53ff40 && !a->memory.fault; /*Original ret. */
    if(method!=0x53de20)return 0;
    if(get(a,record+0x10)!=1)return !a->memory.fault;
    payload=get(a,record+0x48);
    return game_level_scene_transient_reset(a,payload,get(a,get(a,payload)+0x98));
}
static int disabled_property(void *c,GameAddress object,GameAddress method,uint32_t *out){return game_level_scene_property(c,object,method,out);}
static int disable_value(void *c,GameAddress record,int enabled){GameApplication *a=c;unsigned f;
    if(!family(a,record,&f))return 0;
    if(!f)return 1; /*53e820 is the original empty primary method. */
    return gameplay_payload_set_disabled(&a->memory,get(a,record+0x48),f==1,enabled,a,disabled_property);
}
int game_level_group_member_disable(GameApplication *a,GameAddress record){return disable_value(a,record,0);}
extern int game_level_strategic_descriptor_name(GameApplication *,GameAddress,GameAddress *);
/*54a3c0: selling credits half the catalog price, announces the descriptor,
 *and transfers its payload to the departure owner BEFORE recycling the slot.
 *54a420 alone only unlinks the descriptor; using it here orphaned the payload
 *and allowed a replacement factory to reuse its still-borrowed descriptor. */
int game_level_group_remove_member(GameApplication *a,GameAddress group,unsigned slot){
    int count=(int16_t)game_memory_read_u16(&a->memory,group+0x24);GameAddress member,owner,name;
    uint32_t price;unsigned f;int32_t refund,unit;
    GameplayDescriptorServices descriptor=lifecycle(a);
    GameplayGroupDetachServices services={a,disable_value,NULL,NULL};
    if(count<=0 || slot>=(unsigned)count)return 0;
    member=get(a,get(a,group+0x18)+slot*4);if(!member)return !a->memory.fault;
    owner=get(a,group+0x54);unit=(int16_t)game_memory_read_u16(&a->memory,member+0x14);
    if(!family(a,member,&f) || !game_level_player_catalog_unit_price(a,owner,(int16_t)unit,&price)){
        fprintf(stderr,"native station sale price failed: member=%08x owner=%08x unit=%d\n",member,owner,unit);return 0;}
    {int32_t signed_price;memcpy(&signed_price,&price,4);
        price=production_integer((long double)signed_price*real(a,0x5773e0));memcpy(&refund,&price,4);}
    if(!credit(a,owner,refund)){fprintf(stderr,"native station sale credit failed: owner=%08x refund=%d\n",owner,refund);return 0;}
    if(owner==get(a,0x5f0208)){
        char label[128],message[160];unsigned i;
        if(!game_level_strategic_descriptor_name(a,member,&name)){
            fprintf(stderr,"native station sale name failed: member=%08x\n",member);return 0;}
        for(i=0;i<sizeof(label)-1;i++){label[i]=(char)game_memory_read_u8(&a->memory,name+i);if(!label[i])break;}
        label[i]=0;snprintf(message,sizeof(message),"%s sold\n",label);
        if(a->memory.fault || !game_message_enqueue_bytes(a,get(a,0x628a88),message,1))return 0;
    }
    if(get(a,get(a,member)+0x30)!=0x53d860 ||
       !gameplay_descriptor_destroy(&a->memory,&descriptor,member,f,1)){
        fprintf(stderr,"native station sale departure failed: member=%08x family=%u\n",member,f);return 0;}
    return gameplay_group_remove_member(&a->memory,&services,group,(int32_t)slot);
}
static int detach_retire(void *c,GameAddress object){return retire(c,object,0,1);}
int game_level_group_member_detach(GameApplication *a,GameAddress record){unsigned f;
    GameplayGroupDetachServices s={a,disable_value,stop,detach_retire};
    return family(a,record,&f) && gameplay_descriptor_detach(&a->memory,&s,record,f==0);
}
static int sound(void *c,GameAddress point){GameApplication *a=c;
    SoundPlaybackRequest r={0,(float)game_leaf_horizontal_distance_squared(&a->memory,point,get(a,0x58f890)),1,0,0,0};
    return !a->memory.fault && game_menu_sound_play(a,&r);
}
static int phase(void *c,GameAddress object,uint32_t value){GameApplication *a=c;
    return game_level_scene_notice(a,object,get(a,get(a,object)+0x58),value);
}
static int relocate(void *c,GameAddress object,GameAddress point,uint16_t heading){return game_level_secondary_descriptor_relocate(c,object,point,heading);}
static int settled(void *c,GameAddress record){return game_level_secondary_descriptor_settled(c,record);}
static int refresh(void *c,GameAddress object,unsigned slot,GameAddress argument){GameApplication *a=c;GameAddress method=get(a,get(a,object)+slot);
    if(slot==0x78){float p[3];int rollback;unsigned i;extern int game_level_scene_contacts(GameApplication *,GameAddress,GameAddress,const float[3],int *);
        for(i=0;i<3;i++)p[i]=real(a,argument+i*4);
        return game_level_scene_contacts(a,object,method,p,&rollback);
    }
    if(method==0x553ef0 || method==0x4e65a0 || method==0x407ee0)return vehicle_presentation_group_geometry(&a->memory,object,0);
    if(method==0x553b90 || method==0x4e6c20 || method==0x4083e0)return vehicle_presentation_group_geometry(&a->memory,object,2);
    {int ok=game_level_scene_update(a,object,method);
     if(!ok)fprintf(stderr,"native descriptor refresh failed: object=%08x slot=%x method=%08x\n",object,slot,method);
     return ok;}
}
static int activity(void *c,GameAddress object,int *active){GameApplication *a=c;return native_entity_type_activity(&a->memory,object,active);}
static int intersect(void *c,GameAddress object,GameAddress other,float p[3],int *hit){GameApplication *a=c;
    if(get(a,get(a,object)+0x24)!=0x482bb0)return 0;
    return presentation_record_collision(&a->memory,object,other,p,hit);
}
static int contact(void *c,GameAddress object,GameAddress other,const uint32_t v[3],const float p[3],uint32_t *out){GameApplication *a=c;
    *out=0;return game_level_contact_response(a,object,other,get(a,get(a,object)+0x2c),v,p);
}
static GameAddress contact_allocate(void *c){GameApplication *a=c;EffectResourcePool *pool=effect_resource_pool_find(0x631910);
    return pool?effect_resource_pool_acquire_kind(&a->memory,pool,2):0;
}
static int contact_release(void *c,GameAddress head){GameApplication *a=c;EffectResourcePool *pool=effect_resource_pool_find(0x631910);
    return pool && effect_resource_pool_return_chain(&a->memory,pool,head)>0;
}
int game_level_group_member_reset(GameApplication *a,GameAddress record){unsigned f;GameplayDescriptorServices s=lifecycle(a);
    GameplayDescriptorUpdateServices update={&s,a,sound,phase,relocate,refresh,settled,activity,intersect,contact,contact_allocate,contact_release};
    if(!family(a,record,&f)){
        fprintf(stderr,"native descriptor family failed: record=%08x table=%08x\n",record,get(a,record));return 0;
    }
    return gameplay_descriptor_update(&a->memory,&update,record,f);
}
