#include "level_run.h"
#include "level_settings.h"
#include "application_archives.h"
#include "gameplay_state.h"
#include "gameplay_leaves.h"
#include "game_initializers.h"
#include "tagged_scalar_stream.h"
#include "ui_frame.h"
#include "level_world_surface.h"
#include "level_geometry.h"
#include "level_scene_render.h"
#include "level_player_resources.h"
#include "level_player_factory.h"
#include "level_factory.h"
#include "menu_widgets.h"
#include "menu_sound.h"
#include "menu_network.h"
#include "menu_pause.h"
#include "scene_projection_storage.h"
#include "level_effects.h"
#include "level_player_slots.h"
extern int game_application_renderer_warmup(GameApplication *);
#include <stdio.h>
#include <string.h>
extern int game_level_selection_load(GameApplication *);
extern int game_menu_selection_end(GameApplication *);
extern uint32_t game_menu_milliseconds(GameApplication *);
extern int game_menu_input_reset(GameApplication *); /*004d0130: retire native input devices. */
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
static int string_read(GameApplication *a,GameAddress p,char *text,size_t capacity) {
    size_t i;for(i=0;i<capacity;++i){text[i]=(char)game_memory_read_u8(&a->memory,p+(uint32_t)i);if(!text[i])return !a->memory.fault;}return 0;
}
static int stream_read(void *context,uint8_t *bytes,size_t width,size_t count,uint32_t *elements) {
    ResourceFile *file=context;size_t read;
    if(!file || !width || count>SIZE_MAX/width || count>UINT32_MAX)return 0;
    read=resource_file_read(file,bytes,width*count);*elements=(uint32_t)(read/width);return 1;
}
/*45a6d0 consumes the same four existing settings owners through the already
 * compared tagged-record algorithms. FILE pointers never enter the records. */
static int restore_settings(GameApplication *a,GameAddress archive) {
    ResourceFile *file=game_application_archive_file(a,archive);uint32_t marker=get(a,0x63c964);
    TaggedRecordStreamServices services={file,stream_read,NULL,NULL};int ok;
    if(!file)return 0;
    ok=gameplay_state_load_settings(&a->memory,&marker,&services);put(a,0x63c964,marker);
    return ok && !a->memory.fault;
}
/*45a9a0: restore simulation ticks and rebase wall-clock pacing. Fixed-width
 *wrapping arithmetic is independent of the host timer and pointer widths. */
int game_level_restore_runtime(GameApplication *a,GameAddress archive){
    ResourceFile *file=game_application_archive_file(a,archive);
    TaggedRecordStreamServices services={file,stream_read,NULL,NULL};
    uint32_t tick=get(a,0x5ac6ac),marker=get(a,0x63c964),header,elements;uint8_t bytes[4];unsigned i;
    if(!file || a->memory.fault)return 0;
    for(i=0;i<4;i++)bytes[i]=(uint8_t)(tick>>(i*8));
    if(!tagged_record_stream_read_elements(&marker,bytes,4,1,&header,&elements,&services))return 0;
    tick=(uint32_t)bytes[0]|(uint32_t)bytes[1]<<8|(uint32_t)bytes[2]<<16|(uint32_t)bytes[3]<<24;
    put(a,0x63c964,marker);put(a,0x59d4b8,header);put(a,0x5ac6ac,tick);
    put(a,0x5ac90c,game_menu_milliseconds(a)-((tick+1u)<<5));return !a->memory.fault;
}
static int mask(void *a,int enabled){return game_level_screen_clear(a,enabled);}
static int present(void *a,uint32_t interval){return game_level_screen_present(a,interval);}
static int palette(void *a,GameAddress address){return game_level_screen_palette(a,address);}
static int fullscreen(void *a,GameAddress address,uint32_t stride){return game_level_screen_image(a,address,stride);}
/*004176e0: actual application-to-level route. Recover ordinary arguments and
 * returns, retain authoritative records and the archive owner, and make all live
 * world/render/network/loop dependencies explicit. Root control is partial work
 * until those connected dependencies execute; no CPU fallback is permitted. */
