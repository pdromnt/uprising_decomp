#include "menu_widgets.h"
#include "game_initializers.h"
#include <stdio.h>
/* Adjacent catalog/feedback/application algorithms remain real link blockers.
 * These ports take ordinary values and owned record IDs, never CPU state. */
extern int game_menu_quick_catalog(GameApplication *,GameAddress,uint32_t); /*00523870. */
extern int game_menu_quick_feedback(GameApplication *); /*0048fd70. */
extern int game_menu_quick_apply(GameApplication *,GameAddress); /*004a7be0. */
extern int game_menu_parameters_reset(GameApplication *); /*004a70c0, shared with lobby startup. */
static uint32_t get(GameApplication *a,GameAddress p,unsigned o){return game_memory_read_u32(&a->memory,p+o);}
static void put(GameApplication *a,GameAddress p,unsigned o,uint32_t v){game_memory_write_u32(&a->memory,p+o,v);}
static int place(GameApplication *a,GameAddress p,uint32_t x,uint32_t y,uint32_t z,int enable) {
    return p && game_menu_widget_position(a,p,x,y,z,0) && game_menu_widget_visible(a,p,1) && (!enable || game_menu_widget_enable(a,p,1));
}
static int same_name(GameApplication *a,GameAddress first,GameAddress second) {
    unsigned i;for(i=0;i<260;++i) {
        uint8_t x=game_memory_read_u8(&a->memory,first+i),y=game_memory_read_u8(&a->memory,second+i);
        if(x!=y)return 0;
        if(!x)return !a->memory.fault;
    }
    return 0;
}
/*004a70c0/00471dd0/00471e20: reset existing parameter tables and borrow the
 * first already-owned manager. Ending selection resets it, never frees it. */
int game_menu_selection_begin(GameApplication *a) {
    GameMemory *m=&a->memory;GameAddress p=get(a,0x5f0210,0);
    if(!p || !game_menu_parameters_reset(a))return 0;
    put(a,0x5eff04,0,0);game_memory_write_u8(m,0x5efe80,0);
    put(a,0x5f0208,0,p);put(a,0x5f0038,0,UINT32_MAX);
    game_initializer_0047afd0(m,p,UINT32_MAX,0x5efec0,0);
    put(a,0x5f0150,0,1);put(a,0x624f8c,0,7);put(a,0x5eff28,0,p);put(a,0x602bb0,0,0);
    return !m->fault;
}
int game_menu_selection_end(GameApplication *a) {
    GameAddress p=get(a,0x5f0208,0);
    game_memory_write_u8(&a->memory,0x5efe80,0);
    if(!p)return 0;
    put(a,p,0x3c0,UINT32_MAX);game_memory_write_u8(&a->memory,p+0x31c,0);
    put(a,p,0x468,0);put(a,p,0x4c4,0);put(a,0x5f0208,0,0);return !a->memory.fault;
}
static int number_label(GameApplication *a,GameAddress p,GameAddress value) {
    char text[64];snprintf(text,sizeof(text),"%d",(int32_t)get(a,value,0));
    return game_menu_text_image_set_bytes(a,p,text,0);
}
/*004929e0 state9: quick-game selection reuses its initial controls and owns
 * only the background. Its selector retains the existing child catalog chain. */
