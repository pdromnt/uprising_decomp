#include "menu_widgets.h"
#include "scene_projection_storage.h"
#include "state_helper_plans.h"
#include <math.h>
#include <string.h>
extern int game_menu_register_region(GameApplication *,GameAddress,GameAddress,GameAddress,uint32_t,uint32_t,uint32_t);
extern int game_menu_register_delivery(GameApplication *,GameAddress,GameAddress,uint32_t,uint32_t);
extern int game_menu_register_tick(GameApplication *,GameAddress,GameAddress,uint32_t,uint32_t);
extern GameAddress game_menu_controller_construct(GameApplication *);
extern void game_menu_controller_destroy(GameApplication *,GameAddress);
extern int game_menu_unregister(GameApplication *,GameAddress,GameAddress,int);

static uint32_t get(GameApplication *a,GameAddress p,unsigned o) {return game_memory_read_u32(&a->memory,p+o);}
static void put(GameApplication *a,GameAddress p,unsigned o,uint32_t v) {game_memory_write_u32(&a->memory,p+o,v);}
static void rect(GameApplication *a,GameAddress p,unsigned o,uint32_t l,uint32_t t,uint32_t r,uint32_t b) {
    put(a,p,o,l);put(a,p,o+4,t);put(a,p,o+8,r);put(a,p,o+12,b);
}
static void dirty(GameApplication *a,GameAddress p) {
    unsigned i;if(!get(a,p,0x1c))for(i=0;i<16;i+=4)put(a,p,0x30+i,get(a,p,0x20+i));
    put(a,p,0x1c,1);
}
/* 00560db0 truncates to int64, then this caller keeps the low word. The
 * integer-indefinite int64 value therefore has a zero low word. */
static uint32_t ratio(long double distance,long double travel,long double total) {
    long double v=distance*(travel/total);
    if(!isfinite(v) || v<-9223372036854775808.0L || v>=9223372036854775808.0L)return 0;
    return (uint32_t)(uint64_t)(int64_t)truncl(v);
}
/* 0051fd10: thumbs derive directly from the owned item chain and legacy bounds. */
int game_menu_selector_scroll_geometry(GameApplication *a,GameAddress p) {
    GameAddress first=get(a,p,0x5c);uint32_t dx=0,dy=0,l=get(a,p,0x20),t=get(a,p,0x24);
    uint32_t r=get(a,p,0x28),b=get(a,p,0x2c),sw=get(a,p,0xc0),sh=get(a,p,0xbc);
    uint32_t hw=get(a,p,0xe0),hh=get(a,p,0xdc),thumb;
    if(first){dx=l-get(a,first,0x20);dy=t-get(a,first,0x24);}
    thumb=t+ratio((int32_t)dy,(int32_t)(b-t-sh),(int32_t)get(a,p,0x98));
    rect(a,p,0xac,r-sw,thumb,r,thumb+sh);
    thumb=l-ratio((int32_t)dx,(int32_t)(r-hw-l),(int32_t)get(a,p,0x9c));
    rect(a,p,0xcc,thumb,b-hh,thumb+hw,b);
    return !a->memory.fault;
}
/* 0051ef30. This container owns its intrusive item chain and two images; it
 * does not allocate a parallel array or synchronize a second representation. */
