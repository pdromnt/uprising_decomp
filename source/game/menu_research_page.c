#include "menu_choice_controls.h"
#include "menu_sound.h"
extern int game_menu_text_cache_release(GameApplication *);
extern int game_menu_text_cache_initialize(GameApplication *);
extern int game_menu_request(GameApplication *,GameAddress,uint32_t,uint32_t,uint32_t);
extern int game_menu_register_observer(GameApplication *,GameAddress,uint32_t,uint32_t);
extern int game_menu_unregister_tick_method(GameApplication *,GameAddress,uint32_t);
extern int game_menu_controller_keyboard_handler(GameApplication *,GameAddress,uint32_t,uint32_t);
extern int game_menu_unregister_keyboard_method(GameApplication *,GameAddress,uint32_t);
extern uint32_t game_menu_milliseconds(GameApplication *);
/*52f310 still requires the shared live strategic world/campaign consumers. */
extern int game_menu_strategic_state(GameApplication *,uint32_t);
int game_menu_strategic_toggle(GameApplication *,uint32_t);
extern GameAddress game_menu_controller_construct(GameApplication *);
extern void game_menu_controller_destroy(GameApplication *,GameAddress);
/*Distinct4c18b0 and4c2330 screens, with their actual entry/show phases. */
extern int game_menu_strategic_depart(GameApplication *,uint32_t,uint32_t);
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t value){game_memory_write_u32(&a->memory,p,value);}
static int publish(GameApplication *a,GameAddress slot,GameAddress p,uint32_t x,uint32_t y,uint32_t z,int visible,int enable) {
    if(!p)return 0;
    put(a,slot,p);
    return game_menu_widget_position(a,p,x,y,z,0) &&
        (!visible || game_menu_widget_visible(a,p,1)) && (!enable || game_menu_widget_enable(a,p,1));
}
static GameAddress button(GameApplication *a,uint32_t id,GameAddress normal,GameAddress hover,GameAddress pressed,
        uint32_t callback,uint32_t argument,uint32_t timer) {
    GameAddress names[4]={normal,hover,pressed,0};
    GameAddress p=game_menu_button_create(a,id,names,callback?UINT32_MAX:69,
        callback?UINT32_MAX:1000,callback?argument:18,callback);
    if(p){put(a,p+0x84,UINT32_MAX);put(a,p+0x88,timer);}
    return p;
}
static int label(GameApplication *a,GameAddress slot,GameAddress value,const GameMenuTextStyle *style,
        uint32_t x,uint32_t y,uint32_t z,int visible) {
    return publish(a,slot,game_menu_text_image_create(a,value,style),x,y,z,visible,0);
}
static int release(GameApplication *a) {
    static const GameAddress slots[]={0x5977c4,0x597788,0x59778c,0x597790,0x5977b0,0x5977b8,
        0x59c6b4,0x597794,0x597798,0x59779c,0x5977a0,0x5977a4,0x5977a8,0x5977ac,
        0x5977b4,0x5977bc,0x5977c8,0x5977cc,0x5977d0,0x5977d4};
    unsigned i;GameAddress p;
    for(i=0;i<sizeof(slots)/sizeof(slots[0]);++i) {
        p=get(a,slots[i]);if(p && !game_menu_widget_destroy(a,p))return 0;
        put(a,slots[i],0);
    }
    return !a->memory.fault;
}
/*511240/state32: all controls and the preview have one actual native owner.
 * Global slots publish those owners to the existing catalog/purchase consumers. */
