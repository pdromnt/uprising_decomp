#include "level_run.h"
#include "menu_network.h"
#include "menu_pause.h"
#include "gameplay_input.h"
#include "effect_resource_pool.h"
#include "scene_projection_storage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern uint32_t game_menu_milliseconds(GameApplication *);
extern int game_level_display_visibility(GameApplication *,int);
extern int game_level_clock_queue_return(GameApplication *,GameAddress,GameAddress);
extern int game_level_clock_queue_return_chain(GameApplication *,GameAddress,GameAddress);
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
static int32_t signed_word(uint32_t v){return v<=INT32_MAX?(int32_t)v:(int32_t)((int64_t)v-4294967296LL);}
static int less(uint32_t x,uint32_t y){return (x^0x80000000u)<(y^0x80000000u);}
static void wire_word(uint8_t *p,uint32_t v){unsigned i;for(i=0;i<4;++i)p[i]=(uint8_t)(v>>(i*8));}
static uint32_t read_word(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static uint32_t terminal_sum(GameApplication *a,GameAddress node){
    GameAddress record=get(a,node+0x10);uint32_t last=(uint32_t)game_memory_read_u8(&a->memory,record+0xd)-1u;
    return game_memory_read_u8(&a->memory,get(a,record+0x10)+last)+game_memory_read_u8(&a->memory,get(a,record+0x14)+last);
}
/*457cf0/4fec60 deliberately exclude byte0 of each bitmap. Queue terminal
 *bytes and counters belong to their existing records, not a serialized mirror. */
static uint32_t visibility_checksum(GameApplication *a,GameAddress owner){
    uint32_t sum=0;unsigned i;
    for(i=1;i<2048;i++)sum+=game_memory_read_u8(&a->memory,owner+i)+game_memory_read_u8(&a->memory,owner+0x11834+i);
    for(i=0;i<4;i++)sum+=get(a,owner+0x1800+i*4);
    if(get(a,owner+0x1814))sum+=terminal_sum(a,get(a,owner+0x1814))+terminal_sum(a,get(a,owner+0x1818));
    return sum+get(a,owner+0x11830)+get(a,owner+0x12034)+get(a,owner+0x12038);
}
static uint32_t work_checksum(GameApplication *a,GameAddress owner){
    uint32_t sum=0;unsigned i;
    for(i=1;i<8192;i++)sum+=game_memory_read_u8(&a->memory,owner+i);
    for(i=0;i<10;i++)sum+=get(a,owner+0x2000+i*4);
    if(get(a,owner+0x202c))sum+=terminal_sum(a,get(a,owner+0x202c))+terminal_sum(a,get(a,owner+0x2030));
    return sum;
}
static int checksum_method_depth(GameApplication *,GameAddress,GameAddress,uint32_t *,unsigned);
/*42dfb0 walks the existing intrusive list past retired records before any
 *virtual consumer can inspect their tables. Pool records may already be reset. */
static int checksum_next_live(GameApplication *a,GameAddress node,GameAddress *out,unsigned *visits){
    GameAddress method=get(a,get(a,node)+0x50);
    if(method!=0x42dfb0){
        fprintf(stderr,"native checksum next rejected: object=%08x method=%08x\n",node,method);return 0;
    }
    node=get(a,node+0x48);
    while(node){
        if(a->memory.fault || ++*visits>65536)return 0;
        if(get(a,node+0x58))break;
        node=get(a,node+0x48);
    }
    *out=node;return !a->memory.fault;
}
/*47a480: weighted upgrades, shared navigation state, root, squads and all
 *three owned scene lists. Keep unsigned wrap and signed16 member counts. */
static int root_checksum(GameApplication *a,GameAddress player,GameAddress method,uint32_t *out,unsigned depth){
    uint32_t sum=0,value;unsigned i,visits=0;GameAddress node;
    if(!a || !out || a->memory.fault)return 0;
    if(method==0x47a480){
        for(i=0;i<17;i++)sum+=get(a,player+0x29c+i*4)*i;
        sum+=visibility_checksum(a,0x5cf038)+work_checksum(a,0x60a4c8);
        node=get(a,player+0x2e0);
        if(!checksum_method_depth(a,node,get(a,get(a,node)+0x64),&value,depth+1))return 0;
        sum+=value;
    }else if(method==0x478540){
        for(i=0;i<13;i++)sum+=get(a,player+0x1fc+i*4)*i;
    }
    for(node=(method==0x4764b0 || method==0x478540)?0:get(a,player+0x294);node;node=get(a,node+0x64)){
        uint16_t bits=game_memory_read_u16(&a->memory,node+0x24);int16_t count;GameAddress members=get(a,node+0x18);
        memcpy(&count,&bits,2);if(++visits>65536)return 0;sum+=get(a,node+0x14);
        for(i=0;(int)i<count;i++){
            GameAddress member=get(a,members+i*4);if(!member)continue;
            if(!checksum_method_depth(a,member,get(a,get(a,member)+0x40),&value,depth+1))return 0;
            sum+=value;
        }
    }
    for(i=0;i<3;i++){
        static const unsigned heads[3]={0x1c,0x80,0xe4};
        GameAddress head=player+heads[i];
        if(!checksum_next_live(a,head,&node,&visits))return 0;
        while(node){
            if(!checksum_method_depth(a,node,get(a,get(a,node)+0x64),&value,depth+1))return 0;
            sum+=value;if(!checksum_next_live(a,node,&node,&visits))return 0;
        }
    }
    if(method==0x47bd70){
        static const unsigned children[3]={0x3bc,0x448,0x450};
        static const unsigned fields[]={0x148,0x14c,0x57c,0x588,0x3c8,0x22c,0x444,0x4b4,0x4b8,0x4bc,0x4c0,0x500,0x504,0x508,0x50c,0x510,0x514};
        for(i=0;i<3;i++){
            node=get(a,player+children[i]);
            if(!checksum_method_depth(a,node,get(a,get(a,node)+0x64),&value,depth+1))return 0;
            sum+=value;
        }
        for(i=0;i<sizeof(fields)/sizeof(fields[0]);i++)sum+=get(a,player+fields[i]);
        sum+=get(a,0x597f7c)+get(a,0x6297b8)+get(a,0x5ad8ac);
        for(i=0;i<32;i++)sum+=get(a,player+0x29c+i*4)*i;
    }
    *out=sum;return !a->memory.fault;
}
static int checksum_method_depth(GameApplication *a,GameAddress object,GameAddress method,uint32_t *out,unsigned depth){
    unsigned i;uint32_t sum;int16_t angle;uint16_t bits;
    if(depth>64 || !a || !out || a->memory.fault)return 0;
    if(method==0x47a480 || method==0x478810 || method==0x4764b0 || method==0x478540 || method==0x47bd70)
        return root_checksum(a,object,method,out,depth);
    if(method==0x53e830 || method==0x53f1d0 || method==0x53fca0){
        GameAddress child=get(a,object+0x48);
        return checksum_method_depth(a,child,get(a,get(a,child)+0x64),out,depth+1);
    }
    if(method==0x53ff50){*out=0;return 1;} /*Original empty base contribution. */
    if(method==0x4c0ae0){
        static const unsigned fields[]={0x140,0x1a4,0x1c4,0x2b4,0x2b8,0x2bc,0x2c0,0x2c4,0xd98};uint32_t value;
        sum=0;
        for(i=0;i<sizeof(fields)/sizeof(fields[0]);i++)sum+=get(a,object+fields[i]);
        for(i=0;i<14;i++)sum+=game_memory_read_u8(&a->memory,object+0x1a8+i)+game_memory_read_u8(&a->memory,object+0x1b6+i);
        if(!checksum_method_depth(a,object+0x1c8,get(a,get(a,object+0x1c8)),&value,depth+1))return 0;
        *out=sum+value;return !a->memory.fault;
    }
    if(method==0x442990){
        sum=0;for(i=4;i<=0x3c;i+=4)sum+=get(a,object+i);
        for(i=0x78;i<=0x8c;i+=4)if(i!=0x84)sum+=get(a,object+i);
        *out=sum;return !a->memory.fault;
    }
    if(method==0x45e510){
        static const unsigned fields[]={0xd8,0xb8,0xb4,0xd4,0xd0,0xbc,0xc0,0xc4};
        if(!checksum_method_depth(a,object,0x42ec40,&sum,depth+1))return 0;
        for(i=0;i<sizeof(fields)/sizeof(fields[0]);i++)sum+=get(a,object+fields[i]);
        bits=game_memory_read_u16(&a->memory,object+0xc8);memcpy(&angle,&bits,2);
        *out=sum+(uint32_t)(int32_t)angle;return !a->memory.fault;
    }
    if(method==0x4f8a40){
        static const unsigned words[]={0x498,0x1ac,0x1b0,0x1b4},shorts[]={0x26c,0x224,0x1b8};
        if(!checksum_method_depth(a,object,0x42ec40,&sum,depth+1))return 0;
        for(i=0;i<sizeof(words)/sizeof(words[0]);i++)sum+=get(a,object+words[i]);
        for(i=0;i<sizeof(shorts)/sizeof(shorts[0]);i++){
            bits=game_memory_read_u16(&a->memory,object+shorts[i]);memcpy(&angle,&bits,2);sum+=(uint32_t)(int32_t)angle;
        }
        *out=sum;return !a->memory.fault;
    }
    if(method==0x439630){
        if(!checksum_method_depth(a,object,0x42ec40,&sum,depth+1))return 0;
        for(i=0;i<2;i++){
            bits=game_memory_read_u16(&a->memory,object+0x2e8+i*2);memcpy(&angle,&bits,2);sum+=(uint32_t)(int32_t)angle;
        }
        *out=sum+get(a,object+0x2f4)+get(a,object+0x2f8);return !a->memory.fault;
    }
    /*Unknown IDs fail explicitly; they never enter generated execution. */
    if(method!=0x42ec40 && method!=0x4e8bd0 && method!=0x554880){
        fprintf(stderr,"native checksum method rejected: object=%08x method=%08x depth=%u\n",object,method,depth);return 0;
    }
    sum=get(a,object+0x14);
    for(i=0;i<3;i++){bits=game_memory_read_u16(&a->memory,object+0x24+i*2);memcpy(&angle,&bits,2);sum+=(uint32_t)(int32_t)angle+get(a,object+0x18+i*4);}
    if(method!=0x42ec40){
        for(i=0;i<3;i++){bits=game_memory_read_u16(&a->memory,object+0x148+i*2);memcpy(&angle,&bits,2);sum+=(uint32_t)(int32_t)angle;}
        if(method==0x554880){
            for(i=0;i<4;i++)sum+=get(a,object+0x29c+i*4);
            bits=game_memory_read_u16(&a->memory,object+0x2ac);memcpy(&angle,&bits,2);sum+=(uint32_t)(int32_t)angle;
        }
    }
    *out=sum;return !a->memory.fault;
}
int game_level_checksum_method(GameApplication *a,GameAddress object,GameAddress method,uint32_t *out){
    return checksum_method_depth(a,object,method,out,0);
}
int game_level_player_checksum(GameApplication *a,GameAddress player,uint32_t *out){
    return game_level_checksum_method(a,player,0x47a480,out);
}
static EffectResource *packet(GameApplication *a,GameAddress clock,GameAddress id){
    EffectResource *r=effect_resource_find(id);
    return r && !r->in_pool && r->owner->object_kind==4 && r->owner->projection==get(a,clock+4) &&
        r->clock_length<=r->clock_capacity?r:NULL;
}
static uint8_t *bytes(EffectResource *r){return r && r->clock_capacity?scene_projection_storage_borrow_bytes(r->clock_buffer,r->clock_capacity):NULL;}
static int send_bytes(GameApplication *a,uint32_t destination,const uint8_t *data,uint32_t size){
    uint32_t status=game_network_send_result(a,get(a,0x5f0038),destination,data,size);char message[80];int length=0;
    switch(status){
    case 0x80070057:length=snprintf(message,sizeof(message),"Invalid params %d\n",signed_word(status));break;
    case 0x88770082:length=snprintf(message,sizeof(message),"Invalid object %d\n",signed_word(status));break;
    case 0x88770096:length=snprintf(message,sizeof(message),"Invalid player %d %d\n",size?data[0]:0,signed_word(status));break;
    case 0x8877010e:length=snprintf(message,sizeof(message),"busy %d\n",signed_word(status));break;
    default:break;
    }
    if(length>0 && (unsigned)length<sizeof(message))game_memory_write(&a->memory,0x5a3888,message,(size_t)length+1);
    return !a->memory.fault;
}
static int send_packet(GameApplication *a,GameAddress clock,GameAddress id,uint32_t destination){
    EffectResource *r=packet(a,clock,id);uint8_t *data=bytes(r);
    return data && send_bytes(a,destination,data,r->clock_length);
}
/*440880: compute the local simulation checksum and exchange it with the host.
 *49ca00 replaces its accumulator on every iteration, returning only the last
 *little-endian word. Preserve that quirk rather than inventing a stronger hash. */
int game_level_network_tick(GameApplication *a){
    GameAddress clock,root,stamp,check;uint32_t sum,value,tick;int32_t index;unsigned i;
    uint8_t message[16]={0};
    if(!a || a->memory.fault)return 0;
    clock=get(a,0x5accd4);
    if(get(a,clock+0x14) || get(a,0x5ac970))return !a->memory.fault;
    sum=get(a,0x6308c0);
    for(i=0;i<6;i++){
        root=get(a,0x5f0210+i*4);
        if(!get(a,root+0x14))continue;
        if(!game_level_checksum_method(a,root,get(a,get(a,root)+8),&value))return 0;
        sum+=value;
    }
    sum+=get(a,0x6024ec);tick=get(a,0x5ac6ac);
    if(get(a,0x5f0150)){
        root=get(a,0x5f0208);message[0]='H';
        wire_word(message+4,game_memory_read_u16(&a->memory,root+8));
        wire_word(message+8,tick);wire_word(message+12,sum);
        return !a->memory.fault && send_bytes(a,0,message,sizeof(message));
    }
    index=signed_word((tick>>7)|((tick&0x80000000u)?0xfe000000u:0))%8;
    stamp=0x5ad9c8+(uint32_t)index*4;check=0x5ad9e8+(uint32_t)index*4;
    if(get(a,stamp)!=tick){put(a,stamp,tick);put(a,check,sum);}
    else if(get(a,check)!=sum){
        int previous=(int)get(a,0x597f80);
        printf("Bad Checksum at tick %d: one side: %d, other: %d\n",signed_word(tick),signed_word(sum),signed_word(get(a,check)));
        if(!game_level_display_visibility(a,1))return 0;
        (void)game_application_error(a,"Bad Checksum","Bad Checksum");
        if(!game_level_display_visibility(a,previous) || !game_level_request_end(a))return 0;
    }
    return !a->memory.fault;
}
int game_level_pause_network(GameApplication *a){
    if(!a || a->memory.fault)return 0;
    game_level_pause_mark(a);return !a->memory.fault;
}
static uint32_t player_lane(GameApplication *a,uint32_t sender){unsigned i;
    for(i=0;i<4;++i){GameAddress root=get(a,0x5f0210+i*4);if(root && get(a,root+0x3c0)==sender)return i;}
    return UINT32_MAX;
}
static int raw_append(GameApplication *a,GameAddress clock,GameAddress id){GameAddress tail=get(a,clock+0x20);
    if(tail)put(a,tail+4,id);else put(a,clock+0x1c,id);
    put(a,id+4,0);put(a,clock+0x20,id);return !a->memory.fault;
}
static int system_publish(GameApplication *a,GameAddress clock,GameAddress id,const uint8_t *data,uint32_t size){
    GameNetworkSystemEvent event;EffectResource *r=packet(a,clock,id);uint8_t *record=bytes(r);
    if(!record || r->clock_capacity<12 || !game_network_decode_clock_system(data,size,&event))return 0;
    wire_word(record,event.type);wire_word(record+4,event.player_type);wire_word(record+8,event.player_id);
    put(a,id+16,12);put(a,id+24,UINT32_MAX);return raw_append(a,clock,id);
}
static int chat(GameApplication *a,uint32_t lane,const uint8_t *data,uint32_t length){
    GameAddress root;char name[128],message[256];unsigned i;int count;
    if(lane>=4 || length>=128)return !a->memory.fault;
    root=get(a,0x5f0210+lane*4);if(!root || !get(a,root+0x14))return !a->memory.fault;
    if(!game_memory_write(&a->memory,root+0x31c,data,length))return 0;
    game_memory_write_u8(&a->memory,root+0x31c+length,0);
    for(i=0;i<sizeof(name);++i){name[i]=(char)game_memory_read_u8(&a->memory,root+0x39c+i);if(!name[i])break;}
    if(i==sizeof(name))return 0;
    /*Only display through the first wire NUL, exactly as the original %s. */
    count=snprintf(message,sizeof(message),"Message from \"%s\":\n%.*s",name,(int)length,(const char *)data);
    return count>=0 && (unsigned)count<sizeof(message) && game_application_timed_message_bytes(a,(const uint8_t *)message,(size_t)count,4);
}
/*43fd60: request missing sequence ranges, or periodically acknowledge a lane
 *with no gap. Timing uses signed wrapped words and the original float factor. */
int game_level_clock_request_missing(GameApplication *a,GameAddress clock){
    unsigned lane;uint8_t message[12]={0};float factor;uint32_t word=get(a,0x573020);memcpy(&factor,&word,4);
    for(lane=0;lane<4;++lane){GameAddress root=get(a,0x5f0210+lane*4),waiting=get(a,clock+0x4c+lane*4);
        uint32_t elapsed,last=get(a,clock+0x90+lane*4),sequence=get(a,clock+0x5c+lane*4);EffectResource *r;uint8_t *data;
        if(!root || !get(a,root+0x468) || root==get(a,0x5f0208))continue;
        elapsed=game_menu_milliseconds(a)-get(a,0x5ac90c);
        if(waiting){r=packet(a,clock,waiting);data=bytes(r);if(!data || r->clock_length<5)return 0;
            if((long double)signed_word(elapsed)<=signed_word(last)+(long double)signed_word(get(a,clock+8))*factor)continue;
            put(a,clock+0x90+lane*4,elapsed);message[0]='B';wire_word(message+4,sequence+1);wire_word(message+8,read_word(data+1)-1);
            if(!send_bytes(a,get(a,root+0x3c0),message,12))return 0;
        }else if(less(last+500,elapsed)){
            put(a,clock+0x90+lane*4,elapsed);message[0]='C';wire_word(message+4,sequence);
            if(!send_bytes(a,get(a,root+0x3c0),message,8))return 0;
        }
    }
    return !a->memory.fault;
}
/*43f070: raw protocol, retained retransmissions, acknowledgments and checksum
 *termination. SDK messages arrive as pointer-free native system events. */
int game_level_clock_raw_packets(GameApplication *a,GameAddress clock){
    GameAddress id=get(a,clock+0x1c),next,root;unsigned visits=0,lane;uint32_t first,last,value,minimum;uint8_t *data;EffectResource *r;
    while(id){
        if(++visits>65536 || !(r=packet(a,clock,id)) || !(data=bytes(r)) || !r->clock_length)return 0;
        lane=r->clock_sequence;next=get(a,id+4);root=get(a,0x5f0208);
        if(lane==UINT32_MAX){
            if(r->clock_length<12)return 0;
            value=read_word(data);
            if(value==5 && read_word(data+4)==1 && get(a,0x5f0150))(void)game_network_player_destroy(a,read_word(data+8));
            else if(value==257){put(a,0x5f0150,1);put(a,0x5eff28,root);}
            id=next;continue;
        }
        if(lane>=4)return 0;
        switch(data[0]){
        case 'B':
            if(r->clock_length<12)return 0;first=read_word(data+4);last=read_word(data+8);value=first-1;
            if(less(get(a,clock+0x6c+lane*4),value))put(a,clock+0x6c+lane*4,value);
            {GameAddress retained=get(a,clock+0x24);unsigned walked=0;int found=0;
                while(retained){EffectResource *sent=packet(a,clock,retained);uint8_t *payload=bytes(sent);
                    if(++walked>65536 || !payload || sent->clock_length<5)return 0;
                    value=read_word(payload+1);if(value==first)found=1;
                    if(found){GameAddress destination=get(a,0x5f0210+lane*4);if(!destination || !send_packet(a,clock,retained,get(a,destination+0x3c0)))return 0;if(value==last)break;}
                    retained=get(a,retained+4);
                }}break;
        case 'C':
            if(r->clock_length<8)return 0;value=read_word(data+4);
            if(less(get(a,clock+0x6c+lane*4),value))put(a,clock+0x6c+lane*4,value);break;
        case 'D':
            if(r->clock_length<12)return 0;
            if(root && !get(a,root+0x580)){value=read_word(data+4);if(value>=4)return 0;
                if(value!=game_memory_read_u16(&a->memory,root+8) && get(a,clock+0x5c+value*4)!=read_word(data+8)){
                    GameRootEventServices services=game_input_event_services(a);if(!game_root_signal_publish(&a->memory,&services,root,25))return 0;
                }}break;
        case 'E':if(get(a,0x5f0150) && !game_level_clock_pause_request(a,0,0))return 0;break;
        case 'F':if(get(a,0x5f0150) && !game_level_clock_pause_request(a,1,(int)get(a,clock+0xa0)))return 0;break;
        case 'H':
            if(r->clock_length<16)return 0;
            if(root && !get(a,root+0x580)){uint32_t tick=read_word(data+8),checksum=read_word(data+12);
                int32_t index=signed_word((tick>>7)|((tick&0x80000000u)?0xfe000000u:0))%8;
                GameAddress stamp=0x5ad9c8+(uint32_t)index*4,check=0x5ad9e8+(uint32_t)index*4;
                if(get(a,stamp)!=tick){put(a,stamp,tick);put(a,check,checksum);}
                else if(get(a,check)!=checksum){int previous=(int)get(a,0x597f80);
                    if(!game_level_display_visibility(a,1))return 0;
                    (void)game_application_error(a,"Bad Checksum","Bad Checksum");
                    if(!game_level_display_visibility(a,previous) || !game_level_request_end(a))return 0;
                }}break;
        default:break;
        }
        id=next;
    }
    id=get(a,clock+0x1c);
    if(id && !game_level_clock_queue_return_chain(a,get(a,clock+4),id))return 0;
    put(a,clock+0x1c,0);put(a,clock+0x20,0);
    id=get(a,clock+0x24);
    if(id){r=packet(a,clock,get(a,clock+0x28));data=bytes(r);if(!data || r->clock_length<5)return 0;minimum=read_word(data+1);
        root=get(a,0x5f0208);if(!root)return 0;
        for(lane=0;lane<4;++lane){GameAddress other=get(a,0x5f0210+lane*4);
            value=get(a,clock+0x6c+lane*4);
            if(other && get(a,other+0x468) && lane!=game_memory_read_u16(&a->memory,root+8) && less(value,minimum))minimum=value;
        }
        while((id=get(a,clock+0x24))!=0){r=packet(a,clock,id);data=bytes(r);if(!data || r->clock_length<5)return 0;
            if(less(minimum,read_word(data+1)))break;next=get(a,id+4);put(a,clock+0x24,next);put(a,id+4,0);
            if(!game_level_clock_queue_return(a,get(a,clock+4),id))return 0;
        }
        if(!get(a,clock+0x24))put(a,clock+0x28,0);
    }
    if(!game_level_clock_request_missing(a,clock))return 0;
    if(get(a,clock+0x14))for(id=get(a,clock+0x24),visits=0;id;id=get(a,id+4)){
        if(++visits>65536 || !send_packet(a,clock,id,0))return 0;
    }
    return !a->memory.fault;
}
/*43ff50: constrain the simulation to remote progress. A stalled player gets
 *the actual kind12 game-over event and is excluded from subsequent barriers. */
int game_level_clock_allowed_tick(GameApplication *a,GameAddress clock,uint32_t requested,uint32_t *result){
    unsigned lane=0,visits=0;uint32_t allowed=requested,deadline;
    if(!result || a->memory.fault)return 0;
    if(get(a,clock+0x14)){*result=requested;return !a->memory.fault;}
    while(lane<4){GameAddress root=get(a,0x5f0210+lane*4);
        if(++visits>32)return 0;
        if(root && get(a,root+0x468) && root!=get(a,0x5f0208) &&
           !(game_memory_read_u8(&a->memory,clock+0xa4)&(1u<<lane))){
            deadline=get(a,clock+0x80+lane*4);
            if(less((get(a,0x639968)>>5)+312,requested) && deadline && less(deadline+625,requested)){
                EffectResourcePool *events=effect_resource_pool_find(0x5cee40);GameAddress event;uint8_t message[12]={0};
                if(!events || !(event=effect_resource_pool_acquire_kind(&a->memory,events,12)))return 0;
                put(a,event+12,deadline+1);game_memory_write_u8(&a->memory,event+16,25);put(a,event+20,lane);
                if(!game_root_event_receive(&a->memory,root,event)){
                    (void)effect_resource_pool_return_chain(&a->memory,events,event);return 0;}
                put(a,root+0x584,1);
                game_memory_write_u8(&a->memory,clock+0xa4,(uint8_t)(game_memory_read_u8(&a->memory,clock+0xa4)|(1u<<lane)));
                if(get(a,0x5f0150)){message[0]='D';wire_word(message+4,lane);wire_word(message+8,get(a,clock+0x5c+lane*4));
                    if(!send_bytes(a,0,message,12))return 0;
                }else lane=0; /*The original resumes its barrier scan at lane0. */
            }
            deadline=get(a,clock+0x80+lane*4);if(less(deadline,allowed))allowed=deadline;
        }
        ++lane;
    }
    *result=allowed;return !a->memory.fault;
}
/*43ec60: receive into the pool's actual owned byte buffer. A native transient
 *allocation handles oversized SDK/chat messages; it never becomes a guest ID. */
int game_level_clock_receive(GameApplication *a,GameAddress clock){
    EffectResourcePool *pool=effect_resource_pool_find(get(a,clock+4));unsigned attempts=0;
    if(!pool || pool->object_kind!=4)return 0;
    for(;;){GameAddress id;EffectResource *r;uint8_t *data;uint32_t size,sender=0,receiver=0,status,lane;int keep=0,ok=1;
        if(++attempts>65536)return 0;
        id=effect_resource_pool_acquire_kind(&a->memory,pool,2);r=packet(a,clock,id);data=bytes(r);if(!data)return 0;
        size=r->clock_capacity;status=game_network_receive_result(a,&sender,&receiver,data,&size);
        if(status==0x8877001e){uint8_t *large;uint32_t capacity;
            if(!game_level_clock_queue_return(a,pool->projection,id))return 0;
            do{if(size==UINT32_MAX || !(large=calloc((size_t)size+1,1)))return 0;capacity=size;
                status=game_network_receive_result(a,&sender,&receiver,large,&size);
                if(!status && size<=capacity){if(!sender){GameAddress system=effect_resource_pool_acquire_kind(&a->memory,pool,2);
                        ok=system && system_publish(a,clock,system,large,size);
                        if(!ok && system)(void)game_level_clock_queue_return(a,pool->projection,system);}
                    else if(size && large[0]=='G')ok=chat(a,player_lane(a,sender),large+1,size-1);}
                free(large);if(!ok)return 0;
            }while(status==0x8877001e);
            continue;
        }
        if(!status){
            if(size>r->clock_capacity)ok=0;
            else if(!sender){ok=system_publish(a,clock,id,data,size);keep=ok;}
            else{lane=player_lane(a,sender);
                if(lane<4 && !(game_memory_read_u8(&a->memory,clock+0xa4)&(1u<<lane)) && size){
                    put(a,id+16,size);put(a,id+24,lane);
                    if(data[0]=='A'){ok=game_level_clock_received_packet(a,clock,id);keep=ok;}
                    else if(data[0]=='G')ok=chat(a,lane,data+1,size-1);
                    else{ok=raw_append(a,clock,id);keep=ok;}
                }
            }
        }
        if(!keep && !game_level_clock_queue_return(a,pool->projection,id))return 0;
        if(!ok || a->memory.fault)return 0;
        if(status==0x887700be)break;
        if(status && status!=0x88770096)return 0;
    }
    return game_level_clock_raw_packets(a,clock);
}
