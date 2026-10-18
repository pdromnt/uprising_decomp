#include "menu_network.h"
#include "menu_widgets.h"
#include "menu_level_catalog.h"
#include "game_initializers.h"
#include <stdio.h>
#include <string.h>
extern int game_menu_quick_catalog(GameApplication *,GameAddress,uint32_t);
extern int game_menu_register_observer(GameApplication *,GameAddress,uint32_t,uint32_t);
extern int game_menu_unregister_tick_method(GameApplication *,GameAddress,uint32_t);
extern int game_menu_request(GameApplication *,GameAddress,uint32_t,uint32_t,uint32_t);
extern int game_menu_controller_tick(GameApplication *,GameAddress);
extern GameAddress game_menu_controller_construct(GameApplication *);
extern void game_menu_controller_destroy(GameApplication *,GameAddress);
extern int game_menu_window_clear(GameApplication *);
extern int game_menu_palette_load_named(GameApplication *,GameAddress);
extern int game_network_level_capacity(GameApplication *,GameAddress,uint32_t *); /*4aa1c0*/
extern int game_network_enable_players(GameApplication *,int);
static uint32_t get(GameApplication *a,GameAddress p,unsigned o){return game_memory_read_u32(&a->memory,p+o);}
static void put(GameApplication *a,GameAddress p,unsigned o,uint32_t v){game_memory_write_u32(&a->memory,p+o,v);}
static int place(GameApplication *a,GameAddress p,uint32_t x,uint32_t y,uint32_t z,int enabled) {
    return p && game_menu_widget_position(a,p,x,y,z,0) && game_menu_widget_visible(a,p,1) && (!enabled || game_menu_widget_enable(a,p,1));
}
static int read_name(GameApplication *a,GameAddress p,char *out,size_t cap) {
    size_t i;for(i=0;i<cap;++i){out[i]=(char)game_memory_read_u8(&a->memory,p+(uint32_t)i);if(!out[i])return !a->memory.fault;}return 0;
}
/*491770: these slots own controls; the description/spinner are borrowed. The
 * combo's capture must retire before its parent controls disappear. */
int game_menu_network_page_release(GameApplication *a,GameAddress e) {
    static const unsigned slots[]={0x120,0xe8,8,0xf0,0x118,0x11c,0x110,0x114,0x10c,0x108,0x104};unsigned i;
    for(i=0;i<sizeof(slots)/sizeof(slots[0]);++i){GameAddress p=get(a,e,slots[i]);if(p && !game_menu_widget_destroy(a,p))return 0;put(a,e,slots[i],0);}
    for(i=0;i<4;++i){unsigned j;static const unsigned banks[]={0x124,0x144,0xf4,0x134};
        for(j=0;j<4;++j){GameAddress p=get(a,e,banks[j]+i*4);if(p && !game_menu_widget_destroy(a,p))return 0;put(a,e,banks[j]+i*4,0);}}
    return !a->memory.fault;
}
/*4918f0: refresh the existing roster, ready flags and chat; the local editor
 * survives remote refresh unless the caller explicitly requests all rows. */