int game_menu_research_popup_state(GameApplication *a,GameAddress event,uint32_t phase) {
    static const struct {GameAddress slot,name;uint32_t x,y;} images[]={
        {0x597788,0x5977e4,0,0},{0x5977b0,0x597948,463,398},
        {0x5977d0,0x597c38,59,31},{0x5977d4,0x597c5c,596,31}};
    static const struct {GameAddress slot,n0,n1,n2;uint32_t id,callback,x;} categories[]={
        {0x597798,0x597a3c,0x597a14,0x5979ec,240,0x52bf80,157},
        {0x59779c,0x597ab0,0x597a88,0x597a64,241,0x52bfb0,274},
        {0x5977a0,0x597b28,0x597b00,0x597ad8,242,0x52bfe0,391}};
    GameAddress controller=get(a,0x59d4c0),p;unsigned i;uint32_t width;
    GameMenuTextStyle style={148,156,82,1,70,11,0,0,0};
    if(phase==1) {
        if(!game_menu_unregister_keyboard_method(a,controller,0x531700) ||
           !game_menu_unregister_tick_method(a,controller,0x512000) || !release(a))return 0;
        return get(a,event+0x14)==18 || game_menu_strategic_state(a,1);
    }
    if(phase)return !a->memory.fault; /*The retail state has no slide phases. */
    if(!game_menu_text_cache_release(a) || !game_menu_text_cache_initialize(a))return 0;
    if(!publish(a,images[0].slot,game_menu_image_create(a,UINT32_MAX,images[0].name),0,0,0,1,0))goto failed;
    p=button(a,UINT32_MAX,0x59785c,0x597834,0x59780c,0,0,0);
    if(!publish(a,0x59778c,p,38,454,51,1,1))goto failed;
    put(a,p+0x50,144);put(a,p+0x54,145);
    p=button(a,UINT32_MAX,0x5978d0,0x5978a8,0x597884,0x52fce0,0,0);
    if(!publish(a,0x597790,p,460,454,51,1,1))goto failed;
    put(a,p+0x50,144);put(a,p+0x54,145);
    {GameAddress names[4]={0x597920,0,0x5978f8,0};p=game_menu_checkbox_create(a,names,0x5120d0);}
    if(!publish(a,0x5977ac,p,162,198,51,1,1))goto failed;
    put(a,p+0x50,UINT32_MAX);put(a,p+0x54,149);put(a,0x5977d8,0);
    if(!publish(a,0x5977b0,game_menu_image_create(a,UINT32_MAX,0x597948),463,398,55,0,0) ||
       !game_menu_image_mode(a,get(a,0x5977b0),101) ||
       !label(a,0x5977b8,0x597970,&style,468,407,56,0))goto failed;
    p=button(a,UINT32_MAX,0x5979b8,0x597994,0x597974,0x512720,UINT32_MAX,0);
    if(!publish(a,0x5977b4,p,563,258,51,1,1))goto failed;
    style.width=84;style.height=12;
    if(!label(a,0x59c6b4,0x5979dc,&style,0,79,60,0))goto failed;
    p=get(a,0x59c6b4);width=get(a,p+0x28)-get(a,p+0x20);
    if(!game_menu_widget_position(a,p,331-width/2,79,60,0) || !game_menu_widget_visible(a,p,1) ||
       !game_menu_faction_budget_feedback(a))goto failed;
    style.red=238;style.green=216;style.blue=0;style.font=4;style.width=464;style.height=30;
    if(!label(a,0x5977c8,0x5979e0,&style,62,274,2,1))goto failed;
    style.width=108;
    if(!label(a,0x5977cc,0x5979e4,&style,445,274,2,1))goto failed;
    style.red=0;style.green=203;style.blue=14;style.font=1;style.width=300;style.height=134;
    if(!label(a,0x5977bc,0x5979e8,&style,241,298,2,1))goto failed;
    p=game_menu_edit_create(a,530,11,0x5285a0);
    if(!publish(a,0x597794,p,158,144,10,1,1) || !game_menu_selector_resize(a,p,350,42))goto failed;
    put(a,p+0xe4,147);
    for(i=0;i<3;++i) {
        GameAddress names[4]={categories[i].n0,categories[i].n1,categories[i].n2,0};
        p=game_menu_toggle_create(a,categories[i].id,30,names,categories[i].callback,0,0);
        if(!publish(a,categories[i].slot,p,categories[i].x,114,10,1,1))goto failed;
        put(a,p+0x50,UINT32_MAX);put(a,p+0x54,143);
    }
    p=button(a,243,0x597b98,0x597b74,0x597b50,0x512070,1,1);
    if(!publish(a,0x5977a4,p,95,145,10,1,1))goto failed;
    put(a,p+0x70,0x512060);
    for(i=0;i<3;++i) {GameAddress image=get(a,p+0x58+i*4);put(a,image+0xb0,101);}
    p=button(a,244,0x597c10,0x597be8,0x597bc0,0x512070,UINT32_MAX,UINT32_MAX);
    if(!publish(a,0x5977a8,p,513,145,10,1,1))goto failed;
    put(a,p+0x70,0x512060);
    for(i=0;i<3;++i) {GameAddress image=get(a,p+0x58+i*4);put(a,image+0x13c,254);put(a,image+0xb0,101);}
    for(i=2;i<4;++i)
        if(!publish(a,images[i].slot,game_menu_image_create(a,UINT32_MAX,images[i].name),images[i].x,images[i].y,10,0,0))goto failed;
    if(!publish(a,0x5977c4,game_menu_research_preview_create(a,226,164),48,268,3,1,0))goto failed;
    put(a,0x5977c0,0);
    if(!game_menu_button_input(a,get(a,0x597798),0x202,0,0) || !game_menu_button_input(a,get(a,0x597798),0x201,0,0) ||
       !game_menu_controller_keyboard_handler(a,controller,0x531700,UINT32_MAX) ||
       !game_menu_register_observer(a,controller,0x512000,0))goto failed;
    return !a->memory.fault;
failed:
    (void)game_menu_unregister_tick_method(a,controller,0x512000);
    (void)game_menu_unregister_keyboard_method(a,controller,0x531700);
    (void)release(a);return 0;
}
/*512000/512070: actual timed arrow feedback and selector page delivery. */
int game_menu_research_popup_tick(GameApplication *a) {
    GameAddress left=get(a,0x5977d0),right=get(a,0x5977d4);uint32_t end,now;
    if(!get(a,left+0x14) && !get(a,right+0x14))return !a->memory.fault;
    end=get(a,0x5977dc)+190;now=game_menu_milliseconds(a);
    if((end<0x80000000u?(int64_t)end:(int64_t)end-INT64_C(4294967296))<
       (now<0x80000000u?(int64_t)now:(int64_t)now-INT64_C(4294967296))) {
        if(!game_menu_widget_visible(a,left,0) || !game_menu_widget_visible(a,right,0))return 0;
        put(a,0x58fd3c,1);
    }
    return !a->memory.fault;
}
int game_menu_research_popup_action(GameApplication *a,uint32_t method,uint32_t argument) {
    if(method==0x512060)return !a->memory.fault; /*Original returns literal1. */
    if(method==0x52fce0)return game_menu_strategic_toggle(a,0) && game_menu_strategic_toggle(a,1);
    if(method!=0x512070)return 0;
    if((argument==1 || argument==UINT32_MAX) &&
       !game_menu_widget_visible(a,get(a,argument==1?0x5977d0:0x5977d4),1))return 0;
    put(a,0x58fd3c,1);put(a,0x5977dc,game_menu_milliseconds(a));
    return game_menu_request(a,get(a,0x59d4c0),530,1011,argument);
}

