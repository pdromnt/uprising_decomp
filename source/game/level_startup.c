#include "level_run.h"
#include "level_load.h"
#include "level_factory.h"
#include "effect_resource_pool.h"
#include "game_initializers.h"
#include "gameplay_leaves.h"
#include "level_player_roster.h"
#include "level_player_slots.h"
#include "application_frame.h"
#include "menu_sound.h"
#include "level_geometry.h"
#include "portable_crt.h"
#include <time.h>
/*Distinct internal implementations still required by this live startup. All
 *arguments describe actual work; none carries registers, stacks or call words. */
extern int game_level_progress_begin(GameApplication *,unsigned); /*515550*/
extern int game_level_progress(GameApplication *,GameAddress,GameAddress); /*5156c0*/
extern int game_level_progress_end(GameApplication *); /*5157c0*/
extern int game_level_work_grid_reset(GameApplication *,GameAddress); /*4fe870*/
extern int game_level_visibility_pool_reset(GameApplication *,GameAddress); /*457b80*/
extern int game_application_options_load(GameApplication *,uint32_t);
extern int game_level_viewport_size(GameApplication *,uint32_t *,uint32_t *); /*5160e0*/
extern int game_level_palette_load(GameApplication *,GameAddress,GameAddress); /*4d81e0*/
extern int game_level_palette_initialize(GameApplication *); /*4d8310*/
extern int game_level_palette_map(GameApplication *); /*4d8590*/
extern int game_level_graphics_initialize(GameApplication *); /*5036f0*/
extern int game_level_player_ui_initialize(GameApplication *); /*5476f0*/
extern int game_level_input_regions_initialize(GameApplication *); /*51bcc0*/
extern int game_level_window_initialize(GameApplication *); /*4ac830*/
extern int game_level_viewport_position(GameApplication *,uint32_t[2]); /*518b40*/
extern int game_level_geometry_prepare(GameApplication *); /*4f0190*/
extern int game_level_world_prepare(GameApplication *); /*4e16e0*/
extern int game_level_texture_profiles_initialize(GameApplication *); /*5566f0*/
extern int game_level_player_prepare(GameApplication *,GameAddress); /*player virtual1c*/
extern int game_level_visibility_finish(GameApplication *); /*4599d0*/
extern int game_level_imperial_initialize(GameApplication *,GameAddress); /*47a340*/
extern int game_level_player_script_load(GameApplication *); /*484930*/
extern int game_level_commands_initialize(GameApplication *); /*4e1b50*/
extern int game_level_geometry_load(GameApplication *,GameAddress); /*49ca40*/
extern int game_level_scene_audio_initialize(GameApplication *); /*5390a0*/
extern int game_level_display_visibility(GameApplication *,int); /*5179c0*/
extern int game_level_clock_start(GameApplication *); /*43e6a0/43e790*/
extern int game_level_default_viewport(GameApplication *); /*4af770*/
extern int game_menu_music_start(GameApplication *,uint32_t);
extern int game_menu_media_available(GameApplication *);
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
/*MSVC's main-thread rand/srand state belongs to this game, independently of
 *the host C library and preserved oracle. All native random consumers share it. */
