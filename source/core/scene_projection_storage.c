#include "scene_projection_storage.h"

#include <stdlib.h>
#include <string.h>

typedef struct SceneProjectionPacketLink {
    struct SceneProjectionBlock *next_block;
    uint32_t next_index,next_ref;
} SceneProjectionPacketLink;
typedef struct SceneProjectionBlock {
    struct SceneProjectionBlock *next;
    uint8_t *data;
    uint32_t base, size, capacity;
    uint32_t generation;
    uint32_t references,shared_source;
    uint32_t model_owner_id;
    const char *purpose;
    MaterialValue *materials;
    SceneProjectionPacketLink *packet_links;
    uint8_t *material_known;
    uint32_t material_count;
    ModelHullSphere *hull;
    size_t hull_count;
    SceneMesh *mesh;
    SceneMeshProjectionPages mesh_pages;
    SceneMesh *presentation_mesh;
    uint8_t presentation_kind;
    uint8_t links_bound,links_changed;
    SceneDescriptorState descriptor;
    uint8_t descriptor_bound;
    SceneHolderState holder;
    uint8_t holder_bound;
    void *state_context;
    SceneProjectionStateRead state_read;
    SceneProjectionStateWrite state_write;
} SceneProjectionBlock;

static SceneProjectionBlock *blocks;
static SceneProjectionBlock *last_block;
/* Projection addresses increase monotonically, including when freed blocks
   are reused. Keep their permanent ranges indexed for per-packet lookups. */
static SceneProjectionBlock **blocks_by_address;
static size_t block_count,block_capacity;
static size_t free_block_count;
static uint32_t next_base = SCENE_PROJECTION_BASE;
static uint32_t next_generation;
static SceneProjectionStorageStats stats;
static void *object_write_context;
static SceneProjectionObjectWrite object_write_callback;
static int polygon_page(const char *purpose){
    return purpose && (!strcmp(purpose,"mesh polygons") ||
        !strcmp(purpose,"cloned mesh polygons"));
}
static SceneProjectionBlock *block_at_address(uint32_t address){
    size_t first=0u,last=block_count;
    while(first<last){
        size_t middle=first+(last-first)/2u;
        if(blocks_by_address[middle]->base<=address)first=middle+1u;
        else last=middle;
    }
    return first?blocks_by_address[first-1u]:NULL;
}

void scene_projection_storage_observe_objects(void *context,
    SceneProjectionObjectWrite callback){
    object_write_context=context;
    object_write_callback=callback;
}

void scene_projection_storage_stats(SceneProjectionStorageStats *output) {
    if (output) *output = stats;
}

uint32_t scene_projection_storage_allocate(size_t size, const char *purpose) {
    SceneProjectionBlock *block;
    uint32_t capacity, base;
    if (!size) return 0u;
    if (size > UINT32_MAX - 15u) return 0u;
    if(next_generation==0x7fffffffu)return 0; /* High-bit namespace is terrain. */
    capacity = ((uint32_t)size + 15u) & ~15u;
    /* Most new pages arrive while every existing page is live. Avoid a
     * fruitless full scan in that case; retain the original reuse order. */
    for (block = free_block_count?blocks:NULL; block; block = block->next) {
        if (!block->data && block->capacity >= capacity) {
            block->data = calloc(size, 1u);
            if (!block->data) return 0u;
            if(polygon_page(purpose)){
                block->packet_links=calloc(size/40u,sizeof(*block->packet_links));
                if(!block->packet_links){free(block->data);block->data=NULL;return 0u;}
            }
            block->size = (uint32_t)size;
            block->generation=++next_generation;
            block->references = 1u;
            block->shared_source = 0u;
            block->model_owner_id = 0u;
            block->holder_bound=0u;
            block->purpose = purpose;
            --free_block_count;
            ++stats.allocated; ++stats.live;
            return block->base;
        }
    }
    /* F7 is the independent recyclable-record owner, never a page allocation. */
    if (next_base >= 0xf7000000u || capacity > 0xf7000000u - next_base) return 0u;
    block = calloc(1u, sizeof(*block));
    if (!block) return 0u;
    block->data = calloc(size, 1u);
    if (!block->data) { free(block); return 0u; }
    if(polygon_page(purpose)){
        block->packet_links=calloc(size/40u,sizeof(*block->packet_links));
        if(!block->packet_links){free(block->data);free(block);return 0u;}
    }
    if(block_count==block_capacity){
        size_t capacity=block_capacity?block_capacity*2u:256u;
        SceneProjectionBlock **next=realloc(blocks_by_address,
            capacity*sizeof(*next));
        if(!next){free(block->packet_links);free(block->data);free(block);return 0u;}
        blocks_by_address=next;block_capacity=capacity;
    }
    base = next_base;
    next_base += capacity;
    block->base = base;
    block->generation=++next_generation;
    block->size = (uint32_t)size;
    block->references = 1u;
    block->capacity = capacity;
    block->purpose = purpose;
    block->next = blocks;
    blocks = block;
    blocks_by_address[block_count++]=block;
    ++stats.allocated; ++stats.live;
    return base;
}

uint32_t *scene_projection_storage_borrow_u32(uint32_t base,uint32_t count) {
    SceneProjectionBlock *block=block_at_address(base);
    if (!count || count > UINT32_MAX / sizeof(uint32_t)) return NULL;
        if (block && block->base==base && block->data &&
            !block->state_context && !block->hull &&
            block->size==(size_t)count*sizeof(uint32_t) &&
            ((uintptr_t)block->data % sizeof(uint32_t))==0u)
            return (uint32_t *)(void *)block->data;
    return NULL;
}
uint8_t *scene_projection_storage_borrow_bytes(uint32_t base,size_t size){
    SceneProjectionBlock *block=block_at_address(base);
    if(!size)return NULL;
        if(block && block->base==base && block->size==size && block->data &&
           !block->state_context && !block->hull && !block->mesh &&
           !block->presentation_mesh && !block->material_count && !block->packet_links)return block->data;
    return NULL;
}

int scene_projection_storage_bind_state(uint32_t base, void *context,
        SceneProjectionStateRead read, SceneProjectionStateWrite write) {
    SceneProjectionBlock *block = block_at_address(base);
    if (!block || block->base != base || !block->data ||
        block->state_context || block->hull || !context || !read || !write) return 0;
    block->state_context = context;
    block->state_read = read;
    block->state_write = write;
    return 1;
}

void *scene_projection_storage_state(uint32_t base,
        SceneProjectionStateRead expected_read) {
    SceneProjectionBlock *block = block_at_address(base);
    return block && block->base == base && block->data &&
        block->state_read == expected_read ? block->state_context : NULL;
}

int scene_projection_storage_describe(uint32_t address, uint32_t *base,
    uint32_t *size, const char **purpose) {
    SceneProjectionBlock *nearest = block_at_address(address);
    if (!nearest) return 0;
    if (base) *base = nearest->base;
    if (size) *size = nearest->size;
    if (purpose) *purpose = nearest->purpose;
    return 1;
}

int scene_projection_storage_free(uint32_t base) {
    SceneProjectionBlock *block=block_at_address(base);
        if (block && block->base == base && block->data) {
            uint32_t shared_source;
            if(block->references>1u){--block->references;return 1;}
            shared_source=block->shared_source;
            free(block->materials);
            free(block->material_known);
            free(block->packet_links);
            block->packet_links=NULL;
            block->materials = NULL;
            block->material_known = NULL;
            block->material_count = 0u;
            block->mesh = NULL;
            memset(&block->mesh_pages,0,sizeof(block->mesh_pages));
            block->presentation_mesh = NULL;
            block->presentation_kind = 0u;
            block->links_bound = block->links_changed = 0u;
            block->descriptor_bound = 0u;
            block->holder_bound=0u;
            block->state_context = NULL;
            block->state_read = NULL;
            block->state_write = NULL;
            free(block->hull);block->hull=NULL;block->hull_count=0u;
            free(block->data);
            block->data = NULL;
            block->size = 0u;
            block->references = block->shared_source = 0u;
            block->model_owner_id = 0u;
            ++stats.released; --stats.live;
            ++free_block_count;
            if(shared_source && !scene_projection_storage_free(shared_source))
                return 0;
            return 1;
        }
    return 0;
}

