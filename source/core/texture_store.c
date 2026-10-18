#include "texture_store.h"
#include <stdlib.h>
#include <string.h>
static unsigned texture_bucket(uint32_t key){
    return (key*2654435761u)>>(32u-11u);
}
TextureRecord *texture_store_find(TextureStore *store,uint32_t key) {
    TextureRecord *record;
    for(record=store->by_key[texture_bucket(key)];record;
        record=record->key_next)if(record->key==key)return record;
    return NULL;
}
TextureRecord *texture_store_find_id(TextureStore *store,uint32_t id) {
    TextureRecord *record;
    for(record=store->by_id[texture_bucket(id)];record;
        record=record->id_next)if(record->id==id)return record;
    return NULL;
}
TextureRecord *texture_store_take(TextureStore *store,uint32_t key,uint32_t flags,AssetTexture *image) {
    TextureRecord *record;
    if(!key || !image || !image->pixels || texture_store_find(store,key))return NULL;
    if(!store->next_id)store->next_id=0xfd000001u;
    if(store->next_id>=0xfe000000u)return NULL;
    record=calloc(1,sizeof(*record));if(!record)return NULL;
    record->id=store->next_id++;record->key=key;record->flags=flags;record->image=*image;
    memset(image,0,sizeof(*image));
    record->next=store->first;store->first=record;
    {unsigned bucket=texture_bucket(key);
     record->key_next=store->by_key[bucket];store->by_key[bucket]=record;}
    {unsigned bucket=texture_bucket(record->id);
     record->id_next=store->by_id[bucket];store->by_id[bucket]=record;}
    ++store->count;return record;
}
void texture_store_release(TextureStore *store) {
    while(store->first){TextureRecord *record=store->first;store->first=record->next;asset_texture_release(&record->image);free(record);}
    memset(store->by_key,0,sizeof(store->by_key));
    memset(store->by_id,0,sizeof(store->by_id));
    store->count=0; /* IDs remain monotonic so stale references cannot alias a new image. */
}

int texture_store_write_indexed(TextureRecord *record,const uint8_t *pixels,size_t size,
    uint32_t a,uint32_t b,int force) {
    if(!record || !pixels || record->image.format!=5 || size!=record->image.size)return -1;
    if(!force && record->cache_valid && record->cache_a==a && record->cache_b==b)return 0;
    memcpy(record->image.pixels,pixels,size);record->cache_a=a;record->cache_b=b;record->cache_valid=1;return 1;
}
int texture_store_resize_indexed(TextureRecord *record,const uint8_t *pixels,size_t size,
    uint32_t width,uint32_t height) {
    uint32_t x,y,w,h;uint8_t *target;
    if(!record || !pixels || !width || !height || record->image.format!=5 ||
       !record->image.pixels || (uint64_t)width*height>size)return 0;
    w=record->image.render_width;h=record->image.render_height;target=record->image.pixels;
    if(!w || !h || (uint64_t)w*h>record->image.size)return 0;
    if(width==w && height==h){
        if(target!=pixels)memcpy(target,pixels,(size_t)w*h);
        return 1;
    }
    /*The device image stays power-of-two. Nearest sampling keeps indexed
     *palette entries intact, including the transparent/background index. */
    for(y=0;y<h;y++){
        size_t row=(size_t)((uint64_t)y*height/h)*width;
        for(x=0;x<w;x++)target[(size_t)y*w+x]=pixels[row+(uint32_t)((uint64_t)x*width/w)];
    }
    return 1;
}

int texture_store_remove(TextureStore *store,uint32_t key) {
    TextureRecord **link=&store->first;
    while(*link){TextureRecord *record=*link;if(record->key==key){
        TextureRecord **indexed=&store->by_key[texture_bucket(record->key)];
        TextureRecord **by_id=&store->by_id[texture_bucket(record->id)];
        while(*indexed && *indexed!=record)indexed=&(*indexed)->key_next;
        while(*by_id && *by_id!=record)by_id=&(*by_id)->id_next;
        if(!*indexed || !*by_id)return 0;
        *indexed=record->key_next;*by_id=record->id_next;
        *link=record->next;asset_texture_release(&record->image);
        free(record);--store->count;return 1;
    }link=&record->next;}
    return 0;
}

typedef struct UploadOwners {
    TextureStore *store;TextureLastUse last_use;TextureEvict evict;void *legacy;
} UploadOwners;
static uint32_t upload_last_use(void *context,uint32_t key) {
    UploadOwners *owners=context;TextureRecord *record=texture_store_find(owners->store,key);
    return record?record->last_use:(owners->last_use?owners->last_use(owners->legacy,key):UINT32_MAX);
}
static void upload_evict(void *context,uint32_t key) {
    UploadOwners *owners=context;TextureRecord *record=texture_store_find(owners->store,key);
    if(record)record->flags&=0x7fffffffu;
    else if(owners->evict)owners->evict(owners->legacy,key);
}
int texture_store_upload(TextureStore *store,TextureResidency *ring,uint32_t key,uint32_t frame,
    TextureUpload upload,void *context,TextureLastUse last_use,TextureEvict evict,void *legacy,
    uint32_t *uploaded_bytes) {
    TextureRecord *record=texture_store_find(store,key);uint32_t address=ring->base;
    UploadOwners owners={store,last_use,evict,legacy};*uploaded_bytes=0;
    if(!record || !upload || record->image.size>UINT32_MAX)return 0;
    if(!texture_residency_allocate(ring,key,(uint32_t)record->image.size,frame,
        upload_last_use,upload_evict,&owners,&address)) {
        record->address=address;record->flags&=0x7fffffffu;return 1;
    }
    record->address=address;
    if(!upload(context,address,record)) {
        record->flags&=0x7fffffffu;texture_residency_forget(ring,key);return 0;
    }
    record->flags|=0x80000000u;*uploaded_bytes=(uint32_t)record->image.size;return 1;
}
