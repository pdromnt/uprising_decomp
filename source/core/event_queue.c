#include "event_queue.h"
#include <string.h>
#include "effect_resource_pool.h"
#include "display_transition.h"
#include "entity_queries.h"
#include "gameplay_catalog.h"
#include "gameplay_state.h"

static int root_control_action(GameMemory *m,const GameRootControlServices *s,GameRootControlAction action,uint32_t value){
    if(m->fault)return 0;
    if(!s || !s->perform || !s->perform(s->context,action,value)){m->fault=1;return 0;}
    return !m->fault;
}
void game_input_queue_clear(GameMemory *m,GameAddress queue){
    game_memory_write_u32(m,queue+4u,0u);game_memory_write_u32(m,queue+8u,0u);
}
int game_input_queue_reset(GameMemory *m,GameAddress queue,int begin){
    GameAddress table=game_memory_read_u32(m,queue);
    uint32_t method=game_memory_read_u32(m,table+(begin?12u:16u));
    if(m->fault)return -1;
    if(method!=(begin?0x558c20u:0x558c30u))return 0;
    game_input_queue_clear(m,queue);
    return m->fault?-1:1;
}
static int root_control_queue(GameMemory *m,const GameRootControlServices *s,GameAddress queue,int begin){
    int status=game_input_queue_reset(m,queue,begin);
    if(status)return status>0;
    if(!s || !s->foreign_queue || !s->foreign_queue(s->context,queue,begin)){m->fault=1;return 0;}
    return !m->fault;
}
int game_root_selector_open(GameMemory *m,const GameRootControlServices *s){
    GameAddress selected,camera;uint32_t playing=0u;
    if(!root_control_action(m,s,GAME_ROOT_OPEN_SOUND,0x88u))return 0;
    if(!s->loop_sound_playing || !s->loop_sound_playing(s->context,&playing)){m->fault=1;return 0;}
    if(!playing && !root_control_action(m,s,GAME_ROOT_LOOP_SOUND,9u))return 0;
    game_memory_write_u32(m,0x60269cu,0u);game_memory_write_u32(m,0x6025b8u,0xffffffffu);
    game_memory_write_u32(m,0x6025a0u,0u);
    if((int16_t)game_memory_read_u16(m,0x639a74u)>0){
        selected=gameplay_player_choose_collection(m,NULL,0u);
        game_memory_write_u32(m,0x6025a0u,selected);
        if(selected && game_memory_read_u32(m,selected+0x54u)==game_memory_read_u32(m,0x5f0208u))
            game_memory_write_u32(m,0x6025b8u,0u);
    }
    if(!root_control_action(m,s,GAME_ROOT_SET_APPLICATION_MODE,7u))return 0;
    game_memory_write_u32(m,0x597f60u,0u);
    if(!root_control_action(m,s,GAME_ROOT_REFRESH_SELECTOR,game_memory_read_u32(m,0x597f80u)) ||
       !gameplay_selector_ranges(m,game_memory_read_u32(m,0x597f80u)!=0u))return 0;
    camera=game_memory_read_u32(m,0x58f8a8u);
    if(!game_camera_activate(m,camera,NULL))return 0;
    camera=game_memory_read_u32(m,0x58f8a8u);
    game_memory_write_u32(m,camera+0x3cu,16u);
    game_memory_write_u16(m,game_memory_read_u32(m,0x58f8a8u)+0x38u,0u);
    game_memory_write_u32(m,0x6027e4u,0u);return !m->fault;
}
int game_root_selector_close(GameMemory *m,const GameRootControlServices *s){
    GameAddress selector;uint32_t rectangle[4];unsigned index;
    static const uint32_t rectangle_fields[]={0x59d450u,0x59d458u,0x59d454u,0x59d45cu};
    game_memory_write_u32(m,0x6025a0u,0u);game_memory_write_u32(m,0x6025b8u,0xffffffffu);
    if(!gameplay_selector_ranges(m,game_memory_read_u32(m,0x597f80u)!=0u))return 0;
    game_memory_write_u8(m,0x59d448u,0xf6u);
    game_memory_write_u32(m,0x597f60u,game_memory_read_u32(m,0x597f84u));
    selector=game_memory_read_u32(m,0x628a88u);
    for(index=0u;index<4u;index++)rectangle[index]=game_memory_read_u32(m,selector+0x534u+index*4u);
    for(index=0u;index<4u;index++)game_memory_write_u32(m,rectangle_fields[index],rectangle[index]);
    return !m->fault && gameplay_panel_settle(m,game_memory_read_u32(m,0x602678u),
        s?s->foreign_widget_kind:NULL,s?s->context:NULL);
}
static int root_control_bind_camera(GameMemory *m,GameAddress root,GameAddress scene){
    GameAddress camera;
    if(game_memory_read_u32(m,0x5f0208u)!=root)return !m->fault;
    camera=game_memory_read_u32(m,0x58f8a8u);
    return !m->fault && (!camera || game_camera_bind_scene(m,camera,scene));
}
int game_root_control_event(GameMemory *m,GameAddress root,GameAddress event,const GameRootControlServices *s){
    uint8_t code=game_memory_read_u8(m,event+16u);GameAddress controller,scene;
    uint32_t index=0xffffffffu;
    if(code!=0x31u && code!=0x32u)return 0;
    controller=game_memory_read_u32(m,root+0x3bcu);
    if(code==0x31u){
        NativeEntityAttachmentResetResult reset;
        if(!game_memory_read_u32(m,controller+0x58u) || game_memory_read_u32(m,root+0x5a0u))return !m->fault;
        if(!native_entity_reset_attachments(m,controller,&reset))return 0;
        controller=game_memory_read_u32(m,root+0x3bcu);scene=game_memory_read_u32(m,root+0x448u);
        if(scene!=controller){
            GameAddress record=game_memory_read_u32(m,scene+0x294u);
            GameAddress id=game_memory_read_u32(m,record+4u);
            game_memory_write_u32(m,root+0x44cu,game_memory_read_u32(m,id));
            if(!root_control_queue(m,s,scene+0x264u,0))return 0;
            controller=game_memory_read_u32(m,root+0x3bcu);
            game_memory_write_u32(m,root+0x448u,controller);game_memory_write_u32(m,root+0x450u,controller);
            if(!root_control_bind_camera(m,root,controller))return 0;
        }else game_memory_write_u32(m,root+0x44cu,0xffffffffu);
        game_memory_write_u32(m,root+0x5a0u,1u);
        return game_memory_read_u32(m,0x5f0208u)!=root?!m->fault:game_root_selector_open(m,s);
    }
    if(!game_memory_read_u32(m,root+0x5a0u))return !m->fault;
    if(game_memory_read_u8(m,event+24u)==3u)index=game_memory_read_u32(m,root+0x44cu);
    else if(game_memory_read_u8(m,event+24u)==2u)index=game_memory_read_u8(m,event+25u);
    if(index!=0xffffffffu){
        GameAddress array=game_memory_read_u32(m,0x639a70u),collection=game_memory_read_u32(m,array+index*4u);
        if(game_memory_read_u32(m,collection+0x54u)!=root)index=0xffffffffu;
        else{
            GameAddress members=game_memory_read_u32(m,collection+0x18u),linked=game_memory_read_u32(m,members);
            if(!linked || game_memory_read_u32(m,linked+0x10u)!=1u)index=0xffffffffu;
            else if(game_memory_read_u32(m,game_memory_read_u32(m,linked+0x48u)+0x2b4u)!=30u)index=0xffffffffu;
        }
    }
    if(m->fault)return 0;
    game_memory_write_u32(m,root+0x5a0u,0u);
    if(index==0xffffffffu){
        controller=game_memory_read_u32(m,root+0x3bcu);game_memory_write_u32(m,root+0x450u,controller);
        if(!root_control_bind_camera(m,root,controller))return 0;
        if(game_memory_read_u32(m,0x5f0208u)!=root)return !m->fault;
        return root_control_action(m,s,GAME_ROOT_CONFIRM_SOUND,121u) && game_root_selector_close(m,s) &&
            root_control_action(m,s,GAME_ROOT_SET_APPLICATION_MODE,1u) && game_camera_restore(m,NULL) &&
            root_control_action(m,s,GAME_ROOT_SET_DISPLAY_CONTROL,0u);
    }
    {
        GameAddress array=game_memory_read_u32(m,0x639a70u),collection=game_memory_read_u32(m,array+index*4u);
        GameAddress members=game_memory_read_u32(m,collection+0x18u),linked=game_memory_read_u32(m,members);
        scene=game_memory_read_u32(m,linked+0x48u);controller=game_memory_read_u32(m,root+0x3bcu);
        if(!root_control_queue(m,s,controller+0x13cu,0))return 0;
        game_memory_write_u32(m,root+0x448u,scene);game_memory_write_u32(m,root+0x450u,scene);
        if(!root_control_bind_camera(m,root,scene) || !root_control_queue(m,s,scene+0x264u,1))return 0;
        if(game_memory_read_u32(m,0x5f0208u)!=root)return !m->fault;
        if(!root_control_action(m,s,GAME_ROOT_STOP_LOOP_SOUND,9u) ||
           !root_control_action(m,s,GAME_ROOT_CONFIRM_SOUND,121u) || !game_root_selector_close(m,s))return 0;
        array=game_memory_read_u32(m,0x639a70u);collection=game_memory_read_u32(m,array+index*4u);
        members=game_memory_read_u32(m,collection+0x18u);linked=game_memory_read_u32(m,members);
        scene=game_memory_read_u32(m,linked+0x48u);
        game_memory_write_u32(m,game_memory_read_u32(m,0x58f8a0u)+0x28u,scene);
        if(!game_camera_activate(m,game_memory_read_u32(m,0x58f8a0u),NULL))return 0;
        return root_control_action(m,s,GAME_ROOT_SET_APPLICATION_MODE,2u) && root_control_action(m,s,GAME_ROOT_SET_DISPLAY_CONTROL,1u);
    }
}

