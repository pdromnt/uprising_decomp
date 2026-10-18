#include "scene_instance_owner.h"
#include "scene_descriptor_clone.h"
#include <stdlib.h>
#include <string.h>
static SceneInstanceOwnerServices owner_services;
void scene_instance_owner_services(const SceneInstanceOwnerServices *services) {
    if(services) owner_services=*services; else memset(&owner_services,0,sizeof(owner_services));
}
static SceneInstanceStore scene_instances;
static SceneInstanceOwnerStats instance_stats;
static void scene_instance_projection_written(void *context,uint32_t object,
    uint32_t offset,size_t size,int kind){
    SceneInstance *instance;
    SceneHolderState holder;
    if(kind==SCENE_PROJECTION_POLYGON_WRITE ||
       kind==SCENE_PROJECTION_UV_WRITE){
        uint32_t first,last,index;
        instance=scene_instance_store_find_projection(context,object,
            kind==SCENE_PROJECTION_UV_WRITE);
        if(!instance || !instance->presentation || !size)return;
        if(kind==SCENE_PROJECTION_POLYGON_WRITE){
            first=offset/40u;last=(uint32_t)((offset+size-1u)/40u);
            for(index=first;index<=last &&
                index<instance->presentation_count;index++){
                RenderArenaPacket *packet=scene_projection_storage_packet(
                    object+index*40u);
                uint32_t field=index*40u+8u;
                if(packet && offset<field+4u && offset+size>field &&
                   instance->presentation[index].flags!=packet->flags){
                    instance->presentation[index].flags=packet->flags;
                    ++instance_stats.polygon_refreshes;
                }
                field=index*40u+20u;
                if(packet && offset<field+4u && offset+size>field &&
                   instance->presentation[index].material_handle!=packet->material){
                    instance->presentation[index].material_handle=packet->material;
                    ++instance_stats.polygon_refreshes;
                }
            }
        }else{
            first=offset/8u;last=(uint32_t)((offset+size-1u)/8u);
            for(index=first;index<=last &&
                index/3u<instance->presentation_count;index++){
                AssetMeshUV value,*owned=&instance->presentation[index/3u].uv[index%3u];
                if(scene_projection_storage_read_uv(object+index*8u,&value) &&
                   memcmp(owned,&value,sizeof(value))){
                    *owned=value;++instance_stats.polygon_refreshes;
                }
            }
        }
        return;
    }
    instance=kind==SCENE_PROJECTION_DESCRIPTOR_WRITE?
        scene_instance_store_find_descriptor(context,object):
        scene_instance_store_find(context,object);
    if(!instance)return;
    if(kind==SCENE_PROJECTION_DESCRIPTOR_WRITE){
        SceneDescriptorState descriptor;
        if(instance->has_render &&
           scene_projection_storage_descriptor(object,&descriptor)){
            SceneRenderDescriptor render={descriptor.source_vertices,
                descriptor.transformed_vertices,descriptor.uv,
                descriptor.uv_work,descriptor.links,descriptor.vertex_count,
                descriptor.polygons,descriptor.polygon_count};
            if(memcmp(&instance->render,&render,sizeof(render)))
                instance->mesh_render_valid=0u;
            instance->render=render;
        }
        return;
    }
    if(!scene_projection_storage_holder(object,&holder))return;
    if(memcmp(instance->local,holder.local,sizeof(holder.local))){
        memcpy(instance->local,holder.local,sizeof(holder.local));
        ++instance_stats.transform_refreshes;
    }
    instance->flags=holder.flags;
    instance->texture_state=holder.texture_state;
    instance->parent=holder.parent;
    instance->previous_child=holder.previous_child;
    instance->sibling_back=holder.sibling_back;
    instance->last_child=holder.last_child;
    if(holder.descriptor!=instance->descriptor)
        instance->mesh_render_valid=0u;
    instance->descriptor=holder.descriptor;
    instance->extension=holder.extension;
    if(offset<0x50u && offset+size>0x40u)
        scene_instance_store_resolve_links(context);
}
void scene_instance_owner_resolve_links(void){
    scene_instance_store_resolve_links(&scene_instances);
}
int scene_instance_owner_view(uint32_t holder,SceneInstance *output){
    SceneInstance *instance=scene_instance_store_find(&scene_instances,holder);
    if(!instance || !output)return 0;
    *output=*instance;return 1;
}
int scene_instance_owner_source_topology(uint32_t source,uint32_t count,
    uint32_t *last_child_source,uint32_t *previous_child_source){
    SceneInstance *instance,*child;
    if(!last_child_source || !previous_child_source)return 0;
    instance=scene_instance_store_find_source(&scene_instances,source,count);
    if(!instance)return 0;
    child=instance->last_child_node;
    if(instance->last_child && !child)return 0;
    *last_child_source=child?child->render.source_vertices:0u;
    child=instance->previous_child_node;
    if(instance->previous_child && !child)return 0;
    *previous_child_source=child?child->render.source_vertices:0u;
    return 1;
}
int scene_instance_owner_register_source(const SceneMeshProjection *projection,
    SceneMesh *mesh,const SceneInstanceProjectionLinks *links,
    uint32_t texture_state){
    SceneInstance instance={0};
    uint32_t base,size,i;
    const char *purpose=NULL;
    if(!projection || !mesh || !projection->holder ||
       scene_instance_store_find(&scene_instances,projection->holder) ||
       !scene_projection_storage_describe(projection->holder,&base,&size,
           &purpose) || base!=projection->holder || size!=0x5cu ||
       !purpose || strcmp(purpose,"mesh holder") ||
       !scene_projection_storage_describe(projection->descriptor,&base,&size,
           &purpose) || base!=projection->descriptor || size!=0x4cu ||
       !purpose || strcmp(purpose,"mesh descriptor") ||
       (mesh->polygon_count && (!mesh->polygons || !mesh->presentation)))
        return 0;
    instance.holder=projection->holder;
    instance.descriptor=projection->descriptor;
    instance.mesh=mesh;
    instance.mesh_render_valid=1u;
    if(links){
        instance.parent=links->parent;
        instance.previous_child=links->previous_child;
        instance.sibling_back=links->sibling_back;
        instance.last_child=links->last_child;
    }
    instance.texture_state=texture_state;
    instance.local[0]=instance.local[5]=instance.local[10]=1.0f;
    instance.render.source_vertices=projection->source;
    instance.render.transformed_vertices=projection->transformed;
    instance.render.uv=projection->uv;
    instance.render.uv_work=projection->uv_work;
    instance.render.links=projection->links;
    instance.render.vertex_count=mesh->vertex_count;
    instance.render.polygons=projection->polygons;
    instance.render.polygon_count=mesh->polygon_count;
    instance.presentation_count=mesh->polygon_count;
    instance.presentation_polygon_base=projection->polygons;
    instance.presentation_uv_base=projection->uv;
    if(mesh->polygon_count){
        instance.presentation=calloc(mesh->polygon_count,
            sizeof(*instance.presentation));
        if(!instance.presentation)return 0;
        for(i=0;i<mesh->polygon_count;i++){
            instance.presentation[i].flags=mesh->presentation[i].flags;
            instance.presentation[i].material_handle=
                mesh->presentation[i].material_handle;
            memcpy(instance.presentation[i].uv,mesh->polygons[i].uv,
                sizeof(instance.presentation[i].uv));
        }
    }
    instance.has_render=1u;
    if(!scene_projection_storage_project_holder(&instance)){
        free(instance.presentation);return 0;
    }
    scene_projection_storage_observe_objects(&scene_instances,
        scene_instance_projection_written);
    if(!scene_instance_store_take(&scene_instances,&instance)){
        free(instance.presentation);
        return 0;
    }
    ++instance_stats.created;++instance_stats.live;
    return 1;
}
void scene_instance_owner_forget_source(uint32_t holder){
    SceneInstance *instance=scene_instance_store_find(&scene_instances,holder);
    if(instance && !instance->source_holder &&
       scene_instance_store_remove(&scene_instances,holder)){
        ++instance_stats.released;--instance_stats.live;
    }
}
void scene_instance_owner_stats(SceneInstanceOwnerStats *output){
    if(output)*output=instance_stats;
}
void scene_instance_owner_note_walk(uint32_t nodes,uint32_t render_nodes){
    instance_stats.native_walk_nodes+=nodes;
    instance_stats.native_render_nodes+=render_nodes;
}
void scene_instance_owner_clear(void){
    scene_projection_storage_observe_objects(NULL,NULL);
    scene_instance_store_clear(&scene_instances);
    instance_stats.live=0;
}