int game_menu_network_page_refresh(GameApplication *a,GameAddress e,int all) {
    unsigned i;char text[60];GameAddress p;
    if(!get(a,0x5f0150,0)) {
        p=get(a,e,8);if(p){char *suffix;if(!read_name(a,0x5efe80,text,sizeof(text)))return 0;suffix=strstr(text,".lfl");if(suffix)*suffix=0;if(!game_menu_text_image_set_bytes(a,p,text,0))return 0;}
        p=get(a,0x58abe8,0);if(p && !game_menu_text_image_set(a,p,0x5ac9e0,0))return 0;
        for(i=0;i<2;++i){p=get(a,0x58abf4+i*4,0);if(p){snprintf(text,sizeof(text),"%d",(int32_t)get(a,i?0x5acbec:0x5ac910,0));if(!game_menu_text_image_set_bytes(a,p,text,0))return 0;}}
        if(all && !game_menu_request(a,get(a,0x59d4c0,0),530,1001,0))return 0;
        put(a,0x5eff00,0,0);
    }
    if(!get(a,e,0x124))return !a->memory.fault;
    for(i=0;i<4;++i) {
        GameAddress manager=get(a,0x5f0210+i*4,0),ready=get(a,e,0x144+i*4),edit=get(a,e,0x134+i*4);
        uint32_t state=get(a,ready,0x44),wanted;
        if(!all && manager==get(a,0x5f0208,0))continue;
        if(get(a,manager,0x468)) {
            if(!game_menu_text_image_set(a,get(a,e,0x124+i*4),manager+0x39c,0) || !game_menu_text_entry_set(a,edit,manager+0x31c))return 0;
            wanted=get(a,manager,0x4c4)!=0;
        } else {
            if(!game_menu_text_entry_set(a,edit,0x58c590) || !game_menu_text_image_set(a,get(a,e,0x124+i*4),get(a,manager,0x46c)?0x58c594:0x58c59c,0))return 0;
            wanted=0;
        }
        if((wanted && state==0) || (!wanted && state==2)){put(a,ready,0x94,wanted);state=wanted?2:0;}
        if(!game_menu_widget_base_state(a,ready,state))return 0;
        put(a,0x58fd3c,0,1);
    }
    return !a->memory.fault;
}
/*475770: host slot actions borrow the roster. The saved parent and current
 *controller own the temporary capture for this page. */
