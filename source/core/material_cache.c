#include "material_cache.h"
#include <stdlib.h>
#include <string.h>
int material_cache_find(const MaterialCache *cache,const char *name,size_t *index) {
    size_t i;if(!cache || !name || !index)return 0;
    for(i=0;i<cache->count;i++)if(!strcmp(cache->entries[i].name,name)){*index=i;return 1;}
    return 0;
}
int material_cache_insert(MaterialCache *cache,const char *name,MaterialValue value,size_t *index) {
    char *copy;size_t length;
    if(!cache || !name || !index)return 0;
    if(material_cache_find(cache,name,index))return 1;
    length=strlen(name);if(length==SIZE_MAX)return 0;
    copy=malloc(length+1);if(!copy)return 0;memcpy(copy,name,length+1);
    if(cache->count==cache->capacity) {
        size_t capacity=cache->capacity?cache->capacity*2:32;MaterialEntry *entries;
        if(capacity<cache->capacity || capacity>SIZE_MAX/sizeof(*entries)){free(copy);return 0;}
        entries=realloc(cache->entries,capacity*sizeof(*entries));
        if(!entries){free(copy);return 0;}cache->entries=entries;cache->capacity=capacity;
    }
    *index=cache->count;cache->entries[cache->count].name=copy;
    cache->entries[cache->count++].value=value;return 1;
}
void material_cache_release(MaterialCache *cache) {
    size_t i;if(!cache)return;
    for(i=0;i<cache->count;i++)free(cache->entries[i].name);
    free(cache->entries);memset(cache,0,sizeof(*cache));
}