int game_root_selection_event(GameMemory *m,GameAddress root,GameAddress event,
        GameRootEntityResolve resolve,void *context){
    uint8_t code=game_memory_read_u8(m,event+16u);
    GameAddress controller,selected=0u;uint32_t identity=0u;
    if(code<0x19u || code>0x34u)return !m->fault;
    if(code!=0x24u && code!=0x2eu && code!=0x2fu)return 0;
    controller=game_memory_read_u32(m,root+0x3bcu);
    if(code==0x24u){
        GameAddress active=controller,camera;uint16_t index_bits;int16_t index;
        if(!game_memory_read_u32(m,root+0x5a0u) || !game_memory_read_u32(m,controller+0x58u))return !m->fault;
        index_bits=game_memory_read_u16(m,event+24u);memcpy(&index,&index_bits,2u);
        if(index!=-1){
            GameAddress array=game_memory_read_u32(m,0x639a70u);
            GameAddress entity=game_memory_read_u32(m,array+(uint32_t)(int32_t)index*4u);
            if(game_memory_read_u32(m,entity+0x54u)==root){
                GameAddress link=game_memory_read_u32(m,entity+24u);
                GameAddress linked=game_memory_read_u32(m,link);
                if(linked)active=game_memory_read_u32(m,linked+0x48u);
            }
        }
        game_memory_write_u32(m,root+0x450u,active);
        if(game_memory_read_u32(m,0x5f0208u)==root){
            camera=game_memory_read_u32(m,0x58f8a8u);
            if(camera && !game_camera_bind_scene(m,camera,active))return 0;
        }
        game_memory_write_u32(m,root+0x454u,game_memory_read_u32(m,0x5ac6acu));
        return !m->fault;
    }
    if(code==0x2fu){
        GameAddress owner;
        if(!game_memory_read_u32(m,controller+0x58u))return !m->fault;
        identity=game_memory_read_u32(m,event+24u);
        owner=game_memory_read_u32(m,0x5f0210u+(identity&63u)*4u);
        if(!resolve || !resolve(context,owner,identity,&selected))return 0;
        if(!selected || !(game_memory_read_u32(m,selected+16u)&0x1c0u)){
            selected=0u;identity=0u;
        }
    }
    game_memory_write_u32(m,root+0x3c4u,selected);
    game_memory_write_u32(m,root+0x3c8u,identity);
    controller=game_memory_read_u32(m,root+0x3bcu);
    if(controller==game_memory_read_u32(m,root+0x448u)){
        game_memory_write_u32(m,controller+0xf0u,selected);
        game_memory_write_u32(m,controller+0xf4u,identity);
    }
    return !m->fault;
}

