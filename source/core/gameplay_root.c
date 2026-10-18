#include "gameplay_root.h"
#include "gameplay_geometry.h"
#include "terrain_asset.h"
#include "entity_queries.h"
#include <math.h>
#include <string.h>

static int32_t signed_word(uint32_t word){int32_t value;memcpy(&value,&word,4);return value;}
static float real(GameMemory *m,GameAddress p){uint32_t bits=game_memory_read_u32(m,p);float value;memcpy(&value,&bits,4);return value;}
static uint32_t truncate_word(long double value){
    return (uint32_t)(uint64_t)(!isfinite(value) || value< -0x1p63L || value>=0x1p63L?INT64_MIN:(int64_t)value);
}
static int random_sample(GameMemory *m,float *out){
    uint32_t cursor=game_memory_read_u32(m,0x6308c0);
    if(cursor==1024)cursor=0;
    if(cursor>1024){m->fault=1;m->fault_address=0x6308c0;m->fault_size=4;return 0;}
    *out=real(m,0x6308c8+cursor*4);game_memory_write_u32(m,0x6308c0,cursor+1);return !m->fault;
}
#define GET(p) game_memory_read_u32(m,(p))
#define PUT(p,v) game_memory_write_u32(m,(p),(v))
static int ground_descriptor_pick(GameMemory *m,GameAddress site,GameAddress *target){
    uint16_t row_word=game_memory_read_u16(m,site+0x24),choice_word=game_memory_read_u16(m,site+0x26);int16_t rows,choices;
    float sample;uint32_t budget;unsigned i;memcpy(&rows,&row_word,2);memcpy(&choices,&choice_word,2);
    if(!random_sample(m,&sample))return 0;
    budget=truncate_word((long double)choices*sample)+1u;
    for(i=0;rows>0 && i<(unsigned)rows && signed_word(budget)>0;i++){
        GameAddress descriptor=GET(GET(site+0x18)+i*4);if(!descriptor)continue;
        budget--;if(!native_entity_descriptor_target(m,descriptor,target))return 0;
    }
    return !m->fault;
}
/*533f40: separate aircraft producer. Its site mask starts at bit1, and a
 *ready cycle restarts the timer even when the random attempt finds no target. */
