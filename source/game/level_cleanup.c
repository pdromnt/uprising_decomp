#include "level_run.h"
#include "level_load.h"
#include "level_factory.h"
#include "level_player_roster.h"
#include "level_player_slots.h"
#include "level_geometry.h"
#include "level_hud.h"
#include "level_world_surface.h"
#include "gameplay_chat.h"
#include "menu_sound.h"
#include "menu_music.h"
#include "effect_resource_pool.h"
#include "scene_projection_storage.h"
#include "application_shutdown.h"
#include "gameplay_leaves.h"
#include "unit_defaults.h"
#include "level_script.h"
#include <stdio.h>

/*Distinct unfinished executable behavior remains a mandatory native link.
 *These contracts do not accept a call address/register/argument-word packet. */
extern int game_level_display_restore_menu(GameApplication *); /*516d20,4ac160*/
extern int game_level_spatial_work_end(GameApplication *); /*41ae80*/
extern int game_level_deferred_collect(GameApplication *); /*53fee0*/
extern int game_level_player_ui_release(GameApplication *);
extern int game_level_clock_stop(GameApplication *);
extern int game_application_options_load(GameApplication *,uint32_t);
extern int game_menu_game_defaults(GameApplication *);
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
static int free_slot(GameApplication *a,GameAddress slot){
    GameAddress p=get(a,slot);
    if(a->memory.fault || (p && !scene_projection_storage_free(p)))return 0;
    put(a,slot,0);return !a->memory.fault;
}
/*417120 shared panel destruction: count + record IDs; each panel child owns
 *four payloads spaced24 bytes apart. Preserve order and clear completed work. */
