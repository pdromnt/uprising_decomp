#ifndef UPRISING_MENU_NETWORK_H
#define UPRISING_MENU_NETWORK_H
#include "application_entry.h"
typedef struct GameNetworkCapabilities {uint32_t flags,hundred_baud,latency;} GameNetworkCapabilities;
/*Native transport owns its interfaces. IDs/data remain fixed-width; no record
 * contains a COM pointer or a host calling convention. */
int game_network_lobby_open(GameApplication *);
int game_network_capabilities(GameApplication *,GameNetworkCapabilities *);
int game_network_player_create(GameApplication *,const char *,const uint8_t[16],uint32_t *);
int game_network_receive(GameApplication *,uint32_t *,uint32_t *,uint8_t *,uint32_t *);
uint32_t game_network_receive_result(GameApplication *,uint32_t *,uint32_t *,uint8_t *,uint32_t *);
typedef struct GameNetworkSystemEvent {uint32_t type,player_type,player_id;} GameNetworkSystemEvent;
int game_network_decode_clock_system(const uint8_t *,uint32_t,GameNetworkSystemEvent *);
int game_network_transport_close(GameApplication *);
int game_network_send(GameApplication *,uint32_t,uint32_t,const uint8_t *,uint32_t);
/*Fixed-width native transport result preserves the clock's HRESULT branches. */
uint32_t game_network_send_result(GameApplication *,uint32_t,uint32_t,const uint8_t *,uint32_t);
int game_network_player_destroy(GameApplication *,uint32_t);
int game_network_decode_system(GameApplication *,const uint8_t *,uint32_t);
int game_network_system_player(GameApplication *,uint32_t,uint32_t,const uint8_t *,uint32_t,const char *);
int game_level_selection_default_row(GameApplication *,uint32_t);
uint32_t game_network_milliseconds(GameApplication *);
void game_network_dispose(GameApplication *);
int game_menu_demo_available(GameApplication *);
int game_menu_demo_start(GameApplication *,unsigned);
int game_network_update_capabilities(GameApplication *,int);
uint32_t game_network_wait(GameApplication *);
int game_network_session_reset(GameApplication *);
int game_network_receive_batch(GameApplication *,uint16_t *);
int game_menu_parameters_reset(GameApplication *);
int game_network_selection_pack(GameApplication *,uint8_t[120],uint32_t);
int game_network_apply_world(GameApplication *,const uint8_t *,uint32_t);
int game_network_publish_roster(GameApplication *,uint32_t,uint32_t,uint32_t);
int game_network_publish_initial_state(GameApplication *,uint32_t,uint32_t);
int game_network_publish_chat(GameApplication *,uint32_t,uint32_t);
int game_network_publish_world(GameApplication *,uint32_t,uint8_t);
int game_network_enable_players(GameApplication *,int);
int game_network_manager_reset(GameApplication *,GameAddress);
int game_network_level_capacity(GameApplication *,GameAddress,uint32_t *);
int game_network_session_initialize(GameApplication *);
int game_network_providers(GameApplication *,GameAddress);
int game_network_provider_open(GameApplication *,const uint8_t[16]);
int game_network_session_open(GameApplication *,int,const char *,const uint8_t[16]);
int game_menu_network_provider_row(GameApplication *,GameAddress,const uint8_t[16],const char *);
int game_network_sessions(GameApplication *,GameAddress,const uint8_t *);
int game_menu_network_session_row(GameApplication *,GameAddress,const uint8_t[16],const char *,const uint8_t *);
int game_menu_network_provider_state(GameApplication *,GameAddress,uint32_t);
int game_menu_network_session_state(GameApplication *,GameAddress,uint32_t);
int game_menu_network_connection_action(GameApplication *,uint32_t,GameAddress,uint32_t);
int game_menu_network_session_tick(GameApplication *);
int game_menu_network_page_state(GameApplication *,GameAddress,uint32_t);
int game_menu_network_page_release(GameApplication *,GameAddress);
int game_menu_network_page_refresh(GameApplication *,GameAddress,int);
int game_menu_network_page_action(GameApplication *,uint32_t,GameAddress,uint32_t);
int game_menu_network_page_tick(GameApplication *,uint32_t);
#endif
