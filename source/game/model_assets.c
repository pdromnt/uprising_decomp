#include "model_assets.h"
#include "effect_model.h"
#include "trail_model.h"
#include "mesh_buffer.h"
#include "game_matrix.h"
#include "asset_sdf.h"
#include "asset_collision.h"
#include "gameplay_geometry.h"
#include "level_factory.h"
#include "render_texture_cache.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
typedef struct MaterialView {struct GameModelAssets *owner;size_t index;GameAddress id;} MaterialView;
typedef struct ChildView {struct GameModelAssets *owner;int category;} ChildView;
typedef struct PartPublication {GameAddress count_address,array_address,page;struct PartPublication *next;} PartPublication;
enum { MATERIAL_VIEW_CAPACITY=1024 }; /*Room for both original512-entry representations. */
struct GameModelAssets {MaterialCache materials;MaterialView views[MATERIAL_VIEW_CAPACITY];TextureStore textures;uint32_t next_key,ui_tile;
    SceneChildRegistry children;GameAddress child_primary,child_secondary;ChildView child_views[2];PartPublication *parts;};
typedef struct ModelStream {const uint8_t *data;size_t size,position;} ModelStream;
static size_t stream_read(void *context,void *out,size_t count) {
    ModelStream *s=context;size_t available=s->size-s->position;
    if(count>available)count=available;
    memcpy(out,s->data+s->position,count);s->position+=count;return count;
}
static struct GameModelAssets *assets(GameApplication *a) {
    if(!a->model_assets)a->model_assets=calloc(1,sizeof(*a->model_assets));
    return a->model_assets;
}
/*639b04/639afc are bounded views of the sole native registry. There are no
 *serialized copies to synchronize during asset loading or rendering. */
