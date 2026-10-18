#include "campaign_world.h"
#include "menu_widgets.h"
#include "game_initializers.h"
#include "game_matrix.h"
#include "portable_crt.h"
#include "runtime_utilities.h"
#include "scene_projection_storage.h"
#include <string.h>
extern int game_menu_register_tick(GameApplication *,GameAddress,GameAddress,uint32_t,uint32_t);
extern int game_menu_register_region(GameApplication *,GameAddress,GameAddress,GameAddress,uint32_t,uint32_t,uint32_t);
extern int game_menu_unregister(GameApplication *,GameAddress,GameAddress,int);
extern uint32_t game_menu_milliseconds(GameApplication *);
extern int game_campaign_caption_update(GameApplication *,GameAddress);
/* Live graph advancement, actual polygon submission and picking remain strong
 * ordinary-C dependencies; no CPU fallback or substituted scene is permitted. */
extern int game_campaign_world_advance(GameApplication *,uint32_t); /*45cdd0*/
extern int game_campaign_preview_render(GameApplication *,GameAddress); /*44cb30*/
extern int game_campaign_planet_pick(GameApplication *,GameAddress,int32_t,int32_t,float *); /*45c360*/
extern int game_campaign_planet_selection_draw(GameApplication *,GameAddress,const GameUiRect *); /*45bff0*/
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
static float real(GameApplication *a,GameAddress p){uint32_t bits=get(a,p);float v;memcpy(&v,&bits,4);return v;}
int game_campaign_selection_refresh(GameApplication *a){put(a,0x5adde8,0);return !a->memory.fault;}
int game_menu_projection_bounds(GameApplication *a,uint32_t left,uint32_t top,uint32_t right,uint32_t bottom){
    put(a,0x59d450,left);put(a,0x59d458,top);put(a,0x59d454,right);put(a,0x59d45c,bottom);return !a->memory.fault;
}
static int camera_view(GameApplication *a,GameAddress camera) {
    float position[3];unsigned i;for(i=0;i<3;++i)position[i]=real(a,camera+0xc+i*4);
    runtime_set_mode_enabled(&a->memory,game_memory_read_u16(&a->memory,camera+6));
    return game_matrix_camera_view(&a->memory,0x5cedf0,position,game_memory_read_u16(&a->memory,camera+10),game_memory_read_u16(&a->memory,camera+8),game_memory_read_u16(&a->memory,camera+6));
}
static int camera_pose(GameApplication *a,GameAddress camera,int small,int distant) {
    put(a,camera+0xc,small?0xc37e0000:0xc3c30000);put(a,camera+0x10,small?0x436c0000:0x43f50000);
    put(a,camera+0x14,distant?(small?0xc56ea800:0xc58c8c00):(small?0xc4886000:0xc4a0a000));
    game_memory_write_u16(&a->memory,camera+6,small?108:90);game_memory_write_u16(&a->memory,camera+8,small?168:148);game_memory_write_u16(&a->memory,camera+10,0);
    return camera_view(a,camera);
}
/*44c720: first click starts departure; a second during travel changes the
 * authoritative selection immediately and cancels pending arrival selection. */