int gameplay_ground_aircraft_update(GameMemory *m,GameAddress object,const GameplayGroundProducerServices *s){
    GameAddress target=0,site=0,unit=0,root;uint32_t start,tick,mask,eligible=0,budget,roll;
    int16_t count;float sample,nearest,position[3],destination[2],candidate[2],radius;unsigned i,visits=0,trials=0;int clear=0;
    if(!s || !s->base_update || !s->acquire || !s->clear || !s->recycle || !s->aircraft_deploy || m->fault)return 0;
    start=GET(object+0xa0);tick=GET(0x5ac6ac);
    if(!GET(0x5853ec) || (start!=UINT32_MAX && signed_word(start+GET(0x5ac69c))>=signed_word(tick)))
        return !m->fault && s->base_update(s->context,object);
    mask=GET(object+0xac);
    if(mask){
        GameAddress sites=GET(0x639a70);uint16_t word=game_memory_read_u16(m,0x639a74);memcpy(&count,&word,2);
        for(i=0;count>0 && i<(unsigned)count;i++){
            GameAddress owner=GET(GET(sites+i*4)+0x54);
            if(owner && (mask&(1u<<((i+1u)&31u))) && !GET(0x5f0160+(GET(object+20)&63u)*24u+(game_memory_read_u16(m,owner+8)&63u)*4u))eligible++;
        }
        if(!eligible)return !m->fault; /*No eligible sites neither updates nor restarts. */
        if(!random_sample(m,&sample))return 0;
        budget=truncate_word((long double)eligible*sample)+1u;
        for(i=0;count>0 && i<(unsigned)count;i++){
            GameAddress candidate_site=GET(sites+i*4),owner=GET(candidate_site+0x54);
            if(owner && (mask&(1u<<((i+1u)&31u))) && !GET(0x5f0160+(GET(object+20)&63u)*24u+(game_memory_read_u16(m,owner+8)&63u)*4u) && !--budget){site=candidate_site;break;}
        }
        if(!site)return 0;
        if(!ground_descriptor_pick(m,site,&target))return 0;
    }else{
        if(!random_sample(m,&sample))return 0;
        roll=truncate_word((long double)sample*real(m,0x5762cc));
        if(!roll){
            nearest=(float)signed_word(GET(0x629730));
            for(i=0;i<4;i++){
                GameAddress player=GET(0x5f0210+i*4),page;
                if(!GET(player+20))continue;
                for(page=GET(player+0x294);page;page=GET(page+0x64)){
                    float distance;if(++visits>65536u)return 0;
                    distance=gameplay_geometry_fast_distance(m,real(m,page+0x48),real(m,page+0x50),real(m,object+24),real(m,object+32));
                    if(distance<nearest || isnan(distance) || isnan(nearest)){site=page;nearest=distance;}
                }
            }
            if(site && !ground_descriptor_pick(m,site,&target))return 0;
        }
    }
    if(target && GET(object+24)>0 && GET(object+24)<0x48000000u && GET(object+32)>0 && GET(object+32)<0x48000000u){
        if(m->fault || !(unit=s->acquire(s->context,object)))return 0;
        radius=real(m,object+0x34);
        if(radius<real(m,object+0x2c) || isnan(radius) || isnan(real(m,object+0x2c)))radius=real(m,object+0x2c);
        radius=(float)((long double)radius+real(m,0x5762c8));
        do{
            (void)gameplay_geometry_nearby_location(m,real(m,object+24),real(m,object+32),radius,candidate);
            position[0]=candidate[0];position[2]=candidate[1];position[1]=(float)(gameplay_geometry_terrain_height(m,position[0],position[2])+real(m,unit+0x30));
            if(m->fault || ++trials>1024 || !s->clear(s->context,unit,position,&clear))goto failed;
        }while(!clear);
        if(!random_sample(m,&sample))goto failed;
        roll=truncate_word((long double)sample*real(m,0x5762d0));
        if(roll && GET(target+24)>0 && GET(target+24)<0x48000000u && GET(target+32)>0 && GET(target+32)<0x48000000u){
            radius=(float)((long double)signed_word(roll)*real(m,0x5762d4));
            (void)gameplay_geometry_nearby_location(m,real(m,target+24),real(m,target+32),radius,destination);
        }else{destination[0]=real(m,target+24);destination[1]=real(m,target+32);}
        root=GET(0x58973c);if(m->fault || !s->aircraft_deploy(s->context,root,unit,position,destination))goto failed;
    }
    PUT(object+0xa0,GET(0x5ac6ac));return !m->fault;
failed:
    if(unit)(void)s->recycle(s->context,unit);
    return 0;
}
int gameplay_ground_producer_update(GameMemory *m,GameAddress object,GameAddress method,
        const GameplayGroundProducerServices *s){
    GameAddress root,special,controller,unit=0;uint32_t start,tick,delay=0,i;float p[3],candidate[2],radius;
    int near=0,clear=0;unsigned trials=0;
    if(!s || !s->base_update || !s->acquire || !s->clear || !s->deploy || !s->recycle || m->fault)return 0;
    if(method==0x533cf0u)delay=0x5ac690u;
    else if(method==0x5345a0u)delay=0x5ac698u;
    else if(method==0x5347a0u)delay=0x5ac694u;
    else if(method!=0x533730u)return 0;
    root=GET(0x58973c);special=GET(0x5f0224);controller=GET(object+0xa8);
    if(delay){
        if(root==special && controller && (!s->controller_refresh || !s->controller_refresh(s->context,controller)))return 0;
        start=GET(object+0xa0);tick=GET(0x5ac6ac);
        if(start!=UINT32_MAX && signed_word(start+GET(delay))>=signed_word(tick))
            return !m->fault && s->base_update(s->context,object);
    }
    if(m->fault || !s->base_update(s->context,object))return 0;
    root=GET(0x58973c);special=GET(0x5f0224);
    if(root==special){
        int32_t result=0;unsigned count=game_memory_read_u8(m,object+12)==0x60?4u:1u;
        if(!s->worker)return 0;
        for(i=0;i<count && result> -1;i++){
            if(!s->worker(s->context,controller,object,&result))return 0;
            if(result>=0)PUT(object+0xa0,GET(0x5ac6ac));
        }
        return !m->fault;
    }
    start=GET(object+0xa0);tick=GET(0x5ac6ac);
    if(GET(object+0xa4)>=8u || (start!=UINT32_MAX && signed_word(tick-start)<=580))return !m->fault;
    for(i=0;i<4;i++){
        GameAddress player=GET(0x5f0210+i*4),rig;
        if(!GET(player+20))continue;
        rig=GET(player+0x3bc);if(signed_word(GET(rig+0x58))<=0)continue;
        {float distance=gameplay_geometry_fast_distance(m,real(m,rig+24),real(m,rig+32),real(m,object+24),real(m,object+32));
        float limit=real(m,0x5762bc);
        if(distance<limit || isnan(distance) || isnan(limit)){
            near=1;break;
        }}
    }
    if(!near || !(GET(object+24)>0 && GET(object+24)<0x48000000u && GET(object+32)>0 && GET(object+32)<0x48000000u))return !m->fault;
    if(m->fault || !(unit=s->acquire(s->context,object)))return 0;
    radius=real(m,object+0x34);
    if(radius<real(m,object+0x2c) || isnan(radius) || isnan(real(m,object+0x2c)))radius=real(m,object+0x2c);
    radius=(float)((long double)radius+real(m,0x5762c8));
    do{
        (void)gameplay_geometry_nearby_location(m,real(m,object+24),real(m,object+32),radius,candidate);
        p[0]=candidate[0];p[2]=candidate[1];
        p[1]=(float)(gameplay_geometry_terrain_height(m,p[0],p[2])+real(m,unit+0x30));
        if(m->fault || !s->clear(s->context,unit,p,&clear))goto failed;
        /* A crowded outlet is ordinary gameplay, not a failed level. Keep
         * the bounded search, return the unused unit and retry next tick
         * without charging production or advancing its accepted timestamp. */
        if(!clear && ++trials>=1024u)
            return s->recycle(s->context,unit) && !m->fault;
    }while(!clear);
    if(!s->deploy(s->context,root,unit,p))goto failed;
    PUT(object+0xa0,GET(0x5ac6ac));PUT(object+0xa4,GET(object+0xa4)+1u);return !m->fault;
failed:
    (void)s->recycle(s->context,unit);return 0;
}
static unsigned controller_category(uint8_t kind){
    if(kind>=0x20 && kind<=0x28)return kind-0x1f;
    if(kind==0x40)return 10;
    if(kind>=0x60 && kind<=0x63)return 11;
    if(kind>=0x80 && kind<=0x83)return 12;
    return kind>=0x84 && kind<=0x89?13:0;
}
static int controller_history(GameMemory *m,GameAddress root,unsigned action,const float position[3],const GameplayControllerActions *s){
    unsigned i=0;
    while(signed_word(GET(root+0x444))>(int32_t)i){
        GameAddress record;uint32_t identity;
        if(i>=10){m->fault=1;return 0;}
        record=GET(root+0x3cc+i*4);identity=GET(root+0x3f4+i*4);
        if(GET(record+20)==identity){
            if(GET(0x58973c)==GET(0x5f0208) && !s->sound(s->context,123))return 0;
            record=GET(root+0x3cc+i*4);
            if(!s->history_order(s->context,record,action,GET(root+0x3c4),position))return 0;
        }
        PUT(root+0x3cc+i*4,0);PUT(root+0x3f4+i*4,0);PUT(root+0x41c+i*4,0);i++;
        if(m->fault)return 0;
    }
    PUT(root+0x444,0);return !m->fault;
}
/*480d00: execute real controller choices and the seven-entry category row.
 *History borrows identities; callbacks consume native command/factory owners.
 *The retained fast reference reverses target and relation branches: these
 *conditions follow the original instruction bytes at481214/481276/4811ee. */
