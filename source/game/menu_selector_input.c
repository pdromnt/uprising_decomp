#include "menu_widgets.h"
#include "menu_sound.h"
#include "menu_runtime_tables.h"
#include <math.h>
extern GameAddress game_menu_controller_construct(GameApplication *);
extern void game_menu_controller_destroy(GameApplication *,GameAddress);
extern int game_menu_controller_tick(GameApplication *,GameAddress);
extern int game_menu_register_region(GameApplication *,GameAddress,GameAddress,GameAddress,uint32_t,uint32_t,uint32_t);
extern int game_menu_register_tick(GameApplication *,GameAddress,GameAddress,uint32_t,uint32_t);
extern int game_menu_unregister(GameApplication *,GameAddress,GameAddress,int);
extern uint32_t game_menu_milliseconds(GameApplication *);
static uint32_t get(GameApplication *a,GameAddress p,unsigned o){return game_memory_read_u32(&a->memory,p+o);}
static void put(GameApplication *a,GameAddress p,unsigned o,uint32_t v){game_memory_write_u32(&a->memory,p+o,v);}
static void dirty(GameApplication *a,GameAddress p) {
    unsigned o;if(!get(a,p,0x1c))for(o=0;o<16;o+=4)put(a,p,0x30+o,get(a,p,0x20+o));put(a,p,0x1c,1);
}
static uint32_t scaled(uint32_t total,uint32_t travel,uint32_t distance) {
    long double n=(long double)(int32_t)total/(long double)(int32_t)travel*(long double)(int32_t)distance;
    if(!isfinite(n) || n<-9223372036854775808.0L || n>=9223372036854775808.0L)return 0;
    return (uint32_t)(uint64_t)(int64_t)truncl(n);
}
static int move_items(GameApplication *a,GameAddress p,uint32_t dx,uint32_t dy) {
    GameAddress item=get(a,p,0x5c);unsigned visits=0;
    while(item && ++visits<65536) {
        if(!game_menu_widget_position(a,item,dx,dy,0,1))return 0;
        item=get(a,item,0x4c);
    }
    dirty(a,p);put(a,p,0x18,1);return !item && !a->memory.fault;
}
/* 00520810: page above/below the thumb using its original pixel increment. */
static GameAddress item_at(GameApplication *a,GameAddress p,int32_t x,int32_t y) {
    GameAddress item=get(a,p,0x5c);unsigned visits=0;
    while(item && ++visits<65536) {
        if(x>=(int32_t)get(a,item,0x20) && x<(int32_t)get(a,item,0x28) &&
           y>=(int32_t)get(a,item,0x24) && y<(int32_t)get(a,item,0x2c))return item;
        item=get(a,item,0x4c);
    }
    if(item)a->memory.fault=1;
    return 0;
}
static int item_state(GameApplication *a,GameAddress p,uint32_t state) {
    if(get(a,p,0)==0x5759e0)return game_menu_toggle_state(a,p,state);
    return game_menu_widget_base_state(a,p,state);
}
/*0051f670: change the persistent selection without invoking its callback. */
int game_menu_selector_select(GameApplication *a,GameAddress p,GameAddress item) {
    GameAddress old=get(a,p,0x60);
    if(old && old!=item && get(a,old,0x44)!=3 && !item_state(a,old,0))return 0;
    if(get(a,p,0x60)==item)return !a->memory.fault;
    put(a,p,0x60,item);
    if(item && get(a,item,0x44)!=3 && !item_state(a,item,2))return 0;
    dirty(a,p);put(a,p,0x18,1);return !a->memory.fault;
}
/*00520550/005204b0: press selects the actual intrusive child. Persistent
 * selectors release it immediately on press; ordinary selectors wait for up. */
