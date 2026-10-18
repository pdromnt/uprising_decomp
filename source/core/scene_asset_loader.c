/* Complete typed model parse followed by explicit legacy-object projection. */
#include "scene_asset_loader.h"

#include "scene_projection_storage.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct SceneProjection {
    struct SceneProjection *next;
    uint32_t object_id,model_id,mesh_count;
    int root_alive;
    SceneMeshProjection meshes[]; /* x86 IDs belong only to this adapter. */
} SceneProjection;
typedef struct SceneLooseMesh {
    struct SceneLooseMesh *next;
    AssetMesh asset;
    SceneMesh mesh;
    SceneMeshProjection projection;
} SceneLooseMesh;
static SceneProjection *scene_projections;
static SceneLooseMesh *loose_meshes;
static SceneModelStore scene_models;
static SceneModelBridgeStats scene_stats;
static uint32_t next_model_id;
static void prune_owned(void *context){(void)context;scene_model_bridge_prune_dead();}
static void forget_owned(void *context,uint32_t holder){(void)context;scene_model_bridge_forget(holder);}
static int register_source(const SceneMeshProjection *projection,SceneMesh *mesh,
        const SceneInstanceProjectionLinks *links,uint32_t texture){
    SceneInstanceOwnerServices services={NULL,prune_owned,forget_owned};
    scene_instance_owner_services(&services);
    return scene_instance_owner_register_source(projection,mesh,links,texture);
}
static void scene_loose_mesh_drop(SceneLooseMesh **link){
    SceneLooseMesh *entry=*link;
    *link=entry->next;
    scene_instance_owner_forget_source(entry->projection.holder);
    scene_projection_storage_unbind_vertices(entry->projection.source);
    scene_projection_storage_unbind_presentation(entry->projection.polygons,
        entry->projection.uv,entry->projection.uv_work);
    scene_mesh_release(&entry->mesh);
    asset_mesh_release(&entry->asset);
    free(entry);
    ++scene_stats.loose_released;--scene_stats.loose_live;
}
int scene_model_bridge_take_loose_mesh(AssetMesh *asset,SceneMesh *mesh,
    const SceneMeshProjection *projection){
    SceneLooseMesh *entry;
    if(!asset || !mesh || !projection || !projection->holder ||
       !projection->descriptor || mesh->asset!=asset)return 0;
    entry=calloc(1,sizeof(*entry));
    if(!entry)return 0;
    entry->asset=*asset;entry->mesh=*mesh;
    entry->mesh.asset=&entry->asset;
    if((mesh->vertex_count && !scene_projection_storage_bind_vertices(
            projection->source,&entry->mesh)) ||
       (mesh->polygon_count && !scene_projection_storage_bind_presentation(
            projection->polygons,projection->uv,projection->uv_work,
            &entry->mesh)) ||
       !register_source(projection,&entry->mesh,NULL,0u)){
        scene_projection_storage_unbind_vertices(projection->source);
        scene_projection_storage_unbind_presentation(projection->polygons,
            projection->uv,projection->uv_work);
        free(entry);return 0;
    }
    entry->projection=*projection;
    entry->next=loose_meshes;loose_meshes=entry;
    ++scene_stats.loose_retained;++scene_stats.loose_live;
    memset(asset,0,sizeof(*asset));memset(mesh,0,sizeof(*mesh));
    return 1;
}
static void scene_projection_drop(SceneProjection **link,int shutdown){
    SceneProjection *entry=*link;
    uint32_t i;
    for(i=0;i<entry->mesh_count;i++){
        scene_instance_owner_forget_source(entry->meshes[i].holder);
        if(entry->meshes[i].source){
            scene_projection_storage_unbind_vertices(entry->meshes[i].source);
            scene_projection_storage_unbind_presentation(
                entry->meshes[i].polygons,entry->meshes[i].uv,
                entry->meshes[i].uv_work);
        }
    }
    scene_projection_storage_retire_model_pages(entry->model_id);
    scene_model_store_remove(&scene_models,entry->model_id);
    *link=entry->next;
    free(entry);
    if(shutdown)++scene_stats.cleared;else ++scene_stats.destructed;
    --scene_stats.live;
}
void scene_model_bridge_prune_dead(void){
    SceneProjection **link=&scene_projections,*entry;
    SceneLooseMesh **loose=&loose_meshes,*mesh;
    while((entry=*link)!=NULL){
        uint32_t i;
        if(entry->root_alive){link=&entry->next;continue;}
        for(i=0;i<entry->mesh_count;i++)
            if(scene_projection_storage_references(entry->meshes[i].holder) ||
               (entry->meshes[i].source &&
                scene_projection_storage_references(entry->meshes[i].source)))
                break;
        if(i==entry->mesh_count)scene_projection_drop(link,0);
        else link=&entry->next;
    }
    while((mesh=*loose)!=NULL){
        if(!scene_projection_storage_references(mesh->projection.holder) &&
           (!mesh->projection.source || !scene_projection_storage_references(mesh->projection.source)))
            scene_loose_mesh_drop(loose);
        else loose=&mesh->next;
    }
}
void scene_model_bridge_forget(uint32_t object_id){
    SceneProjection *entry;
    SceneLooseMesh **loose=&loose_meshes,*mesh;
    while((mesh=*loose)!=NULL){
        if(mesh->projection.holder==object_id){
            /*Cloned descriptors can still borrow this mesh's source vertices.
             * Retire the geometry only after the last borrower releases it. */
            break;
        }
        loose=&mesh->next;
    }
    for(entry=scene_projections;entry;entry=entry->next)
        if(entry->object_id==object_id && entry->root_alive){
            entry->root_alive=0;
            break;
        }
    scene_model_bridge_prune_dead();
}
void scene_model_bridge_clear(void){
    while(scene_projections)scene_projection_drop(&scene_projections,1);
    while(loose_meshes)scene_loose_mesh_drop(&loose_meshes);
}
const SceneModel *scene_model_bridge_find(uint32_t object_id){
    SceneProjection *entry;
    for(entry=scene_projections;entry;entry=entry->next)
        if(entry->object_id==object_id && entry->root_alive)
            return scene_model_store_find(&scene_models,entry->model_id);
    return NULL;
}
static SceneMesh *scene_model_find_source(uint32_t source,uint32_t count,
    int submitted){
    SceneProjection *entry;uint32_t i;
    SceneMesh *bound;
    SceneLooseMesh *loose;
    if(!source)return NULL;
    bound=scene_projection_storage_bound_mesh(source,count);
    if(bound){
        if(submitted){
            ++scene_stats.matched_submissions;
            for(loose=loose_meshes;loose;loose=loose->next)
                if(&loose->mesh==bound){
                    ++scene_stats.loose_matched_submissions;break;
                }
        }
        return bound;
    }
    /* Empty meshes have no bound vertex page; retain their legacy lookup. */
    if(count)return NULL;
    for(loose=loose_meshes;loose;loose=loose->next)
        if(loose->projection.source==source &&
           loose->mesh.vertex_count==count){
            if(submitted){
                ++scene_stats.matched_submissions;
                ++scene_stats.loose_matched_submissions;
            }
            return &loose->mesh;
        }
    for(entry=scene_projections;entry;entry=entry->next)
        for(i=0;i<entry->mesh_count;i++)if(entry->meshes[i].source==source){
            SceneMesh *mesh=scene_model_store_mesh(&scene_models,entry->model_id,i);
            if(mesh && mesh->vertex_count==count){
                if(submitted)++scene_stats.matched_submissions;
                return mesh;
            }
        }
    return NULL;
}
SceneMesh *scene_model_bridge_source(uint32_t source,uint32_t count){
    return scene_model_find_source(source,count,1);
}
SceneMesh *scene_model_bridge_lookup_source(uint32_t source,uint32_t count){
    return scene_model_find_source(source,count,0);
}
void scene_model_bridge_note_native_mesh_submission(void){
    ++scene_stats.matched_submissions;
}
int scene_model_bridge_render_binding(uint32_t source,uint32_t count,
    uint32_t transformed,uint32_t polygons,SceneProjectionRenderBinding *output){
    SceneLooseMesh *loose;
    if(!scene_projection_storage_render_binding(source,count,transformed,
        polygons,output))return 0;
    for(loose=loose_meshes;loose;loose=loose->next)
        if(&loose->mesh==output->mesh){
            ++scene_stats.loose_render_bindings;break;
        }
    return 1;
}
void scene_model_bridge_stats(SceneModelBridgeStats *output){
    SceneProjectionStorageStats storage;
    if(!output)return;
    scene_projection_storage_stats(&storage);
    *output=scene_stats;
    output->vertex_updates=storage.vertex_updates;
}
void scene_model_bridge_note_polygon_inputs(int native_topology){
    ++scene_stats.typed_polygon_submissions;
    if(native_topology)++scene_stats.native_polygon_topology;
}
void scene_model_bridge_note_uv_update(void){++scene_stats.uv_updates;}
void scene_model_bridge_note_retained_topology(void){++scene_stats.retained_topology_submissions;}
void scene_model_bridge_note_walk(int native,uint32_t leaves,
    uint32_t native_graph_nodes,uint32_t native_source_graph_nodes){
    if(native){++scene_stats.native_walks;scene_stats.native_walk_leaves+=leaves;
        scene_stats.native_graph_nodes+=native_graph_nodes;
        scene_stats.native_source_graph_nodes+=native_source_graph_nodes;}
    else ++scene_stats.generated_walk_fallbacks;
}
void scene_model_bridge_note_owned_walk(void){
    ++scene_stats.owned_storage_walks;
}
void scene_model_bridge_note_owned_mesh(void){
    ++scene_stats.owned_storage_meshes;
}
void scene_model_bridge_note_owned_queue(void){
    ++scene_stats.owned_queue_classifications;
}
void scene_model_bridge_note_owned_reject(unsigned reason){
    if(reason>=1u && reason<=6u)++scene_stats.owned_rejects[reason-1u];
}
static void vector(GameMemory *memory,uint32_t address,const AssetMeshVertex *value) {
    game_memory_write_u32(memory,address,value->x_bits);
    game_memory_write_u32(memory,address+4,value->y_bits);
    game_memory_write_u32(memory,address+8,value->z_bits);
}
int scene_asset_load(GameMemory *memory,const SceneAssetRequest *request,const SceneAssetServices *services) {
    AssetModelFormat format=request->format;
    uint32_t arguments[20]={0},index,name,kind,holder,i,j,result=0,holders[8];
    unsigned main_slot=format==ASSET_MODEL_THREE_MESH?11:(format==ASSET_MODEL_EIGHT_MESH?17:6);
    unsigned point_slot=format==ASSET_MODEL_THREE_MESH?12:7;
    unsigned child_slot=format==ASSET_MODEL_SIMPLE?9:(format==ASSET_MODEL_COLLISION?11:(format==ASSET_MODEL_THREE_MESH?14:18));
    char label[256],path[272];AssetModel decoded={0};SceneModel scene={0};AssetModel *model=&scene.asset;
    SceneModelProjectionPages aux={0};int aux_claimed=0;
    SceneModelLinks index_links[8];
    SceneInstanceProjectionLinks holder_links[8]={0};
    uint32_t main_mesh_count=0,part_count=0,child_base;
    SceneProjection *projection=NULL;

    child_base=services->child_count(services->context);
    arguments[0]=request->index;arguments[1]=request->names;
    for(i=0;i<18u;i++)arguments[i+2u]=request->outputs[i];
    if(memory->fault)return 0;
    index=arguments[0]&65535u;if(index>=32768)index-=65536u;
    name=game_memory_read_u32(memory,arguments[1]+index*4);if(memory->fault)return 0;
    for(i=0;i<sizeof(label);i++){label[i]=(char)game_memory_read_u8(memory,name+i);if(memory->fault)return 0;if(!label[i])break;}
    if(i==sizeof(label)){memory->fault=1;return 0;}
    snprintf(path,sizeof(path),"models\\%s%s.bdm",label,
             format==ASSET_MODEL_THREE_MESH?"t":(format==ASSET_MODEL_EIGHT_MESH?"p":""));
    if(!services->load(services->context,path,format,&decoded))goto done;
    kind=(label[0]=='I' || label[0]=='i')?1:((label[0]=='R' || label[0]=='r')?2:0);

    if(!scene_model_take(&scene,&decoded)){memory->fault=1;goto done;}
    main_mesh_count=scene.part_base;
    part_count=model->part_count;
    if((uint64_t)scene.mesh_count*sizeof(projection->meshes[0])>SIZE_MAX-sizeof(*projection)){
        memory->fault=1;goto done;
    }
    projection=calloc(1,sizeof(*projection)+(size_t)scene.mesh_count*sizeof(projection->meshes[0]));
    if(!projection){memory->fault=1;goto done;}
    /*Parts not requested by this constructor stay decoded asset data. They
     *have no published holder and must not enter the native instance graph. */
    projection->mesh_count=main_mesh_count+(arguments[child_slot]?part_count:0u);
    holders[0]=services->mesh(services->context,&scene.meshes[0],kind,
        &projection->meshes[0]);if(memory->fault)goto done;
    game_memory_write_u32(memory,arguments[main_slot],holders[0]);
    if(memory->fault)goto done;
    for(i=0;i<model->extra_mesh_count && !memory->fault;i++){
        holders[i+1]=services->mesh(services->context,&scene.meshes[i+1],kind,
            &projection->meshes[i+1]);
    }
    if(memory->fault)goto done;
    if(!scene_model_links(&scene,main_mesh_count,index_links)){
        memory->fault=1;goto done;
    }
    for(i=0;i<main_mesh_count;i++){
        for(j=0;j<i;j++)if(holders[i]==holders[j]){
            memory->fault=1;goto done;
        }
        if(!holders[i]){memory->fault=1;goto done;}
        holder_links[i].parent=index_links[i].parent<0?0u:
            holders[index_links[i].parent];
        holder_links[i].previous_child=index_links[i].previous_child<0?0u:
            holders[index_links[i].previous_child];
        holder_links[i].sibling_back=index_links[i].sibling_back<0?0u:
            holders[index_links[i].sibling_back];
        holder_links[i].last_child=index_links[i].last_child<0?0u:
            holders[index_links[i].last_child];
    }
    if(!scene_projection_storage_build_model_pages(model,
        format!=ASSET_MODEL_EIGHT_MESH && arguments[point_slot]!=0,
        arguments[child_slot]!=0,services->child_count(services->context),&aux)){
        memory->fault=1;goto done;
    }
    /*402260 reads the marker block only to advance the file. Collision
     *presentations consume the48-byte planes; never leave unowned hull pages. */
    if(format==ASSET_MODEL_COLLISION && aux.markers){
        scene_projection_storage_free(aux.markers);aux.markers=0;
    }
    game_memory_write_u32(memory,arguments[3],model->radius_bits);
    vector(memory,arguments[2],&model->center);
    /* Collision format consumes its marker block without publishing it. */
    if(format!=ASSET_MODEL_COLLISION) {
        if(format==ASSET_MODEL_SIMPLE)
            game_memory_write_u32(memory,arguments[5],model->marker_count);
        if(arguments[4])game_memory_write_u32(memory,arguments[4],aux.markers);
    }
    if(format==ASSET_MODEL_THREE_MESH || format==ASSET_MODEL_EIGHT_MESH) {
        unsigned groups=format==ASSET_MODEL_THREE_MESH?3:4,anchors=format==ASSET_MODEL_THREE_MESH?3:6;
        for(i=0;i<groups;i++)
            game_memory_write_u32(memory,arguments[5+i],model->groups[i]);
        for(i=0;i<anchors;i++)
            vector(memory,arguments[5+groups+i],&model->anchors[i]);
        if(format==ASSET_MODEL_EIGHT_MESH){
            game_memory_write_u32(memory,arguments[15],model->groups[4]);
            game_memory_write_u32(memory,arguments[16],model->groups[5]);
        }
    }
    if(format==ASSET_MODEL_COLLISION) {
        game_memory_write_u32(memory,arguments[4],model->plane_count);
        game_memory_write_u32(memory,arguments[5],aux.planes);
        game_memory_write_u32(memory,arguments[9],model->secondary_point_count);
        game_memory_write_u32(memory,arguments[10],aux.secondary_points);
    }
    if(format!=ASSET_MODEL_EIGHT_MESH && arguments[point_slot]) {
        game_memory_write_u32(memory,arguments[point_slot],model->point_count);
        /* Presentation construction reads two adjacent vectors even when the
           decoded record declares one. Keep that published pair bounded. */
        game_memory_write_u32(memory,arguments[point_slot+1],aux.points);
    }
    if(memory->fault)goto done;
    if(arguments[child_slot]) {
        game_memory_write_u32(memory,arguments[child_slot],model->part_count);
        game_memory_write_u32(memory,arguments[child_slot+1],aux.parts);
        for(i=0;i<model->part_count && !memory->fault;i++) {
            const AssetModelPart *part=&model->parts[i];uint32_t range;
            holder=services->mesh(services->context,&scene.meshes[scene.part_base+i],
                kind,&projection->meshes[scene.part_base+i]);if(memory->fault)goto done;
            for(range=0;range<3;range++) {
                uint32_t vertices=game_memory_read_u32(memory,
                    0x5a0488u+range*8);
                uint32_t triangles=game_memory_read_u32(memory,
                    0x5a048cu+range*8);
                if(vertices<=INT32_MAX && triangles<=INT32_MAX && vertices>=part->mesh.vertex_count && triangles>=part->mesh.triangle_count)break;
            }
            {SceneChildRecord child={holder,range,NULL,0};
                if(!services->child_append(services->context,memory,child))goto done;}
        }
    }
    if(!memory->fault){
        uint32_t model_id=++next_model_id;
        if(!model_id || !scene_model_store_take(&scene_models,model_id,&scene))memory->fault=1;
        else {
            for(i=0;i<projection->mesh_count;i++){
                SceneMesh *native=scene_model_store_mesh(&scene_models,model_id,i);
                const SceneInstanceProjectionLinks *links=i<main_mesh_count?
                    &holder_links[i]:NULL;
                if(!native || (i>=main_mesh_count &&
                   (native->parent>=0 || native->last_child>=0 ||
                    native->previous_child>=0))){
                    memory->fault=1;break;
                }
                if((native->vertex_count &&
                    !scene_projection_storage_bind_vertices(projection->meshes[i].source,native)) ||
                   (native->polygon_count &&
                    !scene_projection_storage_bind_presentation(
                        projection->meshes[i].polygons,projection->meshes[i].uv,
                        projection->meshes[i].uv_work,native)) ||
                   !register_source(&projection->meshes[i],
                        native,links,0u)){
                    memory->fault=1;break;
                }
            }
            if(!memory->fault){
                scene_instance_owner_resolve_links();
                for(j=0;arguments[child_slot] && j<part_count;j++){
                    uint32_t mesh_index=main_mesh_count+j;
                    SceneMesh *mesh=scene_model_store_mesh(&scene_models,
                        model_id,mesh_index);
                    if(!services->child_bind(services->context,child_base+j,
                        projection->meshes[mesh_index].holder,mesh)){
                        memory->fault=1;break;
                    }
                }
            }
            if(!memory->fault){
                aux_claimed=scene_projection_storage_claim_model_pages(model_id,&aux);
                if(!aux_claimed)memory->fault=1;
            }
            if(!memory->fault)
                game_memory_write_u32(memory,0x585334u,1u);
            if(memory->fault){
                for(j=0;j<=i && j<projection->mesh_count;j++)
                    if(projection->meshes[j].source){
                        scene_instance_owner_forget_source(
                            projection->meshes[j].holder);
                        scene_projection_storage_unbind_vertices(
                            projection->meshes[j].source);
                        scene_projection_storage_unbind_presentation(
                            projection->meshes[j].polygons,
                            projection->meshes[j].uv,
                            projection->meshes[j].uv_work);
                    }
                scene_model_store_remove(&scene_models,model_id);
            }else{
                projection->object_id=holders[0];projection->model_id=model_id;
                projection->root_alive=1;projection->next=scene_projections;
                scene_projections=projection;projection=NULL;
                ++scene_stats.retained;++scene_stats.live;
                result=1;
            }
        }
    }
done:
    if(!result && services->child_count(services->context)>child_base)
        services->child_rollback(services->context,memory,child_base);

    if(!aux_claimed)scene_projection_storage_discard_model_pages(&aux);
    free(projection);scene_model_release(&scene);asset_model_release(&decoded);
    return memory->fault?0:(int)result;
}
