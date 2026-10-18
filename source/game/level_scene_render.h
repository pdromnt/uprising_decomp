#ifndef UPRISING_LEVEL_SCENE_RENDER_H
#define UPRISING_LEVEL_SCENE_RENDER_H
#include "application_entry.h"
#include "texture_store.h"
int game_level_world_present(GameApplication *); /*418880*/
int game_level_camera_subject(GameApplication *,GameAddress *); /*4a4aa0*/
int game_level_scene_overlay(GameApplication *,GameAddress); /*4b2f00*/
int game_level_scene_overlay_flush(GameApplication *); /*4b2fe0*/
int game_level_scene_prepare(GameApplication *);
int game_level_scene_release(GameApplication *);
int game_level_scene_invalidate(GameApplication *); /*43aee0*/
int game_level_group_vehicle_present(GameApplication *,GameAddress); /*4e71b0*/
int game_level_colored_render(GameApplication *,GameAddress); /*43a240*/
int game_level_selection_render(GameApplication *,GameAddress,uint32_t,uint32_t); /*43aef0*/
int game_level_selection_vertices(GameApplication *,GameAddress,const float[][2],const float[][2],unsigned,uint32_t,uint32_t);
int game_application_scene_polygon_begin(GameApplication *,int textured);
int game_application_scene_translucency(GameApplication *,int);
int game_application_texture_update(GameApplication *,const TextureRecord *);
#endif
