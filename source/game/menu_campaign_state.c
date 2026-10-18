#include "campaign_world.h"
#include "menu_widgets.h"
#include "menu_sound.h"
#include <stdio.h>
/* Real adjacent dependencies. These must be implemented for the game target
 * to link; none dispatches a CPU calling convention or supplies fake content. */
extern int game_campaign_catalog_prepare(GameApplication *,uint32_t,uint32_t); /*5580c0*/
extern int game_campaign_catalog_release(GameApplication *); /*5584a0*/
extern GameAddress game_campaign_scene_create(GameApplication *); /*44b710*/
extern int game_campaign_scene_select(GameApplication *,GameAddress); /*44ba50*/
extern int game_campaign_scene_update(GameApplication *,GameAddress); /*44bb30*/
extern int game_campaign_scene_destroy(GameApplication *,GameAddress); /*44b8d0*/
extern GameAddress game_campaign_preview_create(GameApplication *,uint32_t,uint32_t); /*44c370*/
extern GameAddress game_campaign_caption_create(GameApplication *); /*44cdb0*/
extern int game_campaign_caption_destroy(GameApplication *,GameAddress); /*44cf80*/
extern int game_campaign_selection_refresh(GameApplication *); /*44b3d0*/
extern int game_menu_projection_bounds(GameApplication *,uint32_t,uint32_t,uint32_t,uint32_t); /*532710*/
extern int game_level_player_ui_initialize(GameApplication *); /*5476f0*/
extern int game_level_player_ui_release(GameApplication *); /*547730*/
extern int game_menu_selection_begin(GameApplication *);
extern int game_menu_selection_end(GameApplication *);
extern int game_menu_cursor_mode(GameApplication *,int);
extern int game_menu_window_clear(GameApplication *);
extern int game_menu_palette_load_named(GameApplication *,GameAddress);
extern int game_menu_text_cache_release(GameApplication *);
extern int game_menu_text_cache_initialize(GameApplication *);
extern int game_menu_register_observer(GameApplication *,GameAddress,uint32_t,uint32_t);
extern int game_menu_controller_keyboard_handler(GameApplication *,GameAddress,uint32_t,uint32_t);
extern int game_menu_unregister_keyboard_method(GameApplication *,GameAddress,uint32_t);
extern int game_menu_strategic_depart(GameApplication *,uint32_t,uint32_t);
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
static int show(GameApplication *a,GameAddress slot,GameAddress p,uint32_t x,uint32_t y,uint32_t z,int enable) {
    if(!p)return 0;put(a,slot,p);
    return game_menu_widget_position(a,p,x,y,z,0) && game_menu_widget_visible(a,p,1) && (!enable || game_menu_widget_enable(a,p,1));
}
static int release_controls(GameApplication *a) {
    unsigned i;int ok=1;for(i=0;i<9;++i){GameAddress slot=0x59c6a8+i*4,p=get(a,slot);
        if(p && !(i==7?game_campaign_scene_destroy(a,p):game_menu_widget_destroy(a,p)))ok=0;else put(a,slot,0);}
    {GameAddress p=get(a,0x59c6cc);if(p && !game_campaign_caption_destroy(a,p))ok=0;else put(a,0x59c6cc,0);}
    return ok && !a->memory.fault;
}
/*52f310: new campaign, saved campaign and post-mission progression connect
 * to one owned world. Research returns keep that world and its catalog alive. */
