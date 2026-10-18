#include "menu_widgets.h"
#include "gameplay_input.h"

extern int game_menu_ingame_pause_state(GameApplication *,uint32_t);
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
typedef struct PlayerPageAssets {
    GameAddress background,back[4],label,absent,check[4];
} PlayerPageAssets;
static const PlayerPageAssets assets[2][2]={
    {{0x593af8,{0x593b78,0x593b4c,0x593b24,0},0x593ba4,0x593bac,{0x593c0c,0x593be0,0x593bb4,0}},
     {0x593c38,{0x593cb8,0x593c8c,0x593c64,0},0x593ce4,0x593cec,{0x593d4c,0x593d20,0x593cf4,0}}},
    {{0x593d78,{0x593df8,0x593dcc,0x593da4,0},0x593e24,0x593e2c,{0x593e8c,0x593e60,0x593e34,0}},
     {0x593eb8,{0x593f38,0x593f0c,0x593ee4,0},0x593f64,0x593f6c,{0x593fcc,0x593fa0,0x593f74,0}}}
};
static int retire(GameApplication *a,GameAddress slot){
    GameAddress p=get(a,slot);if(p && !game_menu_widget_destroy(a,p))return 0;
    put(a,slot,0);return !a->memory.fault;
}
/*Both pages reuse the original four-row table. The background identifies its
 *owner; closing an unopened sibling must not retire the live page's rows. */
static int release(GameApplication *a,unsigned communication){
    GameAddress bg=communication?0x59115c:0x591154;unsigned i;
    if(!get(a,bg))return !a->memory.fault;
    for(i=0;i<4;++i)if(!retire(a,0x6041d0+i*4) || !retire(a,0x604240+i*4))return 0;
    return retire(a,bg+4) && retire(a,bg);
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
/*4c54c0/4c5cb0: alliance and communication rows, actual player names, live
 *checkbox consumers and teardown. Existing players remain the authoritative
 *owners of identities, participation and alliance state. */
static int page(GameApplication *a,unsigned communication,uint32_t phase){
    unsigned i,row=0,large=get(a,0x597f80)!=0;uint32_t x,y,width,height;
    GameAddress bg=communication?0x59115c:0x591154,p,local=get(a,0x5f0208);
    const PlayerPageAssets *names=&assets[communication][large];
    if(phase==4)return release(a,communication);
    if(phase==1 || phase==2){
        p=get(a,bg+4);if(p && !game_menu_widget_enable(a,p,phase==1))return 0;
        for(i=0;i<4;++i){GameAddress player=get(a,0x5f0210+i*4);p=get(a,0x6041d0+i*4);
            if(p && (phase==2 || (player && get(a,player+0x468))) && !game_menu_widget_enable(a,p,phase==1))return 0;}
        return !a->memory.fault;
    }
    if(phase==3){
        if(!draw(a,get(a,bg)))return 0;
        for(i=0;i<4;++i)if(!draw(a,get(a,0x6041d0+i*4)) || !draw(a,get(a,0x604240+i*4)))return 0;
        return draw(a,get(a,bg+4)) && !a->memory.fault;
    }
    if(phase)return !a->memory.fault;
    if(!local || get(a,bg) || get(a,bg+4) || get(a,communication?0x591154:0x59115c))return 0;
    for(i=0;i<4;++i)if(get(a,0x6041d0+i*4) || get(a,0x604240+i*4))return 0;
    p=game_menu_image_create(a,UINT32_MAX,names->background);if(!p)return 0;
    put(a,bg,p);width=get(a,p+0x28)-get(a,p+0x20);height=get(a,p+0x2c)-get(a,p+0x24);
    x=(large?640u:320u)/2-width/2;y=(large?480u:200u)/2-height/2;
    if(!game_menu_widget_position(a,p,x,y,0,0))goto failed;
    p=game_menu_button_create(a,5000,names->back,UINT32_MAX,UINT32_MAX,0,communication?0x4c80a0:0x4c8030);
    if(!p)goto failed;
    put(a,bg+4,p);
    if(!game_menu_widget_position(a,p,x+(large?57:36),y+(large?96:71),1,0))goto failed;
    for(i=0;i<4;++i){GameAddress player=get(a,0x5f0210+i*4);int selected=0;unsigned j;
        GameMenuTextStyle style={0,255,0,large?1:0,large?148:87,12,0,0,0};
        if(player==local)continue;
        if(!player)goto failed;
        p=game_menu_text_image_create(a,names->label,&style);if(!p)goto failed;
        put(a,0x604240+i*4,p);
        if(!game_menu_widget_position(a,p,x+(large?59:37),y+(large?33:22)+row*(large?22:17),10,0) ||
           !game_menu_widget_visible(a,p,1) || !game_menu_text_image_set(a,p,
              get(a,player+(communication?0x468:0x14))?player+0x39c:names->absent,0))goto failed;
        p=game_menu_checkbox_create(a,names->check,communication?0x4c80d0:0x4c8060);if(!p)goto failed;
        put(a,0x6041d0+i*4,p);put(a,p+4,240+row);put(a,p+0x64,i);
        if(!game_menu_widget_position(a,p,x+(large?39:23),y+(large?29:16)+row*(large?22:16),10,0) ||
           !game_menu_widget_visible(a,p,1) || !game_menu_widget_enable(a,p,0))goto failed;
        for(j=0;j<3;++j){GameAddress image=get(a,p+0x58+j*4);if(image && !game_menu_image_mode(a,image,101))goto failed;}
        if(get(a,player+0x468)){
            if(communication)selected=get(a,0x5f0040+i*4)!=0;
            else{unsigned first=game_memory_read_u16(&a->memory,local+8)&63u,second=game_memory_read_u16(&a->memory,player+8)&63u;
                if(first>=6 || second>=6)goto failed;
                selected=get(a,0x5f0160+(first*6+second)*4)!=0;}
        }
        if(!game_menu_widget_base_state(a,p,selected?2:0))goto failed;
        put(a,p+0x94,(uint32_t)selected);++row;
    }
    return !a->memory.fault;
failed:(void)release(a,communication);return 0;
}
int game_menu_ingame_4c54c0(GameApplication *a,uint32_t phase){return page(a,0,phase);}
int game_menu_ingame_4c5cb0(GameApplication *a,uint32_t phase){return page(a,1,phase);}
int game_menu_ingame_players_action(GameApplication *a,uint32_t method,GameAddress receiver,uint32_t index){
    if(method==0x4c8030 || method==0x4c80a0){
        if(!release(a,method==0x4c80a0) || !game_menu_ingame_pause_state(a,1))return 0;
        put(a,0x5910a4,1);return !a->memory.fault;
    }
    if(index>=4 || !receiver)return 0;
    if(method==0x4c80d0){put(a,0x5f0040+index*4,get(a,receiver+0x94));return !a->memory.fault;}
    if(method==0x4c8060){GameRootEventServices services=game_input_event_services(a);
        return game_root_count_publish(&a->memory,&services,get(a,0x5f0208),get(a,receiver+0x94)?0x33:0x34,(uint8_t)(1u<<index));}
    return 0;
}
