#include "gameplay_input.h"
#include "menu_sound.h"
#include "player_state.h"
#include "effect_resource_pool.h"
#include <string.h>
/*The multiplayer observer remains a distinct native link dependency.
 * Queue identities, vehicle/overhead producers and weight snapshots run C. */
extern uint32_t game_menu_milliseconds(GameApplication *);
extern int game_level_clock_observe_event(GameApplication *,GameAddress);
extern int game_input_message(GameApplication *,GameAddress); /*52d530, actual message owner. */
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static int clock_now(void *c,uint32_t *v){*v=game_menu_milliseconds(c);return 1;}
static int notify(void *c,GameAddress event){return game_level_clock_observe_event(c,event);}
static int identity(GameApplication *a,GameAddress queue,uint32_t *v) {
    uint32_t method=get(a,get(a,queue)+0x1c);
    if(a->memory.fault)return 0;
    switch(method){
    case 0x4f9b90:*v=get(a,queue-0x128);break;
    case 0x5549d0:*v=get(a,queue-0x250);break;
    default:a->memory.fault=1;return 0;
    }
    return !a->memory.fault;
}
static int delta(void *c,GameAddress queue,uint32_t tick,uint32_t *changed) {
    GameApplication *a=c;GamePlayerNetworkDelta d;GameEventPacket packet={0};uint32_t method;
    (void)tick;*changed=0;method=get(a,get(a,queue)+0x14);
    if(a->memory.fault)return 0;
    if(method==0x5527a0){
        uint32_t health=get(a,queue-0x20c);int32_t signed_health;uint16_t first,second;
        memcpy(&signed_health,&health,4);if(signed_health<=0)return !a->memory.fault;
        first=game_memory_read_u16(&a->memory,queue-0xac);second=game_memory_read_u16(&a->memory,queue-0xaa);
        if(first==game_memory_read_u16(&a->memory,queue-0x11a) && second==game_memory_read_u16(&a->memory,queue-0x118))return !a->memory.fault;
        if(a->memory.fault || !game_input_words_publish(a,queue,23,first,second))return 0;
        *changed=1;return 1;
    }
    if(method!=0x4f8860){a->memory.fault=1;return 0;}
    if(!game_player_build_network_delta(&a->memory,queue,&d))return 0;
    *changed=d.ready;if(!d.ready)return 1;
    packet.compact=1;packet.code=d.route;packet.mask=d.mask;packet.count=d.count;
    memcpy(packet.values,d.values,(size_t)d.count*sizeof(d.values[0]));
    if(!game_input_packet_publish(a,queue,&packet))return 0;
    if(d.float_update){packet.compact=0;packet.code=13;memcpy(packet.words,d.float_words,sizeof(packet.words));
        if(!game_input_packet_publish(a,queue,&packet))return 0;}
    return !a->memory.fault;
}
GameRootEventServices game_input_event_services(GameApplication *a){GameRootEventServices s={a,clock_now,delta,notify};return s;}
int game_input_root_events(GameApplication *a,GameAddress root,uint32_t tick){
    GameRootEventServices s=game_input_event_services(a);return game_root_events_produce(&a->memory,&s,root,tick);
}
int game_input_packet_publish(GameApplication *a,GameAddress queue,GameEventPacket *packet) {
    GameAddress event;uint32_t id;
    packet->deadline=(game_menu_milliseconds(a)-get(a,0x5ac90c))>>5;
    if(!identity(a,queue,&id))return 0;
    packet->identity=id;
    if(!game_event_packet_publish(&a->memory,queue,packet,&event))return 0;
    return (!get(a,0x5eff04) || notify(a,event)) && !a->memory.fault;
}
/*558cb0 uses kind0 in the type2 pool, not the float-pair publisher's kind1.
 * Keep untouched payload bytes untouched when recycling the original owner. */
static int small_packet_publish(GameApplication *a,GameAddress queue,uint8_t code,const uint8_t *bytes,unsigned size) {
    EffectResourcePool *pool=effect_resource_pool_find(0x5cef90);EffectResource *record;
    GameAddress event;uint32_t id;unsigned i;
    if(!pool || a->memory.fault || !identity(a,queue,&id))return 0;
    event=effect_resource_pool_acquire_kind(&a->memory,pool,0);record=effect_resource_find(event);
    if(!record)return 0;
    record->deadline=(game_menu_milliseconds(a)-get(a,0x5ac90c))>>5;record->event_code=code;
    for(i=0;i<4;++i)record->event_padding[i+2]=(uint8_t)(id>>(i*8));
    for(i=0;i<size;++i)game_memory_write_u8(&a->memory,event+24+i,bytes[i]);
    if(a->memory.fault || !game_event_queue_insert_at(&a->memory,queue+0x28,queue+0x2c,event))goto failed;
    return (!get(a,0x5eff04) || notify(a,event)) && !a->memory.fault;
failed:effect_resource_pool_return_chain(&a->memory,pool,event);return 0;
}
int game_input_byte_publish(GameApplication *a,GameAddress queue,uint8_t code,uint8_t value){return small_packet_publish(a,queue,code,&value,1);}
/*558d20: two sixteen-bit aim values use the same kind0 owner as byte commands. */
int game_input_words_publish(GameApplication *a,GameAddress queue,uint8_t code,uint16_t first,uint16_t second){
    uint8_t bytes[4]={(uint8_t)first,(uint8_t)(first>>8),(uint8_t)second,(uint8_t)(second>>8)};
    return small_packet_publish(a,queue,code,bytes,4);
}
static int button(void *c,GameAddress queue,uint8_t code){
    GameApplication *a=c;uint32_t id;GameRootEventServices s=game_input_event_services(a);
    return identity(a,queue,&id) && game_input_button_publish(&a->memory,&s,queue,code,id);
}
int game_input_queue_button(GameApplication *a,GameAddress queue,uint8_t code){return button(a,queue,code);}
static int floats(void *c,GameAddress queue,uint32_t first,uint32_t second){
    GameEventPacket packet={0};packet.code=13;packet.words[0]=first;packet.words[1]=second;
    return game_input_packet_publish(c,queue,&packet);
}
static int feedback(void *c,uint32_t sound,GameAddress message){
    GameApplication *a=c;
    return (!sound || game_menu_sound(a,sound)) && (!message || game_input_message(a,message)) && !a->memory.fault;
}
static int palette(void *c){
    GameApplication *a=c; /*4b2d50 retains the actual weight-selector cancel snapshot. */
    game_memory_write_u32(&a->memory,0x603800,get(a,0x6040fc));
    game_memory_write_u32(&a->memory,0x603a40,get(a,0x6040a4));
    game_memory_write_u32(&a->memory,0x6040d8,get(a,0x604114));return !a->memory.fault;
}
static int price(void *c,GameAddress owner,int16_t unit,uint32_t *v){GameApplication *a=c;return gameplay_catalog_unit_price(&a->memory,owner,unit,v)==1;}
static int budget(void *c,GameAddress owner,uint32_t cost,uint32_t *v){GameApplication *a=c;return gameplay_catalog_unit_affordable(&a->memory,owner,cost,v)==1;}
GameKeyboardServices game_input_keyboard_services(GameApplication *a) {
    GameKeyboardServices s={0};s.context=a;s.events=game_input_event_services(a);
    s.units.context=a;s.units.price=price;s.units.can_afford=budget;
    s.feedback=feedback;s.queue_button=button;s.palette_changed=palette;s.queue_float_pair=floats;return s;
}
