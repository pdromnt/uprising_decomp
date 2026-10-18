#include "menu_level_catalog.h"
#include "menu_widgets.h"
#include "menu_sound.h"
#include "menu_settings.h"
#include "level_settings.h"
#include <stdio.h>
#include <string.h>
extern int game_level_selection_load(GameApplication *);
extern int game_level_selection_presets(GameApplication *);
extern int game_menu_quick_catalog(GameApplication *,GameAddress,uint32_t);
extern int game_menu_quick_feedback(GameApplication *);
extern int game_menu_request(GameApplication *,GameAddress,uint32_t,uint32_t,uint32_t);
static uint32_t get(GameApplication *a,GameAddress p,unsigned o){return game_memory_read_u32(&a->memory,p+o);}
static void put(GameApplication *a,GameAddress p,unsigned o,uint32_t v){game_memory_write_u32(&a->memory,p+o,v);}
static int text_read(GameApplication *a,GameAddress p,char *text,size_t capacity) {
    size_t i;for(i=0;i<capacity;++i){text[i]=(char)game_memory_read_u8(&a->memory,p+(uint32_t)i);if(!text[i])return !a->memory.fault;}return 0;
}
static int feedback(GameApplication *a) {
    unsigned i;char text[64];static const GameAddress labels[]={0x58abf4,0x58abf8},values[]={0x5ac910,0x5acbec};
    for(i=0;i<2;++i) {
        GameAddress label=get(a,labels[i],0);if(!label)continue;
        snprintf(text,sizeof(text),"%d",(int32_t)get(a,values[i],0));
        if(!game_menu_text_image_set_bytes(a,label,text,0))return 0;
    }
    if(!game_menu_quick_feedback(a))return 0;
    return !get(a,0x58abe8,0) || game_menu_text_image_set(a,get(a,0x58abe8,0),0x5ac9e0,0);
}
/*00523df0: selection borrows its child's owned text, updates the existing
 * settings string, then runs actual level parsing/presets/label consumers.
 * An unchanged choice is successful input; the original callback's zero result
 * is ignored by these selector callers. */
static int select_level(GameApplication *a,GameAddress row) {
    GameAddress frame=get(a,row,0x58),name=frame?get(a,frame,0x16c):0;
    char selected[128],previous[60],filename[60];int size;
    put(a,0x598118,0,name);
    if(!game_menu_request(a,get(a,0x59d4c0,0),0x20a,1009,row))return 0;
    if(!name)return !a->memory.fault;
    if(!text_read(a,name,selected,sizeof(selected)) || !text_read(a,0x5efe80,previous,sizeof(previous)))return 0;
    if(strstr(previous,selected))return !a->memory.fault;
    size=snprintf(filename,sizeof(filename),"%s.lfl",selected);if(size<0 || size>=(int)sizeof(filename))return 0;
    if(!game_memory_write(&a->memory,0x5efe80,filename,(size_t)size+1) ||
       !game_menu_settings_string(a,get(a,0x5943c8,0)+4,(const uint8_t *)selected,strlen(selected)))return 0;
    put(a,0x5eff08,0,1);put(a,0x5eff00,0,1);
    if(!game_level_selection_load(a))return 0;
    put(a,0x602bb0,0,0);return game_level_selection_presets(a) && feedback(a);
}
/*00523ee0: rebuild the same selector after a category radio click, reveal the
 * saved filename when present, then drive real press/release selection. */
int game_menu_level_action(GameApplication *a,uint32_t method,GameAddress receiver) {
    GameAddress event,selector,row,saved;char selected[128],remembered[128];unsigned visits=0;uint32_t x,y,group;
    if(method==0x523df0)return select_level(a,receiver);
    if(method!=0x523ee0)return 0;
    event=get(a,0x59d4c4,0);selector=get(a,event,0x5c);group=get(a,receiver,4)-0x31cu;
    put(a,0x58abe0,0,group);if(!game_menu_quick_catalog(a,selector,group))return 0;
    x=get(a,selector,0x20)+5;y=get(a,selector,0x24)+5;row=get(a,selector,0x5c);saved=get(a,get(a,0x5943c8,0),4);
    if(row && saved) {
        if(!text_read(a,saved,remembered,sizeof(remembered)))return 0;
        while(row && ++visits<65536) {
            if(!text_read(a,row+0x98,selected,sizeof(selected)))return 0;
            if(!strcmp(selected,remembered))break;
            row=get(a,row,0x4c);
        }
        if(row){if(!game_menu_selector_reveal(a,selector,row))return 0;x=get(a,row,0x20)+5;y=get(a,row,0x24)+5;}
    }
    if(visits==65536)return 0;
    return game_menu_selector_pointer(a,selector,0x201,(int32_t)x,(int32_t)y) &&
        game_menu_selector_pointer(a,selector,0x202,(int32_t)x,(int32_t)y);
}
