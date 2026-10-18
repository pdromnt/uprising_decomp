#ifndef UPRISING_SCENE_INSTANCE_OWNER_H
#define UPRISING_SCENE_INSTANCE_OWNER_H
#include "scene_projection_storage.h"
#include "game_memory.h"
typedef struct SceneMeshProjection {
    uint32_t holder,descriptor,source,transformed,polygons,links,uv,uv_work;
} SceneMeshProjection;
typedef struct SceneInstanceProjectionLinks {
    uint32_t parent,previous_child,sibling_back,last_child;
} SceneInstanceProjectionLinks;
typedef struct SceneInstanceOwnerStats {
    uint32_t created,released,live,native_clone_links,native_release_links;
    uint32_t native_walk_nodes,native_render_nodes,transform_refreshes;
    uint32_t polygon_refreshes,native_release_graphs,native_presentation_clones;
} SceneInstanceOwnerStats;
typedef struct SceneInstanceOwnerServices {
    void *context;
    void (*prune_models)(void *);
    void (*forget_model)(void *, uint32_t holder);
} SceneInstanceOwnerServices;
void scene_instance_owner_services(const SceneInstanceOwnerServices *);
int scene_instance_owner_register_source(const SceneMeshProjection *, SceneMesh *,
    const SceneInstanceProjectionLinks *, uint32_t texture_state);
void scene_instance_owner_resolve_links(void);
int scene_instance_owner_view(uint32_t holder, SceneInstance *);
int scene_instance_owner_source_topology(uint32_t source, uint32_t count,
    uint32_t *last_child_source, uint32_t *previous_child_source);
void scene_instance_owner_forget_source(uint32_t holder);
void scene_instance_owner_stats(SceneInstanceOwnerStats *);
void scene_instance_owner_note_walk(uint32_t nodes, uint32_t render_nodes);
void scene_instance_owner_clear(void);
/* 1 native success, 0 foreign/mixed, -1 failed. */
int scene_instance_owner_clone(GameMemory *, uint32_t source, uint32_t *output);
int scene_instance_owner_release_graph(uint32_t holder);
/* Roll back a fresh clone before transferring it to a live presentation owner.
 * Borrowed descriptor ancillary data belongs to its source and is retained. */
int scene_instance_owner_discard_clone(uint32_t holder);
int scene_instance_owner_release_mesh_graph(uint32_t holder, const SceneMesh *);
int scene_instance_owner_destruct_graph(uint32_t holder);
int scene_instance_owner_is_holder(uint32_t holder);
/* Transitional foreign-graph cleanup must retire the same native record. */
SceneInstance *scene_instance_owner_find(uint32_t holder);
int scene_instance_owner_remove_record(uint32_t holder);
void scene_instance_owner_note_release_link(void);
#endif