static GameAddress selector_create(GameApplication *a,size_t size,uint32_t id,uint32_t flags,uint32_t callback,GameAddress name) {
    GameAddress p=scene_projection_storage_allocate(size,"portable menu selector"),image;
    uint32_t w,h;
    if(!p)return 0;
    game_memory_fill_u8(&a->memory,p,size,0,0);state_helper_construct_packet(&a->memory,p,id);
    put(a,p,0,0x575a70);put(a,p,0x50,UINT32_MAX);put(a,p,0x54,UINT32_MAX);
    put(a,p,0x68,flags&4);put(a,p,0x6c,flags&2);put(a,p,0x70,flags&1);put(a,p,0x74,flags&8);
    put(a,p,0x18,flags&4);put(a,p,0xa0,callback);put(a,p,0xe4,UINT32_MAX);
    if(flags&4) {
        image=game_menu_image_create(a,UINT32_MAX,name?name:0x5980d4);if(!image)goto fail;
        put(a,p,0xa8,image);w=get(a,image,0x28)-get(a,image,0x20);h=get(a,image,0x2c)-get(a,image,0x24);
        put(a,p,0xc0,w);put(a,p,0xbc,h);
        rect(a,p,0x78,0,0,0u-w,0);rect(a,p,0x88,1u-w,0,0,0);rect(a,p,0xac,0u-w,0,0,h);
        image=game_menu_image_create(a,UINT32_MAX,0x5980f4);if(!image)goto fail;
        put(a,p,0xc8,image);w=get(a,image,0x28)-get(a,image,0x20);h=get(a,image,0x2c)-get(a,image,0x24);
        put(a,p,0xe0,w);put(a,p,0xdc,h);rect(a,p,0xcc,0,0u-h,0,0);
    }
    if(!a->memory.fault)return p;
fail:
    (void)game_menu_selector_release(a,p);scene_projection_storage_free(p);return 0;
}
GameAddress game_menu_selector_create(GameApplication *a,uint32_t id,uint32_t flags,uint32_t callback,GameAddress name) {
    return selector_create(a,0xe8,id,flags,callback,name);
}
/* 005211f0: construct the inherited selector with no background-name argument,
 * then publish the edit class and its extra zero word in the same owner. */
GameAddress game_menu_edit_create(GameApplication *a,uint32_t id,uint32_t flags,uint32_t callback) {
    GameAddress p=selector_create(a,0xf4,id,flags,callback,0);
    if(p){put(a,p,0,0x575b08);put(a,p,0xe8,0);}return p;
}
/* 0051f460/70 defer scroll geometry until the original position/enable stage. */
int game_menu_selector_resize(GameApplication *a,GameAddress p,uint32_t width,uint32_t height) {
    put(a,p,0x28,get(a,p,0x20)+width);put(a,p,0x2c,get(a,p,0x24)+height);return !a->memory.fault;
}
static int move_items(GameApplication *a,GameAddress p,uint32_t x,uint32_t y,uint32_t z) {
    GameAddress item=get(a,p,0x5c);size_t limit=65536;
    while(item && limit--) {
        if(!game_menu_widget_position(a,item,x,y,z,1))return 0;
        item=get(a,item,0x4c);
    }
    return !item && !a->memory.fault;
}
/* 0051f530 is relative; 0051f480 is absolute. Their item updates deliberately
 * differ in depth and ordering. Arithmetic wraps explicitly in uint32_t. */