int game_menu_selector_pointer(GameApplication *a,GameAddress p,uint32_t message,int32_t x,int32_t y) {
    GameAddress item,old;
    if(message!=0x201 && message!=0x202)return 0;
    if(message==0x202 && get(a,p,0x74))return !a->memory.fault;
    item=item_at(a,p,x,y);if(!item)return !a->memory.fault;
    old=get(a,p,0x60);
    if(message==0x201) {
        if(get(a,p,0x74) && item==old)return !a->memory.fault;
        if(old && get(a,old,0x44)!=3 && !item_state(a,old,0))return 0;
        put(a,p,0x60,item);
        if(!game_menu_button_input(a,item,0x201,x,y))return 0;
        dirty(a,p);put(a,p,0x18,1);
        if(get(a,p,0x74)) {
            item=get(a,p,0x60);
            if(!game_menu_button_input(a,item,0x202,x,y))return 0;
            item=get(a,p,0x60);if(item && !item_state(a,item,2))return 0;
        }
        return !a->memory.fault;
    }
    if(item!=old)return !a->memory.fault;
    if(!game_menu_button_input(a,item,0x202,x,y))return 0;
    item=get(a,p,0x60);
    if(item==get(a,p,0x58) && !game_menu_button_input(a,item,0x20a,x,y))return 0;
    if(get(a,p,0x74) && !item_state(a,get(a,p,0x60),2))return 0;
    dirty(a,p);put(a,p,0x18,1);return !a->memory.fault;
}
/*00520700: clear only the hover identity, retaining a fixed selection. */
int game_menu_selector_hover_exit(GameApplication *a,GameAddress p,int32_t x,int32_t y) {
    GameAddress hovered=get(a,p,0x58);
    if(!hovered)return !a->memory.fault;
    dirty(a,p);
    if((!get(a,p,0x74) || hovered!=get(a,p,0x60)) && !game_menu_button_input(a,hovered,0x20b,x,y))return 0;
    put(a,p,0x58,0);return !a->memory.fault;
}
int game_menu_selector_page(GameApplication *a,GameAddress p,int32_t y) {
    uint32_t total=get(a,p,0x98),size=get(a,p,0xbc),begin=get(a,p,0x24),end=get(a,p,0x2c);
    uint32_t top=get(a,p,0xb0),bottom=get(a,p,0xb8),distance=0,delta;
    if((int32_t)total<=0)return !a->memory.fault;
    if(y>(int32_t)bottom)distance=(int32_t)(bottom+size)>(int32_t)end?end-bottom:size;
    else if(y<(int32_t)top)distance=(int32_t)(top-size)<(int32_t)begin?begin-top:0u-size;
    delta=scaled(total,end-begin-size,distance);
    if(delta && !move_items(a,p,0,0u-delta))return 0;
    return !delta || game_menu_selector_scroll_geometry(a,p);
}
/* 0051fdf0/51ff40/51ffb0: this capture owns a temporary controller and borrows
 * only its selector. Its parent remains the published authoritative controller. */
int game_menu_selector_capture_begin(GameApplication *a,GameAddress p,int32_t x,int32_t y,int horizontal) {
    GameAddress parent=get(a,0x59d4c0,0),window=get(a,0x59d4bc,0),controller;uint32_t left,top;
    if(!parent || !window || get(a,0x627ab4,0))return 0;
    if((int32_t)get(a,p,horizontal?0x9c:0x98)<(horizontal?1:0))return !a->memory.fault;
    controller=game_menu_controller_construct(a);if(!controller)return 0;
    left=get(a,window,0x58)-get(a,window,0x44);top=get(a,window,0x5c)-get(a,window,0x48);
    put(a,0x627a98,0,left);put(a,0x627a98,4,top);
    put(a,0x627a98,8,get(a,window,0x60)-get(a,window,0x44)+left);
    put(a,0x627a98,12,get(a,window,0x64)-get(a,window,0x48)+top);
    if(!game_menu_register_region(a,controller,p,0x627a98,0x202,0x520340,0) ||
       !game_menu_register_tick(a,controller,p,0x5203b0,0) ||
       !game_menu_register_region(a,controller,p,0x627a98,0x200,horizontal?0x520210:0x520020,0)) {
        game_menu_controller_destroy(a,controller);return 0;
    }
    put(a,0x627ab4,0,parent);put(a,0x59d4c0,0,controller);
    put(a,0x627a90,0,(uint32_t)x);put(a,0x627a94,0,(uint32_t)y);
    put(a,0x627aac,0,(uint32_t)x-get(a,p,horizontal?0xcc:0xac));
    put(a,0x627ab0,0,(uint32_t)y-get(a,p,horizontal?0xd0:0xb0));
    put(a,0x5980c8,0,0);put(a,0x5980cc,0,0);return !a->memory.fault;
}
int game_menu_selector_capture_end(GameApplication *a,GameAddress p) {
    GameAddress controller=get(a,0x59d4c0,0),parent=get(a,0x627ab4,0);
    if(!parent || parent==controller)return 0;
    if(get(a,p,0xe4)!=UINT32_MAX && get(a,0x5980cc,0)) {
        if(!game_menu_sound_loop_end(a,get(a,p,0xe4),get(a,0x627aa8,0)))return 0;
        put(a,0x5980cc,0,0);
    }
    if(!game_menu_unregister(a,controller,p,0))return 0;
    game_menu_controller_destroy(a,controller);put(a,0x59d4c0,0,parent);put(a,0x627ab4,0,0);return !a->memory.fault;
}
/* 00520020/00520210: both axes retain the original endpoint special case.
 * The thumb and the intrusive item chain are updated in their existing records. */