int gameplay_root_controller_action(GameMemory *m,GameAddress root,const uint16_t coordinates[4],const GameplayControllerActions *s){
    GameAddress target;uint32_t selected,relation=0;unsigned category=0,i;int valid;
    float point[3];
    if(!m || m->fault || !s || !coordinates)return 0;
    point[0]=(float)((uint32_t)coordinates[2]*2);point[1]=32768;point[2]=(float)((uint32_t)coordinates[3]*2);
    selected=GET(root+0x604);
    if(selected==8 || selected==9)return s->player_service(s->context,GET(root+0x3bc),selected==9);
    if(selected==12)return controller_history(m,root,12,point,s);
    target=GET(root+0x3c4);valid=target && GET(target+20)==GET(root+0x3c8);
    if(valid){
        uint32_t owner=game_memory_read_u16(m,root+8),other=GET(target+20)&63u;
        relation=GET(0x5f0160+(owner&63u)*24+other*4);
        if(GET(root+0x444)){
            if(selected==11){
                if(relation)return !m->fault;
                return controller_history(m,root,11,point,s);
            }
            if(selected==13 && relation)return controller_history(m,root,13,point,s);
        }
        category=controller_category(game_memory_read_u8(m,target+12));
        if(selected==10 && (GET(root+0x3c8)&63u)==owner && (category==12 || category==13 || category==4)){
            int count=(int16_t)game_memory_read_u16(m,0x639a74),site;
            for(site=0;site<count;site++){
                GameAddress group=GET(GET(0x639a70)+(unsigned)site*4);unsigned slot=0;
                while((int32_t)slot<(int16_t)game_memory_read_u16(m,group+0x24)){
                    GameAddress member=GET(GET(group+0x18)+slot*4);
                    if(member){
                        uint32_t maximum;GameAddress payload=GET(member+0x48);
                        if(!s->maximum(s->context,payload,&maximum))return 0;
                        if(signed_word(maximum)>signed_word(GET(payload+0x58)))
                            return s->repair(s->context,root,(uint8_t)GET(group),(uint8_t)slot);
                    }
                    slot++;if(slot>32767 || m->fault)return 0;
                    group=GET(GET(0x639a70)+(unsigned)site*4);
                }
                count=(int16_t)game_memory_read_u16(m,0x639a74);
            }
        }
    }
    for(i=0;i<7;i++){
        uint8_t action=game_memory_read_u8(m,root+0x518+category*7+i);
        if(m->fault)return 0;
        switch(action){
        case 0:return 1;
        case 1:if(GET(root+0x4cc) && signed_word(GET(root+0x500))<16)return s->produce(s->context,root,0x40,4,0);break;
        case 2:if(GET(root+0x4d0) && signed_word(GET(root+0x504)+1)<=8)return s->produce(s->context,root,0x21,0,0);break;
        case 3:if(GET(root+0x4d4) && signed_word(GET(root+0x508)+1)<=7)return s->produce(s->context,root,0x27,0,0);break;
        case 4:case 7:
            if(GET(root+(action==4?0x4d8:0x4e4)) && !relation){
                target=GET(root+0x3c4);
                if(target && GET(target+20)==GET(root+0x3c8))
                    return s->produce(s->context,root,action==4?0x26:11,truncate_word(real(m,target+24)),truncate_word(real(m,target+32)));
                return s->message(s->context,action==4?0x589a84:0x589a94);
            }
            break;
        case 5:case 6:
            /*Retail uses kind22 for both configured primary choices. */
            if(GET(root+(action==5?0x4dc:0x4e0)))return s->produce(s->context,root,0x22,(uint32_t)coordinates[0]*2,(uint32_t)coordinates[1]*2);
            break;
        }
    }
    return !m->fault;
}
/*47d5e8: select/remove a borrowed unit, or send the retained group to a real
 *target. Arrays remain the controller's records; command nodes retain their
 *existing scene owner. Retail leaves the shifted tail stale after removal. */
