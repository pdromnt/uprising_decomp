#include "scene_model.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
struct SceneModelRecord {uint32_t object_id;SceneModel model;struct SceneModelRecord *next;};
struct SceneInstanceRecord {SceneInstance value;struct SceneInstanceRecord *next;};
/* Index the existing owned nodes, retaining their stable pointers and list
 * order. Per-frame holder writes/graph lookups must not scan every live model. */
static size_t holder_position(const SceneInstanceStore *store,uint32_t holder){
    size_t first=0,last=store->count;
    while(first<last){size_t middle=first+(last-first)/2;
        if(store->by_holder[middle]->value.holder<holder)first=middle+1;
        else last=middle;
    }
    return first;
}
SceneInstance *scene_instance_store_find(SceneInstanceStore *store,
    uint32_t holder){
    size_t index;
    if(!store || !holder)return NULL;
    index=holder_position(store,holder);
    if(index<store->count && store->by_holder[index]->value.holder==holder)
        return &store->by_holder[index]->value;
    return NULL;
}
SceneInstance *scene_instance_store_find_source(SceneInstanceStore *store,
    uint32_t source,uint32_t count){
    SceneInstanceRecord *record;
    if(!store || !source)return NULL;
    for(record=store->records;record;record=record->next){
        SceneInstance *instance=&record->value;
        if(!instance->source_holder && instance->mesh &&
           instance->mesh_render_valid &&
           instance->has_render &&
           instance->render.source_vertices==source &&
           instance->render.vertex_count==count)
            return instance;
    }
    return NULL;
}
SceneInstance *scene_instance_store_find_descriptor(SceneInstanceStore *store,
    uint32_t descriptor){
    SceneInstanceRecord *record;
    if(!store || !descriptor)return NULL;
    for(record=store->records;record;record=record->next)
        if(record->value.descriptor==descriptor)return &record->value;
    return NULL;
}
SceneInstance *scene_instance_store_find_projection(SceneInstanceStore *store,
    uint32_t base,int uv){
    SceneInstanceRecord *record;
    if(!store || !base)return NULL;
    for(record=store->records;record;record=record->next)
        if(record->value.has_render &&
           (uv?record->value.presentation_uv_base:
               record->value.presentation_polygon_base)==base)
            return &record->value;
    return NULL;
}
int scene_instance_store_take(SceneInstanceStore *store,
    const SceneInstance *instance){
    SceneInstanceRecord *record;size_t index;
    if(!store || !instance || !instance->holder ||
       scene_instance_store_find(store,instance->holder))return 0;
    record=malloc(sizeof(*record));if(!record)return 0;
    if(store->count==store->capacity){
        size_t capacity=store->capacity?store->capacity*2:64;
        SceneInstanceRecord **grown;
        if(capacity<store->capacity || capacity>SIZE_MAX/sizeof(*grown)){free(record);return 0;}
        grown=realloc(store->by_holder,capacity*sizeof(*grown));
        if(!grown){free(record);return 0;}
        store->by_holder=grown;store->capacity=capacity;
    }
    index=holder_position(store,instance->holder);
    memmove(store->by_holder+index+1,store->by_holder+index,(store->count-index)*sizeof(*store->by_holder));
    store->by_holder[index]=record;++store->count;
    record->value=*instance;record->next=store->records;store->records=record;
    return 1;
}
int scene_instance_store_remove(SceneInstanceStore *store,uint32_t holder){
    SceneInstanceRecord **link,*record;
    if(!store || !holder)return 0;
    for(link=&store->records;(record=*link)!=NULL;link=&record->next)
        if(record->value.holder==holder){
            SceneInstanceRecord *other;size_t index=holder_position(store,holder);
            for(other=store->records;other;other=other->next){
                SceneInstance *node=&other->value,*removed=&record->value;
                if(node->parent_node==removed)node->parent_node=NULL;
                if(node->previous_child_node==removed)node->previous_child_node=NULL;
                if(node->sibling_back_node==removed)node->sibling_back_node=NULL;
                if(node->last_child_node==removed)node->last_child_node=NULL;
            }
            *link=record->next;
            --store->count;
            memmove(store->by_holder+index,store->by_holder+index+1,(store->count-index)*sizeof(*store->by_holder));
            free(record->value.presentation);
            free(record);return 1;
        }
    return 0;
}
void scene_instance_store_resolve_links(SceneInstanceStore *store){
    SceneInstanceRecord *record;
    if(!store)return;
    for(record=store->records;record;record=record->next){
        SceneInstance *node=&record->value;
        node->parent_node=scene_instance_store_find(store,node->parent);
        node->previous_child_node=scene_instance_store_find(store,
            node->previous_child);
        node->sibling_back_node=scene_instance_store_find(store,
            node->sibling_back);
        node->last_child_node=scene_instance_store_find(store,node->last_child);
    }
}
void scene_instance_store_clear(SceneInstanceStore *store){
    if(!store)return;
    while(store->records)
        scene_instance_store_remove(store,store->records->value.holder);
    free(store->by_holder);store->by_holder=NULL;store->capacity=0;
}
static void scene_model_rebind(SceneModel *scene){
    uint32_t i;
    scene->meshes[0].asset=&scene->asset.mesh;
    for(i=0;i<scene->asset.extra_mesh_count;i++)
        scene->meshes[i+1].asset=&scene->asset.extra_meshes[i];
    for(i=0;i<scene->asset.part_count;i++)
        scene->meshes[scene->part_base+i].asset=&scene->asset.parts[i].mesh;
}

