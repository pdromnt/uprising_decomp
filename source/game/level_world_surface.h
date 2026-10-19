#ifndef UPRISING_LEVEL_WORLD_SURFACE_H
#define UPRISING_LEVEL_WORLD_SURFACE_H
#include "level_run.h"
#include "model_assets.h"
#include "gameplay_geometry.h"
int game_level_world_surface_release(GameApplication *);
int game_level_world_surface_draw(GameApplication *);
typedef struct GameSkyFrame {
    const char *name;
    float view[12],center[2],scale,vertical,speed;
} GameSkyFrame;
int game_application_sky_begin(GameApplication *,const GameSkyFrame *);
int game_application_sky_end(GameApplication *);
typedef struct GameRemoteSkyFrame {
    const char *name;
    int32_t bounds[4];
    float yaw,scale,speed;
} GameRemoteSkyFrame;
int game_application_remote_sky(GameApplication *,const GameRemoteSkyFrame *);
/* Bound the actual 3D submissions; screen artwork and final overlay queues
 * must not replace the temporal renderer's unquantized world depth. */
int game_application_scene_depth(GameApplication *,int enabled);
int game_application_surface_begin(GameApplication *);
int game_application_surface_clamp(GameApplication *,int);
int game_application_surface_texture(GameApplication *,const TextureRecord *);
int game_application_surface_polygon(GameApplication *,const GameRenderVertex *,unsigned);
int game_application_surface_motion_polygon(GameApplication *,const GameRenderVertex *,unsigned,const uint32_t *);
#endif
