#include "campaign_world.h"
#include "menu_widgets.h"
#include "menu_sound.h"
#include "portable_crt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
extern GameAddress game_menu_controller_construct(GameApplication *);
extern void game_menu_controller_destroy(GameApplication *,GameAddress);
extern int game_level_geometry_prepare(GameApplication *);
extern int game_level_geometry_finalize(GameApplication *);
extern uint32_t game_menu_milliseconds(GameApplication *);
extern int game_menu_register_observer(GameApplication *,GameAddress,uint32_t,uint32_t);
extern int game_menu_unregister_tick_method(GameApplication *,GameAddress,uint32_t);
extern int game_campaign_caption_update(GameApplication *,GameAddress);
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
static int64_t signed_word(uint32_t v){return v&0x80000000u?(int64_t)v-INT64_C(4294967296):v;}
static GameAddress selected(GameApplication *a){GameAddress root=get(a,0x585408);return root?game_campaign_mission(a,get(a,root+0x1c),get(a,root+0x20)):0;}
static int text(GameApplication *a,GameAddress p,char *out,size_t capacity){size_t i;
    for(i=0;i<capacity;++i){out[i]=(char)game_memory_read_u8(&a->memory,p+(uint32_t)i);if(!out[i])return !a->memory.fault;}return 0;}
static int position(GameApplication *a,GameAddress p,uint32_t x,uint32_t y,uint32_t z){return p && game_menu_widget_position(a,p,x,y,z,0);}
static int publish_credits(GameApplication *a){char value[64];int32_t credits=(int32_t)signed_word(get(a,0x5e1448));
    if(credits>=1000000)snprintf(value,sizeof(value),"%d,%03d,%03d",credits/1000000,(credits%1000000)/1000,credits%1000);
    else if(credits>1000)snprintf(value,sizeof(value),"%d,%03d",credits/1000,credits%1000);else snprintf(value,sizeof(value),"%d",credits);
    return game_menu_text_image_set_bytes(a,get(a,0x59c6b4),value,1);}
static int retire(GameApplication *a,GameAddress slot){GameAddress p=get(a,slot);if(p && !game_menu_widget_destroy(a,p))return 0;put(a,slot,0);return !a->memory.fault;}
/*530db0: query the real selected LFL without modifying its live settings.
 * The original consumes one RNG draw even when all technology entries fail. */