static int root_event_publish(GameMemory *m,const GameRootEventServices *services,
        GameAddress root,uint8_t code,uint32_t payload){
    int fixed=code==0x2fu || code==0x24u;
    uint32_t kind=code==0x2fu?10u:code==0x24u?13u:3u,now,identity;
    EffectResourcePool *pool=effect_resource_pool_find(fixed?0x5cef90u:0x5cee40u);
    EffectResource *record;GameAddress event;unsigned i;
    if(!pool || !services->clock_now)return 0;
    event=effect_resource_pool_acquire_kind(m,pool,kind);record=effect_resource_find(event);
    if(!record || m->fault)return 0;
    if(!services->clock_now(services->context,&now)){
        effect_resource_pool_return_chain(m,pool,event);return 0;
    }
    record->deadline=(now-game_memory_read_u32(m,0x5ac90cu))>>5u;
    record->event_code=code;identity=game_memory_read_u16(m,root+8u);
    for(i=0;i<4u;i++)record->event_padding[i+2u]=(uint8_t)(identity>>(i*8u));
    if(fixed)record->event_arguments[0]=payload;
    if(!game_event_queue_insert(m,root,event)){
        effect_resource_pool_return_chain(m,pool,event);return 0;
    }
    if(game_memory_read_u32(m,0x5eff04u)){
        if(!services->network_notify || !services->network_notify(services->context,event))return 0;
    }
    return !m->fault;
}
int game_root_events_produce(GameMemory *m,const GameRootEventServices *services,
        GameAddress root,uint32_t tick){
    uint32_t selected,changed;GameAddress controller,scene;
    if(!m || !services || !services->controller_delta || !root)return 0;
    selected=game_memory_read_u32(m,0x603e38u);
    if(selected!=game_memory_read_u32(m,root+0x3c8u)){
        if(!root_event_publish(m,services,root,selected?0x2fu:0x2eu,selected))return 0;
    }
    controller=game_memory_read_u32(m,root+0x3bcu);
    if(m->fault || !services->controller_delta(services->context,controller+0x13cu,tick,&changed))return 0;
    scene=game_memory_read_u32(m,root+0x448u);
    controller=game_memory_read_u32(m,root+0x3bcu);
    if(scene!=controller && !services->controller_delta(services->context,scene+0x264u,tick,&changed))return 0;
    if(game_memory_read_u32(m,root+0x5a0u)){
        controller=game_memory_read_u32(m,root+0x3bcu);
        if(controller!=game_memory_read_u32(m,root+0x450u) && game_memory_read_u32(m,controller+0x140u)){
            if(!root_event_publish(m,services,root,0x24u,0xffffu))return 0;
            changed=1u;
        }
    }
    if(!changed && game_memory_read_u32(m,0x5eff04u)){
        if(!root_event_publish(m,services,root,0x35u,0u))return 0;
    }
    return !m->fault;
}

