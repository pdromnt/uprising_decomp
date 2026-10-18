#include "presentation_instance.h"
#include "scene_projection_storage.h"
#include "game_initializers.h"
#include "projection_owner_pages.h"
#include "scene_instance_owner.h"
#include "effect_model.h"
#include "trail_model.h"
#include "mesh_buffer.h"
#include "game_matrix.h"
#include "gameplay_geometry.h"
#include "gameplay_leaves.h"
#include "derived_state_copy.h"
#include "game_intrusive_list.h"
#include "attachment_effect.h"
#include "effect_initialization.h"

#include <stdlib.h>
#include <math.h>
#include <string.h>

typedef struct InstanceEntry {
    PresentationInstance instance;
    struct InstanceEntry *next;
} InstanceEntry;
static InstanceEntry *instances;
static InstanceEntry **instances_by_address;
static size_t instance_count, instance_capacity;
static InstanceEntry *last_pose;
static uint64_t last_gap_begin, last_gap_end;
const uint16_t *presentation_instance_owner_pages(void) { return projection_owner_pages(); }
static uint32_t instance_size(const InstanceEntry *entry){return entry->instance.has_player_controls?0x48cu:entry->instance.definition.type==0x88u?0x32cu:entry->instance.definition.type==0x2au?0x108u:entry->instance.has_spatial_sample?0x188u:entry->instance.lifecycle.has_runtime?0x13cu:entry->instance.has_attachment?0x124u:(entry->instance.has_fragment || entry->instance.definition.type==0x10u)?0xe8u:entry->instance.has_projectile?entry->instance.projectile.projection_size:entry->instance.has_arrival?0x104u:entry->instance.has_animation?0xf0u:(entry->instance.has_particle || entry->instance.has_expansion)?0xccu:entry->instance.definition.type==0x0bu?0xb8u:0xb4u;}
static void change_owner_pages(GameAddress object,uint32_t size, int adding) {
    (void)projection_owner_pages_change(object,size,adding);
}
int presentation_instance_projection_bounds(GameAddress *begin, GameAddress *end) {
    if (!instance_count) return 0;
    *begin = instances_by_address[0]->instance.projection;
    *end = instances_by_address[instance_count - 1u]->instance.projection + instance_size(instances_by_address[instance_count - 1u]);
    return 1;
}