int gameplay_root_history_toggle(GameMemory *m,GameAddress root,const GameplayControllerActions *s){
    GameAddress target,player;uint32_t identity;int count,index;unsigned i;int removed=0;
    if(!m || m->fault || !s)return 0;
    player=GET(root+0x3bc);if(!GET(player+0x58))return !m->fault;
    target=GET(root+0x3c4);identity=GET(root+0x3c8);
    if(target && game_memory_read_u8(m,target+12)==0x26)return !m->fault;
    if(!target || GET(target+20)!=identity){
        for(i=0;i<10;i++){PUT(root+0x3cc+i*4,0);PUT(root+0x3f4+i*4,0);PUT(root+0x41c+i*4,0);}
        PUT(root+0x444,0);
        return GET(0x58973c)!=GET(0x5f0208)?!m->fault:s->sound(s->context,123);
    }
    count=signed_word(GET(root+0x444));if(count<0 || count>10){m->fault=1;return 0;}
    for(index=0;index<count;index++)if(GET(root+0x3f4+(unsigned)index*4)==identity){
        for(i=(unsigned)index+1;i<(unsigned)count;i++){
            PUT(root+0x3f4+(i-1)*4,GET(root+0x3f4+i*4));
            PUT(root+0x3cc+(i-1)*4,GET(root+0x3cc+i*4));
            PUT(root+0x41c+(i-1)*4,GET(root+0x41c+i*4));
        }
        PUT(root+0x444,(uint32_t)--count);removed=1;break;
    }
    if(!removed && game_memory_read_u16(m,root+8)==(identity&63u) && count<9){
        if(GET(target+16)&0x40u){
            PUT(root+0x3cc+(unsigned)count*4,target);PUT(root+0x3f4+(unsigned)count*4,identity);
            PUT(root+0x41c+(unsigned)count*4,GET(0x5ac6ac));PUT(root+0x444,(uint32_t)count+1);
            return GET(0x58973c)!=GET(0x5f0208)?!m->fault:s->sound(s->context,123);
        }
        return controller_history(m,root,13,NULL,s);
    }
    if(!count)return !m->fault;
    if(GET(0x5f0160+(game_memory_read_u16(m,root+8)&63u)*24+(GET(target+20)&63u)*4))return !m->fault;
    /*4125b0 compares against the local player, even for another controller. */
    player=GET(GET(0x5f0208)+0x3bc);
    if(GET(player+20)==GET(target+20) || !(GET(target+16)&0x8040u))return !m->fault;
    return controller_history(m,root,11,NULL,s);
}
/*480bd0: cumulative signed weights and the optional special-ammo gate. */
int gameplay_root_choose_pickup(GameMemory *m,GameAddress root,int check_inventory,
    uint32_t *variant,uint32_t *payload,int *selected){
    uint32_t total=GET(root+0x5a8),weight=0,index,threshold;float sample;
    *selected=0;*variant=*payload=0;if(!total)return !m->fault;
    if(!random_sample(m,&sample))return 0;
    threshold=truncate_word((long double)signed_word(total)*sample);
    for(index=0;index<9;index++){
        weight+=GET(root+0x5ac+index*8);
        if(signed_word(weight)>signed_word(threshold))break;
    }
    if(index==9)return !m->fault;
    *variant=index;*payload=GET(root+0x5b0+index*8);
    if(check_inventory && index==4){uint32_t choice;
        if(!random_sample(m,&sample))return 0;
        choice=truncate_word((long double)sample*real(m,0x573e30));
        *selected=GET(GET(root+0x3bc)+0x1dc+choice*4)>=1u;
    }else *selected=1;
    return !m->fault;
}
int gameplay_root_update(GameMemory *m,GameAddress root,const GameplayRootServices *s){
    static const unsigned lists[3]={0x1c,0x80,0xe4};
    GameAddress player,seen,group,event,object,table,next;uint32_t tick,status,index,count,method;
    unsigned i,j,visits;float sample;
    if(!m || !s || m->fault)return 0;
    player=GET(root+0x3bc);
    if(!GET(player+0x58))PUT(root+0x604,0);
    else if(!(GET(0x5ac6ac)&3u) && (!s->idle || !s->idle(s->context,root)))return 0;
    status=GET(root+0x448);seen=GET(root+0x450);
    if(status!=seen && signed_word(GET(root+0x454)+218u)<signed_word(GET(0x5ac6ac))){
        PUT(root+0x450,status);
        if(root==GET(0x5f0208)){GameAddress camera=GET(0x58f8a8);
            if(camera && (!s->camera || !s->camera(s->context,camera,status)))return 0;}
        PUT(root+0x454,GET(0x5ac6ac));
    }
    if(m->fault || !s->sound_update || !s->sound_update(s->context))return 0;
    if(root==GET(0x5f0208) && GET(root+0x57c)==GET(0x5ac6ac) && !GET(root+0x580)){
        if(!s->notice || !s->notice(s->context,root))return 0;
        /*The original skips network_update after this event in offline play. */
        if(GET(0x5eff04) && GET(root+0x460) &&
           (!s->network_update || !s->network_update(s->context,root)))return 0;
    }else if(GET(0x5eff04) && GET(root+0x460)){
        if(!s->network_update || !s->network_update(s->context,root))return 0;
    }
    tick=GET(0x5ac6ac);
    if(signed_word(tick)%30==0){
        group=GET(root+0x294);method=group?GET(GET(root)+0x34):0;visits=0;
        while(group){uint32_t value;
            if(++visits>65536 || !s->group_value || !s->credit_group ||
               !s->group_value(s->context,group,&value) || !s->credit_group(s->context,root,method,value))return 0;
            group=GET(group+0x64);if(m->fault)return 0;
        }
    }
    /*Original cumulative comparison is signed, even though the additions wrap.
     *A failed requested spawn leaves its deadline and consumes no deadline RNG. */
    if(GET(0x5ac68c) && signed_word(GET(root+0x5a4))<signed_word(GET(0x5ac6ac)) &&
       GET(root+0x448)==GET(root+0x3bc) && signed_word(GET(GET(root+0x3bc)+0x58))>0){
        int selected=0,spawned;uint32_t payload=0;
        if(!gameplay_root_choose_pickup(m,root,1,&index,&payload,&selected))return 0;
        if(selected){
            if(m->fault || !s->spawn || !s->spawn(s->context,root,GET(root+0x5f4),GET(root+0x5f8),index,payload,&spawned))return 0;
            if(!spawned)goto queued_events;
        }
        {uint32_t minimum=GET(root+0x5fc),span=GET(root+0x600)-minimum;
            if(!random_sample(m,&sample))return 0;
            PUT(root+0x5a4,truncate_word((long double)signed_word(span)*sample)+minimum+GET(0x5ac6ac));}
    }
queued_events:
    visits=0;
    while((event=GET(root+0x458))!=0 && GET(event+12)==GET(0x5ac6ac)){
        if(m->fault || ++visits>65536)return 0;
        PUT(root+0x458,GET(event+4));if(GET(root+0x45c)==event)PUT(root+0x45c,0);
        if(m->fault || !s->retire_event || !s->retire_event(s->context,root,event))return 0;
    }
    player=GET(root+0x3bc);method=GET(GET(player)+0x34);
    if(m->fault || !s->update || !s->update(s->context,player,method))return 0;
    /*47ad17..47ad86 intentionally advances after a deletion. Adjacent invalid
     *entries can therefore survive one tick; expiration is exact age640. */
    count=GET(root+0x444);if(signed_word(count)>10){m->fault=1;m->fault_address=root+0x444;m->fault_size=4;return 0;}
    for(i=0;signed_word(GET(root+0x444))>(int32_t)i;i++){
        GameAddress at=root+0x3cc+i*4;object=GET(at);
        if(object && GET(object+0x14)==GET(at+0x28) && GET(at+0x50)-GET(0x5ac6ac)!=0xfffffd80u)continue;
        count=GET(root+0x444);
        for(j=i+1;j<count;j++){
            PUT(root+0x3cc+(j-1)*4,GET(root+0x3cc+j*4));
            PUT(root+0x3f4+(j-1)*4,GET(root+0x3f4+j*4));
            PUT(root+0x41c+(j-1)*4,GET(root+0x41c+j*4));
        }
        PUT(root+0x444,GET(root+0x444)-1u);if(m->fault)return 0;
    }
    for(i=0;i<3;i++){
        object=root+lists[i];method=GET(GET(object)+0x50);
        if(m->fault || !s->next || !s->next(s->context,object,method,&next))return 0;
        object=next;visits=0;
        while(object){
            if(++visits>65536)return 0;
            table=GET(object);method=GET(table+0x34);
            if(m->fault || !s->update(s->context,object,method))return 0;
            method=GET(table+0x50); /*Table is cached before the update. */
            if(m->fault || !s->next(s->context,object,method,&next))return 0;
            object=next;
        }
    }
    group=GET(root+0x294);visits=0;
    while(group){
        if(++visits>65536 || !s->finish_group || !s->finish_group(s->context,group))return 0;
        group=GET(group+0x64);if(m->fault)return 0;
    }
    return !m->fault;
}
/*4808b0: sample angle/radius, reject terrain/collision/slope at most ten
 *times, then initialize and transfer the existing factory member to its root. */