int game_menu_selector_position(GameApplication *a,GameAddress p,uint32_t x,uint32_t y,uint32_t z,int relative) {
    uint32_t width=get(a,p,0x28)-get(a,p,0x20),height=get(a,p,0x2c)-get(a,p,0x24);
    uint32_t oldx=get(a,p,0x20),oldy=get(a,p,0x24),itemx=x,itemy=y,itemz=z,sw=get(a,p,0x68)?get(a,p,0xc0):0;
    dirty(a,p);
    if(relative){x+=oldx;y+=oldy;z+=get(a,p,0x40);}
    else {itemx=x-oldx;itemy=y-oldy;}
    put(a,p,0x20,x);put(a,p,0x24,y);put(a,p,0x28,x+width);put(a,p,0x2c,y+height);put(a,p,0x40,z);
    rect(a,p,0x78,x,y,x+width-sw,y+height);
    if(get(a,p,0x68))rect(a,p,0x88,x+width-get(a,p,0xc0)+1,y,x+width,y+height);
    if(!relative) {
        if(!move_items(a,p,itemx,itemy,0))return 0;
        return game_menu_selector_scroll_geometry(a,p);
    }
    if(!game_menu_selector_scroll_geometry(a,p))return 0;
    return move_items(a,p,itemx,itemy,itemz);
}
/* 0051f700/7c0: append vertically or with the supplied horizontal spacing. */
int game_menu_selector_append(GameApplication *a,GameAddress p,GameAddress item,int horizontal,uint32_t spacing) {
    GameAddress first=get(a,p,0x5c),tail=first;uint32_t x=get(a,p,0x78),y=get(a,p,0x7c);size_t limit=65536;
    if(!item || get(a,item,0x4c))return 0;
    while(tail && get(a,tail,0x4c) && limit--)tail=get(a,tail,0x4c);
    if(!limit)return 0;
    dirty(a,p);
    if(tail) {
        if(horizontal){x=get(a,tail,0x20)+spacing;y=get(a,tail,0x24);}else y=get(a,tail,0x2c);
        put(a,tail,0x4c,item);
    } else put(a,p,0x5c,item);
    if(!game_menu_widget_position(a,item,x,y,get(a,p,0x40),0))return 0;
    first=get(a,p,0x5c);
    put(a,p,0x98,get(a,item,0x2c)-get(a,first,0x24)+get(a,p,0x24)-get(a,p,0x2c));
    put(a,p,0x9c,get(a,item,0x28)-get(a,first,0x20)+get(a,p,0x20)-get(a,p,0x28));
    return game_menu_selector_scroll_geometry(a,p);
}
/* 0051fb30/0051f220: capture the next ID before retiring each item. */
int game_menu_selector_clear(GameApplication *a,GameAddress p) {
    GameAddress item=get(a,p,0x5c);size_t limit=65536;
    while(item && limit--) {
        GameAddress next=get(a,item,0x4c);
        if(!game_menu_widget_destroy(a,item))return 0;
        put(a,p,0x5c,next);item=next;
    }
    if(item)return 0;
    put(a,p,0x98,0);put(a,p,0x9c,0);put(a,p,0x58,0);put(a,p,0x60,0);
    dirty(a,p);put(a,p,0x18,1);return !a->memory.fault;
}
int game_menu_selector_release(GameApplication *a,GameAddress p) {
    static const unsigned slots[]={0xa8,0xc8};unsigned i;
    if(get(a,0x627ab4,0)) {
        GameAddress controller=get(a,0x59d4c0,0),node=get(a,get(a,controller,8),0);
        if(node && get(a,get(a,node,8),8)==p && !game_menu_selector_capture_end(a,p))return 0;
    }
    if(!game_menu_selector_clear(a,p))return 0;
    for(i=0;i<2;++i){if(!game_menu_widget_destroy(a,get(a,p,slots[i])))return 0;put(a,p,slots[i],0);}
    return game_menu_widget_enable(a,p,0);
}
/* 0051fb80: reveal an item by translating the entire authoritative chain. */
int game_menu_selector_reveal(GameApplication *a,GameAddress p,GameAddress item) {
    uint32_t dy=0;GameAddress child;size_t limit=65536;
    if((int32_t)get(a,item,0x2c)>(int32_t)get(a,p,0x2c))dy=get(a,p,0x2c)-get(a,item,0x2c);
    else if((int32_t)get(a,item,0x24)<(int32_t)get(a,p,0x24))dy=get(a,item,0x24)-get(a,p,0x24);
    if(dy){
        put(a,p,0x18,1);child=get(a,p,0x5c);
        while(child && limit--){if(!game_menu_widget_position(a,child,0,dy,0,1))return 0;child=get(a,child,0x4c);}
        if(child)return 0;
        dirty(a,p);
    }
    return game_menu_selector_scroll_geometry(a,p);
}
/* 0051f310: selectors register their actual region/delivery consumers, rather
 * than inheriting the plain button's click/timer registration by accident. */