int scene_projection_storage_attach_materials(uint32_t polygon_base,
    const MaterialValue *values, uint32_t count) {
    SceneProjectionBlock *block;
    MaterialValue *copy;
    uint8_t *known;
    if (!values || !count) return 0;
    for (block = blocks; block; block = block->next)
        if (block->base == polygon_base && block->data) break;
    if (!block || block->size / 40u < count || block->materials) return 0;
    copy = malloc((size_t)count * sizeof(*copy));
    known = malloc(count);
    if (!copy || !known) { free(copy); free(known); return 0; }
    memcpy(copy, values, (size_t)count * sizeof(*copy));
    memset(known, 1, count);
    block->materials = copy;
    block->material_known = known;
    block->material_count = count;
    stats.typed_materials += count;
    return 1;
}

int scene_projection_storage_material(uint32_t polygon, MaterialValue *output) {
    SceneProjectionBlock *block;
    uint32_t offset,index;
    if (!output || polygon<SCENE_PROJECTION_BASE ||
        polygon>=SCENE_PROJECTION_END) return 0;
    block=block_at_address(polygon);
    if(!block || !block->data || !block->materials)return 0;
    offset=polygon-block->base;
    index=offset/40u;
    if(offset%40u || index>=block->material_count ||
       !block->material_known[index])return 0;
    *output=block->materials[index];
    ++stats.material_reads;
    return 1;
}

int scene_projection_storage_bind_vertices(uint32_t source,SceneMesh *mesh) {
    SceneProjectionBlock *block=block_at_address(source);
    if (!mesh) return 0;
        if (block && block->base==source && block->data &&
            block->size==(uint64_t)mesh->vertex_count*12u && !block->mesh) {
            block->mesh=mesh;
            return 1;
        }
    return 0;
}

SceneMesh *scene_projection_storage_bound_mesh(uint32_t source,uint32_t count){
    SceneProjectionBlock *block;
    if(source<SCENE_PROJECTION_BASE || source>=SCENE_PROJECTION_END)return NULL;
    block=block_at_address(source);
    if(block && block->base==source && block->data && block->mesh &&
       block->mesh->vertex_count==count)return block->mesh;
    return NULL;
}

void scene_projection_storage_unbind_vertices(uint32_t source) {
    SceneProjectionBlock *block=block_at_address(source);
    if (block && block->base==source) block->mesh=NULL;
}

static SceneProjectionBlock *projection_block(uint32_t address,size_t size){
    SceneProjectionBlock *block=last_block;uint32_t offset;
    if(address<SCENE_PROJECTION_BASE || address>=SCENE_PROJECTION_END)return NULL;
    if(block && block->data && address>=block->base){
        offset=address-block->base;
        if(offset<=block->size && size<=block->size-offset)return block;
    }
    block=block_at_address(address);
    if(!block || !block->data)return NULL;
    offset=address-block->base;
    if(offset>block->size || size>block->size-offset)return NULL;
    last_block=block;return block;
}
static uint8_t *projection_span(uint32_t address, size_t size) {
    SceneProjectionBlock *block;
    if (address < SCENE_PROJECTION_BASE || address >= SCENE_PROJECTION_END)
        return NULL;
    if (last_block && last_block->data && !last_block->hull && address >= last_block->base) {
        uint32_t offset = address - last_block->base;
        if (offset <= last_block->size && size <= last_block->size - offset)
            return last_block->data + offset;
    }
    block=block_at_address(address);
    if(block && block->data && !block->hull){
        uint32_t offset=address-block->base;
        if(offset<=block->size && size<=block->size-offset){
            last_block=block;
            return block->data+offset;
        }
    }
    return NULL;
}

const ModelHullSphere *scene_projection_storage_hull(uint32_t address,size_t count){
    SceneProjectionBlock *block;uint32_t offset;
    if(address<SCENE_PROJECTION_BASE || address>=SCENE_PROJECTION_END)return NULL;
    block=block_at_address(address);
    if(!block || !block->data || !block->hull)return NULL;
    offset=address-block->base;
    if(offset%28u || offset/28u>block->hull_count || count>block->hull_count-offset/28u)return NULL;
    return block->hull+offset/28u;
}
static int hull_transfer(SceneProjectionBlock *block,uint32_t offset,void *bytes,size_t size,int writing){
    uint8_t *cursor=bytes;size_t i;
    if(!block || !block->data || !block->hull || !bytes || offset>block->size || size>block->size-offset)return 0;
    for(i=0u;i<size;++i){
        uint32_t at=offset+(uint32_t)i,field=(at%28u)/4u,bits,shift=(at&3u)*8u;
        ModelHullSphere *sphere=&block->hull[at/28u];
        void *value=field<3u?(void *)(sphere->metadata+field):field<6u?(void *)(sphere->position+field-3u):(void *)&sphere->radius;
        memcpy(&bits,value,4u);
        if(writing){bits=(bits&~(255u<<shift))|((uint32_t)cursor[i]<<shift);memcpy(value,&bits,4u);}
        else cursor[i]=(uint8_t)(bits>>shift);
    }
    return 1;
}
int scene_projection_storage_read(uint32_t address, void *output, size_t size) {
    SceneProjectionBlock *block=projection_block(address,size);
    if(block && block->hull){int ok=hull_transfer(block,address-block->base,output,size,0);if(ok)++stats.reads;return ok;}
    uint8_t *source = projection_span(address, size);
    if (!source || !output) return 0;
    if (last_block->state_read) {
        if (!last_block->state_read(last_block->state_context,
                address - last_block->base, output, size)) return 0;
    } else memcpy(output, source, size);
    ++stats.reads;
    return 1;
}
int scene_projection_storage_bind_presentation(uint32_t polygons,uint32_t uv,
    uint32_t uv_work,SceneMesh *mesh){
    SceneProjectionBlock *block,*pages[3]={NULL,NULL,NULL};
    uint32_t bases[3]={polygons,uv,uv_work};
    const char *purposes[3]={"mesh polygons","mesh uv","mesh uv work"};
    uint64_t sizes[3];unsigned i;
    if(!mesh || (mesh->polygon_count && !mesh->presentation))return 0;
    if(!mesh->polygon_count)return 1;
    sizes[0]=(uint64_t)mesh->polygon_count*40u;
    sizes[1]=sizes[2]=(uint64_t)mesh->polygon_count*24u;
    for(i=0;i<3u;i++){
        for(block=blocks;block;block=block->next)
            if(block->base==bases[i] && block->data)break;
        if(!block || !block->purpose || strcmp(block->purpose,purposes[i]) ||
           block->size!=sizes[i] || block->presentation_mesh)return 0;
        pages[i]=block;
    }
    for(i=0;i<3u;i++){
        pages[i]->presentation_mesh=mesh;
        pages[i]->presentation_kind=(uint8_t)(i+1u);
    }
    return 1;
}
void scene_projection_storage_unbind_presentation(uint32_t polygons,
    uint32_t uv,uint32_t uv_work){
    uint32_t bases[3]={polygons,uv,uv_work};SceneProjectionBlock *block;
    unsigned i;
    for(i=0;i<3u;i++)for(block=blocks;block;block=block->next)
        if(block->base==bases[i]){
            block->presentation_mesh=NULL;
            block->presentation_kind=0u;
            break;
        }
}

int scene_projection_storage_attach_shared_source(uint32_t descriptor,
    uint32_t source) {
    SceneProjectionBlock *owner=NULL,*vertices=NULL,*block;
    for(block=blocks;block;block=block->next){
        if(block->base==descriptor && block->data)owner=block;
        if(block->base==source && block->data)vertices=block;
    }
    if(!owner || !vertices || !owner->purpose || !vertices->purpose ||
       strcmp(owner->purpose,"cloned mesh descriptor") ||
       strcmp(vertices->purpose,"mesh vertices") ||
       owner->shared_source || vertices->references==UINT32_MAX)return 0;
    owner->shared_source=source;
    ++vertices->references;
    return 1;
}

uint32_t scene_projection_storage_references(uint32_t base) {
    SceneProjectionBlock *block=block_at_address(base);
    if(block && block->base==base && block->data)return block->references;
    return 0u;
}