static size_t instance_position(GameAddress object) {
    size_t first = 0u, last = instance_count;
    while (first < last) {
        size_t middle = first + (last - first) / 2u;
        if (instances_by_address[middle]->instance.projection < object) first = middle + 1u;
        else last = middle;
    }
    return first;
}
static int index_instance(InstanceEntry *entry) {
    size_t position = instance_position(entry->instance.projection);
    if (instance_count == instance_capacity) {
        size_t capacity = instance_capacity ? instance_capacity * 2u : 64u;
        InstanceEntry **next = realloc(instances_by_address, capacity * sizeof(*next));
        if (!next) return 0;
        instances_by_address = next; instance_capacity = capacity;
    }
    memmove(instances_by_address + position + 1u, instances_by_address + position,
        (instance_count - position) * sizeof(*instances_by_address));
    instances_by_address[position] = entry; ++instance_count;
    change_owner_pages(entry->instance.projection,instance_size(entry), 1);
    last_gap_begin = last_gap_end = 0u;
    return 1;
}
PresentationPose *presentation_instance_pose(GameAddress object) {
    size_t position = instance_position(object);
    return position < instance_count && instances_by_address[position]->instance.projection == object ?
        &instances_by_address[position]->instance.pose : NULL;
}
PresentationLifecycle *presentation_instance_lifecycle(GameAddress object){
    size_t position=instance_position(object);
    return position<instance_count && instances_by_address[position]->instance.projection==object?
        &instances_by_address[position]->instance.lifecycle:NULL;
}
PlayerControls *presentation_instance_player_controls(GameAddress object){
    size_t position=instance_position(object);
    return position<instance_count && instances_by_address[position]->instance.projection==object &&
        instances_by_address[position]->instance.has_player_controls?&instances_by_address[position]->instance.controls:NULL;
}
int presentation_instance_enable_player(GameMemory *memory,GameAddress object){
    size_t position=instance_position(object);InstanceEntry *entry;
    if(position>=instance_count || instances_by_address[position]->instance.projection!=object)return 1;
    entry=instances_by_address[position];if(entry->instance.has_player_controls)return 1;
    if(!player_controls_import(memory,object,&entry->instance.controls))return 0;
    if(!player_rig_import(memory,object,&entry->instance.rig))return 0;
    change_owner_pages(object,instance_size(entry),0);entry->instance.has_player_controls=1;
    change_owner_pages(object,instance_size(entry),1);last_gap_begin=last_gap_end=0u;
    return 1;
}
PlayerRig *presentation_instance_player_rig(GameAddress object){
    size_t position=instance_position(object);
    return position<instance_count && instances_by_address[position]->instance.projection==object &&
        instances_by_address[position]->instance.has_player_controls?&instances_by_address[position]->instance.rig:NULL;
}
EffectAnimation *presentation_instance_animation(GameAddress object){
    size_t p=instance_position(object);
    return p<instance_count && instances_by_address[p]->instance.projection==object &&
        instances_by_address[p]->instance.has_animation?&instances_by_address[p]->instance.animation:NULL;
}
int presentation_instance_enable_animation(GameMemory *memory,GameAddress object,int arrival){
    size_t p=instance_position(object);InstanceEntry *entry;EffectAnimation imported;unsigned o;
    if(p>=instance_count || instances_by_address[p]->instance.projection!=object)return 0;
    entry=instances_by_address[p];imported=entry->instance.animation;
    for(o=entry->instance.has_animation?0xf0u:(entry->instance.has_particle || entry->instance.has_expansion)?0xccu:0xb4u;o<(arrival?0x104u:0xf0u);o+=4u){
        uint32_t v=game_memory_read_u32(memory,object+o);
        if(!effect_animation_projection_write(&imported,o,&v,4u))return 0;
    }
    if(memory->fault)return 0;
    change_owner_pages(object,instance_size(entry),0);entry->instance.animation=imported;
    entry->instance.has_animation=1;if(arrival)entry->instance.has_arrival=1;
    change_owner_pages(object,instance_size(entry),1);last_gap_begin=last_gap_end=0u;return 1;
}
ParticleEffect *presentation_instance_particle(GameAddress object){
    size_t p=instance_position(object);
    return p<instance_count && instances_by_address[p]->instance.projection==object &&
        instances_by_address[p]->instance.has_particle?&instances_by_address[p]->instance.particle:NULL;
}
int presentation_instance_enable_particle(GameMemory *memory,GameAddress object){
    size_t p=instance_position(object);InstanceEntry *entry;ParticleEffect imported;
    if(p>=instance_count || instances_by_address[p]->instance.projection!=object)return 0;
    entry=instances_by_address[p];if(entry->instance.has_particle)return 1;
    if(!particle_effect_import(memory,object,&imported))return 0;
    change_owner_pages(object,instance_size(entry),0);entry->instance.particle=imported;
    entry->instance.has_particle=1;change_owner_pages(object,instance_size(entry),1);
    last_gap_begin=last_gap_end=0u;return 1;
}
ExpansionEffect *presentation_instance_expansion(GameAddress object){
    size_t p=instance_position(object);
    return p<instance_count && instances_by_address[p]->instance.projection==object &&
        instances_by_address[p]->instance.has_expansion?&instances_by_address[p]->instance.expansion:NULL;
}
int presentation_instance_enable_expansion(GameMemory *memory,GameAddress object){
    size_t p=instance_position(object);InstanceEntry *entry;ExpansionEffect imported;
    if(p>=instance_count || instances_by_address[p]->instance.projection!=object)return 0;
    entry=instances_by_address[p];if(entry->instance.has_expansion)return 1;
    if(!expansion_effect_import(memory,object,&imported))return 0;
    change_owner_pages(object,instance_size(entry),0);entry->instance.expansion=imported;
    entry->instance.has_expansion=1;change_owner_pages(object,instance_size(entry),1);
    last_gap_begin=last_gap_end=0u;return 1;
}
Projectile *presentation_instance_projectile(GameAddress object){
    size_t p=instance_position(object);
    return p<instance_count && instances_by_address[p]->instance.projection==object &&
        instances_by_address[p]->instance.has_projectile?&instances_by_address[p]->instance.projectile:NULL;
}
int presentation_instance_enable_projectile(GameMemory *memory,GameAddress object){
    size_t p=instance_position(object);InstanceEntry *entry;Projectile imported;
    if(p>=instance_count || instances_by_address[p]->instance.projection!=object)return 0;
    entry=instances_by_address[p];if(entry->instance.has_projectile)return 1;
    if(entry->instance.lifecycle.has_runtime || !projectile_import(memory,object,&imported))return 0;
    change_owner_pages(object,instance_size(entry),0);entry->instance.projectile=imported;
    entry->instance.has_projectile=1;change_owner_pages(object,instance_size(entry),1);
    last_gap_begin=last_gap_end=0u;return 1;
}
int presentation_instance_enable_fragment(GameMemory *memory,GameAddress object){
    size_t p=instance_position(object);InstanceEntry *entry;uint8_t record[0x34];unsigned i;
    if(p>=instance_count || instances_by_address[p]->instance.projection!=object)return 0;
    entry=instances_by_address[p];if(entry->instance.has_fragment)return 1;
    if(entry->instance.lifecycle.has_runtime)return 0;
    /* Allocator-backed records may sit outside the flat image. Read through
     * bounded accessors before the private span becomes native-owned. */
    for(i=0;i<sizeof(record);i++)record[i]=game_memory_read_u8(memory,object+0xb4u+i);
    if(memory->fault)return 0;
    change_owner_pages(object,instance_size(entry),0);
    memcpy(entry->instance.fragment_record,record,sizeof(record));entry->instance.has_fragment=1u;
    change_owner_pages(object,instance_size(entry),1);last_gap_begin=last_gap_end=0u;return 1;
}
static int fragment_read(PresentationInstance *instance,uint32_t offset,void *output,size_t size){
    if(!instance->has_fragment || offset<0xb4u || offset>0xe8u || size>0xe8u-offset)return 0;
    memcpy(output,instance->fragment_record+offset-0xb4u,size);return 1;
}
static int fragment_write(PresentationInstance *instance,uint32_t offset,const void *input,size_t size){
    if(!instance->has_fragment || offset<0xb4u || offset>0xe8u || size>0xe8u-offset)return 0;
    memcpy(instance->fragment_record+offset-0xb4u,input,size);return 1;
}
int presentation_instance_enable_attachment(GameMemory *memory,GameAddress object){
    size_t p=instance_position(object);InstanceEntry *entry;uint8_t record[0x70];unsigned i;
    if(p>=instance_count || instances_by_address[p]->instance.projection!=object)return 0;
    entry=instances_by_address[p];if(entry->instance.has_attachment)return 1;
    if(entry->instance.lifecycle.has_runtime || entry->instance.has_animation ||
        entry->instance.has_fragment || entry->instance.has_projectile)return 0;
    for(i=0;i<sizeof(record);i++)record[i]=game_memory_read_u8(memory,object+0xb4u+i);
    if(memory->fault)return 0;
    change_owner_pages(object,instance_size(entry),0);
    memcpy(entry->instance.attachment_record,record,sizeof(record));entry->instance.has_attachment=1;
    change_owner_pages(object,instance_size(entry),1);last_gap_begin=last_gap_end=0u;return 1;
}
int presentation_instance_reserve_attachment_buffers(GameAddress object){
    size_t p=instance_position(object);PresentationInstance *instance;GameAddress vertices,controller;
    if(p>=instance_count || instances_by_address[p]->instance.projection!=object)return 0;
    instance=&instances_by_address[p]->instance;if(instance->pose.vertex_count>20u)return 0;
    vertices=scene_projection_storage_allocate(240u,"attachment vertices");
    if(!vertices)return 0;
    controller=scene_projection_storage_allocate(240u,"attachment controller");
    if(!controller){scene_projection_storage_free(vertices);return 0;}
    if(instance->vertices)scene_projection_storage_free(instance->vertices);
    if(instance->controller)scene_projection_storage_free(instance->controller);
    instance->vertices=instance->pose.vertices=vertices;
    instance->controller=instance->pose.controller=controller;return 1;
}
static int attachment_read(PresentationInstance *instance,uint32_t offset,void *output,size_t size){
    if(!instance->has_attachment || offset<0xb4u || offset>0x124u || size>0x124u-offset)return 0;
    memcpy(output,instance->attachment_record+offset-0xb4u,size);return 1;
}
static int attachment_write(PresentationInstance *instance,uint32_t offset,const void *input,size_t size){
    if(!instance->has_attachment || offset<0xb4u || offset>0x124u || size>0x124u-offset)return 0;
    memcpy(instance->attachment_record+offset-0xb4u,input,size);return 1;
}
int presentation_instance_enable_spatial_sample(GameMemory *memory,GameAddress object){
    size_t p=instance_position(object);InstanceEntry *entry;uint8_t record[0xd4];unsigned i;
    if(p>=instance_count || instances_by_address[p]->instance.projection!=object)return 0;
    entry=instances_by_address[p];if(entry->instance.has_spatial_sample)return 1;
    if(entry->instance.lifecycle.has_runtime || entry->instance.has_player_controls || entry->instance.has_attachment || entry->instance.has_animation || entry->instance.has_fragment || entry->instance.has_projectile)return 0;
    for(i=0u;i<sizeof(record);i++)record[i]=game_memory_read_u8(memory,object+0xb4u+i);
    if(memory->fault)return 0;
    change_owner_pages(object,instance_size(entry),0);
    memcpy(entry->instance.spatial_record,record,sizeof(record));entry->instance.has_spatial_sample=1u;
    change_owner_pages(object,instance_size(entry),1);last_gap_begin=last_gap_end=0u;return 1;
}
static int spatial_read(PresentationInstance *instance,uint32_t offset,void *output,size_t size){
    if(!instance->has_spatial_sample || offset<0xb4u || offset>0x188u || size>0x188u-offset)return 0;
    memcpy(output,instance->spatial_record+offset-0xb4u,size);return 1;
}
static int spatial_write(PresentationInstance *instance,uint32_t offset,const void *input,size_t size){
    if(!instance->has_spatial_sample || offset<0xb4u || offset>0x188u || size>0x188u-offset)return 0;
    memcpy(instance->spatial_record+offset-0xb4u,input,size);return 1;
}
static int presentation_stream_field(GameMemory *memory,GameAddress address,int reading,uint32_t *marker,
    size_t width,size_t count,uint32_t tag,const TaggedRecordStreamServices *services){
    uint8_t bytes[12];size_t index,total=width*count;uint32_t header,payload;int success;
    if(total>sizeof(bytes))return 0;
    for(index=0u;index<total;index++)bytes[index]=game_memory_read_u8(memory,address+(uint32_t)index);
    if(memory->fault)return 0;
    success=reading?tagged_record_stream_read_elements(marker,bytes,width,count,&header,&payload,services):
        tagged_record_stream_write_elements(marker,tag,bytes,width,count,services);
    if(success && reading)for(index=0u;index<total;index++)game_memory_write_u8(memory,address+(uint32_t)index,bytes[index]);
    return success && !memory->fault;
}
int presentation_instance_stream(GameMemory *memory,GameAddress object,int reading,unsigned layer,uint32_t *marker,const PresentationArchiveServices *services){
    static const unsigned angle_fields[3]={0x28u,0x26u,0x24u};unsigned axis;uint8_t kind;GameAddress catalog,model;uint32_t flags;
    if(!services || !services->records || !marker || layer>3u)return 0;
    if(layer==3u){
        if(reading){game_memory_write_u32(memory,object+0x168u,0u);game_memory_write_u32(memory,object+0x178u,0u);}
        if(!presentation_stream_field(memory,object+0xc4u,reading,marker,4u,1u,0xfedcba93u,services->records) ||
           !presentation_stream_field(memory,object+0xc8u,reading,marker,4u,1u,0xfedcba93u,services->records))return 0;
    }
    if(layer>=2u && !presentation_stream_field(memory,object+0xb8u,reading,marker,1u,1u,0xfedcba91u,services->records))return 0;
    if(!presentation_stream_field(memory,object+0x14u,reading,marker,4u,1u,0xfedcba90u,services->records) ||
       !presentation_stream_field(memory,object+0x18u,reading,marker,4u,3u,0xfedcba9bu,services->records))return 0;
    for(axis=0u;axis<3u;axis++)if(!presentation_stream_field(memory,object+angle_fields[axis],reading,marker,2u,1u,0xfedcba97u,services->records))return 0;
    for(axis=0u;axis<3u;axis++)if(!presentation_stream_field(memory,object+0x58u+axis*4u,reading,marker,4u,1u,0xfedcba92u,services->records))return 0;
    if(!reading)return !memory->fault;
    kind=game_memory_read_u8(memory,object+12u);
    if(kind!=0xfeu){
        catalog=game_memory_read_u32(memory,0x5f0210u+(game_memory_read_u32(memory,object+0x14u)&63u)*4u);
        model=game_memory_read_u32(memory,object+0x54u);
        game_memory_write_u32(memory,model+0x34u,catalog+0x10u);game_memory_write_u32(memory,model+0x38u,catalog+0x10u);
        if(memory->fault || !services->frames || !services->frames(services->context,object,0))return 0;
    }
    flags=game_memory_read_u32(memory,object+0x10u);kind=game_memory_read_u8(memory,object+12u);
    if(!(flags&0x100u) && kind!=0xfeu && kind!=0xa2u && (!services->publish || !services->publish(services->context,object)))return 0;
    if(layer){
        for(axis=0u;axis<3u;axis++){
            game_memory_write_u32(memory,object+0x64u+axis*4u,game_memory_read_u32(memory,object+0x18u+axis*4u));
            game_memory_write_u16(memory,object+0x78u-axis*2u,game_memory_read_u16(memory,object+0x24u+axis*2u));
        }
        game_memory_write_u32(memory,object+0x7cu,0u);
        if(game_memory_read_u8(memory,object+12u)!=0xfeu && (!services->frames || !services->frames(services->context,object,1)))return 0;
        game_memory_write_u32(memory,object+0xb0u,0u);
    }
    return !memory->fault;
}
int presentation_instance_reset(GameMemory *memory,GameAddress object,unsigned kind,const PresentationResetServices *services){
    size_t position=instance_position(object);PresentationInstance *instance;
    if(position>=instance_count || instances_by_address[position]->instance.projection!=object)return 0;
    instance=&instances_by_address[position]->instance;
    return presentation_lifecycle_reset(memory,object,&instance->pose,&instance->lifecycle,kind,services)?1:-1;
}
static InstanceEntry *pose_at_address(GameAddress address) {
    size_t position;
    if (!instance_count || address < instances_by_address[0]->instance.projection ||
        (uint64_t)address >= (uint64_t)instances_by_address[instance_count - 1u]->instance.projection + instance_size(instances_by_address[instance_count - 1u]))
        return NULL;
    /* Vertex/model reads often repeat inside the same non-owner allocation.
     * Cache that interval without bypassing any native-owned field. Creation
     * and destruction invalidate it before the address set changes. */
    if ((uint64_t)address >= last_gap_begin && (uint64_t)address < last_gap_end) return NULL;
    if (last_pose && address - last_pose->instance.projection < instance_size(last_pose)) return last_pose;
    position = instance_position(address);
    if (position == instance_count || instances_by_address[position]->instance.projection != address) {
        if (!position) return NULL;
        --position;
    }
    last_pose = instances_by_address[position];
    if (address - last_pose->instance.projection < instance_size(last_pose)) return last_pose;
    last_gap_begin = (uint64_t)last_pose->instance.projection + instance_size(last_pose);
    last_gap_end = position + 1u < instance_count ?
        instances_by_address[position + 1u]->instance.projection : UINT64_C(0x100000000);
    return NULL;
}
int presentation_instance_projection_read(GameAddress address, void *output, size_t size) {
    InstanceEntry *entry = pose_at_address(address);
    return entry && (presentation_pose_projection_read(&entry->instance.pose,
        address - entry->instance.projection, output, size) ||
        (entry->instance.has_animation && size<=instance_size(entry)-(address-entry->instance.projection) &&
        effect_animation_projection_read(&entry->instance.animation,address-entry->instance.projection,output,size)) ||
        (entry->instance.has_particle && particle_effect_projection_read(&entry->instance.particle,address-entry->instance.projection,output,size)) ||
        (entry->instance.has_expansion && expansion_effect_projection_read(&entry->instance.expansion,address-entry->instance.projection,output,size)) ||
        (entry->instance.has_projectile && projectile_projection_read(&entry->instance.projectile,address-entry->instance.projection,output,size)) ||
        fragment_read(&entry->instance,address-entry->instance.projection,output,size) ||
        attachment_read(&entry->instance,address-entry->instance.projection,output,size) ||
        spatial_read(&entry->instance,address-entry->instance.projection,output,size) ||
        presentation_lifecycle_projection_read(&entry->instance.lifecycle,
        address-entry->instance.projection,output,size) ||
        (entry->instance.has_player_controls && player_controls_projection_read(&entry->instance.controls,
        address-entry->instance.projection,output,size)) ||
        (entry->instance.has_player_controls && player_rig_projection_read(&entry->instance.rig,
        address-entry->instance.projection,output,size)));
}
int presentation_instance_projection_write(GameAddress address, const void *input, size_t size) {
    InstanceEntry *entry = pose_at_address(address);
    return entry && (presentation_pose_projection_write(&entry->instance.pose,
        address - entry->instance.projection, input, size) ||
        (entry->instance.has_animation && size<=instance_size(entry)-(address-entry->instance.projection) &&
        effect_animation_projection_write(&entry->instance.animation,address-entry->instance.projection,input,size)) ||
        (entry->instance.has_particle && particle_effect_projection_write(&entry->instance.particle,address-entry->instance.projection,input,size)) ||
        (entry->instance.has_expansion && expansion_effect_projection_write(&entry->instance.expansion,address-entry->instance.projection,input,size)) ||
        (entry->instance.has_projectile && projectile_projection_write(&entry->instance.projectile,address-entry->instance.projection,input,size)) ||
        fragment_write(&entry->instance,address-entry->instance.projection,input,size) ||
        attachment_write(&entry->instance,address-entry->instance.projection,input,size) ||
        spatial_write(&entry->instance,address-entry->instance.projection,input,size) ||
        presentation_lifecycle_projection_write(&entry->instance.lifecycle,
        address-entry->instance.projection,input,size) ||
        (entry->instance.has_player_controls && player_controls_projection_write(&entry->instance.controls,
        address-entry->instance.projection,input,size)) ||
        (entry->instance.has_player_controls && player_rig_projection_write(&entry->instance.rig,
        address-entry->instance.projection,input,size)));
}

