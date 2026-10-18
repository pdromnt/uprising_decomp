#ifndef UPRISING_SPATIAL_ATTACHMENT_H
#define UPRISING_SPATIAL_ATTACHMENT_H

#include "game_memory.h"

/* The attachment's construction inputs, decoded before entering the native
 * constructor. The projection remains until its update consumers migrate. */
typedef struct GameSpatialAttachmentInit {
    GameAddress parent;
    GameAddress motion;
    uint16_t heading;
    float position[3];
    float secondary_position[3];
    uint32_t orient_to_motion;
    uint32_t mode;
    uint32_t resource;
    uint32_t slot;
    uint32_t kind;
} GameSpatialAttachmentInit;

int game_spatial_attachment_construct(GameMemory *memory,
    GameAddress object, const GameSpatialAttachmentInit *init);

#endif
