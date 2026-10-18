#include "menu_widgets.h"
#include <stdio.h>
#include <string.h>
extern int game_menu_window_clear(GameApplication *);
extern int game_menu_palette_load_named(GameApplication *,GameAddress);
extern int game_menu_controller_keyboard_handler(GameApplication *,GameAddress,uint32_t,uint32_t);
extern int game_menu_unregister_keyboard_method(GameApplication *,GameAddress,uint32_t);
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
static int read_text(GameApplication *a,GameAddress source,char *out,size_t size) {
    size_t i;for(i=0;i<size;++i){out[i]=(char)game_memory_read_u8(&a->memory,source+(uint32_t)i);if(!out[i])return !a->memory.fault;}return 0;
}
static int show(GameApplication *a,GameAddress p,uint32_t x,uint32_t y,uint32_t z) {
    return game_menu_widget_position(a,p,x,y,z,0) && game_menu_widget_visible(a,p,1);
}
/*54d6b0: only an untied positive maximum gets green. Signed comparisons and
 * resetting the tie when a later greater maximum appears are intentional. */
uint32_t game_menu_results_highlight(GameApplication *a,uint32_t column,uint32_t player) {
    int32_t maximum=0;unsigned winner=0,i;int tied=1;uint8_t mask=game_memory_read_u8(&a->memory,0x5f0158);
    for(i=0;i<6;++i)if(mask&(1u<<i)) {
        GameAddress manager=get(a,0x5f0210+i*4);int32_t value;
        if(!manager || column>=18)return 0;
        value=(int32_t)get(a,manager+0x154+column*4);
        if(value>maximum){maximum=value;winner=i;tied=0;}else if(value==maximum)tied=1;
    }
    return player==winner && !tied?0x0200ff00:0x023e9c94;
}
static int paint(GameApplication *a,GameAddress image,const char *text,uint32_t font,uint32_t color,int32_t x,int32_t y) {
    return game_menu_text_surface_paint(a,image,font,text,color,x,y,0,get(a,0x5840f8)!=0);
}
/*54d760/54d820: construct the actual row's pixels once on an existing owned
 * mutable image. The manager is borrowed only while painting. Toggle refresh
 * rebuilds rows, so no subclass/vtable or second statistics state is necessary. */
static GameAddress result_row(GameApplication *a,GameAddress manager,uint32_t selection) {
    static const unsigned columns[]={0,2,1,3,4,5,7};
    static const int32_t positions[]={60,115,170,225,280,335,390};
    GameAddress source=0,image=0;uint32_t player,bank=(1u-selection)*9u,color;char text[128];unsigned i;
    if(!manager || selection>1)return 0;
    source=game_menu_image_create(a,UINT32_MAX,0x58c5a8);if(!source)goto fail;
    image=game_menu_image_surface_create(a,get(a,source+0x28),get(a,source+0x2c),0);if(!image)goto fail;
    if(!game_menu_image_compose(a,image,source,0,0,0,0) || !game_menu_widget_destroy(a,source))goto fail;source=0;
    player=game_memory_read_u16(&a->memory,manager+8);
    if(player<4 && game_memory_read_u8(&a->memory,manager+0x39c+player)) {
        if(!read_text(a,manager+0x39c,text,sizeof(text)))goto fail;
    }else if(player==4 || player==5) {
        if(!read_text(a,player==4?0x59de44:0x59de50,text,sizeof(text)))goto fail;
    }else snprintf(text,sizeof(text),"PLAYER %u",player);
    if(!paint(a,image,text,3,0x02529c94,6,25))goto fail;
    if(player==4 || player==5)memcpy(text,"  -",4);
    else snprintf(text,sizeof(text),"%3d",(int32_t)get(a,manager+0x16c+bank*4));
    if(!paint(a,image,text,2,0x023e9c94,5,3))goto fail;
    for(i=0;i<7;++i) {
        unsigned column=bank+columns[i];color=game_menu_results_highlight(a,column,player);if(!color)goto fail;
        if(player==5 && bank && columns[i]==4)memcpy(text,"  -",4);
        else snprintf(text,sizeof(text),"%3d",(int32_t)get(a,manager+0x154+column*4));
        if(!paint(a,image,text,2,color,positions[i],3))goto fail;
    }
    if(!game_menu_image_mode(a,image,101))goto fail;
    if(a->memory.fault)goto fail;
    return image;
fail:
    if(source)(void)game_menu_widget_destroy(a,source);
    if(image)(void)game_menu_widget_destroy(a,image);
    return 0;
}
static int clear_rows(GameApplication *a,GameAddress event) {
    unsigned i;for(i=0;i<6;++i){GameAddress slot=event+0x158+i*4,p=get(a,slot);
        if(p && !game_menu_widget_destroy(a,p))return 0;put(a,slot,0);}
    return !a->memory.fault;
}
/*491b70: event owns all six row slots; the authoritative statistics remain
 * in the existing player managers. At most five rows are shown. */