int game_menu_selector_enable(GameApplication *a,GameAddress p) {
    GameAddress controller=game_memory_read_u32(&a->memory,0x59d4c0);
    if(get(a,p,0x48))return !a->memory.fault;
    put(a,p,0x48,1);
    if(get(a,p,0x6c) &&
        (!game_menu_register_region(a,controller,p,p+0x78,0x201,0x5232c0,0) ||
         !game_menu_register_region(a,controller,p,p+0x78,0x202,0x5232b0,0)))return 0;
    if(get(a,p,0x70) &&
        (!game_menu_register_region(a,controller,p,p+0x78,0x200,0x520750,0) ||
         !game_menu_register_region(a,controller,p,p+0x78,0x20a,0x523290,0) ||
         !game_menu_register_region(a,controller,p,p+0x78,0x20b,0x5232a0,0)))return 0;
    if(get(a,p,0x68)) {
        rect(a,p,0x78,get(a,p,0x20),get(a,p,0x24),get(a,p,0x28)-get(a,p,0xc0)-1,get(a,p,0x2c)-1);
        if(!game_menu_selector_scroll_geometry(a,p) ||
            !game_menu_register_region(a,controller,p,p+0x88,0x201,0x520810,0) ||
            !game_menu_register_region(a,controller,p,p+0xac,0x201,0x51ffb0,0))return 0;
    }
    if(get(a,p,0xa0) && !game_menu_register_delivery(a,controller,p,0x3e9,0x520960))return 0;
    return !a->memory.fault;
}
/* 00520750: bounds use left/top inclusive and right/bottom exclusive. A fixed
 * selected item can retain its state while another item receives hover. */
int game_menu_selector_hover_move(GameApplication *a,GameAddress p,int32_t x,int32_t y) {
    GameAddress item=get(a,p,0x5c),old=get(a,p,0x58),fixed=get(a,p,0x74)?get(a,p,0x60):0;unsigned visits=0;
    while(item && ++visits<65536) {
        if(x>=(int32_t)get(a,item,0x20) && x<(int32_t)get(a,item,0x28) &&
           y>=(int32_t)get(a,item,0x24) && y<(int32_t)get(a,item,0x2c))break;
        item=get(a,item,0x4c);
    }
    if(visits>=65536)return 0;
    if(item==old || (!item && old==fixed))return !a->memory.fault;
    if(old && old!=fixed && !game_menu_button_input(a,old,0x20b,x,y))return 0;
    put(a,p,0x58,item);
    if(item && item!=fixed && !game_menu_button_input(a,item,0x20a,x,y))return 0;
    dirty(a,p);return !a->memory.fault;
}
static GameUiRect bounds(GameApplication *a,GameAddress p) {
    GameUiRect r={(int32_t)get(a,p,0x20),(int32_t)get(a,p,0x24),(int32_t)get(a,p,0x28),(int32_t)get(a,p,0x2c)};return r;
}
static int clipped(GameUiRect *r,const GameUiRect *clip) {
    if(!clip)return r->left<r->right && r->top<r->bottom;
    if(r->left<clip->left)r->left=clip->left;
    if(r->top<clip->top)r->top=clip->top;
    if(r->right>clip->right)r->right=clip->right;
    if(r->bottom>clip->bottom)r->bottom=clip->bottom;
    return r->left<r->right && r->top<r->bottom;
}
/* 00520990: items and visible thumbs draw through their native class. */
int game_menu_selector_draw(GameApplication *a,GameAddress p,const GameUiRect *clip) {
    GameAddress item=get(a,p,0x5c);GameUiRect r;size_t limit=65536;unsigned i;
    while(item && limit--) {
        r=bounds(a,item);if(clipped(&r,clip) && !game_menu_widget_draw(a,item,&r))return 0;
        item=get(a,item,0x4c);
    }
    if(item)return 0;
    if(get(a,p,0x68))for(i=0;i<2;++i) {
        unsigned child=i?0xc8:0xa8,position=i?0xcc:0xac;
        if(i && (int32_t)get(a,p,0x9c)<=0)continue;
        item=get(a,p,child);
        if(!game_menu_widget_position(a,item,get(a,p,position),get(a,p,position+4),0,0))return 0;
        r=bounds(a,item);
        if(clipped(&r,clip) && !game_menu_widget_draw(a,item,&r))return 0;
    }
    put(a,p,0x18,0);return !a->memory.fault;
}
/* 00522430: one image owns the slider thumb, and the 8c record owns its
 * orientation/value/callbacks. Values are float bits, never host pointers. */
