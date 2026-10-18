#ifndef UPRISING_ASSET_MESH_H
#define UPRISING_ASSET_MESH_H
#include "resource_archive.h"
/* IEEE-754 words are retained exactly until geometry math consumes them. */
typedef struct AssetMeshVertex { uint32_t x_bits,y_bits,z_bits; } AssetMeshVertex;
typedef struct AssetMeshUV { uint32_t u_bits,v_bits; } AssetMeshUV;
typedef struct AssetMeshTriangle {
    uint16_t vertices[3];
    AssetMeshUV uv[3];
    uint16_t material;
} AssetMeshTriangle;
typedef struct AssetMesh {
    uint32_t material_count,vertex_count,triangle_count;
    char **materials;
    AssetMeshVertex *vertices;
    AssetMeshTriangle *triangles;
    uint32_t radius_bits;
    AssetMeshVertex center;
    uint32_t auxiliary_bits; /* serialized scalar at legacy descriptor +0x34 */
} AssetMesh;
/* Owns all parsed storage, no fixed global name/index tables. Pass empty output.
 * Consumes exactly one descriptor at the stream cursor; leaves trailing asset
 * arrays untouched. Failure consumes attempted bytes but publishes no output. */
int asset_mesh_read(ResourceStream *stream,AssetMesh *output);
void asset_mesh_release(AssetMesh *mesh);
typedef struct AssetModelMarker { uint32_t words[7]; } AssetModelMarker;
typedef struct AssetModelPart { AssetMeshVertex position;AssetMesh mesh; } AssetModelPart;
typedef enum AssetModelFormat {
    ASSET_MODEL_SIMPLE, ASSET_MODEL_COLLISION, ASSET_MODEL_THREE_MESH, ASSET_MODEL_EIGHT_MESH
} AssetModelFormat;
typedef struct AssetCollisionPlane {
    uint32_t vertex_count;
    AssetMeshVertex normal;
    AssetMeshVertex *vertices,*secondary_vectors;
} AssetCollisionPlane;
typedef struct AssetModel {
    AssetMesh mesh;
    uint32_t radius_bits;
    AssetMeshVertex center;
    uint32_t marker_count,point_count,part_count;
    AssetModelMarker *markers;
    AssetMeshVertex *points;
    AssetModelPart *parts;
    AssetModelFormat format;
    uint32_t extra_mesh_count,plane_count,secondary_point_count;
    AssetMesh extra_meshes[7];
    AssetCollisionPlane *planes;
    AssetMeshVertex *secondary_points;
    uint32_t groups[6];
    AssetMeshVertex anchors[6];
} AssetModel;
/* Simple indexed-model route00401990 only; other BDM variants have additional
 * collision or mesh sections. This is not a universal BDM auto-detector. */
int asset_model_read(ResourceStream *stream,AssetModel *output);
int asset_model_read_format(ResourceStream *stream,AssetModelFormat format,AssetModel *output);
void asset_model_release(AssetModel *model);
#endif
