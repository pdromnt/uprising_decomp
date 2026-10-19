#include "menu_network.h"
#include "menu_widgets.h"
#include "menu_level_catalog.h"
#include <string.h>
extern int game_menu_demo_start(GameApplication *,unsigned);
extern int game_menu_parameters_reset(GameApplication *);
extern int game_menu_request(GameApplication *,GameAddress,uint32_t,uint32_t,uint32_t);
extern int game_menu_register_observer(GameApplication *,GameAddress,uint32_t,uint32_t);
extern int game_menu_unregister_tick_method(GameApplication *,GameAddress,uint32_t);
static int sessions(GameApplication *,GameAddress);
static uint32_t get(GameApplication *a,GameAddress p,unsigned o){return game_memory_read_u32(&a->memory,p+o);}
static void put(GameApplication *a,GameAddress p,unsigned o,uint32_t v){game_memory_write_u32(&a->memory,p+o,v);}
static int text(GameApplication *a,GameAddress p,char *out,size_t capacity) {
    size_t i;for(i=0;i<capacity;++i){out[i]=(char)game_memory_read_u8(&a->memory,p+(uint32_t)i);if(!out[i])return !a->memory.fault;}return 0;
}
static int place(GameApplication *a,GameAddress p,uint32_t x,uint32_t y,uint32_t z,int input) {
    return p && game_menu_widget_position(a,p,x,y,z,0) && game_menu_widget_visible(a,p,1) && (!input || game_menu_widget_enable(a,p,1));
}
int game_menu_network_provider_row(GameApplication *a,GameAddress selector,const uint8_t guid[16],const char *name) {
    static const GameAddress guids[]={0x577ce8,0x577cd8,0x577cc8,0x577cb8},labels[]={0x5896b0,0x5896b8,0x5896bc,0x5896c4};
    uint8_t known[16];char label[128];GameAddress row;unsigned i;
    if(!guid || !name)return 0;
    for(i=0;i<4;++i){if(!game_memory_read(&a->memory,guids[i],known,16))return 0;if(!memcmp(guid,known,16))break;}
    if(i<4){if(!text(a,labels[i],label,sizeof(label)))return 0;name=label;}
    /*The GUID lives in otherwise unused tail bytes of this owned row. Its
     * callback takes a bounded record identity, never the SDK's host pointer. */
    if(strlen(name)>=128)return 0;
    row=game_menu_text_button_create(a,name,UINT32_MAX,UINT32_MAX,3,0,0,0x474280,4);if(!row)return 0;
    put(a,row,4,570+get(a,0x5eff24,0));put(a,row,0x64,row+0x30c);
    if(!game_memory_write(&a->memory,row+0x30c,guid,16) || !game_menu_selector_append(a,selector,row,0,0)) {
        (void)game_menu_widget_destroy(a,row);return 0;
    }
    return !a->memory.fault;
}
static int spacer(GameApplication *a,GameAddress selector) {
    GameAddress names[4]={0},image=game_menu_image_surface_create(a,8,10,0),row;
    if(!image)return 0;
    row=game_menu_button_create(a,UINT32_MAX,names,0,UINT32_MAX,0,0);
    if(!row){(void)game_menu_widget_destroy(a,image);return 0;}
    put(a,row,0x58,image);put(a,row,0x90,image);put(a,row,0x28,8);put(a,row,0x2c,10);
    if(!game_menu_selector_append(a,selector,row,0,0)){(void)game_menu_widget_destroy(a,row);return 0;}return !a->memory.fault;
}
static int providers(GameApplication *a,GameAddress selector) {
    put(a,0x5eff24,0,0);put(a,0x5eff2c,0,0);
    return game_menu_selector_clear(a,selector) && spacer(a,selector) && game_network_providers(a,selector) && spacer(a,selector);
}
/*495838: actual provider/name/host/join screen, sharing the reconstructed
 * text entry and selector owners. Every row retains its copied GUID lifetime. */