static int native_holder(uint32_t object) {
    uint32_t base,size;
    const char *purpose=NULL;
    return scene_projection_storage_describe(object,&base,&size,&purpose) &&
        base==object && size==0x5cu && purpose &&
        !strcmp(purpose,"mesh holder");
}

typedef struct SceneCloneContext {
    void *external_context;
    uint32_t (*clone_external)(void *,uint32_t descriptor);
    uint32_t fault_address;
    int fault;
    uint32_t *allocated;
    size_t count,capacity;
} SceneCloneContext;

static uint32_t clone_source_link(SceneCloneContext *context,uint32_t holder,
    int first){
    SceneInstance *instance=scene_instance_store_find(&scene_instances,holder);
    SceneHolderState view;
    if(instance){
        ++instance_stats.native_clone_links;
        SceneInstance *linked=first?instance->last_child_node:
            instance->previous_child_node;
        if((first?instance->last_child:instance->previous_child) && !linked){
            context->fault=1;context->fault_address=holder;return 0;
        }
        return linked?linked->holder:0u;
    }
    if(!scene_projection_storage_holder(holder,&view)){
        context->fault=1;context->fault_address=holder;return 0;
    }
    return first?view.last_child:view.previous_child;
}

static uint32_t clone_holder(SceneCloneContext *context,uint32_t source,
    uint32_t parent,uint32_t sibling_back,unsigned depth) {
    uint32_t output,source_child,previous_clone=0,descriptor,clone=0;
    SceneInstance instance={0},*owned;
    if(context->fault)return 0;
    if(depth>4096u || !native_holder(source)){
        context->fault=1;context->fault_address=source;return 0;
    }
    if(context->count>=4096u){context->fault=1;context->fault_address=source;return 0;}
    if(context->count==context->capacity){
        size_t capacity=context->capacity?context->capacity*2u:64u;
        uint32_t *next=realloc(context->allocated,capacity*sizeof(*next));
        if(!next){context->fault=1;context->fault_address=source;return 0;}
        context->allocated=next;context->capacity=capacity;
    }
    output=scene_projection_storage_create_holder();
    if(!output){context->fault=1;context->fault_address=0;return 0;}
    context->allocated[context->count++]=output;
    instance.holder=output;instance.source_holder=source;
    instance.parent=parent;instance.sibling_back=sibling_back;
    instance.local[0]=instance.local[5]=instance.local[10]=1.0f;
    if(context->fault || !scene_instance_store_take(&scene_instances,&instance)){
        context->fault=1;context->fault_address=output;return 0;
    }
    ++instance_stats.created;++instance_stats.live;
    source_child=clone_source_link(context,source,1);
    while(source_child && !context->fault){
        uint32_t next=clone_source_link(context,source_child,0);
        if(context->fault)return 0;
        clone=clone_holder(context,source_child,output,previous_clone,
            depth+1u);
        if(context->fault)return 0;
        if(previous_clone){
            owned=scene_instance_store_find(&scene_instances,previous_clone);
            if(!owned){context->fault=1;
                context->fault_address=previous_clone;return 0;}
            owned->previous_child=clone;
            if(!scene_projection_storage_project_holder(owned)){
                context->fault=1;context->fault_address=previous_clone+0x44u;
                return 0;
            }
        }else{
            owned=scene_instance_store_find(&scene_instances,output);
            if(!owned){context->fault=1;context->fault_address=output;
                return 0;}
            owned->last_child=clone;
        }
        previous_clone=clone;
        source_child=next;
    }
    if(context->fault)return 0;
    owned=scene_instance_store_find(&scene_instances,source);
    if(owned)descriptor=owned->descriptor;
    else {
        SceneHolderState view;
        if(!scene_projection_storage_holder(source,&view)){
            context->fault=1;context->fault_address=source+0x50u;return 0;
        }
        descriptor=view.descriptor;
    }
    if(descriptor && !context->fault){
        const SceneInstance *source_instance=
            scene_instance_store_find(&scene_instances,source);
        SceneMesh *source_mesh=source_instance && source_instance->has_render &&
            source_instance->mesh_render_valid &&
            source_instance->descriptor==descriptor?
            source_instance->mesh:NULL;
        int native_clone=source_mesh?
            scene_projection_storage_clone_owned_descriptor(source_instance,
                source_mesh,&clone):0;
        if(native_clone==0)
            native_clone=scene_projection_storage_clone_descriptor(descriptor,
                &clone);
        if(native_clone==0 && context->clone_external)
            clone=context->clone_external(context->external_context,descriptor);
        if(native_clone<0 || !clone){
            context->fault=1;context->fault_address=descriptor;
        }
        if(context->fault)return 0;
        owned=scene_instance_store_find(&scene_instances,output);
        if(owned && !context->fault){
            uint32_t i;
            SceneDescriptorState record;
            owned->descriptor=clone;
            if(!scene_projection_storage_descriptor(clone,&record)){
                context->fault=1;context->fault_address=clone;return 0;
            }
            owned->render.source_vertices=record.source_vertices;
            owned->render.transformed_vertices=record.transformed_vertices;
            owned->render.uv=record.uv;
            owned->render.uv_work=record.uv_work;
            owned->render.links=record.links;
            owned->render.vertex_count=record.vertex_count;
            owned->render.polygons=record.polygons;
            owned->render.polygon_count=record.polygon_count;
            if(native_clone==1){
                owned->mesh=source_mesh;
                owned->mesh_render_valid=1u;
            }
            owned->presentation_polygon_base=owned->render.polygons;
            owned->presentation_uv_base=owned->render.uv;
            if(!context->fault && owned->render.polygon_count){
                uint32_t count=owned->render.polygon_count;
                owned->presentation=calloc(count,sizeof(*owned->presentation));
                if(!owned->presentation){
                    context->fault=1;context->fault_address=clone;
                }else owned->presentation_count=count;
                if(!context->fault && native_clone==1 && source_instance &&
                   source_instance->descriptor==descriptor &&
                   source_instance->has_render &&
                   source_instance->presentation &&
                   source_instance->presentation_count==count){
                    memcpy(owned->presentation,source_instance->presentation,
                        (size_t)count*sizeof(*owned->presentation));
                    ++instance_stats.native_presentation_clones;
                }else {
                    const AssetMeshUV *uv=scene_projection_storage_uv(
                        owned->render.uv,(size_t)count*3u);
                    if(!uv){context->fault=1;
                        context->fault_address=owned->render.uv;}
                    for(i=0;i<count && !context->fault;i++){
                        ScenePolygonPresentation *polygon=&owned->presentation[i];
                        RenderArenaPacket *packet=scene_projection_storage_packet(
                            owned->render.polygons+i*40u);
                        if(!packet){
                            context->fault=1;context->fault_address=
                                owned->render.polygons+i*40u+8u;break;
                        }
                        polygon->flags=packet->flags;
                        polygon->material_handle=packet->material;
                        memcpy(polygon->uv,uv+i*3u,sizeof(polygon->uv));
                    }
                }
            }
            owned->has_render=!context->fault;
        }
    }
    owned=scene_instance_store_find(&scene_instances,source);
    if(owned)instance.flags=owned->flags;
    else {
        SceneHolderState view;
        if(!scene_projection_storage_holder(source,&view)){
            context->fault=1;context->fault_address=source+0x3cu;return 0;
        }
        instance.flags=view.flags;
    }
    owned=scene_instance_store_find(&scene_instances,output);
    if(!owned || context->fault){
        context->fault=1;context->fault_address=output;return 0;
    }
    owned->flags=instance.flags;
    if(!scene_projection_storage_project_holder(owned)){
        context->fault=1;context->fault_address=output;return 0;
    }
    return context->fault?0:output;
}