/*531a50: the strategic/research modal borrows the previous controller and owns
 * its replacement, background and four buttons. The existing bounded widget
 * records are authoritative; all image owners live in the same menu cache. */
int game_menu_strategic_toggle(GameApplication *a,uint32_t phase) {
    static const GameAddress slots[]={0x59c6dc,0x59c6e0,0x59c6e4,0x59c6e8,0x59c6ec};
    static const GameAddress names[2][4][4]={
        {{0x59d0a0,0x59d078,0x59d054,0},{0x59d114,0x59d0ec,0x59d0c8,0},
         {0x59d18c,0x59d164,0x59d13c,0},{0x59d200,0x59d1d8,0x59d1b4,0}},
        {{0x59d2a0,0x59d278,0x59d250,0},{0x59d318,0x59d2f0,0x59d2c8,0},
         {0x59d394,0x59d368,0x59d340,0},{0x59d410,0x59d3e8,0x59d3c0,0}}};
    static const uint32_t callbacks[]={0x532190,0x5321c0,0x5321f0,0x532200};
    GameAddress parent,controller,p;uint32_t state,x,y,width,height;unsigned family,i;
    if(phase==0) {
        state=get(a,get(a,0x59d4c4)+0x3c);
        /*Only states18 and32 have assets in the original constructor. Do not
         * silently create an empty popup for an unsupported caller. */
        if((state!=18 && state!=32) || get(a,0x59c738) || a->memory.fault)return 0;
        parent=get(a,0x59d4c0);if(!parent)return 0;
        controller=game_menu_controller_construct(a);
        if(!controller)return 0;
        put(a,0x59c738,parent);put(a,0x59d4c0,controller);family=state==32;
        p=game_menu_image_create(a,UINT32_MAX,family?0x59d228:0x59d02c);
        if(!p)goto failed;
        put(a,slots[0],p);width=get(a,p+0x28)-get(a,p+0x20);height=get(a,p+0x2c)-get(a,p+0x24);
        x=320-width/2;y=240-height/2;
        if(!game_menu_widget_position(a,p,x,y,1000,0))goto failed;
        for(i=0;i<4;++i) {
            p=game_menu_button_create(a,5000,names[family][i],UINT32_MAX,0,0,callbacks[i]);
            if(!p)goto failed;
            put(a,slots[i+1],p);
            if(!game_menu_widget_position(a,p,x+36,y+40+i*37,1001,0))goto failed;
            put(a,p+0x50,UINT32_MAX);put(a,p+0x54,121);
        }
        put(a,0x58fd3c,1);return !a->memory.fault;
    }
    if(phase==1) {
        for(i=0;i<5;++i) {
            p=get(a,slots[i]);if(!p || !game_menu_widget_visible(a,p,1))return 0;
        }
        for(i=1;i<5;++i)if(!game_menu_widget_enable(a,get(a,slots[i]),1))return 0;
        return !a->memory.fault;
    }
    if(phase==2) {
        for(i=1;i<5;++i)if(!game_menu_widget_enable(a,get(a,slots[i]),0))return 0;
        return !a->memory.fault;
    }
    if(phase!=4)return !a->memory.fault; /*Phase3 and out-of-range are empty in retail. */
    for(i=0;i<5;++i) {
        p=get(a,slots[i]);if(p && !game_menu_widget_destroy(a,p))return 0;
        put(a,slots[i],0);
    }
    parent=get(a,0x59c738);
    /*An already closed modal must not destroy the borrowed parent. */
    if(parent) {
        controller=get(a,0x59d4c0);if(controller)game_menu_controller_destroy(a,controller);
        put(a,0x59d4c0,parent);put(a,0x59c738,0);
    }
    put(a,0x58fd3c,1);return !a->memory.fault;
failed:
    (void)game_menu_strategic_toggle(a,4);return 0;
}

/*532190/1c0/1f0/2200: close before handing control back to the real caller. */
int game_menu_strategic_modal_action(GameApplication *a,uint32_t method) {
    if(method!=0x532190 && method!=0x5321c0 && method!=0x5321f0 && method!=0x532200)return 0;
    if(!game_menu_strategic_toggle(a,4))return 0;
    if(method==0x5321f0)return !a->memory.fault;
    if(method==0x532200)return game_menu_request(a,get(a,0x59d4c0),69,1000,3);
    put(a,0x59c6a0,1);
    return game_menu_strategic_depart(a,method==0x532190?0:1,0) &&
        game_menu_strategic_depart(a,method==0x532190?0:1,1);
}