GameAddress game_menu_slider_create(GameApplication *a,uint32_t id,GameAddress name,uint32_t width,uint32_t height,
    uint32_t callback,uint32_t timing,float value,uint32_t orientation,uint32_t begin,uint32_t end) {
    GameAddress p=scene_projection_storage_allocate(0x8c,"portable menu slider"),image;uint32_t bits;unsigned i;
    if(!p)return 0;
    game_memory_fill_u8(&a->memory,p,0x8c,0,0);state_helper_construct_packet(&a->memory,p,id);
    put(a,p,0,0x575ba8);put(a,p,0x50,UINT32_MAX);put(a,p,0x54,UINT32_MAX);
    memcpy(&bits,&value,4);put(a,p,0x64,callback);put(a,p,0x68,timing);put(a,p,0x6c,bits);
    put(a,p,0x58,orientation);put(a,p,0x5c,begin);put(a,p,0x60,end);
    image=game_menu_image_create(a,UINT32_MAX,name);if(!image){scene_projection_storage_free(p);return 0;}
    put(a,p,0x88,image);put(a,image,0x13c,254);put(a,image,0xb0,101);
    for(i=0;i<16;i+=4)put(a,p,0x78+i,get(a,image,0x20+i));
    put(a,p,0x74,get(a,image,0x28)-get(a,image,0x20));put(a,p,0x70,get(a,image,0x2c)-get(a,image,0x24));
    rect(a,p,0x20,0,0,width-1,height-1);rect(a,p,0x30,0,0,width-1,height-1);
    return !a->memory.fault?p:0;
}
/* 00522680: ordinary float arithmetic with the original truncation boundary. */
int game_menu_slider_geometry(GameApplication *a,GameAddress p) {
    uint32_t bits=get(a,p,0x6c),l=get(a,p,0x20),t=get(a,p,0x24),r=get(a,p,0x28),b=get(a,p,0x2c);
    uint32_t w=get(a,p,0x74),h=get(a,p,0x70),v;float value;memcpy(&value,&bits,4);
    if(get(a,p,0x58)==1) {
        v=l+ratio((int32_t)(r-w-l),value,1);rect(a,p,0x78,v,t,v+w-1,t+h-1);
    } else if(!get(a,p,0x58)) {
        v=t+ratio((int32_t)(b-t-h),value,1);rect(a,p,0x78,l,v,l+w-1,v+h-1);
    }
    return !a->memory.fault;
}
int game_menu_slider_draw(GameApplication *a,GameAddress p,const GameUiRect *clip) {
    GameAddress image=get(a,p,0x88);GameUiRect r;
    if(!game_menu_widget_position(a,image,get(a,p,0x78),get(a,p,0x7c),0,0))return 0;
    r=bounds(a,image);if(clipped(&r,clip) && !game_menu_widget_draw(a,image,&r))return 0;
    put(a,p,0x18,0);return !a->memory.fault;
}
/* 00522720/00522890: modal drag controller owns only its registrations.
 * The parent keeps its live timer list; 00522880 forwards those timers. */
static int slider_capture(GameApplication *a,GameAddress p,int32_t x,int32_t y) {
    GameAddress parent=get(a,0x59d4c0,0),window=get(a,0x59d4bc,0),controller;
    uint32_t l,t;
    if(!parent || !window || get(a,0x627ab4,0))return 0;
    controller=game_menu_controller_construct(a);if(!controller)return 0;
    l=get(a,window,0x58)-get(a,window,0x44);
    t=get(a,window,0x5c)-get(a,window,0x48);
    rect(a,0x627a98,0,l,t,get(a,window,0x60)-get(a,window,0x44)+l,
         get(a,window,0x64)-get(a,window,0x48)+t);
    if(!game_menu_register_region(a,controller,p,0x627a98,0x200,0x5228f0,0) ||
       !game_menu_register_region(a,controller,p,0x627a98,0x202,0x522ad0,0) ||
       !game_menu_register_tick(a,controller,p,0x522880,0)) {
        game_menu_controller_destroy(a,controller);return 0;
    }
    put(a,0x627ab4,0,parent);put(a,0x59d4c0,0,controller);
    put(a,0x627a90,0,(uint32_t)x);put(a,0x627a94,0,(uint32_t)y);
    put(a,0x5980c8,0,0);put(a,0x5980cc,0,0);
    put(a,0x627aac,0,(uint32_t)x-get(a,p,0x78));
    put(a,0x627ab0,0,(uint32_t)y-get(a,p,0x7c));
    return !get(a,p,0x5c) || game_menu_button_callback(a,get(a,p,0x5c),p,UINT32_MAX,p+0x6c);
}
static int slider_release_capture(GameApplication *a,GameAddress p) {
    GameAddress controller=get(a,0x59d4c0,0),parent=get(a,0x627ab4,0);
    uint32_t callback;
    if(!parent || controller==parent)return 0;
    if(!game_menu_unregister(a,controller,p,0))return 0;
    game_menu_controller_destroy(a,controller);
    put(a,0x59d4c0,0,parent);put(a,0x627ab4,0,0);
    callback=get(a,p,0x60);
    return !a->memory.fault && (!callback || game_menu_button_callback(a,callback,p,UINT32_MAX,p+0x6c));
}
/* A menu can close while the mouse is held. Retire the modal owner before
 * its receiver or the parent's registrations are destroyed. */
