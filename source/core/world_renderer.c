#include "world_renderer.h"
#include "render_arena.h"
#include "terrain_asset.h"
#include "gameplay_geometry.h"
#include "scene_projection_storage.h"
#include "render_packet_view.h"
#include <stdlib.h>
#include <string.h>
static WorldRendererStats world_stats;
static float read_f32(GameMemory *,uint32_t);
static int world_owned_read(GameAddress address,void *output,size_t size){
    return render_arena_read(address,output,size) ||
        scene_projection_storage_read(address,output,size);
}
static int world_owned_point_list(const RenderArenaPacket *packet,
    GameplayPointList *output){
    uint32_t index;
    if(packet->count>0x100000u || packet->material>255u)return 0;
    memset(output,0,sizeof(*output));
    output->count=packet->count;output->palette=packet->material;
    if(packet->count){
        output->items=calloc(packet->count,sizeof(*output->items));
        if(!output->items)return 0;
    }
    for(index=0;index<packet->count;index++){
        GameplayPointInput *point=&output->items[index];
        uint32_t bits;
        GameAddress position=packet->vertices+index*12u;
        if(!world_owned_read(packet->attributes+index,&point->active,1u))
            goto fail;
        if(!point->active)continue;
        if(!world_owned_read(position+8u,&bits,4u))goto fail;
        memcpy(&point->z,&bits,4u);
        if((int32_t)bits<=(int32_t)0x42c7ffffu ||
           (int32_t)bits>=(int32_t)0x466a6001u)continue;
        if(!world_owned_read(position,&bits,4u))goto fail;
        memcpy(&point->x,&bits,4u);
        if(!world_owned_read(position+4u,&bits,4u))goto fail;
        memcpy(&point->y,&bits,4u);
    }
    return 1;
fail:
    gameplay_geometry_release_point_list(output);
    return 0;
}
void world_renderer_stats(WorldRendererStats *output){
    if(output)*output=world_stats;
}
void world_renderer_note_texture_owner(int owned){
    if(owned)++world_stats.owned_texture_binds;
    else ++world_stats.external_texture_binds;
}
static void world_packet_capture(GameMemory *memory,GameAddress address,
    RenderArenaPacket *output){
    RenderArenaPacket *native=render_packet_view(address);
    uint32_t words[10]={0};unsigned word;
    if(native){*output=*native;++world_stats.native_packets;return;}
    ++world_stats.guest_packets;
    for(word=0;word<10u && !memory->fault;word++)
        words[word]=game_memory_read_u32(memory,address+word*4u);
    memcpy(output,words,sizeof(*output));
}
static void world_packet_capture_vertices(GameMemory *memory,
    const RenderArenaPacket *packet,GameWorldVertexInput input[32]){
    uint32_t *indices=render_arena_indices(packet->vertices,packet->count);
    RenderArenaUv *uv=render_arena_uv(packet->attributes,packet->count);
    uint32_t index;
    for(index=0;index<packet->count && !memory->fault;index++){
        uint32_t reference;
        RenderArenaVertex projected_vertex,*vertex;
        AssetMeshUV projected_uv;
        int scene_attribute=0;
        GameWorldVertexInput *vertex_input=&input[index];
        if(indices)reference=indices[index];
        else if(scene_projection_storage_read(packet->vertices+index*4u,
            &reference,sizeof(reference)))++world_stats.scene_indices;
        else{reference=game_memory_read_u32(memory,packet->vertices+index*4u);
            ++world_stats.guest_indices;}
        vertex=render_arena_vertices(reference,1u);
        if(vertex)++world_stats.native_vertices;
        else if(scene_projection_storage_read(reference,&projected_vertex,
            sizeof(projected_vertex))){vertex=&projected_vertex;
            ++world_stats.scene_vertices;}
        else ++world_stats.guest_vertices;
        if(!uv && scene_projection_storage_read_uv(
            packet->attributes+index*8u,&projected_uv))scene_attribute=1;
        if(uv)++world_stats.native_uv;
        else if(scene_attribute)++world_stats.scene_uv;
        else ++world_stats.guest_uv;
        vertex_input->x=vertex?vertex->position[0]:read_f32(memory,reference);
        vertex_input->y=vertex?vertex->position[1]:read_f32(memory,reference+4u);
        vertex_input->depth=vertex?vertex->position[2]:read_f32(memory,reference+8u);
        vertex_input->light=vertex?vertex->light:read_f32(memory,reference+12u);
        if(uv){vertex_input->u=uv[index].u;vertex_input->v=uv[index].v;}
        else if(scene_attribute){
            memcpy(&vertex_input->u,&projected_uv.u_bits,4u);
            memcpy(&vertex_input->v,&projected_uv.v_bits,4u);
        }else{
            vertex_input->u=read_f32(memory,packet->attributes+index*8u);
            vertex_input->v=read_f32(memory,packet->attributes+index*8u+4u);
        }
    }
}
static uint32_t read_u32(GameMemory *memory, uint32_t address) {
    return game_memory_read_u32(memory, address);
}