int game_event_packet_publish(GameMemory *m,GameAddress queue,const GameEventPacket *packet,GameAddress *output){
    EffectResourcePool *pool;EffectResource *record;GameAddress address;unsigned i;
    if(!m || !packet || !output || packet->count>8u)return 0;
    pool=effect_resource_pool_find(packet->compact?0x5ceee8u:0x5cef90u);
    if(!pool)return 0;
    address=effect_resource_pool_acquire_kind(m,pool,packet->compact?0u:1u);
    record=effect_resource_find(address);
    if(!record || m->fault)return 0;
    record->deadline=packet->deadline;record->event_code=packet->code;
    for(i=0;i<4u;i++)record->event_padding[i+2u]=(uint8_t)(packet->identity>>(i*8u));
    if(packet->compact){
        record->packet_mask=packet->mask;
        memcpy(record->packet_data,packet->values,(size_t)packet->count*2u);
    }else memcpy(record->event_arguments,packet->words,8u);
    if(!game_event_queue_insert_at(m,queue+0x28u,queue+0x2cu,address)){
        effect_resource_pool_return_chain(m,pool,address);return 0;
    }
    *output=address;return !m->fault;
}

int game_event_record_compare(GameMemory *m,GameAddress left,GameAddress right,int *result){
    uint32_t a,b,table;int32_t deadline_a,deadline_b;unsigned i,count=0u;
    if(!m || !result)return 0;
    table=game_memory_read_u32(m,left);
    if(table!=0x574108u && table!=0x5740f8u && table!=0x5740e8u)return 0;
    a=game_memory_read_u32(m,left+12u);b=game_memory_read_u32(m,right+12u);
    memcpy(&deadline_a,&a,4u);memcpy(&deadline_b,&b,4u);
    if(deadline_a!=deadline_b){*result=deadline_a>deadline_b;return !m->fault;}
    for(i=16u;i<18u;i++){
        a=game_memory_read_u8(m,left+i);b=game_memory_read_u8(m,right+i);
        if(a!=b){*result=a>b;return !m->fault;}
    }
    if(table==0x5740f8u){
        for(i=24u;i<32u;i+=4u){
            a=game_memory_read_u32(m,left+i);b=game_memory_read_u32(m,right+i);
            if(a!=b){*result=a>b;return !m->fault;}
        }
    }else if(table==0x5740e8u){
        a=game_memory_read_u8(m,left+24u);b=game_memory_read_u8(m,right+24u);
        if(a!=b){*result=a>b;return !m->fault;}
        for(i=0;i<8u;i++)count+=(a>>i)&1u;
        for(i=0;i<count;i++){
            uint16_t x=game_memory_read_u16(m,left+26u+i*2u),y=game_memory_read_u16(m,right+26u+i*2u);
            int16_t sx,sy;memcpy(&sx,&x,2u);memcpy(&sy,&y,2u);
            if(sx!=sy){*result=sx<sy;return !m->fault;}
        }
    }
    *result=1;return !m->fault;
}
int game_event_queue_insert_at(GameMemory *m,GameAddress head_address,GameAddress tail_address,GameAddress event){
    GameAddress head,tail,node,next;int after;unsigned visits=0u;
    if(!m || !head_address || !tail_address || !event)return 0;
    head=game_memory_read_u32(m,head_address);
    if(!head){
        game_memory_write_u32(m,tail_address,event);game_memory_write_u32(m,head_address,event);
        return !m->fault;
    }
    tail=game_memory_read_u32(m,tail_address);
    if(!game_event_record_compare(m,event,tail,&after))return 0;
    if(after){
        game_memory_write_u32(m,tail+4u,event);game_memory_write_u32(m,tail_address,event);
        return !m->fault;
    }
    if(!game_event_record_compare(m,head,event,&after))return 0;
    if(after){
        game_memory_write_u32(m,event+4u,head);game_memory_write_u32(m,head_address,event);
        return !m->fault;
    }
    node=head;
    do{
        next=game_memory_read_u32(m,node+4u);
        if(++visits>65536u || !next || !game_event_record_compare(m,next,event,&after))return 0;
        if(!after)node=next;
    }while(!after);
    game_memory_write_u32(m,event+4u,next);game_memory_write_u32(m,node+4u,event);
    return !m->fault;
}
int game_event_queue_insert(GameMemory *m,GameAddress root,GameAddress event){
    return root && game_event_queue_insert_at(m,root+0x458u,root+0x45cu,event);
}
int game_root_event_receive(GameMemory *m,GameAddress root,GameAddress event){
    GameAddress player,group,member,payload,tail;uint32_t identity;unsigned visits=0;
    if(!m || !root || !event || m->fault)return 0;
    identity=game_memory_read_u32(m,event+20u);
    if(identity==game_memory_read_u16(m,root+8u))return game_event_queue_insert(m,root,event);
    player=game_memory_read_u32(m,root+0x3bcu);
    if(player && game_memory_read_u32(m,player+20u)==identity)
        return game_event_queue_insert_at(m,player+0x164u,player+0x168u,event);
    group=game_memory_read_u32(m,root+0x294u);
    while(group && !m->fault){
        if(++visits>65536u)return 0;
        member=game_memory_read_u32(m,game_memory_read_u32(m,group+0x18u));
        if(member){
            payload=game_memory_read_u32(m,member+0x48u);
            if(payload && game_memory_read_u32(m,payload+20u)==identity)
                return game_event_queue_insert_at(m,payload+0x28cu,payload+0x290u,event);
        }
        group=game_memory_read_u32(m,group+0x64u);
    }
    if(m->fault)return 0;
    /*An unknown identity can become valid before its deadline.47dfe0 owns
     *later delivery/drop, and player teardown owns the remaining FIFO. */
    tail=game_memory_read_u32(m,root+0x464u);
    if(tail)game_memory_write_u32(m,tail+4u,event);
    else game_memory_write_u32(m,root+0x460u,event);
    game_memory_write_u32(m,event+4u,0u);
    game_memory_write_u32(m,root+0x464u,event);
    return !m->fault;
}