int game_menu_network_provider_state(GameApplication *a,GameAddress e,uint32_t phase) {
    static const GameAddress button_names[][4]={{0x58d684,0x58d65c,0x58d638,0},{0x58d6f4,0x58d6d0,0x58d6ac,0},{0x58d764,0x58d73c,0x58d718,0}};
    static const GameAddress slots[]={0x58b12c,0x58b128,0x58b130};
    static const uint32_t callbacks[]={0x4743e0,0x474310,0},ys[]={331,376,376},xs[]={95,95,369};
    GameAddress p;unsigned i;char name[60];
    if(phase==1) {
        if(get(a,e,0x60) && !game_menu_widget_visible(a,get(a,e,0x60),0))return 0;
        if(get(a,e,0x14)==2)put(a,0x5eff04,0,0);
        if(!game_memory_read_u8(&a->memory,0x5efec0)) {
            if(!text(a,0x58d78c,name,sizeof(name)) || !game_memory_write(&a->memory,0x5efec0,name,strlen(name)+1))return 0;
        }
        for(i=0;i<6;++i){p=get(a,0x58b11c+i*4,0);if(p && !game_menu_widget_destroy(a,p))return 0;put(a,0x58b11c+i*4,0,0);}
        put(a,0x5eff2c,0,0);return !a->memory.fault;
    }
    if(phase)return !a->memory.fault;
    game_network_dispose(a);put(a,0x5eff04,0,1);
    p=game_menu_image_create(a,UINT32_MAX,0x58d5e4);if(!p)goto fail;put(a,0x58b11c,0,p);if(!place(a,p,0,0,0,0))goto fail;
    if(!game_memory_read_u8(&a->memory,0x5efec0)){if(!text(a,0x58d608,name,sizeof(name)) || !game_memory_write(&a->memory,0x5efec0,name,strlen(name)+1))goto fail;}
    p=game_menu_text_entry_create(a,550,240,13,2,0x5efec0);if(!p)goto fail;put(a,0x58b120,0,p);
    if(!place(a,p,100,174,10,1))goto fail;
    game_memory_write_u8(&a->memory,p+0x100,13);game_memory_write_u8(&a->memory,p+0x114,9);put(a,p,0x1c8,2);put(a,p,0x1cc,11);put(a,p,0x70,0x473f20);
    if(!game_menu_text_entry_activate(a,p))goto fail;
    p=game_menu_selector_create(a,551,15,0x4740d0,0x58d610);if(!p)goto fail;put(a,0x58b124,0,p);
    if(!game_menu_selector_resize(a,p,440,68) || !place(a,p,101,240,10,1) || !providers(a,p))goto fail;
    for(i=0;i<3;++i) {
        p=game_menu_button_create(a,552+i,button_names[i],i==2?69:0,i==2?1000:UINT32_MAX,i==2?2:0,callbacks[i]);if(!p)goto fail;put(a,slots[i],0,p);
        put(a,p,0x50,144);put(a,p,0x54,i==2?146:UINT32_MAX);if(!place(a,p,xs[i],ys[i],10,1))goto fail;
    }
    return place(a,get(a,e,0x60),544,383,12,0);
fail:(void)game_menu_network_provider_state(a,e,1);return 0;
}
int game_menu_network_connection_action(GameApplication *a,uint32_t method,GameAddress receiver,uint32_t argument) {
    char name[128];uint8_t guid[16],ipx[16];GameAddress selected;
    if(method==0x473f20) {
        char *cursor;if(!text(a,receiver+0x78,name,sizeof(name)))return 0;
        if(strlen(name)>=31){name[31]=0;game_memory_write_u8(&a->memory,receiver+0x78+31,0);}
        if(!game_memory_write(&a->memory,0x5efec0,name,strlen(name)+1))return 0;
        cursor=strchr(name,'_');if(cursor)game_memory_write_u8(&a->memory,receiver+0x78+(uint32_t)(cursor-name),0);return !a->memory.fault;
    }
    if(method==0x4740d0)return providers(a,receiver);
    if(method==0x474280) {
        if(!argument || !game_memory_read(&a->memory,argument,guid,16) || !game_memory_read(&a->memory,0x577cd8,ipx,16))return 0;
        put(a,0x5eff2c,0,argument);put(a,0x5eff20,0,!memcmp(guid,ipx,16));return !a->memory.fault;
    }
    if(method==0x474920)return sessions(a,receiver);
    if(method==0x474ab0){put(a,0x5efeec,0,argument);return !a->memory.fault;}
    if(method==0x474ac0) {
        selected=get(a,0x5efeec,0);
        if(!selected || !game_memory_read(&a->memory,selected,guid,16))return game_menu_sound(a,146);
        if(!game_menu_widget_enable(a,receiver,0))return 0;
        if(game_network_session_initialize(a) && game_network_session_open(a,0,NULL,guid) &&
           game_menu_parameters_reset(a) && game_menu_demo_start(a,0))
            return game_menu_sound(a,145) && game_menu_request(a,get(a,0x59d4c0,0),69,1000,15);
        (void)game_network_transport_close(a);
        {
            unsigned i;for(i=0;i<4;++i){GameAddress manager=get(a,0x5f0210+i*4,0);
                if(!game_network_manager_reset(a,manager))return 0;put(a,manager,0x46c,1);}
        }
        game_memory_write_u8(&a->memory,0x5efe80,0);put(a,0x5f0208,0,0);put(a,0x5eff28,0,0);
        put(a,0x5f0150,0,0);put(a,0x624f8c,0,1);
        return game_menu_widget_enable(a,receiver,1) && game_menu_sound(a,146);
    }
    if(method!=0x474310 && method!=0x4743e0)return 0;
    if(method==0x4743e0 && get(a,0x597084,0)) {
        put(a,0x597084,0,0);
    }
    if(method==0x4743e0 && !game_menu_widget_enable(a,receiver,0))return 0;
    selected=get(a,0x5eff2c,0);
    if(!selected || !text(a,0x5efec0,name,sizeof(name)) || !*name ||
       !game_memory_read(&a->memory,selected,guid,16) || !game_network_provider_open(a,guid))goto fail;
    if(method==0x474310){put(a,0x5f0150,0,0);return game_menu_sound(a,145) && game_menu_request(a,get(a,0x59d4c0,0),69,1000,14);}
    if(!game_network_session_initialize(a))goto fail;put(a,0x5f0150,0,1);
    if(!game_network_session_open(a,1,name,NULL))goto fail;
    (void)game_network_enable_players(a,1);
    if(!game_menu_parameters_reset(a) || !game_menu_demo_start(a,0))goto fail;
    put(a,0x589180,0,0);
    return game_menu_sound(a,145) && game_menu_request(a,get(a,0x59d4c0,0),69,1000,15);
fail:
    if(method==0x4743e0){game_network_dispose(a);if(!game_menu_widget_enable(a,receiver,1))return 0;}
    return game_menu_sound(a,146);
}
/*474690/474840/4748e0: each selector row owns its instance GUID. This replaces
 * the reallocating global GUID vector and sorted pending-row compatibility list.
 * Selection is borrowed until refresh snapshots its sixteen bytes. */
