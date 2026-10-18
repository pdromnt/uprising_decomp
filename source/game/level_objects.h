#ifndef UPRISING_PORTABLE_LEVEL_OBJECTS_H
#define UPRISING_PORTABLE_LEVEL_OBJECTS_H
#include "application_entry.h"
int game_level_objects_load(GameApplication *,const char *,int32_t,uint32_t *);
/*Distinct placement families retain real C arguments, not guest call words. */
int game_level_object_place(GameApplication *,GameAddress,uint32_t,const float[3],uint32_t packed_angle);
int game_level_object_place_angles(GameApplication *,GameAddress,uint32_t,const float[3],const uint16_t[3]);
int game_level_object_place_pose(GameApplication *,GameAddress,uint32_t,const float[3],const uint16_t[3],int controller);
int game_level_object_place_common(GameApplication *,GameAddress,uint32_t,const float[3],const uint16_t[3],int controller);
int game_level_projectile_launch(GameApplication *,GameAddress,uint32_t,const float[3],const uint16_t[3],GameAddress,GameAddress,uint32_t method,float strength);
int game_level_object_footprint(GameApplication *,GameAddress,float[3],uint16_t);
int game_level_scene_object_place(GameApplication *,GameAddress,uint32_t,const float[3],const uint16_t[3]);
int game_level_scene_object_world(GameApplication *,GameAddress);
int game_level_object_archive_frames(GameApplication *,GameAddress,int rendered);
int game_level_position_shot_place(GameApplication *,GameAddress,uint32_t,const float[3],const uint16_t[3],GameAddress,uint8_t);
int game_level_teleporters_load(GameApplication *,const char *,int32_t,uint32_t *);
int game_level_teleporter_destination_place(GameApplication *,GameAddress,uint32_t,float[3],const float[3],GameAddress);
int game_level_teleporter_bind(GameApplication *,GameAddress,GameAddress);
int game_level_teleporter_update(GameApplication *,GameAddress);
int game_level_teleporter_destroy(GameApplication *,GameAddress);
int game_level_citadels_load(GameApplication *,const char *,int32_t,uint32_t *);
int game_level_citadel_destroy(GameApplication *,GameAddress);
#endif