int game_menu_selector_capture_move(GameApplication *a,GameAddress p,int32_t x,int32_t y,int horizontal) {
    unsigned start=horizontal?0x20:0x24,limit=horizontal?0x28:0x2c,first=horizontal?0xcc:0xb0,last=horizontal?0xd4:0xb8;
    unsigned size_offset=horizontal?0xe0:0xbc,total_offset=horizontal?0x9c:0x98;
    uint32_t begin=get(a,p,start),end=get(a,p,limit),size=get(a,p,size_offset),old=get(a,p,first),old_end=get(a,p,last);
    uint32_t coordinate=(uint32_t)(horizontal?x:y),candidate=coordinate-get(a,horizontal?0x627aac:0x627ab0,0),wanted,delta;
    GameAddress item=get(a,p,0x5c);
    if((int32_t)candidate>=(int32_t)begin && (int32_t)(candidate+size-1)<=(int32_t)end) {
        put(a,p,first,candidate);put(a,p,last,candidate+size-1);
    } else if(old!=begin) {
        if((int32_t)candidate<(int32_t)begin){put(a,p,first,begin);put(a,p,last,old_end+begin-old);}
        else if((int32_t)(candidate+size-1)>(int32_t)end){put(a,p,first,old+end-old_end);put(a,p,last,end);}
    }
    wanted=begin+scaled(get(a,p,total_offset),end-size-begin,begin-get(a,p,first));
    if(item) {
        delta=wanted-get(a,item,start);
        if(!move_items(a,p,horizontal?delta:0,horizontal?0:delta))return 0;
    }
    if(!horizontal && get(a,p,0xe4)!=UINT32_MAX) {
        uint32_t movement=(uint32_t)y-get(a,0x627a94,0),magnitude=(int32_t)movement<0?0u-movement:movement;
        put(a,0x5980c8,0,1);put(a,0x5980d0,0,game_menu_milliseconds(a));
        if(magnitude) {
            float pitch=(float)((long double)(int32_t)(magnitude>10?10:magnitude)*game_menu_runtime_float(&a->memory,0x5758fc)+game_menu_runtime_float(&a->memory,0x575900));
            if(get(a,0x5980c8,0)) {
                if(!get(a,0x5980cc,0)) {
                    if(!game_menu_sound_drag_begin(a,get(a,p,0xe4),pitch))return 0;
                    put(a,0x5980cc,0,1);
                } else if(!game_menu_sound_drag_pitch(a,get(a,p,0xe4),pitch))return 0;
            }
        }
    }
    put(a,0x627a90,0,(uint32_t)x);put(a,0x627a94,0,(uint32_t)y);dirty(a,p);return !a->memory.fault;
}
/* 005203b0: keep the parent's event pump alive and retire a stopped gesture. */
int game_menu_selector_capture_tick(GameApplication *a,GameAddress p) {
    uint32_t now;GameAddress controller=get(a,0x59d4c0,0),parent=get(a,0x627ab4,0);
    if(!game_menu_controller_tick(a,parent))return 0;
    if(get(a,0x59d4c0,0)!=controller || get(a,0x627ab4,0)!=parent)return !a->memory.fault;
    if(get(a,p,0xe4)==UINT32_MAX)return !a->memory.fault;
    if(!get(a,0x5980c8,0) && get(a,0x5980cc,0)) {
        if(!game_menu_sound_loop_end(a,get(a,p,0xe4),get(a,0x627aa8,0)))return 0;
        put(a,0x5980cc,0,0);
    }
    now=game_menu_milliseconds(a);
    if((int32_t)(get(a,0x5980d0,0)+500)<=(int32_t)now){put(a,0x5980c8,0,0);put(a,0x5980d0,0,now);}
    return !a->memory.fault;
}