int scene_projection_storage_descriptor(uint32_t base,SceneDescriptorState *output){
    SceneProjectionBlock *block=block_at_address(base);
    if(!output || sizeof(SceneDescriptorState)!=0x4cu)return 0;
        if(block && block->base==base && block->data && block->size==0x4cu &&
           block->purpose &&
           (!strcmp(block->purpose,"mesh descriptor") ||
            !strcmp(block->purpose,"cloned mesh descriptor"))){
            if(!block->descriptor_bound){
                memcpy(&block->descriptor,block->data,sizeof(block->descriptor));
                block->descriptor_bound=1u;
            }
            *output=block->descriptor;
            return 1;
        }
    return 0;
}

int scene_projection_storage_bind_links(uint32_t links) {
    SceneProjectionBlock *block=block_at_address(links);
        if (block && block->base==links && block->data && block->purpose &&
            (!strcmp(block->purpose,"mesh links") ||
             !strcmp(block->purpose,"cloned mesh links"))) {
            block->links_bound=1u;
            block->links_changed=0u;
            return 1;
        }
    return 0;
}

int scene_projection_storage_links_clean(uint32_t links) {
    SceneProjectionBlock *block=block_at_address(links);
        if (block && block->base==links && block->data)
            return block->links_bound && !block->links_changed;
    return 0;
}

static int packet_slot(uint32_t address,SceneProjectionBlock **owner,
    uint32_t *index){
    SceneProjectionBlock *block;
    uint32_t offset;
    if(sizeof(RenderArenaPacket)!=40u)return 0;
    block=block_at_address(address);
    if(!block || !block->data)return 0;
    offset=address-block->base;
    if(offset%40u || offset>block->size || block->size-offset<40u ||
       !block->packet_links)return 0;
    *owner=block;*index=offset/40u;
    return 1;
}

float *scene_projection_storage_positions(uint32_t address,size_t count) {
    if(count>SIZE_MAX/(3u*sizeof(float)))return NULL;
    return (float *)projection_span(address,count*3u*sizeof(float));
}
static void refresh_holder_state(SceneProjectionBlock *block){
    const uint8_t *page=block->data;
    SceneHolderState *holder=&block->holder;
    memcpy(holder->local,page+4u,sizeof(holder->local));
    memcpy(&holder->texture_state,page+0x38u,4u);
    holder->flags=page[0x3cu];
    memcpy(&holder->parent,page+0x40u,4u);
    memcpy(&holder->previous_child,page+0x44u,4u);
    memcpy(&holder->sibling_back,page+0x48u,4u);
    memcpy(&holder->last_child,page+0x4cu,4u);
    memcpy(&holder->descriptor,page+0x50u,4u);
    memcpy(&holder->extension,page+0x58u,4u);
    block->holder_bound=1u;
}
int scene_projection_storage_holder(uint32_t base,SceneHolderState *output){
    SceneProjectionBlock *block=block_at_address(base);
    if(!output)return 0;
        if(block && block->base==base && block->data && block->size==0x5cu &&
           block->purpose && !strcmp(block->purpose,"mesh holder")){
            if(!block->holder_bound)refresh_holder_state(block);
            *output=block->holder;
            ++stats.typed_holder_views;
            return 1;
        }
    return 0;
}
static RenderArenaPacket *packet_at(SceneProjectionBlock *block,uint32_t index){
    return (RenderArenaPacket *)(void *)(block->data+index*40u);
}
static void refresh_packet_link(SceneProjectionBlock *block,uint32_t index){
    SceneProjectionPacketLink *link=&block->packet_links[index];
    SceneProjectionBlock *next_block=NULL;
    uint32_t next_index=0u;
    link->next_ref=packet_at(block,index)->next;
    if(packet_slot(link->next_ref,&next_block,&next_index)){
        link->next_block=next_block;link->next_index=next_index;
    }else {link->next_block=NULL;link->next_index=0u;}
}
RenderArenaPacket *scene_projection_storage_packet(uint32_t address) {
    SceneProjectionBlock *block=NULL;uint32_t index=0u;
    if(!packet_slot(address,&block,&index))return NULL;
    ++stats.packet_views;
    return packet_at(block,index);
}
int scene_projection_storage_packet_set_next(uint32_t address,uint32_t next_ref){
    SceneProjectionBlock *block=NULL;uint32_t index=0u;
    if(!packet_slot(address,&block,&index))return 0;
    packet_at(block,index)->next=next_ref;
    refresh_packet_link(block,index);
    return 1;
}
int scene_projection_storage_packet_link(uint32_t address,uint32_t *next_ref,
    RenderArenaPacket **next_owned){
    SceneProjectionBlock *block=NULL;uint32_t index=0u;
    SceneProjectionPacketLink *link;
    if(!next_ref || !next_owned || !packet_slot(address,&block,&index))return 0;
    link=&block->packet_links[index];
    *next_ref=link->next_ref;
    *next_owned=link->next_block && link->next_block->data?
        packet_at(link->next_block,link->next_index):NULL;
    if(*next_owned)++stats.native_packet_links;
    return 1;
}

int scene_projection_storage_render_binding(uint32_t source,uint32_t count,
    uint32_t transformed,uint32_t polygons,
    SceneProjectionRenderBinding *output){
    SceneProjectionBlock *block;
    const SceneMeshProjectionPages *view;
    uint32_t base,size,links=0;
    const char *purpose=NULL;
    RenderArenaPacket *first;
    if(!output || !source)return 0;
    block=block_at_address(source);
    if(!block || block->base!=source || !block->data || !block->mesh ||
       block->mesh->vertex_count!=count)return 0;
    view=&block->mesh_pages;
    if(view->source!=source)return 0;
    if(view->transformed==transformed && view->polygons==polygons){
        output->mesh=block->mesh;output->transformed=transformed;
        output->polygons=polygons;output->links=view->links;
        output->uv_work=view->uv_work;output->source_presentation=1;
        return 1;
    }
    if(!scene_projection_storage_describe(transformed,&base,&size,&purpose) ||
       base!=transformed || size!=(uint64_t)count*16u ||
       !purpose || strcmp(purpose,"cloned mesh transformed"))return 0;
    if(block->mesh->polygon_count){
        if(!scene_projection_storage_describe(polygons,&base,&size,&purpose) ||
           base!=polygons ||
           size!=(uint64_t)block->mesh->polygon_count*40u ||
           !purpose || strcmp(purpose,"cloned mesh polygons"))return 0;
        first=scene_projection_storage_packet(polygons);
        if(!first)return 0;
        links=first->vertices;
        if(!scene_projection_storage_describe(links,&base,&size,&purpose) ||
           base!=links ||
           size!=(uint64_t)block->mesh->polygon_count*12u ||
           !purpose || strcmp(purpose,"cloned mesh links"))return 0;
    }else if(polygons)return 0;
    output->mesh=block->mesh;output->transformed=transformed;
    output->polygons=polygons;output->links=links;
    output->uv_work=0;output->source_presentation=0;
    return 1;
}

int scene_projection_storage_set_packet_flags(uint32_t address,uint32_t flags){
    SceneProjectionBlock *block=block_at_address(address);
    if(block){
        uint32_t offset,index;
        RenderArenaPacket *packet;
        if(!block->data || address<block->base || !block->purpose ||
           (strcmp(block->purpose,"mesh polygons") &&
            strcmp(block->purpose,"cloned mesh polygons")))return 0;
        offset=address-block->base;
        if(offset%40u || offset>block->size || block->size-offset<40u)return 0;
        index=offset/40u;
        packet=(RenderArenaPacket *)(void *)(block->data+offset);
        packet->flags=flags;
        if(block->presentation_mesh && block->presentation_kind==1u &&
           index<block->presentation_mesh->polygon_count)
            block->presentation_mesh->presentation[index].flags=flags;
        if(object_write_callback)
            object_write_callback(object_write_context,block->base,
                offset+8u,4u,SCENE_PROJECTION_POLYGON_WRITE);
        return 1;
    }
    return 0;
}

RenderArenaVertex *scene_projection_storage_transformed(uint32_t address,
    size_t count){
    SceneProjectionBlock *block=block_at_address(address);
    uint32_t offset;
    if(!block || !block->data || !block->purpose ||
       (strcmp(block->purpose,"mesh transformed") &&
        strcmp(block->purpose,"cloned mesh transformed")))return NULL;
    offset=address-block->base;
    if(offset%16u || offset>block->size ||
       count>(block->size-offset)/16u)return NULL;
    return (RenderArenaVertex *)(void *)(block->data+offset);
}