int game_menu_strategic_state(GameApplication *a,uint32_t phase) {
    static const struct {GameAddress slot,n0,n1,n2;uint32_t callback,x,y,release;} buttons[]={
        {0x59c6ac,0x59c7dc,0x59c7b8,0x59c798,0,223,454,145},
        {0x59c6b0,0x59c844,0x59c820,0x59c7fc,0x52fce0,434,454,145},
        {0x59c6b8,0x59c8b4,0x59c890,0x59c86c,0x52fd00,260,123,UINT32_MAX},
        {0x59c6bc,0x59c938,0x59c908,0x59c8d8,0x52fe60,27,177,UINT32_MAX},
        {0x59c6c0,0x59c9c4,0x59c994,0x59c968,0x52ff50,138,177,UINT32_MAX}};
    GameAddress event=get(a,0x59d4c4),controller=get(a,0x59d4c0),root,p;uint32_t previous=get(a,event+0x10),next=get(a,event+0x14);unsigned i;int ok=1;
    if(phase==1){
        if(!game_menu_unregister_keyboard_method(a,controller,0x531700) || !release_controls(a))ok=0;
        if(next==31 && !game_campaign_save(a,"saves\\temp"))ok=0;
        if(next!=32){if(!game_campaign_destroy(a) || !game_campaign_catalog_release(a) || !game_level_player_ui_release(a))ok=0;}
        if(next!=32 && next!=31 && next!=17 && !game_menu_selection_end(a))ok=0;
        if(get(a,0x59c6a0) && (!game_menu_strategic_depart(a,0,4) || !game_menu_strategic_depart(a,1,4)))ok=0;
        put(a,0x5910a4,0);if(!game_menu_window_clear(a) || !game_menu_palette_load_named(a,next==32?0x59c9f0:0x59ca0c))ok=0;
        if(next!=32 && (!game_menu_text_cache_release(a) || !game_menu_text_cache_initialize(a)))ok=0;
        return ok && !a->memory.fault;
    }
    if(phase)return !a->memory.fault;
    if(!game_menu_cursor_mode(a,1) || ((previous!=32 || get(a,0x59c6d8)) && !game_menu_selection_begin(a)) ||
       !game_menu_projection_bounds(a,0,0,639,479))return 0;
    if(previous!=32 || get(a,0x59c6d8)){
        put(a,0x59c6d8,0);
        if(!game_level_player_ui_initialize(a) || !game_campaign_catalog_prepare(a,20,7))return 0;
        if(previous==29){if(!game_campaign_new(a))return 0;game_campaign_factions_default(a);}
        else {
            if(!game_campaign_load(a,"saves\\temp"))return 0;
            p=get(a,0x5f0210);
            if(previous==19 && p){
                put(a,0x602874,5);for(i=0;i<4;++i)put(a,0x602878+i*4,get(a,p+0x4b4+i*4));
                if(get(a,p+0x148)){
                    root=get(a,0x585408);
                    if(!game_campaign_complete(a,get(a,root+0x14),get(a,root+0x18)))return 0;
                    if(get(a,game_campaign_system(a,get(a,root+0x14))+4)==2){
                        uint32_t faction=game_memory_read_u16(&a->memory,p+8);if(faction>=4)return 0;
                        put(a,0x60286c+faction*0xf0,0);
                        if(!game_campaign_save(a,"saves\\temp") || !game_menu_register_observer(a,controller,0x524730,30))return 0;
                    }
                }
            }
        }
    }
    if(!game_menu_window_clear(a) || !game_menu_palette_load_named(a,0x59c758))goto fail;
    if(!show(a,0x59c6a8,game_menu_image_create(a,UINT32_MAX,0x59c774),0,0,0,0))goto fail;
    for(i=0;i<sizeof(buttons)/sizeof(*buttons);++i){
        GameAddress names[4]={buttons[i].n0,buttons[i].n1,buttons[i].n2,0};
        p=game_menu_button_create(a,UINT32_MAX,names,buttons[i].callback?UINT32_MAX:69,buttons[i].callback?UINT32_MAX:1000,buttons[i].callback?0:32,buttons[i].callback);
        if(!show(a,buttons[i].slot,p,buttons[i].x,buttons[i].y,51,1))goto fail;
        put(a,p+0x50,144);put(a,p+0x54,buttons[i].release);
    }
    {GameMenuTextStyle style={148,156,82,1,83,12,0,0,0};int32_t credits=(int32_t)get(a,0x5e1448);char value[48];uint32_t width;
        if(credits>=1000000)snprintf(value,sizeof(value),"%d,%03d,%03d",credits/1000000,(credits/1000)%1000,credits%1000);
        else if(credits>1000)snprintf(value,sizeof(value),"%d,%03d",credits/1000,credits%1000);else snprintf(value,sizeof(value),"%d",credits);
        p=game_menu_text_image_create(a,0x59c868,&style);if(!p)goto fail;put(a,0x59c6b4,p);
        if(!game_menu_text_image_set_bytes(a,p,value,1))goto fail;width=get(a,p+0x28)-get(a,p+0x20);
        if(!show(a,0x59c6b4,p,302-width/2,92,51,0))goto fail;
    }
    p=game_campaign_scene_create(a);if(!p)goto fail;put(a,0x59c6c4,p);
    if(!game_campaign_scene_select(a,p) || !game_campaign_scene_update(a,p))goto fail;
    if(!show(a,0x59c6c8,game_campaign_preview_create(a,226,167),358,27,51,0))goto fail;
    p=game_campaign_caption_create(a);if(!p)goto fail;put(a,0x59c6cc,p);
    if(!game_menu_controller_keyboard_handler(a,controller,0x531700,UINT32_MAX) || !game_campaign_selection_refresh(a))goto fail;
    return !a->memory.fault;
fail:(void)game_menu_unregister_keyboard_method(a,controller,0x531700);(void)release_controls(a);return 0;
}