int gameplay_root_random_spawn(GameMemory *m,GameAddress root,uint32_t mode,uint32_t angle_width,
    uint32_t minimum_radius,uint32_t maximum_radius,uint32_t variant,uint32_t payload,
    const GameplayRootSpawnServices *s,GameAddress *result){
    GameAddress player,object;float span=(float)signed_word(maximum_radius-minimum_radius),width;
    uint16_t width_bits=(uint16_t)angle_width,half=(uint16_t)((width_bits>>1)|((width_bits&0x8000u)?0x8000u:0));
    int16_t signed_width;unsigned attempt;float position[3],normal[3],sample;uint32_t sequence,identity,flags;
    *result=0;if(!s || m->fault)return 0;player=GET(root+0x3bc);
    if(signed_word(GET(player+0x58))<=0)return !m->fault;
    if(!s->acquire || !(object=s->acquire(s->context)))return 0;
    memcpy(&signed_width,&width_bits,2);width=(float)signed_width;
    for(attempt=0;attempt<10 && !m->fault;attempt++){
        uint16_t angle;uint32_t radius,ix,iz,offset,normal_bits;uint8_t terrain_flags;int clear=0;
        if(!random_sample(m,&sample))goto failed;
        angle=(uint16_t)truncate_word((long double)sample*width);player=GET(root+0x3bc);
        angle=(uint16_t)((angle+game_memory_read_u16(m,player+36)+game_memory_read_u16(m,player+0x26c)-half)&2047u);
        if(!random_sample(m,&sample))goto failed;
        radius=minimum_radius+truncate_word((long double)sample*span);
        player=GET(root+0x3bc);
        position[0]=(float)((long double)signed_word(radius)*real(m,0x6298a8+(uint32_t)angle*4)+real(m,player+24));
        position[2]=(float)((long double)signed_word(radius)*real(m,0x6298a8+(((uint32_t)angle+512u)&2047u)*4)+real(m,player+32));
        position[1]=(float)(gameplay_geometry_terrain_height(m,position[0],position[2])+real(m,object+0x30));
        ix=truncate_word(position[0])&0xfffffe3fu;ix=(ix>>6)|((ix&0x80000000u)?0xfc000000u:0);
        iz=truncate_word(position[2])&0xfffffe00u;offset=ix+iz*4u;
        if(m->terrain){if(offset>=65536u*8u || !m->terrain->cells){m->fault=1;goto failed;}
            terrain_flags=m->terrain->cells[offset/8u].flags;}
        else terrain_flags=game_memory_read_u8(m,GET(0x597d90)+offset+5);
        if(!gameplay_geometry_terrain_normal_values(m,normal,position[0],position[2]))goto failed;
        if(terrain_flags&0x56)continue;
        if(!gameplay_geometry_position_clear(m,object,position,&clear))goto failed;
        if(!clear)continue;
        memcpy(&normal_bits,normal+1,4);
        if(signed_word(normal_bits)>0x3f59999a)break;
    }
    if(m->fault)goto failed;
    if(attempt==10)return s->recycle && s->recycle(s->context,object);
    sequence=GET(root+24);identity=(sequence<<6)|game_memory_read_u16(m,root+8);PUT(root+24,sequence+1);
    flags=(mode&0xffffff00u)|(((mode<1?255u:0u)+3u)&255u);
    if(!s->initialize || !s->initialize(s->context,object,identity,position,variant,payload,flags))goto failed;
    if(!s->publish || !s->publish(s->context,root,object))return 0;
    if(root==GET(0x5f0208) && (!s->sound || !s->sound(s->context)))return 0;
    *result=object;return !m->fault;
failed:{GameMemory original_fault=*m;
    m->fault=0;if(s->recycle)(void)s->recycle(s->context,object);
    if(original_fault.fault){m->fault=original_fault.fault;m->fault_address=original_fault.fault_address;
        m->fault_size=original_fault.fault_size;m->fault_write=original_fault.fault_write;}
    return 0;}
}
#undef GET
#undef PUT