int game_menu_quick_state(GameApplication *a,GameAddress e,uint32_t phase) {
    static const unsigned slots[]={0x50,0x58,0x54,0x5c,0x64,0x68,0x6c};
    static const uint32_t x[]={244,244,244,94,86,86,86},y[]={247,301,355,202,117,142,167};
    GameAddress p,selector,row,saved;unsigned i,visits=0;uint32_t group,next;
    if(!phase) {
        if(get(a,e,0x10)!=11 && !game_menu_selection_begin(a))goto fail;
        put(a,0x58fd3c,0,1);
        p=game_menu_image_create(a,UINT32_MAX,0x58d270);if(!p)goto fail;put(a,0x58b0e4,0,p);
        if(!place(a,p,0,0,0,0))goto fail;
        if(!game_menu_widget_base_state(a,get(a,e,0x58),0) || !game_menu_widget_base_state(a,get(a,e,0x54),0))goto fail;
        for(i=0;i<3;++i) {
            p=get(a,e,0x64+i*4);
            if(get(a,p,0x44)==2){if(!game_menu_widget_base_state(a,p,0))goto fail;put(a,p,0x94,0);}
            if(!get(a,p,0x1c)){unsigned o;for(o=0;o<16;o+=4)put(a,p,0x30+o,get(a,p,0x20+o));}
            put(a,p,0x1c,1);
        }
        for(i=0;i<7;++i)if(!place(a,get(a,e,slots[i]),x[i],y[i],10,1))goto fail;
        group=get(a,0x58abe0,0);p=group==4?get(a,e,0x64):group==6?get(a,e,0x6c):group==5?get(a,e,0x68):0;
        if(p && get(a,p,0x44)==0){if(!game_menu_toggle_state(a,p,2))goto fail;put(a,p,0x94,1);}
        selector=get(a,e,0x5c);
        if(get(a,e,0x10)!=11) {
            int32_t px,py;
            if(!game_menu_quick_catalog(a,selector,group))goto fail;
            px=(int32_t)(get(a,selector,0x20)+5);py=(int32_t)(get(a,selector,0x24)+5);
            row=get(a,selector,0x5c);saved=get(a,get(a,0x5943c8,0),4);
            if(row && saved) {
                while(row && ++visits<65536 && !same_name(a,row+0x98,saved))row=get(a,row,0x4c);
                if(visits>=65536)goto fail;
                if(row){if(!game_menu_selector_reveal(a,selector,row))goto fail;px=(int32_t)(get(a,row,0x20)+5);py=(int32_t)(get(a,row,0x24)+5);}
            }
            if(!game_menu_selector_pointer(a,selector,0x201,px,py) || !game_menu_selector_pointer(a,selector,0x202,px,py))goto fail;
        }
        p=get(a,0x58abe8,0);if(p && !game_menu_text_image_set(a,p,0x5ac9e0,0))goto fail;
        if(!place(a,p,247,83,10,0))goto fail;
        p=get(a,0x58abf4,0);if(p && !number_label(a,p,0x5ac910))goto fail;
        p=get(a,0x58abf8,0);if(p && !number_label(a,p,0x5acbec))goto fail;
        if(!place(a,get(a,0x58abec,0),399,213,10,0) || !game_menu_quick_feedback(a) ||
           !place(a,get(a,0x58abf0,0),246,213,10,0) || !place(a,get(a,e,0x60),544,383,12,0))goto fail;
        game_memory_write_u8(&a->memory,0x5e1248,0);return !a->memory.fault;
    }
    if(phase!=1)return !a->memory.fault;
    next=get(a,e,0x14);
    if(next!=17 && next!=11){if(!game_menu_selection_end(a))return 0;put(a,0x58ac08,0,0);}
    if(!get(a,0x602bb0,0) && next==17 && !game_menu_quick_apply(a,0x5acbec))return 0;
    p=get(a,0x58b0e4,0);if(p && !game_menu_widget_destroy(a,p))return 0;put(a,0x58b0e4,0,0);
    for(i=0;i<7;++i){p=get(a,e,slots[i]);if(!game_menu_widget_visible(a,p,0) || !game_menu_widget_enable(a,p,0))return 0;}
    if(!game_menu_widget_visible(a,get(a,0x58abe8,0),0) || !game_menu_widget_visible(a,get(a,0x58abec,0),0) ||
       !game_menu_widget_visible(a,get(a,0x58abf0,0),0) || !game_menu_widget_visible(a,get(a,e,0x60),0))return 0;
    return !a->memory.fault;
fail:(void)game_menu_quick_state(a,e,1);return 0;
}