static int panel_release(GameApplication *a,GameAddress slot){
    GameAddress panel=get(a,slot);uint32_t count,i,j,base,size;
    if(a->memory.fault)return 0;
    if(!panel)return 1;
    if(!scene_projection_storage_describe(panel,&base,&size,NULL) || base!=panel || size<4)return 0;
    count=get(a,panel);if(a->memory.fault || count>(size-4)/4)return 0;
    for(i=0;i<count;++i){GameAddress field=panel+4+i*4,child=get(a,field);
        if(a->memory.fault)return 0;
        if(!child)continue;
        if(!scene_projection_storage_describe(child,&base,&size,NULL) || base!=child || size<0x120)return 0;
        put(a,child,0x571fb0);
        for(j=0;j<4;++j)if(!free_slot(a,child+0xd4+j*24))return 0;
        if(!free_slot(a,field))return 0;
    }
    put(a,panel,0);return free_slot(a,slot);
}
/*502e10 pending owned payloads.412730 is an actual empty retail function. */
static int pending_release(GameApplication *a){
    GameAddress record;unsigned visits=0;
    while((record=get(a,0x624ee4))!=0){GameAddress next=get(a,record+0x18);
        if(a->memory.fault || ++visits>65536 || next==record || !free_slot(a,record+0x14))return 0;
        if(!scene_projection_storage_free(record))return 0;
        put(a,0x624ee4,next);
    }
    return !a->memory.fault;
}
static int detach(GameApplication *a){
    unsigned i;GameAddress player;
    for(i=0;i<6;++i)put(a,0x5f01f0+i*4,UINT32_MAX);
    put(a,0x5ac8b8,0);put(a,0x589738,0);
    for(i=0;i<6;++i)if(level_player_slots_scene_player(i,&player))put(a,player+4,0);
    level_player_roster_clear();
    /*5198b0 increments its offset before the second publication. */
    for(i=0;i<4;++i)if(!free_slot(a,0x626758+i*4) || !free_slot(a,0x626e68+i*4))return 0;
    return !a->memory.fault && game_level_cameras_end(a);
}
static int notify(GameApplication *a,GameAddress player){
    uint32_t method=get(a,get(a,player)+0x14);
    if(a->memory.fault)return 0;
    if(method==0x47afa0){if(!get(a,0x5eff04))put(a,player+0x3c0,UINT32_MAX);}
    else if(method!=0x476600)return 0;
    put(a,player+0x14,0);put(a,player+0x18,1);return !a->memory.fault;
}
static int perform(void *c,ApplicationShutdownStep step,unsigned index,uint32_t players[6],uint8_t *mask){
    GameApplication *a=c;unsigned i;GameAddress player,music;
    static const GameAddress pools[]={0x631910,0x5ad7d0,0x5cee40,0x5cef90,0x5ceee8};
    switch(step){
    case APPLICATION_SHUTDOWN_WAIT_PRESENTATION:return game_level_clock_stop(a);
    case APPLICATION_SHUTDOWN_STOP_AUDIO_INPUT:
        if(!pending_release(a) || !game_menu_sound_stop_all(a))return 0;
        if(get(a,0x59bd98)){if(!game_menu_sound_mode(a,2))return 0;put(a,0x59bd98,0);}break;
    case APPLICATION_SHUTDOWN_DETACH_PLAYERS:return detach(a);
    case APPLICATION_SHUTDOWN_RELEASE_LEVEL:
        for(i=0;i<6;++i)players[i]=get(a,0x5f0210+i*4);
        return !a->memory.fault && game_level_session_release(a,LEVEL_RELEASE_NORMAL,LEVEL_RELEASE_PLAYER_SCENES);
    case APPLICATION_SHUTDOWN_FINISH_FRAME:return game_level_factories_release(a);
    case APPLICATION_SHUTDOWN_RELEASE_POOL:return effect_resource_pool_destroy_owned(&a->memory,pools[index]);
    case APPLICATION_SHUTDOWN_FINISH_POOLS:return game_level_deferred_collect(a);
    case APPLICATION_SHUTDOWN_UNLOAD_LEVEL:return game_level_session_release(a,LEVEL_RELEASE_NORMAL,LEVEL_RELEASE_NATIVE_STATE);
    case APPLICATION_SHUTDOWN_FLUSH_PRESENTATION:return game_level_terrain_depth_end(a);
    /*4e18b0's tables/nodes are owned by the existing route/script services.
     *UNLOAD_LEVEL retired those tables; this validates the final manager roots. */
    case APPLICATION_SHUTDOWN_CLOSE_PRESENTATION:return game_level_world_release(a);
    case APPLICATION_SHUTDOWN_CLEAR_ACTIVE:game_memory_write_u8(&a->memory,0x5f0158,0);break;
    case APPLICATION_SHUTDOWN_NOTIFY_PLAYER:
        player=players[index];if(!player || !get(a,player+0x14))break;
        *mask=(uint8_t)(*mask|(1u<<index));game_memory_write_u8(&a->memory,0x5f0158,*mask);return notify(a,player);
    case APPLICATION_SHUTDOWN_MODE_ZERO:
        if(!game_application_options_load(a,0))return 0;
        put(a,0x583b10,0);break;
    case APPLICATION_SHUTDOWN_RELEASE_SERVICE:
        switch(index){
        case 0:return game_level_geometry_release(a);
        case 1:return panel_release(a,0x5aced4);
        case 2:return panel_release(a,0x5f023c);
        case 3:return game_level_radar_tables_end(a);
        case 4:return game_level_chat_release(a);
        case 5:return game_level_hud_assets_release(a);
        case 6:return game_level_input_regions_release(a);
        case 7:return free_slot(a,0x596f80);
        case 8:return game_level_spatial_work_end(a);
        case 9:return game_level_player_ui_release(a);
        case 10:return free_slot(a,0x597f78);
        case 11:return game_level_world_surface_release(a);
        default:return 0;}
    case APPLICATION_SHUTDOWN_FINISH_CONTROLLER:
        /*41843a ignores playback availability. A missing disc/device must
         *not prevent mandatory level cleanup; memory faults still fail below. */
        music=get(a,0x59d828);if(music)(void)game_menu_music_play(a,music,0);
        put(a,0x583604,0);break;
    case APPLICATION_SHUTDOWN_CLOSE_ROOT:return game_level_display_restore_menu(a);
    default:return 0;
    }
    return !a->memory.fault;
}
int game_level_cleanup(GameApplication *a){
    ApplicationShutdownServices services={a,perform};
    if(!a)return 0;
    application_shutdown_begin();
    if(!application_shutdown_execute(&services)){
        ApplicationShutdownStats state;application_shutdown_stats(&state);
        fprintf(stderr,"native level cleanup failed: step=%u cursor=%u fault=%d\n",(unsigned)state.step,state.cursor,a->memory.fault);return 0;
    }
    return game_menu_game_defaults(a);
}
