#ifndef UPRISING_SCENE_MODEL_H
#define UPRISING_SCENE_MODEL_H
#include "asset_mesh.h"
#include <stdint.h>

/* Authoritative render geometry. Indices and child links are native indices;
 * guest addresses are created only by the external x86 projection adapter. */
typedef struct ScenePolygon {
    uint32_t vertex[3];
    AssetMeshUV uv[3];
    uint16_t material;
} ScenePolygon;
typedef struct ScenePolygonPresentation {
    uint32_t flags,material_handle;
    AssetMeshUV uv[3];
} ScenePolygonPresentation;
typedef struct SceneMesh {
    const AssetMesh *asset;
    AssetMeshVertex *vertices;
    ScenePolygon *polygons;
    ScenePolygonPresentation *presentation;
    uint32_t vertex_count,polygon_count;
    int32_t parent,previous_child,last_child;
} SceneMesh;
typedef struct SceneModel {
    AssetModel asset;
    SceneMesh *meshes;
    uint32_t mesh_count,part_base;
} SceneModel;
typedef struct SceneModelLinks {
    int32_t parent,previous_child,sibling_back,last_child;
} SceneModelLinks;
typedef struct SceneModelRecord SceneModelRecord;
typedef struct SceneModelStore {SceneModelRecord *records;} SceneModelStore;
typedef struct SceneRenderDescriptor {
    uint32_t source_vertices,transformed_vertices,uv,uv_work,links;
    uint32_t vertex_count,polygons,polygon_count;
} SceneRenderDescriptor;
/* A cloned render holder is an instance of retained model geometry. The x86
 * holder ID is opaque; topology, descriptor identity and local transform are
 * owned here while legacy presentation still mirrors them. */
typedef struct SceneInstance {
    uint32_t holder,source_holder,descriptor,last_child,previous_child;
    uint32_t parent,sibling_back;
    struct SceneInstance *parent_node,*previous_child_node;
    struct SceneInstance *sibling_back_node,*last_child_node;
    uint32_t flags,texture_state,extension;
    float local[12];
    /* Stable cloned mesh submission fields; x86 descriptor pages mirror these. */
    SceneRenderDescriptor render;
    SceneMesh *mesh; /* Retained geometry; shared source keeps its model alive. */
    uint8_t mesh_render_valid;
    uint8_t has_render;
    ScenePolygonPresentation *presentation;
    uint32_t presentation_count;
    uint32_t presentation_polygon_base,presentation_uv_base;
} SceneInstance;
typedef struct SceneInstanceRecord SceneInstanceRecord;
typedef struct SceneInstanceStore {
    SceneInstanceRecord *records;
    SceneInstanceRecord **by_holder;
    size_t count,capacity;
} SceneInstanceStore;
int scene_instance_store_take(SceneInstanceStore *,const SceneInstance *);
SceneInstance *scene_instance_store_find(SceneInstanceStore *,uint32_t holder);
SceneInstance *scene_instance_store_find_source(SceneInstanceStore *,
    uint32_t source,uint32_t count);
SceneInstance *scene_instance_store_find_descriptor(SceneInstanceStore *,
    uint32_t descriptor);
SceneInstance *scene_instance_store_find_projection(SceneInstanceStore *,
    uint32_t base,int uv);
int scene_instance_store_remove(SceneInstanceStore *,uint32_t holder);
void scene_instance_store_clear(SceneInstanceStore *);
void scene_instance_store_resolve_links(SceneInstanceStore *);
int scene_mesh_initialize(SceneMesh *,const AssetMesh *);
void scene_mesh_release(SceneMesh *);
int scene_mesh_write_vertex(SceneMesh *,uint32_t index,const AssetMeshVertex *);
int scene_mesh_write_polygon_uv(SceneMesh *,uint32_t polygon,uint32_t corner,
    const AssetMeshUV *);
int scene_model_take(SceneModel *,AssetModel *);
void scene_model_release(SceneModel *);
int scene_model_link(SceneModel *,uint32_t parent,uint32_t child);
int scene_model_links(const SceneModel *,uint32_t count,SceneModelLinks *output);
/* External object IDs are opaque lookup keys, never addresses dereferenced here. */
int scene_model_store_take(SceneModelStore *,uint32_t object_id,SceneModel *);
const SceneModel *scene_model_store_find(const SceneModelStore *,uint32_t object_id);
SceneMesh *scene_model_store_mesh(SceneModelStore *,uint32_t object_id,
    uint32_t mesh_index);
void scene_model_store_remove(SceneModelStore *,uint32_t object_id);
void scene_model_store_clear(SceneModelStore *);
#endif
