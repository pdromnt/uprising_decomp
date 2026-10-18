#include "menu_widgets.h"
#include "menu_level_catalog.h"
#include "application_archives.h"
#include "campaign_world.h"
#include "menu_pause.h"
#include <stdio.h>
#include <string.h>
extern GameAddress game_menu_controller_construct(GameApplication *);
extern void game_menu_controller_destroy(GameApplication *,GameAddress);
extern int game_menu_request(GameApplication *,GameAddress,uint32_t,uint32_t,uint32_t);
/*Live level/archive consumers remain strong dependencies: actual world snapshot
 *45de40, scenario serialization45a3a0, map preview4e18b0/16e0/484930/1b50,
 *pause screen4c0b80 and resume4414e0/47f020. No CPU callback or fake success. */
#include "level_save.h"
extern int game_menu_map_file_preview(GameApplication *,const char *);
extern int game_menu_saved_campaign_load(GameApplication *,const char *);
extern int game_menu_saved_snapshot_copy(GameApplication *,const char *);
extern int game_campaign_catalog_release(GameApplication *);
extern int game_level_player_ui_release(GameApplication *);
extern int game_menu_selection_end(GameApplication *);
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
static const GameAddress slots[2][5]={{0x5910d8,0x5910dc,0x5910e0,0x5910e4,0},
    {0x5910ec,0x5910fc,0x5910f4,0x5910f8,0x5910f0}};
static int text(GameApplication *a,GameAddress p,char *out,size_t size){
    size_t i;if(!p)return 0;
    for(i=0;i<size;++i){out[i]=(char)game_memory_read_u8(&a->memory,p+(uint32_t)i);if(!out[i])return !a->memory.fault;}
    return 0;
}
static int place(GameApplication *a,GameAddress p,uint32_t x,uint32_t y,uint32_t z){
    return p && game_menu_widget_position(a,p,x,y,z,0);
}
static int release(GameApplication *a,unsigned family){
    static const unsigned order[2][5]={{0,2,3,1,4},{4,0,2,3,1}};
    unsigned i;GameAddress slot,p,parent_slot=family?0x591180:0x59117c,parent;
    for(i=0;i<5;++i){slot=slots[family][order[family][i]];if(!slot)continue;
        p=get(a,slot);if(p && !game_menu_widget_destroy(a,p))return 0;put(a,slot,0);
    }
    /*The saved parent establishes ownership even during failed construction.
     *Repeated close must never retire the restored borrowed controller. */
    parent=get(a,parent_slot);
    if(parent){p=get(a,0x59d4c0);if(p)game_menu_controller_destroy(a,p);
        put(a,0x59d4c0,parent);put(a,parent_slot,0);put(a,0x59c6a0,0);put(a,0x58fd3c,1);}
    return !a->memory.fault;
}
static int draw(GameApplication *a,unsigned family,int large){
    static const unsigned order[2][5]={{0,1,2,3,4},{0,2,3,4,1}};
    GameUiRect clip={0,0,large?639:319,large?479:199};unsigned i;GameAddress p,slot;
    for(i=0;i<5;++i){GameUiRect r;slot=slots[family][order[family][i]];
        if(!slot || !(p=get(a,slot)))continue;
        r.left=(int32_t)get(a,p+0x20);r.top=(int32_t)get(a,p+0x24);
        r.right=(int32_t)get(a,p+0x28);r.bottom=(int32_t)get(a,p+0x2c);
        if(r.left<clip.left)r.left=clip.left;
        if(r.top<clip.top)r.top=clip.top;
        if(r.right>clip.right)r.right=clip.right;
        if(r.bottom>clip.bottom)r.bottom=clip.bottom;
        if(r.left<r.right && r.top<r.bottom && !game_menu_widget_draw(a,p,&r))return 0;
    }
    return !a->memory.fault;
}
static GameAddress button(GameApplication *a,const GameAddress names[4],uint32_t callback){
    return game_menu_button_create(a,5000,names,UINT32_MAX,UINT32_MAX,UINT32_MAX,callback);
}
/*4c18b0/4c2330: the existing records own background/buttons/edit/selector.
 *The selector alone owns its heterogeneous text rows and three image frames.
 *Names are temporary directory results, never a persistent competing copy. */