uint64_t scene_projection_storage_vertex_key(uint32_t address){
    SceneProjectionBlock *block;
    if(!scene_projection_storage_transformed(address,1))return 0;
    block=block_at_address(address);
    return ((uint64_t)block->generation<<32)|(address-block->base);
}
const uint32_t *scene_projection_storage_links(uint32_t address,size_t count){
    SceneProjectionBlock *block=block_at_address(address);
    uint32_t offset;
    if(!block || !block->data || !block->purpose ||
       (strcmp(block->purpose,"mesh links") &&
        strcmp(block->purpose,"cloned mesh links")))return NULL;
    offset=address-block->base;
    if(offset%4u || offset>block->size ||
       count>(block->size-offset)/4u)return NULL;
    return (const uint32_t *)(const void *)(block->data+offset);
}

const RenderArenaUv *scene_projection_storage_work_uv(uint32_t address,
    size_t count){
    SceneProjectionBlock *block=block_at_address(address);
    uint32_t offset;
    if(!block || !block->data || !block->purpose ||
       (strcmp(block->purpose,"mesh uv work") &&
        strcmp(block->purpose,"cloned mesh uv work")))return NULL;
    offset=address-block->base;
    if(offset%8u || offset>block->size ||
       count>(block->size-offset)/8u)return NULL;
    return (const RenderArenaUv *)(const void *)(block->data+offset);
}

int scene_projection_storage_set_work_uv(uint32_t address,
    const RenderArenaUv *value){
    SceneProjectionBlock *block=block_at_address(address);
    uint32_t offset,index;
    if(!value)return 0;
    if(!block || !block->data || !block->purpose ||
       (strcmp(block->purpose,"mesh uv work") &&
        strcmp(block->purpose,"cloned mesh uv work")))return 0;
    offset=address-block->base;
    if(offset%8u || offset>block->size || block->size-offset<8u)return 0;
    index=offset/8u;
    memcpy(block->data+offset,value,sizeof(*value));
    if(block->presentation_mesh && block->presentation_kind==3u &&
       index/3u<block->presentation_mesh->polygon_count)
        memcpy(&block->presentation_mesh->presentation[index/3u].uv[index%3u],
            value,sizeof(*value));
    return 1;
}

/* Prepare the retained instance's packet links and UV work directly in its
 * owned pages. The x86 address fields remain projections for external users. */
int scene_projection_storage_prepare_instance(const SceneInstance *instance){
    const SceneRenderDescriptor *render;
    SceneProjectionBlock *polygons,*work;
    uint32_t i;
    if(!instance || !instance->has_render || !instance->presentation)return 0;
    render=&instance->render;
    if(!render->polygon_count ||
       render->polygon_count!=instance->presentation_count ||
       render->polygons!=instance->presentation_polygon_base ||
       !scene_projection_storage_links_clean(render->links))return 0;
    polygons=block_at_address(render->polygons);
    work=block_at_address(render->uv_work);
    if(!polygons || polygons->base!=render->polygons || !polygons->data ||
       !polygons->purpose ||
       (strcmp(polygons->purpose,"mesh polygons") && strcmp(polygons->purpose,"cloned mesh polygons")) ||
       polygons->size!=(uint64_t)render->polygon_count*40u ||
       !work || work->base!=render->uv_work || !work->data || !work->purpose ||
       (strcmp(work->purpose,"mesh uv work") && strcmp(work->purpose,"cloned mesh uv work")) ||
       work->size!=(uint64_t)render->polygon_count*24u ||
       (work->presentation_mesh &&
        (work->presentation_kind!=3u ||
         work->presentation_mesh->polygon_count!=render->polygon_count)))return 0;
    for(i=0;i<render->polygon_count;i++){
        RenderArenaPacket *packet=(RenderArenaPacket *)(void *)(polygons->data+i*40u);
        const ScenePolygonPresentation *presentation=&instance->presentation[i];
        packet->count=3u;
        packet->vertices=render->links+i*12u;
        packet->attributes=render->uv_work+i*24u;
        packet->material=presentation->material_handle;
        memcpy(work->data+i*24u,presentation->uv,24u);
        if(work->presentation_mesh)
            memcpy(work->presentation_mesh->presentation[i].uv,
                presentation->uv,24u);
    }
    return 1;
}

int scene_projection_storage_read_uv(uint32_t address,AssetMeshUV *output) {
    /* Effect models own live UVs in C state. Their bounded address views do
     * not synchronize those values into backing bytes. Unclipped packets keep
     * these references; clipped packets instead use the frame UV pool. */
    if(!output || !scene_projection_storage_read(address,output,sizeof(*output)))return 0;
    ++stats.uv_reads;
    return 1;
}

int scene_projection_storage_write(uint32_t address, const void *input,
    size_t size) {
    SceneProjectionBlock *hull_block=projection_block(address,size);
    if(hull_block && hull_block->hull){int ok=hull_transfer(hull_block,address-hull_block->base,(void *)input,size,1);if(ok)++stats.writes;return ok;}
    uint8_t *destination = projection_span(address, size);
    if (!destination || !input) return 0;
    if (last_block->state_write && !last_block->state_write(
            last_block->state_context, address - last_block->base,
            input, size)) return 0;
    memcpy(destination, input, size);
    if (size) {
        SceneProjectionBlock *block=block_at_address(address);
        if (!block || !block->data) return 0;
        if(block->packet_links){
            uint32_t offset,first,last,index;
            offset=address-block->base;
            first=offset/40u;last=(uint32_t)((offset+size-1u)/40u);
            for(index=first;index<=last && index<block->size/40u;index++){
                uint32_t field=index*40u+12u;
                if(offset<field+4u && offset+size>field)
                    refresh_packet_link(block,index);
            }
        }
        if(block->descriptor_bound)
            memcpy(&block->descriptor,block->data,sizeof(block->descriptor));
        if(block->holder_bound && block->purpose &&
           !strcmp(block->purpose,"mesh holder"))
            refresh_holder_state(block);
        if(object_write_callback && block->purpose){
                if(!strcmp(block->purpose,"mesh holder"))
                    object_write_callback(object_write_context,block->base,
                        address-block->base,size,SCENE_PROJECTION_HOLDER_WRITE);
                else if(!strcmp(block->purpose,"cloned mesh descriptor") ||
                        !strcmp(block->purpose,"mesh descriptor"))
                    object_write_callback(object_write_context,block->base,
                        address-block->base,size,SCENE_PROJECTION_DESCRIPTOR_WRITE);
                else if(!strcmp(block->purpose,"cloned mesh polygons") ||
                        !strcmp(block->purpose,"mesh polygons"))
                    object_write_callback(object_write_context,block->base,
                        address-block->base,size,SCENE_PROJECTION_POLYGON_WRITE);
                else if(!strcmp(block->purpose,"cloned mesh uv") ||
                        !strcmp(block->purpose,"mesh uv"))
                    object_write_callback(object_write_context,block->base,
                        address-block->base,size,SCENE_PROJECTION_UV_WRITE);
        }
        if (block->links_bound) block->links_changed=1u;
        if (block->materials) {
            uint32_t offset,first,last,index;
            offset = address - block->base;
            first = offset / 40u;
            last = (uint32_t)((offset + size - 1u) / 40u);
            for (index = first; index <= last && index < block->material_count; ++index) {
                uint32_t field = index * 40u + 20u;
                if (offset < field + 4u && offset + size > field)
                    block->material_known[index] = 0u;
            }
        }
        if (block->mesh) {
            uint32_t offset,first,last,index;
            offset=address-block->base;
            first=offset/12u;
            last=(uint32_t)((offset+size-1u)/12u);
            for(index=first;index<=last && index<block->mesh->vertex_count;index++) {
                AssetMeshVertex value;
                memcpy(&value,block->data+index*12u,sizeof(value));
                if(memcmp(&value,&block->mesh->vertices[index],sizeof(value))) {
                    if(!scene_mesh_write_vertex(block->mesh,index,&value))return 0;
                    ++stats.vertex_updates;
                }
            }
        }
        if(block->presentation_mesh){
            uint32_t offset,first,last,index;
            SceneMesh *mesh=block->presentation_mesh;
            offset=address-block->base;
            if(block->presentation_kind==1u){
                first=offset/40u;last=(uint32_t)((offset+size-1u)/40u);
                for(index=first;index<=last && index<mesh->polygon_count;index++){
                    uint32_t field=index*40u+8u;
                    if(offset<field+4u && offset+size>field)
                        memcpy(&mesh->presentation[index].flags,
                            block->data+field,4u);
                    field=index*40u+20u;
                    if(offset<field+4u && offset+size>field)
                        memcpy(&mesh->presentation[index].material_handle,
                            block->data+field,4u);
                }
            }else{
                first=offset/8u;last=(uint32_t)((offset+size-1u)/8u);
                for(index=first;index<=last && index/3u<mesh->polygon_count;
                    index++){
                    AssetMeshUV value;
                    memcpy(&value,block->data+index*8u,sizeof(value));
                    if(block->presentation_kind==2u){
                        if(!scene_mesh_write_polygon_uv(mesh,index/3u,index%3u,
                            &value))return 0;
                    }else if(block->presentation_kind==3u)
                        mesh->presentation[index/3u].uv[index%3u]=value;
                }
            }
        }
    }
    ++stats.writes;
    return 1;
}

