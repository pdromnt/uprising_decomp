#include "menu_widgets.h"

extern int game_menu_register_tick(GameApplication *,GameAddress,GameAddress,uint32_t,uint32_t);

static uint32_t get(GameApplication *a,GameAddress p,unsigned o) {return game_memory_read_u32(&a->memory,p+o);}
static void put(GameApplication *a,GameAddress p,unsigned o,uint32_t v) {game_memory_write_u32(&a->memory,p+o,v);}
static void dirty(GameApplication *a,GameAddress p) {
    unsigned o;if(!get(a,p,0x1c))for(o=0;o<16;o+=4)put(a,p,0x30+o,get(a,p,0x20+o));
    put(a,p,0x1c,1);
}
static uint32_t magnitude(uint32_t value) {return (int32_t)value<0 ? 0u-value:value;}
static int move_items(GameApplication *a,GameAddress p,uint32_t distance,int vertical) {
    GameAddress item=get(a,p,0x5c);unsigned visits=0;
    put(a,p,0x18,1);
    while(item && !a->memory.fault) {
        if(++visits>65536u || !game_menu_widget_position(a,item,vertical?0:distance,vertical?distance:0,0,1))return 0;
        item=get(a,item,0x4c);
    }
    dirty(a,p);put(a,p,0xf0,get(a,p,0xf0)+distance);return !a->memory.fault;
}
static int renew_input(GameApplication *a,GameAddress p) {
    return game_menu_widget_enable(a,p,0) && game_menu_widget_enable(a,p,1);
}
/* 00521320/00521540: finish a pending scroll before selecting the next item.
 * The existing intrusive chain remains authoritative throughout animation. */
int game_menu_edit_scroll_request(GameApplication *a,GameAddress p,uint32_t argument,int vertical,uint32_t *result) {
    GameAddress first,item,last;unsigned visits=0,start=vertical?0x24:0x20,end=vertical?0x2c:0x28;
    uint32_t distance=0;
    *result=1;
    if(get(a,p,0xe8)) {
        uint32_t target=get(a,p,0xec),total=magnitude(target),done=magnitude(get(a,p,0xf0));
        int32_t sign;
        if(!total)return 0;
        sign=(int32_t)((int64_t)(int32_t)target/(int32_t)total);
        if(!move_items(a,p,(total-done)*(uint32_t)sign,vertical))return 0;
        put(a,p,0xe8,0);if(!renew_input(a,p))return 0;
    }
    first=get(a,p,0x5c);
    if(!first){*result=0;return !a->memory.fault;}
    put(a,p,0xec,0);
    if((int32_t)argument<0) {
        last=first;
        while(get(a,last,0x4c)) {
            if(++visits>65536u)return 0;
            last=get(a,last,0x4c);
        }
        if((int32_t)get(a,last,end)>(int32_t)get(a,p,end)) {
            item=first;
            while(item && (int32_t)get(a,item,end)<=(int32_t)get(a,p,start)) {
                if(++visits>65536u)return 0;
                item=get(a,item,0x4c);
            }
            /* The original expects a successor here. Reject a malformed chain
             * instead of reading address zero as an accidental record. */
            if(!item || !(item=get(a,item,0x4c)))return 0;
            distance=get(a,p,start)-get(a,item,start);
        }
    } else if((int32_t)argument>0 && (int32_t)get(a,first,start)<(int32_t)get(a,p,start)) {
        /* Retail walks to the containing item but uses the first item's width
         * for this branch. Preserve that behavior rather than normalize it. */
        distance=(get(a,first,end)-get(a,first,start))*argument;
    }
    put(a,p,0xec,distance);
    if(distance) {
        put(a,p,0xf0,0);put(a,p,0xe8,1);
        if(!game_menu_register_tick(a,get(a,0x59d4c0,0),p,vertical?0x523280:0x5232e0,0))return 0;
        if(!vertical && get(a,p,0xe4)!=UINT32_MAX && !game_menu_sound(a,get(a,p,0xe4)))return 0;
    }
    return !a->memory.fault;
}
/* 005214a0/00521690: quarter-distance easing with the actual axis-specific
 * minimum step. Vertical steps additionally clamp the final overshoot. */
int game_menu_edit_scroll_tick(GameApplication *a,GameAddress p,int vertical) {
    uint32_t target=get(a,p,0xec),total=magnitude(target),done=magnitude(get(a,p,0xf0)),amount;
    int32_t sign;
    if((int32_t)done>=(int32_t)total) {
        put(a,p,0xe8,0);return renew_input(a,p);
    }
    if(!total)return 0;
    amount=(total-done)/4u;
    if((int32_t)amount<=(vertical?2:1))amount=vertical?2u:1u;
    sign=(int32_t)((int64_t)(int32_t)target/(int32_t)total);
    amount*=(uint32_t)sign;
    if(vertical && (int32_t)magnitude(get(a,p,0xf0)+amount)>(int32_t)total)
        amount=(total-done)*(uint32_t)sign;
    return move_items(a,p,amount,vertical);
}