int scene_mesh_initialize(SceneMesh *out,const AssetMesh *asset) {
    SceneMesh next={0};uint32_t i,j;
    if(!out || !asset || out->asset ||
       (asset->vertex_count && !asset->vertices) ||
       (asset->triangle_count && !asset->triangles) ||
       (uint64_t)asset->vertex_count*sizeof(*next.vertices)>SIZE_MAX ||
       (uint64_t)asset->triangle_count*sizeof(*next.polygons)>SIZE_MAX)return 0;
    next.asset=asset;next.vertex_count=asset->vertex_count;
    next.polygon_count=asset->triangle_count;
    next.parent=next.previous_child=next.last_child=-1;
    if(next.vertex_count){
        next.vertices=malloc((size_t)next.vertex_count*sizeof(*next.vertices));
        if(!next.vertices)return 0;
        memcpy(next.vertices,asset->vertices,(size_t)next.vertex_count*sizeof(*next.vertices));
    }
    if(next.polygon_count){
        next.polygons=calloc(next.polygon_count,sizeof(*next.polygons));
        next.presentation=calloc(next.polygon_count,sizeof(*next.presentation));
        if(!next.polygons || !next.presentation){
            free(next.presentation);free(next.polygons);free(next.vertices);
            return 0;
        }
        for(i=0;i<next.polygon_count;i++){
            const AssetMeshTriangle *triangle=&asset->triangles[i];
            ScenePolygon *polygon=&next.polygons[i];
            if(triangle->material>=asset->material_count){
                free(next.presentation);free(next.polygons);free(next.vertices);return 0;
            }
            polygon->material=triangle->material;
            for(j=0;j<3;j++){
                if(triangle->vertices[j]>=next.vertex_count){
                    free(next.presentation);free(next.polygons);free(next.vertices);return 0;
                }
                polygon->vertex[j]=triangle->vertices[j];
                polygon->uv[j]=triangle->uv[j];
            }
        }
    }
    *out=next;return 1;
}
void scene_mesh_release(SceneMesh *mesh){
    if(!mesh)return;
    free(mesh->vertices);free(mesh->polygons);free(mesh->presentation);
    memset(mesh,0,sizeof(*mesh));
}
int scene_mesh_write_vertex(SceneMesh *mesh,uint32_t index,const AssetMeshVertex *vertex){
    if(!mesh || !vertex || index>=mesh->vertex_count || !mesh->vertices)return 0;
    mesh->vertices[index]=*vertex;return 1;
}
int scene_mesh_write_polygon_uv(SceneMesh *mesh,uint32_t polygon,uint32_t corner,
    const AssetMeshUV *uv){
    if(!mesh || !uv || polygon>=mesh->polygon_count || corner>=3 || !mesh->polygons)return 0;
    mesh->polygons[polygon].uv[corner]=*uv;return 1;
}
int scene_model_link(SceneModel *scene,uint32_t parent,uint32_t child){
    SceneMesh *p,*c;
    if(!scene || parent>=scene->mesh_count || child>=scene->mesh_count ||
       parent==child || scene->meshes[child].parent>=0)return 0;
    p=&scene->meshes[parent];c=&scene->meshes[child];
    c->parent=(int32_t)parent;c->previous_child=p->last_child;
    p->last_child=(int32_t)child;return 1;
}
int scene_model_links(const SceneModel *model,uint32_t count,
    SceneModelLinks *output){
    uint32_t paths[8][8],lengths[8]={0};
    uint8_t seen[8]={0};uint32_t i,j;
    if(!model || !model->meshes || !output || !count || count>8u ||
       model->part_base!=count || model->mesh_count<count)return 0;
    for(i=0;i<count;i++){
        int32_t child=model->meshes[i].last_child;
        while(child>=0){
            if((uint32_t)child>=count || lengths[i]>=count ||
               seen[child])return 0;
            seen[child]=1u;
            paths[i][lengths[i]++]=(uint32_t)child;
            child=model->meshes[child].previous_child;
        }
    }
    for(i=0;i<count;i++)
        output[i].parent=output[i].previous_child=
            output[i].sibling_back=output[i].last_child=-1;
    for(i=0;i<count;i++){
        int32_t previous=-1;
        for(j=lengths[i];j>0;j--){
            uint32_t child=paths[i][j-1u];
            output[child].parent=(int32_t)i;
            output[child].previous_child=previous;
            if(previous>=0)output[previous].sibling_back=(int32_t)child;
            output[i].last_child=(int32_t)child;
            previous=(int32_t)child;
        }
    }
    return 1;
}
void scene_model_release(SceneModel *scene){
    uint32_t i;if(!scene)return;
    if(scene->meshes)for(i=0;i<scene->mesh_count;i++)scene_mesh_release(&scene->meshes[i]);
    free(scene->meshes);asset_model_release(&scene->asset);memset(scene,0,sizeof(*scene));
}
int scene_model_take(SceneModel *scene,AssetModel *asset){
    SceneModel next={0};uint32_t i;
    if(!scene || !asset || scene->meshes || scene->mesh_count ||
       asset->extra_mesh_count>7 || asset->part_count>UINT32_MAX-8)return 0;
    next.asset=*asset;memset(asset,0,sizeof(*asset));
    next.part_base=1+next.asset.extra_mesh_count;
    next.mesh_count=next.part_base+next.asset.part_count;
    next.meshes=calloc(next.mesh_count,sizeof(*next.meshes));
    if(!next.meshes)goto fail;
    if(!scene_mesh_initialize(&next.meshes[0],&next.asset.mesh))goto fail;
    for(i=0;i<next.asset.extra_mesh_count;i++)
        if(!scene_mesh_initialize(&next.meshes[i+1],&next.asset.extra_meshes[i]))goto fail;
    for(i=0;i<next.asset.part_count;i++)
        if(!scene_mesh_initialize(&next.meshes[next.part_base+i],&next.asset.parts[i].mesh))goto fail;
    if(next.asset.format==ASSET_MODEL_THREE_MESH){
        if(!scene_model_link(&next,0,1) || !scene_model_link(&next,1,2))goto fail;
    }else if(next.asset.format==ASSET_MODEL_EIGHT_MESH){
        const uint8_t links[][2]={{2,3},{4,5},{6,7},{0,6},{0,4},{0,2},{0,1}};
        for(i=0;i<sizeof(links)/sizeof(links[0]);i++)
            if(!scene_model_link(&next,links[i][0],links[i][1]))goto fail;
    }
    *scene=next;
    scene_model_rebind(scene);
    return 1;
fail:
    scene_model_release(&next);return 0;
}
const SceneModel *scene_model_store_find(const SceneModelStore *store,uint32_t object_id){
    const SceneModelRecord *record;
    if(!store)return NULL;
    for(record=store->records;record;record=record->next)
        if(record->object_id==object_id)return &record->model;
    return NULL;
}
SceneMesh *scene_model_store_mesh(SceneModelStore *store,uint32_t object_id,
    uint32_t mesh_index){
    SceneModelRecord *record;
    if(!store)return NULL;
    for(record=store->records;record;record=record->next)
        if(record->object_id==object_id && mesh_index<record->model.mesh_count)
            return &record->model.meshes[mesh_index];
    return NULL;
}
void scene_model_store_remove(SceneModelStore *store,uint32_t object_id){
    SceneModelRecord **link,*record;
    if(!store)return;
    link=&store->records;
    while((record=*link)!=NULL){
        if(record->object_id==object_id){
            *link=record->next;scene_model_release(&record->model);free(record);return;
        }
        link=&record->next;
    }
}
void scene_model_store_clear(SceneModelStore *store){
    if(!store)return;
    while(store->records)scene_model_store_remove(store,store->records->object_id);
}
int scene_model_store_take(SceneModelStore *store,uint32_t object_id,SceneModel *scene){
    SceneModelRecord *record;
    if(!store || !scene || !scene->meshes || !object_id ||
       scene_model_store_find(store,object_id))return 0;
    record=malloc(sizeof(*record));if(!record)return 0;
    record->object_id=object_id;record->model=*scene;
    scene_model_rebind(&record->model);memset(scene,0,sizeof(*scene));
    record->next=store->records;store->records=record;return 1;
}