static int slot_popup(GameApplication *a,GameAddress e,int open) {
    GameAddress parent=get(a,0x58917c,0),capture=get(a,0x59d4c0,0),p,manager;unsigned i;uint32_t slot=get(a,0x5891a4,0);
    if(!open) {
        /*475770 owns its capture through58917c and the published controller.
         *Event+ec is reused by other pages; it is never a capture owner. */
        if(!parent)return !a->memory.fault;
        if(!capture || capture==parent)return 0;
        if(!game_menu_widget_visible(a,get(a,e,0x104),0) || !game_menu_widget_enable(a,get(a,e,0x108),0))return 0;
        for(i=0;i<3;++i){p=get(a,e,0x10c+i*4);if(!game_menu_widget_enable(a,p,0) || !game_menu_widget_visible(a,p,0))return 0;}
        game_menu_controller_destroy(a,capture);put(a,0x58917c,0,0);put(a,0x5891a4,0,UINT32_MAX);
        put(a,0x59d4c0,0,parent);put(a,0x58fd3c,0,1);return !a->memory.fault;
    }
    if(parent || slot>=4 || !(parent=capture))return 0;
    capture=game_menu_controller_construct(a);if(!capture)return 0;
    put(a,0x58917c,0,parent);put(a,0x59d4c0,0,capture);
    manager=get(a,0x5f0210+slot*4,0);
    if(!place(a,get(a,e,0x104),186,251+slot*29,99,0) || !game_menu_widget_enable(a,get(a,e,0x108),1))goto fail;
    for(i=0;i<3;++i) {
        unsigned offset=i==0?0x114:i==1?0x110:0x10c;
        int enabled=i==0?!get(a,manager,0x46c):i==1?!!get(a,manager,0x46c):!!get(a,manager,0x468);
        p=get(a,e,offset);if(!game_menu_widget_base_state(a,p,0) || !place(a,p,197,253+slot*29+i*13,100,enabled))goto fail;
    }
    if(!game_menu_register_observer(a,capture,0x475750,0))goto fail;return !a->memory.fault;
fail:(void)slot_popup(a,e,0);return 0;
}
int game_menu_network_page_state(GameApplication *a,GameAddress e,uint32_t phase) {
    static const GameAddress buttons[][4]={{0x58d910,0x58d8ec,0x58d8c8,0},{0x58d97c,0x58d958,0x58d934,0},{0x58d9ec,0x58d9c4,0x58d9a0,0}};
    static const unsigned slots[]={0xf0,0x118,0x11c};static const uint32_t callbacks[]={0x475530,0x475710,0x4756f0},xs[]={95,430,263};
    GameAddress p,row,local=get(a,0x5f0208,0);unsigned i;int host=!!get(a,0x5f0150,0);char text[128];
    if(phase==1) {
        if(!slot_popup(a,e,0))return 0;
        p=get(a,e,0x120);if(p && !game_menu_combo_close(a,p,0))return 0;
        if(!game_menu_unregister_tick_method(a,get(a,0x59d4c0,0),0x474d40) || !game_menu_unregister_tick_method(a,get(a,0x59d4c0,0),0x474eb0))return 0;
        if(get(a,0x58abe8,0) && !game_menu_widget_visible(a,get(a,0x58abe8,0),0))return 0;
        if(get(a,e,0x60) && !game_menu_widget_visible(a,get(a,e,0x60),0))return 0;
        if(get(a,e,0x14)!=16 && get(a,e,0x14)!=22){if(!game_network_session_reset(a) || !game_menu_palette_load_named(a,0x58dbcc))return 0;}
        return get(a,e,0x14)==22?!a->memory.fault:game_menu_network_page_release(a,e);
    }
    if(phase)return !a->memory.fault;
    if(get(a,e,0x3c)==22) {
        if(!game_menu_widget_base_state(a,get(a,e,0x11c),0) || !game_menu_widget_base_state(a,get(a,e,0xe8),get(a,get(a,e,0xe8),0x44)))return 0;
    } else {
        if(!local)return 0;put(a,0x602bb0,0,0);
        p=game_menu_image_create(a,UINT32_MAX,0x58d8a4);if(!p)goto fail;put(a,e,0xe8,p);if(!place(a,p,0,0,0,0))goto fail;
        for(i=0;i<3;++i){p=game_menu_button_create(a,240+i,buttons[i],0,UINT32_MAX,0,callbacks[i]);if(!p)goto fail;put(a,e,slots[i],p);
            if(!place(a,p,xs[i],386,10,i || host))goto fail;if(i){put(a,p,0x50,144);put(a,p,0x54,i==1?146:145);}}
        if(!game_menu_widget_position(a,get(a,0x58abe8,0),250,127,10,0))goto fail;
        if(host) {
            GameAddress names[4]={0x58dab4,0x58da8c,0x58da64,0};
            p=game_menu_combo_create(a,522,names,98,129,0x58da3c,0x58da14,123,248,0);if(!p)goto fail;put(a,e,0x120,p);
            if(!place(a,p,208,129,10,1) || !game_menu_quick_catalog(a,get(a,p,0x94),0) || !game_menu_combo_open(a,p))goto fail;
            row=get(a,get(a,p,0x94),0x5c);
            if(row && (!game_menu_button_input(a,row,0x201,0,0) || !game_menu_button_input(a,row,0x202,0,0)))goto fail;
            put(a,0x602bb0,0,1);
        } else {
            GameMenuTextStyle style={0,255,0,3,120,18,2,2,0};p=game_menu_text_image_create(a,0x5efe80,&style);if(!p)goto fail;put(a,e,8,p);
            if(!place(a,p,98,129,12,0))goto fail;
        }
        p=game_menu_image_create(a,UINT32_MAX,0x58dadc);if(!p)goto fail;put(a,e,0x104,p);if(!game_menu_image_mode(a,p,101))goto fail;
        for(i=0;i<3;++i){uint32_t callback=i==0?0x475bc0:i==1?0x475ae0:0x475b60;
            if(!read_name(a,0x58dafc+i*8,text,sizeof(text)))goto fail;
            p=game_menu_text_button_create(a,text,UINT32_MAX,UINT32_MAX,5,0,0,callback,2);if(!p)goto fail;put(a,e,0x10c+i*4,p);put(a,p,0x64,0);put(a,p,0x50,144);put(a,p,0x54,145);}
        p=game_menu_click_region_create(a,UINT32_MAX,UINT32_MAX,UINT32_MAX,0,0x475c30);if(!p)goto fail;put(a,e,0x108,p);put(a,p,0x14,640);put(a,p,0x18,480);
        for(i=0;i<4;++i) {
            uint32_t y=263+i*29;GameMenuTextStyle style={game_memory_read_u8(&a->memory,0x58b030+i*3),game_memory_read_u8(&a->memory,0x58b031+i*3),game_memory_read_u8(&a->memory,0x58b032+i*3),2,83,15,0,0,0};
            p=game_menu_text_image_create(a,0x58db14,&style);if(!p)goto fail;put(a,e,0x124+i*4,p);if(!place(a,p,96,y,10,0))goto fail;
            if(host && game_memory_read_u16(&a->memory,local+8)!=i){GameAddress names[4]={0x58db1c,0,0,0};p=game_menu_button_create(a,UINT32_MAX,names,0,UINT32_MAX,i,0x475aa0);if(!p)goto fail;put(a,e,0xf4+i*4,p);if(!game_menu_image_mode(a,get(a,p,0x58),101) || !place(a,p,186,y+1,10,1))goto fail;}
            {GameAddress names[4]={0x58db84,0x58db60,0x58db40,0};p=game_menu_checkbox_create(a,names,0x4754f0);if(!p)goto fail;put(a,p,4,241+i*3);put(a,p,0x50,144);put(a,p,0x54,145);put(a,e,0x144+i*4,p);if(!place(a,p,199,y-2,10,game_memory_read_u16(&a->memory,local+8)==i))goto fail;}
            p=game_menu_text_entry_create(a,552+i*3,277,15,2,0x58dba8);if(!p)goto fail;put(a,e,0x134+i*4,p);put(a,p,0x1cc,33);put(a,p,0x1d0,1);
            put(a,p,0x58,style.red);put(a,p,0x5c,style.green);put(a,p,0x60,style.blue);if(!place(a,p,265,y,10,0))goto fail;
            if(game_memory_read_u16(&a->memory,local+8)==i){game_memory_write_u8(&a->memory,p+0x100,13);put(a,p,0x104,0x4754b0);put(a,p,0x1c8,1);put(a,p,0x70,0x475480);if(!game_menu_text_entry_activate(a,p))goto fail;}
        }
    }
    if(!place(a,get(a,e,0x60),544,383,12,0) || !game_menu_widget_visible(a,get(a,0x58abe8,0),1) ||
       !game_menu_register_observer(a,get(a,0x59d4c0,0),0x474eb0,0) || !game_menu_register_observer(a,get(a,0x59d4c0,0),0x474d40,0))goto fail;
    if(get(a,e,0x3c)!=22 && (!game_menu_window_clear(a) || !game_menu_palette_load_named(a,0x58dbac)))goto fail;
    return game_menu_network_page_refresh(a,e,1);
fail:
    (void)slot_popup(a,e,0);p=get(a,e,0x120);if(p)(void)game_menu_combo_close(a,p,0);(void)game_menu_network_page_release(a,e);return 0;
}
int game_menu_network_page_action(GameApplication *a,uint32_t method,GameAddress receiver,uint32_t argument) {
    GameAddress e=get(a,0x59d4c4,0),local=get(a,0x5f0208,0),manager;uint32_t slot=get(a,0x5891a4,0);
    if(method==0x475480 || method==0x4754b0){put(a,0x5eff30,0,1);if(method==0x4754b0 && !game_menu_text_entry_set(a,receiver,0x58970c))return 0;return local && game_menu_text_entry_copy(a,receiver,local+0x31c,128);}
    if(method==0x4754f0){unsigned index;if(!local || (index=game_memory_read_u16(&a->memory,local+8))>=4)return 0;put(a,get(a,0x5f0210+index*4,0),0x4c4,get(a,receiver,0x94));put(a,0x5f003c,0,1);return !a->memory.fault;}
    if(method==0x4756f0 || method==0x475710)return game_menu_request(a,get(a,0x59d4c0,0),69,1000,method==0x4756f0?22:get(a,0x5f0150,0)?13:14);
    if(method==0x475530) {
        uint32_t count=0,capacity,start,elapsed=0;unsigned i;
        if(!get(a,0x589180,0))return game_menu_sound(a,146);
        for(i=0;i<4;++i){manager=get(a,0x5f0210+i*4,0);if(get(a,manager,0x468)){++count;if(manager!=local && !get(a,manager,0x4c4))return game_menu_sound(a,146);}}
        if(count<2)return game_menu_sound(a,146);
        if(!game_network_level_capacity(a,0x5acc90,&capacity))return 0;
        if((int32_t)capacity<(int32_t)count) {
            char format[128],level[60],message[128];int n;
            if(!game_menu_sound(a,4) || !read_name(a,0x589710,format,sizeof(format)) || !read_name(a,0x5efe80,level,sizeof(level)))return 0;
            n=snprintf(message,sizeof(message),format,level,(int32_t)capacity,(int32_t)count);
            if(n<0 || n>=(int)sizeof(message) || !game_memory_write(&a->memory,local+0x31c,message,(size_t)n+1))return 0;
            put(a,0x5eff30,0,1);return game_menu_sound(a,146);
        }
        (void)game_network_enable_players(a,0);start=game_network_milliseconds(a);
        for(i=0;i<4;++i){if(i)while(game_network_milliseconds(a)-start<=elapsed+get(a,0x5efee8,0)){}
            elapsed=game_network_milliseconds(a)-start;(void)game_network_publish_initial_state(a,0,3);}
        return game_menu_request(a,get(a,0x59d4c0,0),69,1000,16) && game_menu_sound(a,145);
    }
    if(method==0x475aa0){if(slot==UINT32_MAX){if(argument>=4)return 0;put(a,0x5891a4,0,argument);return slot_popup(a,e,1);}return slot_popup(a,e,0);}
    if(method==0x475c30)return slot_popup(a,e,0);
    if(method==0x475ae0 || method==0x475b60 || method==0x475bc0) {
        if(slot>=4)return 0;manager=get(a,0x5f0210+slot*4,0);
        if(slot) {
            if(method==0x475b60){if(!get(a,manager,0x468) && !get(a,manager,0x46c)){put(a,manager,0x46c,1);put(a,0x5efee0,0,1);}}
            else {if(get(a,manager,0x468)){(void)game_network_player_destroy(a,get(a,manager,0x3c0));if(!game_network_manager_reset(a,manager))return 0;}
                if(method==0x475ae0)put(a,manager,0x46c,0);put(a,0x5efee0,0,1);}
        }
        return slot_popup(a,e,0) && game_menu_network_page_refresh(a,e,0);
    }
    return 0;
}
/*474d40/474eb0: native lobby observers drive the connected codecs, SDK owner
 * and UI consumers. Transport failures are retained as failures; no CPU route. */