int game_level_run(GameApplication *a) {
    GameMemory *m=&a->memory;GameAddress archive=0,settings,player;char path[512];uint8_t bytes[4]={0};
    uint32_t count=0,now,offset,cleanup_started=0;unsigned i;int ok=0,prepared=0;
    const char *stage="archive/player slots";
    put(a,0x5ac6b0,1);
    if(!get(a,0x5eff04) && game_memory_read_u8(m,0x5e1248)) {
        if(!string_read(a,0x5e1248,path,sizeof(path)))goto done;
        archive=game_application_archive_open(a,path,0);put(a,0x5acc88,archive);
        if(archive && strstr(path,".sav")) {
            ResourceFile *file=game_application_archive_file(a,archive);
            (void)resource_file_read(file,bytes,4);
            offset=(uint32_t)bytes[0]|(uint32_t)bytes[1]<<8|(uint32_t)bytes[2]<<16|(uint32_t)bytes[3]<<24;
            if(!resource_file_seek(file,(int32_t)offset,0))goto done;
        }
        game_memory_write_u8(m,0x5e1248,0);
    }else put(a,0x5acc88,0);
    for(i=0;i<4;++i){player=get(a,0x5f0210+i*4);if(!player)goto done;if(get(a,player+0x468))++count;}
    put(a,0x5ac6b4,count);
    if(count==1 && get(a,0x5eff04)) {
        put(a,0x5eff04,0);
        if(!game_network_transport_close(a))goto done;
    }
    stage="selected level settings";
    if(!game_level_selection_load(a))goto done;
    settings=get(a,0x5943c8);if(!settings)goto done;
    put(a,0x594d68,get(a,settings+0x48));put(a,0x594d64,get(a,settings+0x4c));put(a,0x594d44,get(a,settings+0x44));
    if(archive){if(!restore_settings(a,archive))goto done;}
    else if(!game_level_player_configuration_publish(a))goto done;
    put(a,0x624f8c,8);
    for(i=0;i<4;++i){player=get(a,0x5f0210+i*4);if(get(a,player+0x468))game_leaf_004765f0(m,player);}
    if(get(a,0x5ac678))game_leaf_004765f0(m,get(a,0x5f0220));
    if(get(a,0x5ac67c))game_leaf_004765f0(m,get(a,0x5f0224));
    stage="player slot bindings";
    if(!game_level_player_slot_bindings(a))goto done;
    stage="interface/scene preparation";
    if(m->fault || !game_level_interface_prepare(a) || !game_level_scene_prepare(a))goto done;
    game_initializer_004d8ae0(m);put(a,0x5840f4,1);prepared=1;
    stage="world startup";
    if(m->fault || !game_level_startup(a))goto done;
    if(archive) {
        stage="restore environment";
        if(!game_level_restore_environment(a,archive))goto done;
        player=get(a,0x5f0208);if(!player)goto done;
        stage="restore player commands";
        for(i=0;i<9;++i)if(get(a,player+0x5ac+i*8) && !game_level_restore_player_command(a,i))goto done;
    }
    stage="geometry finalization";if(!game_level_geometry_finalize(a))goto done;
    stage="display retirement";if(!game_level_display_end(a))goto done;
    stage="display initialization";if(!game_level_display_begin(a))goto done;
    stage="world loading";if(!game_level_world_load(a))goto done;
    stage="renderer warmup";if(!game_application_renderer_warmup(a))goto done;
    stage="progress cleanup";if(!game_level_progress_end(a))goto done;
    stage="display transition";
    {GameUiFrameOps ops={a,mask,present,palette,fullscreen};
        if(!game_ui_frame_run(m,get(a,0x597f88)?GAME_UI_FRAME_PALETTE_TRANSITION:GAME_UI_FRAME_CLEAR,&ops))goto done;}
    put(a,0x5840f0,1);put(a,0x5840f8,1);put(a,0x624f8c,9);
    stage="runtime clock/synchronization";
    if(archive) {
        if(!game_level_restore_runtime(a,archive))goto done;
        if(!game_application_archive_close(a,archive))goto done;
        archive=0;put(a,0x5acc88,0);
    }else {
        if(get(a,0x5eff04)) {
            /*417986 skips this temporary sky suppression for single-player.
             *Keep the startup setting; only network synchronization clears it. */
            if(!game_level_display_sync(a,0))goto done;
            if(!game_level_network_synchronize(a,get(a,0x5f0150)!=0))goto done;
            if(!game_level_display_sync(a,get(a,0x597f84)))goto done;
            while((uint32_t)(game_menu_milliseconds(a)-get(a,0x5ac90c))<get(a,0x639968))game_application_sleep(a,1);
            put(a,0x5ac6ac,1);put(a,0x5ac90c,get(a,0x5ac90c)+get(a,0x639968)-64);
        }else {
            now=game_menu_milliseconds(a);put(a,0x5ac90c,now);put(a,0x5ac6ac,(game_menu_milliseconds(a)-now)>>5);
        }
        if(!game_menu_sound(a,10))goto done;
    }
    put(a,0x5acbe8,get(a,0x5ac6ac));put(a,0x583604,0);
    /*The existing sound owner supplies the real looping startup voice. Its
     * original volume is0.88, not the full-volume convenience loop interface. */
    stage="startup sound";
    if(!game_menu_game_start_sound(a))goto done;
    game_level_pause_reset(a);
    stage="gameplay loop";
    put(a,0x624f8c,10);ok=game_level_game_loop(a);
done:
    if(!ok)game_application_log(a,"native level failed: %s\n",stage);
    if(prepared) {
        SceneProjectionStorageStats pages;LevelEffectsStats effects;LevelPlayerSlotsStats members;
        char detail[192];
        cleanup_started=game_menu_milliseconds(a);
        scene_projection_storage_stats(&pages);level_effects_stats(&effects);level_player_slots_stats(&members);
        snprintf(detail,sizeof(detail),"pages=%u effects=%u members=%u",pages.live,effects.live,members.live_members);
        game_application_log(a,"native level exit begin: %s\n",detail);
        put(a,0x5840f8,0);put(a,0x5840f4,0);put(a,0x5840f0,0);
        if(!game_application_level_graphics_end(a)){game_application_log(a,"native teardown failed: %s\n","graphics");ok=0;}
        put(a,0x624f8c,11);
        if(get(a,0x5910a4) && !game_menu_ingame_pause_state(a,4))ok=0;
        if(!game_level_progress_end(a))ok=0;
        if(!game_level_scene_release(a)){game_application_log(a,"native teardown failed: %s\n","scenes");ok=0;}
        if(!game_level_geometry_release(a)){game_application_log(a,"native teardown failed: %s\n","geometry");ok=0;}
        if(!game_level_cleanup(a)){game_application_log(a,"native teardown failed: %s\n","level cleanup");ok=0;}
        if(!game_level_player_factory_shutdown(a)){game_application_log(a,"native teardown failed: %s\n","player factories");ok=0;}
        if(!game_level_vehicle_factory_shutdown(a)){game_application_log(a,"native teardown failed: %s\n","vehicle factories");ok=0;}
        if(!game_level_player_command_models_release(a)){game_application_log(a,"native teardown failed: %s\n","command models");ok=0;}
        if(!game_level_radar_tables_end(a))ok=0;
        if(!game_level_display_masks_release(a))ok=0;
    }
    if(!game_level_world_surface_release(a))ok=0;
    if(!game_level_interface_end(a))ok=0;
    if(archive){if(!game_application_archive_close(a,archive))ok=0;put(a,0x5acc88,0);}
    if(prepared){
        SceneProjectionStorageStats pages;LevelEffectsStats effects;LevelPlayerSlotsStats members;
        char detail[192];
        scene_projection_storage_stats(&pages);level_effects_stats(&effects);level_player_slots_stats(&members);
        snprintf(detail,sizeof(detail),"elapsed_ms=%u pages=%u effects=%u members=%u ok=%d",
            game_menu_milliseconds(a)-cleanup_started,pages.live,effects.live,members.live_members,ok && !m->fault);
        game_application_log(a,"native level exit end: %s\n",detail);
    }
    return ok && !m->fault;
}
int game_level_end(GameApplication *a) {
    return game_menu_selection_end(a) && game_menu_input_reset(a) && game_level_display_end(a) && game_level_display_masks_release(a);
}
