#include "menu_widgets.h"
#include "scene_projection_storage.h"
#include "state_helper_plans.h"
#include "gameplay_state.h"
#include <stdio.h>
#include <string.h>
extern int game_menu_request(GameApplication *,GameAddress,uint32_t,uint32_t,uint32_t);
static uint32_t get(GameApplication *a,GameAddress p,unsigned o){return game_memory_read_u32(&a->memory,p+o);}
static void put(GameApplication *a,GameAddress p,unsigned o,uint32_t v){game_memory_write_u32(&a->memory,p+o,v);}
/*0051db00/0051cec0, style3: the selector owns this button and its three text
 * frames. Its filename stays in the original bounded inline field. */
GameAddress game_menu_text_button_create(GameApplication *a,const char *name,uint32_t width,uint32_t height,uint32_t font,uint32_t x,uint32_t y,uint32_t callback,uint32_t format) {
    static const unsigned slots[]={0x58,0x60,0x5c};
    /* 51cf67/51d0b5/51d111: RGB argument order, not local-variable order.
     * The normal/selected text is olive green; hover is pure green. */
    static const uint32_t colors[][3]={{44,83,28},{44,83,28},{0,255,0}};
    static const uint32_t plain[][3]={{20,20,20},{0,0,255},{0,255,0}};
    static const uint32_t provider[][3]={{0,255,0},{0,0,0},{0,255,0}};
    GameAddress p;unsigned i,o;size_t length;
    if(!name || (length=strlen(name))>=0x284 || !width || !height || (format!=2 && format!=3 && format!=4 && format!=7))return 0;
    p=scene_projection_storage_allocate(0x31c,"portable level selector row");if(!p)return 0;
    game_memory_fill_u8(&a->memory,p,0x31c,0,0);
    state_helper_construct_packet(&a->memory,p,UINT32_MAX);
    gameplay_state_initialize_presentation_packet(&a->memory,p);
    put(a,p,0,0x575950);put(a,p,0x94,font);
    put(a,p,0x6c,callback);put(a,p,0x64,UINT32_MAX);
    for(o=0x7c;o<=0x84;o+=4)put(a,p,o,UINT32_MAX);
    put(a,p,0x88,0);put(a,p,0x50,UINT32_MAX);put(a,p,0x54,UINT32_MAX);
    if(!game_memory_write(&a->memory,p+0x98,name,length+1))goto fail;
    for(i=0;i<3;++i) {
        const uint32_t *color=format==2?plain[i]:format==4?provider[i]:colors[i];
        GameMenuTextStyle style={color[0],color[1],color[2],font,width,height,x,y,i==1?(format==3?3:format==4?246:format==7?47:0):0};
        GameAddress frame=game_menu_text_image_create_bytes(a,name,&style);
        if(!frame)goto fail;
        put(a,p,slots[i],frame);
    }
    for(o=0;o<16;o+=4){uint32_t v=get(a,get(a,p,0x58),0x20+o);put(a,p,0x20+o,v);put(a,p,0x30+o,v);}
    return a->memory.fault?0:p;
fail:
    for(i=0;i<3;++i){GameAddress frame=get(a,p,slots[i]);if(frame)(void)game_menu_widget_destroy(a,frame);}
    scene_projection_storage_free(p);return 0;
}
GameAddress game_menu_level_row_create(GameApplication *a,const char *name,uint32_t width,uint32_t height,uint32_t font,uint32_t x,uint32_t y,uint32_t callback) {
    return game_menu_text_button_create(a,name,width,height,font,x,y,callback,3);
}
/*523fe0: launch the selected quick-game row through the existing briefing/game
 *route. The selection borrows its text frame; only the published filename is
 *copied into the application lane consumed by native video/game start. */
int game_menu_level_launch_selected(GameApplication *a){
    GameAddress selected=get(a,0x598118,0);char name[128],filename[128];unsigned i;int length;
    if(!selected)return !a->memory.fault; /*No selection: original emits no request. */
    for(i=0;i<sizeof(name);++i){name[i]=(char)game_memory_read_u8(&a->memory,selected+i);if(!name[i])break;}
    if(i==sizeof(name) || a->memory.fault)return 0;
    length=snprintf(filename,sizeof(filename),"%s.lfl",name);
    if(length<0 || (size_t)length>=sizeof(filename))return 0;
    if(!game_memory_read_u8(&a->memory,0x5efec0) && !game_memory_write(&a->memory,0x5efec0,"PLAYER",7))return 0;
    if(!game_memory_write(&a->memory,0x629808,filename,(size_t)length+1))return 0;
    return game_menu_request(a,get(a,0x59d4c0,0),69,1000,17) && !a->memory.fault;
}