typedef struct SceneReleaseScan {
    uint32_t holders[4096];
    uint32_t count;
} SceneReleaseScan;

/* Check the entire graph before mutation. Mixed children/descriptors retain
 * the CPU destructor, which still knows their virtual ownership contract. */
static int release_graph_ready(uint32_t object,SceneReleaseScan *scan){
    SceneInstance *owned;uint32_t descriptor,i;
    if(!native_holder(object) ||
       !scene_projection_storage_references(object) ||
       scan->count>=4096u)return 0;
    for(i=0;i<scan->count;i++)if(scan->holders[i]==object)return 0;
    scan->holders[scan->count++]=object;
    owned=scene_instance_store_find(&scene_instances,object);
    if(!owned)return 0;
    descriptor=owned->descriptor;
    if((owned->previous_child && !owned->previous_child_node) ||
       (owned->last_child && !owned->last_child_node))return 0;
    if(descriptor &&
       !scene_projection_storage_can_release_descriptor(descriptor))
        return 0;
    if(owned->previous_child_node &&
       !release_graph_ready(owned->previous_child_node->holder,scan))return 0;
    if(owned->last_child_node &&
       !release_graph_ready(owned->last_child_node->holder,scan))return 0;
    return 1;
}

static int release_holder_native(uint32_t object,int retain_holder){
    SceneInstance *owned=scene_instance_store_find(&scene_instances,object);
    SceneInstance *previous,*child;
    uint32_t descriptor,base_vtable=0x00571fa0u;
    if(!owned)return 0;
    previous=owned->previous_child_node;child=owned->last_child_node;
    descriptor=owned->descriptor;
    ++instance_stats.native_release_links;
    if(previous && !release_holder_native(previous->holder,0))return 0;
    if(child && !release_holder_native(child->holder,0))return 0;
    if(descriptor){
        if(scene_projection_storage_release_descriptor(descriptor)!=1 ||
           !scene_projection_storage_free(descriptor))return 0;
        if(owner_services.prune_models) owner_services.prune_models(owner_services.context);
    }
    if(retain_holder){
        if(!scene_projection_storage_write(object,&base_vtable,
            sizeof(base_vtable)))return 0;
    }else if(!scene_projection_storage_free(object))return 0;
    if(scene_instance_store_remove(&scene_instances,object)){
        ++instance_stats.released;--instance_stats.live;
    }
    return 1;
}
static int scene_instance_owner_destruct(uint32_t holder,int retain_holder){
    SceneReleaseScan scan={0};
    if(!native_holder(holder) ||
       !release_graph_ready(holder,&scan))return 0;
    ++instance_stats.native_release_graphs;
    if(!release_holder_native(holder,retain_holder))return -1;
    if(owner_services.forget_model) owner_services.forget_model(owner_services.context,holder);
    return 1;
}
int scene_instance_owner_release_graph(uint32_t holder){
    return scene_instance_owner_destruct(holder,0);
}
static int discard_clone_ready(uint32_t holder,SceneReleaseScan *scan){
    SceneInstance *owned;SceneDescriptorState descriptor;uint32_t i;
    if(scan->count>=4096u)return 0;
    for(i=0u;i<scan->count;i++)if(scan->holders[i]==holder)return 0;
    owned=scene_instance_store_find(&scene_instances,holder);
    if(!owned || !owned->source_holder || !native_holder(holder) ||
       !scene_projection_storage_references(holder) ||
       (owned->previous_child && !owned->previous_child_node) ||
       (owned->last_child && !owned->last_child_node))return 0;
    if(owned->descriptor &&
       !scene_projection_storage_descriptor(owned->descriptor,&descriptor))return 0;
    scan->holders[scan->count++]=holder;
    if(owned->previous_child_node &&
       !discard_clone_ready(owned->previous_child_node->holder,scan))return 0;
    if(owned->last_child_node &&
       !discard_clone_ready(owned->last_child_node->holder,scan))return 0;
    return 1;
}
int scene_instance_owner_discard_clone(uint32_t holder){
    SceneReleaseScan scan={0};
    if(!discard_clone_ready(holder,&scan))return 0;
    while(scan.count){
        uint32_t object=scan.holders[--scan.count];
        SceneInstance *owned=scene_instance_store_find(&scene_instances,object);
        if(!owned || (owned->descriptor &&
           !scene_descriptor_discard_clone_native(owned->descriptor)) ||
           !scene_projection_storage_free(object))return -1;
        scene_instance_owner_remove_record(object);
    }
    if(owner_services.prune_models)owner_services.prune_models(owner_services.context);
    if(owner_services.forget_model)owner_services.forget_model(owner_services.context,holder);
    return 1;
}
int scene_instance_owner_release_mesh_graph(uint32_t holder,
    const SceneMesh *mesh){
    SceneInstance *instance=scene_instance_store_find(&scene_instances,holder);
    if(!instance || !mesh || instance->mesh!=mesh)return -1;
    return scene_instance_owner_destruct(holder,0);
}
int scene_instance_owner_destruct_graph(uint32_t holder){
    return scene_instance_owner_destruct(holder,1);
}


