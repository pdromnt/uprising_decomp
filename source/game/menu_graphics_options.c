#include "menu_widgets.h"
#include "menu_settings.h"
#include "menu_sound.h"

static uint32_t get(GameApplication *a,GameAddress p,unsigned o) {return game_memory_read_u32(&a->memory,p+o);}
static void put(GameApplication *a,GameAddress p,unsigned o,uint32_t v) {game_memory_write_u32(&a->memory,p+o,v);}

/* State 4 connects the main menu to the graphics, audio and input pages. */
int game_menu_options_state(GameApplication *a,GameAddress e,uint32_t phase) {
    static const GameAddress names[4][4]={{0x58cd5c,0x58cd38,0x58cd18,0},
        {0x58cdc0,0x58cd9c,0x58cd7c,0},{0x58ce1c,0x58cdfc,0x58cde0,0},
        {0x58ce7c,0x58ce5c,0x58ce3c,0}};
    static const unsigned commands[]={6,5,7,2};GameAddress p;unsigned i;
    if(!phase) {
        p=game_menu_image_create(a,UINT32_MAX,0x58ccfc);if(!p)goto fail;
        put(a,0x58b0b8,0,p);
        if(!game_menu_widget_position(a,p,0,0,0,0) || !game_menu_widget_visible(a,p,1))goto fail;
        for(i=0;i<4;++i) {
            p=game_menu_button_create(a,200+i,names[i],69,1000,commands[i],0);if(!p)goto fail;
            put(a,0x58b0a8+i*4,0,p);put(a,p,0x50,144);put(a,p,0x54,145);
            if(!game_menu_widget_position(a,p,129,137+i*64,10,0) || !game_menu_widget_visible(a,p,1) || !game_menu_widget_enable(a,p,1))goto fail;
        }
        return game_menu_widget_position(a,get(a,e,0x60),544,383,12,0) && game_menu_widget_visible(a,get(a,e,0x60),1);
    }
    if(phase!=1)return !a->memory.fault;
    if(!game_menu_widget_visible(a,get(a,e,0x60),0))return 0;
    for(i=0;i<5;++i) {
        GameAddress slot=i?0x58b0a4+i*4:0x58b0b8;p=get(a,slot,0);
        if(p && !game_menu_widget_destroy(a,p))return 0;
        put(a,slot,0,0);
    }
    return !a->memory.fault;
fail:(void)game_menu_options_state(a,e,1);return 0;
}

