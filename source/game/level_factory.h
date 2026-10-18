#ifndef UPRISING_PORTABLE_LEVEL_FACTORY_H
#define UPRISING_PORTABLE_LEVEL_FACTORY_H
#include "application_entry.h"
#include "projectile_factory.h"
#include "presentation_state_copy.h"
GameAddress game_level_factory_template(GameApplication *,uint8_t,uint16_t);
int game_level_factory_fill(GameApplication *,uint8_t,uint16_t,int32_t);
GameAddress game_level_factory_take(GameApplication *,uint8_t,uint16_t);
int game_level_factory_destroy(GameApplication *,GameAddress);
/*447dd0: templates, ready lists and source caches retire through native owners. */
int game_level_factories_release(GameApplication *);
int game_level_spawn_factory_shutdown(GameApplication *);
int game_level_spawn_reset(GameApplication *,GameAddress);
int game_level_spawn_variant_load(GameApplication *,uint32_t);
int game_level_spawn_variants_shutdown(GameApplication *);
/*Distinct remaining asset/entity families are genuine internal dependencies.
 * Native typed descriptions transfer owned inputs to the existing factories. */
int game_level_projectile_describe(GameApplication *,const ProjectileFamily *,uint16_t,PresentationInstanceDefinition *);
void game_level_projectile_factory_services(GameApplication *,ProjectileFactoryServices *);
int game_level_presentation_describe(GameApplication *,const GamePresentationFactoryFamily *,uint16_t,float[3],GamePresentationDefinition *);
uint32_t game_level_texture_from_indexed(GameApplication *,const uint8_t *,uint32_t,uint32_t,uint32_t);
int game_level_factory_release_pool_member(GameApplication *,GameAddress);
int game_level_factory_detach_packet_chain(GameApplication *,GameAddress);
GameAddress game_level_vehicle_factory(GameApplication *,uint8_t,uint16_t,unsigned,int32_t);
int game_level_vehicle_factory_destroy(GameApplication *,GameAddress);
int game_level_vehicle_factory_shutdown(GameApplication *);
GameAddress game_level_infantry_factory(GameApplication *,uint16_t,unsigned,int32_t);
GameAddress game_level_aircraft_factory(GameApplication *,uint16_t,unsigned,int32_t);
GameAddress game_level_class29_factory(GameApplication *,uint16_t,unsigned,int32_t);
int game_level_class29_destroy(GameApplication *,GameAddress);
GameAddress game_level_class40_factory(GameApplication *,uint16_t,unsigned,int32_t);
int game_level_class40_destroy(GameApplication *,GameAddress);
int game_level_class40_reset(GameApplication *,GameAddress);
int game_level_class40_geometry(GameApplication *,GameAddress,unsigned);
int game_level_class40_place(GameApplication *,GameAddress,uint32_t,const float[3],const uint16_t[3]);
int game_level_class40_move(GameApplication *,GameAddress,float,float);
int game_level_class40_timed_gun(GameApplication *,GameAddress);
int game_level_class40_profile(GameApplication *,GameAddress,GameAddress method);
int game_level_class40_fire(GameApplication *,GameAddress,GameAddress target);
int game_level_class40_control(GameApplication *,GameAddress);
int game_level_class40_combat(GameApplication *,GameAddress);
int game_level_class40_reaction(GameApplication *,GameAddress);
int game_level_class40_guns_build(GameApplication *,GameAddress,GameAddress catalog);
int game_level_class40_notice(GameApplication *,GameAddress,uint32_t);
int game_level_class29_place(GameApplication *,GameAddress,uint32_t,const float[3],const uint16_t[3]);
int game_level_class29_move(GameApplication *,GameAddress,float,float);
int game_level_class29_ground(GameApplication *,GameAddress);
int game_level_class29_steer(GameApplication *,GameAddress);
int game_level_aircraft_destroy(GameApplication *,GameAddress);
int game_level_aircraft_reset(GameApplication *,GameAddress);
int game_level_aircraft_destination(GameApplication *,GameAddress,const float[3]);
int game_level_aircraft_move(GameApplication *,GameAddress,float,float);
int game_level_aircraft_place(GameApplication *,GameAddress,uint32_t,const float[3],const uint16_t[3]);
int game_level_aircraft_activate(GameApplication *,GameAddress,uint32_t,const float[3],const uint16_t[3],const float[2],GameAddress);
int game_level_infantry_destroy(GameApplication *,GameAddress);
int game_level_infantry_reset(GameApplication *,GameAddress);
int game_level_infantry_geometry(GameApplication *,GameAddress,unsigned);
int game_level_infantry_place(GameApplication *,GameAddress,uint32_t,const float[3],const uint16_t[3]);
int game_level_vehicle_reset(GameApplication *,GameAddress);
int game_level_vehicle_descriptor_retire(GameApplication *,GameAddress);
GameAddress game_level_carrier_factory(GameApplication *,uint16_t,unsigned,int32_t);
int game_level_carrier_destroy(GameApplication *,GameAddress);
int game_level_carrier_place(GameApplication *,GameAddress,uint32_t,const float[3],const uint16_t[3]);
int game_level_carrier_update(GameApplication *,GameAddress);
int game_level_carrier_reset(GameApplication *,GameAddress);
GameAddress game_level_station_payload_factory(GameApplication *,uint8_t,uint16_t,unsigned,int32_t);
int game_level_station_payload_destroy(GameApplication *,GameAddress);
int game_level_station_payload_refresh(GameApplication *,GameAddress,GameAddress);
int game_level_station_payload_update(GameApplication *,GameAddress,GameAddress);
int game_level_station_payload_reset(GameApplication *,GameAddress,int);
int game_level_station_payload_notice(GameApplication *,GameAddress,GameAddress,uint32_t);
int game_level_scene_notice(GameApplication *,GameAddress,GameAddress,uint32_t);
int game_level_scene_reset(GameApplication *,GameAddress,GameAddress);
int game_level_group_reaction(GameApplication *,GameAddress,GameAddress,int32_t);
int game_level_group_vehicle_reset(GameApplication *,GameAddress);
int game_level_group_vehicle_restore_components(GameApplication *,GameAddress);
int game_level_infantry_guns_build(GameApplication *,GameAddress,GameAddress);
int game_level_aircraft_archive_gun(GameApplication *,GameAddress);
int game_level_station_payload_death(GameApplication *,GameAddress,GameAddress);
int game_level_vehicle_place(GameApplication *,GameAddress,uint32_t,const float[3],const uint16_t[3],GameAddress,uint32_t);
#endif
