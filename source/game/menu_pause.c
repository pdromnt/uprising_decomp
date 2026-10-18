#include "menu_pause.h"
#include "menu_widgets.h"
#include "gameplay_input.h"
#include "menu_network.h"

extern uint32_t game_menu_milliseconds(GameApplication *);
extern int game_menu_transition_notify(GameApplication *);
extern int game_menu_pointer_position(GameApplication *,int32_t,int32_t);
extern int game_menu_controller_keyboard_handler(GameApplication *,GameAddress,uint32_t,uint32_t);
extern int game_menu_unregister_keyboard_method(GameApplication *,GameAddress,uint32_t);
extern int game_menu_strategic_depart(GameApplication *,uint32_t,uint32_t);
extern int game_application_timed_message(GameApplication *,GameAddress,int32_t);
/*Distinct owned option screens and clock kind4/flush are unfinished native
 *dependencies. Their ordinary signatures cannot re-enter the CPU runtime. */
extern int game_menu_ingame_4c3110(GameApplication *,uint32_t);
extern int game_menu_ingame_4c3950(GameApplication *,uint32_t);
extern int game_menu_ingame_4c4170(GameApplication *,uint32_t);
extern int game_menu_ingame_4c4e00(GameApplication *,uint32_t);
extern int game_menu_ingame_4c54c0(GameApplication *,uint32_t);
extern int game_menu_ingame_4c5cb0(GameApplication *,uint32_t);
extern int game_menu_ingame_4c6470(GameApplication *,uint32_t);
extern int game_menu_controller_tick(GameApplication *,GameAddress);
extern int game_level_clock_packet_enqueue(GameApplication *,GameAddress,const uint8_t *,uint32_t);
extern int game_level_clock_packet_flush(GameApplication *,GameAddress);
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
/*4c6eb0: tick callbacks before reading the current page. A callback may change
 *the page or its owned controls; do not cache that state before delivery. */
int game_level_frame_004c6eb0(GameApplication *a){
    if(!a || a->memory.fault || !game_menu_controller_tick(a,get(a,0x59d4c0)))return 0;
    switch(get(a,0x5910a4)){
    case 1:return game_menu_ingame_pause_state(a,3);
    case 2:return game_menu_strategic_depart(a,0,3);
    case 3:return game_menu_strategic_depart(a,1,3);
    case 4:return game_menu_ingame_4c3110(a,3);
    case 5:return game_menu_ingame_4c54c0(a,3);
    case 6:return game_menu_ingame_4c5cb0(a,3);
    case 7:return game_menu_ingame_4c6470(a,3);
    case 8:return game_menu_ingame_4c3950(a,3);
    case 9:return game_menu_ingame_4c4e00(a,3);
    case 10:return game_menu_ingame_4c4170(a,3);
    default:return !a->memory.fault;
    }
}
typedef struct PauseButton {GameAddress slot,names[2][4];uint32_t callback;unsigned row,command;} PauseButton;
static const PauseButton buttons[]={
 {0x5910d4,{{0x591c0c,0x591be4,0x591bc0,0},{0x591fd4,0x591fac,0x591f88,0}},0x4c70b0,1,0},
 {0x5910b8,{{0x591c7c,0x591c54,0x591c30,0},{0x592044,0x59201c,0x591ff8,0}},0x4c6f70,2,1},
 {0x5910bc,{{0x591cec,0x591cc4,0x591ca0,0},{0x5920b4,0x59208c,0x592068,0}},0x4c6fb0,3,1},
 {0x5910cc,{{0x591d60,0x591d38,0x591d10,0},{0x592128,0x592100,0x5920d8,0}},0x4c7030,2,0},
 {0x5910d0,{{0x591ddc,0x591db0,0x591d88,0},{0x5921a4,0x592178,0x592150,0}},0x4c7070,3,0},
 {0x5910c8,{{0x591e54,0x591e2c,0x591e04,0},{0x59221c,0x5921f4,0x5921cc,0}},0x4c6ff0,4,0},
 {0x5910c0,{{0x591ecc,0x591ea4,0x591e7c,0},{0x592294,0x59226c,0x592244,0}},0x4c70f0,5,1},
 {0x5910c4,{{0x591f40,0x591f18,0x591ef4,0},{0x592308,0x5922e0,0x5922bc,0}},0x4c71a0,5,1}
};
static int pause_release(GameApplication *a){
    static const GameAddress slots[]={0x5910b4,0x5910c8,0x5910c0,0x5910b8,0x5910bc,0x5910cc,0x5910d0,0x5910d4,0x5910c4};
    unsigned i;GameAddress p;
    for(i=0;i<sizeof(slots)/sizeof(slots[0]);++i){p=get(a,slots[i]);
        if(p && !game_menu_widget_destroy(a,p))return 0;
        put(a,slots[i],0);}
    put(a,0x5910a4,0);
    return game_menu_unregister_keyboard_method(a,get(a,0x59d4c0),0x52c2e0) && !a->memory.fault;
}
/*4c0b80: one controller borrows these owned controls; each button owns its
 *three bitmap frames. No parallel object or repeated presentation mirror. */
