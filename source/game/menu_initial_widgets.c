#include "menu_music.h"
#include "sound_platform.h"
#include "menu_widgets.h"
#include "scene_projection_storage.h"
#include <stdio.h>
#include <string.h>

extern int game_menu_event_release_assets(GameApplication *,GameAddress);
extern int game_menu_popup_initialize(GameApplication *); /* 00524af0. */
extern int game_menu_text_cache_initialize(GameApplication *); /* 0054bf70. */
extern int game_menu_sound_bank_load(GameApplication *,uint32_t);

static uint32_t get(GameApplication *a,GameAddress p,unsigned o) {return game_memory_read_u32(&a->memory,p+o);}
static void put(GameApplication *a,GameAddress p,unsigned o,uint32_t v) {game_memory_write_u32(&a->memory,p+o,v);}
static int publish(GameApplication *a,GameAddress slot,GameAddress object) {
    if(!object){fprintf(stderr,"native initial widget failed: slot=%08lx\n",(unsigned long)slot);return 0;}
    game_memory_write_u32(&a->memory,slot,object);return !a->memory.fault;
}
static GameAddress button(GameApplication *a,uint32_t id,GameAddress n0,GameAddress n1,GameAddress n2,
    uint32_t callback,uint32_t command,uint32_t over,uint32_t release) {
    GameAddress names[4]={n0,n1,n2,0};
    GameAddress p=game_menu_button_create(a,id,names,UINT32_MAX,UINT32_MAX,command,callback);
    if(p){put(a,p,0x50,over);put(a,p,0x54,release);}return p;
}
static GameAddress routed(GameApplication *a,uint32_t id,GameAddress n0,GameAddress n1,GameAddress n2,
    uint32_t route,uint32_t recipient,uint32_t command) {
    GameAddress names[4]={n0,n1,n2,0};
    return game_menu_button_create(a,id,names,route,recipient,command,0);
}
static void transparent(GameApplication *a,GameAddress p) {
    static const unsigned slots[]={0x58,0x5c,0x60,0x8c};unsigned i;
    for(i=0;i<4;++i){GameAddress child=get(a,p,slots[i]);if(child)put(a,child,0xb0,101);}
}
static GameAddress toggle(GameApplication *a,uint32_t id,uint32_t group,GameAddress n0,GameAddress n1,GameAddress n2,
    uint32_t callback,uint32_t over,uint32_t release,int transparent) {
    GameAddress names[4]={n0,n1,n2,0};GameAddress p=game_menu_toggle_create(a,id,group,names,callback,0,0);
    unsigned i;static const unsigned slots[]={0x58,0x5c,0x60,0x8c};
    if(!p)return 0;
    put(a,p,0x50,over);put(a,p,0x54,release);
    if(transparent)for(i=0;i<4;++i){GameAddress image=get(a,p,slots[i]);if(image)put(a,image,0xb0,101);}
    return p;
}
static GameAddress text(GameApplication *a,GameAddress source,uint32_t r,uint32_t g,uint32_t b,unsigned font,uint32_t w,uint32_t h) {
    GameMenuTextStyle style={r,g,b,font,w,h,0,0,0};return game_menu_text_image_create(a,source,&style);
}
static GameAddress number(GameApplication *a,GameAddress address) {
    char value[32];GameMenuTextStyle style={148,156,82,2,60,11,0,0,0};
    snprintf(value,sizeof(value),"%d",(int32_t)game_memory_read_u32(&a->memory,address));
    return game_menu_text_image_create_bytes(a,value,&style);
}
static GameAddress image(GameApplication *a,GameAddress name,int transparent) {
    GameAddress p=game_menu_image_create(a,UINT32_MAX,name);if(p && transparent)put(a,p,0xb0,101);return p;
}
static GameAddress click_region(GameApplication *a,GameAddress image) {
    GameAddress p=game_menu_click_region_create(a,UINT32_MAX,0,UINT32_MAX,0,0x523800);unsigned i;
    if(!p)return 0;
    for(i=0;i<16;i+=4)put(a,p,0xc+i,get(a,image,0x20+i));
    return p;
}
/* 00490300: the actual mixed initial menu builder. Constructors take ordinary
 * arguments and own their resources; address constants identify data/callbacks.
 * Popup/text-cache consumers remain explicit native dependencies, never CPU calls. */