int game_level_random_table(GameApplication *a){return game_initializer_effect_random_table(&a->memory);}
/*5188b0 is a verified empty diagnostic routine, not loading/gameplay work. */
int game_level_loading_text(GameApplication *a,GameAddress text){(void)text;return !a->memory.fault;}
static int progress(GameApplication *a,GameAddress message){return game_level_progress(a,message,0);}
static int initialize(GameApplication *a) {
    GameMemory *m=&a->memory;uint32_t width,height;
    if(!effect_resource_pool_begin(m,0x631910,5,0,5) || !effect_resource_pool_begin(m,0x5ad7d0,1,5,1) ||
       !effect_resource_pool_initialize_events(m,0x5cee40,6,1,4) ||
       !effect_resource_pool_initialize_events(m,0x5cef90,6,2,4) ||
       !effect_resource_pool_initialize_events(m,0x5ceee8,3,3,1))return 0;
    put(a,0x631908,0);put(a,0x63a2bc,0x455ac000);put(a,0x5ac6ac,0);put(a,0x60464c,1);put(a,0x594d38,0);
    if(!game_level_progress_begin(a,24) || !game_level_progress(a,0x583ba0,0x583b88) ||
       !game_level_random_table(a) || !game_level_work_grid_reset(a,0x60a4c8) || !game_level_visibility_pool_reset(a,0x5cf038))return 0;
    portable_crt_rand_seed(m,(uint32_t)time(NULL));game_leaf_00502e00(m);
    /*412720 is also an original empty method. */
    if(!game_application_options_load(a,1))return 0;
    put(a,0x583b10,0x418470);
    if(!game_level_viewport_size(a,&width,&height))return 0;
    put(a,0x5ac6a0,width);put(a,0x5ac6a4,height);return !m->fault;
}
static int display(GameApplication *a) {
    const char *stage="level palette load";
    int32_t eighth=(int32_t)get(a,0x5ac6a4)>>3,half=(int32_t)get(a,0x5ac6a0)>>1,quarter=(int32_t)get(a,0x5ac6a4)>>1;
    uint32_t position[2];GameAddress path=game_memory_read_u8(&a->memory,0x5ac930)?0x5ac930:0x583ba4;
    put(a,0x5acce0,(uint32_t)half+(uint32_t)eighth);put(a,0x5ac920,(uint32_t)half-(uint32_t)eighth);
    put(a,0x5acce4,(uint32_t)quarter+(uint32_t)eighth);put(a,0x5ac924,(uint32_t)quarter-(uint32_t)eighth);
    if(!game_level_palette_load(a,path,0x5ad3a8) && (path!=0x5ac930 || !game_level_palette_load(a,0x583bb0,0x5ad3a8)))goto fail;
    stage="palette initialize";if(!game_level_palette_initialize(a))goto fail;
    stage="palette map";if(!game_level_palette_map(a))goto fail;
    stage="graphics initialize";if(!game_level_graphics_initialize(a))goto fail;
    stage="player UI";if(!game_level_player_ui_initialize(a))goto fail;
    stage="input regions";if(!game_level_input_regions_initialize(a) || !progress(a,0x583bbc))goto fail;
    stage="level window";if(!game_level_window_initialize(a))goto fail;
    game_initializer_00548a90(&a->memory);
    stage="viewport position";if(!progress(a,0x583bc0) || !game_level_viewport_position(a,position))goto fail;
    put(a,0x5ac9c8,position[0]);put(a,0x5ac9cc,position[1]);put(a,0x5ac9d0,position[0]);put(a,0x5ac9d4,position[1]);
    stage="geometry/world prepare";
    if(a->memory.fault || !game_level_geometry_prepare(a) || !game_level_world_prepare(a))goto fail;
    return 1;
fail:
    game_application_log(a,"native level display failed: %s\n",stage);return 0;
}
static int prewarm(GameApplication *a) {
    unsigned i;
    if(!progress(a,0x583bd8) || !game_level_factory_template(a,0x41,0) || !game_level_factory_template(a,0x45,0) ||
       !progress(a,0x583bf0) || !game_level_factory_template(a,5,0) || !game_level_factory_template(a,0x43,0) ||
       !game_level_factory_template(a,0x42,0) || !progress(a,0x583c08) || !game_level_factory_template(a,0x44,0) ||
       !game_level_factory_template(a,0x10,0) || !game_level_factory_fill(a,0x41,0,30) ||
       !game_level_progress(a,0x583c38,0x583c20) || !game_level_factory_fill(a,0x45,0,10) ||
       !game_level_factory_fill(a,5,0,30) || !progress(a,0x583c3c) || !game_level_factory_fill(a,0x43,0,30) ||
       !game_level_factory_fill(a,0x42,0,15) || !game_level_factory_fill(a,0x44,0,20) || !progress(a,0x583c54) ||
       !game_level_factory_fill(a,0x10,0,10) || !game_level_factory_template(a,0xa0,0) ||
       !progress(a,0x583c6c) || !game_level_texture_profiles_initialize(a))return 0;
    for(i=0;i<3;++i) {
        /*Retail prewarms kind0Ah, not kind10h, for these three variants. */
        if(!game_level_factory_template(a,0x0a,(uint16_t)i) || !game_level_factory_fill(a,0x0a,(uint16_t)i,10) || !progress(a,0x583c78))return 0;
    }
    return progress(a,0x583c84);
}
static int roster(GameApplication *a) {
    uint32_t players[6],flags[6];unsigned i;
    for(i=0;i<6;++i) {
        GameAddress methods;LevelPlayerClass kind;
        if(!level_player_slots_scene_player(i,&players[i]))return 0;
        flags[i]=get(a,players[i]+0x14);methods=get(a,players[i]);
        kind=methods==0x573c10 || methods==0x573c98?LEVEL_PLAYER_BASE:methods==0x573e78?LEVEL_PLAYER_SCENE:
            methods==0x573d20?LEVEL_PLAYER_FACTION:methods==0x573da8?LEVEL_PLAYER_IMPERIAL:LEVEL_PLAYER_EXTERNAL;
        if(a->memory.fault || !level_player_slots_bind_class(players[i],kind))return 0;
    }
    if(!level_player_roster_begin(players,flags))return 0;
    for(i=0;i<6;++i){LevelPlayerRosterSlot slot;if(!level_player_roster_slot(i,&slot))return 0;if(slot.player && slot.active_flags)put(a,slot.player+4,slot.next);}
    put(a,0x589738,level_player_roster_head());return !a->memory.fault;
}
static int load(GameApplication *a) {
    unsigned i;
    const char *stage="player preparation";
    for(i=0;i<6;++i) {
        GameAddress player=get(a,0x5f0210+i*4);if(!player)goto fail;
        if(get(a,player+0x14) && !game_level_player_prepare(a,player))goto fail;
        if(!progress(a,0x583c98))goto fail;
    }
    stage="level session";if(!progress(a,0x583ca0) || !game_level_session_load(a))goto fail;
    stage="visibility finish";if(!game_level_visibility_finish(a))goto fail;
    stage="imperial initialization";if(get(a,0x5ac678) && !game_level_imperial_initialize(a,get(a,0x5f0220)))goto fail;
    stage="player script";
    if(!progress(a,0x583cb4) || (game_memory_read_u8(&a->memory,0x5acc40) && !game_level_player_script_load(a)))goto fail;
    stage="commands";if(!progress(a,0x583ccc) || !game_level_commands_initialize(a) || !progress(a,0x583ce4))goto fail;
    stage="geometry";if(!game_level_geometry_load(a,0x5acc90))goto fail;
    stage="deployment/radar";if(!game_level_deployment_menu_begin(a) ||
       !game_level_deployment_actions_begin(a) || !game_level_radar_tables_begin(a))goto fail;
    stage="scene audio";if(!game_level_scene_audio_initialize(a))goto fail;
    game_initializer_004d7660(&a->memory);
    stage="progress cleanup";if(!game_level_progress_end(a))goto fail;
    if(!get(a,0x59d828) && !game_menu_media_available(a)) {
        int32_t track=(int32_t)portable_crt_rand_step(&a->memory)%3+2;
        (void)game_menu_music_start(a,(uint32_t)track);
    }
    /*499320 is a verified empty method. */
    stage="display synchronization";
    if(!game_level_display_visibility(a,get(a,0x583608)!=0) || !game_level_display_sync(a,get(a,0x597f84)))goto fail;
    stage="cameras";if(!game_level_cameras_begin(a))goto fail;
    stage="roster";if(!roster(a))goto fail;
    return 1;
fail:
    game_application_log(a,"native level assets failed: %s\n",stage);return 0;
}
int game_level_startup(GameApplication *a) {
    const char *stage="initialization";
    if(!initialize(a))goto fail;
    stage="display";if(!display(a))goto fail;
    stage="factory prewarm";if(!prewarm(a))goto fail;
    stage="level assets and roster";if(!load(a))goto fail;
    stage="clock";if(!game_level_clock_start(a))goto fail;
    game_leaf_00441420(&a->memory);
    put(a,0x603e38,0);put(a,0x604114,0x3eaaaa9f);put(a,0x6040a4,0x3eaaaa9f);put(a,0x60387c,0);
    put(a,0x5ac970,0);put(a,0x6040fc,0x3eaaaac1);game_memory_write_u16(&a->memory,0x603744,3);
    stage="default viewport";if(!game_level_default_viewport(a))goto fail;
    stage="game sound bank";if(get(a,0x59bd98) && !game_menu_sound_bank_load(a,2))goto fail;
    application_frame_begin(get(a,0x5acbe8));return !a->memory.fault;
fail:
    game_application_log(a,"native world startup failed: %s\n",stage);return 0;
}