enum {
    EVENT_QUEUE_TICK = 0x005ac6acu,
    EVENT_QUEUE_HEAD = 0x28u,
    EVENT_QUEUE_TAIL = 0x2cu,
    EVENT_NEXT = 0x04u,
    EVENT_DEADLINE = 0x0cu
};

int game_timed_event_queue_expire(GameMemory *memory,
    const GameEventQueueCallbacks *callbacks, GameAddress queue) {
    GameAddress head = game_memory_read_u32(memory, queue + EVENT_QUEUE_HEAD);
    while (!memory->fault && head != 0u) {
        uint32_t tick = game_memory_read_u32(memory, EVENT_QUEUE_TICK);
        uint32_t deadline = game_memory_read_u32(memory,
            head + EVENT_DEADLINE);
        GameAddress next;
        GameAddress tail;
        if (memory->fault || deadline != tick) break;
        next = game_memory_read_u32(memory, head + EVENT_NEXT);
        tail = game_memory_read_u32(memory, queue + EVENT_QUEUE_TAIL);
        if (memory->fault) break;
        game_memory_write_u32(memory, queue + EVENT_QUEUE_HEAD, next);
        if (tail == head)
            game_memory_write_u32(memory, queue + EVENT_QUEUE_TAIL, 0u);
        if (memory->fault || !callbacks || !callbacks->remove ||
                !callbacks->remove(callbacks->context,
                memory, queue, head))
            return 0;
        if (!callbacks->release ||
                !callbacks->release(callbacks->context, head))
            return 0;
        head = game_memory_read_u32(memory, queue + EVENT_QUEUE_HEAD);
    }
    return !memory->fault;
}

int game_event_list_clear_head(GameMemory *memory,
    const GameEventQueueCallbacks *callbacks, GameAddress head_address) {
    GameAddress head;
    if (!memory || !callbacks || !callbacks->release || !head_address)
        return 0;
    head = game_memory_read_u32(memory, head_address);
    while (!memory->fault && head) {
        GameAddress next = game_memory_read_u32(memory, head + EVENT_NEXT);
        if (memory->fault) return 0;
        game_memory_write_u32(memory, head_address, next);
        if (memory->fault || !callbacks->release(callbacks->context, head))
            return 0;
        head = game_memory_read_u32(memory, head_address);
    }
    return !memory->fault;
}

int game_timed_event_queue_clear(GameMemory *memory,
    const GameEventQueueCallbacks *callbacks, GameAddress queue) {
    if (!memory || !queue ||
        !game_event_list_clear_head(memory, callbacks,
            queue + EVENT_QUEUE_HEAD)) return 0;
    if (memory->fault) return 0;
    game_memory_write_u32(memory, queue + EVENT_QUEUE_TAIL, 0u);
    return !memory->fault;
}

