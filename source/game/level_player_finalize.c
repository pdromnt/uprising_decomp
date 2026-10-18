#include "level_player_scene.h"
#include "level_player_factory.h"
#include "level_factory.h"
#include "level_load.h"
#include "player_scene_children.h"
#include "player_runtime_pool.h"
#include "gameplay_leaves.h"
#include "gameplay_state.h"
#include "game_intrusive_list.h"
#include "scene_projection_storage.h"
#include "display_transition.h"
#include "application_mode.h"
#include "event_queue.h"
#include <string.h>
extern int game_application_options_load(GameApplication *,uint32_t);
extern int game_level_catalog_refresh_selection(void *,GameAddress);
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
static float real(GameApplication *a,GameAddress p){uint32_t bits=get(a,p);float value;memcpy(&value,&bits,4);return value;}
static int child_release(GameApplication *a,GameAddress child){
    game_leaf_0043b4c0(&a->memory,child);
    return !a->memory.fault && scene_projection_storage_free(child);
}
/*4f69d0 retires the active scene while retaining its pooled player/rig object.
 *Children save ammunition before release; event/contact chains return to their
 *existing owners. Later factory destruction owns the model and base allocation. */
int game_level_player_finalize(GameApplication *a,GameAddress player){
    GameAddress catalog,value,source,camera;uint32_t flags,method;uint16_t heading;float from[3],to[3],clearance;unsigned i;
    GameplayPeerServices peers={a,NULL};
    if(!a || !player || a->memory.fault)return 0;
    flags=get(a,player+20);catalog=get(a,0x5f0210+(flags&63)*4);
    game_memory_write_u8(&a->memory,player+0xd8,0);put(a,player+0x58,0);
    game_list_detach_node(&a->memory,player,4,8);
    if(a->memory.fault)return 0;
    if(player_scene_children_find(player)){
        if(!player_scene_children_finalize(&a->memory,player) || !player_runtime_pool_bind_parts(&a->memory,player))return 0;
    }else{
        for(i=0;i<15;i++){GameAddress slot=player+0x1d4+i*4,child=get(a,slot);
            if(child){put(a,player+0x3e8+i*4,get(a,child+12));if(!child_release(a,child))return 0;put(a,slot,0);}}
        value=get(a,player+0x210);if(value){if(!child_release(a,value))return 0;put(a,player+0x210,0);}
    }
    put(a,player+0x1cc,0);put(a,player+0x1d0,0);
    for(i=0;i<2;i++){GameAddress slot=player+0x21c+i*4,attached=get(a,slot);
        if(attached){put(a,attached+0xe4,0);put(a,slot,0);}}
    if(a->memory.fault || !game_level_player_clear_events(a,player))return 0;
    if(get(a,catalog+0x448)!=get(a,catalog+0x3bc) && !game_level_catalog_refresh_selection(a,catalog))return 0;
    put(a,player+20,get(a,player+20)&63);
    value=get(a,player+0xb0);if(value){if(!game_level_factory_release_pool_member(a,value))return 0;put(a,player+0xb0,0);}
    method=get(a,get(a,player)+0x100);
    if(a->memory.fault || !gameplay_state_peer_method(&a->memory,player,0,method,&peers))return 0;
    put(a,catalog+0x5a0,0);
    if(catalog!=get(a,0x5f0208) || application_mode_current()==4)return !a->memory.fault;
    if(application_mode_current()!=7 && !game_root_selector_close(&a->memory,NULL))return 0;
    if(!game_application_options_load(a,3))return 0;
    value=get(a,0x58f890);
    if(get(a,0x58f898)==value || get(a,0x58f89c)==value){source=value+12;heading=game_memory_read_u16(&a->memory,value+6);}
    else{source=player+0x64;heading=game_memory_read_u16(&a->memory,player+0x78);}
    for(i=0;i<3;i++){from[i]=real(a,source+i*4);to[i]=real(a,player+24+i*4);}
    clearance=real(a,player+0x30);camera=get(a,0x58f8b0);
    if(a->memory.fault || !game_camera_transition_begin(&a->memory,camera,from,to,heading,clearance))return 0;
    return game_camera_activate(&a->memory,get(a,0x58f8b0),NULL);
}