const PresentationInstance *presentation_instance_find(GameAddress object) {
    size_t position=instance_position(object);
    return position<instance_count && instances_by_address[position]->instance.projection==object?
        &instances_by_address[position]->instance:NULL;
}
int presentation_instance_source_is_empty(GameAddress source){
    InstanceEntry *entry;
    for(entry=instances;entry;entry=entry->next)if(entry->instance.definition.source==source)return 0;
    return 1;
}

int presentation_instance_create_copy(GameMemory *memory,GameAddress object,
        GameAddress source,int runtime,const PresentationConstructionServices *services){
    PresentationInstanceDefinition d;GameAddress model,clone;int owned_clone=0;
    unsigned i;
    memset(&d,0,sizeof(d));d.source=source;d.copy_vertices=1;
    d.type=game_memory_read_u8(memory,source+12u);
    d.flags=game_memory_read_u32(memory,source+16u);
    for(i=0u;i<3u;++i){uint32_t bits=game_memory_read_u32(memory,source+0x2cu+i*4u);
        memcpy(&d.dimensions[i],&bits,4u);}
    d.radius=game_memory_read_u32(memory,source+0x38u);
    d.resource=game_memory_read_u32(memory,source+0x3cu);
    d.count=(int16_t)game_memory_read_u16(memory,source+0x44u);
    model=game_memory_read_u32(memory,source+0x54u);d.model=model;
    if(memory->fault)return 0;
    if(model){
        int result=0;
        /*The actual allocation owner selects an ordinary clone algorithm.
         *Owned native models do not need a retail virtual-table copy in memory. */
        if(scene_instance_owner_is_holder(model))result=scene_instance_owner_clone(memory,model,&clone);
        else if(effect_model_find(model)){clone=effect_model_clone(memory,model);result=clone?1:-1;}
        else if(trail_model_find(model)){clone=trail_model_clone(memory,model);result=clone?1:-1;}
        else if(mesh_buffer_is_native(memory,model)){clone=mesh_buffer_clone(memory,model,mesh_buffer_owned_services());result=clone?1:-1;}
        else {
            /*Immutable legacy profiles can be inputs before an instance exists.
             *These IDs select native decoders; no guest call is dispatched. */
            GameAddress table=game_memory_read_u32(memory,model);
            GameAddress method=game_memory_read_u32(memory,table+4u);
            if(memory->fault)return 0;
            if(method==0x00409df0u){clone=effect_model_clone(memory,model);result=clone?1:-1;}
            else if(method==0x004da510u){clone=trail_model_clone(memory,model);result=clone?1:-1;}
            else if(method==0x00540ad0u){clone=mesh_buffer_clone(memory,model,mesh_buffer_owned_services());result=clone?1:-1;}
        }
        if(result<0 || memory->fault)return 0;
        if(result>0)owned_clone=1;
        else if(!services || !services->clone_model ||
            !services->clone_model(services->context,model,&clone))return 0;
        d.model=clone;
    }
    if(presentation_instance_create(memory,object,&d,runtime))return 1;
    if(owned_clone && !trail_model_release(d.model) && !effect_model_release(d.model)){
        if(mesh_buffer_is_native(memory,d.model)){
            if(mesh_buffer_release(memory,d.model,mesh_buffer_owned_services()))scene_projection_storage_free(d.model);
        }else scene_instance_owner_discard_clone(d.model);
    }
    return 0;
}

int presentation_instance_take_render_buffers(GameMemory *m,GameAddress object,GameAddress vertices,GameAddress controller){
    InstanceEntry *entry;
    for(entry=instances;entry && entry->instance.projection!=object;entry=entry->next);
    if(!entry || m->fault || !vertices || !controller || vertices==controller || entry->instance.vertices || entry->instance.controller)return 0;
    entry->instance.vertices=vertices;entry->instance.controller=controller;
    entry->instance.pose.vertices=vertices;entry->instance.pose.controller=controller;
    game_memory_write_u32(m,object+0x40,vertices);game_memory_write_u32(m,object+0x70,controller);
    return 1; /*Ownership transferred; memory.fault reports failed publication. */
}
int presentation_instance_create(GameMemory *memory, GameAddress object,
        const PresentationInstanceDefinition *d, int runtime) {
    InstanceEntry *entry;
    PresentationInstance *instance;
    unsigned axis;
    if (!memory || !object || !d || presentation_instance_find(object) ||
            d->count < 0 || (uint32_t)d->count > UINT32_MAX / 12u) return 0;
    entry = calloc(1, sizeof(*entry));
    if (!entry) return 0;
    instance = &entry->instance;
    instance->projection = object;
    instance->definition = *d;
    if (d->count) {
        size_t size = (size_t)d->count * 12u;
        if (d->copy_vertices) {
            instance->vertices = scene_projection_storage_allocate(size,
                "presentation instance vertices");
            if (!instance->vertices) goto failed;
        }
        instance->controller = scene_projection_storage_allocate(size,
            "presentation instance controller");
        if (!instance->controller) goto failed;
    }
    game_initializer_0042cfb0(memory, object);
    game_memory_write_u8(memory, object + 0xcu, d->type);
    game_memory_write_u32(memory, object + 0x10u, d->flags | 1u);
    for (axis = 0u; axis < 3u; ++axis) {
        uint32_t bits;
        memcpy(&bits, &d->dimensions[axis], sizeof(bits));
        game_memory_write_u32(memory, object + 0x2cu + axis * 4u, bits);
    }
    game_memory_write_u32(memory, object + 0x38u, d->radius);
    game_memory_write_u32(memory, object + 0x3cu, d->resource);
    game_memory_write_u16(memory, object + 0x44u, (uint16_t)d->count);
    game_memory_write_u32(memory, object + 0x40u, instance->vertices);
    game_memory_write_u32(memory, object + 0x50u, d->source);
    game_memory_write_u32(memory, object + 0x54u, d->model);
    game_memory_write_u32(memory, object + 0x70u, instance->controller);
    game_memory_write_u32(memory, object + 0xb0u, 0u);
    game_memory_write_u32(memory, object, 0x005764f8u);
    if (runtime) game_initializer_presentation_runtime(memory, object,
        d->flags | 0x41u);
    if (memory->fault || !presentation_pose_import(memory, object, &instance->pose) ||
        !presentation_lifecycle_import(memory,object,&instance->lifecycle,runtime) ||
        !index_instance(entry)) goto failed;
    entry->next = instances; instances = entry;
    return 1;
failed:
    if (instance->vertices) scene_projection_storage_free(instance->vertices);
    if (instance->controller) scene_projection_storage_free(instance->controller);
    free(entry);
    return 0;
}

