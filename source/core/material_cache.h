#ifndef UPRISING_MATERIAL_CACHE_H
#define UPRISING_MATERIAL_CACHE_H
#include <stddef.h>
#include <stdint.h>
/* Backend resources have their own lifetime. This cache owns names and immutable
 * material metadata; backend_handle is opaque, never a native pointer. */
typedef struct MaterialValue { uint16_t width,height;uint32_t backend_handle; } MaterialValue;
typedef struct MaterialEntry { char *name;MaterialValue value; } MaterialEntry;
typedef struct MaterialCache { MaterialEntry *entries;size_t count,capacity; } MaterialCache;
int material_cache_find(const MaterialCache *cache,const char *name,size_t *index);
int material_cache_insert(MaterialCache *cache,const char *name,MaterialValue value,size_t *index);
void material_cache_release(MaterialCache *cache);
#endif
