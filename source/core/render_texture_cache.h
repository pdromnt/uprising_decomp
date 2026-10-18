#ifndef UPRISING_RENDER_TEXTURE_CACHE_H
#define UPRISING_RENDER_TEXTURE_CACHE_H
#include "game_memory.h"
#include "texture_store.h"
/* Keys are existing texture identities, not simulated device-memory addresses. */
typedef struct GameRenderTextureServices {
    void *context;
    int (*statistics)(void *,uint32_t[5]);
    int (*upload)(void *,uint32_t,const uint8_t[4],GameAddress,uint32_t);
    TextureRecord *(*native_record)(void *,uint32_t);
    int (*upload_owned)(void *,uint32_t,const AssetTexture *);
    void (*release)(void *,uint32_t);
    void (*reset)(void *);
} GameRenderTextureServices;
int game_render_texture_cache_begin(GameMemory *,const GameRenderTextureServices *);
int game_render_texture_cache_end(GameMemory *);
int game_render_texture_cache_frame(GameMemory *,const GameRenderTextureServices *);
GameAddress game_render_texture_create(GameMemory *,uint32_t,const uint8_t[4],const uint8_t *,uint32_t);
int game_render_texture_use(GameMemory *,GameAddress,const GameRenderTextureServices *);
int game_render_texture_forget(GameMemory *,GameAddress);
#endif