int game_menu_strategic_depart(GameApplication *a,uint32_t family,uint32_t phase){
    static const GameAddress backgrounds[2][3]={{0x592368,0x5924e0,0x5924bc},{0x59276c,0x5928f0,0x5928cc}};
    static const GameAddress selector_names[2][3]={{0x592390,0x59252c,0x592508},{0x5927a0,0x592950,0x59292c}};
    static const GameAddress buttons[2][3][2][4]={
        {{{0x59240c,0x5923e0,0x5923b8,0},{0x592490,0x592464,0x592438,0}},
         {{0x59261c,0x5925f0,0x5925c8,0},{0x592718,0x5926ec,0x5926c0,0}},
         {{0x5925a0,0x592578,0x592554,0},{0x592698,0x592670,0x592648,0}}},
        {{{0x59281c,0x5927f0,0x5927c8,0},{0x5928a0,0x592874,0x592848,0}},
         {{0x592a40,0x592a14,0x5929ec,0},{0x592b3c,0x592b10,0x592ae4,0}},
         {{0x5929c4,0x59299c,0x592978,0},{0x592abc,0x592a94,0x592a6c,0}}}};
    GameLevelNames names;GameAddress p,background,selector,controller,parent_slot;
    uint32_t large,modal,theme,width,height,x,y,z=0;unsigned i;char initial[128],*suffix;
    if(family>1)return 0;
    large=get(a,0x597f80)!=0;modal=get(a,0x59c6a0)!=0;
    if(phase==4)return release(a,family);
    if(phase==3)return draw(a,family,large);
    if(phase==1){
        for(i=1;i<4;++i)if(!game_menu_widget_enable(a,get(a,slots[family][i]),1))return 0;
        if(family && !game_menu_widget_enable(a,get(a,slots[1][4]),1))return 0;
        if(modal){for(i=0;i<5;++i)if(slots[family][i] && !game_menu_widget_visible(a,get(a,slots[family][i]),1))return 0;put(a,0x58fd3c,1);}
        return !a->memory.fault;
    }
    if(phase==2){
        /*The original save screen disables the shared load buttons here. */
        if(!game_menu_widget_enable(a,get(a,slots[0][2]),0) || !game_menu_widget_enable(a,get(a,slots[0][3]),0) ||
           !game_menu_widget_enable(a,get(a,slots[family][1]),0))return 0;
        return !family || game_menu_widget_enable(a,get(a,slots[1][4]),0);
    }
    if(phase)return !a->memory.fault;
    for(i=0;i<5;++i)if(slots[family][i] && get(a,slots[family][i]))return 0;
    put(a,0x5910e8,0);
    if(!game_menu_level_files(a,&names,!family && get(a,0x59708c)?"grids\\*.hzs":modal || get(a,0x5910a8)?"saves\\*.sav":"saves\\*.sfl",
        !family && get(a,0x59708c)?".hzs":modal || get(a,0x5910a8)?".sav":".sfl",NULL,0))return 0;
    parent_slot=family?0x591180:0x59117c;
    if(modal){if(get(a,parent_slot))return 0;controller=get(a,0x59d4c0);if(!controller)return 0;
        p=game_menu_controller_construct(a);if(!p)return 0;put(a,parent_slot,controller);put(a,0x59d4c0,p);z=1000;}
    theme=!large?0:modal && get(a,get(a,0x59d4c4)+0x3c)==18?2:1;
    background=game_menu_image_create(a,UINT32_MAX,backgrounds[family][theme]);if(!background)goto failed;
    put(a,slots[family][0],background);width=get(a,background+0x28)-get(a,background+0x20);height=get(a,background+0x2c)-get(a,background+0x24);
    x=(large?640:320)/2-width/2;y=(large?480:200)/2-height/2;
    if(!place(a,background,x,y,z))goto failed;
    selector=game_menu_selector_create(a,UINT32_MAX,15,0,selector_names[family][theme]);if(!selector)goto failed;
    put(a,slots[family][1],selector);
    if(!game_menu_selector_resize(a,selector,large?169:104,large?(family?50:52):32) ||
       !place(a,selector,x+(large?38:22),y+(large?(family?35:34):19),z+1))goto failed;
    if(family){
        if(modal)memcpy(initial,"save",5);else{if(!text(a,0x5efe80,initial,sizeof(initial)))goto failed;
            suffix=strstr(initial,".lfl");if(suffix)*suffix=0;}
        p=game_menu_text_entry_create_bytes(a,UINT32_MAX,large?172:104,12,large?1:0,initial);if(!p)goto failed;
        put(a,slots[1][4],p);
        if(!place(a,p,x+(large?36:22),y+(large?96:57),z+10) || !game_menu_text_entry_enable(a,p,1) || !game_menu_text_entry_activate(a,p))goto failed;
        put(a,p+0x70,0x4c7570);put(a,p+0x1cc,10);
        /*521970 accepts only nine bindings. Preserve the original rejection
         *of the last two calls instead of overflowing the fixed owner. */
        {static const uint8_t keys[]={13,9,'.','\\','/','*','?','"','<'};
         for(i=0;i<9;++i){game_memory_write_u8(&a->memory,p+0x100+i*20,keys[i]);put(a,p+0x104+i*20,i?0:0x4c76f0);}put(a,p+0x1c8,9);}
    }
    for(i=0;i<2;++i){p=button(a,buttons[family][theme][i],family?(i?0x4c76a0:0x4c7590):(i?0x4c7500:0x4c7310));if(!p)goto failed;
        put(a,slots[family][i+2],p);
        if(!place(a,p,x+(large?(family?34:36):22)+(family && i && !large?55:0),
            y+(large?(family?(i?124:128):(i?128:96)):(family?76:(i?76:57))),z+1))goto failed;
        put(a,p+0x50,UINT32_MAX);put(a,p+0x54,i?122:121);
    }
    width=get(a,selector+0x28)-get(a,selector+0x20);if(width<=(large?10u:7u))goto failed;
    for(i=0;i<names.count;++i){p=game_menu_text_button_create(a,names.names[i],width-(large?10:7),12,large?1:0,1,0,
        family?0x4c7550:0x4c7270,theme==2?7:4);if(!p)goto failed;
        if(!game_menu_selector_append(a,selector,p,0,0)){game_menu_widget_destroy(a,p);goto failed;}}
    return !a->memory.fault;
failed:(void)release(a,family);return 0;
}
static int close_screen(GameApplication *a,unsigned family,int cancel){
    int modal=get(a,0x59c6a0)!=0;
    if(!game_menu_strategic_depart(a,family,4))return 0;
    if(!modal && !game_menu_ingame_pause_state(a,1))return 0;
    put(a,0x5910a4,modal?(family && cancel?1:0):1);
    if(family && modal && cancel)put(a,0x5910ac,0);
    return !a->memory.fault;
}
/*531980: ordinary bounded paths and the existing native file service. Retail
 *ignores CopyFileA's result; preserve that behavior without a CPU/SDK adapter. */