int scene_instance_owner_is_holder(uint32_t holder) { return native_holder(holder); }
SceneInstance *scene_instance_owner_find(uint32_t holder) { return scene_instance_store_find(&scene_instances,holder); }
int scene_instance_owner_remove_record(uint32_t holder) {
    if(!scene_instance_store_remove(&scene_instances,holder)) return 0;
    ++instance_stats.released; --instance_stats.live; return 1;
}
void scene_instance_owner_note_release_link(void) { ++instance_stats.native_release_links; }
static uint32_t clone_descriptor_port(void *context, uint32_t descriptor) {
    return scene_descriptor_clone_native(context,descriptor);
}
int scene_instance_owner_clone(GameMemory *memory,uint32_t source,uint32_t *output) {
    SceneCloneContext context;
    memset(&context,0,sizeof(context));context.external_context=memory;
    context.clone_external=clone_descriptor_port;
    if(!memory || !output || memory->fault)return -1;
    *output=0u;
    if(!native_holder(source))return 0;
    scene_projection_storage_observe_objects(&scene_instances,scene_instance_projection_written);
    *output=clone_holder(&context,source,0u,0u,0u);
    if(context.fault) {
        while(context.count){
            uint32_t holder=context.allocated[--context.count];
            SceneInstance *instance=scene_instance_store_find(&scene_instances,holder);
            if(instance && instance->descriptor)
                scene_descriptor_discard_clone_native(instance->descriptor);
            scene_projection_storage_free(holder);
            scene_instance_owner_remove_record(holder);
        }
        free(context.allocated);*output=0u;
        if(owner_services.prune_models)owner_services.prune_models(owner_services.context);
        if(!memory->fault) { memory->fault=1; memory->fault_address=context.fault_address;
            memory->fault_size=1u; memory->fault_write=1; }
        return -1;
    }
    scene_instance_store_resolve_links(&scene_instances);
    free(context.allocated);
    return 1;
}