/* 004924d0: one panel owns the selector and its navigation buttons. */
static int layout(GameApplication *a,GameAddress e,uint32_t x,uint32_t y,uint32_t z) {
    static const unsigned slots[]={0xa0,0xa4,0xa8,0xac,0xb0};
    static const unsigned dx[]={32,15,386,424,424},dy[]={62,62,62,68,109};
    unsigned i;
    if(!game_menu_widget_position(a,get(a,e,0x9c),x,y,z,0))return 0;
    x=get(a,get(a,e,0x9c),0x20);y=get(a,get(a,e,0x9c),0x24);
    for(i=0;i<5;++i)if(!game_menu_widget_position(a,get(a,e,slots[i]),x+dx[i],y+dy[i],z+1,0))return 0;
    return !a->memory.fault;
}
/* 00528110: values remain in the authoritative settings allocation. */
static int values_build(GameApplication *a,GameAddress selector) {
    static const unsigned offsets[]={0x10,0x14,0x1c,0x24,0x28,0x2c,0x34};
    static const GameAddress labels[]={0x599e18,0x599e28,0x599e3c,0x599e50,0x599e58,0x599e64,0x599e70};
    static const uint32_t steps[]={1,5,1,1,1,1,10},minimum[]={20,70,0,0,0,0,40},maximum[]={89,200,1,1,2,1,100};
    GameAddress settings=get(a,0x5943c8,0),p=0;unsigned i;uint32_t horizon=get(a,settings,0x14);int scaled=0;
    if(!settings || !game_menu_selector_clear(a,selector))return 0;
    for(i=0;i<7;++i) {
        if(i==1){put(a,settings,0x14,(uint32_t)((int32_t)horizon/100));scaled=1;}
        p=game_menu_value_create(a,420+i,settings+offsets[i],labels[i],steps[i],minimum[i],maximum[i],0,i==2 || i==3 || i==5);
        if(!p)goto fail;
        put(a,p,0x110,get(a,0x628798+i*12,0));put(a,p,0x114,get(a,0x628798+i*12,4));
        if(!game_menu_widget_enable(a,p,1))goto fail;
        put(a,p,0x58,selector);
        {GameAddress item=p;p=0;
            if(!game_menu_selector_append(a,selector,item,1,get(a,item,0x70)))goto fail;}
    }
    return !a->memory.fault;
fail:
    if(p)(void)game_menu_widget_destroy(a,p);
    (void)game_menu_selector_clear(a,selector);
    if(scaled)put(a,settings,0x14,horizon);
    return 0;
}
static int controls_enable(GameApplication *a,GameAddress e,int enable) {
    static const unsigned slots[]={0xa0,0xa4,0xa8,0xac,0xb0};unsigned i;
    for(i=0;i<5;++i)if(!game_menu_widget_enable(a,get(a,e,slots[i]),enable))return 0;
    return !a->memory.fault;
}
static int retire(GameApplication *a,GameAddress e,int apply) {
    static const unsigned slots[]={0x9c,0xa0,0xac,0xb0,0xa4,0xa8};
    static const unsigned offsets[]={8,0xc,0x10,0x14,0x18,0x1c,0x20,0x24,0x28,0x2c,0x30,0x34};
    static const unsigned keys[]={0,2,3,4,5,6,7,8,9,10,11,1};
    GameAddress p=get(a,0x58b0dc,0),settings=get(a,0x5943c8,0);unsigned i;
    if(!game_menu_widget_visible(a,get(a,e,0x60),0))return 0;
    if(p && !game_menu_widget_destroy(a,p))return 0;
    put(a,0x58b0dc,0,0);
    for(i=0;i<6;++i){p=get(a,e,slots[i]);if(p && !game_menu_widget_destroy(a,p))return 0;put(a,e,slots[i],0);}
    if(apply) {
        put(a,settings,0x14,get(a,settings,0x14)*100u);
        for(i=0;i<12;++i)if(!game_menu_option_set(a,keys[i],get(a,settings,offsets[i])))return 0;
    }
    return !a->memory.fault;
}
/* 004929e0 state 6: construct, animate, interact, retire and apply. */
int game_menu_graphics_state(GameApplication *a,GameAddress e,uint32_t phase) {
    static const GameAddress names[4][4]={{0x58d10c,0x58d0e8,0,0},{0x58d15c,0x58d134,0,0},
        {0x58d1c4,0x58d1a4,0x58d184,0},{0x58d22c,0x58d208,0x58d1e4,0}};
    static const unsigned ids[]={241,242,203,204};GameAddress p,animation=get(a,e,0x60);unsigned i;uint32_t frame;
    if(!phase) {
        p=game_menu_image_create(a,UINT32_MAX,0x58d0a4);if(!p)goto fail;put(a,0x58b0dc,0,p);
        if(!game_menu_widget_position(a,p,0,0,0,0) || !game_menu_widget_visible(a,p,1))goto fail;
        p=game_menu_image_create(a,UINT32_MAX,0x58d0c4);if(!p)goto fail;put(a,e,0x9c,p);
        if(!game_menu_image_mode(a,p,101u))goto fail;
        p=game_menu_edit_create(a,530,0,0);if(!p)goto fail;put(a,e,0xa0,p);put(a,p,0xe4,147);
        if(!game_menu_selector_resize(a,p,350,70))goto fail;
        for(i=0;i<4;++i) {
            p=game_menu_button_create(a,ids[i],names[i],i<2?530:69,i<2?1011:1000,i==0?1:i==1?UINT32_MAX:4,0);
            if(!p)goto fail;
            put(a,e,0xa4+i*4,p);
            if(i<2){unsigned j;static const unsigned images[]={0x58,0x5c,0x60,0x8c};
                for(j=0;j<4;++j){GameAddress image=get(a,p,images[j]);if(image && !game_menu_image_mode(a,image,101u))goto fail;}}
            else {put(a,p,0x50,144);put(a,p,0x54,i==2?145:146);}
        }
        if(!game_menu_widget_position(a,animation,544,383,12,0) || !game_menu_widget_visible(a,animation,1) || !values_build(a,get(a,e,0xa0)))goto fail;
        if(!layout(a,e,659,236,11))goto fail_scaled;
        for(i=0;i<6;++i)if(!game_menu_widget_visible(a,get(a,e,0x9c+i*4),1))goto fail_scaled;
        put(a,e,0x24,2);put(a,e,0x1c,100);put(a,e,0x28,3);put(a,e,0x20,100);return !a->memory.fault;
    }
    if(phase==1)return retire(a,e,1);
    if(phase==2 || phase==3) {
        unsigned counter=phase==2?0x1c:0x20;
        if(get(a,e,counter)==100) {
            put(a,animation,0x308,1);put(a,animation,0x304,phase==2?1:UINT32_MAX);
            if(phase==3 && !controls_enable(a,e,0))return 0;
            return game_menu_sound_loop_begin(a,151,0x5f06ac);
        }
        if(!game_menu_animation_advance(a,animation,&frame))return 0;
        if(frame==UINT32_MAX) {
            put(a,e,counter,1);
            if(phase==2 && !controls_enable(a,e,1))return 0;
            if(!game_menu_sound_loop_end(a,151,get(a,0x5f06ac,0)))return 0;
            if(phase==3)return !a->memory.fault;
        } else put(a,e,counter,2);
        p=get(a,e,0x9c);
        if(!layout(a,e,get(a,p,0x20)+(phase==2?0xffffffeeu:18),236,get(a,p,0x40)))return 0;
        put(a,0x58fd3c,0,1);
    }
    return !a->memory.fault;
fail_scaled:put(a,get(a,0x5943c8,0),0x14,get(a,get(a,0x5943c8,0),0x14)*100u);
fail:(void)retire(a,e,0);return 0;
}
