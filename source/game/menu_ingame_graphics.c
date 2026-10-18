#include "menu_widgets.h"
#include "menu_pause.h"
#include "menu_settings.h"
extern int game_menu_text_cache_initialize(GameApplication *);
extern int game_menu_text_cache_release(GameApplication *);
extern int game_menu_ingame_4c3110(GameApplication *,uint32_t);
extern int game_level_display_visibility(GameApplication *,int);
extern int game_level_display_sync(GameApplication *,uint32_t); /*517240: real display refresh. */
int game_menu_ingame_4c3950(GameApplication *,uint32_t);
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
static const GameAddress slots[]={0x591114,0x59111c,0x591118,0x591124,0x591120};
static int assets_release(GameApplication *a){
    uint32_t count=get(a,0x604238);unsigned i;GameAddress p;
    if(count>6)return 0;
    if(!game_menu_text_cache_release(a))return 0;
    for(i=0;i<count;++i){p=get(a,0x6041e8+i*8);
        if(p && !game_menu_widget_destroy(a,p))return 0;
        put(a,0x6041e8+i*8,0);put(a,0x6041ec+i*8,0);}
    put(a,0x604238,0);return !a->memory.fault;
}
/*4c7d70: each eight-byte row owns its first image; its second slot borrows
 *that image. Value controls borrow these backgrounds until selector retirement. */
static int assets_begin(GameApplication *a){
    static const GameAddress names[]={0x594284,0x5942ac,0x5942d4,0x5942fc,0x594324,0x59434c};
    unsigned i;GameAddress p;
    if(get(a,0x604238) || !game_menu_text_cache_initialize(a))return 0;
    for(i=0;i<6;++i){p=game_menu_image_create(a,UINT32_MAX,names[i]);if(!p){(void)assets_release(a);return 0;}
        put(a,0x6041e8+i*8,p);put(a,0x6041ec+i*8,p);put(a,0x604238,i+1);}
    return !a->memory.fault;
}
static int release(GameApplication *a){
    unsigned i;GameAddress p;int owned=get(a,0x591114)!=0 || get(a,0x604238)!=0;
    for(i=0;i<5;++i){p=get(a,slots[i]);if(p && !game_menu_widget_destroy(a,p))return 0;put(a,slots[i],0);}
    return (!owned || assets_release(a)) && !a->memory.fault;
}
/*4c7750: ordinary numeric/boolean controls point directly at existing settings.
 *The selector owns the heterogeneous chain, including each label and surface. */