int game_menu_results_refresh(GameApplication *a,GameAddress event,uint32_t selection) {
    uint8_t mask=game_memory_read_u8(&a->memory,0x5f0158);unsigned count=0,i,shown=0,step,y=184;
    put(a,0x58fd3c,1);if(!clear_rows(a,event))return 0;
    if(selection!=UINT32_MAX)put(a,event+0x154,selection);selection=get(a,event+0x154);
    if(selection>1)return 0;
    for(i=0;i<6;++i)if(mask&(1u<<i))++count;
    if(!count)return 0; /*Malformed/no-player results cannot divide by zero. */
    if(count>5)count=5;step=236/count;if(step>75)step=75;
    for(i=0;i<6 && shown<5;++i)if(mask&(1u<<i)) {
        GameAddress row=result_row(a,get(a,0x5f0210+i*4),selection);if(!row)return 0;
        put(a,event+0x158+i*4,row);if(!show(a,row,105,y,1))return 0;y+=step;++shown;
    }
    return !a->memory.fault;
}
int game_menu_results_toggle(GameApplication *a,GameAddress receiver) {
    return game_menu_results_refresh(a,get(a,0x59d4c4),1u-get(a,receiver+0x94));
}
int game_menu_results_state(GameApplication *a,GameAddress event,uint32_t phase) {
    static const GameAddress frames[4]={0x58d53c,0x58d51c,0x58d4fc,0};
    GameAddress controller=get(a,0x59d4c0),p;char level[128];uint32_t next;unsigned i;
    if(phase==1) {
        if(!game_menu_unregister_keyboard_method(a,controller,0x5246f0))return 0;
        for(i=0;i<5;++i){GameAddress slot=0x58b108+i*4;p=get(a,slot);if(p && !game_menu_widget_destroy(a,p))return 0;put(a,slot,0);}
        return clear_rows(a,event) && game_menu_window_clear(a) && game_menu_palette_load_named(a,0x58d5c8);
    }
    if(phase)return !a->memory.fault;
    if(!game_menu_window_clear(a) || !game_menu_palette_load_named(a,0x58d494) || !clear_rows(a,event) ||
       !read_text(a,0x629808,level,sizeof(level)))goto fail;
    if(strstr(level,"Train")) {
        GameMenuTextStyle style={148,156,82,3,430,220,0,0,0};next=23;
        p=game_menu_image_create(a,UINT32_MAX,0x58d4bc);if(!p)goto fail;put(a,0x58b108,p);if(!show(a,p,0,0,0))goto fail;
        p=game_menu_text_image_create(a,0x5ac6b8,&style);if(!p)goto fail;put(a,0x58b114,p);if(!show(a,p,105,182,5))goto fail;
    }else {
        p=game_menu_image_create(a,UINT32_MAX,0x58d4dc);if(!p)goto fail;put(a,0x58b108,p);if(!show(a,p,0,0,0))goto fail;
        if(!game_menu_results_refresh(a,event,1))goto fail;
        if(get(a,0x5910a8))put(a,0x58abd8,18);put(a,0x5910a8,0);next=get(a,0x58abd8);
        p=game_menu_checkbox_create(a,frames,0x52c1f0);if(!p)goto fail;put(a,0x58b110,p);
        if(!show(a,p,392,60,10) || !game_menu_widget_enable(a,p,1))goto fail;
        if((next==18 || next==9) && (game_memory_read_u8(&a->memory,0x5f0158)&1)) {
            GameAddress manager=get(a,0x5f0210),name=get(a,manager+0x148)?0x58d55c:get(a,manager+0x14c)?0x58d580:0x58d5a4;
            p=game_menu_image_create(a,UINT32_MAX,name);if(!p)goto fail;put(a,0x58b118,p);put(a,p+0x13c,254);
            if(!game_menu_image_mode(a,p,101) || !show(a,p,302u-get(a,p+0x28)/2u,134,10))goto fail;
        }
    }
    p=game_menu_click_region_create(a,362,69,1000,next,0);if(!p)goto fail;put(a,0x58b10c,p);
    put(a,p+0xc,0);put(a,p+0x10,0);put(a,p+0x14,640);put(a,p+0x18,480);
    if(!game_menu_widget_enable(a,p,1) || !game_menu_controller_keyboard_handler(a,controller,0x5246f0,next))goto fail;
    return !a->memory.fault;
fail:(void)game_menu_results_state(a,event,1);return 0;
}
