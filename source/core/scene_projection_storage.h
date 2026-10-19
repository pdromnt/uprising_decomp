#ifndef UPRISING_SCENE_PROJECTION_STORAGE_H
#define UPRISING_SCENE_PROJECTION_STORAGE_H

#include <stddef.h>
#include <stdint.h>
#include "material_cache.h"
#include "scene_model.h"
#include "render_arena.h"

/* Bounded address views for model projection buffers still consumed by x86
 * callers. Storage and lifetime belong to the native scene projection owner. */
#define SCENE_PROJECTION_BASE 0xf4000000u
#define SCENE_PROJECTION_END 0xf8000000u

typedef struct SceneProjectionStorageStats {
    uint32_t allocated, released, live, reads, writes;
    uint32_t typed_materials, material_reads;
    uint32_t vertex_updates;
    uint32_t uv_reads;
    uint32_t packet_views;
    uint32_t native_packet_links;
    uint32_t owned_descriptor_clones;
    uint32_t holder_projects;
    uint32_t typed_holder_views;
} SceneProjectionStorageStats;
typedef struct SceneDescriptorState {
    uint32_t source_vertices,transformed_vertices,uv,uv_work,links;
    uint32_t vertex_count,polygons,polygon_count;
    uint32_t radius_bits,center[3],name;
    uint32_t auxiliary[3],optional_shorts,optional_bytes,state;
} SceneDescriptorState;
typedef struct SceneHolderState {
    float local[12];
    uint32_t texture_state,flags;
    uint32_t parent,previous_child,sibling_back,last_child;
    uint32_t descriptor,extension;
} SceneHolderState;
typedef struct SceneMeshProjectionPages {
    uint32_t source,transformed,uv,uv_work,links,polygons,descriptor;
} SceneMeshProjectionPages;
typedef struct SceneProjectionRenderBinding {
    SceneMesh *mesh;
    uint32_t transformed,polygons,links,uv_work;
    int source_presentation;
} SceneProjectionRenderBinding;
typedef struct ModelHullSphere {
    float position[3],radius;
    uint32_t metadata[3];
} ModelHullSphere;
/* Stable typed hull storage owned by the model-page lifetime. */
const ModelHullSphere *scene_projection_storage_hull(uint32_t,size_t);

typedef struct SceneModelProjectionPages {
    uint32_t markers,points,planes,secondary_points,parts;
    uint32_t *plane_vertices,*plane_secondary;
    uint32_t plane_count;
} SceneModelProjectionPages;
int scene_projection_storage_build_model_pages(const AssetModel *model,
    int include_points,int include_parts,uint32_t first_child,
    SceneModelProjectionPages *pages);
void scene_projection_storage_discard_model_pages(SceneModelProjectionPages *pages);
int scene_projection_storage_claim_model_pages(uint32_t model_id,
    SceneModelProjectionPages *pages);
void scene_projection_storage_release_model_pages(uint32_t model_id);
void scene_projection_storage_retire_model_pages(uint32_t model_id);
/* Builds the authoritative source-instance snapshot from owned holder and
 * descriptor state. Caller transfers presentation ownership to its store. */
/* 1 cloned, 0 external/mixed source, -1 native source failed. */
int scene_projection_storage_clone_descriptor(uint32_t source,
    uint32_t *clone);
int scene_projection_storage_clone_owned_descriptor(const SceneInstance *source,
    const SceneMesh *mesh,uint32_t *clone);
/* Preflight and release owned descriptor members; caller frees descriptor page. */
int scene_projection_storage_can_release_descriptor(uint32_t descriptor);
/* 1 released, 0 mixed/unknown, -1 failed after native preflight. */
int scene_projection_storage_release_descriptor(uint32_t descriptor);
/* Owns the retained holder, named descriptor and all mesh pages. Material
 * handles are external backend IDs; the scene owner retains typed values. */
uint32_t scene_projection_storage_create_mesh(SceneMesh *scene,
    const char *label,size_t label_size,const uint32_t *material_handles,
    const MaterialValue *materials,const uint8_t *material_flags,
    uint32_t material_count,SceneMeshProjectionPages *pages);
uint32_t scene_projection_storage_create_holder(void);
int scene_projection_storage_project_holder(const SceneInstance *instance);
uint32_t scene_projection_storage_create_descriptor(const char *label,size_t size);
int scene_projection_storage_set_holder_descriptor(uint32_t holder,uint32_t descriptor);
int scene_projection_storage_build_mesh(SceneMesh *scene,
    const uint32_t *material_handles,const MaterialValue *materials,
    const uint8_t *material_flags,uint32_t material_count,
    SceneMeshProjectionPages *pages);