static int child_read(void *context,uint32_t offset,void *out,size_t count){ChildView *view=context;struct GameModelAssets *s=view->owner;
    uint8_t *bytes=out;size_t i;uint64_t size=(uint64_t)s->children.capacity*4;
    if(offset>size || count>size-offset)return 0;
    for(i=0;i<count;++i){uint32_t at=offset+(uint32_t)i;const SceneChildRecord *r=&s->children.records[at/4];
        uint32_t value=view->category?r->category:r->holder_id;bytes[i]=(uint8_t)(value>>((at%4)*8));}return 1;
}
static int child_write(void *context,uint32_t offset,const void *input,size_t count){uint8_t existing[64];
    /*Ownership changes go through native registry operations. Retained readers
     *may issue identical stores, but cannot manufacture foreign mesh owners. */
    return count<=sizeof(existing) && child_read(context,offset,existing,count) && !memcmp(existing,input,count);
}
static int child_pages(struct GameModelAssets *s,uint32_t capacity,GameAddress pages[2]){
    unsigned i;pages[0]=pages[1]=0;if(!capacity || capacity>UINT32_MAX/4)return 0;
    for(i=0;i<2;++i){pages[i]=scene_projection_storage_allocate((size_t)capacity*4,"native model child view");
        s->child_views[i].owner=s;s->child_views[i].category=(int)i;
        if(!pages[i] || !scene_projection_storage_bind_state(pages[i],&s->child_views[i],child_read,child_write))goto fail;
    }return 1;
fail:for(i=0;i<2;++i)if(pages[i])scene_projection_storage_free(pages[i]);pages[0]=pages[1]=0;return 0;
}
int game_level_texture_profiles_release(GameApplication *a){struct GameModelAssets *s=a->model_assets;GameMemory cleanup=a->memory;unsigned i;cleanup.fault=0;
    if(s){for(i=0;i<s->children.count;++i){SceneChildRecord *child=&s->children.records[i];
            if(child->holder_id && scene_instance_owner_release_graph(child->holder_id)!=1)return 0;
            child->holder_id=child->external_id=0;child->mesh=NULL;}
        if(s->child_primary && !scene_projection_storage_free(s->child_primary))return 0;
        if(s->child_secondary && !scene_projection_storage_free(s->child_secondary))return 0;
        s->child_primary=s->child_secondary=0;scene_child_registry_release(&s->children);
    }
    game_memory_write_u32(&cleanup,0x639b04,0);game_memory_write_u32(&cleanup,0x639afc,0);
    game_memory_write_u32(&cleanup,0x639af8,0);game_memory_write_u32(&cleanup,0x639b00,0);return !cleanup.fault;
}
int game_level_texture_profiles_initialize(GameApplication *a){struct GameModelAssets *s=assets(a);GameAddress pages[2];
    if(!s || a->memory.fault || !game_level_texture_profiles_release(a) || !scene_child_registry_initialize(&s->children,36))return 0;
    if(!child_pages(s,36,pages)){scene_child_registry_release(&s->children);return 0;}
    s->child_primary=pages[0];s->child_secondary=pages[1];game_memory_write_u32(&a->memory,0x639b04,pages[0]);
    game_memory_write_u32(&a->memory,0x639afc,pages[1]);game_memory_write_u32(&a->memory,0x639b00,36);game_memory_write_u32(&a->memory,0x639af8,0);
    return !a->memory.fault;
}
static uint32_t child_count(void *context){GameApplication *a=context;return a->model_assets?a->model_assets->children.count:0;}
static int child_append(void *context,GameMemory *memory,SceneChildRecord record){GameApplication *a=context;struct GameModelAssets *s=assets(a);
    GameAddress pages[2]={0},old[2];uint32_t capacity,i;int grow;
    if(!s || !record.external_id || record.mesh || memory->fault)return 0;
    if(!s->children.records && !game_level_texture_profiles_initialize(a))return 0;
    capacity=s->children.capacity;grow=s->children.count==capacity;
    if(grow && (capacity>UINT32_MAX/8 || !child_pages(s,capacity*2,pages)))return 0;
    if(!scene_child_registry_append(&s->children,record)){if(grow){scene_projection_storage_free(pages[0]);scene_projection_storage_free(pages[1]);}return 0;}
    if(grow){for(i=s->children.count;i<s->children.capacity;++i)s->children.records[i].category=UINT32_MAX;
        old[0]=s->child_primary;old[1]=s->child_secondary;s->child_primary=pages[0];s->child_secondary=pages[1];
        game_memory_write_u32(memory,0x639b04,pages[0]);game_memory_write_u32(memory,0x639afc,pages[1]);game_memory_write_u32(memory,0x639b00,s->children.capacity);
        (void)scene_projection_storage_free(old[0]);(void)scene_projection_storage_free(old[1]);}
    game_memory_write_u32(memory,0x639af8,s->children.count);return !memory->fault;
}
static int child_bind(void *context,uint32_t index,GameAddress holder,SceneMesh *mesh){GameApplication *a=context;
    return a->model_assets && scene_child_registry_bind_mesh(&a->model_assets->children,index,holder,mesh);
}
static int child_rollback(void *context,GameMemory *memory,uint32_t count){GameApplication *a=context;struct GameModelAssets *s=a->model_assets;
    if(!s || count>s->children.count)return 0;
    while(s->children.count>count){SceneChildRecord *r=&s->children.records[s->children.count-1];
        r->holder_id=r->external_id=0;r->category=UINT32_MAX;r->mesh=NULL;scene_child_registry_remove_last(&s->children);}
    GameMemory cleanup=*memory;cleanup.fault=0;game_memory_write_u32(&cleanup,0x639af8,count);return !cleanup.fault;
}
typedef struct DefinitionMesh {SceneMeshProjection projection;struct DefinitionMesh *next;} DefinitionMesh;
typedef struct DefinitionLoad {GameApplication *app;DefinitionMesh *meshes;} DefinitionLoad;
static int definition_read(void *context,const char *name,AssetModelFormat format,AssetModel *output){DefinitionLoad *load=context;GameApplication *a=load->app;
    ResourceBlob blob={0};ModelStream bytes;ResourceStream stream={0};int ok=0;
    if(game_application_load_blob(a,name,&blob)){bytes.data=blob.data;bytes.size=blob.size;bytes.position=0;stream.context=&bytes;stream.read=stream_read;
        ok=asset_model_read_format(&stream,format,output);}
    resource_blob_release(&blob);return ok;
}
static GameAddress definition_mesh(void *context,SceneMesh *mesh,unsigned kind,SceneMeshProjection *projection){DefinitionLoad *load=context;DefinitionMesh *entry=calloc(1,sizeof(*entry));GameAddress holder;
    if(!entry)return 0;holder=game_model_mesh_project(load->app,mesh,kind,projection);
    if(!holder){free(entry);return 0;}entry->projection=*projection;entry->next=load->meshes;load->meshes=entry;return holder;
}
static uint32_t definition_child_count(void *context){return child_count(((DefinitionLoad *)context)->app);}
static int definition_child_append(void *context,GameMemory *memory,SceneChildRecord child){return child_append(((DefinitionLoad *)context)->app,memory,child);}
static int definition_child_bind(void *context,uint32_t index,GameAddress holder,SceneMesh *mesh){return child_bind(((DefinitionLoad *)context)->app,index,holder,mesh);}
static int definition_child_rollback(void *context,GameMemory *memory,uint32_t count){return child_rollback(((DefinitionLoad *)context)->app,memory,count);}
int game_model_asset_definition_load(GameApplication *a,const SceneAssetRequest *request){
    DefinitionLoad load={a,NULL};DefinitionMesh *entry;int ok;
    SceneAssetServices services={&load,definition_read,definition_mesh,definition_child_count,definition_child_append,definition_child_bind,definition_child_rollback};
    if(!a || !request || a->memory.fault)return 0;
    ok=scene_asset_load(&a->memory,request,&services);
    while((entry=load.meshes)!=NULL){SceneDescriptorState state;load.meshes=entry->next;
        if(!ok){scene_instance_owner_forget_source(entry->projection.holder);
            if(scene_projection_storage_descriptor(entry->projection.descriptor,&state) &&
                scene_projection_storage_release_descriptor(entry->projection.descriptor)==1)
                (void)scene_projection_storage_free(entry->projection.descriptor);
            (void)scene_projection_storage_free(entry->projection.holder);}
        free(entry);
    }
    return ok;
}
int game_model_definition_release(GameApplication *a,GameModelDefinition *d){
    GameMemory cleanup=a->memory;uint32_t i;GameAddress *pages[4]={&d->markers,&d->points,&d->secondary_points,&d->parts};cleanup.fault=0;
    if(d->model){if(!game_application_scene_model_release(a,d->model))return 0;d->model=0;}
    if(d->format==ASSET_MODEL_COLLISION && d->markers)for(i=0;i<d->marker_count;++i){unsigned j;
        for(j=0;j<2;++j){GameAddress at=d->markers+i*48+16+j*4,page=game_memory_read_u32(&cleanup,at);
            if(page && !scene_projection_storage_free(page))return 0;game_memory_write_u32(&cleanup,at,0);}}
    /*Model retirement detaches auxiliary pages; the consuming definition owns
     *their physical release. Parts borrow cached child meshes, not their pages. */
    for(i=0;i<4;++i)if(*pages[i]){if(!scene_projection_storage_free(*pages[i]))return 0;*pages[i]=0;}
    memset(d,0,sizeof(*d));return !cleanup.fault;
}
int game_model_definition_read(GameApplication *a,AssetModelFormat format,uint16_t slot,GameAddress names,
        int modify,int points,int parts,GameModelDefinition *out){
    SceneAssetRequest request={0};GameAddress work;unsigned i;int ok;GameModelDefinition d={0};
    if(!a || !out || a->memory.fault || (format!=ASSET_MODEL_SIMPLE && format!=ASSET_MODEL_COLLISION))return 0;
    d.format=format;work=scene_projection_storage_allocate(64,"model definition outputs");if(!work)return 0;
    request.format=format;request.index=slot;request.names=names;
    /*These are bounded result slots, not an emulated stack/calling convention. */
    request.outputs[0]=work;request.outputs[1]=work+12;
    request.outputs[2]=work+16;request.outputs[3]=work+20;request.outputs[4]=work+24;
    if(points){request.outputs[5]=work+28;request.outputs[6]=work+32;}
    if(format==ASSET_MODEL_COLLISION){request.outputs[7]=work+36;request.outputs[8]=work+40;}
    if(parts){unsigned at=format==ASSET_MODEL_SIMPLE?7:9;request.outputs[at]=work+44;request.outputs[at+1]=work+48;}
    ok=game_model_asset_definition_load(a,&request);
    if(ok){
        for(i=0;i<3;++i){uint32_t bits=game_memory_read_u32(&a->memory,work+i*4);memcpy(&d.dimensions[i],&bits,4);}
        d.radius=game_memory_read_u32(&a->memory,work+12);
        if(format==ASSET_MODEL_SIMPLE){d.markers=game_memory_read_u32(&a->memory,work+16);d.marker_count=game_memory_read_u32(&a->memory,work+20);}
        else {d.marker_count=game_memory_read_u32(&a->memory,work+16);d.markers=game_memory_read_u32(&a->memory,work+20);}
        d.model=game_memory_read_u32(&a->memory,work+24);
        d.point_count=game_memory_read_u32(&a->memory,work+28);d.points=game_memory_read_u32(&a->memory,work+32);
        d.secondary_count=game_memory_read_u32(&a->memory,work+36);d.secondary_points=game_memory_read_u32(&a->memory,work+40);
        d.part_count=game_memory_read_u32(&a->memory,work+44);d.parts=game_memory_read_u32(&a->memory,work+48);
    }
    scene_projection_storage_free(work);
    if(!ok){if(a->memory.fault)return 0;return game_model_text_definition_load(a,format,slot,names,modify,points,parts,out);}
    if(a->memory.fault){game_model_definition_release(a,&d);return 0;}
    *out=d;return 1;
}
int game_level_presentation_describe(GameApplication *a,const GamePresentationFactoryFamily *family,uint16_t slot,
        float dimensions[3],GamePresentationDefinition *out){
    struct GameModelAssets *s;PartPublication *entry;GameModelDefinition d={0};GamePresentationDefinition result={0};
    if(!family || slot>=family->slots || !out || !dimensions || !(s=assets(a)))return 0;
    /*The family tables own the32-byte child recipe pages; the presentation
     *owns48-byte collision rows and both vector arrays. No writable mirrors. */
    entry=calloc(1,sizeof(*entry));if(!entry)return -1;
    if(!game_model_definition_read(a,ASSET_MODEL_COLLISION,slot,family->names,0,1,1,&d)){free(entry);return a->memory.fault?-1:0;}
    if(d.marker_count>32767u || d.secondary_count>65536u || d.point_count>65536u){game_model_definition_release(a,&d);free(entry);return -1;}
    result.radius=d.radius;result.record_count=(uint16_t)d.marker_count;result.records=d.markers;
    result.model=d.model;result.data_count=d.point_count;result.data=d.points;
    result.slot_count=(int32_t)d.secondary_count;result.positions=d.secondary_points;
    entry->count_address=family->primary+slot*4u;entry->array_address=family->secondary+slot*4u;entry->page=d.parts;
    game_memory_write_u32(&a->memory,entry->count_address,d.part_count);
    game_memory_write_u32(&a->memory,entry->array_address,d.parts);
    if(a->memory.fault){game_model_definition_release(a,&d);free(entry);return -1;}
    entry->next=s->parts;s->parts=entry;memcpy(dimensions,d.dimensions,sizeof(d.dimensions));*out=result;return 1;
}
/*449600/449d00: the existing decoder owns all meshes and publishes the same
 *multipart hull, groups, anchors and child recipe cache consumed by the classes. */