static int probe_details(GameApplication *a){
    static const char *keys[]={"IMP_SOLDIER_TL","IMP_TANK_TL","IMP_AAV_TL","IMP_BOMBER_TL","RBL_SOLDIER_TL","RBL_TANK_TL","RBL_AAV_TL","RBL_BOMBER_TL"};
    static const char *units[]={"soldiers","tanks","AAVS","bombers"};
    GameAddress planet=selected(a),background=get(a,0x59c6f0),p;char name[128],path[256],value[128],information[512]={0},row[128];unsigned i;uint32_t x,y;
    GameMenuTextStyle title={0,255,0,5,UINT32_MAX,UINT32_MAX,0,0,0},body={0,255,0,1,272,120,0,0,0};
    if(!planet || !background || !text(a,planet+0xb0,name,sizeof(name)) || snprintf(path,sizeof(path),"grids\\%s",name)>=(int)sizeof(path))return 0;
    x=get(a,background+0x20);y=get(a,background+0x24);
    for(i=0;i<8;++i)if(game_campaign_level_value(a,path,keys[i],value,sizeof(value))){long level=strtol(value,NULL,10);size_t used=strlen(information),count;
        if(level<INT32_MIN || level>=INT32_MAX)return 0;
        snprintf(row,sizeof(row),"%s has level %d %s\n",i<4?"Imperium":"Native",(int)level+1,units[i%4]);count=strlen(row);
        if(count>=sizeof(information)-used)return 0;memcpy(information+used,row,count+1);}
    if(!information[0])strcpy(information,"UNSUCCESSFUL");
    if(portable_crt_rand_step(&a->memory)%40==10)strcpy(information,"Probe destroyed by the Imperium");
    if(!retire(a,0x59c71c) || !retire(a,0x59c720) || !retire(a,0x59c724))return 0;
    p=game_menu_text_image_create_bytes(a,"TECHNOLOGY PROBE",&title);if(!p)return 0;put(a,0x59c71c,p);
    if(!position(a,p,x+194-(get(a,p+0x28)-get(a,p+0x20))/2,y+28,2001) || !game_menu_widget_visible(a,p,1))return 0;
    p=game_menu_text_image_create_bytes(a,information,&body);if(!p)return 0;put(a,0x59c720,p);
    return position(a,p,x+61,y+70,2001) && game_menu_widget_visible(a,p,1);
}
/*5312b0: probe and spy publish into their existing owned text/image surfaces. */
static int details(GameApplication *a){GameAddress planet,p,background;char title[128],bonus[1024],value[1536];uint32_t x,y;
    if(get(a,0x59c6d0)==6)return probe_details(a);
    planet=selected(a);p=get(a,0x59c6fc);background=get(a,0x59c6f0);
    if(!planet || !p || !background || !text(a,planet+0x30,title,sizeof(title)) || !text(a,planet+0x154,bonus,sizeof(bonus)))return 0;
    snprintf(value,sizeof(value),"\nSPY:\n\n\"%s\"\n%s",title,bonus);x=get(a,background+0x20);y=get(a,background+0x24);
    return position(a,p,x+26,y+30,101) && game_menu_text_surface_fill(a,p,0) &&
        game_menu_text_surface_paint(a,p,1,value,0x0200ff00,0,0,0,0) && game_menu_widget_visible(a,p,1);
}
static int button(GameApplication *a,GameAddress slot,GameAddress normal,GameAddress hover,GameAddress pressed,uint32_t callback,uint32_t argument,uint32_t x,uint32_t y,int cancel){
    GameAddress names[4]={normal,hover,pressed,0},p=game_menu_button_create(a,UINT32_MAX,names,UINT32_MAX,UINT32_MAX,argument,callback);if(!p)return 0;
    put(a,slot,p);put(a,p+0x50,144);put(a,p+0x54,cancel?146:UINT32_MAX);return position(a,p,x,y,101);
}
int game_campaign_purchase_toggle(GameApplication *a,uint32_t phase){
    GameAddress planet=selected(a),background,parent,controller,p;uint32_t mode=get(a,0x59c6d0),x,y,i;int purchased,probe=mode==6;char title[128],cost[32],message[256];int32_t price;
    if(phase>4 || phase==3)return !a->memory.fault;
    if(phase==4){if(!game_level_geometry_finalize(a))return 0;
        for(i=0;i<14;++i)if(!retire(a,0x59c6f0+i*4))return 0;put(a,0x59c6d0,5);parent=get(a,0x59c728);
        if(parent){controller=get(a,0x59d4c0);if(controller)game_menu_controller_destroy(a,controller);put(a,0x59d4c0,parent);put(a,0x59c728,0);}
        put(a,0x58fd3c,1);return !a->memory.fault;}
    if(!planet || (mode!=6 && mode!=7))return 0;purchased=get(a,planet+(probe?0x568:0x558)) && get(a,planet+(probe?0x56c:0x554));
    if(phase==2){for(i=0;i<2;++i){p=get(a,0x59c6f4+i*4);if(p && !game_menu_widget_enable(a,p,0))return 0;}return !a->memory.fault;}
    if(phase==1){for(i=0;i<4;++i){p=get(a,0x59c6f0+i*4);if(p && !game_menu_widget_visible(a,p,1))return 0;}
        return (purchased || game_menu_widget_enable(a,get(a,0x59c6f4),1)) && game_menu_widget_enable(a,get(a,0x59c6f8),1);}
    if(get(a,0x59c728) || !game_level_geometry_prepare(a))return 0;parent=get(a,0x59d4c0);if(!parent)goto failed;
    controller=game_menu_controller_construct(a);if(!controller)goto failed;put(a,0x59c728,parent);put(a,0x59d4c0,controller);
    background=game_menu_image_create(a,UINT32_MAX,0x59ca70);if(!background)goto failed;put(a,0x59c6f0,background);
    x=320-(get(a,background+0x28)-get(a,background+0x20))/2;y=275-(get(a,background+0x2c)-get(a,background+0x24))/2;
    if(!position(a,background,x,y,100))goto failed;
    if(purchased){
        if(!button(a,0x59c6f4,probe?0x59cad8:0x59cba8,probe?0x59cab4:0x59cb88,probe?0x59ca94:0x59cb68,0x531460,0,x+25,y+193,0) ||
           !button(a,0x59c6f8,probe?0x59cb44:0x59cc10,probe?0x59cb20:0x59cbec,probe?0x59cafc:0x59cbc8,0x531650,UINT32_MAX,x+197,y+193,1))goto failed;
        if(!probe){p=game_menu_image_surface_create(a,340,158,0);if(!p)goto failed;put(a,0x59c6fc,p);if(!position(a,p,x+26,y+30,101))goto failed;}
        if(!details(a))goto failed;
    }else{
        p=game_menu_image_surface_create(a,340,158,0);if(!p)goto failed;put(a,0x59c6fc,p);
        if(!position(a,p,x+26,y+30,101) || !text(a,planet+0x30,title,sizeof(title)))goto failed;
        price=(int32_t)signed_word(get(a,planet+(probe?0x570:0x560)));
        if(price>=1000)snprintf(cost,sizeof(cost),"%d,%03d",price/1000,price%1000);else snprintf(cost,sizeof(cost),"%d",price);
        snprintf(message,sizeof(message),probe?(price>=1000?"\nTECHNOLOGY PROBE\n\n\"%s\"\n\nCOST = %s":"\nTECHNOLOGY PROBE\n\n\"%s\"\nCOST = %s"):"\nSPY\n\n\"%s\"\n\nCOST = %s",title,cost);
        if(!game_menu_text_surface_fill(a,p,0) || !game_menu_text_surface_paint(a,p,4,message,0x0200ff00,0,0,1,0) ||
           !button(a,0x59c6f4,probe?0x59ccf8:0x59cd5c,probe?0x59ccd4:0x59cd3c,probe?0x59ccb4:0x59cd1c,0x531460,0,x+25,y+193,0) ||
           !button(a,0x59c6f8,0x59cdc4,0x59cda0,0x59cd7c,0x531650,UINT32_MAX,x+197,y+193,1))goto failed;
        p=game_menu_image_create(a,UINT32_MAX,probe?0x59cde8:0x59ce14);if(!p)goto failed;put(a,0x59c700,p);if(!position(a,p,x+25,y+70,101))goto failed;
    }
    put(a,0x58fd3c,1);return !a->memory.fault;
failed:(void)game_campaign_purchase_toggle(a,4);return 0;
}
/*5313a0: the real three-second feedback deadline and seventy-ms blink. */
int game_campaign_purchase_tick(GameApplication *a){uint32_t now=game_menu_milliseconds(a);GameAddress flash=get(a,0x59c700);put(a,0x628b3c,now);
    if(signed_word(now)>signed_word(get(a,0x628b38))){if(get(a,0x59c6d4) && flash && !game_menu_widget_visible(a,flash,0))return 0;
        if(!game_menu_unregister_tick_method(a,get(a,0x59d4c0),0x5313a0) || !details(a))return 0;put(a,0x59c6a4,0);}
    if(signed_word(get(a,0x59c72c)+70)<=signed_word(now)){uint32_t visible=!get(a,0x59c6d4);put(a,0x59c72c,now);
        if(!flash || !game_menu_widget_visible(a,flash,(int)visible))return 0;put(a,0x59c6d4,visible);put(a,0x58fd3c,1);}return !a->memory.fault;
}
int game_campaign_purchase_action(GameApplication *a,uint32_t method){GameAddress planet=selected(a),p;uint32_t mode=get(a,0x59c6d0),price,balance,flag,now;
    if(method==0x531650){if(get(a,0x59c6a4))return !a->memory.fault;
        if(!game_campaign_purchase_toggle(a,4))return 0;put(a,0x5910a4,0);return game_campaign_caption_update(a,get(a,0x59c6cc));}
    if(!planet)return 0;
    if(method==0x52fe60 || method==0x52ff50){int probe=method==0x52fe60;uint32_t available=get(a,planet+(probe?0x568:0x558)),active=get(a,planet+(probe?0x56c:0x554));
        if(!available || (!active && get(a,planet+0x2c)))return game_menu_sound(a,146);
        if(probe && !game_menu_sound(a,145))return 0;put(a,0x59c6d0,probe?6:7);
        if(!game_campaign_purchase_toggle(a,0) || !game_campaign_purchase_toggle(a,1))return 0;return probe?!a->memory.fault:game_menu_sound(a,145);}
    if(method!=0x531460 || (mode!=6 && mode!=7))return 0;
    price=get(a,planet+(mode==6?0x570:0x560));balance=get(a,0x5e1448);flag=mode==6?0x56c:0x554;
    if(signed_word(balance)<signed_word(price))return game_menu_sound(a,146);put(a,0x5e1448,balance-price);put(a,planet+flag,1);
    if(!game_menu_widget_visible(a,get(a,0x59c6fc),0) || !game_menu_register_observer(a,get(a,0x59d4c0),0x5313a0,0))return 0;
    now=game_menu_milliseconds(a);put(a,0x628b3c,now);put(a,0x628b38,now+3000);put(a,0x59c6d4,0);put(a,0x58fd3c,1);
    p=get(a,0x59c6f4);if(!game_menu_widget_enable(a,p,0))return 0;
    if(signed_word(get(a,p+0x20))>0 && !position(a,p,UINT32_MAX-999,0,0))return 0;put(a,p+0x44,3);put(a,p+0x18,1);
    if(!publish_credits(a) || !game_menu_sound(a,145))return 0;put(a,0x59c6a4,1);return !a->memory.fault;
}