int game_menu_network_session_row(GameApplication *a,GameAddress selector,const uint8_t guid[16],const char *name,const uint8_t *selected) {
    GameAddress row;int32_t x,y;
    if(!guid || !name || strlen(name)>=128)return 0;
    row=game_menu_text_button_create(a,name,UINT32_MAX,UINT32_MAX,3,0,0,0x474ab0,4);if(!row)return 0;
    put(a,row,4,570+get(a,0x5eff24,0));put(a,row,0x64,row+0x30c);
    if(!game_memory_write(&a->memory,row+0x30c,guid,16) || !game_menu_selector_append(a,selector,row,0,0)) {
        (void)game_menu_widget_destroy(a,row);return 0;
    }
    if(selected && !memcmp(guid,selected,16)) {
        x=(int32_t)(get(a,row,0x20)+2u);y=(int32_t)(get(a,row,0x24)+2u);
        if(!game_menu_selector_pointer(a,selector,0x201,x,y) || !game_menu_selector_pointer(a,selector,0x202,x,y))return 0;
    }
    return !a->memory.fault;
}
static int sessions(GameApplication *a,GameAddress selector) {
    uint8_t selected[16];GameAddress p=get(a,0x5efeec,0);int have=p!=0;
    if(have && !game_memory_read(&a->memory,p,selected,16))return 0;
    put(a,0x5efeec,0,0);put(a,0x5eff34,0,0);
    return game_menu_selector_clear(a,selector) && game_network_sessions(a,selector,have?selected:NULL);
}
int game_menu_network_session_tick(GameApplication *a) {
    uint32_t now;if(get(a,0x5f0150,0))return !a->memory.fault;
    now=game_network_milliseconds(a);
    if((int32_t)(get(a,0x589194,0)+1000u)<=(int32_t)now) {
        if(!game_menu_request(a,get(a,0x59d4c0,0),630,1001,UINT32_MAX))return 0;
        put(a,0x589194,0,game_network_milliseconds(a));
    }
    return !a->memory.fault;
}
int game_menu_network_session_state(GameApplication *a,GameAddress e,uint32_t phase) {
    static const GameAddress names[][4]={{0x58d808,0x58d7e0,0x58d7bc,0},{0x58d87c,0x58d854,0x58d82c,0}};
    GameAddress p,controller=get(a,0x59d4c0,0);unsigned i;
    if(phase==1) {
        if(!game_menu_widget_visible(a,get(a,e,0x60),0) || !game_menu_unregister_tick_method(a,controller,0x474a60))return 0;
        put(a,0x5efeec,0,0);put(a,0x5eff34,0,0);
        for(i=0;i<4;++i){p=get(a,0x58b134+i*4,0);if(p && !game_menu_widget_destroy(a,p))return 0;put(a,0x58b134+i*4,0,0);}
        return !a->memory.fault;
    }
    if(phase==2) {
        if(get(a,e,0x1c)!=1)return !a->memory.fault;
        if(get(a,0x5eff20,0))return game_menu_register_observer(a,controller,0x474a60,0);
        return sessions(a,get(a,0x58b140,0));
    }
    if(phase)return !a->memory.fault;
    put(a,0x5efeec,0,0);put(a,0x5eff34,0,0);
    p=game_menu_image_create(a,UINT32_MAX,0x58d794);if(!p)goto fail;put(a,0x58b134,0,p);if(!place(a,p,0,0,0,0))goto fail;
    p=game_menu_selector_create(a,630,11,0x474920,0);if(!p)goto fail;put(a,0x58b140,0,p);
    if(!game_menu_selector_resize(a,p,440,70) || !place(a,p,100,208,10,1))goto fail;
    for(i=0;i<2;++i) {
        p=game_menu_button_create(a,590+i,names[i],i?69:0,i?1000:UINT32_MAX,i?13:0,i?0:0x474ac0);if(!p)goto fail;
        put(a,0x58b138+i*4,0,p);put(a,p,0x50,144);put(a,p,0x54,i?146:UINT32_MAX);
        if(!place(a,p,i?369:95,376,10,1))goto fail;
    }
    put(a,e,0x24,1);put(a,e,0x1c,2);
    return place(a,get(a,e,0x60),544,383,12,0);
fail:(void)game_menu_network_session_state(a,e,1);return 0;
}
