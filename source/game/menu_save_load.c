#include "menu_widgets.h"
#include "menu_level_catalog.h"
#include "menu_settings.h"
#include "application_archives.h"
#include <stdio.h>
#include <string.h>
extern int game_menu_request(GameApplication *,GameAddress,uint32_t,uint32_t,uint32_t);
extern int game_menu_selection_begin(GameApplication *);
extern int game_menu_controller_keyboard_handler(GameApplication *,GameAddress,uint32_t,uint32_t);
static uint32_t get(GameApplication *a,GameAddress p,unsigned o){return game_memory_read_u32(&a->memory,p+o);}
static void put(GameApplication *a,GameAddress p,unsigned o,uint32_t v){game_memory_write_u32(&a->memory,p+o,v);}
static int place(GameApplication *a,GameAddress p,uint32_t x,uint32_t y,uint32_t z,int enable) {
    return p && game_menu_widget_position(a,p,x,y,z,0) && game_menu_widget_visible(a,p,1) && (!enable || game_menu_widget_enable(a,p,1));
}
static int selected_name(GameApplication *a,GameAddress p,char name[128]) {
    unsigned i;if(!p)return 0;
    for(i=0;i<128;++i){name[i]=(char)game_memory_read_u8(&a->memory,p+i);if(!name[i])return !a->memory.fault;}
    return 0;
}
static int save_path(char path[160],const char *name,const char *extension) {
    int n;if(!*name || strchr(name,'/') || strchr(name,'\\') || strchr(name,':'))return 0;
    n=snprintf(path,160,"saves\\%s%s",name,extension);return n>=0 && n<160;
}
/*00524090/00524180: the selector owns its heterogeneous text/button children;
 * selection globals only borrow text from those children until teardown. */
int game_menu_save_catalog(GameApplication *a,GameAddress selector,int scenario) {
    GameLevelNames names;unsigned i;
    if(!game_menu_selector_clear(a,selector) ||
       !game_menu_level_files(a,&names,scenario?"saves\\*.sfl":"saves\\*.sav",scenario?".sfl":".sav",NULL,0))return 0;
    for(i=0;i<names.count;++i) {
        GameAddress row=game_menu_level_row_create(a,names.names[i],412,24,5,10,2,scenario?0x5242b0:0x524270);
        if(!row)return 0;
        if(!game_menu_selector_append(a,selector,row,0,0)){game_menu_widget_destroy(a,row);return 0;}
    }
    return !a->memory.fault;
}
/*0049544d: construct, register, populate and retire the actual state12 browser.
 * Borrowed shared controls remain with the event; all new widgets are published
 * immediately so partial construction can release their normal owners. */
int game_menu_save_state(GameApplication *a,GameAddress event,uint32_t phase) {
    static const GameAddress names[2][4]={{0x58d3cc,0x58d3ac,0x58d38c,0},{0x58d430,0x58d40c,0x58d3ec,0}};
    static const GameAddress slots[]={0x58b0f4,0x58b0f0};GameAddress p,controller=get(a,0x59d4c0,0);unsigned i;int ok=1;
    if(!phase) {
        p=game_menu_image_create(a,UINT32_MAX,0x58d320);if(!p)goto fail;put(a,0x58b0f8,0,p);
        if(!place(a,p,0,0,0,0))goto fail;
        for(i=0;i<2;++i) {
            p=game_menu_selector_create(a,0x2a1+i,15,i?0x524180:0x524090,i?0x58d368:0x58d344);
            if(!p)goto fail;
            put(a,event,0x170+4*i,p);
            if(!game_menu_selector_resize(a,p,435,110) || !place(a,p,104,130+139*i,10,1) || !game_menu_save_catalog(a,p,(int)i))goto fail;
        }
        for(i=0;i<2;++i) {
            p=game_menu_button_create(a,0x118,names[i],UINT32_MAX,UINT32_MAX,0,i?0x5246b0:0x524480);
            if(!p)goto fail;
            put(a,slots[i],0,p);
            if(!place(a,p,i?457:95,393,10,1))goto fail;
            put(a,p,0x50,144);put(a,p,0x54,i?146:UINT32_MAX);
        }
        if(!place(a,get(a,event,0x60),544,383,12,0) || !game_menu_controller_keyboard_handler(a,controller,0x5242f0,UINT32_MAX))goto fail;
        return !a->memory.fault;
    }
    if(phase!=1)return !a->memory.fault;
    if(!game_menu_widget_visible(a,get(a,event,0x60),0))ok=0;
    put(a,controller,24,0); /*004cdb30 clears the shared handler only. */
    put(a,0x59811c,0,0);put(a,0x598120,0,0);
    for(i=0;i<5;++i) {
        GameAddress slot=i<2?slots[i]:i<4?event+0x170+4*(i-2):0x58b0f8;
        p=get(a,slot,0);if(p && !game_menu_widget_destroy(a,p))ok=0;else put(a,slot,0,0);
    }
    return ok && !a->memory.fault;
fail:(void)game_menu_save_state(a,event,1);return 0;
}
/*0045ddd0: the first little-endian word selects running versus completed SAV.
 * A short read leaves the unread bytes zero, as the original initialized word. */
