#include "menu_widgets.h"
#include "menu_pause.h"
#include <stdlib.h>
#include <string.h>

extern int game_menu_text_wrapped_height(GameApplication *,unsigned,uint32_t,const char *,uint32_t *);
extern GameAddress game_menu_controller_construct(GameApplication *);
extern void game_menu_controller_destroy(GameApplication *,GameAddress);
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
static const GameAddress slots[]={0x591164,0x591168,0x59116c,0x591170};
static int retire(GameApplication *a,GameAddress slot){
    GameAddress p=get(a,slot);if(p && !game_menu_widget_destroy(a,p))return 0;
    put(a,slot,0);return !a->memory.fault;
}
static int release(GameApplication *a){
    return retire(a,0x591170) && retire(a,0x59116c) && retire(a,0x591164) && retire(a,0x591168);
}
static void restore_controller(GameApplication *a){
    GameAddress parent=get(a,0x591178),current=get(a,0x59d4c0);
    if(!parent)return;
    if(current && current!=parent)game_menu_controller_destroy(a,current);
    put(a,0x59d4c0,parent);put(a,0x591178,0);put(a,0x591174,0);put(a,0x58fd3c,1);
}
static char *read_text(GameApplication *a,GameAddress source,size_t *length){
    size_t n;char *text;if(!source)return NULL;
    for(n=0;n<65536 && !a->memory.fault;++n)if(!game_memory_read_u8(&a->memory,source+(uint32_t)n))break;
    if(n==65536 || a->memory.fault)return NULL;
    text=malloc(n+1);if(!text)return NULL;
    if(!game_memory_read(&a->memory,source,text,n+1)){free(text);return NULL;}
    *length=n;return text;
}
/*The source strings are borrowed until this construction copies them into the
 *existing text owner. Retail's large branch omits the trailing objective text;
 *it also measures at width264 despite creating a width338 surface. */
static char *information_text(GameApplication *a,int large){
    size_t lengths[4]={0},used=0,i;char *parts[4]={0},*result=NULL,*extension;
    GameAddress sources[]={large?0x594178:0x59405c,0x5efe80,0x5ac9e0,large?0:get(a,0x59109c)};
    for(i=0;i<(large?3u:4u);++i){parts[i]=read_text(a,sources[i],lengths+i);if(!parts[i])goto done;}
    extension=strstr(parts[1],".lfl");if(extension){*extension=0;lengths[1]=(size_t)(extension-parts[1]);}
    result=malloc(lengths[0]+lengths[1]+lengths[2]+lengths[3]+3);if(!result)goto done;
    for(i=0;i<4;++i){if(parts[i]){memcpy(result+used,parts[i],lengths[i]);used+=lengths[i];}
        if(i==1 || i==2)result[used++]='\n';}
    result[used]=0;
done:for(i=0;i<4;++i)free(parts[i]);return result;
}
static int draw(GameApplication *a,GameAddress p){
    GameUiRect r;int large=get(a,0x597f80)!=0;
    if(!p)return !a->memory.fault;
    r.left=(int32_t)get(a,p+0x20);r.top=(int32_t)get(a,p+0x24);
    r.right=(int32_t)get(a,p+0x28);r.bottom=(int32_t)get(a,p+0x2c);
    if(r.left<0)r.left=0;
    if(r.top<0)r.top=0;
    if(r.right>(large?639:319))r.right=large?639:319;
    if(r.bottom>(large?479:199))r.bottom=large?479:199;
    return r.left>=r.right || r.top>=r.bottom || game_menu_widget_draw(a,p,&r);
}
/*4c6470: actual mission-information controls, owned scrolling content, two
 *layouts and the training modal's independently owned controller. */