static float articulated_float(GameMemory *memory,GameAddress address){
    uint32_t bits=game_memory_read_u32(memory,address);float value;memcpy(&value,&bits,4u);return value;
}
static void articulated_store_float(GameMemory *memory,GameAddress address,float value){
    uint32_t bits;memcpy(&bits,&value,4u);game_memory_write_u32(memory,address,bits);
}
static void articulated_vector(GameMemory *memory,GameAddress destination,GameAddress source){
    unsigned axis;for(axis=0u;axis<3u;axis++)game_memory_write_u32(memory,destination+axis*4u,game_memory_read_u32(memory,source+axis*4u));
}
static void articulated_model_parts(GameMemory *memory,GameAddress object){
    GameAddress part=game_memory_read_u32(memory,game_memory_read_u32(memory,object+0x54u)+0x4cu);
    unsigned index;static const uint32_t main_offsets[3]={0x1c8u,0x200u,0x238u};
    static const uint32_t child_offsets[3]={0x270u,0x2a8u,0x2e0u};
    game_memory_write_u32(memory,object+0xecu,part);
    for(index=0u;index<3u;index++){
        part=game_memory_read_u32(memory,part+0x44u);
        game_memory_write_u32(memory,object+main_offsets[index],part);
        game_memory_write_u32(memory,object+child_offsets[index],game_memory_read_u32(memory,part+0x4cu));
    }
    game_memory_write_u32(memory,object+0x2fcu,0u);
    game_memory_write_u32(memory,object+0x30cu,0u);
    game_memory_write_u32(memory,object+0x31cu,0u);
}
static void articulated_probes(GameMemory *memory,GameAddress object,uint32_t first,uint32_t second){
    GameAddress vertices=game_memory_read_u32(memory,object+0x3cu),row;
    long double scale=articulated_float(memory,0x572d28u);uint32_t index;float bound;
    articulated_vector(memory,object+0x300u,vertices+12u);
    articulated_vector(memory,object+0x310u,vertices+12u);
    articulated_store_float(memory,object+0x308u,(float)((long double)articulated_float(memory,vertices+24u)*scale+articulated_float(memory,object+0x308u)));
    articulated_store_float(memory,object+0x314u,(float)((long double)articulated_float(memory,vertices+24u)*scale+articulated_float(memory,object+0x314u)));
    for(index=0u;index<first;index++){
        /* Preserve the original look-ahead radius in the Z probe gate. */
        row=vertices+index*28u;
        if((long double)articulated_float(memory,row+52u)*scale+articulated_float(memory,row+20u)>articulated_float(memory,object+0x308u)){
            articulated_vector(memory,object+0x300u,row+12u);
            articulated_store_float(memory,object+0x308u,(float)((long double)articulated_float(memory,row+24u)*scale+articulated_float(memory,object+0x308u)));
        }
    }
    for(index=0u;index<first;index++){
        row=vertices+index*28u;
        if((long double)articulated_float(memory,row+24u)*scale+articulated_float(memory,row+16u)>articulated_float(memory,object+0x314u)){
            articulated_vector(memory,object+0x310u,row+12u);
            articulated_store_float(memory,object+0x314u,(float)((long double)articulated_float(memory,row+24u)*scale+articulated_float(memory,object+0x314u)));
        }
    }
    row=vertices+first*28u;articulated_vector(memory,object+0x320u,row+12u);
    bound=articulated_float(memory,row+24u);
    for(index=0u;index<second;index++){
        row=vertices+(first+index)*28u;
        /* The comparison keeps the initial radius, even after replacement. */
        if(articulated_float(memory,row+24u)>bound){
            articulated_vector(memory,object+0x320u,row+12u);
            articulated_store_float(memory,object+0x324u,(float)((long double)articulated_float(memory,row+24u)*scale+articulated_float(memory,object+0x324u)));
        }
    }
}
int presentation_instance_articulated_create(GameMemory *memory,GameAddress object,const PresentationArticulatedDefinition *definition){
    PresentationInstanceDefinition base;float rotation[12],translation[12]={1,0,0,0,0,1,0,0,0,0,1,0},matrix[12];
    uint64_t count;uint32_t offset;unsigned index,axis;float sine,cosine;
    static const uint32_t groups_at[7]={0xfcu,0x1ccu,0x204u,0x23cu,0x274u,0x2acu,0x2e4u};
    static const uint32_t anchors_at[5]={0x130u,0x13cu,0xf0u,0x148u,0x154u};
    if(!definition)return 0;
    count=(uint64_t)definition->groups[0]+definition->groups[1]+3u*((uint64_t)definition->groups[2]+definition->groups[3]);
    if(count>65536u)return 0;
    base=definition->base;base.count=(int32_t)count;base.type=0x88u;base.flags|=0x8100u;
    if(!presentation_instance_create(memory,object,&base,0))return 0;
    game_memory_write_u32(memory,object,0x572c78u);
    offset=definition->groups[0];
    for(index=0u;index<7u;index++){
        game_memory_write_u32(memory,object+groups_at[index],offset);
        if(index==0u)offset+=definition->groups[1];
        else if(index<4u)offset+=definition->groups[2];
        else offset+=definition->groups[3];
    }
    for(index=0u;index<5u;index++)for(axis=0u;axis<3u;axis++)articulated_store_float(memory,object+anchors_at[index]+axis*4u,definition->anchors[index][axis]);
    articulated_store_float(memory,object+0xf4u,(float)(((long double)definition->spans[1]+definition->spans[0])*articulated_float(memory,0x572d1cu)+definition->anchors[2][1]));
    sine=articulated_float(memory,0x62ab50u);cosine=articulated_float(memory,0x62a350u);
    for(index=0u;index<12u;index++)rotation[index]=0.0f;
    rotation[0]=sine;rotation[2]=cosine;rotation[5]=1.0f;rotation[8]=-cosine;rotation[10]=sine;
    for(axis=0u;axis<3u;axis++)translation[axis*4u+3u]=-definition->anchors[5][axis];
    memcpy(matrix,rotation,sizeof(matrix));
    matrix[3]=(float)((long double)cosine*translation[11]+(long double)sine*translation[3]);
    matrix[7]=translation[7];
    matrix[11]=(float)((long double)(-cosine)*translation[3]+(long double)sine*translation[11]);
    for(axis=0u;axis<3u;axis++)translation[axis*4u+3u]=definition->anchors[5][axis];
    game_matrix_compose_values(rotation,translation,matrix);
    for(index=0u;index<12u;index++)articulated_store_float(memory,object+0x100u+index*4u,rotation[index]);
    articulated_store_float(memory,object+0x160u,definition->spans[0]);articulated_store_float(memory,object+0x164u,definition->spans[1]);
    game_memory_write_u32(memory,object+0xb4u,0u);articulated_model_parts(memory,object);
    articulated_probes(memory,object,definition->groups[0],definition->groups[1]);
    return !memory->fault;
}
int presentation_instance_articulated_copy(GameMemory *memory,GameAddress object,GameAddress source,const PresentationConstructionServices *services){
    if(!presentation_instance_create_copy(memory,object,source,0,services))return 0;
    game_memory_write_u32(memory,object,0x572c78u);
    if(!game_derived_state_copy_sparse(memory,object,source))return 0;
    game_memory_write_u8(memory,object+0xcu,0x88u);
    game_memory_write_u32(memory,object+0x10u,game_memory_read_u32(memory,object+0x10u)|0x8100u);
    game_memory_write_u32(memory,object+0xb4u,0u);
    articulated_model_parts(memory,object);return !memory->fault;
}
static void articulated_hinge(GameMemory *memory,GameAddress output,uint16_t angle,
        GameAddress pivot,GameAddress anchor){
    long double sine=articulated_float(memory,0x6298a8u+((uint32_t)angle&2047u)*4u);
    long double cosine=articulated_float(memory,0x6298a8u+((uint32_t)(angle+512u)&2047u)*4u);
    long double y=articulated_float(memory,anchor+4u),z=articulated_float(memory,anchor+8u);
    float matrix[12]={1,0,0,0,0,0,0,0,0,0,0,0};unsigned index;
    matrix[3]=articulated_float(memory,pivot);matrix[5]=(float)cosine;matrix[6]=(float)-sine;
    matrix[9]=(float)sine;matrix[10]=(float)cosine;
    matrix[7]=(float)((-y*cosine+z*sine)+articulated_float(memory,pivot+4u)+y);
    matrix[11]=(float)((-y*sine-z*cosine)+articulated_float(memory,pivot+8u)+z);
    for(index=0u;index<12u;index++)articulated_store_float(memory,output+index*4u,matrix[index]);
}
int presentation_instance_articulated_present(GameMemory *memory,GameAddress object){
    uint16_t angles[3],hinge;unsigned axis,index;float matrix[12]={1,0,0,0,0,1,0,0,0,0,1,0};
    long double span,cosine;
    for(axis=0u;axis<3u;axis++)angles[axis]=game_memory_read_u16(memory,object+0x24u+axis*2u);
    game_matrix_attachment_orientation(memory,matrix,angles);
    for(axis=0u;axis<3u;axis++)matrix[axis*4u+3u]=articulated_float(memory,object+0x18u+axis*4u);
    for(index=0u;index<12u;index++)articulated_store_float(memory,object+0x80u+index*4u,matrix[index]);
    hinge=game_memory_read_u16(memory,object+0x2eau);
    cosine=articulated_float(memory,0x6298a8u+((uint32_t)(hinge+512u)&2047u)*4u);
    span=(long double)articulated_float(memory,object+0x160u)+articulated_float(memory,object+0x164u);
    for(index=0u;index<12u;index++)matrix[index]=0.0f;
    matrix[0]=matrix[5]=matrix[10]=1.0f;
    matrix[3]=articulated_float(memory,object+0xf0u);
    matrix[7]=(float)(span*cosine+articulated_float(memory,object+0xf4u));
    matrix[11]=articulated_float(memory,object+0xf8u);
    for(index=0u;index<12u;index++)articulated_store_float(memory,object+0xbcu+index*4u,matrix[index]);
    articulated_hinge(memory,object+0x198u,hinge,object+0x130u,object+0x148u);
    articulated_hinge(memory,object+0x168u,(uint16_t)(0u-(uint32_t)hinge*2u),object+0x13cu,object+0x154u);
    return game_matrix_compose(memory,object+0x1d0u,object+0x100u,object+0x198u) &&
           game_matrix_compose(memory,object+0x208u,object+0x100u,object+0x1d0u);
}
int presentation_instance_bind_source(GameMemory *memory,GameAddress object,uint32_t identity,
    GameAddress point,const uint16_t angles[3],GameAddress source,int adjust,
    const PresentationActivationServices *services){
    float position[3];unsigned axis;
    if(!services || !services->place)return 0;
    game_memory_write_u32(memory,object+0xb4u,source);
    game_memory_write_u8(memory,object+0xb8u,0u);
    for(axis=0u;axis<3u;axis++)position[axis]=articulated_float(memory,point+axis*4u);
    if(adjust){
        position[1]=(float)(gameplay_geometry_terrain_height(memory,position[0],position[2])+
            articulated_float(memory,object+0x30u));
        /* This shared operation mutates the caller's point before placement. */
        articulated_store_float(memory,point+4u,position[1]);
    }
    return !memory->fault && services->place(services->context,object,identity,position,angles);
}
int presentation_instance_articulated_activate(GameMemory *memory,GameAddress object,
    uint32_t identity,GameAddress point,const uint16_t angles[3],GameAddress source,
    int adjust,const PresentationActivationServices *services,uint32_t *value){
    GameAddress nested,catalog;uint32_t mode,result=0u;
    if(!services || !services->catalog_value)return 0;
    game_memory_write_u16(memory,object+0x2e8u,0u);
    game_memory_write_u16(memory,object+0x2ecu,0u);
    game_memory_write_u16(memory,object+0x2eau,256u);
    if(!presentation_instance_bind_source(memory,object,identity,point,angles,source,adjust,services))return 0;
    source=game_memory_read_u32(memory,object+0xb4u);
    nested=game_memory_read_u32(memory,source+4u);
    mode=game_memory_read_u32(memory,nested+0x14u)==0u?4u:
        game_memory_read_u32(memory,source+0x10u)==1u?1u:0u;
    game_memory_write_u32(memory,object+0x2f4u,mode);
    game_memory_write_u32(memory,object+0x2f8u,game_memory_read_u32(memory,0x5ac6acu));
    catalog=game_memory_read_u32(memory,0x5f0210u+(game_memory_read_u32(memory,object+0x14u)&63u)*4u);
    if(memory->fault || !services->catalog_value(services->context,catalog,0x88u,5u,0u,&result))return 0;
    game_memory_write_u32(memory,object+0x2f0u,result);
    game_memory_write_u32(memory,object+0x31cu,0u);
    game_memory_write_u32(memory,object+0x30cu,0u);
    game_memory_write_u32(memory,object+0x2fcu,0u);
    if(value)*value=result;
    return !memory->fault;
}
static void articulated_read_matrix(GameMemory *memory,GameAddress address,float matrix[12]){
    unsigned index;for(index=0u;index<12u;index++)matrix[index]=articulated_float(memory,address+index*4u);
}
int presentation_instance_articulated_death(GameMemory *memory,GameAddress object,GameAddress other,const PresentationArticulatedDeathServices *services){
    static const unsigned models[3]={0x1c8u,0x2a8u,0x2e0u};
    static const unsigned first_vertices[3]={0x1ccu,0x2acu,0x2e4u};
    static const unsigned matrices[3]={0x198u,0x1d0u,0x208u};
    GameAddress catalog,header,child,vertices;uint32_t variant,start,end;unsigned axis,part,profile;
    float position[3],velocity[3]={0.0f,40.0f,0.0f},world[12],local[12],composed[12],hinge[12],transform[12];
    const float upward[3]={0.0f,100.0f,0.0f};
    if(!services || !services->begin || !services->sound || !services->mesh_fragments ||
       !services->variant || !services->anchors || !services->acquire || !services->emit)return 0;
    catalog=game_memory_read_u32(memory,0x5f0210u+(game_memory_read_u32(memory,object+0x14u)&63u)*4u);
    if(memory->fault || !services->begin(services->context,object,other))return 0;
    for(axis=0u;axis<3u;axis++)position[axis]=articulated_float(memory,object+0x18u+axis*4u);
    if(!services->sound(services->context,position,(game_memory_read_u32(memory,0x5ac6acu)&1u)?0x82:0x83))return 0;
    if(other && (game_memory_read_u32(memory,other+0x10u)&2u)){
        for(axis=0u;axis<3u;axis++){
            long double value=(long double)articulated_float(memory,other+0xb4u+axis*4u)*articulated_float(memory,0x572d4cu);
            if(axis==1u)value+=articulated_float(memory,0x572d50u);
            velocity[axis]=(float)value;
        }
    }
    articulated_read_matrix(memory,object+0x80u,world);
    header=game_memory_read_u32(memory,game_memory_read_u32(memory,object+0x54u)+0x50u);
    if(memory->fault || !services->mesh_fragments(services->context,object,header,world,velocity) ||
       !services->variant(services->context,catalog,&variant) ||
       !services->anchors(services->context,object,(int32_t)game_memory_read_u32(memory,0x5acf10u+variant*4u),
            game_memory_read_u32(memory,0x5acf00u+variant*4u),upward))return 0;
    for(part=0u;part<3u;part++){
        AttachmentEmission request;uint16_t angles[2];float magnitude;uint32_t duration;
        memset(&request,0,sizeof(request));
        header=game_memory_read_u32(memory,game_memory_read_u32(memory,object+models[part])+0x50u);
        for(profile=0u;profile<3u;profile++){
            int32_t minimum=(int32_t)game_memory_read_u32(memory,0x5a0488u+profile*8u);
            int32_t maximum=(int32_t)game_memory_read_u32(memory,0x5a048cu+profile*8u);
            if(minimum>=(int32_t)game_memory_read_u32(memory,header+0x14u) &&
               maximum>=(int32_t)game_memory_read_u32(memory,header+0x1cu))break;
        }
        if(memory->fault)return 0;
        child=services->acquire(services->context,(uint16_t)profile);if(!child)return 0;
        start=game_memory_read_u32(memory,object+first_vertices[part]);
        end=part==0u?game_memory_read_u32(memory,object+0x204u):part==1u?
            game_memory_read_u32(memory,object+0x2e4u):(uint32_t)(int32_t)(int16_t)game_memory_read_u16(memory,object+0x44u);
        vertices=game_memory_read_u32(memory,object+0x3cu)+start*28u;
        if(!attachment_effect_configure(memory,child,header,vertices,(uint16_t)(end-start)))goto failed;
        articulated_read_matrix(memory,object+0x80u,world);
        articulated_read_matrix(memory,object+matrices[part],local);
        game_matrix_compose_values(composed,world,local);
        if(part){
            articulated_read_matrix(memory,object+0x168u,hinge);
            game_matrix_compose_values(transform,composed,hinge);
            if(part==1u)for(axis=0u;axis<3u;axis++){
                unsigned row=axis*4u;
                long double translation=(long double)world[row+2u]*local[11u];
                long double offset=(long double)composed[row+2u]*hinge[11u];
                translation+=(long double)world[row+1u]*local[7u];
                translation+=(long double)world[row]*local[3u];
                translation+=world[row+3u];
                offset+=(long double)composed[row+1u]*hinge[7u];
                offset+=(long double)composed[row]*hinge[3u];
                /* The inline second-part composition keeps the first
                 * translation on the x87 stack across its rounded basis. */
                transform[row+3u]=(float)(translation+offset);
            }
        }else memcpy(transform,composed,sizeof(transform));
        if(!game_matrix_extract_heading_pitch(memory,transform,angles))goto failed;
        request.angles[0]=angles[0];request.angles[1]=angles[1];
        for(axis=0u;axis<3u;axis++)request.position[axis]=transform[axis*4u+3u];
        magnitude=(float)(int32_t)(effect_initialization_random(memory,articulated_float(memory,0x572d5cu))+30u);
        request.velocity[0]=magnitude;
        magnitude=(float)(int32_t)(effect_initialization_random(memory,articulated_float(memory,0x572d5cu))+30u);
        request.velocity[2]=magnitude;
        if(effect_initialization_random(memory,articulated_float(memory,0x572d60u)))request.velocity[0]=-request.velocity[0];
        if(effect_initialization_random(memory,articulated_float(memory,0x572d60u)))request.velocity[2]=-request.velocity[2];
        duration=effect_initialization_random(memory,articulated_float(memory,0x572d64u))+60u;
        request.velocity[1]=(float)(int32_t)(effect_initialization_random(memory,articulated_float(memory,0x572d68u))+40u);
        request.duration=duration;request.mode=3u;request.flags=0x17u;
        request.source=game_memory_read_u32(memory,object+0x14u);
        if(memory->fault || !services->emit(services->context,catalog,child,&request))goto failed;
        continue;
failed:
        if(services->discard)(void)services->discard(services->context,child);
        return 0;
    }
    game_terrain_impact(memory,articulated_float(memory,object+0x18u),articulated_float(memory,object+0x20u),1024);
    return !memory->fault;
}
int presentation_instance_articulated_render(GameMemory *memory,GameAddress object){
    GameAddress model=game_memory_read_u32(memory,object+0x54u);
    GameAddress upper=game_memory_read_u32(memory,object+0x1c8u)+4u;
    GameAddress lower=game_memory_read_u32(memory,object+0x270u)+4u;
    uint16_t angles[3],hinge=game_memory_read_u16(memory,object+0x2ecu);
    float matrix[12];unsigned index;
    angles[0]=game_memory_read_u16(memory,object+0x78u);
    angles[1]=game_memory_read_u16(memory,object+0x76u);
    angles[2]=game_memory_read_u16(memory,object+0x74u);
    game_matrix_attachment_orientation(memory,matrix,angles);
    for(index=0u;index<3u;index++)matrix[index*4u+3u]=articulated_float(memory,object+0x64u+index*4u);
    for(index=0u;index<12u;index++)articulated_store_float(memory,model+4u+index*4u,matrix[index]);
    for(index=0u;index<12u;index++)matrix[index]=0.0f;
    matrix[0]=matrix[5]=matrix[10]=1.0f;
    matrix[3]=articulated_float(memory,object+0xf0u);matrix[11]=articulated_float(memory,object+0xf8u);
    matrix[7]=(float)(((long double)articulated_float(memory,object+0x160u)+articulated_float(memory,object+0x164u))*
        articulated_float(memory,0x6298a8u+((uint32_t)(hinge+512u)&2047u)*4u)+articulated_float(memory,object+0xf4u));
    for(index=0u;index<12u;index++)articulated_store_float(memory,game_memory_read_u32(memory,object+0xecu)+4u+index*4u,matrix[index]);
    articulated_hinge(memory,upper,hinge,object+0x130u,object+0x148u);
    articulated_hinge(memory,lower,(uint16_t)(0u-(uint32_t)hinge*2u),object+0x13cu,object+0x154u);
    for(index=0u;index<12u;index++)game_memory_write_u32(memory,
        game_memory_read_u32(memory,object+0x2a8u)+4u+index*4u,game_memory_read_u32(memory,lower+index*4u));
    for(index=0u;index<12u;index++)game_memory_write_u32(memory,
        game_memory_read_u32(memory,object+0x2e0u)+4u+index*4u,game_memory_read_u32(memory,lower+index*4u));
    return game_matrix_compose(memory,game_memory_read_u32(memory,object+0x200u)+4u,object+0x100u,upper) &&
        game_matrix_compose(memory,game_memory_read_u32(memory,object+0x238u)+4u,object+0x100u,game_memory_read_u32(memory,object+0x200u)+4u);
}
static void articulated_point(GameMemory *memory,GameAddress output,const float matrix[12],GameAddress source,const uint8_t order[3][3]){
    float point[3],result[3];unsigned axis,index;
    for(axis=0u;axis<3u;axis++)point[axis]=articulated_float(memory,source+axis*4u);
    for(axis=0u;axis<3u;axis++){
        long double value=(long double)point[order[axis][0]]*matrix[axis*4u+order[axis][0]];
        for(index=1u;index<3u;index++)value+=(long double)point[order[axis][index]]*matrix[axis*4u+order[axis][index]];
        result[axis]=(float)(value+matrix[axis*4u+3u]);
    }
    for(axis=0u;axis<3u;axis++)articulated_store_float(memory,output+axis*4u,result[axis]);
}
static int articulated_vertices(GameMemory *memory,GameAddress object,int rendered){
    float matrices[8][12],part[12];uint32_t bounds[9],index;unsigned range;
    GameAddress source,output,template_object;int32_t count;
    static const uint32_t fields[7]={0xfcu,0x1ccu,0x204u,0x23cu,0x274u,0x2acu,0x2e4u};
    static const uint8_t orders[8][3][3]={
        {{1,2,0},{1,2,0},{1,0,2}},
        {{1,2,0},{1,2,0},{1,2,0}},
        {{2,1,0},{2,1,0},{2,1,0}},
        {{0,1,2},{0,1,2},{0,1,2}},
        {{1,2,0},{1,2,0},{1,2,0}},
        {{1,2,0},{1,2,0},{1,2,0}},
        {{0,2,1},{0,2,1},{0,2,1}},
        {{0,1,2},{0,1,2},{0,1,2}}
    };
    static const uint8_t rendered_orders[8][3][3]={
        {{1,2,0},{1,2,0},{1,0,2}},{{0,1,2},{0,1,2},{0,1,2}},
        {{0,2,1},{0,2,1},{0,2,1}},{{0,1,2},{0,1,2},{0,1,2}},
        {{0,1,2},{0,1,2},{0,1,2}},{{0,2,1},{0,2,1},{0,2,1}},
        {{1,2,0},{1,2,0},{1,2,0}},{{0,1,2},{0,1,2},{0,1,2}}
    };
    static const uint32_t upper_fields[3]={0x1c8u,0x200u,0x238u};
    static const uint32_t lower_fields[3]={0x270u,0x2a8u,0x2e0u};
    count=(int16_t)game_memory_read_u16(memory,object+0x44u);
    if(count<0)return 0;
    bounds[0]=0u;bounds[8]=(uint32_t)count;
    for(range=0u;range<7u;range++)bounds[range+1u]=game_memory_read_u32(memory,object+fields[range]);
    for(range=0u;range<8u;range++)if(bounds[range]>bounds[range+1u])return 0;
    articulated_read_matrix(memory,object+0x80u,matrices[0]);
    articulated_read_matrix(memory,object+0xbcu,part);game_matrix_compose_values(matrices[1],matrices[0],part);
    if(rendered){
        articulated_read_matrix(memory,game_memory_read_u32(memory,object+0x54u)+4u,matrices[0]);
        for(range=0u;range<3u;range++){
            articulated_read_matrix(memory,game_memory_read_u32(memory,object+upper_fields[range])+4u,part);
            game_matrix_compose_values(matrices[range+2u],matrices[0],part);
            articulated_read_matrix(memory,game_memory_read_u32(memory,object+lower_fields[range])+4u,part);
            game_matrix_compose_values(matrices[range+5u],matrices[range+2u],part);
        }
    }else{
        articulated_read_matrix(memory,object+0x198u,part);game_matrix_compose_values(matrices[2],matrices[0],part);
        articulated_read_matrix(memory,object+0x1d0u,part);game_matrix_compose_values(matrices[3],matrices[0],part);
        articulated_read_matrix(memory,object+0x208u,part);game_matrix_compose_values(matrices[4],matrices[0],part);
        articulated_read_matrix(memory,object+0x168u,part);
        game_matrix_compose_values(matrices[5],matrices[2],part);
        game_matrix_compose_values(matrices[6],matrices[3],part);
        game_matrix_compose_values(matrices[7],matrices[4],part);
    }
    source=game_memory_read_u32(memory,object+0x3cu);output=game_memory_read_u32(memory,object+(rendered?0x70u:0x40u));
    /* Visit ranges in the original reverse order, retaining overlap behavior. */
    for(range=8u;range>0u;range--){
        memcpy(part,matrices[range-1u],sizeof(part));
        for(index=bounds[range-1u];index<bounds[range];index++){
        articulated_point(memory,output+index*12u,part,source+index*28u+12u,rendered?rendered_orders[range-1u]:orders[range-1u]);
        if(memory->fault)return 0;
    }
    }
    if(rendered)return !memory->fault;
    template_object=game_memory_read_u32(memory,object+0x50u);
    articulated_point(memory,object+0x300u,matrices[0],template_object+0x300u,orders[0]);
    articulated_point(memory,object+0x310u,matrices[0],template_object+0x310u,orders[0]);
    articulated_point(memory,object+0x320u,matrices[1],template_object+0x320u,orders[0]);
    return !memory->fault;
}
int presentation_instance_articulated_vertices(GameMemory *memory,GameAddress object){return articulated_vertices(memory,object,0);}
int presentation_instance_articulated_render_vertices(GameMemory *memory,GameAddress object){return articulated_vertices(memory,object,1);}
static int32_t articulated_signed(uint32_t value){
    return value<=INT32_MAX?(int32_t)value:(int32_t)((int64_t)value-4294967296LL);
}
static int articulated_effect_create(GameMemory *memory,GameAddress object,unsigned index,
    const PresentationArticulatedServices *services){
    static const uint32_t fields[3]={0x2fcu,0x30cu,0x31cu};
    static const float widths[3]={1000.0f,500.0f,400.0f},heights[3]={800.0f,700.0f,400.0f};
    float position[3];GameAddress effect=0u;unsigned axis;
    for(axis=0u;axis<3u;axis++)position[axis]=articulated_float(memory,object+0x300u+index*16u+axis*4u);
    if(memory->fault || !services->create_effect || !services->create_effect(services->context,object,position,widths[index],heights[index],&effect))return 0;
    game_memory_write_u32(memory,object+fields[index],effect);return !memory->fault;
}
static int articulated_effect_retire(GameMemory *memory,GameAddress object,uint32_t field,
    int immediate,const PresentationArticulatedServices *services){
    GameAddress effect=game_memory_read_u32(memory,object+field);
    if(memory->fault || !services->retire_effect || !services->retire_effect(services->context,effect,immediate))return 0;
    game_memory_write_u32(memory,object+field,0u);return !memory->fault;
}
int presentation_instance_articulated_update(GameMemory *memory,GameAddress object,
    const PresentationArticulatedServices *services){
    uint32_t tick,limit,health,mode,remaining,amount;GameAddress source,nested,child;
    int visible;
    if(!services)return 0;
    visible=game_terrain_cell_presence(memory,object,0u)!=0u;
    tick=game_memory_read_u32(memory,0x5ac6acu);
    if(!(tick&15u) && visible){
        if(!services->health_limit || !services->health_limit(services->context,object,&limit))return 0;
        health=game_memory_read_u32(memory,object+0x58u);
        child=game_memory_read_u32(memory,object+0x2fcu);
        if(articulated_signed(health*2u)<articulated_signed(limit)){
            if(!child && !articulated_effect_create(memory,object,0u,services))return 0;
        }else if(child && !articulated_effect_retire(memory,object,0x2fcu,0,services))return 0;
        child=game_memory_read_u32(memory,object+0x30cu);
        if(articulated_signed(health*4u)<articulated_signed(limit)){
            if(!child && (!articulated_effect_create(memory,object,1u,services) || !articulated_effect_create(memory,object,2u,services)))return 0;
        }else if(child && (!articulated_effect_retire(memory,object,0x30cu,0,services) || !articulated_effect_retire(memory,object,0x31cu,0,services)))return 0;
    }
    child=game_memory_read_u32(memory,object+0x2fcu);
    if(visible && child){
        if((tick&3u)==0u && (!services->surface_effect || !services->surface_effect(services->context,object,child+0x18u)))return 0;
        child=game_memory_read_u32(memory,object+0x30cu);
        if(child && (tick&3u)==2u && (!services->surface_effect || !services->surface_effect(services->context,object,child+0x18u)))return 0;
    }
    mode=game_memory_read_u32(memory,object+0x2f4u);
    source=game_memory_read_u32(memory,object+0xb4u);
    if(mode==4u){
        nested=game_memory_read_u32(memory,source+4u);
        if(articulated_signed(game_memory_read_u32(memory,nested+0x14u))<=0)return !memory->fault;
        mode=1u;game_memory_write_u32(memory,object+0x2f4u,mode);
    }
    if(mode==0u && source && game_memory_read_u32(memory,source+0x10u)==1u){
        nested=game_memory_read_u32(memory,source+4u);
        mode=articulated_signed(game_memory_read_u32(memory,nested+0x14u))>0?1u:4u;
        game_memory_write_u32(memory,object+0x2f4u,mode);
    }
    if(articulated_signed(tick)%30==0 && source && (mode==1u || mode==2u)){
        nested=game_memory_read_u32(memory,source+4u);
        remaining=game_memory_read_u32(memory,nested+0x14u);amount=game_memory_read_u32(memory,object+0x2f0u);
        if(articulated_signed(amount)>=articulated_signed(remaining))amount=remaining;
        game_memory_write_u32(memory,nested+0x14u,remaining-amount);
        if(memory->fault || !services->credit || !services->credit(services->context,game_memory_read_u32(memory,nested+0x54u),amount))return 0;
        if(!game_memory_read_u32(memory,nested+0x14u)){
            mode=3u;game_memory_write_u32(memory,object+0x2f4u,mode);
            game_memory_write_u32(memory,object+0x2f8u,game_memory_read_u32(memory,0x5ac6acu));
        }
    }
    if((mode==1u || mode==3u) && articulated_signed(game_memory_read_u32(memory,object+0x2f8u)+300u)<articulated_signed(game_memory_read_u32(memory,0x5ac6acu))){
        mode=mode==1u?2u:4u;game_memory_write_u32(memory,object+0x2f4u,mode);
        game_memory_write_u32(memory,object+0x2f8u,game_memory_read_u32(memory,0x5ac6acu));
    }
    return !memory->fault && presentation_instance_articulated_advance(memory,object,services);
}
static uint32_t articulated_truncate(long double value){
    if(!isfinite(value) || value < (long double)INT64_MIN || value >= -((long double)INT64_MIN))return 0u;
    return (uint32_t)(int64_t)value;
}
/*438280: display tick, position and hinge interpolation in the same owned
 *legacy record. The divisor and fraction have separate binary32 stores. */
