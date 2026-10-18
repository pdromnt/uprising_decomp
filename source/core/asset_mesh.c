#include "asset_mesh.h"
#include <stdlib.h>
#include <string.h>
static int word(ResourceStream *stream,uint32_t *value,unsigned bytes) {
    uint8_t data[4];unsigned i;uint32_t result=0;
    if(stream->read(stream->context,data,bytes)!=bytes)return 0;
    for(i=0;i<bytes;i++)result|=(uint32_t)data[i]<<(i*8);
    *value=result;return 1;
}
static int vector(ResourceStream *stream,AssetMeshVertex *value) {
    return word(stream,&value->x_bits,4) && word(stream,&value->y_bits,4) && word(stream,&value->z_bits,4);
}
void asset_mesh_release(AssetMesh *mesh) {
    uint32_t i;if(!mesh)return;
    if(mesh->materials)for(i=0;i<mesh->material_count;i++)free(mesh->materials[i]);
    free(mesh->materials);free(mesh->vertices);free(mesh->triangles);memset(mesh,0,sizeof(*mesh));
}
int asset_mesh_read(ResourceStream *stream,AssetMesh *output) {
    AssetMesh mesh={0};uint32_t names,i,j,value;
    if(!stream || !stream->read || !output || output->materials || output->vertices || output->triangles)return 0;
    if(!word(stream,&names,2) || !word(stream,&mesh.vertex_count,4) || !word(stream,&mesh.triangle_count,4))goto failed;
    mesh.material_count=names<32768?names:0; /* retail signed-short loop */
    if(mesh.vertex_count>INT32_MAX || mesh.triangle_count>INT32_MAX ||
       (uint64_t)mesh.vertex_count*sizeof(*mesh.vertices)>SIZE_MAX ||
       (uint64_t)mesh.triangle_count*sizeof(*mesh.triangles)>SIZE_MAX)goto failed;
    if(mesh.material_count) {
        mesh.materials=calloc(mesh.material_count,sizeof(*mesh.materials));if(!mesh.materials)goto failed;
    }
    for(i=0;i<mesh.material_count;i++) {
        char line[256];size_t length=0;
        while(length<255) {
            uint8_t byte;if(stream->read(stream->context,&byte,1)!=1)break;
            line[length++]=(char)byte;if(byte=='\n')break;
        }
        if(!length)goto failed;
        line[length]=0;
        /* Retail removes the last character returned by strlen, including a
         * non-newline byte on truncated lines. CR is deliberately retained. */
        {size_t used=strlen(line);if(!used)goto failed;line[used-1]=0;}
        mesh.materials[i]=malloc(length+1);if(!mesh.materials[i])goto failed;
        memcpy(mesh.materials[i],line,length+1);
    }
    if(mesh.vertex_count){mesh.vertices=calloc(mesh.vertex_count,sizeof(*mesh.vertices));if(!mesh.vertices)goto failed;}
    if(mesh.triangle_count){mesh.triangles=calloc(mesh.triangle_count,sizeof(*mesh.triangles));if(!mesh.triangles)goto failed;}
    for(i=0;i<mesh.vertex_count;i++)if(!vector(stream,&mesh.vertices[i]))goto failed;
    for(i=0;i<mesh.triangle_count;i++)for(j=0;j<3;j++) {
        if(!word(stream,&value,2) || value>=mesh.vertex_count || value>=32768)goto failed;
        mesh.triangles[i].vertices[j]=(uint16_t)value;
    }
    for(i=0;i<mesh.triangle_count;i++)for(j=0;j<3;j++) {
        if(!word(stream,&mesh.triangles[i].uv[j].u_bits,4) || !word(stream,&mesh.triangles[i].uv[j].v_bits,4))goto failed;
    }
    for(i=0;i<mesh.triangle_count;i++) {
        if(!word(stream,&value,2) || value>=mesh.material_count || value>=32768)goto failed;
        mesh.triangles[i].material=(uint16_t)value;
    }
    if(!word(stream,&mesh.radius_bits,4) || !vector(stream,&mesh.center) || !word(stream,&mesh.auxiliary_bits,4))goto failed;
    *output=mesh;return 1;
failed:
    asset_mesh_release(&mesh);return 0;
}

