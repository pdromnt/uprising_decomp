#ifndef UPRISING_SCENE_DESCRIPTOR_CLONE_H
#define UPRISING_SCENE_DESCRIPTOR_CLONE_H
#include "game_memory.h"
#include <stdint.h>
uint32_t scene_descriptor_clone_native(GameMemory *memory,uint32_t source);
int scene_descriptor_can_release_native(GameMemory *memory,uint32_t descriptor);
/* Returns 1 when a native descriptor was released, 0 for legacy ownership. */
int scene_descriptor_release_native(GameMemory *memory,uint32_t descriptor);
/* Roll back a descriptor newly cloned by this component; borrowed ancillary
 * identities are retained. This is not a general foreign destructor. */
int scene_descriptor_discard_clone_native(uint32_t descriptor);
#endif
