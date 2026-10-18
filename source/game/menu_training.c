#include "menu_widgets.h"
#include <stdio.h>
#include <string.h>
extern int game_menu_selection_begin(GameApplication *);
extern int game_menu_selection_end(GameApplication *);
extern int game_menu_request(GameApplication *,GameAddress,uint32_t,uint32_t,uint32_t);
static uint32_t get(GameApplication *a,GameAddress p,unsigned o){return game_memory_read_u32(&a->memory,p+o);}
static void put(GameApplication *a,GameAddress p,unsigned o,uint32_t v){game_memory_write_u32(&a->memory,p+o,v);}
/*00497750: four ordinary buttons and their background belong to this page;
 * the shared corner control and active selection manager belong to the event.
 * Training retains that manager across the video and game-start routes. */
int game_menu_training_state(GameApplication *a,GameAddress event,uint32_t phase) {
    static const GameAddress names[4][4]={{0x58cb8c,0x58cb64,0x58cb40,0},
        {0x58cbfc,0x58cbd4,0x58cbb0,0},{0x58cc6c,0x58cc44,0x58cc20,0},
        {0x58ccd8,0x58ccb4,0x58cc90,0}};
    GameAddress p;unsigned i;int ok=1;
    if(!phase) {
        p=game_menu_image_create(a,UINT32_MAX,0x58cb1c);if(!p)goto fail;
        put(a,0x58b094,0,p);
        if(!game_menu_widget_position(a,p,0,0,0,0) || !game_menu_widget_visible(a,p,1))goto fail;
        for(i=0;i<4;++i) {
            p=game_menu_button_create(a,0xa0,names[i],i==3?69:UINT32_MAX,i==3?1000:UINT32_MAX,i==3?3:i+1,i==3?0:0x52c140);
            if(!p)goto fail;
            put(a,0x58b098+4*i,0,p);
            if(!game_menu_widget_position(a,p,129,137+64*i,10,0) || !game_menu_widget_visible(a,p,1) || !game_menu_widget_enable(a,p,1))goto fail;
            put(a,p,0x50,144);put(a,p,0x54,i==3?146:145);
        }
        p=get(a,event,0x60);
        if(!game_menu_widget_position(a,p,544,383,12,0) || !game_menu_widget_visible(a,p,1) || !game_menu_selection_begin(a))goto fail;
        put(a,0x602848,0,0);put(a,0x602844,0,0);return !a->memory.fault;
    }
    if(phase!=1)return !a->memory.fault;
    if(get(a,event,0x14)!=17 && get(a,event,0x14)!=27 && !game_menu_selection_end(a))ok=0;
    if(!game_menu_widget_visible(a,get(a,event,0x60),0))ok=0;
    for(i=0;i<5;++i) {
        GameAddress slot=0x58b094+4*i;p=get(a,slot,0);
        if(p && !game_menu_widget_destroy(a,p))ok=0;else put(a,slot,0,0);
    }
    return ok && !a->memory.fault;
fail:(void)game_menu_training_state(a,event,1);return 0;
}
/*0052c140: the button's actual argument chooses the level and video route.
 * Route27 must play/skip its real media before requesting17; it is not bypassed. */
int game_menu_training_choose(GameApplication *a,uint32_t number) {
    char name[60];int size=number?snprintf(name,sizeof(name),"Training%01d.lfl",(int32_t)number):snprintf(name,sizeof(name),"sahara.lfl");
    if(size<0 || (size_t)size>=sizeof(name) || !game_memory_write(&a->memory,0x629808,name,(size_t)size+1))return 0;
    return game_menu_request(a,get(a,0x59d4c0,0),69,1000,number?27:17);
}
