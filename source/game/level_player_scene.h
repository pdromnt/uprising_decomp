#ifndef UPRISING_LEVEL_PLAYER_SCENE_H
#define UPRISING_LEVEL_PLAYER_SCENE_H
#include "application_entry.h"
#include "player_attachment.h"
/*4f4f60 includes placement, transient runtime, rig history and every child. */
int game_level_player_scene_place(GameApplication *,GameAddress,uint32_t,const float[3],const uint16_t[3]);
int game_level_entity_scene_place(GameApplication *,GameAddress,uint32_t,const float[3],const uint16_t[3]);
int game_level_entity_runtime_initialize(GameApplication *,GameAddress);
int game_level_entity_archive_frames(void *,GameAddress,int rendered);
int game_level_entity_archive_publish(void *,GameAddress);
int game_level_player_attachment_submit(GameApplication *,GameAddress,const float[3],const PlayerAttachmentFrame *);
int game_level_player_attachment_update(GameApplication *,GameAddress,const PlayerAttachmentFrame *);
int game_level_raw_auxiliary_fire(GameApplication *,GameAddress,const uint16_t[3],const float[12]);
int game_level_player_contact(GameApplication *,GameAddress,GameAddress,const float[3],const float[3]);
int game_level_player_apply_damage(GameApplication *,GameAddress,uint32_t);
int game_level_player_finalize(GameApplication *,GameAddress);
int game_level_player_frame(GameApplication *,GameAddress);
int game_level_player_death_effect(GameApplication *,GameAddress,GameAddress);
int game_level_player_position_finalize(GameApplication *,GameAddress,uint32_t *);
int game_level_player_event_deliver(GameApplication *,GameAddress,GameAddress,int packet);
int game_level_player_relocate(GameApplication *,GameAddress,const float[2]);
int game_level_player_respawn(GameApplication *,GameAddress,const float[3],uint16_t heading);
/*Workspace handles select bounded native matrix storage owned by the frame. */
int game_level_player_scene_update_weapons(GameApplication *,GameAddress,GameAddress intermediate,GameAddress firing);
#endif