int game_menu_slider_cancel(GameApplication *a) {
    GameAddress parent=get(a,0x627ab4,0),controller,list,node,callback,receiver;
    if(!parent)return !a->memory.fault;
    controller=get(a,0x59d4c0,0);
    if(!controller || controller==parent)return 0;
    list=get(a,controller,8);node=get(a,list,0);
    if(!node)return 0;
    callback=get(a,node,8);receiver=get(a,callback,8);
    if(get(a,receiver,0)==0x575ba8u)return slider_release_capture(a,receiver);
    if(get(a,receiver,0)==0x575a70u || get(a,receiver,0)==0x575b08u || get(a,receiver,0)==0x577400u)return game_menu_selector_capture_end(a,receiver);
    return 0;
}
/* 005228f0: preserve wrapped coordinates, the original endpoint special case
 * and signed comparison of stored float bits. In particular an endpoint thumb
 * does not jump across the track when an outside drag begins there. */
int game_menu_slider_input(GameApplication *a,GameAddress p,uint32_t message,int32_t x,int32_t y) {
    uint32_t orientation=get(a,p,0x58),bits,callback;
    if(message==0x201)return slider_capture(a,p,x,y);
    if(message==0x202)return slider_release_capture(a,p);
    if(message!=0x200)return 0;
    put(a,0x5980c8,0,1);
    if(orientation<=1) {
        unsigned start=orientation?0x20:0x24,limit=orientation?0x28:0x2c;
        unsigned first=orientation?0x78:0x7c,last=orientation?0x80:0x84,size=orientation?0x74:0x70;
        uint32_t coordinate=(uint32_t)(orientation?x:y),offset=get(a,orientation?0x627aac:0x627ab0,0);
        uint32_t candidate=coordinate-offset,begin=get(a,p,start),end=get(a,p,limit);
        uint32_t extent=get(a,p,size),old=get(a,p,first),old_end=get(a,p,last);
        if((int32_t)candidate>=(int32_t)begin && (int32_t)(candidate+extent-1u)<=(int32_t)end) {
            put(a,p,first,candidate);put(a,p,last,candidate+extent-1u);
        } else if(old!=begin) {
            if((int32_t)candidate<(int32_t)begin) {
                put(a,p,first,begin);put(a,p,last,old_end+begin-old);
            } else if((int32_t)(candidate+extent-1u)>(int32_t)end) {
                put(a,p,first,old+end-old_end);put(a,p,last,end);
            }
        }
        if(get(a,p,first)==begin)bits=0;
        else {
            float value=(float)((long double)(int32_t)(get(a,p,first)-begin)/
                               (long double)(int32_t)(end-extent-begin));
            memcpy(&bits,&value,4);
        }
        put(a,p,0x6c,bits);
    }
    bits=get(a,p,0x6c);if((int32_t)bits>(int32_t)0x3f800000u)put(a,p,0x6c,0x3f800000);
    put(a,0x627a90,0,(uint32_t)x);put(a,0x627a94,0,(uint32_t)y);
    dirty(a,p);put(a,p,0x18,1);callback=get(a,p,0x64);
    return !a->memory.fault && (!callback || game_menu_button_callback(a,callback,p,get(a,p,0x68),p+0x6c));
}
