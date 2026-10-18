#include "menu_choice_controls.h"
#include "menu_sound.h"
extern int game_menu_quick_state(GameApplication *,GameAddress,uint32_t);
extern int game_menu_sound_loop_begin(GameApplication *,uint32_t,GameAddress);
extern int game_menu_sound_loop_end(GameApplication *,uint32_t,uint32_t);
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
typedef struct ResearchPlacement {GameAddress slot;uint32_t x,y;} ResearchPlacement;
/*491cf0: borrow the controls already owned by the application's menu event.
 * Moving them updates their original hit regions and intrusive child positions. */
static const ResearchPlacement layout[]={
    {0x74,32,62},{0x78,31,32},{0x7c,119,32},{0x80,207,32},{0x84,295,32},
    {0x58abfc,15,62},{0x58ac00,386,62},{0x88,423,59},{0x90,423,86},
    {0x8c,423,114},{0x58abf4,330,4},{0x58abf8,330,18},{0x94,363,5},{0x98,381,5}
};
static GameAddress object(GameApplication *a,GameAddress event,GameAddress slot) {
    return get(a,slot<0x100?event+slot:slot);
}
int game_menu_research_controls_position(GameApplication *a,GameAddress event,uint32_t x,uint32_t y,uint32_t z) {
    GameAddress panel=get(a,event+0x70);unsigned i;
    if(!game_menu_widget_position(a,panel,x,y,z,0))return 0;
    x=get(a,panel+0x20);y=get(a,panel+0x24);
    for(i=0;i<sizeof(layout)/sizeof(layout[0]);++i)
        if(!game_menu_widget_position(a,object(a,event,layout[i].slot),x+layout[i].x,y+layout[i].y,z+1,0))return 0;
    return !a->memory.fault;
}
/*491e60/491f30/491fd0/492070 share the same owners with network setup.
 * The two resource-adjustment buttons are exposed only to the network host. */
int game_menu_research_controls_visible(GameApplication *a,GameAddress event,int visible) {
    static const GameAddress slots[]={0x70,0x74,0x78,0x7c,0x80,0x84,0x58abfc,0x58ac00,
        0x88,0x8c,0x90,0x58abf4,0x58abf8,0x94,0x98};
    unsigned i;
    if(visible && (!game_menu_widget_base_state(a,get(a,event+0x88),0) ||
                  !game_menu_widget_base_state(a,get(a,event+0x8c),0)))return 0;
    for(i=0;i<sizeof(slots)/sizeof(slots[0]);++i) {
        if(visible && i>=13 && (!get(a,0x5eff04) || !get(a,0x5f0150)))continue;
        if(!game_menu_widget_visible(a,object(a,event,slots[i]),visible))return 0;
    }
    return !a->memory.fault;
}
int game_menu_research_controls_enable(GameApplication *a,GameAddress event,int enable) {
    static const GameAddress slots[]={0x74,0x78,0x7c,0x80,0x84,0x58ac00,0x58abfc,0x88,0x8c,0x90,0x94,0x98};
    unsigned i;
    for(i=0;i<sizeof(slots)/sizeof(slots[0]);++i) {
        if(enable && i>=10 && (!get(a,0x5eff04) || !get(a,0x5f0150)))continue;
        if(!game_menu_widget_enable(a,object(a,event,slots[i]),enable))return 0;
    }
    return !a->memory.fault;
}
/*4950ec: the actual quick-game research state, including its two animation
 * phases. No new page object or mirrored research values are introduced. */
