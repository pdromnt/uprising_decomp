#include "menu_music.h"
#include "sound_platform.h"
#include "menu_widgets.h"
#include <string.h>
extern int game_menu_ingame_4c3110(GameApplication *,uint32_t);
extern int game_menu_register_tick(GameApplication *,GameAddress,GameAddress,uint32_t,uint32_t);
extern int game_menu_unregister_tick_method(GameApplication *,GameAddress,uint32_t);
extern int game_menu_music_label_tick(GameApplication *);
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
static const GameAddress music_slots[]={0x591128,0x59112c,0x591134,0x591130,0x591138,0x59113c,0x591140};
static const GameAddress sound_slots[]={0x591144,0x591148,0x59114c,0x591150};
static int controls(GameApplication *a,const GameAddress *slots,unsigned count,int enabled){
    unsigned i;GameAddress p;
    for(i=1;i<count;++i){p=get(a,slots[i]);if(p && !game_menu_widget_enable(a,p,enabled))return 0;}
    return !a->memory.fault;
}
static int draw(GameApplication *a,const GameAddress *slots,unsigned count){
    unsigned i;GameAddress p;int large=get(a,0x597f80)!=0;GameUiRect clip={0,0,large?639:319,large?479:199};
    for(i=0;i<count;++i){GameUiRect r;p=get(a,slots[i]);if(!p)continue;
        r.left=(int32_t)get(a,p+0x20);r.top=(int32_t)get(a,p+0x24);
        r.right=(int32_t)get(a,p+0x28);r.bottom=(int32_t)get(a,p+0x2c);
        if(r.left<0)r.left=0;
        if(r.top<0)r.top=0;
        if(r.right>clip.right)r.right=clip.right;
        if(r.bottom>clip.bottom)r.bottom=clip.bottom;
        if(r.left<r.right && r.top<r.bottom && !game_menu_widget_draw(a,p,&r))return 0;
    }
    return !a->memory.fault;
}
static int retire_slot(GameApplication *a,GameAddress slot){
    GameAddress p=get(a,slot);if(p && !game_menu_widget_destroy(a,p))return 0;put(a,slot,0);return !a->memory.fault;
}
static int background(GameApplication *a,GameAddress slot,GameAddress name,uint32_t *x,uint32_t *y){
    GameAddress p=game_menu_image_create(a,UINT32_MAX,name);uint32_t width,height;
    if(!p)return 0;
    put(a,slot,p);width=get(a,p+0x28)-get(a,p+0x20);height=get(a,p+0x2c)-get(a,p+0x24);
    *x=(get(a,0x597f80)?640u:320u)/2-width/2;*y=(get(a,0x597f80)?480u:200u)/2-height/2;
    return game_menu_widget_position(a,p,*x,*y,0,0);
}
static int transparent(GameApplication *a,GameAddress p){
    static const unsigned slots[]={0x58,0x5c,0x60,0x8c};unsigned i;
    for(i=0;i<4;++i){GameAddress image=get(a,p+slots[i]);if(image && !game_menu_image_mode(a,image,101))return 0;}
    return !a->memory.fault;
}
static int music_release(GameApplication *a){
    GameAddress e=get(a,0x59d4c4);unsigned i;
    /*The background is the owning page's lifetime marker, retired last. Other
     *pages can own event+ec; an unopened CD page must not destroy their label. */
    if(!get(a,music_slots[0]))return !a->memory.fault;
    if(!game_menu_unregister_tick_method(a,get(a,0x59d4c0),0x52c940))return 0;
    game_music_worker_join(a);
    for(i=1;i<7;++i)if(!retire_slot(a,music_slots[i]))return 0;
    return retire_slot(a,e+0xec) && retire_slot(a,music_slots[0]);
}
/*4c4170: ordinary CD page; the existing music record owns playlist/device state.
 *This page owns radio controls, navigation and its shared event-slot title. */
