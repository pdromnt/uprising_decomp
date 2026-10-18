#ifndef UPRISING_SCENE_ASSET_LOADER_H
#define UPRISING_SCENE_ASSET_LOADER_H
#include "scene_instance_owner.h"
#include "scene_child_registry.h"
/* Output handles address bounded owned views. This record describes asset
 * outputs, never registers, a guest stack or host pointer encodings. */
typedef struct SceneAssetRequest {
    AssetModelFormat format;
    uint16_t index;
    GameAddress names;
    GameAddress outputs[18];
} SceneAssetRequest;
typedef struct SceneAssetServices {
    void *context;
    int (*load)(void *,const char *,AssetModelFormat,AssetModel *);
    GameAddress (*mesh)(void *,SceneMesh *,unsigned,SceneMeshProjection *);
    uint32_t (*child_count)(void *);
    int (*child_append)(void *,GameMemory *,SceneChildRecord);
    int (*child_bind)(void *,uint32_t,GameAddress,SceneMesh *);
    int (*child_rollback)(void *,GameMemory *,uint32_t);
} SceneAssetServices;
int scene_asset_load(GameMemory *,const SceneAssetRequest *,const SceneAssetServices *);
int scene_model_bridge_take_loose_mesh(AssetMesh *,SceneMesh *,const SceneMeshProjection *);
void scene_model_bridge_prune_dead(void);
void scene_model_bridge_forget(uint32_t);
void scene_model_bridge_clear(void);
const SceneModel *scene_model_bridge_find(uint32_t);
SceneMesh *scene_model_bridge_source(uint32_t,uint32_t);
SceneMesh *scene_model_bridge_lookup_source(uint32_t,uint32_t);
int scene_model_bridge_render_binding(uint32_t,uint32_t,uint32_t,uint32_t,SceneProjectionRenderBinding *);
void scene_model_bridge_note_native_mesh_submission(void);
void scene_model_bridge_note_polygon_inputs(int);
void scene_model_bridge_note_uv_update(void);
void scene_model_bridge_note_retained_topology(void);
void scene_model_bridge_note_walk(int,uint32_t,uint32_t,uint32_t);
void scene_model_bridge_note_owned_walk(void);
void scene_model_bridge_note_owned_mesh(void);
void scene_model_bridge_note_owned_queue(void);
void scene_model_bridge_note_owned_reject(unsigned);
typedef struct SceneModelBridgeStats {
    uint32_t retained,destructed,cleared,live,matched_submissions,vertex_updates;
    uint32_t loose_retained,loose_released,loose_live;
    uint32_t loose_matched_submissions,loose_render_bindings;
    uint32_t typed_polygon_submissions;
    uint32_t native_polygon_topology;
    uint32_t retained_topology_submissions;
    uint32_t uv_updates;
    uint32_t native_walks,native_walk_leaves,generated_walk_fallbacks;
    uint32_t owned_storage_walks;
    uint32_t owned_storage_meshes;
    uint32_t owned_queue_classifications;
    uint32_t owned_rejects[6];
    uint32_t native_graph_nodes,native_source_graph_nodes;
} SceneModelBridgeStats;
void scene_model_bridge_stats(SceneModelBridgeStats *);
#endif
