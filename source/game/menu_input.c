#include "menu_widgets.h"

extern int game_menu_keyboard_native(GameApplication *,uint32_t method,GameAddress receiver,
    uint32_t key,uint32_t flags,uint32_t argument);
extern int game_menu_value_hover(GameApplication *,GameAddress,int);

static uint32_t get(GameMemory *m,GameAddress p,unsigned offset) {
    return game_memory_read_u32(m,p+offset);
}
static int inside(GameMemory *m,GameAddress rectangle,int32_t x,int32_t y) {
    /* Win32 PtInRect includes left/top and excludes right/bottom. This is
     * intentionally different from the button's strict release hit test. */
    return x>=(int32_t)get(m,rectangle,0) && x<(int32_t)get(m,rectangle,8) &&
        y>=(int32_t)get(m,rectangle,4) && y<(int32_t)get(m,rectangle,12);
}
static int invoke(GameApplication *a,GameAddress callback,int32_t x,int32_t y,uint32_t buttons) {
    GameMemory *m=&a->memory;
    GameAddress receiver=get(m,callback,8);
    uint32_t method=get(m,callback,12),message;
    if(get(m,receiver,0)==0x575b60u) {
        if(method==0x5232c0u)return game_menu_button_input(a,receiver,0x201,x,y);
        if(method==0x5232b0u)return game_menu_button_input(a,receiver,0x202,x,y);
        if(method==0x523290u)return game_menu_button_input(a,receiver,0x20a,x,y);
        if(method==0x5232a0u)return game_menu_button_input(a,receiver,0x20b,x,y);
    }
    if(get(m,receiver,0)==0x5731e8u)
        return game_menu_research_preview_pointer(a,receiver,method,x,y);
    if(get(m,receiver,0)==0x5731c8u && method==0x44c660u){
        extern int game_campaign_preview_pick(GameApplication *,GameAddress,int32_t,int32_t);
        return game_campaign_preview_pick(a,receiver,x,y);
    }
    if(get(m,receiver,0)==0x5758a0) {
        if(method==0x5232d0)return !m->fault; /* Actual press callback is empty. */
        if(method==0x51c0f0) {
            if(get(m,receiver,0x20)!=UINT32_MAX) {
                extern int game_menu_request(GameApplication *,GameAddress,uint32_t,uint32_t,uint32_t);
                return game_menu_request(a,game_memory_read_u32(m,0x59d4c0),get(m,receiver,0x1c),get(m,receiver,0x20),get(m,receiver,0x24));
            }
            return !get(m,receiver,0x28) || game_menu_button_callback(a,get(m,receiver,0x28),receiver,0x202,UINT32_MAX);
        }
    }
    if(get(m,receiver,0)==0x575a70 || get(m,receiver,0)==0x575b08 || get(m,receiver,0)==0x577400) {
        if(method==0x5232b0 || method==0x5232c0)
            return game_menu_selector_pointer(a,receiver,method==0x5232b0?0x202:0x201,x,y);
        if(method==0x520810)return game_menu_selector_page(a,receiver,y);
        if(method==0x51ffb0 || method==0x51ff40)return game_menu_selector_capture_begin(a,receiver,x,y,method==0x51ff40);
        if(method==0x520020 || method==0x520210)return game_menu_selector_capture_move(a,receiver,x,y,method==0x520210);
        if(method==0x520340)return game_menu_selector_capture_end(a,receiver);
        if(method==0x520750)return game_menu_selector_hover_move(a,receiver,x,y);
        if(method==0x523290 || method==0x5232a0)return game_menu_button_input(a,receiver,method==0x523290?0x20a:0x20b,x,y);
    }
    if(get(m,receiver,0)==0x577450u || get(m,receiver,0)==0x5774e8u) {
        if(method==0x523290u)return game_menu_value_hover(a,receiver,1);
        if(method==0x5232a0u)return game_menu_value_hover(a,receiver,0);
    }
    if(get(m,receiver,0)==0x575ba8u) {
        switch(method) {
        case 0x522890u:message=0x201u;break;
        case 0x5228f0u:message=0x200u;break;
        case 0x522ad0u:message=0x202u;break;
        default:goto other;
        }
        return game_menu_slider_input(a,receiver,message,x,y);
    }
    if(get(m,receiver,0)==0x575950u || get(m,receiver,0)==0x575908u || get(m,receiver,0)==0x575ac0u || get(m,receiver,0)==0x5759e0u || get(m,receiver,0)==0x575998u || get(m,receiver,0)==0x5774a0u || get(m,receiver,0)==0x577580u) {
        switch(method) {
        case 0x523290u:message=0x20au;break;
        case 0x5232a0u:message=0x20bu;break;
        case 0x5232b0u:message=0x202u;break;
        case 0x5232c0u:message=0x201u;break;
        default:goto other;
        }
        return game_menu_button_input(a,receiver,message,x,y);
    }
other:
    /*The real4cd630 callers register the native widget, selector, slider and
     *campaign/research capture methods above. Unknown records cannot re-enter
     *a general CPU callback dispatcher. */
    (void)buttons;m->fault=1;return 0;
}
static int focus_reset(GameApplication *a) {
    GameMemory *m=&a->memory;
    GameAddress root=get(m,0x5980c4u,0),link,receiver;
    if(!root)return 0;
    link=get(m,root,8);
    if(!link)return !m->fault;
    receiver=get(m,link,0);
    if(!game_menu_button_input(a,receiver,0x20bu,0,0))return 0;
    game_memory_write_u32(m,root+8u,0);return !m->fault;
}
/* 004ce030: hover exits, first hover entry, then the matching input region.
 * Each pass re-reads the list after callbacks; next is captured before invoking
 * a callback, preserving the original mutation order. */
