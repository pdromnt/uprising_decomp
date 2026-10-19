#include "menu_network.h"
#include "game_initializers.h"
#include <string.h>
/*Shared packet algorithms use native payloads/IDs, never registers or guest
 * stack words. The transport owns copied events until this thread drains.*/
extern int game_network_game_packet(GameApplication *,const uint8_t *,uint32_t,uint32_t,int *);
extern int game_network_publish_initial_state(GameApplication *,uint32_t,uint32_t); /*473600*/
extern int game_network_publish_roster(GameApplication *,uint32_t,uint32_t,uint32_t); /*4732a0*/
extern int game_network_publish_chat(GameApplication *,uint32_t,uint32_t); /*473170*/
extern int game_network_apply_world(GameApplication *,const uint8_t *,uint32_t); /*473c20*/
extern int game_level_selection_load(GameApplication *);
extern int game_level_selection_presets(GameApplication *);
extern int game_menu_request(GameApplication *,GameAddress,uint32_t,uint32_t,uint32_t);
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t value){game_memory_write_u32(&a->memory,p,value);}
/*4a70c0: one shared reset, used by local selection and network startup. */
int game_menu_parameters_reset(GameApplication *a) {
    GameMemory *m=&a->memory;
    game_initializer_004a6df0(m);
    game_memory_fill_u32(m,0x602f78,10,0xffffffbb,0);
    game_memory_fill_u32(m,0x602fd4,17,0xffffffbb,0);
    game_memory_fill_u32(m,0x602fa0,13,0xffffffbb,0);
    return !m->fault;
}
static int name(GameApplication *a,GameAddress p,char *out,size_t capacity) {
    size_t i;for(i=0;i<capacity;++i){out[i]=(char)game_memory_read_u8(&a->memory,p+(uint32_t)i);
        if(!out[i])return !a->memory.fault;}return 0;
}
static int message(GameApplication *a,GameAddress body,GameAddress title) {
    char text[256],caption[64];if(!name(a,body,text,sizeof(text)) || !name(a,title,caption,sizeof(caption)))return 0;
    (void)game_application_error(a,text,caption);return !a->memory.fault;
}
static int managers_reset(GameApplication *a) {
    unsigned i;for(i=0;i<4;++i) {
        GameAddress p=get(a,0x5f0210+i*4);if(!p)return 0;
        put(a,p+0x3c0,UINT32_MAX);game_memory_write_u8(&a->memory,p+0x31c,0);
        put(a,p+0x468,0);put(a,p+0x4c4,0);put(a,p+0x46c,1);
    }
    return !a->memory.fault;
}
/*472250: initialize session selection without replacing existing managers. */
int game_network_session_initialize(GameApplication *a) {
    unsigned i;put(a,0x624f8c,1);game_memory_write_u8(&a->memory,0x5efe80,0);
    put(a,0x5eff28,get(a,0x5f0210));for(i=0;i<4;++i)put(a,0x5f0040+i*4,1);
    put(a,0x5f0150,0);put(a,0x5ac910,0);put(a,0x5acbec,0);return !a->memory.fault;
}
int game_menu_demo_available(GameApplication *a) {
    if(!game_network_lobby_open(a) || !game_network_session_initialize(a))return 0;
    put(a,0x5eff04,1);return game_menu_parameters_reset(a);
}
/*471e40/471ef0: convert capabilities through wrapped integer arithmetic. */
int game_network_update_capabilities(GameApplication *a,int choose_host) {
    GameNetworkCapabilities caps;
    if(!game_network_capabilities(a,&caps))return 0;
    put(a,0x5efe78,caps.hundred_baud*100u);put(a,0x5efee8,caps.latency?caps.latency:100);
    if(choose_host && (caps.flags&2))put(a,0x5f0150,1);
    return !a->memory.fault;
}
/*472410: one bounded receive batch. Transport system events and game messages
 * are pointer-free byte records. The actual consumers own roster/state changes.*/