int game_menu_ingame_pause_state(GameApplication *a,uint32_t phase){
    unsigned i,large=get(a,0x597f80)!=0,network=get(a,0x5eff04)!=0;
    GameAddress p;uint32_t x,y,width,height;
    if(phase==4)return pause_release(a);
    if(phase==3){
        GameAddress order[]={0x5910b4,0x5910c0,network?0x5910cc:0x5910b8,network?0x5910d0:0x5910bc,0x5910d4,0x5910c8,0x5910c4};
        GameUiRect clip={0,0,large?640:320,large?480:200};
        for(i=0;i<7;++i){GameUiRect r;p=get(a,order[i]);if(!p)continue;
            r.left=(int32_t)get(a,p+0x20);r.top=(int32_t)get(a,p+0x24);
            r.right=(int32_t)get(a,p+0x28);r.bottom=(int32_t)get(a,p+0x2c);
            if(r.left<0)r.left=0;
            if(r.top<0)r.top=0;
            if(r.right>clip.right)r.right=clip.right;
            if(r.bottom>clip.bottom)r.bottom=clip.bottom;
            if(r.left<r.right && r.top<r.bottom && !game_menu_widget_draw(a,p,&r))return 0;}
        return !a->memory.fault;
    }
    if(phase==1 || phase==2){
        for(i=0;i<8;++i){p=get(a,buttons[i].slot);if(p && !game_menu_widget_enable(a,p,phase==1))return 0;}
        return phase!=1 || game_menu_pointer_position(a,large?320:160,large?240:100);
    }
    if(phase)return !a->memory.fault;
    if(get(a,0x5910b4))return 0;
    for(i=0;i<8;++i)if(get(a,buttons[i].slot))return 0;
    if(!game_menu_transition_notify(a) || !game_menu_controller_keyboard_handler(a,get(a,0x59d4c0),0x52c2e0,UINT32_MAX))return 0;
    put(a,0x5910a4,1);
    p=game_menu_image_create(a,UINT32_MAX,large?0x591f64:0x591b9c);if(!p)goto failed;
    put(a,0x5910b4,p);width=get(a,p+0x28)-get(a,p+0x20);height=get(a,p+0x2c)-get(a,p+0x24);
    x=(large?640u:320u)/2-width/2;
    y=(large?480u:200u)/2-height/2;
    if(!game_menu_widget_position(a,p,x,y,0,0))goto failed;
    for(i=0;i<8;++i){const PauseButton *b=buttons+i;
        if((i==1 || i==2) && network)continue;
        if((i==3 || i==4) && !network)continue;
        p=game_menu_button_create(a,b->command?5000:UINT32_MAX,b->names[large],UINT32_MAX,UINT32_MAX,b->command?UINT32_MAX:0,b->callback);
        if(!p)goto failed;
        put(a,b->slot,p);
        if(!game_menu_widget_position(a,p,x+(i==7?(large?123:75):(large?36:22)),y+b->row*(large?32:19),1,0))goto failed;
        put(a,p+0x50,UINT32_MAX);put(a,p+0x54,121);
    }
    return !a->memory.fault;
failed:(void)pause_release(a);return 0;
}
static int screens_release(GameApplication *a,int all){
    if(!game_menu_ingame_pause_state(a,4) || !game_menu_strategic_depart(a,0,4) || !game_menu_strategic_depart(a,1,4))return 0;
    if(all && !game_menu_ingame_4c3110(a,4))return 0;
    if(!game_menu_ingame_4c54c0(a,4) || !game_menu_ingame_4c5cb0(a,4) || !game_menu_ingame_4c3950(a,4))return 0;
    return !all || (game_menu_ingame_4c4170(a,4) && game_menu_ingame_4c6470(a,4) && game_menu_ingame_4c4e00(a,4));
}
void game_level_pause_reset(GameApplication *a){put(a,0x584668,0);put(a,0x5acbe4,0);}
void game_level_pause_mark(GameApplication *a){uint32_t elapsed=game_menu_milliseconds(a)-get(a,0x5ac90c);if(!get(a,0x584668))put(a,0x584668,elapsed);}
uint32_t game_level_pause_elapsed(GameApplication *a){uint32_t stamp=get(a,0x584668);return stamp?game_menu_milliseconds(a)-get(a,0x5ac90c)-stamp:0;}
int game_level_pause_begin(GameApplication *a){
    GameAddress root=get(a,0x5f0208),first=get(a,root+0x3bc),second=get(a,root+0x448);
    put(a,0x5acbe4,1);
    if((int32_t)get(a,first+0x58)>0 && !game_input_queue_button(a,first+0x13c,18))return 0;
    if(second!=first && (int32_t)get(a,second+0x58)>0 && !game_input_queue_button(a,second+0x264,24))return 0;
    if(get(a,0x5eff04))return game_application_timed_message(a,0x58466c,30000);
    return game_menu_ingame_pause_state(a,0) && game_menu_ingame_pause_state(a,1);
}
int game_level_pause_resume(GameApplication *a,uint32_t duration){
    uint32_t stamp=get(a,0x584668);GameAddress clock;
    if(!stamp)return !a->memory.fault;
    if(!duration)put(a,0x5ac90c,game_menu_milliseconds(a)-stamp);
    else{while(game_menu_milliseconds(a)-get(a,0x5ac90c)<=stamp+duration){if(a->memory.fault)return 0;}
        put(a,0x5ac90c,get(a,0x5ac90c)+duration);}
    game_level_pause_reset(a);
    if(!get(a,0x5eff04))return screens_release(a,1);
    clock=get(a,0x5accd4);if(!clock)return 0;
    put(a,clock+0x10,0);put(a,clock+0x90,0);put(a,clock+0x94,0);put(a,clock+0x98,0);put(a,clock+0x9c,0);
    put(a,0x63993c,0);return !a->memory.fault;
}
int game_level_resume_presentation(GameApplication *a){
    GameRootEventServices s=game_input_event_services(a);
    char text[128];unsigned i;
    for(i=0;i<sizeof(text);++i){text[i]=(char)game_memory_read_u8(&a->memory,0x589928+i);if(!text[i])break;}
    if(i==sizeof(text) || !game_memory_write(&a->memory,0x5ac6b8,text,i+1))return 0;
    return game_root_signal_publish(&a->memory,&s,get(a,0x5f0208),25);
}
/*440520/630 and host600/730: native transport requests and ordered clock
 *packets; the queue/flush remain real unresolved services, never fake success. */