int game_menu_research_state(GameApplication *a,GameAddress event,uint32_t phase) {
    GameAddress p,animation=get(a,event+0x60);unsigned i;uint32_t frame;
    static const GameAddress labels[]={0x58abe8,0x58abec,0x58abf0};
    if(!phase) {
        if(get(a,event+0x10)!=9 && !game_menu_quick_state(a,event,0))return 0;
        p=get(a,event+0x58);
        if(!game_menu_widget_visible(a,p,0) || !game_menu_widget_enable(a,p,0))return 0;
        put(a,0x58fd3c,1);
        p=game_menu_image_create(a,UINT32_MAX,0x58d298);if(!p)return 0;put(a,event+0x4c,p);
        if(!game_menu_widget_position(a,p,0,0,0,0) || !game_menu_widget_visible(a,p,1) ||
           !game_menu_widget_position(a,animation,544,383,12,0) || !game_menu_widget_visible(a,animation,1) ||
           !game_menu_widget_visible(a,get(a,event+0x5c),1))goto fail;
        for(i=0;i<3;++i)if(!game_menu_widget_visible(a,get(a,labels[i]),1))goto fail;
        if(!game_menu_research_controls_position(a,event,659,236,11) || !game_menu_research_controls_visible(a,event,1))goto fail;
        for(i=0;i<4;++i)if(!game_menu_widget_base_state(a,get(a,event+0x78+i*4),0))goto fail;
        p=get(a,event+0x78);
        if(!game_menu_button_input(a,p,0x202,0,0) || !game_menu_button_input(a,p,0x201,0,0) ||
           !game_menu_research_catalog_build(a,get(a,event+0x74),0))goto fail;
        put(a,event+0x24,2);put(a,event+0x1c,100);put(a,event+0x28,3);put(a,event+0x20,100);
        return !a->memory.fault;
    }
    if(phase==2 || phase==3) {
        GameAddress progress=event+(phase==2?0x1c:0x20);
        if(get(a,progress)==100) {
            /*40c4c0 deliberately discards its duration; advance consumes one
             * retained image frame per application transition tick. */
            put(a,animation+0x308,1);put(a,animation+0x304,phase==2?1:UINT32_MAX);
            if(phase==3 && !game_menu_research_controls_enable(a,event,0))return 0;
            put(a,event+0x48,1);
            return !a->memory.fault && game_menu_sound_loop_begin(a,151,0x5f06ac);
        }
        if(!game_menu_animation_advance(a,animation,&frame))return 0;
        if(frame==UINT32_MAX) {
            put(a,progress,1);
            if(phase==2 && !game_menu_research_controls_enable(a,event,1))return 0;
            put(a,event+0x48,0);put(a,0x58fd3c,1);
            if(!game_menu_sound_loop_end(a,151,get(a,0x5f06ac)))return 0;
        } else {put(a,progress,2);put(a,event+0x48,1);}
        p=get(a,event+0x70);
        return game_menu_research_controls_position(a,event,get(a,p+0x20)+(phase==2?0u-18u:18u),236,get(a,p+0x40));
    }
    if(phase!=1)return !a->memory.fault;
    p=get(a,event+0x4c);
    if(p && !game_menu_widget_destroy(a,p))return 0;
    put(a,event+0x4c,0);
    if(!game_menu_widget_visible(a,get(a,event+0x5c),0))return 0;
    for(i=0;i<3;++i)if(!game_menu_widget_visible(a,get(a,labels[i]),0))return 0;
    if(!game_menu_selector_clear(a,get(a,event+0x74)) || !game_menu_research_controls_visible(a,event,0))return 0;
    return game_menu_widget_visible(a,animation,0);
fail:
    (void)game_menu_research_state(a,event,1);return 0;
}
static GameUiRect rectangle(GameApplication *a,GameAddress p) {
    GameUiRect r={(int32_t)get(a,p),(int32_t)get(a,p+4),(int32_t)get(a,p+8),(int32_t)get(a,p+12)};return r;
}
static int intersection(GameUiRect *out,GameUiRect a,GameUiRect b) {
    out->left=a.left>b.left?a.left:b.left;out->top=a.top>b.top?a.top:b.top;
    out->right=a.right<b.right?a.right:b.right;out->bottom=a.bottom<b.bottom?a.bottom:b.bottom;
    return out->left<out->right && out->top<out->bottom;
}
static int clipped_draw(GameApplication *a,GameAddress p,GameUiRect bounds,GameAddress region) {
    GameUiRect clip;
    if(!intersection(&clip,bounds,rectangle(a,region)))return !a->memory.fault;
    return game_menu_widget_draw(a,p,&clip);
}
/*492100: the transition's actual damage consumer. Reuse the native widget and
 * surface services; retain the background/selector/panel/corner draw order. */
int game_menu_loading_tick(GameApplication *a,GameAddress event) {
    extern int game_menu_surface_present_rectangle(GameApplication *,const GameUiRect *);
    static const GameAddress slots[]={0x60,0x74,0x78,0x7c,0x80,0x84,0x58abfc,0x58ac00,0x88,0x90,0x8c};
    GameUiRect screen={0,0,639,479},strip,clip;
    GameAddress panel=get(a,event+0x70),corner=get(a,event+0x60),background=0;unsigned i,o;
    uint32_t state=get(a,event+0x3c);
    if(state==11)background=get(a,event+0x4c);
    else if(state==22)background=get(a,event+0xe8);
    else return 0;
    if(!clipped_draw(a,background,screen,panel+0x30))return 0;
    if(state==11 && !clipped_draw(a,get(a,event+0x5c),rectangle(a,panel+0x30),get(a,event+0x5c)+0x20))return 0;
    if(!clipped_draw(a,background,screen,corner+0x20) || !clipped_draw(a,panel,screen,panel+0x20))return 0;
    for(o=0;o<16;o+=4)put(a,panel+0x30+o,get(a,panel+0x20+o));
    put(a,panel+0x1c,0);
    for(i=0;i<sizeof(slots)/sizeof(slots[0]);++i) {
        GameAddress p=object(a,event,slots[i]);
        if(!clipped_draw(a,p,screen,p+0x20))return 0;
    }
    strip.left=115;strip.right=639;strip.top=(int32_t)get(a,panel+0x24);strip.bottom=(int32_t)get(a,panel+0x2c);
    if(!game_menu_surface_present_rectangle(a,&strip))return 0;
    if(intersection(&clip,rectangle(a,corner+0x20),screen) && !game_menu_surface_present_rectangle(a,&clip))return 0;
    return !a->memory.fault;
}