int game_menu_ingame_4c6470(GameApplication *a,uint32_t phase){
    static const GameAddress back[2][4]={{0x5940cc,0x5940a4,0x59407c,0},{0x5941e8,0x5941c0,0x594198,0}};
    int large=get(a,0x597f80)!=0,modal=large && get(a,0x591174)!=0;
    unsigned i;uint32_t x,y,width,height=0;GameAddress p,selector,frame=0,row=0;char *text=NULL;
    if(phase==4)return release(a);
    if(phase==1 || phase==2){
        p=get(a,0x591170);if(p && !game_menu_widget_enable(a,p,phase==1))return 0;
        p=get(a,0x59116c);return (!p || game_menu_widget_enable(a,p,phase==1)) && !a->memory.fault;
    }
    if(phase==3)return draw(a,get(a,0x591164)) && draw(a,get(a,0x59116c)) && draw(a,get(a,0x591170)) && !a->memory.fault;
    if(phase)return !a->memory.fault;
    for(i=0;i<4;++i)if(get(a,slots[i]))return 0;
    if(get(a,0x591178))return 0;
    if(modal){p=game_menu_image_create(a,UINT32_MAX,0x5940f4);if(!p)goto failed;
        put(a,0x591168,p);if(!game_menu_widget_position(a,p,0,0,19,0))goto failed;}
    p=game_menu_image_create(a,UINT32_MAX,large?0x594114:0x593ff8);if(!p)goto failed;
    put(a,0x591164,p);width=get(a,p+0x28)-get(a,p+0x20);height=get(a,p+0x2c)-get(a,p+0x24);
    x=(large?640u:320u)/2-width/2;y=(large?480u:200u)/2-height/2;
    if(!game_menu_widget_position(a,p,x,y,modal?20:0,0))goto failed;
    selector=game_menu_selector_create(a,UINT32_MAX,15,0,large?0x59413c:0x594020);if(!selector)goto failed;
    put(a,0x59116c,selector);
    if(!game_menu_selector_resize(a,selector,large?357:281,large?168:147) ||
       !game_menu_widget_position(a,selector,x+(large?27:10),y+(large?50:24),large?21:1,0))goto failed;
    text=information_text(a,large);if(!text)goto failed;
    height=0;if(!game_menu_text_wrapped_height(a,(unsigned)large,264,text,&height) || !height)height=320;
    {GameMenuTextStyle style={148,156,82,(unsigned)large,large?338:264,height,2,2,0};
     GameAddress images[4]={0};frame=game_menu_text_image_create_bytes(a,text,&style);free(text);text=NULL;
     if(!frame)goto failed;
     images[0]=frame;row=game_menu_button_borrowed_create(a,UINT32_MAX,images,0,0,0,0,0);if(!row)goto failed;
     put(a,row+0x68,0);frame=0; /*The row owns its supplied text frame. */
     if(!game_menu_selector_append(a,selector,row,0,0)){
        if(get(a,selector+0x5c)==row)row=0;
        goto failed;}
     row=0;}
    p=game_menu_button_create(a,5000,back[large],UINT32_MAX,UINT32_MAX,0,0x4c80f0);if(!p)goto failed;
    put(a,0x591170,p);put(a,p+0x50,UINT32_MAX);put(a,p+0x54,121);
    if(!game_menu_widget_position(a,p,x+(large?313:230),y+(large?20:3),large?21:1,0))goto failed;
    if(modal){GameAddress parent=get(a,0x59d4c0),controller;
        if(!parent || !(controller=game_menu_controller_construct(a)))goto failed;
        put(a,0x591178,parent);put(a,0x59d4c0,controller);
        for(i=0;i<4;++i)if(!game_menu_widget_visible(a,get(a,slots[i]),1))goto failed;}
    return !a->memory.fault;
failed:
    free(text);if(row)(void)game_menu_widget_destroy(a,row);if(frame)(void)game_menu_widget_destroy(a,frame);
    (void)release(a);restore_controller(a);return 0;
}
int game_menu_ingame_information_back(GameApplication *a){
    int modal=get(a,0x591178)!=0;
    if(!release(a))return 0;
    if(modal){restore_controller(a);put(a,0x5910a4,0);return !a->memory.fault;}
    if(!game_menu_ingame_pause_state(a,1))return 0;
    put(a,0x5910a4,1);return !a->memory.fault;
}