int scene_projection_storage_build_mesh(SceneMesh *scene,
    const uint32_t *material_handles,const MaterialValue *materials,
    const uint8_t *material_flags,uint32_t material_count,
    SceneMeshProjectionPages *pages){
    SceneMeshProjectionPages next={0};MaterialValue *polygon_materials=NULL;
    AssetMeshVertex *vertices;AssetMeshUV *uv;
    RenderArenaPacket *packets;uint32_t *links;
    uint32_t i,j;
    if(!scene || !pages || !scene->asset ||
       (scene->vertex_count && !scene->vertices) ||
       (scene->polygon_count && (!scene->polygons || !scene->presentation)) ||
       (material_count && (!material_handles || !materials || !material_flags)) ||
       (uint64_t)scene->vertex_count*16u>SIZE_MAX ||
       (uint64_t)scene->polygon_count*40u>SIZE_MAX)return 0;
    next.source=scene_projection_storage_allocate((size_t)scene->vertex_count*12u,
        "mesh vertices");
    next.transformed=scene_projection_storage_allocate((size_t)scene->vertex_count*16u,
        "mesh transformed");
    next.uv=scene_projection_storage_allocate((size_t)scene->polygon_count*24u,
        "mesh uv");
    next.uv_work=scene_projection_storage_allocate((size_t)scene->polygon_count*24u,
        "mesh uv work");
    next.links=scene_projection_storage_allocate((size_t)scene->polygon_count*12u,
        "mesh links");
    next.polygons=scene_projection_storage_allocate((size_t)scene->polygon_count*40u,
        "mesh polygons");
    if((scene->vertex_count && (!next.source || !next.transformed)) ||
       (scene->polygon_count && (!next.uv || !next.uv_work ||
        !next.links || !next.polygons)))goto fail;
    vertices=scene->vertex_count?(AssetMeshVertex *)(void *)projection_span(next.source,
        (size_t)scene->vertex_count*12u):NULL;
    uv=scene->polygon_count?(AssetMeshUV *)(void *)projection_span(next.uv,
        (size_t)scene->polygon_count*24u):NULL;
    links=scene->polygon_count?(uint32_t *)(void *)projection_span(next.links,
        (size_t)scene->polygon_count*12u):NULL;
    packets=scene->polygon_count?(RenderArenaPacket *)(void *)projection_span(next.polygons,
        (size_t)scene->polygon_count*40u):NULL;
    if((scene->vertex_count && !vertices) ||
       (scene->polygon_count && (!uv || !links || !packets)))goto fail;
    if(scene->polygon_count){
        polygon_materials=malloc((size_t)scene->polygon_count*sizeof(*polygon_materials));
        if(!polygon_materials)goto fail;
    }
    if(scene->vertex_count)memcpy(vertices,scene->vertices,
        (size_t)scene->vertex_count*sizeof(*vertices));
    for(i=0;i<scene->polygon_count;i++){
        const ScenePolygon *polygon=&scene->polygons[i];
        uint32_t material=polygon->material;
        if(material>=material_count)goto fail;
        polygon_materials[i]=materials[material];
        for(j=0;j<3u;j++){
            uint32_t corner=i*3u+j;
            if(polygon->vertex[j]>=scene->vertex_count)goto fail;
            links[corner]=next.transformed+polygon->vertex[j]*16u;
            uv[corner]=polygon->uv[j];
        }
        packets[i].count=3u;
        packets[i].vertices=next.links+i*12u;
        packets[i].flags=material_flags[material]?0x101u:1u;
        packets[i].next=0u;
        packets[i].attributes=next.uv_work+i*24u;
        packets[i].material=material_handles[material];
        scene->presentation[i].flags=packets[i].flags;
        scene->presentation[i].material_handle=packets[i].material;
    }
    if(scene->polygon_count &&
       (!scene_projection_storage_attach_materials(next.polygons,
        polygon_materials,scene->polygon_count) ||
        !scene_projection_storage_bind_links(next.links)))goto fail;
    free(polygon_materials);*pages=next;return 1;
fail:
    free(polygon_materials);
    if(next.polygons)scene_projection_storage_free(next.polygons);
    if(next.links)scene_projection_storage_free(next.links);
    if(next.uv_work)scene_projection_storage_free(next.uv_work);
    if(next.uv)scene_projection_storage_free(next.uv);
    if(next.transformed)scene_projection_storage_free(next.transformed);
    if(next.source)scene_projection_storage_free(next.source);
    return 0;
}

uint32_t scene_projection_storage_create_holder(void){
    uint32_t base=scene_projection_storage_allocate(0x5cu,"mesh holder");
    uint32_t *words;
    if(!base)return 0;
    words=(uint32_t *)(void *)projection_span(base,0x5cu);
    if(!words){scene_projection_storage_free(base);return 0;}
    words[0]=0x00573220u;
    words[1]=words[6]=words[11]=0x3f800000u;
    return base;
}

int scene_projection_storage_project_holder(const SceneInstance *instance){
    SceneProjectionBlock *block;
    uint8_t *page=NULL;
    if(!instance || !instance->holder)return 0;
    block=block_at_address(instance->holder);
        if(block && block->base==instance->holder && block->data &&
           block->size==0x5cu && block->purpose &&
           !strcmp(block->purpose,"mesh holder")){
            page=block->data;
        }
    if(!page)return 0;
    memcpy(page+4u,instance->local,sizeof(instance->local));
    memcpy(page+0x38u,&instance->texture_state,4u);
    page[0x3cu]=(uint8_t)instance->flags;
    memcpy(page+0x40u,&instance->parent,4u);
    memcpy(page+0x44u,&instance->previous_child,4u);
    memcpy(page+0x48u,&instance->sibling_back,4u);
    memcpy(page+0x4cu,&instance->last_child,4u);
    memcpy(page+0x50u,&instance->descriptor,4u);
    memcpy(page+0x58u,&instance->extension,4u);
    memcpy(block->holder.local,instance->local,sizeof(block->holder.local));
    block->holder.texture_state=instance->texture_state;
    block->holder.flags=(uint8_t)instance->flags;
    block->holder.parent=instance->parent;
    block->holder.previous_child=instance->previous_child;
    block->holder.sibling_back=instance->sibling_back;
    block->holder.last_child=instance->last_child;
    block->holder.descriptor=instance->descriptor;
    block->holder.extension=instance->extension;
    block->holder_bound=1u;
    ++stats.holder_projects;
    return 1;
}
const AssetMeshUV *scene_projection_storage_uv(uint32_t address,size_t count){
    SceneProjectionBlock *block=block_at_address(address);
    if(block){
        uint32_t offset;
        if(!block->data || address<block->base || !block->purpose ||
           (strcmp(block->purpose,"mesh uv") &&
            strcmp(block->purpose,"cloned mesh uv")))return NULL;
        offset=address-block->base;
        if(offset%sizeof(AssetMeshUV) || offset>block->size ||
           count>(block->size-offset)/sizeof(AssetMeshUV))return NULL;
        return (const AssetMeshUV *)(const void *)(block->data+offset);
    }
    return NULL;
}

