#ifndef UPRISING_PORTABLE_LEVEL_LOAD_H
#define UPRISING_PORTABLE_LEVEL_LOAD_H
#include "application_entry.h"
#include "level_player_slots.h"
int game_level_session_load(GameApplication *); /*4a8100,distinct from4d78f0*/
int game_level_session_release(GameApplication *,LevelReleaseMode,LevelReleaseAction);
/*Still executable internal work. None is a successful placeholder or CPU
 *adapter. The startup must initialize these before loading the section chain. */
int game_level_terrain_load(GameApplication *,const char *,int32_t);
int game_level_terrain_release(GameApplication *);
#define GAME_TERRAIN_CELLS UINT32_C(0xfa000000)
#define GAME_TERRAIN_COARSE UINT32_C(0xfa080000)
#define GAME_TERRAIN_MATERIALS UINT32_C(0xfa090000)
int game_level_terrain_read(GameApplication *,GameAddress,void *,size_t);
int game_level_terrain_write(GameApplication *,GameAddress,const void *,size_t);
int game_level_slot_place(void *,uint32_t,uint32_t,const LevelPlayerSlot *);
int game_level_terrain_depth_begin(GameApplication *); /*50dd50*/
int game_level_terrain_depth_end(GameApplication *); /*50e050*/
int game_level_loading_text(GameApplication *,GameAddress); /*5188b0*/
int game_level_visibility_reset(GameApplication *,GameAddress); /*456c50*/
int game_level_visibility_finish(GameApplication *); /*4599d0*/
int game_level_imperial_initialize(GameApplication *,GameAddress); /*47a340*/
int game_level_imperial_release(GameApplication *,GameAddress); /*4b3e30/4c0b60*/
int game_level_resource_clusters_release(GameApplication *);
int game_level_resource_cluster_update(GameApplication *,GameAddress);
int game_level_resource_cluster_accumulate(GameApplication *,GameAddress,uint8_t,int32_t,int32_t,int32_t,uint32_t);
int game_level_visibility_pool_reset(GameApplication *,GameAddress); /*457b80*/
int game_level_work_grid_reset(GameApplication *,GameAddress); /*4fe870*/
int game_level_navigation_routes_build(GameApplication *,GameAddress); /*4c8450*/
int game_level_navigation_routes_release(GameApplication *,GameAddress);
int game_level_palette_load(GameApplication *,GameAddress,GameAddress); /*4d81e0*/
int game_level_palette_initialize(GameApplication *); /*4d8310*/
int game_level_palette_map(GameApplication *); /*4d8590*/
int game_level_transition_assets_release(GameApplication *);
int game_level_input_regions_initialize(GameApplication *); /*51bcc0 bitmap descriptor*/
int game_level_input_regions_load(GameApplication *,GameAddress); /*51bd40*/
int game_level_input_regions_release(GameApplication *); /*51bdc0*/
int game_level_player_scene_destroy(GameApplication *,GameAddress);
#endif
