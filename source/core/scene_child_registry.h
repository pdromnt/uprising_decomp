#ifndef UPRISING_SCENE_CHILD_REGISTRY_H
#define UPRISING_SCENE_CHILD_REGISTRY_H
#include <stdint.h>
struct SceneMesh;

typedef struct SceneChildRecord {
    /* External records retain an opaque ABI key; model children use mesh. */
    uint32_t external_id;
    uint32_t category;
    struct SceneMesh *mesh;
    /*The native binding keeps its source identity after external dispatch is
     *disabled. Bounded legacy tables expose this ID, never the mesh pointer. */
    uint32_t holder_id;
} SceneChildRecord;

typedef struct SceneChildRegistry {
    SceneChildRecord *records;
    uint32_t count;
    uint32_t capacity;
} SceneChildRegistry;

int scene_child_registry_initialize(SceneChildRegistry *, uint32_t capacity);
int scene_child_registry_append(SceneChildRegistry *, SceneChildRecord);
int scene_child_registry_get(const SceneChildRegistry *, uint32_t index,
    SceneChildRecord *);
int scene_child_registry_bind_mesh(SceneChildRegistry *,uint32_t index,
    uint32_t object_id,struct SceneMesh *mesh);
void scene_child_registry_remove_last(SceneChildRegistry *);
void scene_child_registry_release(SceneChildRegistry *);
#endif