uint32_t scene_projection_storage_create_descriptor(const char *label,size_t size){
    uint32_t descriptor,name;SceneDescriptorState record={0};
    uint8_t *name_bytes,*descriptor_bytes;
    if(!label || !size || size>256u || label[size-1u])return 0;
    descriptor=scene_projection_storage_allocate(0x4cu,"mesh descriptor");
    if(!descriptor)return 0;
    name=scene_projection_storage_allocate(size,"mesh descriptor name");
    if(!name){scene_projection_storage_free(descriptor);return 0;}
    name_bytes=projection_span(name,size);
    descriptor_bytes=projection_span(descriptor,sizeof(record));
    if(!name_bytes || !descriptor_bytes){
        scene_projection_storage_free(name);
        scene_projection_storage_free(descriptor);
        return 0;
    }
    memcpy(name_bytes,label,size);
    record.name=name;record.state=1u;
    memcpy(descriptor_bytes,&record,sizeof(record));
    return descriptor;
}

int scene_projection_storage_set_holder_descriptor(uint32_t holder,
    uint32_t descriptor){
    SceneProjectionBlock *block;
    if(!scene_projection_storage_descriptor(descriptor,
        &(SceneDescriptorState){0}))return 0;
    for(block=blocks;block;block=block->next)
        if(block->base==holder && block->data && block->size==0x5cu &&
           block->purpose && !strcmp(block->purpose,"mesh holder"))
            return scene_projection_storage_write(holder+0x50u,
                &descriptor,sizeof(descriptor));
    return 0;
}

uint32_t scene_projection_storage_create_mesh(SceneMesh *scene,
    const char *label,size_t label_size,const uint32_t *material_handles,
    const MaterialValue *materials,const uint8_t *material_flags,
    uint32_t material_count,SceneMeshProjectionPages *pages){
    SceneMeshProjectionPages built={0};SceneDescriptorState record={0};
    const AssetMesh *mesh;
    uint32_t holder=0,descriptor=0;
    SceneProjectionBlock *block;
    if(!scene || !scene->asset || !pages)return 0;
    mesh=scene->asset;
    holder=scene_projection_storage_create_holder();
    descriptor=scene_projection_storage_create_descriptor(label,label_size);
    if(!holder || !descriptor ||
       !scene_projection_storage_set_holder_descriptor(holder,descriptor) ||
       !scene_projection_storage_build_mesh(scene,material_handles,materials,
           material_flags,material_count,&built) ||
       !scene_projection_storage_descriptor(descriptor,&record))goto fail;
    record.source_vertices=built.source;
    record.transformed_vertices=built.transformed;
    record.uv=built.uv;record.uv_work=built.uv_work;
    record.links=built.links;record.vertex_count=scene->vertex_count;
    record.polygons=built.polygons;record.polygon_count=scene->polygon_count;
    record.radius_bits=mesh->radius_bits;
    record.center[0]=mesh->center.x_bits;
    record.center[1]=mesh->center.y_bits;
    record.center[2]=mesh->center.z_bits;
    record.auxiliary[0]=mesh->auxiliary_bits;
    if(!scene_projection_storage_write(descriptor,&record,sizeof(record)))goto fail;
    built.descriptor=descriptor;
    for(block=blocks;block;block=block->next)
        if(block->base==built.source && block->data){
            block->mesh_pages=built;break;
        }
    if(built.source && !block)goto fail;
    *pages=built;
    return holder;
fail:
    if(built.polygons)scene_projection_storage_free(built.polygons);
    if(built.links)scene_projection_storage_free(built.links);
    if(built.uv_work)scene_projection_storage_free(built.uv_work);
    if(built.uv)scene_projection_storage_free(built.uv);
    if(built.transformed)scene_projection_storage_free(built.transformed);
    if(built.source)scene_projection_storage_free(built.source);
    if(descriptor){
        if(scene_projection_storage_descriptor(descriptor,&record) && record.name)
            scene_projection_storage_free(record.name);
        scene_projection_storage_free(descriptor);
    }
    if(holder)scene_projection_storage_free(holder);
    return 0;
}