static uint32_t save_header(GameApplication *a,const char *path) {
    uint8_t bytes[4]={0};GameAddress id=game_application_archive_open(a,path,0);
    if(!id)return 0;
    (void)resource_file_read(game_application_archive_file(a,id),bytes,4);game_application_archive_close(a,id);
    return (uint32_t)bytes[0]|(uint32_t)bytes[1]<<8|(uint32_t)bytes[2]<<16|(uint32_t)bytes[3]<<24;
}
int game_menu_save_action(GameApplication *a,uint32_t method,GameAddress receiver,uint32_t key) {
    GameAddress event=get(a,0x59d4c4,0),controller=get(a,0x59d4c0,0),choice;char name[128],path[160];uint32_t route;
    if(method==0x524090 || method==0x524180)return game_menu_save_catalog(a,receiver,method==0x524180);
    if(method==0x524270 || method==0x5242b0) {
        int scenario=method==0x5242b0;GameAddress other=get(a,event,scenario?0x170:0x174),frame=get(a,receiver,0x58);
        put(a,scenario?0x59811c:0x598120,0,0);
        if(other && !game_menu_selector_select(a,other,0))return 0;
        put(a,scenario?0x598120:0x59811c,0,frame?get(a,frame,0x16c):0);return !a->memory.fault;
    }
    if(method==0x5246b0) {
        put(a,0x59811c,0,0);put(a,0x598120,0,0);return game_menu_request(a,controller,69,1000,3);
    }
    if(method==0x5242f0) {
        if(key!=46)return !a->memory.fault;
        choice=get(a,0x59811c,0);if(!choice)choice=get(a,0x598120,0);if(!choice)return !a->memory.fault;
        if(!selected_name(a,choice,name) || !save_path(path,name,get(a,0x59811c,0)?".sav":".sfl"))return 0;
        if(!game_application_file_delete(a,path))return game_menu_sound(a,146);
        put(a,0x59811c,0,0);put(a,0x598120,0,0);
        return game_menu_request(a,controller,0x2a2,1001,0) && game_menu_request(a,controller,0x2a1,1001,0);
    }
    if(method!=0x524480)return 0;
    choice=get(a,0x59811c,0);if(!choice)choice=get(a,0x598120,0);if(!choice)return game_menu_sound(a,146);
    if(!selected_name(a,choice,name) || !save_path(path,name,get(a,0x59811c,0)?".sav":".sfl"))return 0;
    game_memory_write_u8(&a->memory,0x629808,0);route=17;
    if(get(a,0x59811c,0)) {
        if(save_header(a,path)) {
            put(a,0x5910a8,0,1);
            if(!game_memory_write(&a->memory,0x5e1248,path,strlen(path)+1))return 0;
        }else route=18;
        (void)game_application_file_copy(a,path,"saves\\temp"); /*Original ignores CopyFileA failure. */
    }else {
        if(!game_memory_write(&a->memory,0x5e1248,path,strlen(path)+1) ||
           !game_menu_settings_string(a,get(a,0x5943c8,0)+4,(const uint8_t *)name,strlen(name)))return 0;
    }
    if(route==17 && !game_menu_selection_begin(a))return 0;
    if(!game_menu_request(a,controller,69,1000,route))return 0;
    put(a,get(a,0x59811c,0)?0x59811c:0x598120,0,0);return game_menu_sound(a,145);
}