/* 47c490/510/600/700/770 use the same owned event lists, with distinct pool
 * record kinds and payload widths. Payload bytes are values, never a call frame. */
static int root_payload_publish(GameMemory *m,const GameRootEventServices *s,GameAddress root,
        uint8_t code,GameAddress pool_id,uint16_t kind,const uint8_t *payload,unsigned size){
    EffectResourcePool *pool=effect_resource_pool_find(pool_id);
    EffectResource *record;GameAddress event;uint32_t now,identity;unsigned i;
    if(!pool || !s || !s->clock_now || size>8u || m->fault)return 0;
    event=effect_resource_pool_acquire_kind(m,pool,kind);record=effect_resource_find(event);
    if(!record)return 0;
    if(!s->clock_now(s->context,&now))goto failed;
    record->deadline=(now-game_memory_read_u32(m,0x5ac90cu))>>5u;
    record->event_code=code;identity=game_memory_read_u16(m,root+8u);
    for(i=0;i<4u;++i)record->event_padding[i+2u]=(uint8_t)(identity>>(i*8u));
    for(i=0;i<size;++i)game_memory_write_u8(m,event+24u+i,payload[i]);
    if(pool_id==0x5cee40u && code==25u)game_memory_write_u32(m,root+0x580u,1u);
    if(m->fault || !game_event_queue_insert(m,root,event))goto failed;
    if(game_memory_read_u32(m,0x5eff04u) &&
       (!s->network_notify || !s->network_notify(s->context,event)))return 0;
    return !m->fault;
failed:
    effect_resource_pool_return_chain(m,pool,event);return 0;
}
int game_root_signal_publish(GameMemory *m,const GameRootEventServices *s,GameAddress r,uint8_t code){
    return root_payload_publish(m,s,r,code,0x5cee40u,3u,NULL,0u);
}
int game_root_count_publish(GameMemory *m,const GameRootEventServices *s,GameAddress r,uint8_t code,uint8_t count){
    return root_payload_publish(m,s,r,code,0x5cef90u,8u,&count,1u);
}
int game_root_coordinates_publish(GameMemory *m,const GameRootEventServices *s,GameAddress r,uint8_t code,const uint16_t coordinates[4]){
    uint8_t bytes[8];unsigned i;
    for(i=0;i<4u;++i){bytes[i*2u]=(uint8_t)coordinates[i];bytes[i*2u+1u]=(uint8_t)(coordinates[i]>>8u);}
    return root_payload_publish(m,s,r,code,0x5cef90u,9u,bytes,8u);
}
int game_root_scene_publish(GameMemory *m,const GameRootEventServices *s,GameAddress r,uint8_t code,uint16_t scene){
    uint8_t bytes[2]={(uint8_t)scene,(uint8_t)(scene>>8u)};
    return root_payload_publish(m,s,r,code,0x5cef90u,13u,bytes,2u);
}
int game_root_pair_publish(GameMemory *m,const GameRootEventServices *s,GameAddress r,uint8_t code,uint32_t first,uint32_t second){
    uint8_t bytes[8];unsigned i;
    for(i=0;i<4u;++i){bytes[i]=(uint8_t)(first>>(i*8u));bytes[i+4u]=(uint8_t)(second>>(i*8u));}
    return root_payload_publish(m,s,r,code,0x5cef90u,11u,bytes,8u);
}
/*47c690 publishes a single identity word, using kind10 in the same pool. */
int game_root_word_publish(GameMemory *m,const GameRootEventServices *s,GameAddress r,uint8_t code,uint32_t value){
    uint8_t bytes[4];unsigned i;
    for(i=0;i<4u;++i)bytes[i]=(uint8_t)(value>>(i*8u));
    return root_payload_publish(m,s,r,code,0x5cef90u,10u,bytes,4u);
}
int game_root_command_publish(GameMemory *m,const GameRootEventServices *s,GameAddress root,
        uint8_t code,uint8_t unit,uint8_t selection){
    EffectResourcePool *pool=effect_resource_pool_find(0x5cef90u);
    EffectResource *record;GameAddress event;uint32_t now,identity;unsigned i;
    if(!pool || !s || !s->clock_now)return 0;
    event=effect_resource_pool_acquire_kind(m,pool,9u);record=effect_resource_find(event);
    if(!record || m->fault)return 0;
    if(!s->clock_now(s->context,&now))goto failed;
    record->deadline=(now-game_memory_read_u32(m,0x5ac90cu))>>5u;
    record->event_code=code;identity=game_memory_read_u16(m,root+8u);
    for(i=0u;i<4u;i++)record->event_padding[i+2u]=(uint8_t)(identity>>(i*8u));
    game_memory_write_u8(m,event+24u,unit);game_memory_write_u8(m,event+25u,selection);
    if(m->fault || !game_event_queue_insert(m,root,event))goto failed;
    if(game_memory_read_u32(m,0x5eff04u) &&
        (!s->network_notify || !s->network_notify(s->context,event)))return 0;
    return !m->fault;
failed:
    effect_resource_pool_return_chain(m,pool,event);return 0;
}
int game_input_direction(GameMemory *m,GameAddress queue,unsigned direction,int pressed,uint32_t strength){
    static const uint32_t bits[10]={1u,2u,16u,32u,4u,8u,64u,128u,256u,512u};
    uint32_t bit,opposite,active,delta;GameAddress value;
    if(direction>=10u)return 0;
    bit=bits[direction];opposite=bits[direction^1u];value=queue+20u+(direction/2u)*4u;
    active=game_memory_read_u32(m,queue+4u);delta=game_memory_read_u32(m,queue+8u);
    if(pressed){
        game_memory_write_u32(m,queue+4u,active|bit);
        game_memory_write_u32(m,value,strength);
        game_memory_write_u32(m,queue+8u,(delta|bit)&~opposite);
    }else{
        active&=~bit;game_memory_write_u32(m,queue+4u,active);
        game_memory_write_u32(m,queue+8u,delta&~bit);
        if(!(active&opposite))game_memory_write_u32(m,value,0u);
    }
    return !m->fault;
}
int game_panel_press_clear(GameMemory *m,GameAddress panel){
    uint32_t index=0u,count=game_memory_read_u32(m,panel);
    if(count>256u && count<0x80000000u){m->fault=1;return 0;}
    while(index<count && count<0x80000000u && !m->fault){
        GameAddress widget=game_memory_read_u32(m,panel+4u+index*4u);
        game_memory_write_u32(m,widget+4u,0u);++index;count=game_memory_read_u32(m,panel);
    }
    return !m->fault;
}
int game_selector_request(GameMemory *m,const GameRootEventServices *s,uint8_t code){
    GameAddress chosen=game_memory_read_u32(m,0x6025a0u),widget=game_memory_read_u32(m,0x6025dcu);
    uint8_t selection=chosen?game_memory_read_u8(m,chosen):code;
    uint32_t saved=game_memory_read_u32(m,widget+4u);
    if(!game_panel_press_clear(m,game_memory_read_u32(m,0x60259cu)) ||
        !game_panel_press_clear(m,game_memory_read_u32(m,0x602678u)))return 0;
    game_memory_write_u32(m,widget+4u,saved);
    return !m->fault && game_root_command_publish(m,s,game_memory_read_u32(m,0x5f0208u),50u,code,selection);
}
int game_selector_unit_request(GameMemory *m,const GameRootEventServices *events,
    const GameplayFeedbackServices *feedback,uint32_t unit){
    GameAddress collection,root,message=0u;uint32_t slot=0u;uint16_t reason=0u;
    if(!unit)return !m->fault;
    if(!feedback || !feedback->sound || !feedback->text)return 0;
    collection=game_memory_read_u32(m,0x6025a0u);
    if(!collection)message=0x58f5ccu;
    else{
        root=game_memory_read_u32(m,0x5f0208u);
        if(game_memory_read_u32(m,collection+0x54u)!=root)message=0x58f5e0u;
        else if(game_memory_read_u16(m,collection+0x24u)==game_memory_read_u16(m,collection+0x26u))message=0x58f608u;
    }
    if(m->fault)return 0;
    if(message)return feedback->sound(feedback->context,122u) && feedback->text(feedback->context,message,NULL);
    if(!gameplay_collection_eligibility(m,&feedback->units,collection,(int16_t)unit,&slot,&reason))return 0;
    /* The capability query diagnoses invalid units before returning reason4.
     * Preserve that notification at this connected feedback boundary. */
    if(reason==4u && ((int16_t)unit<0 || (int16_t)unit>10) &&
        (!feedback->diagnostic || !feedback->diagnostic(feedback->context,0x5897d0u)))return 0;
    if(!reason){
        /* Reload after eligibility callbacks, matching the original shared selector globals. */
        collection=game_memory_read_u32(m,0x6025a0u);root=game_memory_read_u32(m,0x5f0208u);
        if(!game_root_command_publish(m,events,root,26u,(uint8_t)unit,game_memory_read_u8(m,collection)))return 0;
    }
    if(!feedback->sound(feedback->context,reason?122u:121u))return 0;
    return gameplay_collection_feedback(m,feedback,(int16_t)reason,(uint8_t)unit);
}
int game_player_keyboard(GameMemory *m,const GameKeyboardServices *s,uint32_t key,uint32_t flags){
    uint32_t action;GameAddress root,controller;unsigned direction;int pressed=(flags&128u)!=0u;
    if(key==57u){game_memory_write_u32(m,0x60269cu,(uint32_t)pressed);return !m->fault;}
    if(key>=256u)return !m->fault;
    action=game_memory_read_u32(m,0x6047f0u+key*4u);
    switch(action){case 1u:direction=0u;break;case 2u:direction=1u;break;
        case 3u:direction=2u;break;case 4u:direction=3u;break;
        case 5u:direction=4u;break;case 6u:direction=5u;break;
        case 32u:direction=6u;break;case 33u:direction=7u;break;default:direction=8u;break;}
    if(direction<8u){
        root=game_memory_read_u32(m,0x5f0208u);controller=game_memory_read_u32(m,root+0x3bcu);
        if(controller!=game_memory_read_u32(m,root+0x448u))return !m->fault;
        return game_input_direction(m,controller+0x13cu,direction,pressed,0x3f800000u);
    }
    if(!pressed || m->fault)return !m->fault;
    if(!s)return 0;
    root=game_memory_read_u32(m,0x5f0208u);
    if(action==17u || action==47u)return game_selector_request(m,&s->events,3u);
    if(action==22u || action==34u){
        controller=game_memory_read_u32(m,root+0x3bcu);
        return s->queue_button && s->queue_button(s->context,controller+0x13cu,action==22u?17u:20u);
    }
    if(action==37u){
        static const uint32_t weights[4][3]={{0u,0x3f800000u,0u},{0x3f800000u,0u,0u},
            {0u,0u,0x3f800000u},{0x3eaaaa3bu,0x3eaaaa3bu,0x3eaaab8au}};
        static const GameAddress messages[4]={0x595dd4u,0x595de0u,0x595decu,0x595df8u};
        int32_t cycle=((int32_t)(int16_t)game_memory_read_u16(m,0x603744u)+1)%4;
        game_memory_write_u16(m,0x603744u,(uint16_t)cycle);
        if(cycle>=0){
            game_memory_write_u32(m,0x6040a4u,weights[cycle][0]);
            game_memory_write_u32(m,0x6040fcu,weights[cycle][1]);
            game_memory_write_u32(m,0x604114u,weights[cycle][2]);
            if(!s->feedback || !s->feedback(s->context,0u,messages[cycle]))return 0;
        }
        if(!s->palette_changed || !s->palette_changed(s->context))return 0;
        root=game_memory_read_u32(m,0x5f0208u);controller=game_memory_read_u32(m,root+0x3bcu);
        return !m->fault && s->queue_float_pair && s->queue_float_pair(s->context,controller+0x13cu,
            game_memory_read_u32(m,0x604114u),game_memory_read_u32(m,0x6040a4u));
    }
    if(action>=52u && action<=57u){
        uint8_t unit=action==56u?10u:action==57u?5u:
            (uint8_t)((game_memory_read_u8(m,0x594d5cu)?6u:1u)+action-52u);
        GameAddress chosen=gameplay_player_choose_collection(m,&s->units,unit);
        if(m->fault)return 0;
        if(unit==5u){
            uint32_t root_state;
            root=game_memory_read_u32(m,0x5f0208u);root_state=game_memory_read_u32(m,root+0x2d4u);
            if(root_state<0x80000000u)return 1;
            return s->feedback && s->feedback(s->context,123u,0x596098u);
        }
        if(!chosen)return s->feedback && s->feedback(s->context,123u,0x5960b8u);
        if(!s->feedback || !s->feedback(s->context,121u,0u))return 0;
        root=game_memory_read_u32(m,0x5f0208u);
        if(!game_root_command_publish(m,&s->events,root,26u,unit,game_memory_read_u8(m,chosen)))return 0;
        return s->feedback && s->feedback(s->context,0u,gameplay_collection_message(unit));
    }
    return !m->fault;
}
int game_input_button_publish(GameMemory *m,const GameRootEventServices *s,GameAddress queue,uint8_t code,uint32_t identity){
    EffectResourcePool *pool=effect_resource_pool_find(0x5cee40u);
    EffectResource *record;GameAddress event;uint32_t now;unsigned i;
    if(!pool || !s || !s->clock_now)return 0;
    event=effect_resource_pool_acquire_kind(m,pool,0u);record=effect_resource_find(event);
    if(!record || m->fault)return 0;
    if(!s->clock_now(s->context,&now))goto failed;
    record->deadline=(now-game_memory_read_u32(m,0x5ac90cu))>>5u;record->event_code=code;
    for(i=0u;i<4u;i++)record->event_padding[i+2u]=(uint8_t)(identity>>(i*8u));
    if(!game_event_queue_insert_at(m,queue+0x28u,queue+0x2cu,event))goto failed;
    if(game_memory_read_u32(m,0x5eff04u) && (!s->network_notify || !s->network_notify(s->context,event)))return 0;
    return !m->fault;
failed:
    effect_resource_pool_return_chain(m,pool,event);return 0;
}
