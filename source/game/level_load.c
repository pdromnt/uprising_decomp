#include "level_load.h"
#include "level_hud.h"
#include "level_run.h"
#include "gameplay_chat.h"
#include "level_script.h"
#include "level_objects.h"
#include "level_factory.h"
#include "level_routes.h"
#include "../core/level_objects.h"
#include "level_citadels.h"
#include "level_entity_store.h"
#include "level_teleporters.h"
#include "application_name_registry.h"
#include "level_world_surface.h"
#include "level_player_resources.h"
#include <stdio.h>
typedef struct Load {GameApplication *a;uint32_t offset;char path[512];} Load;
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
static int destroy_entity(void *c,uint32_t o){return game_level_citadel_destroy(c,o);}
static int destroy_object(void *c,uint32_t o){return level_citadels_owns_runtime(o)?game_level_citadel_destroy(c,o):game_level_factory_destroy(c,o);}
static int destroy_player(void *c,uint32_t p){return game_level_player_scene_destroy(c,p);}
static int destroy_teleporter(void *c,uint32_t o) {
    if(!game_level_teleporter_destroy(c,o))return 0;
    level_player_slots_forget_scene_member(o);return 1;
}
static int active(void *c,uint32_t p,int *enabled){GameApplication *a=c;*enabled=get(a,p+0x14)!=0;return !a->memory.fault;}
static int release_action(void *c,LevelReleaseAction action) {
    GameApplication *a=c;GameAddress view;uint32_t players[6];
    switch(action) {
    case LEVEL_RELEASE_ENTITIES:
        view=level_entity_store_view();if(!view)return 1;
        if(!level_entity_store_destroy_all(a,destroy_entity) || !level_entity_store_release(view))return 0;
        game_memory_write_u16(&a->memory,0x639a74,0);put(a,0x639a70,0);break;
    case LEVEL_RELEASE_ORPHANS:return level_entity_store_destroy_orphans(a,destroy_object);
    case LEVEL_RELEASE_TELEPORTERS:return level_teleporters_destroy_owned(a,destroy_teleporter);
    case LEVEL_RELEASE_SCENE_MEMBERS:return level_player_slots_destroy_scene_members(a,destroy_object);
    case LEVEL_RELEASE_PLAYER_SCENES:
        /* Player resources precede level_session_load/begin_scenes. A failed
         * preparation still owns those pools, even though no scene registry
         * was published. Release through the existing manager slots; each
         * successful return clears its field for a practical cleanup retry. */
        if(!level_player_slots_scene_player(0,&view)){
            unsigned i;
            for(i=0;i<6;++i){GameAddress player=get(a,0x5f0210+i*4);
                if(a->memory.fault || (player && !game_level_player_resources_release(a,player)))return 0;
            }
            return !a->memory.fault;
        }
        if(a->level_session.release_mode==LEVEL_RELEASE_ROLLBACK)return level_player_slots_destroy_new_scenes(a,destroy_player);
        return level_player_slots_destroy_active_scenes(a,active,destroy_player,players);
    case LEVEL_RELEASE_NAMES:
        if(a->level_names_active && !application_name_registry_release_since(a->level_name_mark))return 0;
        a->level_names_active=0;break;
    case LEVEL_RELEASE_NATIVE_STATE:
        if(!game_level_terrain_depth_end(a))return 0;
        if(!game_level_spawn_factory_shutdown(a))return 0;
        if(!game_level_spawn_variants_shutdown(a))return 0;
        if(!game_level_vehicle_factory_shutdown(a))return 0;
        if(!game_level_radar_tables_end(a))return 0;
        if(!game_level_world_surface_release(a))return 0;
        if(!game_level_chat_release(a))return 0;
        if(!game_level_hud_assets_release(a))return 0;
        if(!game_level_display_masks_release(a))return 0;
        if(!game_level_input_regions_release(a))return 0;
        if(!game_level_transition_assets_release(a))return 0;
        if(!game_level_navigation_routes_release(a,0x604258) || !game_level_script_release(a) || !game_level_terrain_release(a) || !game_level_routes_release(a))return 0;
        level_teleporters_unload();level_objects_unload();level_citadels_unload();level_player_slots_unload();level_entity_store_clear_guest_objects();
        put(a,0x597d90,0);put(a,0x597d8c,0);put(a,0x597d94,0);put(a,0x626710,0);put(a,0x597db0,0);break;
    default:return 0;
    }
    return !a->memory.fault;
}
int game_level_session_release(GameApplication *a,LevelReleaseMode mode,LevelReleaseAction through) {
    /*418200 retires camera borrowers before releasing the level's scenes. */
    if(mode==LEVEL_RELEASE_NORMAL && !game_level_cameras_end(a))return 0;
    int ok=level_session_release_owned(&a->level_session,mode,through,a,release_action);
    if(!ok)fprintf(stderr,"native level release failed: action=%u through=%u mode=%u fault=%d\n",a->level_session.release_cursor,through,mode,a->memory.fault);
    if(ok && through==LEVEL_RELEASE_NATIVE_STATE && mode==LEVEL_RELEASE_NORMAL)level_session_unload(&a->level_session);
    return ok;
}
static int rollback(void *c,LevelLoadStage failed,uint32_t completed) {
    static const char *names[]={"terrain","players","citadels","objects","routes","teleporters"};
    Load *load=c;(void)completed;
    game_application_log(load->a,"native level load failed: %s\n",
        (unsigned)failed<LEVEL_STAGE_COUNT?names[failed]:"unknown stage");
    return game_level_session_release(load->a,LEVEL_RELEASE_ROLLBACK,LEVEL_RELEASE_NATIVE_STATE);
}
static int stage(void *c,LevelLoadStage which,const LevelManifest *manifest) {
    Load *load=c;GameApplication *a=load->a;uint32_t next;int ok;
    switch(which) {
    case LEVEL_STAGE_TERRAIN:return game_level_terrain_load(a,load->path,manifest->terrain_offset);
    case LEVEL_STAGE_PLAYERS:
        if(!game_level_loading_text(a,0x58f9f0) || !game_level_terrain_depth_begin(a) || !game_level_loading_text(a,0x58fa0c))return 0;
        ok=level_player_slots_load(load->path,manifest->slots_offset,a,game_level_slot_place,&next);break;
    case LEVEL_STAGE_ENTITIES:ok=game_level_citadels_load(a,load->path,(int32_t)load->offset,&next);break;
    case LEVEL_STAGE_RECORDS:
        if(!game_level_loading_text(a,0x58fa24) || !game_level_visibility_reset(a,0x5cf038) || !game_level_loading_text(a,0x58fa40))return 0;
        ok=game_level_objects_load(a,load->path,(int32_t)load->offset,&next);break;
    case LEVEL_STAGE_SCENE:ok=game_level_routes_load(a,load->path,(int32_t)load->offset,&next);break;
    case LEVEL_STAGE_EFFECTS:ok=game_level_teleporters_load(a,load->path,(int32_t)load->offset,&next);break;
    default:return 0;
    }
    if(ok)load->offset=next;
    return ok && !a->memory.fault;
}
int game_level_session_load(GameApplication *a) {
    Load load={0};LevelManifest manifest;LevelSessionLoader loader={&load,stage,rollback};
    uint32_t players[6],scenes[6],sequences[6];uint16_t owners[6];unsigned i;char stem[256];int length;
    load.a=a;
    if(!level_session_retry_rollback(&a->level_session,&loader))return 0;
    for(i=0;i<6;++i) {
        GameAddress methods;players[i]=get(a,0x5f0210+i*4);if(!players[i] || a->memory.fault)return 0;
        methods=get(a,players[i]);scenes[i]=methods==0x573e78?get(a,players[i]+0x448):0;
        owners[i]=game_memory_read_u16(&a->memory,players[i]+8);sequences[i]=get(a,players[i]+0x18);
    }
    for(i=0;i<sizeof(stem);++i){stem[i]=(char)game_memory_read_u8(&a->memory,0x5acc90+i);if(!stem[i])break;}
    if(a->memory.fault || i==sizeof(stem))return 0;
    length=snprintf(load.path,sizeof(load.path),"grids\\%s",stem);
    if(length<=0 || (size_t)length>=sizeof(load.path) || !level_manifest_read(load.path,&manifest))return 0;
    a->level_name_mark=application_name_registry_mark();a->level_names_active=1;
    if(!level_player_slots_begin_scenes(&a->memory,players,scenes,owners,sequences)){a->level_names_active=0;return 0;}
    /*Rollback may run before startup publishes the roster. Bind the real
     *controller class now so a failed player placement can retire its owner. */
    for(i=0;i<6;++i){GameAddress methods=get(a,players[i]);
        LevelPlayerClass kind=methods==0x573c10 || methods==0x573c98?LEVEL_PLAYER_BASE:
            methods==0x573e78?LEVEL_PLAYER_SCENE:LEVEL_PLAYER_EXTERNAL;
        if(a->memory.fault || !level_player_slots_bind_class(players[i],kind))return 0;
    }
    return level_session_load(&a->level_session,load.path,&manifest,&loader) && !a->memory.fault;
}
