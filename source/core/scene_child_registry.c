#include "scene_child_registry.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void scene_child_registry_release(SceneChildRegistry *registry) {
    if (!registry) return;
    free(registry->records);
    memset(registry, 0, sizeof(*registry));
}

int scene_child_registry_initialize(SceneChildRegistry *registry, uint32_t capacity) {
    SceneChildRecord *records;
    if (!registry || !capacity || (uint64_t)capacity * sizeof(*records) > SIZE_MAX)
        return 0;
    records = calloc(capacity, sizeof(*records));
    if (!records) return 0;
    scene_child_registry_release(registry);
    registry->records = records;
    registry->capacity = capacity;
    return 1;
}

int scene_child_registry_append(SceneChildRegistry *registry, SceneChildRecord record) {
    SceneChildRecord *grown;
    uint32_t capacity;
    if (!registry || !registry->records || registry->count == UINT32_MAX) return 0;
    if (registry->count == registry->capacity) {
        if (registry->capacity > UINT32_MAX / 2) return 0;
        capacity = registry->capacity * 2u;
        if ((uint64_t)capacity * sizeof(*grown) > SIZE_MAX) return 0;
        grown = realloc(registry->records, (size_t)capacity * sizeof(*grown));
        if (!grown) return 0;
        memset(grown + registry->capacity, 0,
            (size_t)(capacity - registry->capacity) * sizeof(*grown));
        registry->records = grown;
        registry->capacity = capacity;
    }
    if(!record.holder_id)record.holder_id=record.external_id;
    registry->records[registry->count++] = record;
    return 1;
}

int scene_child_registry_get(const SceneChildRegistry *registry, uint32_t index,
    SceneChildRecord *out) {
    if (!registry || !out || index >= registry->count) return 0;
    *out = registry->records[index];
    return 1;
}
int scene_child_registry_bind_mesh(SceneChildRegistry *registry,uint32_t index,
    uint32_t object_id,struct SceneMesh *mesh){
    SceneChildRecord *child;
    if(!registry || !mesh || !object_id || index>=registry->count)return 0;
    child=&registry->records[index];
    if(child->external_id!=object_id || child->mesh)return 0;
    child->mesh=mesh;
    child->external_id=0u;
    return 1;
}

void scene_child_registry_remove_last(SceneChildRegistry *registry) {
    if (registry && registry->count) --registry->count;
}
