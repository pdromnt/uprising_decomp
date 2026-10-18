#ifndef UPRISING_PORTABLE_GAMEPLAY_INPUT_H
#define UPRISING_PORTABLE_GAMEPLAY_INPUT_H
#include "application_entry.h"
#include "event_queue.h"
#include "portable_input_devices.h"
GameKeyboardServices game_input_keyboard_services(GameApplication *);
GameRootEventServices game_input_event_services(GameApplication *);
int game_input_upgrade_feedback(GameApplication *,uint16_t,uint8_t);
int game_input_packet_publish(GameApplication *,GameAddress,GameEventPacket *);
int game_input_byte_publish(GameApplication *,GameAddress,uint8_t,uint8_t);
int game_input_words_publish(GameApplication *,GameAddress,uint8_t,uint16_t,uint16_t);
int game_input_queue_button(GameApplication *,GameAddress,uint8_t);
int game_input_vehicle_keyboard(GameApplication *,uint32_t,uint32_t);
int game_input_overhead_keyboard(GameApplication *,const GameInputEvent *);
int game_input_placement_keyboard(GameApplication *,const GameInputEvent *);
int game_input_game_keyboard(GameApplication *,const GameInputEvent *);
int game_input_game_mouse(GameApplication *,uint32_t,const GameInputEvent *);
int game_input_placement_mouse(GameApplication *,const GameInputEvent *);
int game_input_panel_close(GameApplication *,int);
int game_input_panel_clamp(GameApplication *,int,uint32_t[2]);
int game_input_panel_pointer(GameApplication *,GameAddress,int,int32_t,int32_t,GameAddress *);
int game_input_panel_hover(GameApplication *,GameAddress,int32_t,int32_t,GameAddress *);
int game_input_command_panel_toggle(GameApplication *,int);
int game_input_selector_pair(GameApplication *,uint32_t,uint32_t);
int game_input_navigation_set(GameApplication *,uint32_t,uint32_t,int enabled);
int game_input_selector_hit(GameApplication *,int32_t,int32_t,uint32_t *,uint32_t *,int *);
int game_input_selector_choose(GameApplication *,int32_t,int32_t,int *);
int game_input_panel_action(GameApplication *,uint32_t,uint32_t);
int game_input_nearby_destination(GameApplication *,GameAddress,uint32_t *,uint32_t *,int *);
int game_input_nearby_target(GameApplication *,GameAddress,uint32_t *,uint32_t *,int *);
int game_input_screen_target(GameApplication *,uint32_t *,uint32_t *,int *);
int game_input_world_line(GameApplication *,const float[3],const float[3],float[3],int *);
int game_input_camera_ray(GameApplication *,const float[3],uint16_t,uint16_t,float[3]);
int game_input_camera_terrain_limit(GameApplication *,const float[3],uint16_t,uint16_t,float,float[3],int *);
int game_input_selection_previous(GameApplication *);
int game_input_selection_next(GameApplication *);
int game_input_target_cycle(GameApplication *);
int game_input_scatter_point(GameApplication *,int16_t,uint16_t,float,const float[3],float[3]);
int game_input_ground_target(GameApplication *,uint32_t *,uint32_t *,int *);
int game_input_target_coordinates(GameApplication *,uint16_t[4],int *);
int game_input_root_select(GameApplication *,GameAddress);
int game_input_tracking_camera(GameApplication *);
int game_input_selector_begin(GameApplication *);
int game_input_root_events(GameApplication *,GameAddress,uint32_t);
int game_level_joystick_update(GameApplication *);
int game_input_weight_move(GameApplication *,unsigned,uint32_t);
int game_input_weight_publish(GameApplication *);
#endif