int game_menu_network_page_tick(GameApplication *a,uint32_t method) {
    GameAddress local=get(a,0x5f0208,0),host=get(a,0x5eff28,0),e=get(a,0x59d4c4,0);unsigned i;
    uint32_t now=game_network_milliseconds(a),from;uint8_t mask=0,packet[130]={0};uint16_t received;
    if(method==0x475750)return game_menu_controller_tick(a,get(a,0x58917c,0));
    /*Start has closed admission and handed the next packets to level startup.
     *A final queued lobby observer must not reopen joins or drain those packets
     *before phase1 unregisters these handlers during the pending16 transition.*/
    if((method==0x474d40 || method==0x474eb0) && get(a,e,0x14)==16)return !a->memory.fault;
    if(!local || !host)return 0;from=get(a,local,0x3c0);
    for(i=0;i<4;++i){GameAddress p=get(a,0x5f0210+i*4,0);if(get(a,p,0x468))mask|=(uint8_t)(1u<<i);if(get(a,p,0x46c))mask|=(uint8_t)(16u<<i);}
    if(method==0x474d40) {
        if((int32_t)(get(a,0x589198,0)+1000u)>(int32_t)now)return !a->memory.fault;
        put(a,0x589198,0,now);
        if(!get(a,0x5f0150,0)){packet[0]=12;packet[1]=mask;(void)game_network_send(a,from,get(a,host,0x3c0),packet,2);}
        return !a->memory.fault;
    }
    if(method!=0x474eb0)return 0;
    if((int32_t)get(a,0x58919c,0)>(int32_t)now)return !a->memory.fault;put(a,0x58919c,0,now);
    if(get(a,0x5f0150,0)) {
        unsigned active=0;int ready=1;
        if(get(a,0x5efee0,0)) {
            int available=0;packet[0]=11;packet[1]=mask;(void)game_network_send(a,from,0,packet,2);
            for(i=0;i<4;++i){GameAddress p=get(a,0x5f0210+i*4,0);if(!get(a,p,0x468) && get(a,p,0x46c)){available=1;break;}}
            (void)game_network_enable_players(a,available);put(a,0x5efee0,0,0);
        }
        if(get(a,0x5eff08,0) || (int32_t)(get(a,0x5891a0,0)+1000u)<(int32_t)now) {
            char level[64];if(!read_name(a,0x5efe80,level,sizeof(level)))return 0;memset(packet,0,65);packet[0]=5;memcpy(packet+1,level,strlen(level)+1);
            (void)game_network_send(a,from,0,packet,65);put(a,0x5eff08,0,0);put(a,0x5891a0,0,now);
        }
        if(get(a,0x5eff00,0)){(void)game_network_publish_world(a,0,6);put(a,0x5eff00,0,0);}
        for(i=0;i<4;++i){GameAddress p=get(a,0x5f0210+i*4,0);if(get(a,p,0x468)){++active;if(p!=local && !get(a,p,0x4c4))ready=0;}}
        put(a,0x589180,0,ready && active>=2);
    }
    if(get(a,0x5f003c,0)) {
        if(get(a,0x5f0150,0))(void)game_network_publish_roster(a,0,0,2);
        else {uint32_t index=game_memory_read_u16(&a->memory,local+8);memset(packet,0,120);packet[0]=2;
            if(!game_network_selection_pack(a,packet,index))return 0;(void)game_network_send(a,from,get(a,get(a,0x5f0210,0),0x3c0),packet,120);}
        put(a,0x5f003c,0,0);
    }
    if(get(a,0x5eff30,0)) {
        if(get(a,0x5f0150,0))(void)game_network_publish_chat(a,0,0);
        else {char text[128];if(!read_name(a,local+0x31c,text,sizeof(text)))return 0;memset(packet,0,sizeof(packet));packet[0]=4;packet[1]=(uint8_t)game_memory_read_u16(&a->memory,local+8);memcpy(packet+2,text,strlen(text)+1);(void)game_network_send(a,from,get(a,host,0x3c0),packet,130);}
        put(a,0x5eff30,0,0);
    }
    if(!game_network_receive_batch(a,&received) || received==UINT16_MAX)return 0;
    if(received){if(!game_menu_network_page_refresh(a,e,1u-get(a,0x589190,0)))return 0;put(a,0x589190,0,0);}
    if(!get(a,host,0x468)) {
        char text[256],caption[64];(void)game_network_session_reset(a);
        if(!read_name(a,0x5896d4,text,sizeof(text)) || !read_name(a,0x5896e8,caption,sizeof(caption)))return 0;
        (void)game_application_error(a,text,caption);
        return game_menu_request(a,get(a,0x59d4c0,0),69,1000,14);
    }
    if(get(a,0x5efef8,0) && get(a,0x5efee4,0)) {
        for(i=0;i<4;++i)if(get(a,get(a,0x5f0210+i*4,0),0x468) && !get(a,0x5eff10+i*4,0))break;
        if(i==4)return game_menu_request(a,get(a,0x59d4c0,0),69,1000,16);
    }
    return !a->memory.fault;
}