int game_level_clock_pause_request(GameApplication *a,int resume,int end_game){
    GameAddress clock=get(a,0x5accd4);uint8_t packet[5];uint32_t elapsed;unsigned i;
    if(!clock)return 0;
    if(resume)put(a,clock+0xa0,(uint32_t)end_game);
    if(!get(a,0x5f0150)){packet[0]=(uint8_t)(resume?'F':'E');
        return game_network_send(a,get(a,0x5f0038),get(a,get(a,0x5eff28)+0x3c0),packet,1);}
    if(!resume){if(get(a,0x5acbe4))return !a->memory.fault;
        packet[0]='6';return game_level_pause_begin(a) && game_level_clock_packet_enqueue(a,clock,packet,1);}
    if(!get(a,0x5acbe4))return !a->memory.fault;
    game_memory_write_u8(&a->memory,0x5ad8c8,'7');elapsed=game_level_pause_elapsed(a);
    if((int32_t)elapsed<400)return !a->memory.fault;
    packet[0]='7';for(i=0;i<4;++i)packet[i+1]=(uint8_t)(elapsed>>(i*8));
    put(a,0x5ad8c9,elapsed);
    if(!game_level_clock_packet_enqueue(a,clock,packet,5) || !game_level_clock_packet_flush(a,clock) || !game_level_pause_resume(a,elapsed))return 0;
    if(get(a,clock+0xa0)){put(a,clock+0xa0,0);return game_level_resume_presentation(a);}
    return !a->memory.fault;
}
int game_input_pause(GameApplication *a,int resume){
    if(get(a,0x5eff04))return game_level_clock_pause_request(a,resume,0);
    return resume?game_level_pause_resume(a,0):game_level_pause_begin(a);
}
int game_menu_pause_action(GameApplication *a,uint32_t method){
    unsigned i;int ok;
    if(method==0x4c70f0 || method==0x4c71a0){
        if(!get(a,0x5acbe4) || get(a,0x5eff04)){
            if(!screens_release(a,0))return 0;
            if(method==0x4c70f0)return !a->memory.fault;
            return get(a,0x5acbe4)?game_level_clock_pause_request(a,1,1):game_level_resume_presentation(a);
        }
        return game_level_pause_resume(a,0) && (method==0x4c70f0 || game_level_resume_presentation(a));
    }
    if(!game_menu_ingame_pause_state(a,2))return 0;
    switch(method){
    case 0x4c6f70:ok=game_menu_strategic_depart(a,0,0) && game_menu_strategic_depart(a,0,1);put(a,0x5910a4,2);break;
    case 0x4c6fb0:ok=game_menu_strategic_depart(a,1,0) && game_menu_strategic_depart(a,1,1);put(a,0x5910a4,3);break;
    case 0x4c6ff0:ok=game_menu_ingame_4c3110(a,0) && game_menu_ingame_4c3110(a,1);put(a,0x5910a4,4);break;
    case 0x4c7030:ok=game_menu_ingame_4c54c0(a,0) && game_menu_ingame_4c54c0(a,1);put(a,0x5910a4,5);break;
    case 0x4c7070:ok=game_menu_ingame_4c5cb0(a,0) && game_menu_ingame_4c5cb0(a,1);put(a,0x5910a4,6);break;
    case 0x4c70b0:ok=game_menu_ingame_4c6470(a,0) && game_menu_ingame_4c6470(a,1);put(a,0x5910a4,7);break;
    default:return 0;
    }
    if(!ok)return 0;
    for(i=0;i<8;++i)if(buttons[i].callback==method)return game_menu_widget_base_state(a,get(a,buttons[i].slot),0);
    return 0;
}