static MaterialValue world_material(GameMemory *memory,const WorldRendererOps *ops,uint32_t node,uint32_t reference){
    TerrainAsset *world=memory->terrain;
    uint32_t offset=reference-0xfa090000u;MaterialValue value;
    if(scene_projection_storage_material(node,&value))return value;
    if(world && offset%8u==0 && offset/8u<world->material_count)return world->materials[offset/8u];
    if(ops->material && ops->material(ops->context,reference,&value))return value;
    value.width=game_memory_read_u16(memory,reference);value.height=game_memory_read_u16(memory,reference+2);
    value.backend_handle=game_memory_read_u32(memory,reference+4);return value;
}

static float bits_float(uint32_t bits) {
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static float read_f32(GameMemory *memory, uint32_t address) {
    return bits_float(game_memory_read_u32(memory, address));
}

static int restore_base_palette(const WorldRenderFrame *frame,
    const WorldRendererOps *ops){
    unsigned i;for(i=0;i<4;i++){unsigned entry=frame->base_palette_entries[i];
        if(!ops->palette(ops->context,frame->palette,entry,entry,0))return 0;
    }return 1;
}
int world_renderer_submit(GameMemory *memory,GameAddress node,
    WorldRenderFrame *frame,const WorldRendererOps *ops) {
    uint32_t scan = node;
    WorldRenderCommand *queue=NULL;
    size_t queue_count=0,queue_capacity=0,command_index;
    int result=0;

    if (!frame || !memory || memory->fault) return 0;
    if ((float)frame->depth_limit == frame->near_offset ||
        frame->depth_scale == 0.0f) {
        result=-1;goto finish;
    }
    /* Validate the command list before changing renderer state. Unsupported
       frames leave the device untouched. */
    while (scan) {
        WorldRenderCommand *command;
        RenderArenaPacket packet;
        uint32_t scan_flags,scan_next;
        if(queue_count>=65536u){result=-1;goto finish;}
        if(queue_count==queue_capacity){
            size_t capacity=queue_capacity?queue_capacity*2u:64u;
            WorldRenderCommand *grown=realloc(queue,capacity*sizeof(*queue));
            if(!grown)goto finish;
            queue=grown;queue_capacity=capacity;
        }
        command=&queue[queue_count++];
        memset(command,0,sizeof(*command));
        world_packet_capture(memory,scan,&packet);
        if(memory->fault)goto finish;
        scan_flags=packet.flags;
        scan_next=packet.next;
        command->flags=scan_flags;
        command->count=packet.count;
        command->palette_key=packet.extra[2];
        if ((scan_flags & 0x1000u) &&
            packet.material > 255u){result=-1;goto finish;}
        if (scan_flags & 0x1000u) {
            if(!gameplay_geometry_capture_point_list(memory,scan,
                &command->points))goto finish;
            ++world_stats.point_lists;
            world_stats.point_inputs+=command->points.count;
        } else {
            uint32_t scan_count=packet.count;
            uint32_t scan_header=packet.material;
            uint32_t scan_texture;
            if (!scan_count || scan_count > 32u || !scan_header) {
                result=-1;goto finish;
            }
            command->material=world_material(memory,ops,scan,scan_header);
            scan_texture=command->material.backend_handle;
            if (!scan_texture || !(int16_t)command->material.width ||
                !(int16_t)command->material.height) {
                if(!memory->fault)result=-1;
                goto finish;
            }
            if(scan_flags&0x200u){
                unsigned palette_index;
                uint32_t palette=command->palette_key;
                for(palette_index=0;palette_index<4u && !memory->fault;
                    palette_index++)
                    command->palette_map[palette_index]=game_memory_read_u8(
                        memory,palette+palette_index);
                if(memory->fault)goto finish;
            }
            world_packet_capture_vertices(memory,&packet,command->input);
        }
        if (memory->fault) goto finish;
        scan = scan_next;
    }
    ++world_stats.captured_queues;
    result=world_renderer_draw_commands(queue,queue_count,frame,ops);
finish:
    if(result<0)++world_stats.generated_fallbacks;
    for(command_index=0;command_index<queue_count;command_index++)
        gameplay_geometry_release_point_list(&queue[command_index].points);
    free(queue);
    return result;
}

/* Capture the whole owned queue before the backend begins. Mixed packets can
 * then use the compatibility route without replaying partial draw state. */
static uint32_t world_packet_next(uint32_t node,RenderArenaPacket *packet,
    RenderArenaPacket **next_owned){
    uint32_t next;
    if(render_packet_link(node,&next,next_owned))return next;
    *next_owned=NULL;
    return packet->next;
}
int world_renderer_submit_owned(GameAddress head,struct TerrainAsset *terrain,
    WorldRenderFrame *frame,const WorldRendererOps *ops){
    WorldRenderCommand *queue=NULL;
    WorldRendererStats captured={0};
    uint32_t node=head,guard=0;
    RenderArenaPacket *owned_packet=NULL;
    size_t count=0,capacity=0;
    int result=-1;
    ++world_stats.owned_attempts;
    if(!frame || !ops ||
       (float)frame->depth_limit==frame->near_offset ||
       frame->depth_scale==0.0f){++world_stats.owned_rejects;return -1;}
    while(node){
        RenderArenaPacket *packet=owned_packet?owned_packet:render_packet_view(node);
        WorldRenderCommand *command;
        uint32_t *arena_indices;
        RenderArenaUv *arena_uv;
        uint32_t index,reference;
        ++world_stats.owned_scanned;
        if(++guard>65536u || !packet){world_stats.owned_reject_kind=1;goto finish;}
        if(count==capacity){
            size_t next=capacity?capacity*2u:64u;
            WorldRenderCommand *grown=realloc(queue,next*sizeof(*queue));
            if(!grown){result=0;goto finish;}
            queue=grown;capacity=next;
        }
        command=&queue[count++];memset(command,0,sizeof(*command));
        command->count=packet->count;command->flags=packet->flags;
        command->palette_key=packet->extra[2];
        if(packet->flags&0x1000u){
            if(!world_owned_point_list(packet,&command->points)){
                world_stats.owned_reject_kind=2;goto finish;}
            ++captured.point_lists;
            captured.point_inputs+=packet->count;
            ++captured.native_packets;
            node=world_packet_next(node,packet,&owned_packet);
            continue;
        }
        if(!packet->count || packet->count>32u || !packet->material){
            world_stats.owned_reject_kind=3;goto finish;}
        if(!scene_projection_storage_material(node,&command->material)){
            uint32_t offset=packet->material-0xfa090000u;
            if(terrain && offset%8u==0u &&
               offset/8u<terrain->material_count)
                command->material=terrain->materials[offset/8u];
            else if(!ops->material || !ops->material(ops->context,packet->material,
                &command->material)){world_stats.owned_reject_kind=4;goto finish;}
        }
        if(!command->material.backend_handle ||
           !(int16_t)command->material.width ||
           !(int16_t)command->material.height){
            world_stats.owned_reject_kind=5;goto finish;}
        if(!ops->texture_owned ||
           !ops->texture_owned(ops->context,
               command->material.backend_handle)){
            world_stats.owned_reject_kind=10;goto finish;}
        if((packet->flags&0x200u) &&
           !render_arena_palette_lookup(packet->extra[2],command->palette_map) &&
           !world_owned_read(packet->extra[2],command->palette_map,4u) &&
           (!ops->palette_map || !ops->palette_map(ops->context,
                packet->extra[2],command->palette_map))){
            world_stats.owned_reject_kind=6;
            world_stats.owned_reject_address=packet->extra[2];goto finish;}
        arena_indices=render_arena_indices(packet->vertices,packet->count);
        arena_uv=render_arena_uv(packet->attributes,packet->count);
        for(index=0;index<packet->count;index++){
            RenderArenaVertex *vertex;
            AssetMeshUV scene_uv;
            GameWorldVertexInput *input=&command->input[index];
            if(arena_indices)reference=arena_indices[index];
            else{
                const uint32_t *link=scene_projection_storage_links(
                    packet->vertices+index*4u,1u);
                if(!link){world_stats.owned_reject_kind=7;goto finish;}
                reference=*link;++captured.scene_indices;
            }
            vertex=render_arena_vertices(reference,1u);
            if(vertex)++captured.native_vertices;
            else {vertex=scene_projection_storage_transformed(
                reference,1u);
                if(!vertex){world_stats.owned_reject_kind=8;goto finish;}
                ++captured.scene_vertices;
            }
            input->x=vertex->position[0];input->y=vertex->position[1];
            input->depth=vertex->position[2];input->light=vertex->light;
            if(arena_uv){input->u=arena_uv[index].u;
                input->v=arena_uv[index].v;++captured.native_uv;}
            else if(scene_projection_storage_read_uv(
                packet->attributes+index*8u,&scene_uv)){
                memcpy(&input->u,&scene_uv.u_bits,4u);
                memcpy(&input->v,&scene_uv.v_bits,4u);
                ++captured.scene_uv;
            }else {world_stats.owned_reject_kind=9;goto finish;}
        }
        ++captured.native_packets;
        node=world_packet_next(node,packet,&owned_packet);
    }
    world_stats.captured_queues++;
    world_stats.native_packets+=captured.native_packets;
    world_stats.native_vertices+=captured.native_vertices;
    world_stats.scene_indices+=captured.scene_indices;
    world_stats.scene_vertices+=captured.scene_vertices;
    world_stats.native_uv+=captured.native_uv;
    world_stats.scene_uv+=captured.scene_uv;
    world_stats.point_lists+=captured.point_lists;
    world_stats.point_inputs+=captured.point_inputs;
    result=world_renderer_draw_commands(queue,count,frame,ops);
finish:
    if(result<0)++world_stats.owned_rejects;
    else if(result==1)++world_stats.owned_submissions;
    for(size_t i=0;i<count;i++)
        gameplay_geometry_release_point_list(&queue[i].points);
    free(queue);
    return result;
}

/* Rendering consumes only captured command state and platform callbacks. */
int world_renderer_draw_commands(const WorldRenderCommand *commands,
    size_t command_count,WorldRenderFrame *frame,const WorldRendererOps *ops){
    float depth_scale,near_offset,fog_scale,fog_max,projection_magic,depth_limit;
    float fog_start,fog_factor;
    uint32_t active_palette=0u;
    size_t command_index;
    int palette_changed=0,result=0;
    if(!frame || !ops || (command_count && !commands))return 0;
    depth_scale=frame->depth_scale;near_offset=frame->near_offset;
    fog_scale=frame->fog_scale;fog_max=frame->fog_max;
    projection_magic=frame->projection_magic;
    depth_limit=(float)frame->depth_limit;
    if(depth_limit==near_offset || depth_scale==0.0f)return 0;
    fog_start = depth_scale / (depth_limit - near_offset);
    fog_factor = fog_scale /
        (depth_scale / (depth_limit + near_offset) - fog_start);

    if (!ops->begin(ops->context)) goto finish;

    for(command_index=0;command_index<command_count;
        command_index++) {
        const WorldRenderCommand *command=&commands[command_index];
        uint32_t count=command->count;
        uint32_t flags=command->flags;
        uint32_t texture;
        uint32_t texture_flags;
        int16_t width;
        int16_t height;
        GameRenderVertex native_vertices[32];
        unsigned index;
        ++world_stats.drawn_packets;

        if (flags & 0x1000u) {
            if (palette_changed) {
                if (!restore_base_palette(frame,ops)) goto finish;
                palette_changed = 0;
                active_palette = 0u;
            }
            {GameplayGeometryPointOps point_ops={ops->context,ops->point};
            if(!gameplay_geometry_draw_captured_points(&command->points,
                &frame->points,&point_ops))goto finish;}
            continue;
        }

        /* Retail reserves exactly 32 60-byte vertices in this frame. A bad
           packet should retain the translated routine's diagnostics rather
           than overflow native stack storage. */
        if (!count || count > 32u || !command->material.backend_handle) goto finish;
        MaterialValue material=command->material;
        width=(int16_t)material.width;height=(int16_t)material.height;texture=material.backend_handle;
        WorldTextureProperties texture_view;
        if(!width || !height || !ops->texture(ops->context,texture,frame->texture_frame,&texture_view))goto finish;
        texture_flags=texture_view.flags;

        if (flags & 0x400u)
            if (!ops->state(ops->context,WORLD_ALPHA_FADE, 1)) goto finish;

        if (!(flags & 0x200u)) {
            if (palette_changed) {
                if (!restore_base_palette(frame,ops)) goto finish;
                palette_changed = 0;
                active_palette = 0u;
            }
        } else {
            uint32_t palette=command->palette_key;
            if (!palette_changed || palette != active_palette) {
                for (index = 0; index < 4u; index++) {
                    uint32_t base_entry = frame->base_palette_entries[index];
                    uint32_t mapped_entry = command->palette_map[index];
                    uint32_t colors[256]={0};
                    colors[base_entry]=frame->palette[mapped_entry];
                    if(!ops->palette(ops->context,colors,base_entry,base_entry,0))goto finish;
                }
                palette_changed = 1;
                active_palette = palette;
            }
        }

        if (texture_flags & 1u)
            if (!ops->state(ops->context,WORLD_DEPTH_BIAS, -512)) goto finish;
        if (texture_flags & 2u)
            if (!ops->state(ops->context,WORLD_TEXTURE_FILTER, 0)) goto finish;
        if (texture_flags & 0x40000000u) {
            if(!ops->ncc || !ops->ncc(ops->context,texture_view.ncc))goto finish;
            frame->palette_bytes+=0x70u;
        }
        if (!(texture_flags & 0x10u)) {
            if (flags & 0x100u) {
                if (!ops->state(ops->context,WORLD_ALPHA_FADE, 0) ||
                    !ops->state(ops->context,WORLD_ALPHA_TEST, 2u))
                    goto finish;
            }
        } else {
            if (!ops->state(ops->context,WORLD_DEPTH_WRITE, 0)) goto finish;
        }

        {
            GameWorldPolygonParams params={flags,width,height,fog_scale,fog_max,fog_start,fog_factor,projection_magic};
            if(!game_screen_build_world_polygon(native_vertices,
                command->input,count,&params))goto finish;
            if(!ops->polygon(ops->context,native_vertices,count))goto finish;
        }

        if (!(texture_flags & 0x10u) && (flags & 0x100u))
            if (!ops->state(ops->context,WORLD_ALPHA_TEST, 7u)) goto finish;
        if (flags & 0x400u) {
            if (!ops->state(ops->context,WORLD_ALPHA_FADE, 0) ||
                !ops->state(ops->context,WORLD_DEPTH_WRITE, 1))
                goto finish;
        }
        if (texture_flags & 1u)
            if (!ops->state(ops->context,WORLD_DEPTH_BIAS, 0)) goto finish;
        if (texture_flags & 2u)
            if (!ops->state(ops->context,WORLD_TEXTURE_FILTER, 1)) goto finish;
        if (texture_flags & 0x10u)
            if (!ops->state(ops->context,WORLD_DEPTH_WRITE, 0)) goto finish;
        frame->triangles+=count-2u;
    }

    if (palette_changed && !restore_base_palette(frame,ops)) goto finish;

    result=1;
finish:
    return result;
}

/* Capture one packet, leaving queue links live for callback-driven relinking. */
static int world_projected_capture(GameMemory *memory,GameAddress node,
    ProjectedQueueKind kind,ProjectedRenderCommand *command){
    RenderArenaPacket *packet=render_packet_view(node);
    uint32_t count=packet?packet->count:read_u32(memory,node);
    uint32_t indices=packet?packet->vertices:read_u32(memory,node+4u);
    uint32_t attributes=packet?packet->attributes:read_u32(memory,node+16u);
    uint32_t *native_indices;const uint32_t *scene_indices;
    RenderArenaUv *uv;const RenderArenaUv *scene_uv;unsigned i;
    if(memory->fault || count<3u || count>32u)return 0;
    command->count=count;
    native_indices=render_arena_indices(indices,count);
    scene_indices=native_indices?NULL:scene_projection_storage_links(indices,count);
    uv=render_arena_uv(attributes,count);
    scene_uv=uv?NULL:scene_projection_storage_work_uv(attributes,count);
    for(i=0;i<count && !memory->fault;i++){
        uint32_t ref;
        RenderArenaVertex projected_vertex,*vertex;
        AssetMeshUV projected_uv;
        GameWorldVertexInput *input=&command->input[i];
        int projection_uv_read=0;
        if(native_indices)ref=native_indices[i];
        else if(scene_indices)ref=scene_indices[i];
        else if(!scene_projection_storage_read(indices+i*4u,&ref,4u))
            ref=read_u32(memory,indices+i*4u);
        vertex=render_arena_vertices(ref,1u);
        if(vertex)++world_stats.projected_arena_vertices;
        else if((vertex=scene_projection_storage_transformed(ref,1u)))
            ++world_stats.projected_scene_vertices;
        else if(scene_projection_storage_read(ref,&projected_vertex,
            sizeof(projected_vertex))){vertex=&projected_vertex;
            ++world_stats.projected_scene_vertices;}
        else ++world_stats.projected_guest_vertices;
        input->x=vertex?vertex->position[0]:read_f32(memory,ref);
        input->y=vertex?vertex->position[1]:read_f32(memory,ref+4u);
        input->depth=vertex?vertex->position[2]:read_f32(memory,ref+8u);
        if(kind==PROJECTED_PLAIN)continue;
        if(!uv && !scene_uv && scene_projection_storage_read_uv(attributes+i*8u,
            &projected_uv))projection_uv_read=1;
        if(uv){input->u=uv[i].u;input->v=uv[i].v;}
        else if(scene_uv){input->u=scene_uv[i].u;input->v=scene_uv[i].v;
            ++world_stats.projected_scene_uv;}
        else if(projection_uv_read){
            memcpy(&input->u,&projected_uv.u_bits,4u);
            memcpy(&input->v,&projected_uv.v_bits,4u);
            ++world_stats.projected_scene_uv;
        }else{
            input->u=read_f32(memory,attributes+i*8u);
            input->v=read_f32(memory,attributes+i*8u+4u);
            ++world_stats.projected_guest_uv;
        }
    }
    return !memory->fault;
}

int world_renderer_draw_projected_command(const ProjectedRenderCommand *command,
    ProjectedQueueKind kind,float magic,float alpha_scale,
    const ProjectedRendererOps *ops){
    GameRenderVertex output[32]={{0}};unsigned i;
    if(!command || !ops || !ops->polygon || command->count<3u ||
       command->count>32u)return 0;
    for(i=0;i<command->count;i++){
        const GameWorldVertexInput *input=&command->input[i];
        volatile float rounded=input->x+magic;
        rounded=rounded-magic;output[i].x=rounded;
        rounded=input->y+magic;rounded=rounded-magic;output[i].y=rounded;
        output[i].a=128.0f;output[i].oow=input->depth;
        if(kind==PROJECTED_TEXTURED){
            output[i].sow0=input->u*input->depth;
            output[i].tow0=input->v*input->depth;
        }else if(kind!=PROJECTED_PLAIN){
            volatile float sum=input->u+input->v;
            output[i].a=sum*alpha_scale;
        }
    }
    return ops->polygon(ops->context,output,command->count);
}

/* Owned arena/scene packets can be submitted without a guest-memory bridge.
 * Preflight the initial chain before the backend begins; callbacks may still
 * relink it, so capture each command again immediately before drawing. */
static int world_projected_capture_native(const RenderArenaPacket *packet,
    ProjectedQueueKind kind,ProjectedRenderCommand *command,int count_stats){
    uint32_t *arena_indices;const uint32_t *scene_indices;
    RenderArenaUv *arena_uv;const RenderArenaUv *scene_uv;unsigned i;
    if(!packet || packet->count<3u || packet->count>32u)return 0;
    command->count=packet->count;
    arena_indices=render_arena_indices(packet->vertices,packet->count);
    scene_indices=arena_indices?NULL:scene_projection_storage_links(
        packet->vertices,packet->count);
    if(!arena_indices && !scene_indices)return 0;
    arena_uv=kind==PROJECTED_PLAIN?NULL:
        render_arena_uv(packet->attributes,packet->count);
    scene_uv=kind==PROJECTED_PLAIN || arena_uv?NULL:
        scene_projection_storage_work_uv(packet->attributes,packet->count);
    if(kind!=PROJECTED_PLAIN && !arena_uv && !scene_uv)return 0;
    for(i=0;i<packet->count;i++){
        uint32_t ref=arena_indices?arena_indices[i]:scene_indices[i];
        RenderArenaVertex *vertex;
        GameWorldVertexInput *input=&command->input[i];
        int owned_vertex=0;
        vertex=render_arena_vertices(ref,1u);
        if(vertex)owned_vertex=1;
        else if((vertex=scene_projection_storage_transformed(ref,1u)))
            owned_vertex=2;
        else return 0;
        input->x=vertex->position[0];input->y=vertex->position[1];
        input->depth=vertex->position[2];
        if(kind!=PROJECTED_PLAIN){
            if(arena_uv){input->u=arena_uv[i].u;input->v=arena_uv[i].v;
            }else {input->u=scene_uv[i].u;input->v=scene_uv[i].v;}
        }
        if(count_stats){
            if(owned_vertex==1)++world_stats.projected_arena_vertices;
            else ++world_stats.projected_scene_vertices;
            if(scene_uv)++world_stats.projected_scene_uv;
        }
    }
    return 1;
}

int world_renderer_projected_native(GameAddress head,ProjectedQueueKind kind,
    float magic,float alpha_scale,const ProjectedRendererOps *ops){
    uint32_t node=head,count=0u;
    RenderArenaPacket *owned_packet=NULL;
    ProjectedRenderCommand command={0};
    if(!ops || !ops->begin || !ops->polygon || !ops->end)return 0;
    while(node){
        RenderArenaPacket *packet=owned_packet?owned_packet:render_packet_view(node);
        if(++count>65536u)return 0;
        if(!world_projected_capture_native(packet,kind,&command,0))return -1;
        node=world_packet_next(node,packet,&owned_packet);
    }
    if(!ops->begin(ops->context,kind))return 0;
    node=head;count=0u;owned_packet=NULL;
    while(node){
        RenderArenaPacket *packet=owned_packet?owned_packet:render_packet_view(node);
        if(++count>65536u ||
           !world_projected_capture_native(packet,kind,&command,1))return 0;
        ++world_stats.projected_packets;
        if(!world_renderer_draw_projected_command(&command,kind,magic,
            alpha_scale,ops))return 0;
        node=world_packet_next(node,packet,&owned_packet);
    }
    return ops->end(ops->context,kind);
}

int world_renderer_projected(GameMemory *memory,GameAddress node,
    ProjectedQueueKind kind,float magic,float alpha_scale,
    const ProjectedRendererOps *ops){
    if(!memory || !ops || !ops->begin || !ops->end ||
       !ops->begin(ops->context,kind))return 0;
    while(node && !memory->fault){
        ProjectedRenderCommand command={0};
        RenderArenaPacket *packet;
        if(!world_projected_capture(memory,node,kind,&command))return 0;
        ++world_stats.projected_packets;
        if(!world_renderer_draw_projected_command(&command,kind,magic,
            alpha_scale,ops))return 0;
        /* Read after submission: callbacks may relink the queue. */
        packet=render_packet_view(node);
        if(packet){RenderArenaPacket *next_owned=NULL;
            node=world_packet_next(node,packet,&next_owned);}
        else node=read_u32(memory,node+12u);
    }
    return !memory->fault && ops->end(ops->context,kind);
}