static int multipart_describe(GameApplication *a,uint16_t slot,int eight,PresentationArticulatedDefinition *out){
    SceneAssetRequest request={0};PresentationArticulatedDefinition d={0};
    struct GameModelAssets *s;PartPublication *entry;GameModelDefinition cleanup={0};
    GameAddress work;unsigned i,j,groups=eight?4:3,anchors=eight?6:3;
    uint32_t bits,part_count;int ok;
    if(!a || !out || slot>=(eight?2:6) || a->memory.fault || !(s=assets(a)))return 0;
    entry=calloc(1,sizeof(*entry));if(!entry)return -1;
    work=scene_projection_storage_allocate(128,"multipart asset results");if(!work){free(entry);return -1;}
    request.format=eight?ASSET_MODEL_EIGHT_MESH:ASSET_MODEL_THREE_MESH;
    request.index=slot;request.names=eight?0x5848b8:0x5848a0;
    request.outputs[0]=work;request.outputs[1]=work+12;request.outputs[2]=work+16;
    for(i=0;i<groups;i++)request.outputs[3+i]=work+20+i*4;
    for(i=0;i<anchors;i++)request.outputs[3+groups+i]=work+36+i*12;
    if(eight){request.outputs[13]=work+108;request.outputs[14]=work+112;}
    request.outputs[eight?15:9]=work+116;
    request.outputs[eight?16:12]=work+120;request.outputs[eight?17:13]=work+124;
    ok=game_model_asset_definition_load(a,&request);
    if(!ok){scene_projection_storage_free(work);free(entry);return a->memory.fault?-1:0;}
    for(i=0;i<3;i++){bits=game_memory_read_u32(&a->memory,work+i*4);memcpy(&d.base.dimensions[i],&bits,4);}
    d.base.radius=game_memory_read_u32(&a->memory,work+12);d.base.resource=game_memory_read_u32(&a->memory,work+16);
    d.base.model=game_memory_read_u32(&a->memory,work+116);d.base.type=eight?0x88:0x89;
    for(i=0;i<groups;i++)d.groups[i]=game_memory_read_u32(&a->memory,work+20+i*4);
    for(i=0;i<anchors;i++)for(j=0;j<3;j++){bits=game_memory_read_u32(&a->memory,work+36+i*12+j*4);memcpy(&d.anchors[i][j],&bits,4);}
    if(eight)for(i=0;i<2;i++){bits=game_memory_read_u32(&a->memory,work+108+i*4);memcpy(&d.spans[i],&bits,4);}
    part_count=game_memory_read_u32(&a->memory,work+120);entry->page=game_memory_read_u32(&a->memory,work+124);
    entry->count_address=(eight?0x5acf10:0x626660)+slot*4;
    entry->array_address=(eight?0x5acf00:0x626678)+slot*4;
    game_memory_write_u32(&a->memory,entry->count_address,part_count);
    game_memory_write_u32(&a->memory,entry->array_address,entry->page);
    scene_projection_storage_free(work);
    if(a->memory.fault){cleanup.format=request.format;cleanup.model=d.base.model;cleanup.markers=d.base.resource;cleanup.parts=entry->page;
        (void)game_model_definition_release(a,&cleanup);free(entry);return -1;}
    entry->next=s->parts;s->parts=entry;*out=d;return 1;
}
int game_level_articulated_describe(GameApplication *a,uint16_t slot,PresentationArticulatedDefinition *out){
    return multipart_describe(a,slot,1,out);
}
int game_level_spatial_describe(GameApplication *a,uint16_t slot,SpatialSampleDefinition *out){
    PresentationArticulatedDefinition d={0};int result;
    if(!out)return 0;
    result=multipart_describe(a,slot,0,&d);if(result<=0)return result;
    memset(out,0,sizeof(*out));out->base=d.base;memcpy(out->groups,d.groups,sizeof(out->groups));
    memcpy(out->anchors,d.anchors,sizeof(out->anchors));return 1;
}
/*An eight-byte bounded view exposes the cache's sole metadata owner. Never
 * copy host struct padding or maintain another writable material record. */
static int material_read(void *context,uint32_t offset,void *out,size_t count) {
    MaterialView *v=context;MaterialValue value;uint8_t bytes[8];unsigned i;
    if(v->index>=v->owner->materials.count || offset>8 || count>8-offset)return 0;
    value=v->owner->materials.entries[v->index].value;
    bytes[0]=(uint8_t)value.width;bytes[1]=(uint8_t)(value.width>>8);
    bytes[2]=(uint8_t)value.height;bytes[3]=(uint8_t)(value.height>>8);
    for(i=0;i<4;++i)bytes[4+i]=(uint8_t)(value.backend_handle>>(i*8));
    memcpy(out,bytes+offset,count);return 1;
}
static int material_write(void *context,uint32_t offset,const void *input,size_t count) {
    MaterialView *v=context;MaterialValue *value;uint8_t bytes[8];unsigned i;
    if(!material_read(context,0,bytes,8) || offset>8 || count>8-offset)return 0;
    memcpy(bytes+offset,input,count);value=&v->owner->materials.entries[v->index].value;
    value->width=(uint16_t)((uint32_t)bytes[0]|((uint32_t)bytes[1]<<8));
    value->height=(uint16_t)((uint32_t)bytes[2]|((uint32_t)bytes[3]<<8));
    value->backend_handle=0;for(i=0;i<4;++i)value->backend_handle|=(uint32_t)bytes[4+i]<<(i*8);
    return 1;
}
const TextureRecord *game_model_texture(GameApplication *a,uint32_t id) {
    return a->model_assets?texture_store_find_id(&a->model_assets->textures,id):NULL;
}
TextureRecord *game_model_texture_record(GameApplication *a,uint32_t id){
    return a->model_assets?texture_store_find_id(&a->model_assets->textures,id):NULL;
}
void game_model_texture_residency_clear(GameApplication *a){
    TextureRecord *record;if(!a->model_assets)return;
    for(record=a->model_assets->textures.first;record;record=record->next)record->flags&=0x7fffffffu;
}
/*Exclusive decoded image owners retire here; cached mesh materials
 * retain their existing application lifetime. */
int game_model_texture_release(GameApplication *a,uint32_t id) {
    const TextureRecord *record=game_model_texture(a,id);
    return record?(game_render_texture_forget(&a->memory,id) && texture_store_remove(&a->model_assets->textures,record->key)):id==0;
}
extern int game_menu_palette_read_bytes(GameApplication *,uint8_t[1024]);
int game_model_texture_take(GameApplication *a,AssetTexture *image,uint32_t flags,MaterialValue *value) {
    struct GameModelAssets *s=assets(a);TextureRecord *record;uint32_t key;
    if(!s || s->next_key==UINT32_MAX)return 0;
    key=++s->next_key;record=texture_store_take(&s->textures,key,flags|4u,image);
    if(!record)return 0;
    value->width=(uint16_t)record->image.render_width;value->height=(uint16_t)record->image.render_height;
    value->backend_handle=record->id;return 1;
}
/* One mutable UI tile lives in the existing texture owner, not a per-blit
 * allocation or a serialized pixel mirror. Bulk owner disposal retires it. */
TextureRecord *game_model_ui_tile_texture(GameApplication *a){
    struct GameModelAssets *s=assets(a);TextureRecord *record;AssetTexture image={0};MaterialValue value={0};
    if(!s)return NULL;
    record=game_model_texture_record(a,s->ui_tile);if(record)return record;
    image.width=image.height=image.render_width=image.render_height=32;
    image.lod=3;image.aspect=3;image.format=5;image.size=1024;image.pixels=calloc(1,1024);
    if(!image.pixels || !game_model_texture_take(a,&image,0,&value)){asset_texture_release(&image);return NULL;}
    s->ui_tile=value.backend_handle;return game_model_texture_record(a,s->ui_tile);
}
int game_model_software_material_load(GameApplication *a,const char *path,MaterialValue *value) {
    ResourceBlob blob={0};AssetTexture image={0};int ok=0;
    if(game_application_load_blob(a,path,&blob) && asset_texture_software_bitmap(&blob,&image))
        ok=game_model_texture_take(a,&image,0,value);
    resource_blob_release(&blob);asset_texture_release(&image);return ok;
}
/*439a20/439a80's normal bitmap path: decode real pixels and publish their
 * existing native texture owner. Residency/upload remain backend consumers. */