int game_menu_initial_widgets_construct(GameApplication *a,GameAddress event) {
    GameAddress p;unsigned i;float scale;uint32_t bits=0xb7800080u;
    static const struct {unsigned slot;uint32_t id,group,n0,n1,n2,callback;} groups[]={
        {0x64,800,101,0x58bd04,0x58bce0,0x58bcbc,0x523ee0},
        {0x68,801,101,0x58bd70,0x58bd4c,0x58bd28,0x523ee0},
        {0x6c,802,101,0x58bde4,0x58bdbc,0x58bd94,0x523ee0}};
    static const struct {unsigned slot;uint32_t id,n0,n1,n2,callback;} modes[]={
        {0x78,245,0x58bf30,0x58bf0c,0x58bee8,0x52bf80},
        {0x7c,246,0x58bf98,0x58bf74,0x58bf54,0x52bfb0},
        {0x80,247,0x58c000,0x58bfdc,0x58bfbc,0x52bfe0},
        {0x84,248,0x58c064,0x58c044,0x58c024,0x52c010}};
    static const struct {unsigned slot;uint32_t id,n0,n1,n2,callback;} quality[]={
        {0xc4,249,0x58c2fc,0x58c2d8,0x58c2b0,0x52c680},
        {0xcc,250,0x58c36c,0x58c348,0x58c320,0x52c720},
        {0xc8,251,0x58c3dc,0x58c3b8,0x58c390,0x52c810}};
    if(!publish(a,event+0x50,button(a,240,0x58bb8c,0x58bb64,0x58bb40,0x523fe0,UINT32_MAX,144,145)))goto fail;
    p=routed(a,241,0x58bbfc,0x58bbd4,0x58bbb0,69,1000,3);
    if(!publish(a,event+0x54,p))goto fail;
    put(a,p,0x50,144);put(a,p,0x54,146);
    if(!publish(a,event+0x58,button(a,242,0x58bc70,0x58bc48,0x58bc20,0x52c040,UINT32_MAX,144,145)))goto fail;
    p=game_menu_selector_create(a,UINT32_MAX,15,0,0x58bc98);
    if(!publish(a,event+0x5c,p) || !game_menu_selector_resize(a,p,123,186))goto fail;
    for(i=0;i<3;++i)if(!publish(a,event+groups[i].slot,toggle(a,groups[i].id,groups[i].group,
        groups[i].n0,groups[i].n1,groups[i].n2,groups[i].callback,UINT32_MAX,UINT32_MAX,0)))goto fail;
    if(!publish(a,0x58abe8,text(a,0x58be0c,146,156,82,1,293,104)) ||
       !publish(a,0x58abec,text(a,0x58be14,0,255,0,1,140,12)) ||
       !publish(a,0x58abf0,text(a,0x58be1c,0,255,0,1,140,12)) ||
       !publish(a,event+0x70,image(a,0x58be24,1)))goto fail;
    p=game_menu_edit_create(a,530,0,0x5285a0);
    if(!publish(a,event+0x74,p) || !game_menu_selector_resize(a,p,350,70))goto fail;
    put(a,p,0xe4,147);
    p=routed(a,243,0x58be70,0x58be48,0,530,1011,1);
    if(!publish(a,0x58abfc,p))goto fail;
    transparent(a,p);
    p=routed(a,244,0x58bec0,0x58be98,0,530,1011,UINT32_MAX);
    if(!publish(a,0x58ac00,p))goto fail;
    transparent(a,p);
    for(i=0;i<4;++i)if(!publish(a,event+modes[i].slot,toggle(a,modes[i].id,55,modes[i].n0,modes[i].n1,
        modes[i].n2,modes[i].callback,UINT32_MAX,143,0)))goto fail;
    if(!publish(a,event+0x88,button(a,UINT32_MAX,0x58c0c4,0x58c0a4,0x58c084,0x52c060,0,144,145)) ||
       !publish(a,event+0x90,button(a,UINT32_MAX,0x58c130,0x58c108,0x58c0e4,0x52c100,0,144,145)) ||
       !publish(a,event+0x8c,button(a,UINT32_MAX,0x58c19c,0x58c178,0x58c154,0x52c0b0,0,144,146)))goto fail;
    p=game_menu_animation_create(a,0x58c1c0);
    if(!publish(a,event+0xe4,p) || !game_menu_widget_position(a,p,547,46,100,0) ||
       !publish(a,event+0xe0,click_region(a,p)) ||
       !publish(a,0x58abf4,number(a,0x5ac910)) || !publish(a,0x58abf8,number(a,0x5acbec)))goto fail;
    if(!publish(a,event+0x94,button(a,UINT32_MAX,0x58c210,0x58c1f0,0,0x5236c0,1,UINT32_MAX,145)) ||
       !publish(a,event+0x98,button(a,UINT32_MAX,0x58c254,0x58c230,0,0x5236c0,UINT32_MAX,UINT32_MAX,145)))goto fail;
    p=game_menu_animation_create(a,0x58c278);
    if(!publish(a,event+0x60,p) || !game_menu_image_mode(a,p,101) || !publish(a,event+0xbc,image(a,0x58c290,1)))goto fail;
    for(i=0;i<3;++i)if(!publish(a,event+quality[i].slot,toggle(a,quality[i].id,55,quality[i].n0,quality[i].n1,
        quality[i].n2,quality[i].callback,UINT32_MAX,145,1)))goto fail;
    p=text(a,0x58c400,148,156,82,2,24,15);
    if(!publish(a,event+0xec,p) || !game_menu_text_image_set(a,p,0x58c404,1))goto fail;
    p=button(a,UINT32_MAX,0x58c450,0x58c42c,0x58c408,0x52c8b0,1,144,145);
    if(!publish(a,event+0xd0,p))goto fail;
    transparent(a,p);
    p=button(a,UINT32_MAX,0x58c4bc,0x58c498,0x58c474,0x52c8b0,UINT32_MAX,144,145);
    if(!publish(a,event+0xd4,p))goto fail;
    transparent(a,p);
    memcpy(&scale,&bits,4);
    p=game_menu_slider_create(a,UINT32_MAX,0x58c4e0,27,85,0x52ca10,5,
        (float)((long double)(game_music_volume_read()&65535u)*scale+1.0L),0,0,0);
    if(!publish(a,event+0xd8,p))goto fail;
    p=game_menu_slider_create(a,UINT32_MAX,0x58c504,27,85,0x52ca60,5,
        (float)((long double)(sound_platform_master_volume_read()&65535u)*scale+1.0L),0,0x52cab0,0x52cae0);
    if(!publish(a,event+0xdc,p) ||
       !publish(a,event+0xc0,button(a,UINT32_MAX,0x58c568,0x58c548,0x58c528,0x52c660,0,144,145)))goto fail;
    return !a->memory.fault && game_menu_popup_initialize(a) && game_menu_sound_bank_load(a,1) && game_menu_text_cache_initialize(a);
fail:
    (void)game_menu_event_release_assets(a,event);return 0;
}