int game_campaign_preview_select(GameApplication *a,GameAddress preview,uint32_t index) {
    GameAddress root=get(a,0x585408),camera=get(a,preview+0x170);float position[3];uint16_t heading;
    if(!game_campaign_mission(a,index,1) || !camera)return 0;
    if(get(a,preview+0x16c)==12){
        put(a,root+0x1c,index);put(a,root+0x20,1);put(a,preview+0x17c,UINT32_MAX);
        return game_campaign_caption_update(a,get(a,0x59c6cc)) && camera_pose(a,camera,0,0) &&
            (put(a,preview+0x16c,10),!a->memory.fault);
    }
    put(a,preview+0x16c,12);put(a,preview+0x17c,index);
    position[0]=(float)((long double)((int32_t)(portable_crt_rand_step(&a->memory)%800)-400)-real(a,0x57319c));
    position[1]=real(a,0x573194);position[2]=real(a,0x5731a0);heading=(uint16_t)(portable_crt_rand_step(&a->memory)%90);
    return game_menu_preview_camera_transition(a,camera,position,game_memory_read_u16(&a->memory,camera+10),148,heading,800);
}
/*44c890: reuse the shared native camera's exact transition arithmetic. */
static int camera_advance(GameApplication *a,GameAddress preview,uint32_t elapsed) {
    GameAddress camera=get(a,preview+0x170),root=get(a,0x585408);uint32_t result,index;float position[3];int small;unsigned i;
    if(!game_menu_preview_camera_command(a,camera,elapsed,0,&result))return 0;
    if(result!=11)return !a->memory.fault;
    index=get(a,preview+0x17c);if(index==UINT32_MAX){put(a,preview+0x16c,10);return !a->memory.fault;}
    if(!game_campaign_mission(a,index,1))return 0;put(a,root+0x1c,index);put(a,root+0x20,1);
    if(!game_campaign_caption_update(a,get(a,0x59c6cc)))return 0;put(a,preview+0x17c,UINT32_MAX);
    small=get(a,game_campaign_system(a,index)+4)==2;
    if(!camera_pose(a,camera,small,1))return 0;
    for(i=0;i<3;++i)position[i]=real(a,(small?0x573184:0x573190)+i*4);
    return game_menu_preview_camera_transition(a,camera,position,0,small?168:148,small?108:90,400);
}
int game_campaign_preview_tick(GameApplication *a,GameAddress preview) {
    uint32_t now=game_menu_milliseconds(a),previous=get(a,0x5adde8),elapsed=previous?now-previous:1;
    put(a,0x5adde8,now);put(a,0x5addec,elapsed);
    if(!game_campaign_world_advance(a,elapsed) || (get(a,preview+0x16c)==12 && !camera_advance(a,preview,elapsed)) ||
       !game_campaign_preview_render(a,preview))return 0;
    put(a,preview+0x18,1);put(a,preview+0x1c,1);return !a->memory.fault;
}
/*44c660: back-to-front actual planet picking, excluding hidden records. */
int game_campaign_preview_pick(GameApplication *a,GameAddress preview,int32_t x,int32_t y) {
    GameAddress root=get(a,0x585408),system=game_campaign_system(a,get(a,root+0x1c)),planet;uint32_t count=get(a,system+4),index;float distance;
    if(!system || count>65536)return 0;
    x=(int32_t)((uint32_t)x-get(a,preview+0x20));y=(int32_t)((uint32_t)y-get(a,preview+0x24));
    for(index=count;index>1;){--index;planet=game_campaign_mission(a,get(a,root+0x1c),index);
        if(!get(a,planet+0x20) && game_campaign_planet_pick(a,planet,x,y,&distance)){
            put(a,root+0x20,index);return game_campaign_caption_update(a,get(a,0x59c6cc));}
        if(a->memory.fault)return 0;
    }return !a->memory.fault;
}
int game_campaign_preview_draw(GameApplication *a,GameAddress preview,const GameUiRect *clip) {
    GameAddress image=get(a,preview+0x178),root=get(a,0x585408),system=game_campaign_system(a,get(a,root+0x1c)),planet;GameUiRect bounds;
    if(!image || !clip || !game_menu_widget_position(a,image,(uint32_t)clip->left,(uint32_t)clip->top,0,0) || !game_menu_widget_draw(a,image,clip))return 0;
    if(get(a,preview+0x16c)!=10 || get(a,system+4)==2)return !a->memory.fault;
    planet=game_campaign_mission(a,get(a,root+0x1c),get(a,root+0x20));if(!planet)return !a->memory.fault;
    bounds.left=(int32_t)get(a,preview+0x20);bounds.top=(int32_t)get(a,preview+0x24);bounds.right=(int32_t)get(a,preview+0x28);bounds.bottom=(int32_t)get(a,preview+0x2c);
    return game_campaign_planet_selection_draw(a,planet,&bounds);
}
int game_campaign_preview_destroy(GameApplication *a,GameAddress preview) {
    unsigned i;static const unsigned storage[]={0x174,0x170};GameAddress p;
    if(!game_menu_unregister(a,get(a,0x59d4c0),preview,0) || !game_menu_unregister(a,get(a,0x59d4c0),preview,1))return 0;
    for(i=0;i<2;++i){p=get(a,preview+storage[i]);if(p && !scene_projection_storage_free(p))return 0;put(a,preview+storage[i],0);}
    p=get(a,preview+0x178);if(p && !game_menu_widget_destroy(a,p))return 0;put(a,preview+0x178,0);
    put(a,preview,0x571c58);return game_menu_widget_destroy(a,preview);
}
GameAddress game_campaign_preview_create(GameApplication *a,uint32_t width,uint32_t height) {
    GameAddress preview,camera,p,controller=get(a,0x59d4c0),root=get(a,0x585408);size_t size;
    if(!width || !height || width>8192 || height>8192 || !root)return 0;size=(size_t)width*height*2;
    preview=scene_projection_storage_allocate(0x180,"campaign planet preview");if(!preview)return 0;
    game_memory_fill_u8(&a->memory,preview,0x180,0,0);if(!game_menu_image_surface_initialize(a,preview,width,height,0))goto fail;put(a,preview,0x5731c8);
    camera=scene_projection_storage_allocate(0x48,"campaign preview camera");if(!camera)goto fail;put(a,preview+0x170,camera);
    game_memory_fill_u8(&a->memory,camera,0x48,0,0);game_initializer_004a3980(&a->memory,camera);put(a,camera,0x573178);
    if(!camera_pose(a,camera,get(a,game_campaign_system(a,get(a,root+0x1c))+4)==2,0))goto fail;
    p=game_menu_image_surface_create(a,width,height,1);if(!p)goto fail;put(a,preview+0x178,p);put(a,p+0x13c,254);
    if(!game_menu_image_mode(a,p,101))goto fail;
    p=scene_projection_storage_allocate(size,"campaign preview depth storage");if(!p)goto fail;put(a,preview+0x174,p);put(a,preview+0x17c,UINT32_MAX);
    if(!game_menu_register_tick(a,controller,preview,0x44cab0,0) || !game_menu_register_region(a,controller,preview,preview+0x20,0x201,0x44c660,0))goto fail;
    put(a,preview+0x16c,10);return preview;
fail:(void)game_campaign_preview_destroy(a,preview);return 0;
}