int game_menu_saved_snapshot_copy(GameApplication *a,const char *name){
    char path[256];int n;if(!name)return 0;
    n=snprintf(path,sizeof(path),"saves\\%s.sav",name);
    if(n<0 || (size_t)n>=sizeof(path))return 0;
    (void)game_application_file_copy(a,path,"saves\\temp");return !a->memory.fault;
}
/*45ddd0: a short/failed read retains its initialized zero bytes. */
static uint32_t saved_running(GameApplication *a,const char *path){
    uint8_t bytes[4]={0};GameAddress archive=game_application_archive_open(a,path,0);
    if(archive){(void)resource_file_read(game_application_archive_file(a,archive),bytes,sizeof(bytes));
        if(!game_application_archive_close(a,archive))return 0;}
    return (uint32_t)bytes[0]|((uint32_t)bytes[1]<<8)|((uint32_t)bytes[2]<<16)|((uint32_t)bytes[3]<<24);
}
/*530040: import the actual save, read its little-endian state word, retire the
 *load modal and request running-game or strategic reconstruction. The latter
 *also retires the research-owned world/catalog before re-entering state18. */
int game_menu_saved_campaign_load(GameApplication *a,const char *name){
    char path[256];uint32_t running;int n;
    if(!name || !game_menu_saved_snapshot_copy(a,name))return 0;
    n=snprintf(path,sizeof(path),"saves\\%s.sav",name);if(n<0 || (size_t)n>=sizeof(path))return 0;
    running=saved_running(a,path);
    if(running && !game_memory_write(&a->memory,0x5e1248,path,(size_t)n+1))return 0;
    if(!game_menu_strategic_depart(a,0,4))return 0;
    put(a,0x5910a4,0);
    if(running)put(a,0x5910a8,1);
    else if(get(a,get(a,0x59d4c4)+0x3c)==32){
        put(a,0x59c6d8,1);
        if(!game_campaign_destroy(a) || !game_campaign_catalog_release(a) ||
           !game_level_player_ui_release(a) || !game_menu_selection_end(a))return 0;
    }
    return game_menu_request(a,get(a,0x59d4c0),69,1000,running?17:18);
}
int game_menu_strategic_file_action(GameApplication *a,uint32_t method,GameAddress receiver){
    char name[128],path[160];GameAddress selected,edit;int n;unsigned family;
    if(method==0x4c7270){selected=receiver+0x98;put(a,0x5910e8,selected);
        if(get(a,0x59708c)){if(!text(a,selected,name,sizeof(name)))return 0;
            n=snprintf(path,sizeof(path),"%s.hzs",name);if(n<0 || (size_t)n>=sizeof(path))return 0;
            put(a,0x6041e0,1);n=game_menu_map_file_preview(a,path);put(a,0x6041e0,0);if(!n)return 0;}
        return !a->memory.fault;
    }
    if(method==0x4c7550)return game_menu_text_entry_set(a,get(a,0x5910f0),receiver+0x98);
    if(method==0x4c7570){GameAddress selector=get(a,0x5910fc);
        if(get(a,selector+0x58)){put(a,selector+0x58,0);put(a,selector+0x1c,1);}
        return game_menu_selector_select(a,selector,0);
    }
    if(method==0x4c7500 || method==0x4c76a0)return close_screen(a,method==0x4c76a0,1);
    if(method==0x4c76f0)method=0x4c7590;
    if(method==0x4c7590){edit=get(a,0x5910f0);if(!edit || !text(a,edit+0x78,name,sizeof(name)) || !*name)return 0;
        if(get(a,0x59c6a0)){
            n=snprintf(path,sizeof(path),"saves\\%s.sav",name);
            if(n<0 || (size_t)n>=sizeof(path) || !game_campaign_save(a,path) || !close_screen(a,1,0))return 0;
            if(get(a,0x5910ac)){put(a,0x5910ac,0);return game_menu_request(a,get(a,0x59d4c0),69,1000,3);}return !a->memory.fault;}
        if((int32_t)get(a,get(a,get(a,0x5f0208)+0x3bc)+0x58)<=0)return 0;
        if(get(a,0x5910a8)){if(!game_menu_saved_snapshot_export(a,name))return 0;}
        else{n=snprintf(path,sizeof(path),"saves\\%s.sfl",name);if(n<0 || (size_t)n>=sizeof(path) || !game_level_save_scenario(a,path))return 0;}
        return close_screen(a,1,0);
    }
    if(method!=0x4c7310)return 0;
    family=0;if(get(a,0x59708c))return close_screen(a,family,1);
    selected=get(a,0x5910e8);if(!selected)return !a->memory.fault;
    if(!text(a,selected,name,sizeof(name)))return 0;
    if(get(a,0x59c6a0))return game_menu_saved_campaign_load(a,name);
    if(get(a,0x5910a8)){
        if(!game_menu_saved_snapshot_copy(a,name))return 0;
        n=snprintf(path,sizeof(path),"saves\\%s.sav",name);if(n<0 || (size_t)n>=sizeof(path))return 0;
        if(saved_running(a,path)){put(a,0x58360c,1);put(a,0x5910a8,1);}
        else{put(a,0x5910b0,1);return game_input_pause(a,1) && game_level_resume_presentation(a);}
    }
    else{n=snprintf(path,sizeof(path),"saves\\%s.sfl",name);put(a,0x58360c,1);}
    if(n<0 || (size_t)n>=sizeof(path) || !game_memory_write(&a->memory,0x5e1248,path,(size_t)n+1))return 0;
    return game_input_pause(a,1) && game_level_resume_presentation(a);
}