void asset_model_release(AssetModel *model) {
    uint32_t i;if(!model)return;
    asset_mesh_release(&model->mesh);
    for(i=0;i<7;i++)asset_mesh_release(&model->extra_meshes[i]);
    if(model->planes)for(i=0;i<model->plane_count;i++) {
        free(model->planes[i].vertices);free(model->planes[i].secondary_vectors);
    }
    free(model->planes);free(model->secondary_points);
    if(model->parts)for(i=0;i<model->part_count;i++)asset_mesh_release(&model->parts[i].mesh);
    free(model->markers);free(model->points);free(model->parts);memset(model,0,sizeof(*model));
}
static int vectors(ResourceStream *stream,uint32_t count,AssetMeshVertex **output) {
    uint32_t i;
    if(count>INT32_MAX || (uint64_t)count*sizeof(**output)>SIZE_MAX)return 0;
    if(count){*output=calloc(count,sizeof(**output));if(!*output)return 0;}
    for(i=0;i<count;i++)if(!vector(stream,&(*output)[i]))return 0;
    return 1;
}
int asset_model_read_format(ResourceStream *stream,AssetModelFormat format,AssetModel *output) {
    AssetModel model={0};uint32_t i,j;
    if(!output || output->mesh.vertices || output->mesh.materials || output->mesh.triangles ||
       output->markers || output->points || output->parts || output->planes || output->secondary_points)return 0;
    if(format<ASSET_MODEL_SIMPLE || format>ASSET_MODEL_EIGHT_MESH)return 0;
    model.format=format;
    model.extra_mesh_count=format==ASSET_MODEL_THREE_MESH?2:(format==ASSET_MODEL_EIGHT_MESH?7:0);
    if(!asset_mesh_read(stream,&model.mesh))goto failed;
    for(i=0;i<model.extra_mesh_count;i++)if(!asset_mesh_read(stream,&model.extra_meshes[i]))goto failed;
    if(!word(stream,&model.radius_bits,4) || !vector(stream,&model.center) ||
       !word(stream,&model.marker_count,4))goto failed;
    if(model.marker_count>INT32_MAX || (uint64_t)model.marker_count*sizeof(*model.markers)>SIZE_MAX)goto failed;
    if(model.marker_count){model.markers=calloc(model.marker_count,sizeof(*model.markers));if(!model.markers)goto failed;}
    for(i=0;i<model.marker_count;i++)for(j=0;j<7;j++)if(!word(stream,&model.markers[i].words[j],4))goto failed;
    if(format==ASSET_MODEL_COLLISION) {
        if(!word(stream,&model.plane_count,4) || model.plane_count>INT32_MAX ||
           (uint64_t)model.plane_count*sizeof(*model.planes)>SIZE_MAX)goto failed;
        if(model.plane_count){model.planes=calloc(model.plane_count,sizeof(*model.planes));if(!model.planes)goto failed;}
        for(i=0;i<model.plane_count;i++) {
            AssetCollisionPlane *plane=&model.planes[i];
            if(!word(stream,&plane->vertex_count,4) || plane->vertex_count>=32768 ||
               !vector(stream,&plane->normal) || !vectors(stream,plane->vertex_count,&plane->vertices) ||
               !vectors(stream,plane->vertex_count,&plane->secondary_vectors))goto failed;
        }
    }
    if(format==ASSET_MODEL_THREE_MESH || format==ASSET_MODEL_EIGHT_MESH) {
        unsigned groups=format==ASSET_MODEL_THREE_MESH?2:4,anchors=format==ASSET_MODEL_THREE_MESH?3:6;
        for(i=0;i<groups;i++)if(!word(stream,&model.groups[i],4))goto failed;
        if(format==ASSET_MODEL_THREE_MESH)model.groups[2]=model.marker_count-model.groups[0]-model.groups[1];
        for(i=0;i<anchors;i++)if(!vector(stream,&model.anchors[i]))goto failed;
        if(format==ASSET_MODEL_EIGHT_MESH)for(i=4;i<6;i++)if(!word(stream,&model.groups[i],4))goto failed;
    }
    if(format!=ASSET_MODEL_EIGHT_MESH &&
       (!word(stream,&model.point_count,4) || !vectors(stream,model.point_count,&model.points)))goto failed;
    if(format==ASSET_MODEL_COLLISION &&
       (!word(stream,&model.secondary_point_count,4) || !vectors(stream,model.secondary_point_count,&model.secondary_points)))goto failed;
    if(!word(stream,&model.part_count,4) || model.part_count>INT32_MAX ||
       (uint64_t)model.part_count*sizeof(*model.parts)>SIZE_MAX)goto failed;
    if(model.part_count){model.parts=calloc(model.part_count,sizeof(*model.parts));if(!model.parts)goto failed;}
    for(i=0;i<model.part_count;i++)if(!vector(stream,&model.parts[i].position))goto failed;
    for(i=0;i<model.part_count;i++)if(!asset_mesh_read(stream,&model.parts[i].mesh))goto failed;
    *output=model;return 1;
failed:
    asset_model_release(&model);return 0;
}
int asset_model_read(ResourceStream *stream,AssetModel *output) {
    return asset_model_read_format(stream,ASSET_MODEL_SIMPLE,output);
}
