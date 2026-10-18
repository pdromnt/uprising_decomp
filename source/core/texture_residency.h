#ifndef UPRISING_TEXTURE_RESIDENCY_H
#define UPRISING_TEXTURE_RESIDENCY_H
#include <stdint.h>
typedef struct TextureResidencyBlock TextureResidencyBlock;
typedef struct TextureResidency {
    TextureResidencyBlock *cursor;
    uint32_t base, capacity;
} TextureResidency;
typedef uint32_t (*TextureLastUse)(void *,uint32_t);
typedef void (*TextureEvict)(void *,uint32_t);
int texture_residency_reset(TextureResidency *,uint32_t,uint32_t);
void texture_residency_release(TextureResidency *);
int texture_residency_allocate(TextureResidency *,uint32_t owner,uint32_t size,
    uint32_t frame,TextureLastUse,TextureEvict,void *,uint32_t *address);

void texture_residency_forget(TextureResidency *,uint32_t owner);
#endif