static uint32_t relation(GameMemory *m,GameAddress root,GameAddress target){
    return game_memory_read_u32(m,0x5f0160+(game_memory_read_u16(m,root+8)&63u)*24u+
        (game_memory_read_u32(m,target+0x14)&63u)*4u);
}
static int valid_target(GameMemory *m,GameAddress root,GameAddress target){
    return target && game_memory_read_u32(m,target+0x14)==game_memory_read_u32(m,root+0x3c8);
}
/*4107a0 stores both coordinate deltas and its result as binary32. */
static float region_distance(GameMemory *m,GameAddress point,GameAddress scene){
    long double dx=(long double)real(m,point)-real(m,scene+0x18);
    long double dz=(long double)real(m,point+8)-real(m,scene+0x20);
    float x=(float)dx,z=(float)dz,scale=real(m,0x571f18),zero=real(m,0x571f14);
    if(dx<zero || isnan(dx))x=-x;
    if(dz<zero || isnan(dz))z=-z;
    return (float)((z<x || isnan(z) || isnan(x))?(long double)z*scale+x:(long double)x*scale+z);
}
#define GET(p) game_memory_read_u32(m,(p))
#define PUT(p,v) game_memory_write_u32(m,(p),(v))
int gameplay_root_choose_action(GameMemory *m,GameAddress root,void *context,GameplayRootMaximum maximum){
    GameAddress target,scene,region,member,owner;uint32_t category=0,rel=0,action=0,value;
    unsigned i,j;int special,found=0;
    if(!m || m->fault)return 0;
    scene=GET(root+0x448);
    if(!scene){PUT(root+0x604,0);return !m->fault;}
    PUT(root+0x4dc,0);PUT(root+0x4e0,0);
    if(signed_word(GET(root+0x444))>0){
        target=GET(root+0x3c4);action=12;
        if(valid_target(m,root,target)){
            if(!relation(m,root,target))action=11;
            else if(signed_word(GET(root+0x444))>1 || GET(root+0x3cc)!=target)action=13;
        }
        PUT(root+0x604,action);return !m->fault;
    }
    special=game_memory_read_u8(m,scene+12)==0x23;
    for(i=0;(int32_t)i<(int16_t)game_memory_read_u16(m,0x639a74);i++){
        region=GET(GET(0x639a70)+i*4);
        if(!special){
            GameAddress player=GET(root+0x3bc);float x=real(m,player+0x18),z=real(m,player+0x20);
            float minx=real(m,region+4),maxx=real(m,region+12),minz=real(m,region+8),maxz=real(m,region+16);
            /*Unordered comparisons take the same branch as the original. */
            if(!(minx>x) && !(maxx<x) && !isnan(maxx) && !isnan(x) &&
               !(minz>z) && !(maxz<z) && !isnan(maxz) && !isnan(z)){
                if(GET(region+0x54)==root && GET(GET(region+0x18))){
                    if(m->fault || !maximum || !maximum(context,player,&value))return 0;
                    if(signed_word(value)>signed_word(GET(player+0x58))){PUT(root+0x604,9);return !m->fault;}
                }
                /*The getter may mutate the region publication. */
                region=GET(GET(0x639a70)+i*4);owner=GET(region+0x54);
                if(!owner || (owner==GET(0x58973c) && !GET(GET(region+0x18)))){
                    PUT(root+0x604,8);return !m->fault;
                }
            }
        }
        region=GET(GET(0x639a70)+i*4);
        if(GET(region+0x54)!=root || found)continue;
        member=GET(GET(region+0x18));if(!member || !GET(member+0x4c))continue;
        for(j=0;(int32_t)j<(int16_t)game_memory_read_u16(m,region+0x24);j++){
            float distance;
            scene=GET(root+0x448);
            if((special && GET(member+0x48)==scene) ||
               ((distance=region_distance(m,GET(region+0x1c)+j*12,scene)),distance<real(m,0x573e50) || isnan(distance) || isnan(real(m,0x573e50)))){
                found=1;PUT(root+0x4dc,GET(member+0x4c));PUT(root+0x4e0,GET(member+0x4c));break;
            }
            region=GET(GET(0x639a70)+i*4);
        }
        if(m->fault)return 0;
    }
    target=GET(root+0x3c4);
    if(valid_target(m,root,target)){
        unsigned kind=game_memory_read_u8(m,target+12);
        rel=relation(m,root,target);
        if(kind>=0x20 && kind<=0x28)category=kind-0x20+1;
        else if(kind==0x40)category=10;
        else if(kind==0x60 || (kind>=0x62 && kind<=0x64))category=11;
        else if(kind>=0x80 && kind<=0x83)category=12;
        else if(kind>=0x84 && kind<=0x89)category=13;
        if((GET(root+0x3c8)&63u)==game_memory_read_u16(m,root+8) &&
           (category==12 || category==13 || category==4)){
            if(m->fault || !maximum || !maximum(context,target,&value))return 0;
            if(signed_word(value)>signed_word(GET(target+0x58))){PUT(root+0x604,10);return !m->fault;}
        }
    }
    PUT(root+0x604,0);
    for(i=0;i<7;i++){
        unsigned choice=game_memory_read_u8(m,root+0x518+category*7+i);int eligible=0;
        switch(choice){
        case 0:return !m->fault; /*Retail jump-table zero terminates the row. */
        case 1:eligible=GET(root+0x4cc) && signed_word(GET(root+0x500))<16;break;
        case 2:eligible=GET(root+0x4d0) && signed_word(GET(root+0x504)+1u)<=8;break;
        case 3:eligible=GET(root+0x4d4) && signed_word(GET(root+0x508)+1u)<=7;break;
        case 4:case 7:eligible=GET(root+0x4d8+(choice-4)*4) && !rel && valid_target(m,root,GET(root+0x3c4));break;
        case 5:eligible=GET(root+0x4dc)!=0;break;
        case 6:eligible=GET(root+0x4e0)!=0;break;
        }
        if(eligible){PUT(root+0x604,choice);break;}
    }
    return !m->fault;
}
#undef GET
#undef PUT