static void model_u16(uint8_t *page,size_t offset,uint16_t value){
    page[offset]=(uint8_t)value;
    page[offset+1u]=(uint8_t)(value>>8);
}
static void model_u32(uint8_t *page,size_t offset,uint32_t value){
    page[offset]=(uint8_t)value;
    page[offset+1u]=(uint8_t)(value>>8);
    page[offset+2u]=(uint8_t)(value>>16);
    page[offset+3u]=(uint8_t)(value>>24);
}
static void model_vector(uint8_t *page,size_t offset,const AssetMeshVertex *v){
    model_u32(page,offset,v->x_bits);
    model_u32(page,offset+4u,v->y_bits);
    model_u32(page,offset+8u,v->z_bits);
}
static uint32_t model_vectors(const AssetMeshVertex *values,uint32_t count,
    uint32_t minimum,const char *purpose){
    uint32_t backing=count>minimum?count:minimum,base,i;
    uint8_t *page;
    if(!backing)return 0;
    if((uint64_t)backing*12u>SIZE_MAX)return 0;
    base=scene_projection_storage_allocate((size_t)backing*12u,purpose);
    if(!base)return 0;
    page=projection_span(base,(size_t)backing*12u);
    if(!page){scene_projection_storage_free(base);return 0;}
    for(i=0;i<count;i++)model_vector(page,(size_t)i*12u,&values[i]);
    return base;
}
void scene_projection_storage_discard_model_pages(SceneModelProjectionPages *pages){
    uint32_t i;
    if(!pages)return;
    for(i=0;i<pages->plane_count;i++){
        if(pages->plane_vertices && pages->plane_vertices[i])
            scene_projection_storage_free(pages->plane_vertices[i]);
        if(pages->plane_secondary && pages->plane_secondary[i])
            scene_projection_storage_free(pages->plane_secondary[i]);
    }
    if(pages->parts)scene_projection_storage_free(pages->parts);
    if(pages->secondary_points)scene_projection_storage_free(pages->secondary_points);
    if(pages->planes)scene_projection_storage_free(pages->planes);
    if(pages->points)scene_projection_storage_free(pages->points);
    if(pages->markers)scene_projection_storage_free(pages->markers);
    free(pages->plane_vertices);free(pages->plane_secondary);
    memset(pages,0,sizeof(*pages));
}
int scene_projection_storage_build_model_pages(const AssetModel *model,
    int include_points,int include_parts,uint32_t first_child,
    SceneModelProjectionPages *pages){
    SceneModelProjectionPages next={0};uint8_t *page;uint32_t i;
    if(!model || !pages ||
       (model->marker_count && !model->markers) ||
       (model->point_count && !model->points) ||
       (model->part_count && !model->parts) ||
       (model->plane_count && !model->planes) ||
       (model->secondary_point_count && !model->secondary_points))return 0;
    if(model->format!=ASSET_MODEL_COLLISION && model->marker_count){
        if((uint64_t)model->marker_count*28u>SIZE_MAX)goto fail;
        next.markers=scene_projection_storage_allocate(
            (size_t)model->marker_count*28u,"markers");
        if(!next.markers)goto fail;
        {
            SceneProjectionBlock *hull_block=block_at_address(next.markers);
            ModelHullSphere *hull=calloc(model->marker_count,sizeof(*hull));
            uint8_t *live=calloc(1u,1u);
            if(!hull || !live){free(hull);free(live);goto fail;}
            for(i=0u;i<model->marker_count;++i){
                memcpy(hull[i].metadata,model->markers[i].words,12u);
                memcpy(hull[i].position,model->markers[i].words+3u,12u);
                memcpy(&hull[i].radius,model->markers[i].words+6u,4u);
            }
            /* Keep the opaque range identifier, not its old byte-layout buffer. */
            free(hull_block->data);hull_block->data=live;
            hull_block->hull=hull;hull_block->hull_count=model->marker_count;
        }
    }
    if(include_points && model->format!=ASSET_MODEL_EIGHT_MESH){
        next.points=model_vectors(model->points,model->point_count,2u,
            "model points");
        if(!next.points)goto fail;
    }
    if(model->format==ASSET_MODEL_COLLISION){
        next.plane_count=model->plane_count;
        if(model->plane_count){
            if((uint64_t)model->plane_count*48u>SIZE_MAX)goto fail;
            next.plane_vertices=calloc(model->plane_count,sizeof(uint32_t));
            next.plane_secondary=calloc(model->plane_count,sizeof(uint32_t));
            next.planes=scene_projection_storage_allocate(
                (size_t)model->plane_count*48u,"collision planes");
            if(!next.plane_vertices || !next.plane_secondary || !next.planes)
                goto fail;
            page=projection_span(next.planes,(size_t)model->plane_count*48u);
            if(!page)goto fail;
            for(i=0;i<model->plane_count;i++){
                const AssetCollisionPlane *plane=&model->planes[i];
                size_t offset=(size_t)i*48u;
                if(plane->vertex_count &&
                   (!plane->vertices || !plane->secondary_vectors))goto fail;
                next.plane_vertices[i]=model_vectors(plane->vertices,
                    plane->vertex_count,0,"collision vertices");
                next.plane_secondary[i]=model_vectors(plane->secondary_vectors,
                    plane->vertex_count,0,"collision secondary");
                if(plane->vertex_count && (!next.plane_vertices[i] ||
                   !next.plane_secondary[i]))goto fail;
                model_u16(page,offset,(uint16_t)plane->vertex_count);
                model_vector(page,offset+4u,&plane->normal);
                model_u32(page,offset+16u,next.plane_vertices[i]);
                model_u32(page,offset+20u,next.plane_secondary[i]);
            }
        }
        next.secondary_points=model_vectors(model->secondary_points,
            model->secondary_point_count,0,"secondary points");
        if(model->secondary_point_count && !next.secondary_points)goto fail;
    }
    if(include_parts && model->part_count){
        if((uint64_t)model->part_count*32u>SIZE_MAX)goto fail;
        next.parts=scene_projection_storage_allocate(
            (size_t)model->part_count*32u,"model parts");
        if(!next.parts)goto fail;
        page=projection_span(next.parts,(size_t)model->part_count*32u);
        if(!page)goto fail;
        for(i=0;i<model->part_count;i++){
            size_t offset=(size_t)i*32u;
            model_u16(page,offset,(uint16_t)(first_child+i));
            model_vector(page,offset+4u,&model->parts[i].position);
            model_u32(page,offset+28u,1u);
        }
    }
    *pages=next;return 1;
fail:
    scene_projection_storage_discard_model_pages(&next);
    return 0;
}
static SceneProjectionBlock *model_page(uint32_t base){
    SceneProjectionBlock *block=block_at_address(base);
    if(!base)return NULL;
    if(block && block->base==base && block->data)return block;
    return NULL;
}
static uint32_t model_read_u32(const uint8_t *page,size_t offset){
    return (uint32_t)page[offset] | ((uint32_t)page[offset+1u]<<8) |
        ((uint32_t)page[offset+2u]<<16) | ((uint32_t)page[offset+3u]<<24);
}
static int clone_descriptor(uint32_t source_address,
    const SceneInstance *owned,const SceneMesh *mesh,uint32_t *result){
    SceneDescriptorState source,clone={0};SceneProjectionBlock *name_block;
    MaterialValue *materials=NULL;
    uint8_t *source_packets,*source_links,*source_uv,*destination_packets;
    uint8_t *destination_links,*destination_uv,*name_bytes;
    uint32_t output=0,i,j,typed_materials=0;size_t name_size=0;
    if(!result || !scene_projection_storage_descriptor(source_address,&source))
        return 0;
    *result=0;
    if(source.vertex_count>0x100000u || source.polygon_count>0x100000u)
        return -1;
    if(owned){
        if(!mesh || !owned->has_render ||
           owned->descriptor!=source_address ||
           owned->render.source_vertices!=source.source_vertices ||
           owned->render.transformed_vertices!=source.transformed_vertices ||
           owned->render.links!=source.links ||
           owned->render.polygons!=source.polygons ||
           owned->render.uv!=source.uv ||
           owned->presentation_count!=source.polygon_count ||
           (source.polygon_count && (!owned->presentation || !mesh->polygons)) ||
           mesh->vertex_count!=source.vertex_count ||
           mesh->polygon_count!=source.polygon_count ||
           !scene_projection_storage_links_clean(source.links))return 0;
        for(i=0;i<source.polygon_count;i++)
            for(j=0;j<3u;j++)
                if(mesh->polygons[i].vertex[j]>=source.vertex_count)return 0;
    }
    name_block=model_page(source.name);
    if(!name_block || !name_block->size)return 0;
    while(name_size<name_block->size && name_size<512u &&
        name_block->data[name_size])name_size++;
    if(name_size==name_block->size || name_size==512u)return -1;
    name_size++;
    source_packets=!owned && source.polygon_count?projection_span(source.polygons,
        (size_t)source.polygon_count*40u):NULL;
    source_links=!owned && source.polygon_count?projection_span(source.links,
        (size_t)source.polygon_count*12u):NULL;
    source_uv=!owned && source.polygon_count?projection_span(source.uv,
        (size_t)source.polygon_count*24u):NULL;
    if((!owned && source.polygon_count &&
        (!source_packets || !source_links || !source_uv)) ||
       (source.optional_shorts && !projection_span(source.optional_shorts,
        (size_t)source.polygon_count*2u)) ||
       (source.optional_bytes && !projection_span(source.optional_bytes,
        source.polygon_count)) ||
       (source.source_vertices &&
        !model_page(source.source_vertices)))return 0;
    output=scene_projection_storage_allocate(sizeof(clone),
        "cloned mesh descriptor");
    clone.name=scene_projection_storage_allocate(name_size,
        "cloned descriptor name");
    clone.transformed_vertices=scene_projection_storage_allocate(
        (size_t)source.vertex_count*16u,"cloned mesh transformed");
    clone.links=scene_projection_storage_allocate(
        (size_t)source.polygon_count*12u,"cloned mesh links");
    clone.uv=scene_projection_storage_allocate(
        (size_t)source.polygon_count*24u,"cloned mesh uv");
    clone.uv_work=scene_projection_storage_allocate(
        (size_t)source.polygon_count*24u,"cloned mesh uv work");
    clone.polygons=scene_projection_storage_allocate(
        (size_t)source.polygon_count*40u,"cloned mesh polygons");
    if(!output || !clone.name ||
       (source.vertex_count && !clone.transformed_vertices) ||
       (source.polygon_count && (!clone.links || !clone.uv ||
        !clone.uv_work || !clone.polygons)))goto fail;
    name_bytes=projection_span(clone.name,name_size);
    destination_packets=source.polygon_count?projection_span(clone.polygons,
        (size_t)source.polygon_count*40u):NULL;
    destination_links=source.polygon_count?projection_span(clone.links,
        (size_t)source.polygon_count*12u):NULL;
    destination_uv=source.polygon_count?projection_span(clone.uv,
        (size_t)source.polygon_count*24u):NULL;
    if(!name_bytes || (source.polygon_count &&
        (!destination_packets || !destination_links || !destination_uv)))
        goto fail;
    memcpy(name_bytes,name_block->data,name_size);
    clone.source_vertices=source.source_vertices;
    clone.vertex_count=source.vertex_count;
    clone.polygon_count=source.polygon_count;
    clone.radius_bits=source.radius_bits;
    memcpy(clone.center,source.center,sizeof(clone.center));
    memcpy(clone.auxiliary,source.auxiliary,sizeof(clone.auxiliary));
    if(source.optional_shorts){
        size_t bytes=(size_t)source.polygon_count*2u;
        clone.optional_shorts=scene_projection_storage_allocate(bytes,
            "cloned mesh short flags");
        if(!clone.optional_shorts)goto fail;
        memcpy(projection_span(clone.optional_shorts,bytes),
            projection_span(source.optional_shorts,bytes),bytes);
    }
    if(source.optional_bytes){
        size_t bytes=source.polygon_count;
        clone.optional_bytes=scene_projection_storage_allocate(bytes,
            "cloned mesh byte flags");
        if(!clone.optional_bytes)goto fail;
        memcpy(projection_span(clone.optional_bytes,bytes),
            projection_span(source.optional_bytes,bytes),bytes);
    }
    if(source.polygon_count){
        materials=calloc(source.polygon_count,sizeof(*materials));
        if(!materials)goto fail;
    }
    for(i=0;i<source.polygon_count;i++){
        size_t packet=(size_t)i*40u;
        model_u32(destination_packets,packet,3u);
        model_u32(destination_packets,packet+4u,clone.links+i*12u);
        model_u32(destination_packets,packet+8u,owned?
            owned->presentation[i].flags:
            model_read_u32(source_packets,packet+8u));
        model_u32(destination_packets,packet+16u,clone.uv_work+i*24u);
        model_u32(destination_packets,packet+20u,owned?
            owned->presentation[i].material_handle:
            model_read_u32(source_packets,packet+20u));
        if(scene_projection_storage_material(source.polygons+i*40u,
            &materials[i]))typed_materials++;
        for(j=0;j<3u;j++){
            uint32_t corner=i*3u+j;
            uint32_t old_vertex=owned?0u:
                model_read_u32(source_links,(size_t)corner*4u);
            uint32_t relative=owned?
                mesh->polygons[i].vertex[j]*16u:
                (old_vertex-source.transformed_vertices)&~15u;
            model_u32(destination_links,(size_t)corner*4u,
                clone.transformed_vertices+relative);
            if(owned)memcpy(destination_uv+(size_t)corner*8u,
                &owned->presentation[i].uv[j],8u);
            else memcpy(destination_uv+(size_t)corner*8u,
                source_uv+(size_t)corner*8u,8u);
        }
    }
    if(typed_materials==source.polygon_count && typed_materials &&
       !scene_projection_storage_attach_materials(clone.polygons,materials,
        typed_materials))goto fail;
    if(source.polygon_count && !scene_projection_storage_bind_links(clone.links))
        goto fail;
    if(!scene_projection_storage_write(output,&clone,sizeof(clone)))goto fail;
    if(source.source_vertices &&
       !scene_projection_storage_attach_shared_source(output,
        source.source_vertices))goto fail;
    free(materials);*result=output;
    if(owned)++stats.owned_descriptor_clones;
    return 1;
fail:
    free(materials);
    if(clone.optional_bytes)scene_projection_storage_free(clone.optional_bytes);
    if(clone.optional_shorts)scene_projection_storage_free(clone.optional_shorts);
    if(clone.polygons)scene_projection_storage_free(clone.polygons);
    if(clone.uv_work)scene_projection_storage_free(clone.uv_work);
    if(clone.uv)scene_projection_storage_free(clone.uv);
    if(clone.links)scene_projection_storage_free(clone.links);
    if(clone.transformed_vertices)
        scene_projection_storage_free(clone.transformed_vertices);
    if(clone.name)scene_projection_storage_free(clone.name);
    if(output)scene_projection_storage_free(output);
    return -1;
}
int scene_projection_storage_clone_descriptor(uint32_t source,
    uint32_t *clone){
    return clone_descriptor(source,NULL,NULL,clone);
}
int scene_projection_storage_clone_owned_descriptor(const SceneInstance *source,
    const SceneMesh *mesh,uint32_t *clone){
    if(!source)return 0;
    return clone_descriptor(source->descriptor,source,mesh,clone);
}
int scene_projection_storage_can_release_descriptor(uint32_t descriptor){
    SceneDescriptorState view;uint32_t members[9];unsigned i;
    if(!scene_projection_storage_references(descriptor) ||
       !scene_projection_storage_descriptor(descriptor,&view) ||
       view.auxiliary[1] || view.auxiliary[2])return 0;
    members[0]=view.state?view.source_vertices:0u;
    members[1]=view.uv;members[2]=view.uv_work;
    members[3]=view.links;members[4]=view.polygons;
    members[5]=view.transformed_vertices;members[6]=view.name;
    members[7]=view.optional_shorts;members[8]=view.optional_bytes;
    for(i=0;i<9u;i++){
        SceneProjectionBlock *block;
        if(!members[i])continue;
        block=model_page(members[i]);
        if(!block || !block->references)return 0;
    }
    return 1;
}
int scene_projection_storage_release_descriptor(uint32_t descriptor){
    static const size_t offsets[]={
        offsetof(SceneDescriptorState,uv),offsetof(SceneDescriptorState,uv_work),
        offsetof(SceneDescriptorState,links),offsetof(SceneDescriptorState,polygons),
        offsetof(SceneDescriptorState,transformed_vertices),
        offsetof(SceneDescriptorState,name),
        offsetof(SceneDescriptorState,optional_shorts),
        offsetof(SceneDescriptorState,optional_bytes)};
    SceneDescriptorState view;uint32_t members[8],zero=0;unsigned i;
    if(!scene_projection_storage_can_release_descriptor(descriptor))return 0;
    if(!scene_projection_storage_descriptor(descriptor,&view))return -1;
    if(view.state && view.source_vertices &&
       !scene_projection_storage_free(view.source_vertices))return -1;
    members[0]=view.uv;members[1]=view.uv_work;
    members[2]=view.links;members[3]=view.polygons;
    members[4]=view.transformed_vertices;members[5]=view.name;
    members[6]=view.optional_shorts;members[7]=view.optional_bytes;
    for(i=0;i<8u;i++){
        if(members[i] && !scene_projection_storage_free(members[i]))return -1;
        if(!scene_projection_storage_write(descriptor+(uint32_t)offsets[i],
            &zero,sizeof(zero)))return -1;
    }
    return 1;
}
int scene_projection_storage_claim_model_pages(uint32_t model_id,
    SceneModelProjectionPages *pages){
    uint32_t bases[5],i;SceneProjectionBlock *block;
    if(!model_id || !pages)return 0;
    bases[0]=pages->markers;bases[1]=pages->points;bases[2]=pages->planes;
    bases[3]=pages->secondary_points;bases[4]=pages->parts;
    for(i=0;i<5u;i++)if(bases[i] &&
        (!(block=model_page(bases[i])) || block->model_owner_id))return 0;
    for(i=0;i<pages->plane_count;i++){
        uint32_t a=pages->plane_vertices[i],b=pages->plane_secondary[i];
        if((a && (!(block=model_page(a)) || block->model_owner_id)) ||
           (b && (!(block=model_page(b)) || block->model_owner_id)))return 0;
    }
    for(i=0;i<5u;i++)if(bases[i])model_page(bases[i])->model_owner_id=model_id;
    for(i=0;i<pages->plane_count;i++){
        if(pages->plane_vertices[i])
            model_page(pages->plane_vertices[i])->model_owner_id=model_id;
        if(pages->plane_secondary[i])
            model_page(pages->plane_secondary[i])->model_owner_id=model_id;
    }
    free(pages->plane_vertices);free(pages->plane_secondary);
    pages->plane_vertices=pages->plane_secondary=NULL;
    return 1;
}
void scene_projection_storage_release_model_pages(uint32_t model_id){
    SceneProjectionBlock *block;
    if(!model_id)return;
    for(block=blocks;block;block=block->next)
        if(block->data && block->model_owner_id==model_id)
            scene_projection_storage_free(block->base);
}
void scene_projection_storage_retire_model_pages(uint32_t model_id){
    SceneProjectionBlock *block;
    if(!model_id)return;
    /* The game still frees these projected addresses through 00540170 after
       dropping the native model. Leave each page live until that free. */
    for(block=blocks;block;block=block->next)
        if(block->data && block->model_owner_id==model_id)
            block->model_owner_id=0u;
}

void scene_projection_storage_release_all(void) {
    SceneProjectionBlock *block = blocks;
    while (block) {
        SceneProjectionBlock *next = block->next;
        if (block->data) {
            free(block->data);
            ++stats.released; --stats.live;
        }
        free(block->hull);
        free(block->materials);
        free(block->material_known);
        free(block->packet_links);
        free(block);
        block = next;
    }
    blocks = NULL;
    last_block = NULL;
    free(blocks_by_address);
    blocks_by_address=NULL;
    block_count=block_capacity=free_block_count=0u;
    next_base = SCENE_PROJECTION_BASE;
    object_write_context=NULL;
    object_write_callback=NULL;
}