int game_network_receive_batch(GameApplication *a,uint16_t *count) {
    union {long double real;void *pointer;uint64_t integer;uint8_t bytes[256];} packet;
    unsigned i;uint16_t received=0;
    if(!count)return 0;
    for(i=0;i<16;++i) {
        uint32_t sender=0,receiver=0,size=256;int accepted=0;
        int status=game_network_receive(a,&sender,&receiver,packet.bytes,&size);
        if(status<0){*count=UINT16_MAX;return 1;}
        if(!status)break;
        if(!sender) {
            if(!game_network_decode_system(a,packet.bytes,size))return 0;
            ++received;
        } else {
            if(!game_network_game_packet(a,packet.bytes,size,sender,&accepted))return 0;
            if(accepted)++received;
        }
    }
    *count=received;return !a->memory.fault;
}
/*472300: real handshake states and twenty-second unsigned timeout. */
uint32_t game_network_wait(GameApplication *a) {
    uint32_t start=game_network_milliseconds(a),state;uint16_t count;
    while((state=get(a,0x624f8c))!=5) {
        if(a->memory.fault)return 1;
        if(game_network_milliseconds(a)-start>20000)return 0;
        if(!game_network_receive_batch(a,&count) || count==UINT16_MAX)return 1;
        state=get(a,0x624f8c);if(state==3 || state==4)return state;
    }
    return 2;
}
static int connected(GameApplication *a) {
    unsigned i;put(a,0x5efee4,0);put(a,0x5efef8,0);
    for(i=0;i<4;++i)put(a,0x5eff10+i*4,0);
    return !a->memory.fault;
}
/*471f60: real player creation, host registration or client handshake. */
static int create_player(GameApplication *a) {
    static const GameAddress bodies[5]={0x5891f8,0x589240,0x589300,0x589280,0x5892c0};
    static const GameAddress titles[5]={0x5891e8,0x589230,0x5892f0,0x589270,0x5892b0};
    static const GameAddress sources[3]={0x597f7c,0x6297b8,0x5ad8ac};
    uint8_t data[16]={0};char player_name[60];uint32_t id,status;unsigned i,j;
    put(a,0x624f8c,2);
    for(i=0;i<3;++i){uint32_t value=get(a,sources[i]);for(j=0;j<4;++j)data[i*4+j]=(uint8_t)(value>>(j*8));}
    if(!name(a,0x5efec0,player_name,sizeof(player_name)) || a->memory.fault)return 0;
    if(!game_network_player_create(a,player_name,data,&id)) {
        (void)message(a,0x5891b8,0x5891a8);return 0;
    }
    put(a,0x5f0038,id);
    if(get(a,0x5f0150)) {
        GameAddress manager=get(a,0x5f0210);if(!manager)return 0;
        game_initializer_0047afd0(&a->memory,manager,id,0x5efec0,0);
        if(a->memory.fault)return 0;
        put(a,manager+0x46c,1);
        if(!game_level_selection_default_row(a,0))return 0;
        put(a,0x5f0208,manager);put(a,0x624f8c,5);return connected(a);
    }
    status=game_network_wait(a);if(status==2)return connected(a);
    if(!managers_reset(a))return 0;
    if(status>4)status=2;
    (void)message(a,bodies[status],titles[status]);return 0;
}
int game_menu_demo_start(GameApplication *a,unsigned choose_host) {
    /*The original probes this read-only file/stat but publishes no stat value. */
    char path[260];ResourceFile *file;
    if(!name(a,0x5eff38,path,sizeof(path)))return 0;
    file=resource_file_open(path);resource_file_close(file);put(a,0x5efef4,0);
    return game_network_update_capabilities(a,choose_host!=0) && create_player(a);
}
/*4722a0: close the session, retain transport ownership for later connections. */
int game_network_session_reset(GameApplication *a) {
    /*4722b5 ignores Close's HRESULT. Clock destruction already closes this
     *session; closing twice must not reject an otherwise clean return. */
    (void)game_network_transport_close(a);
    if(!managers_reset(a))return 0;
    game_memory_write_u8(&a->memory,0x5efe80,0);put(a,0x5f0208,0);put(a,0x5eff28,0);
    put(a,0x5f0150,0);put(a,0x624f8c,1);return !a->memory.fault;
}
static uint32_t wire_word(const uint8_t *p) {
    return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
static int64_t signed_word(uint32_t value){return value<0x80000000u?(int64_t)value:(int64_t)value-INT64_C(4294967296);}
static int reject_player(GameApplication *a,uint32_t player,uint8_t code) {
    GameAddress manager=get(a,0x5f0208);if(!manager)return 0;
    return game_network_send(a,get(a,manager+0x3c0),player,&code,1);
}
int game_network_manager_reset(GameApplication *a,GameAddress manager) {
    if(!manager)return 0;
    put(a,manager+0x3c0,UINT32_MAX);game_memory_write_u8(&a->memory,manager+0x31c,0);
    put(a,manager+0x468,0);put(a,manager+0x4c4,0);return !a->memory.fault;
}
/*4724b0: actual player creation/removal consumers. The transport decodes copied
 * byte records first; names and data are borrowed only during this call. */
int game_network_system_player(GameApplication *a,uint32_t kind,uint32_t player,const uint8_t *data,
        uint32_t bytes,const char *player_name) {
    unsigned i;GameAddress manager=0;uint32_t value,limit;
    if(kind==3) {
        if(!get(a,0x5f0150))return !a->memory.fault;
        if(!data || bytes<16)return 0;
        value=wire_word(data);limit=get(a,0x597f7c);
        put(a,0x597f7c,signed_word(value)<signed_word(limit)?value:limit);
        value=wire_word(data+4);limit=get(a,0x597f7c);
        put(a,0x6297b8,signed_word(value)<signed_word(limit)?value:limit);
        put(a,0x5ad8ac,get(a,0x5ad8ac)&wire_word(data+8));
        value=wire_word(data+12);limit=get(a,0x5efef4);
        if(value && limit && value!=limit)return reject_player(a,player,10);
        if(get(a,0x624f8c)!=5)return !a->memory.fault;
        for(i=0;i<4;++i) {
            manager=get(a,0x5f0210+i*4);if(!manager)return 0;
            if(!get(a,manager+0x468) && get(a,manager+0x46c))break;
        }
        if(i==4)return reject_player(a,player,9);
        if(!player_name)return 0;
        /*The verified name field has36 bytes before the player ID. */
        {size_t length=0;while(length<36 && player_name[length])++length;
         if(length==36 || !game_memory_write(&a->memory,manager+0x39c,player_name,length+1))return 0;}
        game_memory_write_u8(&a->memory,manager+0x31c,0);
        put(a,manager+0x468,1);put(a,manager+0x3c0,player);put(a,manager+0x46c,1);
        if(!game_level_selection_default_row(a,i) || !game_network_publish_initial_state(a,player,1) ||
           !game_network_publish_roster(a,0,i,1))return 0;
        put(a,0x5efee0,1);return !a->memory.fault;
    }
    if(kind!=5)return !a->memory.fault; /*Other system notifications are ignored in retail. */
    if(get(a,0x5f0150)) {
        for(i=0;i<4;++i) {
            manager=get(a,0x5f0210+i*4);if(!manager)return 0;
            if(get(a,manager+0x3c0)==player)break;
        }
        if(i>=4 || !i || !get(a,manager+0x468))return !a->memory.fault;
        (void)game_network_player_destroy(a,player);
        if(!game_network_manager_reset(a,manager))return 0;
        put(a,0x5efee0,1);return !a->memory.fault;
    }
    manager=get(a,0x5eff28);
    return !manager || get(a,manager+0x3c0)!=player || game_network_manager_reset(a,manager);
}
static int wire_string(GameApplication *a,GameAddress target,const uint8_t *data,uint32_t size,size_t capacity) {
    const uint8_t *end=memchr(data,0,size);size_t length;
    if(!end)return 0;
    length=(size_t)(end-data);
    return length<capacity && game_memory_write(&a->memory,target,data,length+1);
}
static uint32_t wire_short(const uint8_t *data) {
    uint32_t value=(uint32_t)data[0]|((uint32_t)data[1]<<8);
    return value<0x8000?value:value|0xffff0000u;
}
static int register_wire_player(GameApplication *a,GameAddress manager,const uint8_t *data,uint32_t size) {
    if(!wire_string(a,manager+0x39c,data+1,size-1,36))return 0;
    game_memory_write_u8(&a->memory,manager+0x31c,0);
    put(a,manager+0x468,1);put(a,manager+0x3c0,wire_word(data+0x24));put(a,manager+0x46c,1);
    return !a->memory.fault;
}
/*473db0: unpack the actual120-byte selection packet into its owned faction
 * row. Kind3 also updates the original committed-selection bank, whose distinct
 * game-start semantics already exist; no new serialized state owner is added. */
static int apply_selection(GameApplication *a,const uint8_t *data,uint32_t size,unsigned index,int commit) {
    GameAddress manager,row;unsigned i;if(index>=4 || size<120)return 0;
    manager=get(a,0x5f0210+index*4);row=0x6027f0+index*240;
    if(!manager || !wire_string(a,manager+0x39c,data+1,size-1,36))return 0;
    put(a,manager+0x4c4,wire_short(data+0x32));put(a,row+0x80,wire_word(data+0x2c));put(a,row+0x84,data[0x30]);
    for(i=0;i<4;++i)put(a,row+0x88+i*4,data[0x34+i]==255?0xffffffbb:data[0x34+i]);
    for(i=0;i<32;++i) {
        uint32_t value=wire_short(data+0x38+i*2);put(a,row+i*4,value==255?0xffffffbb:value);
    }
    if(commit)for(i=0;i<38;++i)put(a,0x602bb8+index*240+i*4,get(a,row+i*4));
    return !a->memory.fault;
}
static uint8_t roster_mask(GameApplication *a) {
    unsigned i;uint8_t mask=0;
    for(i=0;i<4;++i){GameAddress manager=get(a,0x5f0210+i*4);
        if(get(a,manager+0x468))mask|=(uint8_t)(1u<<i);
        if(get(a,manager+0x46c))mask|=(uint8_t)(1u<<(i+4));}
    return mask;
}
/*472720: all twelve wire commands retain their real state gates, consumers,
 * forwarding and notifications. Unconverted world/snapshot publishers stay
 * strong native dependencies, never successful placeholder callbacks. */
int game_network_game_packet(GameApplication *a,const uint8_t *data,uint32_t size,uint32_t sender,int *accepted) {
    uint32_t kind,index,state;GameAddress manager,local;unsigned i;
    if(!data || !accepted)return 0;
    *accepted=0;if(!size)return !a->memory.fault;
    kind=data[0];if(kind<1 || kind>12)return !a->memory.fault;
    state=get(a,0x624f8c);
    if(kind<=3) {
        if(kind>1 && state!=5){*accepted=1;return !a->memory.fault;}
        if(size<120 || data[0x28]>=4)return 0;
        index=data[0x28];manager=get(a,0x5f0210+index*4);if(!manager)return 0;
        if(kind==1) {
            if(get(a,manager+0x468)){*accepted=1;return !a->memory.fault;}
            if(!register_wire_player(a,manager,data,size) || !game_level_selection_default_row(a,index) ||
               !apply_selection(a,data,size,index,0))return 0;
            if(wire_word(data+0x24)==get(a,0x5f0038)){put(a,0x624f8c,5);put(a,0x5f0208,manager);}
        } else {
            if(kind==3)put(a,0x5eff10+index*4,1);
            if(!get(a,0x5f0150) && !get(a,manager+0x468) &&
               (!register_wire_player(a,manager,data,size) || !game_level_selection_default_row(a,index)))return 0;
            index=game_memory_read_u16(&a->memory,manager+8);
            if(!apply_selection(a,data,size,index,kind==3))return 0;
            if(get(a,0x5f0150) && !game_network_publish_roster(a,0,data[0x28],2))return 0;
            local=get(a,0x5f0208);
            /*The original forwarding publisher rewrites the shared packet kind
             * to2 before this gate, including a host's incoming kind3. */
            if((kind==2 || get(a,0x5f0150)) && local && game_memory_read_u16(&a->memory,local+8)==game_memory_read_u16(&a->memory,manager+8))put(a,0x589190,1);
        }
    } else if(kind==4) {
        if(state!=5){*accepted=1;return !a->memory.fault;}
        if(size<3 || data[1]>=4)return 0;
        index=data[1];manager=get(a,0x5f0210+index*4);if(!manager)return 0;
        if(get(a,manager+0x468)) {
            if(!wire_string(a,manager+0x31c,data+2,size-2,128))return 0;
            if(get(a,0x5f0150) && !game_network_publish_chat(a,0,index))return 0;
        }
    } else if(kind==5 || kind==7) {
        char previous[60];const uint8_t *end;
        if(state!=5)return !a->memory.fault;
        if(kind==7)put(a,0x5efef8,1);
        if(size<2 || !(end=memchr(data+1,0,size-1)) || (size_t)(end-data-1)>=sizeof(previous) ||
           !name(a,0x5efe80,previous,sizeof(previous)))return 0;
        if(strcmp(previous,(const char *)data+1)) {
            if(!wire_string(a,0x5efe80,data+1,size-1,60) || !game_level_selection_load(a))return 0;
            put(a,0x602bb0,0);if(!game_level_selection_presets(a))return 0;
        }
    } else if(kind==6 || kind==8) {
        if(state!=5){*accepted=1;return !a->memory.fault;}
        if(kind==8)put(a,0x5efee4,1);
        if(!game_network_apply_world(a,data,size))return 0;
    } else if(kind==9 || kind==10)put(a,0x624f8c,kind==9?3:4);
    else if(kind==11) {
        if(size<2)return 0;
        for(i=0;i<4;++i) {
            manager=get(a,0x5f0210+i*4);if(!manager)return 0;
            if(!(data[1]&(1u<<i)) && get(a,manager+0x468)) {
                if(!game_network_manager_reset(a,manager))return 0;
                if(manager==get(a,0x5f0208) && !game_menu_request(a,get(a,0x59d4c0),69,1000,14))return 0;
            }
            put(a,manager+0x46c,!!(data[1]&(1u<<(i+4))));
        }
    } else {
        uint8_t mask,response[2];if(size<2)return 0;mask=roster_mask(a);
        if(mask!=data[1]) {
            local=get(a,0x5f0208);if(!local)return 0;
            response[0]=11;response[1]=mask;
            /*The original logs a failed repair send and still tries all rows. */
            (void)game_network_send(a,get(a,local+0x3c0),sender,response,2);
            for(i=0;i<4;++i)if(!(data[1]&(1u<<i)) && (mask&(1u<<i)) &&
                !game_network_publish_roster(a,sender,i,2))return 0;
        }
    }
    *accepted=1;return !a->memory.fault;
}
static void pack_word(uint8_t *out,uint32_t value) {
    unsigned i;for(i=0;i<4;++i)out[i]=(uint8_t)(value>>(i*8));
}
/*473cd0: explicit little-endian wire codec, preserving byte/short truncation
 * and the two different absent sentinels. Padding is initialized, not executable
 * state; no host layout, alignment or aliasing assumptions enter the packet. */
int game_network_selection_pack(GameApplication *a,uint8_t out[120],uint32_t index) {
    char player[36];GameAddress manager,row;uint32_t value;unsigned i;
    if(!out || index>=4)return 0;
    manager=get(a,0x5f0210+index*4);row=0x6027f0+index*240;
    if(!manager || !name(a,manager+0x39c,player,sizeof(player)))return 0;
    memcpy(out+1,player,strlen(player)+1);
    value=get(a,manager+0x4c4);out[0x32]=(uint8_t)value;out[0x33]=(uint8_t)(value>>8);
    pack_word(out+0x24,get(a,manager+0x3c0));out[0x28]=(uint8_t)index;
    pack_word(out+0x2c,get(a,row+0x80));out[0x30]=(uint8_t)get(a,row+0x84);
    for(i=0;i<4;++i){value=get(a,row+0x88+i*4);out[0x34+i]=value==0xffffffbb?255:(uint8_t)value;}
    for(i=0;i<32;++i){value=get(a,row+i*4);if(value==0xffffffbb)value=255;
        out[0x38+i*2]=(uint8_t)value;out[0x39+i*2]=(uint8_t)(value>>8);}
    return !a->memory.fault;
}
/*473c20: capability/budget and ten global upgrades from the60-byte packet. */
int game_network_apply_world(GameApplication *a,const uint8_t *data,uint32_t size) {
    unsigned i;if(!data || size<60)return 0;
    if(data[0]!=8) {
        uint32_t budget=wire_word(data+0x2c),delta=budget-get(a,0x5ac910);
        if(delta) {
            uint32_t balance=get(a,0x5acbec)+delta;put(a,0x5acbec,balance);put(a,0x5ac910,budget);
            if(!get(a,0x5f0150) && signed_word(balance)<0) {
                GameAddress manager=get(a,0x5f0208);if(!manager)return 0;
                put(a,0x5acbec,budget);
                if(!game_level_selection_default_row(a,game_memory_read_u16(&a->memory,manager+8)))return 0;
                put(a,0x5f003c,1);
            }
        }
    }
    for(i=0;i<10;++i)put(a,0x602f78+i*4,wire_word(data+4+i*4));
    if(data[0]==8){put(a,0x597f7c,wire_word(data+0x30));put(a,0x6297b8,wire_word(data+0x34));put(a,0x5ad8ac,wire_word(data+0x38));}
    return !a->memory.fault;
}
static int send_local(GameApplication *a,uint32_t to,const uint8_t *data,uint32_t size) {
    GameAddress manager=get(a,0x5f0208);if(!manager)return 0;
    return !a->memory.fault && game_network_send(a,get(a,manager+0x3c0),to,data,size);
}
int game_network_publish_roster(GameApplication *a,uint32_t to,uint32_t index,uint32_t kind) {
    uint8_t packet[120]={0};GameAddress manager;if(index>=4)return 0;
    manager=get(a,0x5f0210+index*4);if(!manager)return 0;
    if(!get(a,manager+0x468))return !a->memory.fault;
    packet[0]=(uint8_t)kind;
    return game_network_selection_pack(a,packet,index) && send_local(a,to,packet,sizeof(packet));
}
int game_network_publish_chat(GameApplication *a,uint32_t to,uint32_t index) {
    uint8_t packet[130]={0};char text[128];GameAddress manager;if(index>=4)return 0;
    manager=get(a,0x5f0210+index*4);if(!manager)return 0;
    if(!get(a,manager+0x468))return !a->memory.fault;
    if(!name(a,manager+0x31c,text,sizeof(text)))return 0;
    packet[0]=4;packet[1]=(uint8_t)index;memcpy(packet+2,text,strlen(text)+1);
    return send_local(a,to,packet,sizeof(packet));
}
int game_network_publish_world(GameApplication *a,uint32_t to,uint8_t kind) {
    uint8_t packet[60]={0};unsigned i;packet[0]=kind;
    pack_word(packet+0x2c,get(a,0x5ac910));for(i=0;i<10;++i)pack_word(packet+4+i*4,get(a,0x602f78+i*4));
    if(kind==8){pack_word(packet+0x30,get(a,0x597f7c));pack_word(packet+0x34,get(a,0x6297b8));pack_word(packet+0x38,get(a,0x5ad8ac));}
    return !a->memory.fault && send_local(a,to,packet,sizeof(packet));
}
/*473600: publish active rows, occupancy, level name, shared upgrades and chat.
 * Initial row-send failure aborts; secondary notifications are attempted in the
 * original order, retaining the reference's best-effort behavior. */
int game_network_publish_initial_state(GameApplication *a,uint32_t to,uint32_t kind) {
    uint8_t packet[120]={0},roster[2]={11,0},level[65]={0};char level_name[64];unsigned i;
    for(i=0;i<4;++i) {
        GameAddress manager=get(a,0x5f0210+i*4);if(!manager)return 0;
        if(get(a,manager+0x468)) {
            packet[0]=(uint8_t)kind;
            if(!game_network_selection_pack(a,packet,i) ||
               !game_network_send(a,get(a,0x5f0038),to,packet,sizeof(packet)))return 0;
        }
    }
    roster[1]=roster_mask(a);(void)send_local(a,to,roster,sizeof(roster));
    if(!name(a,0x5efe80,level_name,sizeof(level_name)))return 0;
    level[0]=kind==3?7:5;memcpy(level+1,level_name,strlen(level_name)+1);
    (void)send_local(a,to,level,sizeof(level));(void)game_network_publish_world(a,to,kind==3?8:6);
    if(kind==1)for(i=0;i<4;++i)(void)game_network_publish_chat(a,to,i);
    return !a->memory.fault;
}
