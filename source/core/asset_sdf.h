#ifndef UPRISING_ASSET_SDF_H
#define UPRISING_ASSET_SDF_H
#include "asset_mesh.h"
#include "resource_blob.h"
/*Transient file owners. Native mesh publication consumes these arrays; there
 * is no guest image or second live writable object representation. */
typedef struct AssetSdfMesh { char *name;AssetMesh mesh; } AssetSdfMesh;
typedef struct AssetSdfNode {
    uint32_t mesh,parent,last_child,previous_child;
    float local[12];
} AssetSdfNode;
typedef struct AssetSdf {
    AssetSdfMesh *meshes;uint32_t mesh_count;
    AssetSdfNode *nodes;uint32_t node_count;
} AssetSdf;
/*4ec120/4ecef0/4ed750: bounded original text grammar, normalized UVs.
 * Publication scales UVs by signed material dimensions minus uv_bias.
 * Multi-frame TexControl still needs its native animation consumer. */
int asset_sdf_read(const ResourceBlob *,float uv_bias,AssetSdf *);
void asset_sdf_release(AssetSdf *);
#endif