int presentation_instance_articulated_interpolate(GameMemory *memory,GameAddress object){
    static const unsigned current[4]={0x24u,0x26u,0x28u,0x2eau},local[4]={0x78u,0x76u,0x74u,0x2ecu};
    uint32_t tick=game_memory_read_u32(memory,0x5acbe8u),prior=game_memory_read_u32(memory,object+0x7cu);
    unsigned i;float fraction=0.0f,stored;long double duration;int snap=1;
    if(memory->fault)return 0;
    game_memory_write_u32(memory,object+0x7cu,tick);
    if(articulated_signed(tick-prior)<=10){
        duration=(long double)articulated_signed(game_memory_read_u32(memory,0x5acc80u)-tick)+articulated_float(memory,0x5acc38u);
        stored=(float)duration;
        if(duration>articulated_float(memory,0x572d20u)){
            fraction=(float)((long double)articulated_float(memory,0x572d20u)/stored);snap=0;
        }
    }
    for(i=0u;i<3u;i++){
        float position=articulated_float(memory,object+0x18u+i*4u);
        if(!snap){float previous=articulated_float(memory,object+0x64u+i*4u);
            position=(float)(((long double)position-previous)*fraction+previous);}
        articulated_store_float(memory,object+0x64u+i*4u,position);
    }
    for(i=0u;i<4u;i++){
        uint16_t angle=game_memory_read_u16(memory,object+current[i]);
        if(!snap){
            uint16_t old=game_memory_read_u16(memory,object+local[i]);
            int32_t difference=(int32_t)((angle-old)&2047u);
            if(difference>1024)difference-=2048;
            angle=(uint16_t)(old+articulated_truncate((long double)difference*fraction))&2047u;
            if(i && angle>1024u)angle=(uint16_t)(angle-2048u);
        }
        game_memory_write_u16(memory,object+local[i],angle);
    }
    return !memory->fault;
}
int presentation_instance_articulated_advance(GameMemory *memory,GameAddress object,
    const PresentationArticulatedServices *services){
    uint32_t mode=game_memory_read_u32(memory,object+0x2f4u),tick=game_memory_read_u32(memory,0x5ac6acu),delta=0u;
    uint16_t prior=game_memory_read_u16(memory,object+0x2e8u),next;int32_t signed_prior=(int16_t)prior;
    GameAddress head,node,target,child;unsigned visits=0u,axis,index;float position[3];
    static const uint32_t fields[3]={0x2fcu,0x30cu,0x31cu};
    if(!services)return 0;
    if(mode==2u)delta=21u;
    else if(mode==1u || mode==3u){
        uint32_t start=game_memory_read_u32(memory,object+0x2f8u);
        uint32_t elapsed=mode==1u?tick-start:start-tick+300u;
        delta=articulated_truncate(((long double)articulated_signed(elapsed)*articulated_float(memory,0x572d40u))*articulated_float(memory,0x572d44u));
    }
    next=(uint16_t)(prior+delta*((signed_prior>512 && signed_prior<=1536)?1u:2u));
    game_memory_write_u16(memory,object+0x2e8u,next);
    if((signed_prior<512 && (int16_t)next>=512) || (signed_prior<1536 && (int16_t)next>=1536)){
        if(!services->sound || !services->sound(services->context,object,128))return 0;
    }
    next=(uint16_t)(next&2047u);game_memory_write_u16(memory,object+0x2e8u,next);
    delta=articulated_truncate(((long double)articulated_float(memory,0x6298a8u+(uint32_t)next*4u)+articulated_float(memory,0x572d20u))*articulated_float(memory,0x572d48u));
    game_memory_write_u16(memory,object+0x2eau,(uint16_t)(delta+128u));
    if(!presentation_instance_articulated_present(memory,object) || !presentation_instance_articulated_vertices(memory,object) ||
       !services->gather || !services->gather(services->context,object))return 0;
    head=game_memory_read_u32(memory,object+0xb0u);node=head;
    while(node && !memory->fault){
        if(++visits>65536u)return 0;
        target=game_memory_read_u32(memory,node+12u);
        for(axis=0u;axis<3u;axis++)position[axis]=articulated_float(memory,node+16u+axis*4u);
        if(!services->command || !services->command(services->context,object,target,position) ||
           !services->notify_command || !services->notify_command(services->context,target,object,position))return 0;
        if(!game_memory_read_u32(memory,object+0x58u))return !memory->fault;
        node=game_memory_read_u32(memory,node+4u);
    }
    if(head){
        if(!services->release_commands || !services->release_commands(services->context,head))return 0;
        game_memory_write_u32(memory,object+0xb0u,0u);
    }
    if(game_memory_read_u32(memory,object+0x2fcu)){
        for(index=0u;index<3u;index++){
            child=game_memory_read_u32(memory,object+fields[index]);
            if(index==1u && !child)break;
            for(axis=0u;axis<3u;axis++)position[axis]=articulated_float(memory,object+0x300u+index*16u+axis*4u);
            if(!services->refresh_effect || !services->refresh_effect(services->context,child,position))return 0;
        }
    }
    return !memory->fault;
}
int presentation_instance_source_reset(GameMemory *memory,GameAddress object,int notify,
    const PresentationArticulatedServices *services){
    GameAddress source;
    if(!services || !services->reset)return 0;
    if(notify && articulated_signed(game_memory_read_u32(memory,object+0x58u))<=0 &&
       (!services->deactivate || !services->deactivate(services->context,object)))return 0;
    if(memory->fault || !services->reset(services->context,object))return 0;
    source=game_memory_read_u32(memory,object+0xb4u);
    if(notify && source && (!services->detach_source || !services->detach_source(services->context,source,object)))return 0;
    game_memory_write_u32(memory,object+0xb4u,0u);return !memory->fault;
}
int presentation_instance_articulated_reset(GameMemory *memory,GameAddress object,int notify,
    const PresentationArticulatedServices *services){
    /* Preserve the coupled-child gates of the supported class. */
    if(game_memory_read_u32(memory,object+0x2fcu)){
        if(!articulated_effect_retire(memory,object,0x2fcu,1,services))return 0;
        if(game_memory_read_u32(memory,object+0x30cu)){
            if(!articulated_effect_retire(memory,object,0x30cu,1,services) ||
               !articulated_effect_retire(memory,object,0x31cu,1,services))return 0;
        }
    }
    return !memory->fault && presentation_instance_source_reset(memory,object,notify,services);
}
static int articulated_discard_inputs(const PresentationArticulatedDefinition *definition,const PresentationArticulatedFactoryServices *services){
    if(definition->base.model && (!services->retirement.release_model ||
       !services->retirement.release_model(services->retirement.context,definition->base.model)))return 0;
    return !definition->base.resource || (services->retirement.release_storage &&
        services->retirement.release_storage(services->retirement.context,definition->base.resource));
}
GameAddress presentation_articulated_template(GameMemory *memory,uint16_t slot,const PresentationArticulatedFactoryServices *services){
    GameAddress object,publication;PresentationArticulatedDefinition definition={0};int loaded;
    if(slot>=2u || !services || !services->describe || !services->allocate || !services->release)return 0u;
    publication=0x5add90u+(uint32_t)slot*4u;object=game_memory_read_u32(memory,publication);
    if(object || memory->fault)return object;
    if(game_memory_read_u32(memory,0x624f8cu)==10u || memory->fault)return 0u;
    loaded=services->describe(services->context,slot,&definition);
    if(loaded<=0){if(loaded<0)memory->fault=1;return 0u;}
    object=services->allocate(services->context,0x32cu);
    if(!object){(void)articulated_discard_inputs(&definition,services);memory->fault=1;return 0u;}
    if(!presentation_instance_articulated_create(memory,object,&definition)){
        if(presentation_instance_find(object))(void)presentation_instance_release(memory,object,&services->retirement,1,0);
        else (void)articulated_discard_inputs(&definition,services);
        (void)services->release(services->context,object);memory->fault=1;return 0u;
    }
    game_memory_write_u32(memory,publication,object);return memory->fault?0u:object;
}
int presentation_articulated_fill(GameMemory *memory,uint16_t slot,int32_t count,const PresentationArticulatedFactoryServices *services){
    GameAddress source=presentation_articulated_template(memory,slot,services);int32_t index;
    if(!source || memory->fault)return 0;
    if(count>65536){memory->fault=1;return 0;}
    for(index=0;index<count;index++){
        GameAddress object=services->allocate(services->context,0x32cu);
        if(!object){memory->fault=1;return 0;}
        if(!presentation_instance_articulated_copy(memory,object,source,&services->construction)){
            if(presentation_instance_find(object))(void)presentation_instance_release(memory,object,&services->retirement,1,0);
            (void)services->release(services->context,object);memory->fault=1;return 0;
        }
        (void)game_list_insert_after(memory,object,source,4u,8u);if(memory->fault)return 0;
    }
    return 1;
}
GameAddress presentation_articulated_take(GameMemory *memory,uint16_t slot,const PresentationArticulatedFactoryServices *services){
    GameAddress source=presentation_articulated_template(memory,slot,services),object;
    if(!source || memory->fault)return 0u;
    if(!game_memory_read_u32(memory,source+4u) && !presentation_articulated_fill(memory,slot,1,services))return 0u;
    object=game_memory_read_u32(memory,source+4u);
    if(!object || memory->fault)return 0u;
    (void)game_list_detach_node(memory,object,8u,4u);return memory->fault?0u:object;
}
static int unlink_projection(GameMemory *memory, GameAddress object,
        unsigned forward, unsigned backward) {
    GameAddress next = game_memory_read_u32(memory, object + forward);
    GameAddress previous = game_memory_read_u32(memory, object + backward);
    if (memory->fault) return 0;
    if (next) {
        game_memory_write_u32(memory, next + backward, previous);
        if (previous) game_memory_write_u32(memory, previous + forward, next);
        game_memory_write_u32(memory, object + forward, 0u);
        if (forward == 8u) game_memory_write_u32(memory, object + backward, 0u);
    }
    return !memory->fault;
}

