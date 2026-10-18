#ifndef UPRISING_PLAYER_SCENE_CHILDREN_H
#define UPRISING_PLAYER_SCENE_CHILDREN_H

#include "spatial_attachment.h"
#include "player_attachment.h"

enum { PLAYER_SCENE_CHILD_COUNT = 15 };

/* Decoded resource choices at the presentation boundary. Negative choices
 * disable optional children; motion handles are borrowed from the catalog. */
typedef struct PlayerSceneChildrenDefinition {
    int32_t choices[PLAYER_SCENE_CHILD_COUNT];
    GameAddress motion[PLAYER_SCENE_CHILD_COUNT];
    float mounts[4][3];
    float dimensions[3];
    float small_scale, lateral_shift, vertical_shift, rear_shift;
    float mount_rear_shift;
} PlayerSceneChildrenDefinition;

typedef struct PlayerSceneChild {
    PlayerSceneChildKind kind;
    GameAddress projection;
    GameSpatialAttachmentInit attachment;
    PlayerAttachmentState state;
} PlayerSceneChild;

typedef struct PlayerSceneChildren {
    GameAddress scene;
    PlayerSceneChild children[PLAYER_SCENE_CHILD_COUNT];
    PlayerSceneChild auxiliary;
} PlayerSceneChildren;

/* The native graph owns allocations. Guest slots only publish handles for
 * presentation consumers which have not yet migrated. */
int player_scene_children_create(GameMemory *memory, GameAddress scene,
    const PlayerSceneChildrenDefinition *definition);
int player_scene_children_prepare(GameMemory *memory, GameAddress scene,
    GameAddress motion, int32_t height, int32_t top, int32_t bottom,
    int32_t base);
const PlayerSceneChildren *player_scene_children_find(GameAddress scene);
PlayerAttachmentState *player_scene_children_attachment(GameAddress child);
int player_scene_children_release(GameAddress child);
int player_scene_children_finalize(GameMemory *memory, GameAddress scene);
void player_scene_children_retire(GameAddress scene);

#endif
