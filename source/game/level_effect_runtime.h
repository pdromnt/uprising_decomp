#ifndef UPRISING_PORTABLE_LEVEL_EFFECT_RUNTIME_H
#define UPRISING_PORTABLE_LEVEL_EFFECT_RUNTIME_H
#include "application_entry.h"
int game_level_satellite_launch(GameApplication *,GameAddress,float point[3]);
int game_level_scaled_effect_reset(GameApplication *,GameAddress);
int game_level_scaled_effect_contact(GameApplication *,GameAddress,GameAddress,const float[3]);
int game_level_scaled_effect_launch(GameApplication *,GameAddress,uint32_t,const float[3],const uint16_t[3],GameAddress,GameAddress);
#include "effect_initialization.h"
#include "effect_factory.h"
#include "attachment_effect.h"
void game_level_effect_factory_services(GameApplication *,EffectFactoryServices *);
int game_level_presentation_reset(GameApplication *,GameAddress,unsigned);
int game_level_effect_emit(GameApplication *,GameAddress,GameAddress,EffectInitialization *);
int game_level_effect_burst(GameApplication *,GameAddress,GameAddress,uint16_t,int32_t,int32_t);
int game_level_mesh_fragments(GameApplication *,GameAddress,GameAddress,GameAddress,const float[12],const float[3],int32_t,int32_t);
int game_level_fragment_update(GameApplication *,GameAddress);
int game_level_fragment_motion(GameApplication *,GameAddress);
int game_level_position_shot_detonate(GameApplication *,GameAddress);
int game_level_presentation_flight(GameApplication *,GameAddress);
int game_level_ground_producer_update(GameApplication *,GameAddress,GameAddress);
int game_level_ground_producer_reset(GameApplication *,GameAddress);
int game_level_physical_launch(GameApplication *,GameAddress,uint32_t,const float[3],const uint16_t[3],const float[3],const uint16_t[3],GameAddress);
int game_level_aircraft_release(GameApplication *,GameAddress,GameAddress);
int game_level_aircraft_delivery(GameApplication *,GameAddress);
int game_level_fragments_spawn(GameApplication *,GameAddress,int32_t,const float[3]);
int game_level_projectile_update(GameApplication *,GameAddress,GameAddress);
int game_level_mole_steer(GameApplication *,GameAddress);
int game_level_mole_move(GameApplication *,GameAddress);
int game_level_mole_reset(GameApplication *,GameAddress);
int game_level_mole_terrain_contact(GameApplication *,GameAddress,float[3],int *);
int game_level_mole_contact(GameApplication *,GameAddress,GameAddress,const float[3]);
int game_level_mole_launch(GameApplication *,GameAddress,uint32_t,const float[3],const uint16_t[3],GameAddress,GameAddress);
int game_level_mole_present(GameApplication *,GameAddress);
int game_level_mole_update(GameApplication *,GameAddress);
int game_level_mole_controller(GameApplication *,GameAddress);
int game_level_particle_emit(GameApplication *,GameAddress,const float[3],const float[3],float,GameAddress,uint16_t,uint32_t,uint32_t);
int game_level_line_projectile_launch(GameApplication *,GameAddress,uint32_t,const float[3],const float[3],float,GameAddress,GameAddress);
int game_level_motion_seed_update(GameApplication *,GameAddress);
int game_level_projectile_impact(GameApplication *,GameAddress,const float[3],GameAddress,int,GameAddress);
int game_level_trail_update(GameApplication *,GameAddress);
int game_level_particle_start(GameApplication *,GameAddress,uint32_t,const float[3],GameAddress);
int game_level_particle_update(GameApplication *,GameAddress);
int game_level_surface_particles(GameApplication *,GameAddress,const float[3],int32_t,GameAddress);
int game_level_surface_particles_result(GameApplication *,GameAddress,const float[3],int32_t,GameAddress,int *emitted);
int game_level_expansion_start(GameApplication *,GameAddress,uint32_t,const float[3],float,float,int32_t);
int game_level_expansion_move(GameApplication *,GameAddress,const float[3]);
int game_level_expansion_update(GameApplication *,GameAddress);
int game_level_animation_update(GameApplication *,GameAddress);
int game_level_arrival_update(GameApplication *,GameAddress);
int game_level_arrival_reset(GameApplication *,GameAddress);
int game_level_attachment_reset(GameApplication *,GameAddress);
int game_level_attachment_motion(GameApplication *,GameAddress);
int game_level_attachment_killed(GameApplication *,GameAddress,GameAddress);
int game_level_fragment_reset(GameApplication *,GameAddress);
int game_level_attachment_update(GameApplication *,GameAddress);
int game_level_attachment_contact(GameApplication *,GameAddress,GameAddress);
int game_level_attachment_anchors(GameApplication *,GameAddress,int32_t,GameAddress,const float[3]);
int game_level_attachment_emit(GameApplication *,GameAddress,GameAddress,AttachmentEmission *);
int game_level_contact_damage(GameApplication *,GameAddress,GameAddress,int32_t *);
int game_level_fragment_impact(GameApplication *,GameAddress,uint8_t);
int game_level_projectile_attachment_impact(GameApplication *,GameAddress); /*432d70*/
int game_level_physical_impact(GameApplication *,GameAddress,const float[3]); /*40a0e0*/
int game_level_fragment_projectile_split(GameApplication *,GameAddress);
int game_level_spawn_update(GameApplication *,GameAddress);
int game_level_spawn_present(GameApplication *,GameAddress);
int game_level_spawn_contact(GameApplication *,GameAddress,GameAddress,const uint32_t[3],const float[3]);
int game_level_descriptor_transfer(GameApplication *,GameAddress,uint32_t,GameAddress);
int game_level_descent_update(GameApplication *,GameAddress,GameAddress method);
int game_level_class40_killed(GameApplication *,GameAddress,GameAddress);
int game_level_class40_reaction_effect(GameApplication *,GameAddress);
int game_level_arrival_produce(GameApplication *,GameAddress,uint32_t,const float[3],GameAddress);
int game_level_arrival_produce_requested(GameApplication *,GameAddress,uint32_t,const float[3],GameAddress,uint32_t,uint16_t heading);
#endif
