#ifndef UPRISING_RADAR_COMPOSITOR_H
#define UPRISING_RADAR_COMPOSITOR_H

#include "game_memory.h"
#define RADAR_MAP_BASE 0xfc000000u
#define RADAR_MAP_SLOT_BYTES 0x00400000u
#define RADAR_MAP_END (RADAR_MAP_BASE+2u*RADAR_MAP_SLOT_BYTES)
/* The two existing map buffers have one native owner. These IDs never encode
 * host pointers. Bounded views also serve remaining persistence readers. */
int game_radar_map_initialize(GameMemory *,const uint8_t *source,size_t bytes,
    uint32_t width,uint32_t height);
int game_radar_map_release(GameMemory *);
uint8_t *game_radar_map_borrow(GameAddress,size_t);
int game_radar_map_read(GameAddress,void *,size_t);
int game_radar_map_write(GameAddress,const void *,size_t);
int game_radar_reveal(GameMemory *,GameAddress position,float radius);
typedef struct GameRadarArchiveServices {
    void *context;
    int (*write_record)(void *,uint32_t tag,const uint8_t *,size_t bytes);
    int (*read_record)(void *,uint8_t *,size_t bytes);
} GameRadarArchiveServices;
/* Existing map/collection records remain the owners. The temporary image is
 * only the original save format, never a competing runtime state copy. */
int game_radar_map_save(GameMemory *,const GameRadarArchiveServices *);
int game_radar_map_load(GameMemory *,const GameRadarArchiveServices *);

typedef struct GameRadarInput {
    int32_t screen_x;
    int32_t screen_y;
    uint16_t heading;
    float source_x;
    float source_y;
    float source_scale;
} GameRadarInput;

typedef struct GameRadarOps {
    void *context;
    int (*draw)(void *context, GameAddress descriptor,
        int32_t center_source_x, int32_t center_source_y);
} GameRadarOps;
typedef struct GameRadarGeometry {
    float positions[32][2],uv[32][2];
    int32_t source_x,source_y;
} GameRadarGeometry;

typedef enum GameRadarResult {
    GAME_RADAR_FAULT = 0,
    GAME_RADAR_OK = 1,
    GAME_RADAR_SKIPPED = 2
} GameRadarResult;

GameRadarResult game_ui_compose_radar(GameMemory *memory,
    const GameRadarInput *input, GameAddress scratch_vertices,
    const GameRadarOps *ops);
/* Optional bounded borrowed target pixels; no mirror or per-pixel publication.
 * Caller retains ownership and supplies its complete current buffer size. */
GameRadarResult game_ui_compose_radar_pixels(GameMemory *,const GameRadarInput *,
    GameAddress scratch_vertices,const GameRadarOps *,uint8_t *pixels,size_t pixel_bytes);
/*Shipping geometry is local native data. The legacy packet callback remains
 *available only for existing independent/original consumers. */
GameRadarResult game_ui_compose_radar_geometry(GameMemory *,const GameRadarInput *,
    uint8_t *pixels,size_t pixel_bytes,GameRadarGeometry *);

#endif