int game_model_bitmap_material_load(GameApplication *a,const char *path,int alpha,uint32_t flags,MaterialValue *value) {
    ResourceBlob blob={0};AssetTexture image={0};uint8_t palette[1024];int ok=0;
    if(alpha < -1 || alpha>2 || a->memory.fault)return 0;
    /* Both luminance (0) and inverse-luminance (2) expansion consume the
     * current palette. Texture flags at439a20 are a separate parameter. */
    if((alpha==0 || alpha==2) && !game_menu_palette_read_bytes(a,palette))return 0;
    if(game_application_load_blob(a,path,&blob) && asset_texture_bitmap(&blob,alpha,(alpha==0 || alpha==2)?palette:NULL,&image))
        ok=game_model_texture_take(a,&image,flags,value);
    resource_blob_release(&blob);asset_texture_release(&image);return ok;
}
int game_model_material_load(GameApplication *a,const char *path,int alpha,uint32_t flags,int hardware,MaterialValue *value) {
    if(!hardware)return game_model_software_material_load(a,path,value);
    if(game_memory_read_u32(&a->memory,0x5a0804) || game_memory_read_u32(&a->memory,0x5a0808))
        return game_model_cached_material_load(a,path,alpha,flags,value);
    return game_model_bitmap_material_load(a,path,alpha,flags,value);
}
/*Shared effect-profile producer: pixels are copied into that same texture
 * store, including the original coverage/luminance format selection. */
uint32_t game_level_texture_from_indexed(GameApplication *a,const uint8_t *pixels,uint32_t width,uint32_t height,uint32_t flags) {
    AssetTexture image={0};MaterialValue value={0};uint8_t palette[1024];uint64_t size=(uint64_t)width*height;
    int alpha=(flags&16)?((flags&8)?0:((flags&32)?2:1)):-1,ok;
    if(!size || size>SIZE_MAX || ((alpha==0 || alpha==2) && !game_menu_palette_read_bytes(a,palette)))return 0;
    if(!asset_texture_indexed(pixels,(size_t)size,width,height,alpha,(alpha==0 || alpha==2)?palette:NULL,&image))return 0;
    ok=game_model_texture_take(a,&image,flags,&value);asset_texture_release(&image);return ok?value.backend_handle:0;
}
static int transparent(const char *name) {
    static const char prefix[]="trans";unsigned i;
    for(i=0;i<5;++i){unsigned c=(unsigned char)name[i];if(c>='A' && c<='Z')c+=32;if(c!=(unsigned)prefix[i])return 0;}
    return 1;
}
GameAddress game_model_material_bind(GameApplication *a,const char *path,int alpha,MaterialValue *value) {
    struct GameModelAssets *s=assets(a);size_t index;MaterialView *view;
    char key[514];int hardware=game_memory_read_u32(&a->memory,0x5840f4)!=0,length;
    if(!s || !path || a->memory.fault)return 0;
    /*Software retains BMP dimensions/stride; hardware uses the upload-sized
     * image. They are different assets even when their source filename matches.
     * Keep both in the existing texture owner and give each its own stable view;
     * never replace metadata still borrowed by a live mesh on a mode switch. */
    length=snprintf(key,sizeof(key),"%c:%s",hardware?'h':'s',path);
    if(length<0 || (size_t)length>=sizeof(key))return 0;
    if(!material_cache_find(&s->materials,key,&index)) {
        if(s->materials.count>=MATERIAL_VIEW_CAPACITY || !game_model_material_load(a,path,alpha,0,hardware,value) ||
           !material_cache_insert(&s->materials,key,*value,&index))return 0;
    }
    view=&s->views[index];
    if(!view->id) {
        view->owner=s;view->index=index;view->id=scene_projection_storage_allocate(8,"native material view");
        if(!view->id)return 0;
        if(!scene_projection_storage_bind_state(view->id,view,material_read,material_write)) {
            scene_projection_storage_free(view->id);view->id=0;return 0;
        }
    }
    *value=s->materials.entries[index].value;return view->id;
}
/*402570/402e10: ordinary mesh publication and real material selection. The
 * retained SceneMesh and existing projection owner own all geometry/storage. */
GameAddress game_model_mesh_project(GameApplication *a,SceneMesh *scene,uint32_t kind,SceneMeshProjection *out) {
    const AssetMesh *mesh=scene->asset;SceneMeshProjectionPages pages={0};
    MaterialValue *values=NULL;GameAddress *ids=NULL,holder=0;uint8_t *flags=NULL;
    char path[512],label[256];unsigned i,length;static const char *const prefixes[]={"bitmaps\\","impbmps\\","rblbmps\\"};
    if(kind>2 || !assets(a))return 0;
    for(length=0;length<sizeof(label);++length) {
        label[length]=(char)game_memory_read_u8(&a->memory,0x583230+length);if(!label[length])break;
    }
    if(length==sizeof(label) || a->memory.fault)return 0;
    if(mesh->material_count) {
        ids=calloc(mesh->material_count,sizeof(*ids));values=calloc(mesh->material_count,sizeof(*values));flags=calloc(mesh->material_count,1);
        if(!ids || !values || !flags)goto done;
    }
    for(i=0;i<mesh->material_count;++i) {
        int n=snprintf(path,sizeof(path),"%s%s",prefixes[kind],mesh->materials[i]);
        if(n<0 || (size_t)n>=sizeof(path))goto done;
        flags[i]=(uint8_t)transparent(mesh->materials[i]);
        ids[i]=game_model_material_bind(a,path,flags[i]?1:-1,&values[i]);
        if(!ids[i] && kind && !a->memory.fault) {
            n=snprintf(path,sizeof(path),"bitmaps\\%s",mesh->materials[i]);
            if(n<0 || (size_t)n>=sizeof(path))goto done;
            ids[i]=game_model_material_bind(a,path,flags[i]?1:-1,&values[i]);
        }
        if(!ids[i] || a->memory.fault)goto done;
    }
    holder=scene_projection_storage_create_mesh(scene,label,length+1,ids,values,flags,mesh->material_count,&pages);
    if(holder && out) {
        out->holder=holder;out->descriptor=pages.descriptor;out->source=pages.source;
        out->transformed=pages.transformed;out->polygons=pages.polygons;out->links=pages.links;
        out->uv=pages.uv;out->uv_work=pages.uv_work;
    }
done:free(ids);free(values);free(flags);return holder;
}
/*402fd0's filename consumer: read the actual mesh and retain its native owner.
 * A t.bdm is a separate multi-mesh format; do not silently decode its first
 * mesh and call the missing transforms complete. */
GameAddress game_model_file_load(GameApplication *a,const char *name,uint32_t kind) {
    ResourceBlob blob={0};AssetMesh mesh={0};SceneMesh scene={0};SceneMeshProjection projection={0};
    ModelStream bytes;ResourceStream stream={0};GameAddress holder=0;uint32_t count,i;
    if(kind==UINT32_MAX)kind=0;
    if(!name || !game_application_load_blob(a,name,&blob))return 0;
    if(strstr(name,"t.bdm")) {holder=game_model_file_three_load(a,&blob,kind);goto done;}
    bytes.data=blob.data;bytes.size=blob.size;bytes.position=0;stream.context=&bytes;stream.read=stream_read;
    if(!asset_mesh_read(&stream,&mesh) || !scene_mesh_initialize(&scene,&mesh))goto done;
    count=scene.polygon_count;
    holder=game_model_mesh_project(a,&scene,kind,&projection);
    if(holder && !scene_model_bridge_take_loose_mesh(&mesh,&scene,&projection)) {
        if(scene_projection_storage_release_descriptor(projection.descriptor)==1)scene_projection_storage_free(projection.descriptor);
        scene_projection_storage_free(holder);holder=0;
    }
    if(holder)for(i=0;i<count;++i) {
        GameAddress flags=projection.polygons+i*40+8;
        uint32_t value=game_memory_read_u32(&a->memory,flags);
        game_memory_write_u32(&a->memory,flags,value|4u);
        if(a->memory.fault) {
            (void)game_application_scene_model_release(a,holder);holder=0;break;
        }
    }
done:scene_mesh_release(&scene);asset_mesh_release(&mesh);resource_blob_release(&blob);return holder;
}
/*402fd0's named t.bdm preview reads only this prefix, unlike the indexed
 * gameplay loader's longer points/parts tail. Each retained mesh has its own
 * existing owner; the root's native child links own the complete release graph. */
