#ifndef UPRISING_TEXTURE_STORE_H
#define UPRISING_TEXTURE_STORE_H
#include "asset_texture.h"
#include "texture_residency.h"
typedef struct TextureRecord {
    uint32_t id,key,flags,last_use,address;
    uint32_t cache_a,cache_b;int cache_valid;
    AssetTexture image;
    struct TextureRecord *next;
    struct TextureRecord *key_next,*id_next;
} TextureRecord;
enum { TEXTURE_STORE_BUCKETS=2048 };
typedef struct TextureStore {
    TextureRecord *first,*by_key[TEXTURE_STORE_BUCKETS],
        *by_id[TEXTURE_STORE_BUCKETS];
    uint32_t next_id;
    size_t count;
} TextureStore;
/* key is an opaque caller identity; no address dereference occurs in the core. */
TextureRecord *texture_store_find(TextureStore *,uint32_t key);
TextureRecord *texture_store_find_id(TextureStore *,uint32_t id);
TextureRecord *texture_store_take(TextureStore *,uint32_t key,uint32_t flags,AssetTexture *);
int texture_store_write_indexed(TextureRecord *,const uint8_t *,size_t,uint32_t a,uint32_t b,int force);
/* Copy a bounded indexed image into the existing device-sized pixel owner. */
int texture_store_resize_indexed(TextureRecord *,const uint8_t *,size_t,uint32_t width,uint32_t height);
void texture_store_release(TextureStore *);

int texture_store_remove(TextureStore *,uint32_t key);
typedef int (*TextureUpload)(void *,uint32_t address,const TextureRecord *);
/* Native records are authoritative. Optional legacy callbacks handle only owners
   absent from the store while the shared residency ring still contains them. */
int texture_store_upload(TextureStore *,TextureResidency *,uint32_t key,uint32_t frame,
    TextureUpload,void *upload_context,TextureLastUse,TextureEvict,void *legacy_context,
    uint32_t *uploaded_bytes);
#endif