uint32_t scene_projection_storage_allocate(size_t size, const char *purpose);
typedef int (*SceneProjectionStateRead)(void *, uint32_t, void *, size_t);
typedef int (*SceneProjectionStateWrite)(void *, uint32_t, const void *, size_t);
/* A compatibility page may expose named native state rather than own mutable
 * guest-layout bytes. The binding lives exactly as long as that page. */
int scene_projection_storage_bind_state(uint32_t base, void *context,
    SceneProjectionStateRead read, SceneProjectionStateWrite write);
void *scene_projection_storage_state(uint32_t base,
    SceneProjectionStateRead expected_read);
/* Borrow a complete uint32_t page owned by projection storage. The pointer
 * remains stable until that exact page is freed. */
uint32_t *scene_projection_storage_borrow_u32(uint32_t base,uint32_t count);
/*Exact plain-byte allocation only; no aliasing/alignment requirements. */
uint8_t *scene_projection_storage_borrow_bytes(uint32_t base,size_t size);
int scene_projection_storage_free(uint32_t base);
/* A cloned descriptor borrows the original source vertices until it dies. */
int scene_projection_storage_attach_shared_source(uint32_t descriptor,
    uint32_t source);
uint32_t scene_projection_storage_references(uint32_t base);
int scene_projection_storage_descriptor(uint32_t base,SceneDescriptorState *output);
int scene_projection_storage_holder(uint32_t base,SceneHolderState *output);
int scene_projection_storage_read(uint32_t address, void *output, size_t size);
int scene_projection_storage_write(uint32_t address, const void *input, size_t size);
enum {
    SCENE_PROJECTION_HOLDER_WRITE,
    SCENE_PROJECTION_DESCRIPTOR_WRITE,
    SCENE_PROJECTION_POLYGON_WRITE,
    SCENE_PROJECTION_UV_WRITE
};
typedef void (*SceneProjectionObjectWrite)(void *context,uint32_t object,
    uint32_t offset,size_t size,int kind);
void scene_projection_storage_observe_objects(void *context,
    SceneProjectionObjectWrite callback);
int scene_projection_storage_attach_materials(uint32_t polygon_base,
    const MaterialValue *values, uint32_t count);
int scene_projection_storage_material(uint32_t polygon, MaterialValue *output);
int scene_projection_storage_read_uv(uint32_t address,AssetMeshUV *output);
const AssetMeshUV *scene_projection_storage_uv(uint32_t address,size_t count);
/* Mutable packet view for model polygon pages only. Its lifetime is the page. */
RenderArenaPacket *scene_projection_storage_packet(uint32_t address);
int scene_projection_storage_packet_set_next(uint32_t address,uint32_t next_ref);
int scene_projection_storage_packet_link(uint32_t address,uint32_t *next_ref,
    RenderArenaPacket **next_owned);
int scene_projection_storage_set_packet_flags(uint32_t address,uint32_t flags);
/* Typed spans of owned model work pages. External addresses return NULL. */
float *scene_projection_storage_positions(uint32_t address,size_t count);
RenderArenaVertex *scene_projection_storage_transformed(uint32_t address,
    size_t count);
/* Stable only for this live transformed allocation; reuse changes generation. */
uint64_t scene_projection_storage_vertex_key(uint32_t address);
const uint32_t *scene_projection_storage_links(uint32_t address,size_t count);
const RenderArenaUv *scene_projection_storage_work_uv(uint32_t address,
    size_t count);
int scene_projection_storage_set_work_uv(uint32_t address,
    const RenderArenaUv *value);
int scene_projection_storage_prepare_instance(const SceneInstance *instance);
int scene_projection_storage_bind_vertices(uint32_t source, SceneMesh *mesh);
SceneMesh *scene_projection_storage_bound_mesh(uint32_t source,uint32_t count);
int scene_projection_storage_render_binding(uint32_t source,uint32_t count,
    uint32_t transformed,uint32_t polygons,SceneProjectionRenderBinding *output);
void scene_projection_storage_unbind_vertices(uint32_t source);
int scene_projection_storage_bind_presentation(uint32_t polygons,uint32_t uv,
    uint32_t uv_work,SceneMesh *mesh);
void scene_projection_storage_unbind_presentation(uint32_t polygons,uint32_t uv,
    uint32_t uv_work);
/* Model topology is authoritative after projection; legacy link writes
 * invalidate the retained-index submission route. */
int scene_projection_storage_bind_links(uint32_t links);
int scene_projection_storage_links_clean(uint32_t links);
void scene_projection_storage_release_all(void);
void scene_projection_storage_stats(SceneProjectionStorageStats *output);
int scene_projection_storage_describe(uint32_t address, uint32_t *base,
    uint32_t *size, const char **purpose);

#endif
