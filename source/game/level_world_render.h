#ifndef UPRISING_LEVEL_WORLD_RENDER_H
#define UPRISING_LEVEL_WORLD_RENDER_H
#include "application_entry.h"
#include "world_renderer.h"
int game_level_world_render(GameApplication *,GameAddress); /*43a630*/
int game_level_projected_render(GameApplication *,ProjectedQueueKind); /*5051a0/5052d0/505450*/
/*Real platform services. Full-width SDK pointers are private to the platform. */
int game_application_world_begin(GameApplication *);
void game_application_world_antialias(GameApplication *,int enabled);
int game_application_world_state(GameApplication *,WorldRenderState,int);
int game_application_world_palette(GameApplication *,const uint32_t *,unsigned,unsigned,int);
int game_application_world_ncc(GameApplication *,const uint8_t[112]);
int game_level_graphics_initialize(GameApplication *); /*5036f0*/
int game_level_projected_texture_release(GameApplication *);
int game_application_world_point(GameApplication *,const GameRenderVertex *);
int game_application_render_texture(GameApplication *,GameAddress);
int game_application_projected_begin(GameApplication *,ProjectedQueueKind);
int game_application_projected_end(GameApplication *,ProjectedQueueKind);
#endif
