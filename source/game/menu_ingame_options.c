#include "menu_widgets.h"
#include "menu_pause.h"
extern int game_menu_ingame_4c3950(GameApplication *,uint32_t);
extern int game_menu_ingame_4c4170(GameApplication *,uint32_t);
extern int game_menu_ingame_4c4e00(GameApplication *,uint32_t);
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
static const GameAddress slots[]={0x591100,0x591104,0x591108,0x59110c,0x591110};
static int release(GameApplication *a){
    unsigned i;GameAddress p;
    for(i=0;i<5;++i){p=get(a,slots[i]);if(p && !game_menu_widget_destroy(a,p))return 0;put(a,slots[i],0);}
    return !a->memory.fault;
}
/*4c3110: the shared options menu owns its image and four callback buttons.
 *Small images form a vertical list; large images retain the original column.
 *Child option pages own their distinct controls and stay mandatory native links. */
int game_menu_ingame_4c3110(GameApplication *a,uint32_t phase){
    static const GameAddress names[2][4][4]={
        {{0x592bec,0x592bc0,0x592b94,0},{0x592c6c,0x592c40,0x592c18,0},
         {0x592cf4,0x592cc4,0x592c98,0},{0x592d78,0x592d4c,0x592d24,0}},
        {{0x592e30,0x592e04,0x592dd8,0},{0x592eb0,0x592e84,0x592e5c,0},
         {0x592f38,0x592f08,0x592edc,0},{0x592fbc,0x592f90,0x592f68,0}}};
    static const uint32_t callbacks[]={0x4c3890,0x4c38c0,0x4c38f0,0x4c3920};
    unsigned i,large=get(a,0x597f80)!=0;GameAddress p;uint32_t x,y,width,height;
    if(phase==4)return release(a);
    if(phase==1 || phase==2){
        for(i=1;i<5;++i){p=get(a,slots[i]);if(p && !game_menu_widget_enable(a,p,phase==1))return 0;}
        return !a->memory.fault;
    }
    if(phase==3){GameUiRect clip={0,0,large?639:319,large?479:199};
        for(i=0;i<5;++i){GameUiRect r;p=get(a,slots[i]);if(!p)continue;
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
    p=game_menu_image_create(a,UINT32_MAX,large?0x592da4:0x592b68);if(!p)goto failed;
    put(a,slots[0],p);width=get(a,p+0x28)-get(a,p+0x20);height=get(a,p+0x2c)-get(a,p+0x24);
    x=(large?640u:320u)/2-width/2;y=(large?480u:200u)/2-height/2;
    if(!game_menu_widget_position(a,p,x,y,0,0))goto failed;
    for(i=0;i<4;++i){
        p=game_menu_button_create(a,5000,names[large][i],UINT32_MAX,UINT32_MAX,UINT32_MAX,callbacks[i]);if(!p)goto failed;
        put(a,slots[i+1],p);
        if(!game_menu_widget_position(a,p,x+(large?35:22),y+(large?40+i*37:(i+1)*19),1,0))goto failed;
        put(a,p+0x50,UINT32_MAX);put(a,p+0x54,121);
    }
    return !a->memory.fault;
failed:(void)release(a);return 0;
}
/*4c3890/38c0/38f0/3920: retire the owning parent, construct/enable the real
 *child page or reveal the existing pause controls. No synthetic menu state. */
int game_menu_ingame_options_action(GameApplication *a,uint32_t method){
    uint32_t state;int ok;
    if(method!=0x4c3890 && method!=0x4c38c0 && method!=0x4c38f0 && method!=0x4c3920)return 0;
    if(!release(a))return 0;
    switch(method){
    case 0x4c3890:state=9;ok=game_menu_ingame_4c4e00(a,0) && game_menu_ingame_4c4e00(a,1);break;
    case 0x4c38c0:state=10;ok=game_menu_ingame_4c4170(a,0) && game_menu_ingame_4c4170(a,1);break;
    case 0x4c38f0:state=8;ok=game_menu_ingame_4c3950(a,0) && game_menu_ingame_4c3950(a,1);break;
    default:state=1;ok=game_menu_ingame_pause_state(a,1);break;
    }
    if(!ok)return 0;
    put(a,0x5910a4,state);return !a->memory.fault;
}