int presentation_instance_release(GameMemory *memory, GameAddress object,
        const PresentationReleaseServices *s, int controller, int runtime) {
    InstanceEntry **cursor = &instances;
    InstanceEntry *entry;
    PresentationInstance *instance;
    GameAddress member;
    if (!memory || !s) return -1;
    while (*cursor && (*cursor)->instance.projection != object)
        cursor = &(*cursor)->next;
    if (!*cursor) return 0;
    entry = *cursor; instance = &entry->instance;
    if (instance->release_phase == 0u) {
        if (runtime) {
            member = game_memory_read_u32(memory, object + 0xdcu);
            if (memory->fault || (member && (!s->detach_packet_chain ||
                    !s->detach_packet_chain(s->context, member)))) return -1;
            game_memory_write_u32(memory, object + 0xdcu, 0u);
        }
        instance->release_phase = 1u;
    }
    if (instance->release_phase == 1u) {
        if (controller) {
            member = game_memory_read_u32(memory, object + 0xb0u);
            if (memory->fault || (member && (!s->release_pool_member ||
                    !s->release_pool_member(s->context, member)))) return -1;
            game_memory_write_u32(memory, object + 0xb0u, 0u);
            if (instance->controller &&
                    !scene_projection_storage_free(instance->controller)) return -1;
            instance->controller = 0u;
            instance->pose.controller = 0u;
            game_memory_write_u32(memory, object + 0x70u, 0u);
        }
        instance->release_phase = 2u;
    }
    if (instance->release_phase == 2u) {
        game_memory_write_u32(memory, object + 0x58u, 0u);
        if (!unlink_projection(memory, object, 8u, 4u) ||
            !unlink_projection(memory, object, 0x4cu, 0x48u)) return -1;
        instance->release_phase = 3u;
    }
    if (instance->release_phase == 3u) {
        member = game_memory_read_u32(memory, object + 0x54u);
        /* Graph release may fail after freeing nodes. Drop borrowed links first. */
        instance->pose.model_instance=NULL;
        instance->rig.mount_instance=instance->rig.turret_instance=NULL;
        if (memory->fault || (member && (!s->release_model ||
                !s->release_model(s->context, member)))) return -1;
        game_memory_write_u32(memory, object + 0x54u, 0u);
        instance->pose.model = 0u;
        instance->release_phase = 4u;
    }
    if (instance->release_phase == 4u) {
        if (instance->vertices &&
                !scene_projection_storage_free(instance->vertices)) return -1;
        instance->vertices = 0u;
        instance->pose.vertices = 0u;
        game_memory_write_u32(memory, object + 0x40u, 0u);
        if (!instance->definition.source && !instance->has_attachment) {
            member = game_memory_read_u32(memory, object + 0x3cu);
            if (memory->fault || (member && (!s->release_storage ||
                    !s->release_storage(s->context, member)))) return -1;
            game_memory_write_u32(memory, object + 0x3cu, 0u);
        }
        instance->release_phase = 5u;
    }
    if (memory->fault) return -1;
    game_memory_write_u32(memory, object, 0x00572230u);
    if (memory->fault) return -1;
    /* Attachments borrow independently owned expansion children. Level teardown
     * can retire a child first; invalidate its links before its identity/storage
     * can be recycled. Do not turn an unknown child into a successful removal. */
    if (instance->has_expansion) {
        InstanceEntry *parent;
        for (parent = instances; parent; parent = parent->next) {
            unsigned offset;
            if (!parent->instance.has_attachment) continue;
            for (offset = 0xecu; offset < 0x100u; offset += 4u) {
                uint32_t child;
                uint8_t *slot = parent->instance.attachment_record + offset - 0xb4u;
                memcpy(&child, slot, sizeof(child));
                if (child == object) memset(slot, 0, sizeof(child));
            }
        }
    }
    {
        size_t position = instance_position(object);
        memmove(instances_by_address + position, instances_by_address + position + 1u,
            (instance_count - position - 1u) * sizeof(*instances_by_address));
        --instance_count;
        change_owner_pages(object,instance_size(entry), 0);
        last_gap_begin = last_gap_end = 0u;
        if (last_pose == entry) last_pose = NULL;
    }
    *cursor = entry->next; free(entry);
    return 1;
}