static int values_build(GameApplication *a,GameAddress selector){
    static const unsigned offsets[]={0x34,0x1c,0x24,0x38,0x28,0x2c};
    static const GameAddress labels[]={0x594248,0x594254,0x594258,0x594260,0x59426c,0x594278};
    static const uint32_t step[]={10,1,1,1,1,1},minimum[]={40,0,0,0,0,0},maximum[]={100,1,1,1,2,1};
    GameAddress settings=get(a,0x5943c8),p=0;unsigned i,count=get(a,0x5eff04)?4:6;
    if(!settings || !game_menu_selector_clear(a,selector))return 0;
    put(a,settings+0x1c,get(a,0x597f84));put(a,settings+0x38,get(a,0x597f88));
    for(i=0;i<count;++i){
        p=game_menu_value_create(a,420+i,settings+offsets[i],labels[i],step[i],minimum[i],maximum[i],0,i==1 || i==2 || i==5);
        if(!p)goto failed;
        put(a,p+0x110,get(a,0x6041e8+i*8));put(a,p+0x114,get(a,0x6041ec+i*8));
        if(!game_menu_widget_enable(a,p,1))goto failed;
        put(a,p+0x58,selector);put(a,p+0x5c,0);put(a,p+0xf8,0x4c7b90);
        if(!game_menu_selector_append(a,selector,p,1,get(a,p+0x28)-get(a,p+0x20))){
            GameAddress cursor=get(a,selector+0x5c);unsigned visits=0;
            /*Append publishes before positioning. A published row belongs
             *to the selector even if a later platform/control step fails. */
            while(cursor && visits++<6){if(cursor==p){p=0;break;}cursor=get(a,cursor+0x4c);}
            goto failed;
        }
        p=0;
    }
    return !a->memory.fault;
failed:if(p)(void)game_menu_widget_destroy(a,p);(void)game_menu_selector_clear(a,selector);return 0;
}
int game_menu_ingame_4c3950(GameApplication *a,uint32_t phase){
    static const GameAddress names[2][3][4]={
        {{0x59306c,0x593040,0x593014,0},{0x5930fc,0x5930cc,0x59309c,0},{0x593184,0x593158,0x59312c,0}},
        {{0x593234,0x593208,0x5931dc,0},{0x5932c4,0x593294,0x593264,0},{0x59334c,0x593320,0x5932f4,0}}};
    unsigned i,large=get(a,0x597f80)!=0;GameAddress p,selector;uint32_t x,y,width,height;
    if(phase==4)return release(a);
    if(phase==1 || phase==2){
        for(i=1;i<5;++i){p=get(a,slots[i]);if(p && !game_menu_widget_enable(a,p,phase==1))return 0;}
        return !a->memory.fault;
    }
    if(phase==3){GameUiRect clip={0,0,large?639:319,large?479:199};
        static const GameAddress draw_slots[]={0x591114,0x591118,0x591124,0x591120,0x59111c};
        for(i=0;i<5;++i){GameUiRect r;p=get(a,draw_slots[i]);if(!p)continue;
            r.left=(int32_t)get(a,p+0x20);r.top=(int32_t)get(a,p+0x24);
            r.right=(int32_t)get(a,p+0x28);r.bottom=(int32_t)get(a,p+0x2c);
            if(r.left<0)r.left=0;
            if(r.top<0)r.top=0;
            if(r.right>clip.right)r.right=clip.right;
            if(r.bottom>clip.bottom)r.bottom=clip.bottom;
            if(r.left<r.right && r.top<r.bottom && !game_menu_widget_draw(a,p,&r))return 0;}
        return !a->memory.fault;
    }
    if(phase)return !a->memory.fault;
    for(i=0;i<5;++i)if(get(a,slots[i]))return 0;
    if(!assets_begin(a))return 0;
    p=game_menu_image_create(a,UINT32_MAX,large?0x5931b0:0x592fe8);if(!p)goto failed;
    put(a,0x591114,p);width=get(a,p+0x28)-get(a,p+0x20);height=get(a,p+0x2c)-get(a,p+0x24);
    x=(large?640u:320u)/2-width/2;y=(large?480u:200u)/2-height/2;
    if(!game_menu_widget_position(a,p,x,y,0,0))goto failed;
    selector=game_menu_edit_create(a,530,0,0);if(!selector)goto failed;put(a,0x591118,selector);
    if(!game_menu_selector_resize(a,selector,large?140:70,70) ||
       !game_menu_widget_position(a,selector,x+(large?51:38),y+(large?33:17),0,0) || !values_build(a,selector))goto failed;
    for(i=0;i<3;++i){
        p=game_menu_button_create(a,5000,names[large][i],i==2?UINT32_MAX:530,i==2?UINT32_MAX:1011,i==0?1:UINT32_MAX,i==2?0x4c7710:0);
        if(!p)goto failed;
        put(a,i==0?0x591120:i==1?0x591124:0x59111c,p);
        if(i<2){static const unsigned images[]={0x58,0x5c,0x60,0x8c};unsigned j;
            put(a,p+0x84,0);put(a,p+0x88,0);
            for(j=0;j<4;++j){GameAddress image=get(a,p+images[j]);if(image && !game_menu_image_mode(a,image,101))goto failed;}
        }else{put(a,p+0x50,UINT32_MAX);put(a,p+0x54,121);}
        if(!game_menu_widget_position(a,p,x+(large?(i==2?36:i?195:34):(i==2?21:i?110:22)),
             y+(i==2?(large?137:91):(large?33:17)),1,0))goto failed;
    }
    return !a->memory.fault;
failed:(void)release(a);return 0;
}
int game_menu_ingame_graphics_action(GameApplication *a,uint32_t method,GameAddress receiver){
    static const unsigned offsets[]={8,0x10,0x14,0x18,0x20,0x24,0x28,0x2c,0x34};
    static const unsigned keys[]={0,3,4,5,7,8,9,10,1};
    GameAddress settings=get(a,0x5943c8),value;uint32_t offset;unsigned i;
    if(method==0x4c7740)return !a->memory.fault; /*Original returns1. */
    if(method==0x4c7710){
        if(!release(a) || !game_menu_ingame_4c3110(a,0) || !game_menu_ingame_4c3110(a,1))return 0;
        put(a,0x5910a4,4);return !a->memory.fault;
    }
    if(method!=0x4c7b90 || !settings)return 0;
    value=get(a,receiver+0x78);offset=value-settings;
    if(offset==0x38)return game_menu_render_quality(a,get(a,value));
    if(offset==0x1c)return game_menu_option_set(a,6,get(a,value)) && game_level_display_sync(a,get(a,settings+0x1c));
    for(i=0;i<sizeof(offsets)/sizeof(offsets[0]);++i)if(offset==offsets[i]){
        if(offset==0x14)put(a,value,get(a,value)*100u);
        if(!game_menu_option_set(a,keys[i],get(a,value)))return 0;
        if(offset!=8)return !a->memory.fault;
        if(!release(a) || !game_menu_ingame_pause_state(a,4) || !game_level_display_visibility(a,(int)get(a,settings+8)) ||
           !game_menu_ingame_pause_state(a,0) || !game_menu_ingame_4c3950(a,0) || !game_menu_ingame_4c3950(a,1))return 0;
        put(a,0x5910a4,8);return !a->memory.fault;
    }
    return !a->memory.fault;
}
