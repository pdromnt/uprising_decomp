#ifndef UPRISING_LEVEL_RUN_H
#define UPRISING_LEVEL_RUN_H
#include "application_entry.h"
int game_level_run(GameApplication *);
int game_level_end(GameApplication *);
int game_level_clock_pending(GameApplication *,GameAddress,uint32_t *);
int game_level_clock_received_packet(GameApplication *,GameAddress,GameAddress);
int game_level_clock_apply_packet(GameApplication *,GameAddress,GameAddress);
int game_level_clock_refresh(GameApplication *,GameAddress);
int game_level_clock_packet_enqueue(GameApplication *,GameAddress,const uint8_t *,uint32_t);
int game_level_clock_packet_flush(GameApplication *,GameAddress);
int game_level_clock_observe_event(GameApplication *,GameAddress);
int game_level_clock_receive(GameApplication *,GameAddress);
int game_level_clock_raw_packets(GameApplication *,GameAddress);
int game_level_clock_request_missing(GameApplication *,GameAddress);
int game_level_network_tick(GameApplication *);
int game_level_pause_network(GameApplication *);
int game_level_player_checksum(GameApplication *,GameAddress,uint32_t *);
int game_level_checksum_method(GameApplication *,GameAddress,GameAddress,uint32_t *);
int game_level_scene_point_order(GameApplication *,GameAddress,uint32_t,uint32_t,GameAddress,const float[3]);
int game_level_clock_allowed_tick(GameApplication *,GameAddress,uint32_t,uint32_t *);
int game_level_request_end(GameApplication *);
/*Internal game dependencies, still mandatory ordinary-C link boundaries. These
 * must build the actual world and execute/update/render it, never CPU callbacks
 * or successful placeholders. The root cannot link until they are implemented. */
int game_level_interface_prepare(GameApplication *); /*559480*/
int game_level_interface_end(GameApplication *); /*559590*/
int game_level_texture_frame(GameApplication *); /*559600*/
int game_level_texture_use(GameApplication *,GameAddress); /*559630*/
int game_level_scene_prepare(GameApplication *); /*43ae90/43b070*/
int game_level_startup(GameApplication *); /*417bf0*/
int game_level_cameras_begin(GameApplication *); /*4a5dd0*/
int game_level_cameras_end(GameApplication *); /*4a6170*/
int game_level_clock_start(GameApplication *);
int game_level_clock_stop(GameApplication *);
int game_level_clock_destroy(GameApplication *);
int game_level_restore_environment(GameApplication *,GameAddress archive); /*45a560*/
int game_level_restore_player_command(GameApplication *,unsigned slot); /*41a300*/
int game_level_geometry_finalize(GameApplication *); /*4f01c0*/
int game_level_display_end(GameApplication *); /*517830*/
int game_level_display_begin(GameApplication *); /*DirectDraw/viewport initialization*/
int game_application_level_graphics_begin(GameApplication *); /*439930 platform SDK*/
int game_application_level_graphics_end(GameApplication *); /*4399b0 platform SDK*/
int game_level_screen_clear(GameApplication *,int);
int game_level_screen_present(GameApplication *,uint32_t);
int game_level_screen_palette(GameApplication *,GameAddress);
int game_level_screen_image(GameApplication *,GameAddress,uint32_t);
int game_level_display_sync(GameApplication *,uint32_t); /*517240 surface ID*/
int game_level_display_masks_release(GameApplication *);
int game_level_radar_tables_begin(GameApplication *);
int game_level_radar_tables_end(GameApplication *);
int game_level_root_update(GameApplication *,GameAddress);
int game_level_root_pickup_create(GameApplication *,GameAddress,int falling,const float[3],
    uint32_t minimum,uint32_t maximum,uint32_t variant,uint32_t payload,GameAddress *);
int game_level_scene_property(GameApplication *,GameAddress,GameAddress method,uint32_t *);
int game_level_progress_end(GameApplication *);
int game_level_world_load(GameApplication *,GameAddress name); /*4d78f0*/
int game_level_restore_runtime(GameApplication *,GameAddress archive); /*45a9a0*/
int game_level_network_synchronize(GameApplication *,int host); /*5491e0/549400*/
int game_level_game_loop(GameApplication *); /*417590*/
int game_level_cleanup(GameApplication *); /*418200,then460f50; preceding graphics/scene releases live in game_level_run*/
#endif