int game_menu_ingame_4c4170(GameApplication *a,uint32_t phase){
    static const GameAddress names[2][6][4]={
        {{0x5933f4,0x5933cc,0x5933a8,0},{0x593470,0x593444,0x59341c,0},
         {0x5934f0,0x5934c4,0x59349c,0},{0x593568,0x593540,0x593518,0},
         {0x593598,0,0,0},{0x5935c0,0,0,0}},
        {{0x593668,0x593640,0x59361c,0},{0x5936e4,0x5936b8,0x593690,0},
         {0x593764,0x593738,0x593710,0},{0x5937dc,0x5937b4,0x59378c,0},
         {0x59385c,0x593834,0x59380c,0},{0x5938dc,0x5938b0,0x593884,0}}};
    static const uint32_t callbacks[]={0x52c680,0x52c720,0x52c810};
    static const unsigned small_y[]={20,43,66,114,93,103},large_y[]={40,77,114,184,146,165};
    unsigned i,large=get(a,0x597f80)!=0;GameAddress p,e=get(a,0x59d4c4),music=get(a,0x59d828);uint32_t x,y;
    if(phase==4)return music_release(a);
    if(phase==1 || phase==2)return controls(a,music_slots,7,phase==1);
    if(phase==3){GameAddress order[]={0x591128,0x59112c,0x591134,0x591130,0x591138,e+0xec,0x59113c,0x591140};return draw(a,order,8);}
    if(phase)return !a->memory.fault;
    if(!e || !music || get(a,e+0xec))return 0;
    for(i=0;i<7;++i)if(get(a,music_slots[i]))return 0;
    if(!background(a,music_slots[0],large?0x5935ec:0x593378,&x,&y))goto failed;
    for(i=0;i<6;++i){
        if(i<3)p=game_menu_toggle_create(a,240+i,40,names[large][i],callbacks[i],0,0);
        else p=game_menu_button_create(a,UINT32_MAX,names[large][i],UINT32_MAX,UINT32_MAX,i==4?1:i==5?UINT32_MAX:0,i==3?0x4c4dd0:0x52c8b0);
        if(!p)goto failed;
        put(a,music_slots[i+1],p);put(a,p+0x50,UINT32_MAX);put(a,p+0x54,121);
        if(i>=4 && !transparent(a,p))goto failed;
        if(!large && i==5)put(a,get(a,p+0x58)+0x13c,254);
        if(!game_menu_widget_position(a,p,x+(i>=4?(large?143:84):(large?35:21)),y+(large?large_y[i]:small_y[i]),0,0))goto failed;
    }
    {GameMenuTextStyle style={148,156,82,large?1:0,large?31:24,large?20:16,0,0,0};
     p=game_menu_text_image_create(a,large?0x593804:0x593590,&style);if(!p)goto failed;
     put(a,e+0xec,p);
     if(!game_menu_text_image_set(a,p,large?0x593808:0x593594,1) ||
        !game_menu_widget_position(a,p,x+(large?160:100),y+(large?152:93),0,0))goto failed;}
    i=!get(a,music+0xc) && !get(a,music+0x10)?3:get(a,music+0x10)?2:1;
    if(!game_music_worker_start(a) || !game_menu_toggle_state(a,get(a,music_slots[i]),2) ||
       !game_menu_register_tick(a,get(a,0x59d4c0),get(a,e+0xec),0x52c940,UINT32_MAX) || !game_menu_music_label_tick(a))goto failed;
    return !a->memory.fault;
failed:(void)music_release(a);return 0;
}
static int sound_release(GameApplication *a){
    unsigned i;if(!get(a,sound_slots[0]))return !a->memory.fault;
    if(!game_menu_slider_cancel(a))return 0;
    for(i=1;i<4;++i)if(!retire_slot(a,sound_slots[i]))return 0;
    return retire_slot(a,sound_slots[0]);
}
/*4c4e00: mixer sliders borrow native volume reads; their float and drag state
 *remain in the existing owner. Actual change/sample callbacks are shared. */
int game_menu_ingame_4c4e00(GameApplication *a,uint32_t phase){
    static const GameAddress back[2][4]={{0x5939d8,0x5939b0,0x593988,0},{0x593ad0,0x593aa8,0x593a80,0}};
    unsigned i,large=get(a,0x597f80)!=0;GameAddress p;uint32_t x,y,bits;float scale;
    if(phase==4)return sound_release(a);
    if(phase==1 || phase==2)return controls(a,sound_slots,4,phase==1);
    if(phase==3)return draw(a,sound_slots,4);
    if(phase)return !a->memory.fault;
    for(i=0;i<4;++i)if(get(a,sound_slots[i]))return 0;
    if(!background(a,sound_slots[0],large?0x593a00:0x593908,&x,&y))goto failed;
    bits=get(a,0x574924);memcpy(&scale,&bits,4);
    for(i=0;i<2;++i){uint32_t volume=i?sound_platform_master_volume_read():game_music_volume_read();
        float value=(float)((long double)(volume&65535u)*scale);
        p=game_menu_slider_create(a,UINT32_MAX,i?(large?0x593a58:0x593960):(large?0x593a30:0x593938),
            large?150:102,large?25:20,i?0x52ca10:0x52ca60,6,value,1,i?0:0x52cab0,i?0:0x52cae0);
        if(!p)goto failed;
        put(a,i?0x591148:0x59114c,p);
        if(!game_menu_widget_position(a,p,x+(large?46:23),y+(large?(i?128:57):(i?81:33)),0,0))goto failed;
    }
    p=game_menu_button_create(a,UINT32_MAX,back[large],UINT32_MAX,UINT32_MAX,0,0x4c5490);if(!p)goto failed;
    put(a,0x591150,p);put(a,p+0x50,UINT32_MAX);put(a,p+0x54,121);
    if(!game_menu_widget_position(a,p,x+(large?35:21),y+(large?184:114),0,0))goto failed;
    return !a->memory.fault;
failed:(void)sound_release(a);return 0;
}
int game_menu_ingame_audio_back(GameApplication *a,uint32_t method){
    if(method==0x4c4dd0){if(!music_release(a))return 0;}
    else if(method==0x4c5490){if(!sound_release(a))return 0;}
    else return 0;
    if(!game_menu_ingame_4c3110(a,0) || !game_menu_ingame_4c3110(a,1))return 0;
    put(a,0x5910a4,4);return !a->memory.fault;
}