GameAddress game_model_file_three_load(GameApplication *a,const ResourceBlob *blob,uint32_t kind) {
    AssetMesh meshes[3]={{0}};SceneMesh scenes[3]={{0}};
    SceneMeshProjection projections[3]={{0}};GameAddress holders[3]={0};
    ModelStream bytes={blob->data,blob->size,0};ResourceStream stream={0};
    uint8_t metadata[20],anchors[24];uint32_t markers,i,j;uint64_t skip;int ok=0;
    stream.context=&bytes;stream.read=stream_read;
    for(i=0;i<3;++i)
        if(!asset_mesh_read(&stream,&meshes[i]) || !scene_mesh_initialize(&scenes[i],&meshes[i]))goto done;
    if(stream_read(&bytes,metadata,sizeof(metadata))!=sizeof(metadata))goto done;
    markers=(uint32_t)metadata[16]|((uint32_t)metadata[17]<<8)|
        ((uint32_t)metadata[18]<<16)|((uint32_t)metadata[19]<<24);
    skip=(uint64_t)markers*28u+8u;
    if(skip>bytes.size-bytes.position)goto done;
    bytes.position+=(size_t)skip;
    if(stream_read(&bytes,anchors,sizeof(anchors))!=sizeof(anchors))goto done;
    for(i=0;i<3;++i) {
        holders[i]=game_model_mesh_project(a,&scenes[i],kind,&projections[i]);
        if(!holders[i])goto done;
        if(!scene_model_bridge_take_loose_mesh(&meshes[i],&scenes[i],&projections[i])) {
            if(scene_projection_storage_release_descriptor(projections[i].descriptor)==1)
                scene_projection_storage_free(projections[i].descriptor);
            scene_projection_storage_free(holders[i]);holders[i]=0;goto done;
        }
    }
    /*All fresh local matrices are identity.403380 adds anchor0 to mesh1's
     * translation;4033e0 composes the identity/anchor1 matrix with mesh2. */
    for(i=1;i<3;++i) {
        float translated[12]={1,0,0,0,0,1,0,0,0,0,1,0},local[12];
        SceneInstance *instance=scene_instance_owner_find(holders[i]);
        game_memory_write_u32(&a->memory,holders[i]+0x40,holders[i-1]);
        game_memory_write_u32(&a->memory,holders[i-1]+0x4c,holders[i]);
        for(j=0;j<3;++j) {
            const uint8_t *p=anchors+(i-1)*12+j*4;
            uint32_t bits=(uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
            float value;memcpy(&value,&bits,4);
            translated[3+j*4]=value;
        }
        if(i==1) {
            memcpy(local,instance->local,sizeof(local));
            for(j=0;j<3;++j)local[3+j*4]=(float)((long double)local[3+j*4]+translated[3+j*4]);
        } else game_matrix_compose_values(local,translated,instance->local);
        for(j=0;j<12;++j) {
            uint32_t bits;memcpy(&bits,&local[j],4);
            game_memory_write_u32(&a->memory,holders[i]+4+j*4,bits);
        }
    }
    for(i=0;i<3;++i) {
        SceneInstance *instance=scene_instance_owner_find(holders[i]);
        for(j=0;j<instance->render.polygon_count;++j) {
            GameAddress flags=projections[i].polygons+j*40+8;
            game_memory_write_u32(&a->memory,flags,game_memory_read_u32(&a->memory,flags)|4u);
        }
    }
    if(a->memory.fault)goto done;
    ok=1;
done:
    if(!ok) {
        /*Detach first so a partially linked graph cannot release a sibling twice. */
        for(i=0;i<3;++i)if(holders[i]) {
            uint32_t zero=0;scene_projection_storage_write(holders[i]+0x40,&zero,4);
            scene_projection_storage_write(holders[i]+0x4c,&zero,4);
        }
        for(i=0;i<3;++i)if(holders[i])(void)game_application_scene_model_release(a,holders[i]);
    }
    for(i=0;i<3;++i){scene_mesh_release(&scenes[i]);asset_mesh_release(&meshes[i]);}
    return ok?holders[0]:0;
}
int game_application_scene_model_release(GameApplication *a,GameAddress model) {
    if(!model)return 1;
    if(scene_instance_owner_is_holder(model))return scene_instance_owner_release_graph(model)==1;
    if(effect_model_find(model))return effect_model_release(model);
    if(trail_model_find(model))return trail_model_release(model);
    if(mesh_buffer_is_native(&a->memory,model))return mesh_buffer_release(&a->memory,model,mesh_buffer_owned_services()) && scene_projection_storage_free(model);
    return 0;
}
static int parts_release(GameMemory *m,struct GameModelAssets *s){
    PartPublication *entry;
    while((entry=s->parts)!=NULL){
        /*One owner for the published recipe page. Keep the entry available
         *until release succeeds; shutdown can resume without a second free. */
        if(entry->page && !scene_projection_storage_free(entry->page))return 0;
        entry->page=0;game_memory_write_u32(m,entry->count_address,0);game_memory_write_u32(m,entry->array_address,0);
        if(m->fault)return 0;s->parts=entry->next;free(entry);
    }
    return !m->fault;
}
int game_model_factory_parts_release(GameApplication *a){
    return a && !a->memory.fault && (!a->model_assets || parts_release(&a->memory,a->model_assets));
}
void game_model_assets_dispose(GameApplication *a) {
    struct GameModelAssets *s=a->model_assets;TextureRecord *texture;GameMemory cleanup=a->memory;unsigned i;if(!s)return;cleanup.fault=0;
    if(!game_level_texture_profiles_release(a)){a->memory.fault=1;return;}
    for(texture=s->textures.first;texture;texture=texture->next)
        if(!game_render_texture_forget(&cleanup,texture->id)){a->memory.fault=1;return;}
    scene_model_bridge_clear();
    if(!parts_release(&cleanup,s)){a->memory.fault=1;return;}
    for(i=0;i<MATERIAL_VIEW_CAPACITY;++i)if(s->views[i].id)scene_projection_storage_free(s->views[i].id);
    material_cache_release(&s->materials);texture_store_release(&s->textures);free(s);a->model_assets=NULL;a->selection_texture=0;
}

static float sdf_real(uint32_t bits){float value;memcpy(&value,&bits,4);return value;}
static uint32_t sdf_word(float value){uint32_t bits;memcpy(&bits,&value,4);return bits;}
static uint32_t sdf_first(const AssetSdf *s,uint32_t node){
    uint32_t child,found;if(s->nodes[node].mesh!=UINT32_MAX)return node;
    for(child=s->nodes[node].last_child;child!=UINT32_MAX;child=s->nodes[child].previous_child)
        if((found=sdf_first(s,child))!=UINT32_MAX)return found;
    return UINT32_MAX;
}
static void sdf_bounds(const AssetSdf *s,uint32_t node,float lo[3],float hi[3],int *found){
    uint32_t index=s->nodes[node].mesh,i,j,child;
    if(index!=UINT32_MAX){const AssetMesh *m=&s->meshes[index].mesh;
        for(i=0;i<m->vertex_count;++i){float xyz[3]={sdf_real(m->vertices[i].x_bits),sdf_real(m->vertices[i].y_bits),sdf_real(m->vertices[i].z_bits)};
            for(j=0;j<3;++j){if(!*found || xyz[j]<lo[j])lo[j]=xyz[j];if(!*found || xyz[j]>hi[j])hi[j]=xyz[j];}*found=1;}}
    for(child=s->nodes[node].last_child;child!=UINT32_MAX;child=s->nodes[child].previous_child)sdf_bounds(s,child,lo,hi,found);
}
static void sdf_center(AssetSdf *s,uint32_t node,const float center[3]){
    uint32_t index=s->nodes[node].mesh,i,child;
    /*44fe60 visits shared descriptors repeatedly: retain that arithmetic,
     * rather than silently deduplicating the original graph's mutations. */
    if(index!=UINT32_MAX){AssetMesh *m=&s->meshes[index].mesh;for(i=0;i<m->vertex_count;++i){AssetMeshVertex *v=&m->vertices[i];
        v->x_bits=sdf_word((float)((long double)sdf_real(v->x_bits)-center[0]));
        v->y_bits=sdf_word((float)((long double)sdf_real(v->y_bits)-center[1]));
        v->z_bits=sdf_word((float)((long double)sdf_real(v->z_bits)-center[2]));}}
    for(child=s->nodes[node].last_child;child!=UINT32_MAX;child=s->nodes[child].previous_child)sdf_center(s,child,center);
}
/*Each published node transfers its copied immutable asset to the existing
 * loose-mesh owner. Parser nodes and strings die before the load returns. */
static GameAddress sdf_publish(GameApplication *a,const AssetSdf *s,uint32_t node,uint32_t selected_mesh,float bias,const AssetMeshVertex *extent_override){
    AssetMesh mesh={0};SceneMesh scene={0};SceneMeshProjectionPages pages={0};SceneMeshProjection projection={0};
    MaterialValue *values=NULL;GameAddress *ids=NULL,holder=0,child_holder=0,previous=0;uint8_t *flags=NULL;
    uint32_t index=s->nodes[node].mesh,child,i,j;const char *label="SDF group";int transferred=0;float radius=0,min_y=0,extent[3]={0};
    if(index!=UINT32_MAX){const AssetMesh *source=&s->meshes[index].mesh;label=s->meshes[index].name;
        mesh.material_count=source->material_count;mesh.vertex_count=source->vertex_count;mesh.triangle_count=source->triangle_count;
        mesh.materials=calloc(mesh.material_count,sizeof(*mesh.materials));values=calloc(mesh.material_count,sizeof(*values));ids=calloc(mesh.material_count,sizeof(*ids));flags=calloc(mesh.material_count,1);
        mesh.vertices=calloc(mesh.vertex_count,sizeof(*mesh.vertices));mesh.triangles=calloc(mesh.triangle_count,sizeof(*mesh.triangles));
        if((mesh.material_count && (!mesh.materials || !values || !ids || !flags)) || (mesh.vertex_count && !mesh.vertices) || (mesh.triangle_count && !mesh.triangles))goto done;
        for(i=0;i<mesh.material_count;++i){char path[512],file[256];size_t length=strlen(source->materials[i]);char *dot;
            if(length>=sizeof(file))goto done;memcpy(file,source->materials[i],length+1);dot=strrchr(file,'.');if(!dot || (size_t)(dot-file)+5>sizeof(file))goto done;strcpy(dot+1,"bmp");
            mesh.materials[i]=malloc(strlen(file)+1);if(!mesh.materials[i])goto done;strcpy(mesh.materials[i],file);
            if(snprintf(path,sizeof(path),"bitmaps\\%s",file)<0)goto done;flags[i]=(uint8_t)transparent(file);
            ids[i]=game_model_material_bind(a,path,flags[i]?1:-1,&values[i]);if(!ids[i])goto done;
        }
        for(i=0;i<mesh.vertex_count;++i){float xyz[3]={sdf_real(source->vertices[i].x_bits),sdf_real(source->vertices[i].y_bits),sdf_real(source->vertices[i].z_bits)};long double square;
            mesh.vertices[i].x_bits=sdf_word(xyz[0]);mesh.vertices[i].y_bits=sdf_word(xyz[1]);mesh.vertices[i].z_bits=sdf_word(xyz[2]);
            square=(long double)xyz[0]*xyz[0]+(long double)xyz[1]*xyz[1]+(long double)xyz[2]*xyz[2];
            if(index==selected_mesh){float r=(float)gameplay_geometry_square_root((float)square);if(r>radius)radius=r;
                if(!i || xyz[1]<min_y)min_y=xyz[1];for(j=0;j<3;++j)if(fabsf(xyz[j])>extent[j])extent[j]=fabsf(xyz[j]);}
        }
        mesh.radius_bits=sdf_word(radius);mesh.auxiliary_bits=sdf_word(min_y);
        mesh.center.x_bits=sdf_word(extent[0]);mesh.center.y_bits=sdf_word(extent[1]);mesh.center.z_bits=sdf_word(extent[2]);
        for(i=0;i<mesh.triangle_count;++i){AssetMeshTriangle *t=&mesh.triangles[i];*t=source->triangles[i];if(t->material>=mesh.material_count)goto done;
            for(j=0;j<3;++j){t->uv[j].u_bits=sdf_word((float)((long double)sdf_real(t->uv[j].u_bits)*((int16_t)values[t->material].width-bias)));
                t->uv[j].v_bits=sdf_word((float)((long double)sdf_real(t->uv[j].v_bits)*((int16_t)values[t->material].height-bias)));}}
    }
    if(extent_override)mesh.center=*extent_override;
    if(!scene_mesh_initialize(&scene,&mesh))goto done;
    holder=scene_projection_storage_create_mesh(&scene,label,strlen(label)+1,ids,values,flags,mesh.material_count,&pages);if(!holder)goto done;
    projection.holder=holder;projection.descriptor=pages.descriptor;projection.source=pages.source;projection.transformed=pages.transformed;
    projection.polygons=pages.polygons;projection.links=pages.links;projection.uv=pages.uv;projection.uv_work=pages.uv_work;
    if(!scene_model_bridge_take_loose_mesh(&mesh,&scene,&projection))goto done;transferred=1;
    for(i=0;i<12;++i)game_memory_write_u32(&a->memory,holder+4+i*4,sdf_word(s->nodes[node].local[i]));
    for(i=0;index!=UINT32_MAX && i<s->meshes[index].mesh.triangle_count;++i)
        game_memory_write_u32(&a->memory,pages.polygons+i*40+8,5);
    /*Publish in the same reverse child traversal used by54b0e0, preserving
     * last-child/previous-child links and native graph retirement. */
    for(child=s->nodes[node].last_child;child!=UINT32_MAX;child=s->nodes[child].previous_child){
        child_holder=sdf_publish(a,s,child,selected_mesh,bias,NULL);if(!child_holder)goto fail;
        game_memory_write_u32(&a->memory,child_holder+0x40,holder);
        if(previous){game_memory_write_u32(&a->memory,previous+0x44,child_holder);game_memory_write_u32(&a->memory,child_holder+0x48,previous);}
        else game_memory_write_u32(&a->memory,holder+0x4c,child_holder);previous=child_holder;child_holder=0;
    }
    if(a->memory.fault)goto fail;goto done;
fail:if(child_holder)(void)game_application_scene_model_release(a,child_holder);(void)game_application_scene_model_release(a,holder);holder=0;
done:
    if(holder && !transferred){if(scene_projection_storage_release_descriptor(pages.descriptor)==1)scene_projection_storage_free(pages.descriptor);scene_projection_storage_free(holder);holder=0;}
    free(values);free(ids);free(flags);scene_mesh_release(&scene);asset_mesh_release(&mesh);return holder;
}
/*44fca0: parse, detach the first mesh-bearing group, center its subtree,
 * publish ordinary native scene ownership, retire all temporary definitions. */
GameAddress game_campaign_text_model_load(GameApplication *a,const char *name){
    ResourceBlob blob={0};AssetSdf sdf={0};GameAddress result=0;uint32_t node;unsigned i;float lo[3]={0},hi[3]={0},center[3],bias;int found=0;
    bias=sdf_real(game_memory_read_u32(&a->memory,0x574d28));
    if(!a->memory.fault && game_application_load_blob(a,name,&blob) && asset_sdf_read(&blob,bias,&sdf)){
        node=sdf_first(&sdf,0);if(node!=UINT32_MAX){sdf_bounds(&sdf,node,lo,hi,&found);
            if(found){for(i=0;i<3;++i)center[i]=(float)(((long double)lo[i]+hi[i])*sdf_real(game_memory_read_u32(&a->memory,0x573210)));
                sdf_center(&sdf,node,center);result=sdf_publish(a,&sdf,node,sdf.nodes[node].mesh,bias,NULL);}}}
    asset_sdf_release(&sdf);resource_blob_release(&blob);return result;
}
static void definition_nodes(const AssetSdf *s,uint32_t node,uint32_t *ids,uint32_t *count){
    uint32_t child;if(s->nodes[node].mesh!=UINT32_MAX)ids[(*count)++]=node;
    for(child=s->nodes[node].last_child;child!=UINT32_MAX;child=s->nodes[child].previous_child)definition_nodes(s,child,ids,count);
}
static int definition_sdf(GameApplication *a,const char *name,const char *suffix,float bias,int optional,AssetSdf *out){
    ResourceBlob blob={0};char path[272];int size,ok;
    size=snprintf(path,sizeof(path),"models2\\%s%s.sdf",name,suffix);if(size<0 || (unsigned)size>=sizeof(path))return 0;
    if(!game_application_load_blob(a,path,&blob))return optional && !a->memory.fault;
    ok=asset_sdf_read(&blob,bias,out);resource_blob_release(&blob);return ok;
}
static float definition_radius(const AssetMesh *mesh,const float center[3]){
    uint32_t i;float radius=0;
    for(i=0;i<mesh->vertex_count;++i){float x=(float)((long double)sdf_real(mesh->vertices[i].x_bits)-center[0]);
        float y=(float)((long double)sdf_real(mesh->vertices[i].y_bits)-center[1]);float z=(float)((long double)sdf_real(mesh->vertices[i].z_bits)-center[2]);
        float distance=(float)gameplay_geometry_square_root((float)((long double)x*x+(long double)y*y+(long double)z*z));if(distance>radius)radius=distance;}
    return radius;
}
static void definition_mesh_box(const AssetMesh *mesh,float half,float center[3],float extent[3]){
    float low[3]={0},high[3]={0};uint32_t i;unsigned j;
    for(i=0;i<mesh->vertex_count;++i){float xyz[3]={sdf_real(mesh->vertices[i].x_bits),sdf_real(mesh->vertices[i].y_bits),sdf_real(mesh->vertices[i].z_bits)};
        for(j=0;j<3;++j){if(!i || xyz[j]<low[j])low[j]=xyz[j];if(!i || xyz[j]>high[j])high[j]=xyz[j];}}
    for(j=0;j<3;++j){center[j]=(float)(((long double)low[j]+high[j])*half);extent[j]=(float)((long double)high[j]-center[j]);}
}
static int definition_points(GameApplication *a,const AssetSdf *s,const float center[3],GameAddress *page,uint32_t *count){
    uint32_t *nodes=NULL,i,j,n=0;float position[3],extent[3],half=sdf_real(game_memory_read_u32(&a->memory,0x573210));int ok=0;
    if(!s->node_count){*count=0;*page=0;return 1;}
    nodes=malloc(s->node_count*sizeof(*nodes));if(!nodes)return 0;definition_nodes(s,0,nodes,&n);
    if(n && !(*page=scene_projection_storage_allocate(n*12,"definition points")))goto done;
    for(i=0;i<n;++i){definition_mesh_box(&s->meshes[s->nodes[nodes[i]].mesh].mesh,half,position,extent);
        for(j=0;j<3;++j)game_memory_write_u32(&a->memory,*page+i*12+j*4,sdf_word((float)((long double)position[j]-center[j])));}
    *count=n;ok=!a->memory.fault;
done:free(nodes);return ok;
}
static int definition_default_point(GameApplication *a,float y,GameAddress *page,uint32_t *count){
    *page=scene_projection_storage_allocate(12,"default definition point");if(!*page)return 0;*count=1;
    game_memory_write_u32(&a->memory,*page,0);game_memory_write_u32(&a->memory,*page+4,sdf_word(y));game_memory_write_u32(&a->memory,*page+8,0);return !a->memory.fault;
}
static int definition_hulls(GameApplication *a,AssetSdf *s,const float center[3],int modify,GameModelDefinition *d){
    uint32_t *nodes=NULL,n=0,i,j,k;AssetModelMarker *rows=NULL;float radius=0,zero[3]={0},half=sdf_real(game_memory_read_u32(&a->memory,0x573210));int ok=0;
    if(!s->node_count){d->radius=0;return 1;}
    sdf_center(s,0,center);nodes=malloc(s->node_count*sizeof(*nodes));if(!nodes)goto done;definition_nodes(s,0,nodes,&n);
    if(n>1){rows=calloc(n,sizeof(*rows));if(!rows)goto done;}
    for(i=0;i<n;++i){AssetMesh *mesh=&s->meshes[s->nodes[nodes[i]].mesh].mesh;float position[3],extent[3],distance=definition_radius(mesh,zero);
        if(distance>radius)radius=distance;
        if(rows){definition_mesh_box(mesh,half,position,extent);for(j=0;j<3;++j){rows[i].words[j]=sdf_word(extent[j]);rows[i].words[j+3]=sdf_word(position[j]);}
            rows[i].words[6]=sdf_word(definition_radius(mesh,position));}}
    d->radius=sdf_word(radius);
    if(rows){
        /*54ae50 selects descending radius.44af70 optionally reorders by Z. */
        for(i=0;i<n;++i){k=i;for(j=i+1;j<n;++j)if(sdf_real(rows[k].words[6])<sdf_real(rows[j].words[6]))k=j;
            if(k!=i){AssetModelMarker temp=rows[i];rows[i]=rows[k];rows[k]=temp;}}
        if(modify)for(i=0;i<n;++i){k=i;for(j=i+1;j<n;++j)if(sdf_real(rows[j].words[5])<sdf_real(rows[k].words[5]))k=j;
            if(k!=i){AssetModelMarker temp=rows[i];rows[i]=rows[k];rows[k]=temp;}}
        d->markers=scene_projection_storage_allocate(n*28,"definition hulls");if(!d->markers)goto done;d->marker_count=n;
        for(i=0;i<n;++i)for(j=0;j<7;++j)game_memory_write_u32(&a->memory,d->markers+i*28+j*4,rows[i].words[j]);
    }
    ok=!a->memory.fault;
done:free(rows);free(nodes);return ok;
}
static int definition_planes(GameApplication *a,const AssetModel *collision,GameModelDefinition *d){
    uint32_t i,j;GameAddress points,edges;d->marker_count=collision->plane_count;d->radius=collision->radius_bits;
    if(!d->marker_count)return 1;
    d->markers=scene_projection_storage_allocate(d->marker_count*48,"ASC collision planes");if(!d->markers)return 0;
    for(i=0;i<d->marker_count;++i){const AssetCollisionPlane *plane=collision->planes+i;GameAddress row=d->markers+i*48;
        game_memory_write_u16(&a->memory,row,(uint16_t)plane->vertex_count);game_memory_write_u32(&a->memory,row+4,plane->normal.x_bits);
        game_memory_write_u32(&a->memory,row+8,plane->normal.y_bits);game_memory_write_u32(&a->memory,row+12,plane->normal.z_bits);
        points=scene_projection_storage_allocate(plane->vertex_count*12,"ASC contour vertices");if(!points)return 0;
        game_memory_write_u32(&a->memory,row+16,points);
        edges=scene_projection_storage_allocate(plane->vertex_count*12,"ASC contour edge vectors");if(!edges)return 0;
        game_memory_write_u32(&a->memory,row+20,edges);
        for(j=0;j<plane->vertex_count;++j){const AssetMeshVertex *v=plane->vertices+j,*e=plane->secondary_vectors+j;
            game_memory_write_u32(&a->memory,points+j*12,v->x_bits);game_memory_write_u32(&a->memory,points+j*12+4,v->y_bits);game_memory_write_u32(&a->memory,points+j*12+8,v->z_bits);
            game_memory_write_u32(&a->memory,edges+j*12,e->x_bits);game_memory_write_u32(&a->memory,edges+j*12+4,e->y_bits);game_memory_write_u32(&a->memory,edges+j*12+8,e->z_bits);}
    }
    return !a->memory.fault;
}
static void definition_detach(AssetSdf *s,uint32_t node){
    AssetSdfNode *n=s->nodes+node;
    if(n->parent!=UINT32_MAX && s->nodes[n->parent].last_child==node)s->nodes[n->parent].last_child=n->previous_child;
    if(n->parent!=UINT32_MAX){uint32_t other;
        for(other=s->nodes[n->parent].last_child;other!=UINT32_MAX;other=s->nodes[other].previous_child)
            if(s->nodes[other].previous_child==node){s->nodes[other].previous_child=n->previous_child;break;}}
    n->parent=n->previous_child=UINT32_MAX;
}
static int definition_parts(GameApplication *a,const char *name,const float center[3],float bias,GameModelDefinition *d){
    AssetSdf parts={0};uint32_t *nodes=NULL,n=0,i,j,index,category;GameAddress holder=0;float position[3],extent[3],half=sdf_real(game_memory_read_u32(&a->memory,0x573210));
    uint32_t snapshot=child_count(a);int ok=0;
    if(!definition_sdf(a,name,"wrk",bias,1,&parts))goto done;
    if(!parts.node_count){ok=1;goto done;}
    nodes=malloc(parts.node_count*sizeof(*nodes));if(!nodes)goto done;
    sdf_center(&parts,0,center);definition_nodes(&parts,0,nodes,&n);
    if(n && !(d->parts=scene_projection_storage_allocate(n*32,"text child recipes")))goto done;
    d->part_count=n;
    /*Detach every mesh node before publication: cached native graphs retain
     *the final topology, without maintaining another mutable parser graph. */
    for(i=0;i<n;++i)definition_detach(&parts,nodes[i]);
    for(i=0;i<n;++i){uint32_t node=nodes[i],mesh_id=parts.nodes[node].mesh;AssetMesh *mesh=&parts.meshes[mesh_id].mesh;
        SceneChildRecord record={0};SceneMesh *source;GameAddress descriptor,vertices,row=d->parts+i*32;
        definition_mesh_box(mesh,half,position,extent);sdf_center(&parts,node,position);
        category=0;while(category<3 && (mesh->vertex_count>game_memory_read_u32(&a->memory,0x5a0488+category*8) ||
            mesh->triangle_count>game_memory_read_u32(&a->memory,0x5a048c+category*8)))++category;
        holder=sdf_publish(a,&parts,node,mesh_id,bias,NULL);if(!holder)goto done;
        descriptor=game_memory_read_u32(&a->memory,holder+0x50);vertices=game_memory_read_u32(&a->memory,descriptor);
        source=scene_model_bridge_lookup_source(vertices,game_memory_read_u32(&a->memory,descriptor+0x14));if(!source)goto done;
        index=child_count(a);if(index>UINT16_MAX)goto done;record.external_id=holder;record.category=category;
        if(!child_append(a,&a->memory,record))goto done;
        if(!child_bind(a,index,holder,source))goto done;holder=0;
        game_memory_write_u16(&a->memory,row,(uint16_t)index);for(j=0;j<3;++j)game_memory_write_u32(&a->memory,row+4+j*4,sdf_word(position[j]));
        game_memory_write_u16(&a->memory,row+16,0);game_memory_write_u16(&a->memory,row+18,0);game_memory_write_u16(&a->memory,row+20,0);
        game_memory_write_u32(&a->memory,row+24,0);game_memory_write_u32(&a->memory,row+28,1);
    }
    ok=!a->memory.fault;
done:
    if(holder)(void)game_application_scene_model_release(a,holder);
    if(!ok && a->model_assets){struct GameModelAssets *s=a->model_assets;
        for(i=snapshot;i<s->children.count;++i){GameAddress owned=s->children.records[i].holder_id;if(owned && owned!=holder)(void)game_application_scene_model_release(a,owned);}
        (void)child_rollback(a,&a->memory,snapshot);}
    free(nodes);asset_sdf_release(&parts);return ok;
}
static int definition_prefix(const char *name,const char *prefix){unsigned i;for(i=0;i<3;++i)if(tolower((unsigned char)name[i])!=prefix[i])return 0;return 1;}
/*449290/44ab90: the real text fallback shares all published mesh, collision,
 *point and cached-child owners with the binary path. Parser storage is transient. */
int game_model_text_definition_load(GameApplication *a,AssetModelFormat format,uint16_t slot,GameAddress names,
        int modify,int points,int parts,GameModelDefinition *out){
    AssetSdf model={0},aux={0};AssetModel collision={0};ResourceBlob blob={0};GameModelDefinition d={0};
    char name[128],path[272];GameAddress label;uint32_t node;unsigned i;float low[3]={0},high[3]={0},center[3],half,bias,secondary_y=0;int found=0,ok=0;
    if(!a || !out || (format!=ASSET_MODEL_SIMPLE && format!=ASSET_MODEL_COLLISION) || a->memory.fault)return 0;
    d.format=format;label=game_memory_read_u32(&a->memory,names+(uint32_t)slot*4);
    for(i=0;i<sizeof(name);++i){name[i]=(char)game_memory_read_u8(&a->memory,label+i);if(!name[i])break;}
    if(i==sizeof(name) || !i || a->memory.fault)return 0;
    half=sdf_real(game_memory_read_u32(&a->memory,0x573210));bias=sdf_real(game_memory_read_u32(&a->memory,0x574d28));
    if(!definition_sdf(a,name,"mdl",bias,0,&model))goto done;
    node=sdf_first(&model,0);if(node==UINT32_MAX)goto done;sdf_bounds(&model,node,low,high,&found);if(!found)goto done;
    for(i=0;i<3;++i){center[i]=(float)(((long double)low[i]+high[i])*half);d.dimensions[i]=(float)((long double)high[i]-center[i]);}
    sdf_center(&model,node,center);
    /*44f8f0 recomputes the selected descriptor after shared-node centering.
     * Its dimensions are maximum absolute coordinates, not the initial box. */
    {const AssetMesh *mesh=&model.meshes[model.nodes[node].mesh].mesh;uint32_t v;
        for(i=0;i<3;++i)d.dimensions[i]=0;
        for(v=0;v<mesh->vertex_count;++v){float xyz[3]={sdf_real(mesh->vertices[v].x_bits),sdf_real(mesh->vertices[v].y_bits),sdf_real(mesh->vertices[v].z_bits)};
            for(i=0;i<3;++i)if(fabsf(xyz[i])>d.dimensions[i])d.dimensions[i]=fabsf(xyz[i]);}}
    if(format==ASSET_MODEL_SIMPLE){
        if(!definition_sdf(a,name,"cll",bias,1,&aux) || !definition_hulls(a,&aux,center,modify,&d))goto done;
        asset_sdf_release(&aux);
    }else{
        if(snprintf(path,sizeof(path),"models2\\%scll.asc",name)<0 || !game_application_load_blob(a,path,&blob) ||
           !asset_collision_read_asc(&blob,sdf_real(game_memory_read_u32(&a->memory,0x574d2c)),bias,&collision) || !definition_planes(a,&collision,&d))goto done;
        resource_blob_release(&blob);
        secondary_y=sdf_real(collision.center.y_bits);
        if(points){
            if(!definition_sdf(a,name,"sho",bias,1,&aux) || !definition_points(a,&aux,center,&d.points,&d.point_count))goto done;
            if(!d.point_count && !definition_default_point(a,0,&d.points,&d.point_count))goto done;asset_sdf_release(&aux);
            if(!definition_sdf(a,name,"lfr",bias,1,&aux) || !definition_points(a,&aux,center,&d.secondary_points,&d.secondary_count))goto done;
            if(!d.secondary_count){GameAddress scale=0x573108;
                if(definition_prefix(name,"iib") || definition_prefix(name,"pib"))scale=0x573114;
                else if(definition_prefix(name,"itb") || definition_prefix(name,"ptb") || definition_prefix(name,"iab") || definition_prefix(name,"pab"))scale=0x573110;
                if(!definition_default_point(a,(float)((long double)secondary_y*sdf_real(game_memory_read_u32(&a->memory,scale))),&d.secondary_points,&d.secondary_count))goto done;}
            asset_sdf_release(&aux);
        }
    }
    d.model=sdf_publish(a,&model,node,model.nodes[node].mesh,bias,format==ASSET_MODEL_COLLISION?&collision.center:NULL);if(!d.model)goto done;
    if(parts && !definition_parts(a,name,center,bias,&d))goto done;
    if(a->memory.fault)goto done;*out=d;memset(&d,0,sizeof(d));ok=1;
done:game_model_definition_release(a,&d);asset_model_release(&collision);asset_sdf_release(&aux);asset_sdf_release(&model);resource_blob_release(&blob);return ok;
}