int game_menu_controller_pointer(GameApplication *a,GameAddress controller,
    uint32_t message,int32_t x,int32_t y,uint32_t buttons) {
    GameMemory *m=&a->memory;
    unsigned pass,visits=0;
    if(!focus_reset(a))return 0;
    for(pass=0;pass<3u;++pass) {
        GameAddress list=get(m,controller,8),node=get(m,list,0);
        while(node && !m->fault) {
            GameAddress callback=get(m,node,8),next=get(m,node,4),locked=get(m,controller,32);
            uint32_t seen,key;
            int hit;
            if(++visits>65536u)return 0;
            node=next;
            if(locked && get(m,callback,8)!=locked)continue;
            seen=get(m,callback,20);key=get(m,callback,4);
            if(pass==0u && (!seen || key!=0x20bu))continue;
            if(pass==1u && (seen || key!=0x20au))continue;
            /* A modal thumb drag captures motion and release even outside its
             * original hit box. SDL coordinates are client/game coordinates;
             * the old desktop-window rectangle cannot bound this capture. */
            hit=(get(m,0x627ab4u,0) && get(m,0x59d4c0u,0)==controller &&
                 (message==0x200u || message==0x202u)) ||
                inside(m,get(m,callback,0),x,y);
            if(m->fault)return 0;
            if(pass==0u) {
                if(hit)continue;
                game_memory_write_u32(m,callback+20u,0);
                if(!invoke(a,callback,x,y,buttons))return 0;
                break;
            }
            if(pass==1u) {
                if(!hit)continue;
                game_memory_write_u32(m,callback+20u,1);
                if(!invoke(a,callback,x,y,buttons))return 0;
                break;
            }
            game_memory_write_u32(m,callback+20u,!!hit);
            if(hit && key==message)return invoke(a,callback,x,y,buttons);
        }
    }
    return !m->fault;
}

/* 004ce280: active receiver, fallback callback and shared keyboard route. */
int game_menu_controller_key(GameApplication *a,GameAddress controller,uint32_t key,uint32_t flags) {
    GameMemory *m=&a->memory;
    GameAddress receiver=get(m,controller,16);
    uint32_t direct=get(m,controller,20),fallback=get(m,controller,24),argument=get(m,controller,28);
    game_memory_write_u32(m,controller,0);
    if(receiver && direct)return game_menu_keyboard_native(a,direct,receiver,key,flags,argument);
    if(fallback) {
        /* Fallback functions receive the key and flags plus the stored value. */
        if(!game_menu_keyboard_native(a,fallback,0,key,flags,argument))return 0;
    } else game_memory_write_u32(m,controller,1);
    if(get(m,controller,0))
        return game_menu_keyboard_native(a,0x52c2e0u,0,key,flags,argument);
    return !m->fault;
}
/*004cd970: cancel the old capture through its ordinary native callback before
 * installing a shared keyboard handler in the existing controller. */
int game_menu_controller_keyboard_handler(GameApplication *a,GameAddress controller,uint32_t method,uint32_t argument) {
    if(!controller || !game_menu_controller_key(a,controller,0,UINT32_MAX))return 0;
    game_memory_write_u32(&a->memory,controller+24,method);
    game_memory_write_u32(&a->memory,controller+28,argument);
    game_memory_write_u32(&a->memory,controller+16,0);
    game_memory_write_u32(&a->memory,controller+20,0);return !a->memory.fault;
}
/*4cdb30: remove the matching fallback without sending another key callback. */
int game_menu_unregister_keyboard_method(GameApplication *a,GameAddress controller,uint32_t method) {
    if(get(&a->memory,controller,24)==method)game_memory_write_u32(&a->memory,controller+24,0);
    return !a->memory.fault;
}
