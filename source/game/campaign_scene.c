#include "campaign_world.h"
#include "menu_widgets.h"
#include "scene_projection_storage.h"
#include <stdio.h>
#include <string.h>
extern int game_menu_register_tick(GameApplication *,GameAddress,GameAddress,uint32_t,uint32_t);
extern int game_menu_unregister(GameApplication *,GameAddress,GameAddress,int);
extern uint32_t game_menu_milliseconds(GameApplication *);
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
static GameAddress image_path(GameApplication *a,const char *path) {
    GameAddress name=scene_projection_storage_allocate(strlen(path)+1,"campaign image path"),p=0;
    if(name && game_memory_write(&a->memory,name,path,strlen(path)+1))p=game_menu_image_create(a,UINT32_MAX,name);
    if(name)scene_projection_storage_free(name);return p;
}
static int paint_system_name(GameApplication *a,GameAddress image,GameAddress name,uint32_t y) {
    char text[128];unsigned i;
    for(i=0;i<sizeof(text);++i){text[i]=(char)game_memory_read_u8(&a->memory,name+i);if(!text[i])break;}
    if(i==sizeof(text))return 0;
    return game_menu_text_surface_paint(a,image,2,text,0x0200ff00,0,(int32_t)y,0,0);
}
/*44bbe0: each system button owns three composed bitmap/text frames. */
static GameAddress system_button(GameApplication *a,uint32_t index) {
    GameAddress system=game_campaign_system(a,index),source=0,frames[4]={0},button=0;unsigned i;char path[128];uint32_t height;
    static const uint32_t y[]={65,20,113};
    if(!system)return 0;
    for(i=0;i<3;++i){
        snprintf(path,sizeof(path),i==2?"media\\dibs\\campaign\\ssb%u.bmp":"media\\dibs\\campaign\\ss%u.bmp",get(a,system+0xc));
        source=image_path(a,path);if(!source)goto fail;height=get(a,source+0x2c)-get(a,source+0x24);
        frames[i]=game_menu_image_surface_create(a,78,80,0);if(!frames[i])goto fail;
        put(a,frames[i]+0x13c,254);
        if(!game_menu_text_surface_fill(a,frames[i],254) || !game_menu_image_mode(a,frames[i],101) ||
           !game_menu_image_compose(a,frames[i],source,39-(int32_t)(get(a,source+0x28)-get(a,source+0x20))/2,2,0,0) ||
           !paint_system_name(a,frames[i],get(a,system+8),height+2))goto fail;
        if(!game_menu_widget_destroy(a,source))goto fail;source=0;
    }
    button=game_menu_button_borrowed_create(a,380+index,frames,UINT32_MAX,UINT32_MAX,index,UINT32_MAX,0);if(!button)goto fail;
    put(a,button,0x5759e0);put(a,button+0x6c,0x44c350);put(a,button+0x68,0);put(a,button+0x94,0);put(a,button+0x98,20);
    if(!game_menu_widget_position(a,button,get(a,0x585180)+(index/3)*224+(index%3?112:0)+5,get(a,0x585184)+y[index%3],51,0))goto owned_fail;
    if(index==get(a,get(a,0x585408)+0x1c) && !game_menu_toggle_state(a,button,2))goto owned_fail;
    return button;
owned_fail:(void)game_menu_widget_destroy(a,button);return 0;
fail:if(source)(void)game_menu_widget_destroy(a,source);for(i=0;i<3;++i)if(frames[i])(void)game_menu_widget_destroy(a,frames[i]);return 0;
}
static int enable_available(GameApplication *a,GameAddress scene) {
    uint32_t i,count=get(a,get(a,0x585408)+8);if(count>7)return 0;
    for(i=0;i<count;++i)if(get(a,game_campaign_system(a,i)+0x10) && !game_menu_widget_enable(a,get(a,scene+0x14+i*4),1))return 0;
    return !a->memory.fault;
}
static int fog_update(GameApplication *a,GameAddress scene) {
    GameAddress fog=get(a,scene+8),root=get(a,0x585408);uint32_t left=get(a,0x585180)+get(a,root+0x24);unsigned i;
    if(!get(a,fog+0x1c))for(i=0;i<16;i+=4)put(a,fog+0x30+i,get(a,fog+0x20+i));
    put(a,fog+0x20,left);put(a,fog+0x1c,1);put(a,fog+0x18,1);put(a,0x58fd3c,1);return !a->memory.fault;
}
/*45cda0/44c190: reveal through the contiguous available system prefix. */
int game_campaign_scene_reveal(GameApplication *a,GameAddress scene) {
    GameAddress root=get(a,0x585408);uint32_t count=get(a,root+8),i=0;int32_t last,target;
    if(count>7)return 0;while(i<count && get(a,game_campaign_system(a,i)+0x10))++i;
    last=(int32_t)i-1;target=(last/3)*224+(last%3==1 || last%3==2?112:0)+112;put(a,scene+0xc,(uint32_t)target);
    if((int32_t)get(a,root+0x24)<target){put(a,0x5851b4,0);put(a,scene+0x10,12);}return !a->memory.fault;
}
int game_campaign_scene_select(GameApplication *a,GameAddress scene) {
    uint32_t i,count=get(a,get(a,0x585408)+8);if(count>7)return 0;if(get(a,scene+0x34))return !a->memory.fault;
    put(a,scene+0x34,1);if(!game_menu_widget_visible(a,get(a,scene+8),1))return 0;
    for(i=0;i<count;++i)if(!game_menu_widget_visible(a,get(a,scene+0x14+i*4),1))return 0;return !a->memory.fault;
}
int game_campaign_scene_update(GameApplication *a,GameAddress scene) {
    if(get(a,scene+0x30))return !a->memory.fault;
    if(!game_menu_register_tick(a,get(a,0x59d4c0),scene,0x44c220,0))return 0;
    put(a,scene+0x30,1);return enable_available(a,scene);
}
int game_campaign_scene_tick(GameApplication *a,GameAddress scene) {
    uint32_t now,previous,delay,value,target;GameAddress root=get(a,0x585408);
    if(get(a,scene+0x10)!=12)return !a->memory.fault;
    now=game_menu_milliseconds(a);previous=get(a,0x5851b4);delay=get(a,0x5851b0);
    if((int32_t)(previous+delay)>(int32_t)now)return !a->memory.fault;
    put(a,0x5851b4,now);value=get(a,root+0x24)+2;target=get(a,scene+0xc);
    if((int32_t)value>=(int32_t)target)value=target;put(a,root+0x24,value);
    if(!fog_update(a,scene))return 0;
    if(value==target){if(!enable_available(a,scene))return 0;put(a,scene+0x10,10);}return !a->memory.fault;
}
int game_campaign_scene_destroy(GameApplication *a,GameAddress scene) {
    uint32_t i,count=get(a,get(a,0x585408)+8);GameAddress p;if(count>7)return 0;
    if(get(a,scene+0x30) && !game_menu_unregister(a,get(a,0x59d4c0),scene,0))return 0;put(a,scene+0x30,0);
    p=get(a,scene+8);if(p && !game_menu_widget_destroy(a,p))return 0;put(a,scene+8,0);
    for(i=0;i<count;++i){p=get(a,scene+0x14+i*4);if(p && !game_menu_widget_destroy(a,p))return 0;put(a,scene+0x14+i*4,0);}
    return !a->memory.fault && scene_projection_storage_free(scene);
}
/*44b710: one record owns the fog and seven heterogeneous radio buttons. */
GameAddress game_campaign_scene_create(GameApplication *a) {
    GameAddress root=get(a,0x585408),scene,p;uint32_t i,count=get(a,root+8);
    if(!root || !count || count>7)return 0;
    scene=scene_projection_storage_allocate(0x38,"campaign map controls");if(!scene)return 0;
    game_memory_fill_u8(&a->memory,scene,0x38,0,0);put(a,scene,0x57317c);put(a,scene+4,UINT32_MAX);put(a,scene+0x10,10);
    for(i=0;i<count;++i){p=system_button(a,i);if(!p)goto fail;put(a,scene+0x14+i*4,p);}
    p=game_menu_image_create(a,UINT32_MAX,0x5851bc);if(!p)goto fail;put(a,scene+8,p);put(a,p+0x13c,254);
    if(!game_menu_widget_position(a,p,get(a,0x585180),get(a,0x585184),55,0) || !game_menu_image_mode(a,p,101) || !fog_update(a,scene) || !game_campaign_scene_reveal(a,scene))goto fail;
    return scene;
fail:(void)game_campaign_scene_destroy(a,scene);return 0;
}
