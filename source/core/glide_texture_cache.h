#ifndef UPRISING_GLIDE_TEXTURE_CACHE_H
#define UPRISING_GLIDE_TEXTURE_CACHE_H
#include "game_memory.h"
#include "texture_store.h"
/*GPU addresses are fixed-width device values, never host pointers. */
typedef struct GameGlideTextureServices {
    void *context;
    int (*range)(void *,uint32_t *,uint32_t *);
    int (*statistics)(void *,uint32_t[5]);
    int (*upload)(void *,uint32_t,const uint8_t[4],GameAddress,uint32_t);
    TextureRecord *(*native_record)(void *,uint32_t);
    int (*upload_owned)(void *,uint32_t,const AssetTexture *);
} GameGlideTextureServices;
int game_glide_texture_cache_begin(GameMemory *,const GameGlideTextureServices *);
int game_glide_texture_cache_end(GameMemory *);
int game_glide_texture_cache_frame(GameMemory *,const GameGlideTextureServices *);
GameAddress game_glide_texture_create(GameMemory *,uint32_t,const uint8_t[4],const uint8_t *,uint32_t);
GameAddress game_glide_texture_borrow(GameMemory *,uint32_t,const uint8_t[4],GameAddress,uint32_t);
int game_glide_texture_use(GameMemory *,GameAddress,const GameGlideTextureServices *);
int game_glide_texture_forget(GameMemory *,GameAddress);
#endif
