#include "render_arena.h"
#include "render_packet_view.h"
#include "gameplay_clipper.h"
#include "game_matrix.h"
#include "terrain_asset.h"
#include "gameplay_geometry.h"
#include "presentation_instance.h"
#include "gameplay_spatial.h"

#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/*The actual entity position methods share owned pose records. Stable IDs
 * select these ordinary queries, never a register/stack callback. Linked
 * descriptor families borrow their parent's position;53d6b0 really returns0s. */
int gameplay_geometry_entity_position(GameMemory *memory,GameAddress entity,float output[3]){
    uint32_t method;unsigned i;
    if(!memory || !output || memory->fault)return 0;
    method=game_memory_read_u32(memory,game_memory_read_u32(memory,entity)+0x38u);
    if(memory->fault)return 0;
    switch(method){
    case 0x42eb60u:case 0x5547b0u:break;
    case 0x53dde0u:case 0x53ec80u:case 0x53f740u:
        entity=game_memory_read_u32(memory,entity+0x48u);break;
    case 0x53d6b0u:output[0]=output[1]=output[2]=0.0f;return 1;
    case 0x406fa0u:{
        float point[3],matrix[12];long double x,y,z;
        /*Class28 targets its mounted attachment, not the body origin. Both
         *matrices remain with the existing vehicle/presentation owner. Keep
         *406fa0's different sum order for the final row and binary32 outputs. */
        for(i=0;i<3u;++i){uint32_t bits=game_memory_read_u32(memory,entity+0x18cu+i*16u);memcpy(point+i,&bits,4u);}
        for(i=0;i<12u;++i){uint32_t bits=game_memory_read_u32(memory,entity+0x80u+i*4u);memcpy(matrix+i,&bits,4u);}
        if(memory->fault)return 0;
        x=(long double)point[1]*matrix[1]+(long double)point[0]*matrix[0];
        x+=(long double)point[2]*matrix[2];
        y=(long double)point[0]*matrix[4]+(long double)point[1]*matrix[5];
        y+=(long double)point[2]*matrix[6];
        z=(long double)point[1]*matrix[9]+(long double)point[2]*matrix[10];
        z+=(long double)point[0]*matrix[8];
        output[0]=(float)(x+matrix[3]);output[1]=(float)(y+matrix[7]);output[2]=(float)(z+matrix[11]);
        return 1;
    }
    default:memory->fault=1;return 0;
    }
    for(i=0;i<3u;++i){uint32_t bits=game_memory_read_u32(memory,entity+0x18u+i*4u);memcpy(output+i,&bits,4u);}
    return !memory->fault;
}

static RenderArenaPacket *geometry_packet_view(GameMemory *memory,
    GameAddress address){
    RenderArenaPacket *packet=render_packet_view(address);
    if(packet)return packet;
    packet=scene_surface_packet(memory->surface,address);
    return packet?packet:memory->packet_view?
        memory->packet_view(memory->fallback_context,address):NULL;
}
static GameAddress geometry_packet_next(GameMemory *memory,GameAddress address,
    RenderArenaPacket *packet){
    RenderArenaPacket *next_owned=NULL;
    GameAddress next;
    if(packet && render_packet_link(address,&next,&next_owned))return next;
    return packet?packet->next:game_memory_read_u32(memory,address+12u);
}
static GameAddress geometry_index_value(GameMemory *memory,
    GameAddress address){
    uint32_t *arena=render_arena_indices(address,1u);
    const uint32_t *owned=arena?arena:scene_projection_storage_links(address,1u);
    uint32_t value;
    if(!owned)owned=scene_surface_link(memory->surface,address);
    if(owned)return *owned;
    if(memory->surface && render_arena_read(address,&value,sizeof(value)))
        return value;
    if(memory->index_view){
        uint32_t *native=memory->index_view(memory->fallback_context,address);
        if(native)return *native;
    }
    return game_memory_read_u32(memory,address);
}
static RenderArenaVertex *geometry_vertex_value(GameMemory *memory,
    GameAddress address){
    RenderArenaVertex *vertex=render_arena_vertices(address,1u);
    if(!vertex)vertex=scene_projection_storage_transformed(address,1u);
    if(!vertex)vertex=scene_surface_transformed(memory->surface,address,1u);
    return vertex;
}

static float geometry_read_f32(GameMemory *memory, GameAddress address) {
    float value;
    float *component=scene_surface_transformed_component(memory->surface,address);
    uint32_t bits;
    if(component)return *component;
    if(scene_surface_uv_component(memory->surface,address,&value))return value;
    if(memory->float_view){
        float *native=memory->float_view(memory->fallback_context,address);
        if(native)return *native;
    }
    if(!memory->surface || !render_arena_read(address,&bits,sizeof(bits)))
        bits=game_memory_read_u32(memory,address);
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static void geometry_write_f32(GameMemory *memory, GameAddress address,
    float value) {
    uint32_t bits;
    if(scene_surface_set_uv_component(memory->surface,address,value))return;
    if(memory->float_view){
        float *native=memory->float_view(memory->fallback_context,address);
        if(native){*native=value;
            if(memory->note_typed_write)
                memory->note_typed_write(memory->fallback_context,address);
            return;}
    }
    memcpy(&bits, &value, sizeof(bits));
    if(!memory->surface || !render_arena_write(address,&bits,sizeof(bits)))
        game_memory_write_u32(memory, address, bits);
}

static uint32_t geometry_float_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static int32_t geometry_grid_integer(float value) {
    int32_t truncated = (int32_t)value;
    return geometry_float_bits(value) <= 0x80000000u ?
        truncated : truncated - 1;
}

static float geometry_terrain_height_at(GameMemory *memory,
    GameAddress table, uint32_t x, uint32_t z) {
    if (memory->terrain && memory->terrain->cells)
        return (float)memory->terrain->cells[(z & 255u) * 256u + (x & 255u)].height;
    return (float)(int16_t)game_memory_read_u16(memory,
        table + ((z & 0xffu) * 0x800u) + ((x & 0xffu) * 8u));
}

static float geometry_detail_height_at(GameMemory *memory, uint32_t grid_x,
    uint32_t grid_z) {
    GameAddress tile = game_memory_read_u32(memory, 0x00625660u +
        ((grid_x >> 4u) * 0x80u) + ((grid_z >> 4u) * 4u));
    uint8_t packed;
    if (memory->fault || tile == 0u) return 0.0f;
    packed = game_memory_read_u8(memory, tile + (grid_x & 0x0fu) * 8u +
        ((grid_z & 0x0fu) >> 1u));
    if (memory->fault) return 0.0f;
    return (float)(((grid_z & 1u) != 0u) ? packed >> 4u : packed & 0x0fu);
}

static float geometry_detail_distance(float local, int32_t grid,
    int32_t *neighbor) {
    if ((int32_t)geometry_float_bits(local) <= (int32_t)0x43000000u) {
        *neighbor = grid - 1;
        return 128.0f - local;
    }
    *neighbor = grid + 1;
    return local - 128.0f;
}

static float geometry_subtract(float left, float right) {
    volatile float result = left - right;
    return result;
}

static float geometry_multiply(float left, float right) {
    volatile float result = left * right;
    return result;
}

static float geometry_add(float left, float right) {
    volatile float result = left + right;
    return result;
}

static float geometry_divide(float left, float right) {
    volatile float result = left / right;
    return result;
}

static float geometry_transform_float_component(float x, float y, float z,
        float mx, float my, float mz, float offset) {
    float result = geometry_add(geometry_multiply(x, mx),
        geometry_multiply(y, my));
    result = geometry_add(result, geometry_multiply(z, mz));
    return geometry_add(result, offset);
}

static float geometry_transform_extended_component(float x, float x_scale,
        float y, float y_scale, float z, float z_scale,
        float translation) {
    long double result =
        ((long double)x * (long double)x_scale +
        (long double)y * (long double)y_scale) +
        (long double)z * (long double)z_scale;
    return (float)(result + (long double)translation);
}

GameplayGeometryTransformResult gameplay_geometry_transform_vertex_stream(
        GameMemory *memory, GameAddress matrix, GameAddress begin,
        GameAddress end, GameAddress output, uint32_t initial_eax) {
    GameplayGeometryTransformResult result = {initial_eax, 0u, output, 0};
    float m[12];
    GameAddress cursor;
    size_t count=end>=begin?(end-begin)/16u:0;
    RenderArenaVertex *input=render_arena_vertices(begin,count),*destination=render_arena_vertices(output,count);
    unsigned index;
    for (index = 0u; index < 12u; ++index)
        m[index] = geometry_read_f32(memory, matrix + index * 4u);
    if (memory->fault) return result;
    for (cursor = begin; cursor != end; cursor += 16u, result.output += 16u) {
        float x = input?input[(cursor-begin)/16].position[0]:geometry_read_f32(memory, cursor + 0u);
        float y = input?input[(cursor-begin)/16].position[1]:geometry_read_f32(memory, cursor + 4u);
        float z = input?input[(cursor-begin)/16].position[2]:geometry_read_f32(memory, cursor + 8u);
        float out_x, out_y, out_z;
        if (memory->fault) return result;
        out_x = geometry_transform_float_component(x, y, z,
            m[0], m[1], m[2], m[3]);
        out_y = geometry_transform_float_component(x, y, z,
            m[4], m[5], m[6], m[7]);
        out_z = geometry_transform_float_component(x, y, z,
            m[8], m[9], m[10], m[11]);
        if(destination){RenderArenaVertex *vertex=&destination[(cursor-begin)/16];
            vertex->position[0]=out_x;vertex->position[1]=out_y;vertex->position[2]=out_z;
        }else {
        geometry_write_f32(memory, result.output + 0u, out_x);
        geometry_write_f32(memory, result.output + 4u, out_y);
        geometry_write_f32(memory, result.output + 8u, out_z);
        }
        result.eax = geometry_float_bits(out_y);
        result.processed = 1;
        if (memory->fault) return result;
    }
    return result;
}

static GameplayGeometryTransformResult geometry_transform_packed_values(
        GameMemory *memory, const float m[12], GameAddress begin,
        GameAddress end, GameAddress output, uint32_t initial_eax,
        uint32_t initial_ebx,const AssetMeshVertex *native_vertices,
        SceneSurface *surface) {
    GameplayGeometryTransformResult result = {
        initial_eax, initial_ebx, output, 0};
    GameAddress source = begin;
    RenderArenaVertex *owned_output=end>=begin && !((end-begin)%12u)?
        scene_projection_storage_transformed(output,(end-begin)/12u):NULL;
    if(!owned_output && surface && end>=begin && !((end-begin)%12u))
        owned_output=scene_surface_transformed(surface,output,(end-begin)/12u);
    if (source == end) return result;
    while (source != end) {
        float x,y,z;
        if(native_vertices){
            const AssetMeshVertex *v=&native_vertices[(source-begin)/12u];
            memcpy(&x,&v->x_bits,4);memcpy(&y,&v->y_bits,4);memcpy(&z,&v->z_bits,4);
        }else{
            x=geometry_read_f32(memory, source + 0u);
            y=geometry_read_f32(memory, source + 4u);
            z=geometry_read_f32(memory, source + 8u);
        }
        float out_x, out_y, out_z;
        if (memory->fault) return result;
        out_y = geometry_transform_extended_component(x, m[4], y, m[5],
            z, m[6], m[7]);
        out_z = geometry_transform_extended_component(x, m[8], y, m[9],
            z, m[10], m[11]);
        out_x = geometry_transform_extended_component(x, m[0], y, m[1],
            z, m[2], m[3]);
        if(owned_output){
            RenderArenaVertex *vertex=&owned_output[(source-begin)/12u];
            vertex->position[0]=out_x;
            vertex->position[1]=out_y;
            vertex->position[2]=out_z;
        }else{
            geometry_write_f32(memory, result.output + 0u, out_x);
            geometry_write_f32(memory, result.output + 4u, out_y);
            geometry_write_f32(memory, result.output + 8u, out_z);
        }
        result.eax = geometry_float_bits(out_y);
        result.ebx = geometry_float_bits(out_z);
        result.processed = 1;
        source += 12u;
        result.output += 16u;
        if (memory->fault) return result;
    }
    return result;
}

GameplayGeometryTransformResult gameplay_geometry_transform_packed_stream(GameMemory *memory,
    GameAddress matrix,GameAddress begin,GameAddress end,GameAddress output,uint32_t eax,uint32_t ebx) {
    float m[12]={0};unsigned i;
    if(begin!=end)for(i=0;i<12;i++)m[i]=geometry_read_f32(memory,matrix+i*4);
    return geometry_transform_packed_values(memory,m,begin,end,output,eax,ebx,NULL,NULL);
}

static long double geometry_transform_row_zyx(float z, float mz, float y,
        float my, float x, float mx, float translation) {
    volatile long double value = (long double)z * (long double)mz;
    value += (long double)y * (long double)my;
    value += (long double)x * (long double)mx;
    value += (long double)translation;
    return value;
}

static long double geometry_transform_row_zxy(float z, float mz, float x,
        float mx, float y, float my, float translation) {
    volatile long double value = (long double)z * (long double)mz;
    value += (long double)x * (long double)mx;
    value += (long double)y * (long double)my;
    value += (long double)translation;
    return value;
}

GameplayGeometryObjectTransformResult gameplay_geometry_transform_object_points(
        GameMemory *memory, GameAddress object, uint32_t initial_edx) {
    GameplayGeometryObjectTransformResult result = {0u, initial_edx, 0, 0};
    float matrix[12];
    int32_t count;
    unsigned index;
    result.flags = game_memory_read_u32(memory, object + 0x10u);
    if (memory->fault || (result.flags & 0x00800000u) != 0u)
        return result;
    count = (int16_t)game_memory_read_u16(memory, object + 0x44u);
    for (index = 0u; index < 12u; ++index)
        matrix[index] = geometry_read_f32(memory,
            object + 0x80u + index * 4u);
    if (memory->fault) return result;
    if (count > 0) {
        GameAddress source = game_memory_read_u32(memory, object + 0x3cu);
        GameAddress destination = game_memory_read_u32(memory, object + 0x40u);
        int32_t point;
        if (memory->fault) return result;
        for (point = 0; point < count; ++point) {
            GameAddress input = source + (uint32_t)point * 28u + 12u;
            GameAddress output = destination + (uint32_t)point * 12u;
            float x = geometry_read_f32(memory, input + 0u);
            float y = geometry_read_f32(memory, input + 4u);
            float z = geometry_read_f32(memory, input + 8u);
            if (memory->fault) return result;
            geometry_write_f32(memory, output + 0u, (float)
                geometry_transform_row_zyx(z, matrix[2], y, matrix[1],
                    x, matrix[0], matrix[3]));
            geometry_write_f32(memory, output + 4u, (float)
                geometry_transform_row_zyx(z, matrix[6], y, matrix[5],
                    x, matrix[4], matrix[7]));
            geometry_write_f32(memory, output + 8u, (float)
                geometry_transform_row_zyx(z, matrix[10], y, matrix[9],
                    x, matrix[8], matrix[11]));
            if (memory->fault) return result;
        }
        result.edx = (uint32_t)count * 28u;
    }
    {
        float x = geometry_read_f32(memory, object + 0x190u);
        float y = geometry_read_f32(memory, object + 0x194u);
        float z = geometry_read_f32(memory, object + 0x198u);
        if (memory->fault) return result;
        geometry_write_f32(memory, object + 0x184u, (float)
            geometry_transform_row_zxy(z, matrix[2], x, matrix[0],
                y, matrix[1], matrix[3]));
        geometry_write_f32(memory, object + 0x188u, (float)
            geometry_transform_row_zxy(z, matrix[6], x, matrix[4],
                y, matrix[5], matrix[7]));
        geometry_write_f32(memory, object + 0x18cu, (float)
            geometry_transform_row_zxy(z, matrix[10], x, matrix[8],
                y, matrix[9], matrix[11]));
    }
    result.transformed = !memory->fault;
    result.mark_complete = (result.flags & 0x00080000u) != 0u;
    if (!memory->fault && result.mark_complete)
        game_memory_write_u32(memory, object + 0x10u,
            result.flags | 0x00800000u);
    return result;
}

void gameplay_geometry_terrain_triangles(uint32_t diagonal,uint32_t layout,GameplayTerrainTriangle output[2]) {
    static const float uv[4][4][2]={
        {{31.8f,0.2f},{0.2f,0.2f},{31.8f,31.8f},{0.2f,31.8f}},
        {{0.2f,0.2f},{0.2f,31.8f},{31.8f,0.2f},{31.8f,31.8f}},
        {{0.2f,31.8f},{31.8f,31.8f},{0.2f,0.2f},{31.8f,0.2f}},
        {{31.8f,31.8f},{31.8f,0.2f},{0.2f,31.8f},{0.2f,0.2f}}
    };
    unsigned t,i;uint8_t corners[2][3]={{0,diagonal?3:2,1},{diagonal?0:1,2,3}};
    for(t=0;t<2;t++)for(i=0;i<3;i++) {
        unsigned corner=corners[t][i];output[t].corner[i]=(uint8_t)corner;
        output[t].uv[i][0]=uv[layout&3][corner][0];output[t].uv[i][1]=uv[layout&3][corner][1];
    }
}
static GameplayGeometryPacketResult project_terrain_packets(GameMemory *memory,
    const GameAddress vertices[4],uint32_t material,uint32_t diagonal,uint32_t layout,
    GameplayTerrainFrame *frame) {
    GameplayGeometryPacketResult result={0,0,material};GameplayTerrainTriangle triangles[2];
    GameAddress texcoords;unsigned t,i;
    result.packet=frame?frame->node_cursor:game_memory_read_u32(memory,0x597738u);
    result.positions=frame?frame->clip.packet:game_memory_read_u32(memory,0x597728u);
    texcoords=frame?frame->clip.uv:game_memory_read_u32(memory,0x597730u);
    gameplay_geometry_terrain_triangles(diagonal,layout,triangles);
    {
        RenderArena *arena=render_arena_current();
        uint32_t packet_offset=result.packet-render_arena_view(3),index_offset=result.positions-render_arena_view(1),uv_offset=texcoords-render_arena_view(2);
        if(arena && packet_offset<=600000u-80u && packet_offset%40u==0 &&
            index_offset<=180000u-24u && index_offset%4u==0 && uv_offset<=360000u-48u && uv_offset%8u==0) {
            for(t=0;t<2;t++) {
                RenderArenaPacket *packet=&arena->packets[packet_offset/40u+t];
                packet->count=3;packet->vertices=result.positions+t*12u;packet->flags=0x4000000du;
                if(!t)render_packet_set_next(result.packet,result.packet+40u);
                packet->attributes=texcoords+t*24u;packet->material=material;
                for(i=0;i<3;i++) {
                    arena->indices[index_offset/4u+t*3u+i]=vertices[triangles[t].corner[i]];
                    arena->uv[uv_offset/8u+t*3u+i].u=triangles[t].uv[i][0];
                    arena->uv[uv_offset/8u+t*3u+i].v=triangles[t].uv[i][1];
                }
            }
            result.packet+=80;result.positions+=24;texcoords+=48;
            if(frame){frame->node_cursor=result.packet;frame->clip.packet=result.positions;
                frame->clip.uv=texcoords;}
            else {game_memory_write_u32(memory,0x597738u,result.packet);
                game_memory_write_u32(memory,0x597728u,result.positions);
                game_memory_write_u32(memory,0x597730u,texcoords);}
            return result;
        }
    }

    for(t=0;t<2 && !memory->fault;t++) {
        game_memory_write_u32(memory,result.packet,3);
        game_memory_write_u32(memory,result.packet+4,result.positions);
        game_memory_write_u32(memory,result.packet+8,0x4000000du);
        /* The second link belongs to the caller's painter list. */
        if(!t)game_memory_write_u32(memory,result.packet+12,result.packet+40);
        game_memory_write_u32(memory,result.packet+16,texcoords);
        game_memory_write_u32(memory,result.packet+20,material);
        for(i=0;i<3;i++) {
            game_memory_write_u32(memory,result.positions+i*4,vertices[triangles[t].corner[i]]);
            geometry_write_f32(memory,texcoords+i*8,triangles[t].uv[i][0]);
            geometry_write_f32(memory,texcoords+i*8+4,triangles[t].uv[i][1]);
        }
        result.packet+=40;result.positions+=12;texcoords+=24;
    }
    if(frame){frame->node_cursor=result.packet;frame->clip.packet=result.positions;
        frame->clip.uv=texcoords;}
    else {game_memory_write_u32(memory,0x597738u,result.packet);
        game_memory_write_u32(memory,0x597728u,result.positions);
        game_memory_write_u32(memory,0x597730u,texcoords);}
    return result;
}
GameplayGeometryPacketResult gameplay_geometry_project_terrain_packets(GameMemory *memory,
    const GameAddress vertices[4],uint32_t material,uint32_t diagonal,uint32_t layout) {
    return project_terrain_packets(memory,vertices,material,diagonal,layout,NULL);
}
GameplayGeometryPacketResult gameplay_geometry_build_terrain_packets(GameMemory *memory,
    GameAddress vertices,uint32_t material,uint32_t diagonal,uint32_t layout) {
    GameAddress input[4];unsigned i;
    for(i=0;i<4;i++)input[i]=game_memory_read_u32(memory,vertices+i*4);
    return gameplay_geometry_project_terrain_packets(memory,input,material,diagonal,layout);
}
int16_t gameplay_geometry_wave_height(GameMemory *memory,float x,float z,int alternate) {
    int32_t ix=(int32_t)x,iz=(int32_t)z;
    uint32_t offset=(uint32_t)((int32_t)((uint32_t)ix&0xfffffeffu)>>8)+(uint32_t)(iz>>8);
    offset=(offset+game_memory_read_u32(memory,0x5ac6acu)*2u)&0x7eu;
    return (int16_t)game_memory_read_u16(memory,(alternate?0x5e13c8u:0x5e1348u)+offset);
}

static float geometry_store_extended(long double value) {
    volatile float result = (float)value;
    return result;
}

int gameplay_geometry_capture_point_list(GameMemory *memory,GameAddress list,
    GameplayPointList *output){
    RenderArenaPacket *packet=geometry_packet_view(memory,list);
    int32_t count=packet?(int32_t)packet->count:(int32_t)game_memory_read_u32(memory,list);
    GameAddress positions=packet?packet->vertices:game_memory_read_u32(memory,list+4u);
    GameAddress active=packet?packet->attributes:game_memory_read_u32(memory,list+16u);
    uint32_t palette=packet?packet->material:game_memory_read_u32(memory,list+20u);
    uint32_t index;
    if(!output || memory->fault || count<0 || count>0x100000 || palette>255u)
        return 0;
    memset(output,0,sizeof(*output));
    output->count=(uint32_t)count;output->palette=palette;
    if(count){
        output->items=calloc((size_t)count,sizeof(*output->items));
        if(!output->items)return 0;
    }
    for(index=0;index<(uint32_t)count && !memory->fault;index++){
        GameAddress position=positions+index*12u;
        GameplayPointInput *point=&output->items[index];
        point->active=game_memory_read_u8(memory,active+index);
        if(!point->active || memory->fault)continue;
        point->z=geometry_read_f32(memory,position+8u);
        if((int32_t)geometry_float_bits(point->z)<=(int32_t)0x42c7ffffu ||
           (int32_t)geometry_float_bits(point->z)>=(int32_t)0x466a6001u)
            continue;
        point->x=geometry_read_f32(memory,position);
        point->y=geometry_read_f32(memory,position+4u);
    }
    if(memory->fault){gameplay_geometry_release_point_list(output);return 0;}
    return 1;
}

void gameplay_geometry_release_point_list(GameplayPointList *list){
    if(!list)return;
    free(list->items);memset(list,0,sizeof(*list));
}

int gameplay_geometry_draw_captured_points(const GameplayPointList *list,
    const GameplayPointFrame *frame,const GameplayGeometryPointOps *ops){
    uint32_t color,index;
    if(!list || !frame || !frame->palette || list->palette>255u ||
       !ops || !ops->draw)return 0;
    color=frame->palette[list->palette];
    for(index=0;index<list->count;index++){
        const GameplayPointInput *point=&list->items[index];
        float inverse,x,y;
        int32_t x_bits,y_bits;
        GameRenderVertex vertex;
        if(!point->active)continue;
        if((int32_t)geometry_float_bits(point->z)<=(int32_t)0x42c7ffffu ||
           (int32_t)geometry_float_bits(point->z)>=(int32_t)0x466a6001u)
            continue;
        inverse=geometry_store_extended(1.0L/(long double)point->z);
        x=geometry_store_extended((long double)frame->x_center-
            (long double)point->x*(long double)frame->projection*
            (long double)inverse);
        y=geometry_store_extended((long double)frame->y_center-
            (long double)point->y*(long double)frame->y_scale*
            (long double)frame->projection*(long double)inverse);
        x_bits=(int32_t)geometry_float_bits(x);
        y_bits=(int32_t)geometry_float_bits(y);
        if(x_bits<(int32_t)geometry_float_bits(frame->x_min) ||
           x_bits>=(int32_t)geometry_float_bits(frame->x_max) ||
           y_bits<(int32_t)geometry_float_bits(frame->y_min) ||
           y_bits>=(int32_t)geometry_float_bits(frame->y_max))continue;
        memset(&vertex,0,sizeof(vertex));
        vertex.x=geometry_store_extended(((long double)x+frame->rounding_magic)-frame->rounding_magic);
        vertex.y=geometry_store_extended(((long double)y+frame->rounding_magic)-frame->rounding_magic);
        vertex.r=(float)(color&0xffu);
        vertex.g=(float)((color>>8)&0xffu);
        vertex.b=(float)((color>>16)&0xffu);
        vertex.a=255.0f;
        vertex.oow=inverse;vertex.oow0=inverse;
        if(!ops->draw(ops->context,&vertex))return 0;
    }
    return 1;
}

int gameplay_geometry_draw_point_list(GameMemory *memory,GameAddress list,
    const GameplayPointFrame *frame,const GameplayGeometryPointOps *ops){
    GameplayPointList captured={0};int result;
    if(!memory || !frame || !frame->palette || !ops || !ops->draw)return 0;
    if(!gameplay_geometry_capture_point_list(memory,list,&captured))return 0;
    result=gameplay_geometry_draw_captured_points(&captured,frame,ops);
    gameplay_geometry_release_point_list(&captured);
    return result && !memory->fault;
}

static uint32_t geometry_triangle_key(GameMemory *memory,
        GameAddress vertex) {
    return game_memory_read_u32(memory, vertex + 4u);
}

GameAddress gameplay_geometry_triangulate_ring(GameMemory *memory,
        uint32_t count, GameAddress begin,
        const GameplayGeometryTriangleOps *ops) {
    const uint32_t stride = 0x3cu;
    GameAddress end = begin + (count - 1u) * stride;
    GameAddress center = begin, left, right, cursor;
    uint32_t smallest_key;
    uint32_t remaining;
    if (!count || memory->fault || !ops || !ops->draw) return 0u;
    smallest_key = geometry_triangle_key(memory, begin) * 2u;
    for (cursor = begin + stride; cursor <= end; cursor += stride) {
        uint32_t key = geometry_triangle_key(memory, cursor) * 2u;
        if (memory->fault) return 0u;
        if (key < smallest_key) { smallest_key = key; center = cursor; }
    }
    left = center == begin ? end : center - stride;
    right = center == end ? begin : center + stride;
    if (!ops->draw(ops->context, center, left, right)) return 0u;
    remaining = count - 4u;
    /*481e54's signed <=0 branch skips the middle loop for short rings. */
    while (remaining && remaining<0x80000000u) {
        --remaining;
        GameAddress next_left = left == begin ? end : left - stride;
        GameAddress next_right = right == end ? begin : right + stride;
        GameAddress pivot;
        if (geometry_triangle_key(memory, next_right) >
                geometry_triangle_key(memory, next_left)) {
            pivot = left; left = next_left;
        } else { pivot = right; right = next_right; }
        if (memory->fault || !ops->draw(ops->context, pivot, left, right))
            return 0u;
    }
    cursor = left;
    left = left == begin ? end : left - stride;
    if (!ops->draw(ops->context, cursor, left, right)) return 0u;
    return cursor;
}

int gameplay_geometry_expand_tall_texture(GameMemory *memory,
        GameAddress info, const GameplayGeometryTextureOps *ops) {
    GameAddress old_data, new_data = 0u;
    uint32_t index;
    if (memory->fault || !info || !ops || !ops->allocate || !ops->release)
        return 0;
    if (game_memory_read_u32(memory, info + 0x000u) != 16u ||
        game_memory_read_u32(memory, info + 0x004u) != 256u ||
        game_memory_read_u32(memory, info + 0x008u) != 0u ||
        game_memory_read_u32(memory, info + 0x00cu) != 0u ||
        game_memory_read_u32(memory, info + 0x010u) != 6u ||
        game_memory_read_u32(memory, info + 0x014u) != 5u ||
        game_memory_read_u32(memory, info + 0x41cu) != 16u * 256u)
        return !memory->fault;
    old_data = game_memory_read_u32(memory, info + 0x418u);
    if (memory->fault || !old_data ||
            !ops->allocate(ops->context, 32u * 256u, &new_data) ||
            !new_data)
        return 0;
    for (index = 0u; index < 16u * 256u; ++index) {
        uint8_t pixel = game_memory_read_u8(memory, old_data + index);
        if (memory->fault) return 0;
        game_memory_write_u8(memory, new_data + index, pixel);
        if (memory->fault) return 0;
    }
    for (; index < 32u * 256u; ++index) {
        game_memory_write_u8(memory, new_data + index, 0u);
        if (memory->fault) return 0;
    }
    game_memory_write_u32(memory, info + 0x418u, new_data);
    game_memory_write_u32(memory, info + 0x41cu, 32u * 256u);
    if (memory->fault) return 0;
    return ops->release(ops->context, old_data);
}

static uint64_t geometry_truncate(long double value) {
    if (!isfinite(value) || value > (long double)INT64_MAX ||
            value < (long double)INT64_MIN)
        return UINT64_C(0x8000000000000000);
    return (uint64_t)(int64_t)value;
}
/*42e8e0: asymmetric binary32 boundary stores and nine neighboring sphere
 *lists. Borrow the existing spatial grid and entity owners; no scratch grid. */
static int geometry_position_clear(GameMemory *m,GameAddress object,const float position[3],GameAddress excluded,int *clear){
    long double radius=geometry_read_f32(m,object+0x38),bound;float stored;GameAddress buckets[9];unsigned i,visits=0;
    *clear=0;
    bound=radius*geometry_read_f32(m,0x57217c);stored=(float)bound;
    if(bound>position[0] || !(position[2]>=stored))return !m->fault;
    bound=radius*geometry_read_f32(m,0x572180)+geometry_read_f32(m,0x572184);stored=(float)bound;
    if(!(bound>=position[0]) || position[2]>stored)return !m->fault;
    if(!gameplay_spatial_query_values(0x5f0800,position[0],position[2],buckets))return 0;
    for(i=0;i<9 && !m->fault;i++){
        GameAddress other=game_memory_read_u32(m,buckets[i]+4);
        while(other && !m->fault){
            if(other==excluded){other=game_memory_read_u32(m,other+4);continue;}
            long double dx=(long double)position[0]-geometry_read_f32(m,other+24);
            long double dy=(long double)position[1]-geometry_read_f32(m,other+28);
            long double dz=(long double)position[2]-geometry_read_f32(m,other+32);
            long double sum=(long double)geometry_read_f32(m,other+0x38)+geometry_read_f32(m,object+0x38);
            if(++visits>65536){m->fault=1;return 0;}
            if((dy*dy+dz*dz)+dx*dx>sum*sum){other=game_memory_read_u32(m,other+4);continue;}
            if(isnan((dy*dy+dz*dz)+dx*dx) || isnan(sum)){other=game_memory_read_u32(m,other+4);continue;}
            return !m->fault;
        }
    }
    *clear=1;return !m->fault;
}
int gameplay_geometry_position_clear(GameMemory *m,GameAddress object,const float p[3],int *clear){
    return geometry_position_clear(m,object,p,0,clear);
}
/*42ea50 is the same sphere query, excluding the producer's borrowed handle. */
int gameplay_geometry_position_clear_except(GameMemory *m,GameAddress object,const float p[3],GameAddress excluded,int *clear){
    return geometry_position_clear(m,object,p,excluded,clear);
}
/*410fc0: one sample, then at most three quarter-turn candidates. Both failure
 *and success publish the last candidate; callers may intentionally ignore the
 *boolean result. The shared RNG cursor and immutable sine table keep one owner. */
int gameplay_geometry_nearby_location(GameMemory *memory,float x,float z,float radius,float output[2]){
    uint32_t cursor,angle,i,bits_x,bits_z;
    if(!memory || !output || memory->fault)return 0;
    cursor=game_memory_read_u32(memory,0x6308c0);
    if(cursor==1024)cursor=0;
    if(cursor>=1024){memory->fault=1;memory->fault_address=0x6308c0;memory->fault_size=4;return 0;}
    angle=(uint32_t)geometry_truncate((long double)geometry_read_f32(memory,0x6308c8+cursor*4)*geometry_read_f32(memory,0x571f38));
    game_memory_write_u32(memory,0x6308c0,cursor+1);
    for(i=0;i<3;++i){
        output[0]=(float)((long double)geometry_read_f32(memory,0x6298a8+(angle&2047u)*4)*radius+x);
        output[1]=(float)((long double)geometry_read_f32(memory,0x6298a8+((angle+512)&2047u)*4)*radius+z);
        if(memory->fault)return 0;
        bits_x=geometry_float_bits(output[0]);bits_z=geometry_float_bits(output[1]);
        if((int32_t)bits_x>0 && bits_x<0x48000000 && (int32_t)bits_z>0 && bits_z<0x48000000)return 1;
        angle=(angle+512)&2047u;
    }
    return 0;
}

long double gameplay_geometry_square_root(float value) {
    return sqrtl((long double)value);
}
/*4107a0: original weighted axial distance, with binary32 stores. */
float gameplay_geometry_fast_distance(GameMemory *m,float x,float z,float other_x,float other_z){
    float dx=(float)((long double)x-other_x),dz=(float)((long double)z-other_z),zero=geometry_read_f32(m,0x571f14);
    if(dx<zero || isnan(dx))dx=-dx;
    if(dz<zero || isnan(dz))dz=-dz;
    return dz<dx || isnan(dz) || isnan(dx)?
        (float)((long double)dz*geometry_read_f32(m,0x571f18)+dx):
        (float)((long double)dx*geometry_read_f32(m,0x571f18)+dz);
}
/*411080 chooses an integer Z and the corresponding circle-edge X. The shared
 *two-sample RNG cursor and reflected boundary behavior match the reference. */
int gameplay_geometry_circle_location(GameMemory *m,float x,float z,float radius,float output[2]){
    long double upper=(long double)z+radius,lower=(long double)z-radius;
    float low=(float)lower,high=(float)upper,delta,square;uint32_t cursor,first,last,span,bits;int32_t n;
    if(!m || !output || m->fault)return 0;
    if(!(upper<geometry_read_f32(m,0x571f3c)))high=0x1.ffffp16f;
    if(lower<geometry_read_f32(m,0x571f14) || isnan(lower))low=0;
    first=(uint32_t)geometry_truncate((long double)low+geometry_read_f32(m,0x571f1c));
    last=(uint32_t)geometry_truncate((long double)high-geometry_read_f32(m,0x571f1c));
    span=last-first;memcpy(&n,&span,4);
    cursor=game_memory_read_u32(m,0x6308c0);if(cursor==1024)cursor=0;
    if(cursor>=1024){m->fault=1;return 0;}game_memory_write_u32(m,0x6308c0,cursor+1);
    bits=first+(uint32_t)geometry_truncate((long double)n*geometry_read_f32(m,0x6308c8+cursor*4));memcpy(&n,&bits,4);
    output[1]=(float)n;
    square=(float)((long double)radius*radius-((long double)n-z)*((long double)n-z));
    delta=(float)sqrtl(square);
    cursor=game_memory_read_u32(m,0x6308c0);if(cursor==1024)cursor=0;
    if(cursor>=1024){m->fault=1;return 0;}game_memory_write_u32(m,0x6308c0,cursor+1);
    if((uint32_t)geometry_truncate((long double)geometry_read_f32(m,0x6308c8+cursor*4)*geometry_read_f32(m,0x571f28)))delta=-delta;
    upper=(long double)delta+x;output[0]=(float)upper;memcpy(&bits,&output[0],4);
    if(upper<geometry_read_f32(m,0x571f14) || isnan(upper) || (int32_t)bits>0x47ffff80)
        output[0]=(float)((long double)x-delta);
    return !m->fault;
}

int gameplay_geometry_interpolate_timed_vector(GameMemory *memory,
        GameAddress output, GameAddress first, float first_duration,
        GameAddress second, float second_duration,
        long double final_values[3]) {
    float value;
    long double duration_sum;
    long double factor;
    unsigned component;
    for (component = 0u; component < 3u; ++component) {
        float second_value = geometry_read_f32(memory,
            second + component * 4u);
        float first_value = geometry_read_f32(memory,
            first + component * 4u);
        value = (float)((long double)second_value -
            (long double)first_value);
        geometry_write_f32(memory, output + component * 4u,
            value);
        if (memory->fault) return 0;
    }
    duration_sum = (long double)second_duration +
        (long double)first_duration;
    factor = (long double)first_duration / duration_sum;
    for (component = 0u; component < 3u; ++component) {
        value = geometry_read_f32(memory, output + component * 4u);
        value = (float)((long double)value * factor);
        geometry_write_f32(memory, output + component * 4u,
            value);
        if (memory->fault) return 0;
    }
    for (component = 0u; component < 3u; ++component) {
        float scaled = geometry_read_f32(memory,
            output + component * 4u);
        float first_value = geometry_read_f32(memory,
            first + component * 4u);
        long double final_value = (long double)scaled +
            (long double)first_value;
        if (final_values) final_values[component] = final_value;
        geometry_write_f32(memory, output + component * 4u,
            (float)final_value);
        if (memory->fault) return 0;
    }
    return !memory->fault;
}

void gameplay_geometry_mirror_square_texture(GameMemory *memory,
    GameAddress pixels, int32_t dimension) {
    int32_t row;
    if (dimension <= 0) return;
    for (row = 0; row < dimension && !memory->fault; ++row) {
        GameAddress left = pixels + (uint32_t)row * (uint32_t)dimension;
        GameAddress right = left + (uint32_t)dimension - 1u;
        int32_t swaps;
        for (swaps = dimension >> 1; swaps > 0 && !memory->fault; --swaps) {
            uint8_t left_pixel = game_memory_read_u8(memory, left);
            uint8_t right_pixel = game_memory_read_u8(memory, right);
            if (memory->fault) return;
            game_memory_write_u8(memory, left++, right_pixel);
            if (!memory->fault)
                game_memory_write_u8(memory, right--, left_pixel);
        }
    }
}

GameAddress gameplay_geometry_select_visible_batches(GameMemory *memory,
    GameAddress batch, int32_t batch_count, float threshold) {
    GameAddress head = 0u;
    int32_t batch_index;
    for (batch_index = 0; batch_index < batch_count && !memory->fault;
            ++batch_index, batch += 0x28u) {
        int32_t object_count = (int32_t)game_memory_read_u32(memory, batch);
        GameAddress objects = game_memory_read_u32(memory, batch + 4u);
        int32_t object_index;
        int selected = 1;
        if (memory->fault) return 0u;
        for (object_index = 0; object_index < object_count; ++object_index) {
            GameAddress object = game_memory_read_u32(memory,
                objects + (uint32_t)object_index * 4u);
            float depth = geometry_read_f32(memory, object + 8u);
            if (memory->fault) return 0u;
            if (!(depth >= threshold)) {
                selected = 0;
                break;
            }
        }
        if (selected) {
            game_memory_write_u32(memory, batch + 12u, head);
            head = batch;
        }
    }
    return head;
}

void gameplay_geometry_smooth_yaw(GameMemory *memory, GameAddress object) {
    float velocity_x = geometry_read_f32(memory, object + 0x19cu);
    float threshold = geometry_read_f32(memory, 0x00574d40u);
    int16_t current_yaw;
    int16_t target_yaw;
    int32_t target_magnitude;
    int32_t step;
    int32_t updated_yaw;
    if (memory->fault) return;
    if (!(fabsf(velocity_x) > threshold) &&
            !(fabsf(geometry_read_f32(memory, object + 0x1a4u)) > threshold))
        return;
    current_yaw = (int16_t)game_memory_read_u16(memory, object + 0x2c0u);
    target_yaw = (int16_t)game_memory_read_u16(memory, object + 0x76u);
    if (memory->fault || current_yaw == target_yaw) return;
    target_magnitude = target_yaw < 0 ? -(int32_t)target_yaw : target_yaw;
    step = target_magnitude < 0x27 ? 2 : 1;
    if (current_yaw < target_yaw) {
        updated_yaw = (int32_t)current_yaw + step;
        if (updated_yaw > target_yaw) updated_yaw = target_yaw;
    } else {
        updated_yaw = (int32_t)current_yaw - step;
        if (updated_yaw < target_yaw) updated_yaw = target_yaw;
    }
    game_memory_write_u16(memory, object + 0x2c0u,
        (uint16_t)updated_yaw);
}

void gameplay_geometry_premultiply_uv(GameMemory *memory, GameAddress node) {
    while (node != 0u && !memory->fault) {
        RenderArenaPacket *packet=geometry_packet_view(memory,node);
        int32_t count=packet?(int32_t)packet->count:(int32_t)game_memory_read_u32(memory,node);
        uint32_t flags=packet?packet->flags:game_memory_read_u32(memory,node+8u);
        GameAddress next=geometry_packet_next(memory,node,packet);
        if (memory->fault) return;
        if ((flags & 4u) != 0u && count > 0) {
            GameAddress vertices=packet?packet->vertices:game_memory_read_u32(memory,node+4u);
            GameAddress texcoords=packet?packet->attributes:game_memory_read_u32(memory,node+16u);
            uint32_t *indices=render_arena_indices(vertices,(size_t)count);
            const uint32_t *owned_indices=indices?indices:
                scene_projection_storage_links(vertices,(size_t)count);
            RenderArenaUv *values=render_arena_uv(texcoords,(size_t)count);
            const RenderArenaUv *owned_values=values?values:
                scene_projection_storage_work_uv(texcoords,(size_t)count);
            int32_t index;
            if (memory->fault) return;
            for (index = count - 1; index >= 0; --index) {
                GameAddress vertex=owned_indices?owned_indices[index]:
                    geometry_index_value(memory,vertices+(uint32_t)index*4u);
                RenderArenaVertex *owned_position=geometry_vertex_value(memory,vertex);
                float depth=owned_position?owned_position->position[2]:
                    geometry_read_f32(memory,vertex+8u);
                GameAddress uv = texcoords + (uint32_t)index * 8u;
                float s=owned_values?owned_values[index].u:
                    geometry_read_f32(memory,uv);
                float t=owned_values?owned_values[index].v:
                    geometry_read_f32(memory,uv+4u);
                if (memory->fault) return;
                if(values){values[index].u=geometry_multiply(s,depth);values[index].v=geometry_multiply(t,depth);continue;}
                if(owned_values){
                    RenderArenaUv updated={geometry_multiply(s,depth),
                        geometry_multiply(t,depth)};
                    if(!scene_projection_storage_set_work_uv(uv,&updated))
                        memory->fault=1;
                    continue;
                }
                geometry_write_f32(memory, uv,
                    geometry_multiply(s, depth));
                if (!memory->fault)
                    geometry_write_f32(memory, uv + 4u,
                        geometry_multiply(t, depth));
            }
        }
        node = next;
    }
}

int gameplay_geometry_world_bounds_values(float radius,const float position[3]) {
    uint32_t vertical_bits = geometry_float_bits(position[1]);
    float world_x;
    float world_z;
    volatile long double lower_extended;
    volatile long double upper_extended;
    float lower;
    float upper;
    if (            (int32_t)vertical_bits > (int32_t)0x46ea6000u ||
            vertical_bits > 0xc6ea6000u)
        return 0;
    world_x = position[0];
    world_z = position[2];
    lower_extended = (long double)radius * (long double)2.0f;
    lower = (float)lower_extended;
    if (lower_extended > (long double)world_x || !(world_z >= lower))
        return 0;
    upper_extended = (long double)radius * (long double)-2.0f;
    upper_extended += (long double)131072.0f;
    upper = (float)upper_extended;
    return upper_extended >= (long double)world_x && world_z <= upper;
}
int gameplay_geometry_world_bounds_contains(GameMemory *memory,GameAddress object){
    PresentationPose *pose=presentation_instance_pose(object);
    const PresentationInstance *instance=presentation_instance_find(object);
    float position[3],radius;unsigned i;
    if(pose && instance){memcpy(position,pose->local_position,sizeof(position));memcpy(&radius,&instance->definition.radius,4u);}
    else {for(i=0u;i<3u;++i)position[i]=geometry_read_f32(memory,object+0x64u+i*4u);radius=geometry_read_f32(memory,object+0x38u);}
    return !memory->fault && gameplay_geometry_world_bounds_values(radius,position);
}

GameAddress gameplay_geometry_filter_backfaces(GameMemory *memory,
    GameAddress node, float threshold) {
    GameAddress visible = 0u;
    while (node != 0u && !memory->fault) {
        RenderArenaPacket *packet=geometry_packet_view(memory,node);
        GameAddress next=geometry_packet_next(memory,node,packet);
        GameAddress vertices=packet?packet->vertices:game_memory_read_u32(memory,node+4u);
        uint32_t *indices=render_arena_indices(vertices,3);
        const uint32_t *owned_indices=indices?indices:
            scene_projection_storage_links(vertices,3u);
        GameAddress v0=owned_indices?owned_indices[0]:geometry_index_value(memory,vertices);
        GameAddress v1=owned_indices?owned_indices[1]:geometry_index_value(memory,vertices+4u);
        GameAddress v2=owned_indices?owned_indices[2]:geometry_index_value(memory,vertices+8u);
        RenderArenaVertex *p0=geometry_vertex_value(memory,v0);
        RenderArenaVertex *p1=geometry_vertex_value(memory,v1);
        RenderArenaVertex *p2=geometry_vertex_value(memory,v2);
        float x0 = p0?p0->position[0]:geometry_read_f32(memory, v0);
        float y0 = p0?p0->position[1]:geometry_read_f32(memory, v0 + 4u);
        float x1 = p1?p1->position[0]:geometry_read_f32(memory, v1);
        float y1 = p1?p1->position[1]:geometry_read_f32(memory, v1 + 4u);
        float x2 = p2?p2->position[0]:geometry_read_f32(memory, v2);
        float y2 = p2?p2->position[1]:geometry_read_f32(memory, v2 + 4u);
        float first;
        float second;
        float determinant;
        if (memory->fault) return 0u;
        first = geometry_multiply(geometry_subtract(x0, x1),
            geometry_subtract(y2, y1));
        second = geometry_multiply(geometry_subtract(x2, x1),
            geometry_subtract(y0, y1));
        determinant = geometry_subtract(first, second);
        if (determinant > threshold) {
            if(packet){if(!render_packet_set_next(node,visible))packet->next=visible;}
            else game_memory_write_u32(memory,node+12u,visible);
            visible = node;
        }
        node = next;
    }
    return visible;
}

static int geometry_project_vertices(GameMemory *memory,
    GameAddress begin, GameAddress end, float center_x, float center_y,
    float scale, float vertical_scale, float numerator,SceneSurface *surface) {
    GameAddress cursor;
    RenderArenaVertex *vertices;
    if (end < begin || ((end - begin) & 15u) != 0u ||
            (end - begin) / 16u > 65536u)
        return 0;
    vertices=render_arena_vertices(begin,(end-begin)/16u);
    if(!vertices)vertices=scene_projection_storage_transformed(begin,
        (end-begin)/16u);
    if(!vertices && surface)vertices=scene_surface_transformed(surface,begin,
        (end-begin)/16u);
    for (cursor = begin; cursor != end && !memory->fault; cursor += 16u) {
        RenderArenaVertex *vertex=vertices?&vertices[(cursor-begin)/16u]:NULL;
        float x = vertex?vertex->position[0]:geometry_read_f32(memory, cursor);
        float y = vertex?vertex->position[1]:geometry_read_f32(memory, cursor + 4u);
        float z = vertex?vertex->position[2]:geometry_read_f32(memory, cursor + 8u);
        /*4e4570 retains the reciprocal and products until the three final
         *stores. Early float rounding changes projected coordinates. */
        long double inverse = (long double)numerator/z;
        long double x_term = ((long double)x*scale)*inverse;
        long double y_term = (((long double)y*vertical_scale)*scale)*inverse;
        if (memory->fault) return 0;
        if(vertex){vertex->position[0]=(float)((long double)center_x-x_term);
            vertex->position[1]=(float)((long double)center_y-y_term);vertex->position[2]=(float)inverse;
        }else {
        geometry_write_f32(memory, cursor,
            (float)((long double)center_x-x_term));
        geometry_write_f32(memory, cursor + 4u,
            (float)((long double)center_y-y_term));
        geometry_write_f32(memory, cursor + 8u, (float)inverse);
        }
    }
    return !memory->fault;
}

int16_t gameplay_geometry_build_object_vertices(GameMemory *memory,
        GameAddress object, int use_template) {
    int16_t count = (int16_t)game_memory_read_u16(memory, object + 0x44u);
    GameAddress source;
    GameAddress destination;
    GameAddress template_object = 0u;
    uint32_t x_bits;
    uint32_t z_bits;
    float y_offset;
    int32_t index;
    if (memory->fault || count <= 0) return count;
    source = game_memory_read_u32(memory, object + 0x3cu);
    if (use_template) {
        destination = game_memory_read_u32(memory, object + 0x70u);
        template_object = game_memory_read_u32(memory, object + 0x54u);
        x_bits = game_memory_read_u32(memory, template_object + 0x10u);
        y_offset = geometry_read_f32(memory, template_object + 0x20u);
        z_bits = game_memory_read_u32(memory, template_object + 0x30u);
    } else {
        destination = game_memory_read_u32(memory, object + 0x40u);
        x_bits = game_memory_read_u32(memory, object + 0x8cu);
        y_offset = geometry_read_f32(memory, object + 0x9cu);
        z_bits = game_memory_read_u32(memory, object + 0xacu);
    }
    if (memory->fault) return count;
    for (index = 0; index < count; ++index) {
        GameAddress source_record = source + (uint32_t)index * 28u;
        GameAddress destination_record = destination + (uint32_t)index * 12u;
        float source_y = geometry_read_f32(memory, source_record + 16u);
        game_memory_write_u32(memory, destination_record, x_bits);
        geometry_write_f32(memory, destination_record + 4u,
            geometry_add(source_y, y_offset));
        game_memory_write_u32(memory, destination_record + 8u, z_bits);
        if (memory->fault) break;
    }
    return count;
}

GameplayHeadingMatrixResult gameplay_geometry_build_heading_matrix(
        GameMemory *memory, GameAddress object) {
    GameplayHeadingMatrixResult result;
    uint16_t heading = game_memory_read_u16(memory, object + 0x78u);
    uint32_t sine_index = (uint32_t)heading & 0x7ffu;
    uint32_t cosine_index = ((uint32_t)(int32_t)(int16_t)heading + 0x200u) &
        0x7ffu;
    uint32_t sine_bits = game_memory_read_u32(memory,
        0x006298a8u + sine_index * 4u);
    uint32_t cosine_bits = game_memory_read_u32(memory,
        0x006298a8u + cosine_index * 4u);
    uint32_t translation_y = game_memory_read_u32(memory, object + 0x6cu);
    uint32_t translation_z = game_memory_read_u32(memory, object + 0x64u);
    result.translation_x = game_memory_read_u32(memory, object + 0x68u);
    result.matrix = game_memory_read_u32(memory, object + 0x54u) + 4u;
    game_memory_write_u32(memory, result.matrix + 0x00u, cosine_bits);
    game_memory_write_u32(memory, result.matrix + 0x04u, 0u);
    game_memory_write_u32(memory, result.matrix + 0x08u, sine_bits);
    game_memory_write_u32(memory, result.matrix + 0x0cu, translation_z);
    game_memory_write_u32(memory, result.matrix + 0x10u, 0u);
    game_memory_write_u32(memory, result.matrix + 0x14u, 0x3f800000u);
    game_memory_write_u32(memory, result.matrix + 0x18u, 0u);
    game_memory_write_u32(memory, result.matrix + 0x1cu, result.translation_x);
    game_memory_write_u32(memory, result.matrix + 0x20u,
        sine_bits ^ 0x80000000u);
    game_memory_write_u32(memory, result.matrix + 0x24u, 0u);
    game_memory_write_u32(memory, result.matrix + 0x28u, cosine_bits);
    game_memory_write_u32(memory, result.matrix + 0x2cu, translation_y);
    return result;
}

static float geometry_angle_value(GameMemory *memory, uint16_t angle,
        uint32_t quarter_turn) {
    uint32_t index = ((uint32_t)angle + quarter_turn) & 0x07ffu;
    return geometry_read_f32(memory, 0x006298a8u + index * 4u);
}

void gameplay_geometry_build_orientation_matrix(GameMemory *memory,
        GameAddress object) {
    uint16_t angles[3];
    float position[3], output[12];
    unsigned i;
    for (i = 0u; i < 3u; ++i) {
        angles[i] = game_memory_read_u16(memory, object + 0x24u + i * 2u);
        position[i] = geometry_read_f32(memory, object + 0x18u + i * 4u);
    }
    if (memory->fault) return;
    gameplay_geometry_orientation_values(memory, angles, position, output);
    for (i = 0u; i < 12u && !memory->fault; ++i)
        geometry_write_f32(memory, object + 0x80u + i * 4u, output[i]);
}

void gameplay_geometry_orientation_values(GameMemory *memory,
        const uint16_t angles[3], const float position[3], float output[12]) {
    uint16_t angle_a = angles[0], angle_b = angles[1], angle_c = angles[2];
    float ca, cb, cc, sa, sb, sc, sc_sb_saved;
    long double sc_sb;

    if (memory->fault) return;
    ca = geometry_angle_value(memory, angle_a, 0x200u);
    cb = geometry_angle_value(memory, angle_b, 0x200u);
    cc = geometry_angle_value(memory, angle_c, 0x200u);
    sa = geometry_angle_value(memory, angle_a, 0u);
    sc = geometry_angle_value(memory, angle_c, 0u);
    sb = geometry_angle_value(memory, angle_b, 0u);
    if (memory->fault) return;

    sc_sb = (long double)sc * (long double)sb;
    sc_sb_saved = (float)sc_sb;
    output[0] =
        (float)((long double)cc * (long double)ca +
            sc_sb * (long double)sa);
    output[1] =
        (float)(((long double)sb * (long double)sa) * (long double)cc -
            (long double)sc * (long double)ca);
    output[2] = (float)((long double)sa * (long double)cb);
    output[4] = (float)((long double)sc * (long double)cb);
    output[5] = (float)((long double)cc * (long double)cb);
    output[6] = -sb;
    output[8] =
        (float)((long double)sc_sb_saved * (long double)ca -
            (long double)sa * (long double)cc);
    output[9] =
        (float)((long double)sa * (long double)sc +
            ((long double)sb * (long double)cc) * (long double)ca);
    output[10] = (float)((long double)cb * (long double)ca);
    memcpy(output + 3u, position, sizeof(float));
    memcpy(output + 7u, position + 1u, sizeof(float));
    memcpy(output + 11u, position + 2u, sizeof(float));
}

void gameplay_geometry_build_yaw_matrix(GameMemory *memory,
        GameAddress object) {
    uint16_t angle = game_memory_read_u16(memory, object + 0x24u);
    uint32_t sine_index = (uint32_t)angle & 0x7ffu;
    uint32_t cosine_index =
        ((uint32_t)(int32_t)(int16_t)angle + 0x200u) & 0x7ffu;
    uint32_t sine_bits = game_memory_read_u32(memory,
        0x006298a8u + sine_index * 4u);
    uint32_t cosine_bits = game_memory_read_u32(memory,
        0x006298a8u + cosine_index * 4u);
    uint32_t translation_x = game_memory_read_u32(memory, object + 0x18u);
    uint32_t translation_y = game_memory_read_u32(memory, object + 0x1cu);
    uint32_t translation_z = game_memory_read_u32(memory, object + 0x20u);
    if (memory->fault) return;
    game_memory_write_u32(memory, object + 0x80u, cosine_bits);
    game_memory_write_u32(memory, object + 0x84u, 0u);
    game_memory_write_u32(memory, object + 0x88u, sine_bits);
    game_memory_write_u32(memory, object + 0x8cu, translation_x);
    game_memory_write_u32(memory, object + 0x90u, 0u);
    game_memory_write_u32(memory, object + 0x94u, 0x3f800000u);
    game_memory_write_u32(memory, object + 0x98u, 0u);
    game_memory_write_u32(memory, object + 0x9cu, translation_y);
    game_memory_write_u32(memory, object + 0xa0u,
        sine_bits ^ 0x80000000u);
    game_memory_write_u32(memory, object + 0xa4u, 0u);
    game_memory_write_u32(memory, object + 0xa8u, cosine_bits);
    game_memory_write_u32(memory, object + 0xacu, translation_z);
}

void gameplay_geometry_setup_angle_basis(GameMemory *memory, uint16_t angle) {
    uint32_t sine_index = (uint32_t)angle & 0x7ffu;
    uint32_t cosine_index = ((uint32_t)(int32_t)(int16_t)angle + 0x200u) &
        0x7ffu;
    uint32_t sine_bits = game_memory_read_u32(memory,
        0x006298a8u + sine_index * 4u);
    uint32_t cosine_bits = game_memory_read_u32(memory,
        0x006298a8u + cosine_index * 4u);
    game_memory_write_u32(memory, 0x005ac504u, cosine_bits);
    game_memory_write_u32(memory, 0x005ac508u, sine_bits);
    game_memory_write_u32(memory, 0x005ac50cu, sine_bits ^ 0x80000000u);
    game_memory_write_u32(memory, 0x005ac510u, cosine_bits);
}

GameplaySpriteOverlapResult gameplay_geometry_sprite_overlap(
        GameMemory *memory, GameAddress context, GameAddress sample) {
    GameplaySpriteOverlapResult result;
    GameAddress first;
    GameAddress second;
    uint32_t horizontal;
    uint32_t vertical;
    result.depth_bits = game_memory_read_u32(memory, sample + 0x18u);
    result.distance_bits = 0u;
    result.sample_index = 0u;
    result.horizontal_squared = 0u;
    result.distance_squared = 0u;
    if ((int32_t)result.depth_bits >= (int32_t)0x3f800000u) {
        result.value = 0u;
        result.branch = 0u;
        return result;
    }
    result.distance_bits = game_memory_read_u32(memory, context + 0x1824u);
    if ((int32_t)result.distance_bits <= (int32_t)0x41000000u) {
        result.value = 1u;
        result.branch = 1u;
        return result;
    }
    first = game_memory_read_u32(memory, sample + 0x0cu);
    second = game_memory_read_u32(memory, sample + 0x10u);
    result.sample_index = game_memory_read_u8(memory, second + 0x0du);
    horizontal = (uint32_t)game_memory_read_u8(memory,
        game_memory_read_u32(memory, first + 0x10u)) -
        (uint32_t)game_memory_read_u8(memory,
            game_memory_read_u32(memory, second + 0x10u) +
            result.sample_index - 1u);
    vertical = (uint32_t)game_memory_read_u8(memory,
        game_memory_read_u32(memory, first + 0x14u)) -
        (uint32_t)game_memory_read_u8(memory,
            game_memory_read_u32(memory, second + 0x14u) +
            result.sample_index - 1u);
    result.horizontal_squared = horizontal * horizontal;
    result.distance_squared = result.horizontal_squared + vertical * vertical;
    result.value = (int32_t)result.distance_squared > 64 ? 1u : 0u;
    result.branch = 2u;
    return result;
}

static int geometry_footprint_test(GameMemory *memory,GameAddress player,
    const float position[3],const float footprint[8][3],GameplayGeometryFootprintResult *result) {
    float world_x;
    float world_z;
    unsigned point_index;

    if (!result) return 0;
    result->height_table = 0u;
    result->integer_z_high = 0u;
    world_x = position?position[0]:geometry_read_f32(memory, player + 0x64u);
    world_z = position?position[2]:geometry_read_f32(memory, player + 0x6cu);
    if (memory->fault) return 0;
    for (point_index = 0u; point_index < 8u; ++point_index) {
        GameAddress point = player + 0x424u + point_index * 12u;
        float offset_x = footprint?footprint[point_index][0]:geometry_read_f32(memory, point);
        float offset_z = footprint?footprint[point_index][2]:geometry_read_f32(memory, point + 8u);
        volatile long double sample_x = (long double)offset_x + world_x;
        volatile long double sample_z = (long double)offset_z + world_z;
        uint64_t integer_x;
        uint64_t integer_z;
        int32_t x_row;
        uint32_t cell_offset;
        uint8_t flags;

        if (memory->fault) return 0;
        integer_x = geometry_truncate(sample_x);
        integer_z = geometry_truncate(sample_z);
        result->height_table = game_memory_read_u32(memory, 0x00597d90u);
        x_row = (int32_t)((uint32_t)integer_x & 0xfffffe3fu) >> 6;
        cell_offset = (uint32_t)x_row +
            ((uint32_t)integer_z & 0xfffffe00u) * 4u + 5u;
        flags = memory->terrain && cell_offset<65536u*8u ?
            memory->terrain->cells[cell_offset/8u].flags :
            game_memory_read_u8(memory,result->height_table+cell_offset);
        if (memory->fault) return 0;
        result->integer_z_high = (uint32_t)(integer_z >> 32);
        if ((flags & 0x02u) != 0u) return 1;
    }
    return 0;
}

int gameplay_geometry_footprint_values(GameMemory *memory,const float position[3],
    const float footprint[8][3],GameplayGeometryFootprintResult *result){
    return geometry_footprint_test(memory,0u,position,footprint,result);
}
int gameplay_geometry_footprint_blocked(GameMemory *memory,GameAddress player,
    GameplayGeometryFootprintResult *result){
    PresentationPose *pose=presentation_instance_pose(player);
    PlayerRig *rig=presentation_instance_player_rig(player);
    return geometry_footprint_test(memory,player,pose?pose->local_position:NULL,
        rig?(const float (*)[3])rig->terrain_ring:NULL,result);
}

long double gameplay_geometry_terrain_height(GameMemory *memory,
    float world_x, float world_z) {
    int32_t integer_x = geometry_grid_integer(world_x);
    int32_t integer_z = geometry_grid_integer(world_z);
    uint32_t grid_x = ((uint32_t)(integer_x >> 9)) & 0xffu;
    uint32_t grid_z = ((uint32_t)(integer_z >> 9)) & 0xffu;
    float local_x = world_x - (float)((integer_x >> 9) * 512);
    float local_z = world_z - (float)((integer_z >> 9) * 512);
    GameAddress table = game_memory_read_u32(memory, 0x00597d90u);
    float origin;
    float along_x;
    float along_z;
    const long double scale = (long double)0.001953125f;

    if (memory->fault) return 0.0L;
    if (((grid_x + grid_z) & 1u) == 0u) {
        if (local_z < local_x) {
            origin = geometry_terrain_height_at(memory, table,
                grid_x + 1u, grid_z);
            along_x = geometry_terrain_height_at(memory, table,
                grid_x, grid_z);
            along_z = geometry_terrain_height_at(memory, table,
                grid_x + 1u, grid_z + 1u);
            local_x = 512.0f - local_x;
        } else {
            origin = geometry_terrain_height_at(memory, table,
                grid_x, grid_z + 1u);
            along_x = geometry_terrain_height_at(memory, table,
                grid_x + 1u, grid_z + 1u);
            along_z = geometry_terrain_height_at(memory, table,
                grid_x, grid_z);
            local_z = 512.0f - local_z;
        }
    } else if (local_z < 512.0f - local_x) {
        origin = geometry_terrain_height_at(memory, table, grid_x, grid_z);
        along_x = geometry_terrain_height_at(memory, table,
            grid_x + 1u, grid_z);
        along_z = geometry_terrain_height_at(memory, table,
            grid_x, grid_z + 1u);
    } else {
        origin = geometry_terrain_height_at(memory, table,
            grid_x + 1u, grid_z + 1u);
        along_x = geometry_terrain_height_at(memory, table,
            grid_x, grid_z + 1u);
        along_z = geometry_terrain_height_at(memory, table,
            grid_x + 1u, grid_z);
        local_x = 512.0f - local_x;
        local_z = 512.0f - local_z;
    }
    if (memory->fault) return 0.0L;
    return ((long double)along_z - (long double)origin) * scale *
        (long double)local_z +
        ((long double)along_x - (long double)origin) * scale *
        (long double)local_x + (long double)origin;
}

long double gameplay_geometry_detail_height(GameMemory *memory,
    float world_x, float world_z) {
    int32_t integer_x = (int32_t)world_x;
    int32_t integer_z = (int32_t)world_z;
    int32_t grid_x = integer_x >> 8;
    int32_t grid_z = integer_z >> 8;
    float local_x;
    float local_z;
    int32_t neighbor_x;
    int32_t neighbor_z;
    float distance_x;
    float distance_z;
    float weight_x;
    float weight_z;
    float centre;
    float along_x;
    float along_z;
    float diagonal;
    long double centre_line;
    long double neighbor_line;

    if (memory->fault || grid_x < 0 || grid_z < 0 ||
            grid_x >= 512 || grid_z >= 512)
        return 0.0L;
    local_x = world_x - (float)(grid_x << 8);
    local_z = world_z - (float)(grid_z << 8);
    distance_x = geometry_detail_distance(local_x, grid_x, &neighbor_x);
    distance_z = geometry_detail_distance(local_z, grid_z, &neighbor_z);
    if (neighbor_x < 0 || neighbor_x >= 512) neighbor_x = grid_x;
    if (neighbor_z < 0 || neighbor_z >= 512) neighbor_z = grid_z;
    weight_x = (float)((long double)distance_x *
        (long double)0.00390625f);
    weight_z = (float)((long double)distance_z *
        (long double)0.00390625f);
    centre = geometry_detail_height_at(memory, (uint32_t)grid_x,
        (uint32_t)grid_z);
    along_x = geometry_detail_height_at(memory, (uint32_t)neighbor_x,
        (uint32_t)grid_z);
    along_z = geometry_detail_height_at(memory, (uint32_t)grid_x,
        (uint32_t)neighbor_z);
    diagonal = geometry_detail_height_at(memory, (uint32_t)neighbor_x,
        (uint32_t)neighbor_z);
    if (memory->fault) return 0.0L;
    centre_line = ((long double)along_z - (long double)centre) *
        (long double)weight_z + (long double)centre;
    neighbor_line = ((long double)diagonal - (long double)along_x) *
        (long double)weight_z + (long double)along_x;
    return ((neighbor_line - centre_line) * (long double)weight_x +
        centre_line) * (long double)-8.0f;
}

int gameplay_geometry_terrain_normal_values(GameMemory *memory, float output[3],
    float world_x, float world_z) {
    int32_t integer_x = geometry_grid_integer(world_x);
    int32_t integer_z = geometry_grid_integer(world_z);
    uint32_t grid_x = ((uint32_t)(integer_x >> 9)) & 0xffu;
    uint32_t grid_z = ((uint32_t)(integer_z >> 9)) & 0xffu;
    float local_x = world_x - (float)((integer_x >> 9) * 512);
    float local_z = world_z - (float)((integer_z >> 9) * 512);
    GameAddress table = game_memory_read_u32(memory, 0x00597d90u);
    float origin;
    float ax, ay, az, bx, by, bz;
    float nx, ny, nz, length_squared;
    long double inverse_length;

    if (memory->fault) return 0;
    if (((grid_x + grid_z) & 1u) == 0u) {
        origin = geometry_terrain_height_at(memory, table, grid_x, grid_z);
        if (local_z < local_x) {
            ax = geometry_terrain_height_at(memory, table,
                grid_x + 1u, grid_z + 1u) - origin;
            ay = 512.0f; az = 512.0f;
            bx = geometry_terrain_height_at(memory, table,
                grid_x + 1u, grid_z) - origin;
            by = 512.0f; bz = 0.0f;
        } else {
            ax = geometry_terrain_height_at(memory, table,
                grid_x, grid_z + 1u) - origin;
            ay = 512.0f; az = 0.0f;
            bx = geometry_terrain_height_at(memory, table,
                grid_x + 1u, grid_z + 1u) - origin;
            by = 512.0f; bz = 512.0f;
        }
    } else {
        origin = geometry_terrain_height_at(memory, table,
            grid_x + 1u, grid_z);
        if (local_z < 512.0f - local_x) {
            ax = geometry_terrain_height_at(memory, table,
                grid_x, grid_z) - origin;
            ay = -512.0f; az = -512.0f;
            bx = geometry_terrain_height_at(memory, table,
                grid_x, grid_z + 1u) - origin;
            by = 0.0f; bz = 512.0f;
        } else {
            ax = geometry_terrain_height_at(memory, table,
                grid_x, grid_z + 1u) - origin;
            ay = 0.0f; az = -512.0f;
            bx = geometry_terrain_height_at(memory, table,
                grid_x + 1u, grid_z + 1u) - origin;
            by = 512.0f; bz = 512.0f;
        }
    }
    if (memory->fault) return 0;
    nx = (float)((long double)ax * (long double)bz -
        (long double)bx * (long double)by);
    ny = (float)((long double)by * (long double)ay -
        (long double)bz * (long double)az);
    nz = (float)((long double)bx * (long double)az -
        (long double)ax * (long double)ay);
    length_squared = (float)(((long double)nz * (long double)nz +
        (long double)ny * (long double)ny) +
        (long double)nx * (long double)nx);
    inverse_length = 1.0L / sqrtl((long double)length_squared);
    output[0]=(float)((long double)nx * inverse_length);
    output[1]=(float)((long double)ny * inverse_length);
    output[2]=(float)((long double)nz * inverse_length);
    return !memory->fault;
}
int gameplay_geometry_terrain_normal(GameMemory *memory,GameAddress output,
        float world_x,float world_z){
    float normal[3];unsigned i;
    if(!gameplay_geometry_terrain_normal_values(memory,normal,world_x,world_z))return 0;
    for(i=0;i<3u && !memory->fault;++i)geometry_write_f32(memory,output+i*4u,normal[i]);
    return !memory->fault;
}

static uint8_t geometry_terrain_attribute(GameMemory *memory,uint32_t x,uint32_t z) {
    uint32_t index=(z&255u)*256u+(x&255u);
    if(memory->terrain)return (uint8_t)(memory->terrain->cells[index].reserved>>8);
    return game_memory_read_u8(memory,game_memory_read_u32(memory,0x597d90u)+index*8u+7u);
}
uint8_t gameplay_geometry_terrain_class(GameMemory *memory,float x,float y,float z) {
    long double distance=(long double)y-gameplay_geometry_terrain_height(memory,x,z);
    float stored_distance=(float)distance,value;int32_t ix,iz;uint32_t gx,gz;
    long double tx,tz,a,b,c,d,left,right;
    if(!(distance<4000.0L) || memory->fault)return 0;
    ix=geometry_grid_integer(x);iz=geometry_grid_integer(z);
    gx=((uint32_t)ix&0x1fe00u)>>9;gz=((uint32_t)iz&0x1fe00u)>>9;
    a=geometry_terrain_attribute(memory,gx,gz);b=geometry_terrain_attribute(memory,gx+1,gz);
    d=geometry_terrain_attribute(memory,gx+1,gz+1);c=geometry_terrain_attribute(memory,gx,gz+1);
    /* Retail uses the fractional world coordinate here, not the cell remainder. */
    tx=((long double)x-ix)*0.001953125f;tz=((long double)z-iz)*0.001953125f;
    left=a+(c-a)*tz;right=b+(d-b)*tz;
    value=(float)((left+(right-left)*tx)*0.0625f);
    if((int32_t)geometry_float_bits(stored_distance)>(int32_t)0x453b8000u)
        value=(float)(((long double)stored_distance-3000.0f)*value*0.0010000000474974513f);
    if(!isfinite(value) || value>=2147483648.0f || value< -2147483648.0f)return 0;
    {uint8_t result=(uint8_t)(int32_t)value;return result>4?4:result;}
}

int16_t gameplay_geometry_terrain_cell_height(GameMemory *memory,float x,float z) {
    uint32_t gx=((uint32_t)geometry_grid_integer(x)&0x1fe00u)>>9;
    uint32_t gz=((uint32_t)geometry_grid_integer(z)&0x1fe00u)>>9;
    return (int16_t)geometry_terrain_height_at(memory,
        memory->terrain?0:game_memory_read_u32(memory,0x597d90u),gx,gz);
}

/* Terrain edge scan and painter ordering; packet buffers remain an explicit layout boundary. */
#define TERRAIN_TABLE 0x00597d90u
#define MATERIAL_TABLE 0x00597d94u

static uint32_t world_float_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static int16_t world_dynamic_height(GameMemory *memory,uint32_t target,float x,float z) {
    return gameplay_geometry_wave_height(memory,x,z,target==0x45ad50u);
}

static uint32_t world_emit_vertex(GameMemory *memory, GameplayTerrainFrame *frame,int32_t column,
    int32_t row) {
    uint32_t cursor = frame->clip.vertex;
    uint32_t terrain = game_memory_read_u32(memory, TERRAIN_TABLE) +
        ((uint32_t)column & 255u) * 8u +
        ((uint32_t)row & 255u) * 0x800u;
    TerrainAsset *world = memory->terrain;
    TerrainCell *cell = world ? &world->cells[((uint32_t)row & 255u) * 256u + ((uint32_t)column & 255u)] : NULL;
    int16_t base_height = cell ? cell->height : (int16_t)game_memory_read_u16(memory, terrain);
    uint8_t flags = cell ? cell->flags : game_memory_read_u8(memory, terrain + 5u);
    uint8_t material = cell ? cell->light : game_memory_read_u8(memory, terrain + 4u);
    float x = (float)column * 512.0f;
    float z = (float)row * 512.0f;
    int32_t height = base_height;
    if (flags & 0x10u)
        height += world_dynamic_height(memory, 0x0045ad10u, x, z);
    else if (flags & 0x40u)
        height += world_dynamic_height(memory, 0x0045ad50u, x, z);
    if (memory->fault) return 0;
    {
        RenderArena *arena=render_arena_current();uint32_t offset=cursor-render_arena_view(0);
        if(arena && offset<160000u && offset%16u==0){RenderArenaVertex *vertex=&arena->vertices[offset/16u];
            vertex->position[0]=x;vertex->position[1]=(float)height;vertex->position[2]=z;vertex->light=(float)material;
        }else {
    game_memory_write_u32(memory, cursor, world_float_bits(x));
    game_memory_write_u32(memory, cursor + 4u, world_float_bits((float)height));
    game_memory_write_u32(memory, cursor + 8u, world_float_bits(z));
    game_memory_write_u32(memory, cursor + 12u, world_float_bits((float)material));
        }
    }
    frame->clip.vertex=cursor+16u;
    return cursor;
}

static uint32_t world_cached_or_emit(GameMemory *memory,GameplayTerrainFrame *frame,int32_t column,
    int32_t row, int32_t previous_min, int32_t previous_max,
    uint32_t previous_base) {
    if (column >= previous_min && column <= previous_max)
        return previous_base + (uint32_t)(column - previous_min) * 16u;
    return world_emit_vertex(memory,frame,column,row);
}

typedef struct WorldLink {GameAddress address;GameAddress *local;} WorldLink;
static GameAddress world_link_get(GameMemory *memory,WorldLink link){return link.local?*link.local:game_memory_read_u32(memory,link.address);}
static void world_link_set(GameMemory *memory,WorldLink link,GameAddress value){if(link.local)*link.local=value;else game_memory_write_u32(memory,link.address,value);}

static int world_emit_cell(GameMemory *memory,GameplayTerrainFrame *frame,WorldLink *link,int32_t column,int32_t row,
    uint32_t current_left, uint32_t current_right,
    uint32_t next_left, uint32_t next_right) {
    uint32_t terrain = game_memory_read_u32(memory, TERRAIN_TABLE) +
        ((uint32_t)column & 255u) * 8u +
        ((uint32_t)row & 255u) * 0x800u;
    TerrainAsset *world = memory->terrain;
    uint16_t descriptor = world ? world->cells[((uint32_t)row & 255u) * 256u + ((uint32_t)column & 255u)].tile : game_memory_read_u16(memory, terrain + 2u);
    uint32_t material = game_memory_read_u32(memory, MATERIAL_TABLE) +
        (uint32_t)(descriptor & 0x3fffu) * 8u;
    uint32_t node = frame->node_cursor;
    GameAddress vertices[4]={current_left,current_right,next_left,next_right};

    game_memory_write_u32(memory,node+52u,world_link_get(memory,*link));
    world_link_set(memory,*link,node);link->address=node+52u;link->local=NULL;
    if(memory->fault)return 0;
    project_terrain_packets(memory,vertices,material,
        (uint32_t)((column+row)&1),descriptor>>14,frame);
    return !memory->fault;
}

static int world_pipeline(GameMemory *memory,GameplayTerrainFrame *frame,
    uint32_t matrix,uint32_t render_head) {
    GameplayClipStatus status;float boundaries[4];uint32_t result,tail=0;
    uint32_t begin=frame->vertex_begin,end=frame->clip.vertex;
    float far_value=(float)frame->far_distance-512.0f;
    gameplay_geometry_transform_vertex_stream(memory,matrix,begin,end,begin,0);
    result=gameplay_clipper_near_list_state(memory,render_head,10.0f,far_value,
        &frame->clip,&status);
    frame->render_head=result;
    end=frame->clip.vertex;
    gameplay_geometry_project_vertices(memory,begin,end,
        frame->center_x,frame->center_y,frame->scale,frame->vertical,
        frame->numerator);
    gameplay_geometry_premultiply_uv(memory,result);
    result=gameplay_geometry_filter_backfaces(memory,result,frame->backface_threshold);
    frame->render_head=result;
    memcpy(boundaries,frame->bounds,sizeof(boundaries));
    result=gameplay_clipper_screen_chain_state(memory,result,&tail,boundaries,
        &frame->clip,&status);
    frame->render_head=result;if(memory->fault)return 0;
    if (!result) {
        result = frame->previous_head;
        if (result) frame->render_head=result;
    } else {
        uint32_t old = frame->previous_head;
        if (old && tail) game_memory_write_u32(memory, tail + 12u, old);
    }
    return !memory->fault;
}

int gameplay_geometry_render_terrain_frame(GameMemory *memory,GameAddress matrix,
    int32_t count,float world_x,float world_z,
    GameplayTerrainFrame *frame) {
    GameAddress local_head=0,local_row_head=0;
    if (memory->fault || !frame) return 0;
    if (count < 0 || count > 8) { memory->fault=1; return 0; }
    if (count > 1) {
        int32_t camera_column = (int32_t)((long double)world_x /
            512.0L);
        int32_t camera_row = (int32_t)((long double)world_z / 512.0L);
        unsigned high_index = 0u;
        unsigned low_index = 1u;
        unsigned next_index = 2u;
        GameplayTerrainEdge high = frame->edges[high_index];
        GameplayTerrainEdge low = frame->edges[low_index];
        int32_t high_limit;
        int32_t low_limit;
        int32_t previous_min = 1;
        int32_t previous_max = -1;
        uint32_t previous_base = 0;
        WorldLink link_address={0,&local_head};
        int32_t maximum_row;
        int32_t row;
        if (high.column_16_16 < low.column_16_16) {
            GameplayTerrainEdge temporary = high;
            unsigned temporary_index = high_index;
            high = low; low = temporary;
            high_index = low_index; low_index = temporary_index;
        }
        high_limit = high.first_column;
        low_limit = low.first_column;
        maximum_row = frame->edges[count-1].last_row;
        row = high.first_row;
        while (!memory->fault && row <= maximum_row) {
            int32_t high_column = high.column_16_16 >> 16;
            int32_t low_column = low.column_16_16 >> 16;
            int32_t maximum_column = high_limit > high_column ?
                high_limit : high_column;
            int32_t minimum_column = low_column < low_limit ?
                low_column : low_limit;
            int32_t cache_minimum = previous_min +
                (minimum_column < previous_min ? 1 : 0);
            uint32_t cache_base = previous_base +
                (uint32_t)(cache_minimum - previous_min) * 16u;
            int32_t next_row = row + 1;
            uint32_t next_base = frame->clip.vertex;
            uint32_t current_left;
            uint32_t next_left;
            WorldLink reverse_tail={0,NULL};
            WorldLink row_link;
            int32_t column;
            /* Retail prebuilds the complete next-row span even though the
               camera split may shorten the first triangulation phase. */
            for (column = minimum_column; column <= maximum_column + 1;
                column++)
                world_emit_vertex(memory,frame,column,next_row);
            current_left = world_cached_or_emit(memory,frame,minimum_column,row,
                cache_minimum, previous_max, cache_base);
            next_left = next_base;
            local_row_head=0;
            row_link.address=0;row_link.local=&local_row_head;
            for (column = minimum_column; column <= maximum_column; column++) {
                uint32_t current_right = world_cached_or_emit(memory,frame,column+1,
                    row, cache_minimum, previous_max, cache_base);
                uint32_t next_right = next_base +
                    (uint32_t)(column + 1 - minimum_column) * 16u;
                WorldLink insertion = row_link;
                if (!world_emit_cell(memory,frame,&insertion,column,row,
                        current_left, current_right, next_left, next_right))
                    return 0;
                if (column < camera_column) {
                    row_link = insertion;
                } else if (!reverse_tail.address) {
                    /* Cells on the far side of the camera are called in
                       ascending order but inserted at one fixed link, which
                       gives the painter list its descending column order. */
                    reverse_tail = insertion;
                }
                current_left = current_right;
                next_left = next_right;
            }
            if (reverse_tail.address) row_link = reverse_tail;
            if (local_row_head) {
                /* Insert at the camera split, not at the overall head. Far
                   rows reuse this link; near rows advance it to their tail. */
                world_link_set(memory,row_link,
                    world_link_get(memory,link_address));
                world_link_set(memory,link_address,local_row_head);
                if (row < camera_row) link_address = row_link;
            }
            previous_min = minimum_column;
            previous_max = maximum_column + 1;
            previous_base = next_base;
            if (next_row < high.last_row) {
                high.column_16_16 += high.column_step_16_16;
                frame->edges[high_index].column_16_16=high.column_16_16;
                frame->edge_column_changed[high_index]=1;
                high_limit = high_column;
            } else {
                int32_t old = high_column;
                int32_t old_last = high.last_column;
                if(next_index>=10u){memory->fault=1;return 0;}
                high = frame->edges[next_index];
                high_index = next_index++;
                high_limit = old_last < old ? old : old_last;
            }
            if (next_row < low.last_row) {
                low.column_16_16 += low.column_step_16_16;
                frame->edges[low_index].column_16_16=low.column_16_16;
                frame->edge_column_changed[low_index]=1;
                low_limit = low_column;
            } else {
                int32_t old = low_column;
                int32_t old_last = low.last_column;
                if(next_index>=10u){memory->fault=1;return 0;}
                low = frame->edges[next_index];
                low_index = next_index++;
                low_limit = old_last > old ? old : old_last;
            }
            if (high.column_16_16 < low.column_16_16) {
                GameplayTerrainEdge temporary = high;
                unsigned temporary_index = high_index;
                high = low; low = temporary;
                high_index = low_index; low_index = temporary_index;
            }
            row = next_row;
        }
        if (!world_pipeline(memory,frame,matrix,local_head)) return 0;
    }
    return !memory->fault;
}

/* Connected model pipeline. Head/tail are native locals; no guest callback
   stack or emulated FPU state is needed between stages. */
static GameplayRenderFrame geometry_legacy_render_frame(GameMemory *memory){
    GameplayRenderFrame state={0};
    state.clip_vertices=0x604378u;state.clip_packets=0x604268u;
    state.clip_uv=0x604480u;state.clip_indices=0x604270u;
    state.queue_head=0x604484u;
    state.center_x=geometry_read_f32(memory,0x6290c8u);
    state.center_y=geometry_read_f32(memory,0x6290ccu);
    state.scale=geometry_read_f32(memory,0x6290f4u);
    state.vertical=geometry_read_f32(memory,0x6290f0u);
    state.numerator=geometry_read_f32(memory,0x574cb0u);
    state.backface_threshold=geometry_read_f32(memory,0x574cb4u);
    state.bounds[0]=geometry_read_f32(memory,0x6290d4u);
    state.bounds[1]=geometry_read_f32(memory,0x6290d0u);
    state.bounds[2]=geometry_read_f32(memory,0x6290dcu);
    state.bounds[3]=geometry_read_f32(memory,0x6290d8u);
    return state;
}
static int geometry_submit_mesh(GameMemory *memory,const GameplayMeshSubmission *mesh,
    const float *matrix,const GameplayRenderFrame *state,const AssetMeshVertex *native_vertices,
    const GameplayClipPolygonInput *clip_inputs,SceneSurface *surface) {
    GameAddress head,tail=0,polygon,clip_begin,clip_end;
    uint32_t texture=mesh->texture_state,material=mesh->material_state;
    GameplayClipStatus status={0};
    if(memory->fault || mesh->vertex_count<0 || mesh->vertex_count>65536)return 0;
    if(matrix)geometry_transform_packed_values(memory,matrix,mesh->source_vertices,
        mesh->source_vertices+(uint32_t)mesh->vertex_count*12u,mesh->transformed_vertices,0,0,native_vertices,surface);
    else gameplay_geometry_transform_packed_stream(memory,mesh->matrix,mesh->source_vertices,
        mesh->source_vertices+(uint32_t)mesh->vertex_count*12u,mesh->transformed_vertices,0,0);
    if(memory->fault)return 0;
    clip_begin=state->cursors?state->cursors->vertex:
        game_memory_read_u32(memory,state->clip_vertices);
    head=state->cursors?
        gameplay_clipper_clip_polygon_inputs_state(memory,mesh->polygons,
            mesh->polygon_count,clip_inputs,10.0f,mesh->far_depth,state->cursors):
        gameplay_clipper_clip_polygon_inputs(memory,mesh->polygons,mesh->polygon_count,
            clip_inputs,10.0f,mesh->far_depth,state->clip_vertices,state->clip_packets,
            state->clip_uv,state->clip_indices);
    clip_end=state->cursors?state->cursors->vertex:
        game_memory_read_u32(memory,state->clip_vertices);
    if((mesh->flags&0x200u) && !texture)texture=0x604380u;
    if((mesh->flags&0xc0u) && !material)material=0x604278u;
    for(polygon=head;polygon && !memory->fault;) {
        RenderArenaPacket *packet=geometry_packet_view(memory,polygon);
        uint32_t flags=packet?packet->flags:game_memory_read_u32(memory,polygon+8);
        flags=(flags&0x500u)|(mesh->flags&0xffffffdfu);
        if(packet){
            if(scene_projection_storage_packet(polygon)){
                if(!scene_projection_storage_set_packet_flags(polygon,flags))
                    memory->fault=1;
            }else packet->flags=flags;
            packet->extra[2]=texture;packet->extra[3]=material;
            polygon=geometry_packet_next(memory,polygon,packet);
        }
        else {game_memory_write_u32(memory,polygon+8,flags);game_memory_write_u32(memory,polygon+32,texture);
            game_memory_write_u32(memory,polygon+36,material);polygon=game_memory_read_u32(memory,polygon+12);}
    }
    if(memory->fault || !geometry_project_vertices(memory,mesh->transformed_vertices,
        mesh->transformed_vertices+(uint32_t)mesh->vertex_count*16u,state->center_x,state->center_y,state->scale,state->vertical,state->numerator,surface) ||
        !gameplay_geometry_project_vertices(memory,clip_begin,clip_end,state->center_x,state->center_y,state->scale,state->vertical,state->numerator))return 0;
    gameplay_geometry_premultiply_uv(memory,head);
    head=gameplay_geometry_filter_backfaces(memory,head,state->backface_threshold);
    head=state->cursors?
        gameplay_clipper_screen_chain_state(memory,head,&tail,state->bounds,
            state->cursors,&status):
        gameplay_clipper_screen_chain(memory,head,&tail,state->bounds,
            state->clip_vertices,state->clip_packets,state->clip_uv,state->clip_indices,&status);
    if(head && !memory->fault) {
        RenderArenaPacket *packet=geometry_packet_view(memory,tail);
        GameAddress previous=state->queue_head_value?*state->queue_head_value:
            game_memory_read_u32(memory,state->queue_head);
        if(packet){if(!render_packet_set_next(tail,previous))packet->next=previous;}
        else game_memory_write_u32(memory,tail+12,previous);
        if(state->queue_head_value)*state->queue_head_value=head;
        else game_memory_write_u32(memory,state->queue_head,head);
    }
    return !memory->fault;
}

int gameplay_geometry_submit_mesh(GameMemory *memory,const GameplayMeshSubmission *mesh) {
    GameplayRenderFrame state=geometry_legacy_render_frame(memory);
    return geometry_submit_mesh(memory,mesh,NULL,&state,NULL,NULL,NULL);
}
int gameplay_geometry_submit_mesh_frame(GameMemory *memory,const GameplayMeshSubmission *mesh,
    const GameplayRenderFrame *state){return state?geometry_submit_mesh(memory,mesh,NULL,state,NULL,NULL,NULL):0;}
int gameplay_geometry_submit_scene_mesh(GameMemory *memory,const GameplayMeshSubmission *mesh,
    const GameplayRenderFrame *state,const float matrix[12],const AssetMeshVertex *vertices,
    const GameplayClipPolygonInput *clip_inputs){
    return state && matrix?geometry_submit_mesh(memory,mesh,matrix,state,vertices,clip_inputs,NULL):0;
}
static int geometry_project_visible_vertices(GameMemory *memory,
    GameAddress begin,int32_t count,const GameplayRenderFrame *state){
    int32_t index;
    for(index=0;index<count && !memory->fault;index++){
        GameAddress address=begin+(uint32_t)index*16u;
        RenderArenaVertex *vertex=render_arena_vertices(address,1u);
        if(!vertex)vertex=scene_projection_storage_transformed(address,1u);
        float x=vertex?vertex->position[0]:geometry_read_f32(memory,address);
        float y=vertex?vertex->position[1]:geometry_read_f32(memory,address+4u);
        float z=vertex?vertex->position[2]:geometry_read_f32(memory,address+8u);
        float reciprocal=geometry_divide(state->numerator,z);
        float screen_x=geometry_subtract(state->center_x,
            geometry_multiply(geometry_multiply(x,state->scale),reciprocal));
        float screen_y=geometry_subtract(state->center_y,
            geometry_multiply(geometry_multiply(
                geometry_multiply(y,state->vertical),state->scale),reciprocal));
        if(vertex){vertex->position[0]=screen_x;vertex->position[1]=screen_y;
            vertex->light=reciprocal;}
        else {geometry_write_f32(memory,address,screen_x);
            geometry_write_f32(memory,address+4u,screen_y);
            geometry_write_f32(memory,address+12u,reciprocal);}
    }
    return !memory->fault;
}
int gameplay_geometry_project_vertices(GameMemory *memory,
    GameAddress begin,GameAddress end,float center_x,float center_y,
    float scale,float vertical_scale,float numerator){
    return geometry_project_vertices(memory,begin,end,center_x,center_y,
        scale,vertical_scale,numerator,NULL);
}
/* The alternate scene branch selects complete batches before the shared
 * projection and screen pipeline. Its packet list is already constructed by
 * the scene owner, so it does not allocate near-plane clip polygons. */
int gameplay_geometry_submit_visible_scene_mesh(GameMemory *memory,
    const GameplayMeshSubmission *mesh,const GameplayRenderFrame *state,
    const float matrix[12],const AssetMeshVertex *vertices,
    GameAddress batches,int32_t batch_count,
    const ScenePolygon *native_polygons){
    GameAddress head,node,tail=0;
    GameplayGeometryTransformResult transformed;
    GameplayClipStatus status={0};
    if(!memory || !mesh || !state || !matrix || mesh->vertex_count<0 ||
       mesh->vertex_count>65536 || batch_count<0 || batch_count>65536)
        return 0;
    transformed=geometry_transform_packed_values(memory,matrix,mesh->source_vertices,
        mesh->source_vertices+(uint32_t)mesh->vertex_count*12u,
        mesh->transformed_vertices,0,0,vertices,NULL);
    if(memory->fault)return 0;
    if(native_polygons){
        int32_t index;
        head=0;
        for(index=0;index<batch_count && !memory->fault;index++){
            GameAddress packet=batches+(uint32_t)index*40u;
            unsigned corner;int visible=1;
            for(corner=0;corner<3u;corner++){
                uint32_t vertex=native_polygons[index].vertex[corner];
                float depth;
                RenderArenaVertex *owned_vertex;
                if(vertex>=(uint32_t)mesh->vertex_count){memory->fault=1;break;}
                owned_vertex=scene_projection_storage_transformed(
                    mesh->transformed_vertices+vertex*16u,1u);
                depth=owned_vertex?owned_vertex->position[2]:
                    geometry_read_f32(memory,mesh->transformed_vertices+
                        vertex*16u+8u);
                if(!(depth>=10.0f)){visible=0;break;}
            }
            if(memory->fault)break;
            if(visible){
                RenderArenaPacket *view=geometry_packet_view(memory,packet);
                if(view){if(!render_packet_set_next(packet,head))view->next=head;}
                else game_memory_write_u32(memory,packet+12u,head);
                head=packet;
            }
        }
    }else head=gameplay_geometry_select_visible_batches(memory,batches,
        batch_count,10.0f);
    for(node=head;node && !memory->fault;){
        RenderArenaPacket *packet=geometry_packet_view(memory,node);
        GameAddress next=geometry_packet_next(memory,node,packet);
        uint32_t flags=packet?packet->flags:game_memory_read_u32(memory,node+8u);
        flags=(flags&0x400u)|0x119u;
        if(packet){
            if(scene_projection_storage_packet(node)){
                if(!scene_projection_storage_set_packet_flags(node,flags))
                    memory->fault=1;
            }else packet->flags=flags;
            packet->extra[2]=transformed.ebx;
            packet->extra[3]=transformed.ebx;}
        else {game_memory_write_u32(memory,node+8u,flags);
            game_memory_write_u32(memory,node+32u,transformed.ebx);
            game_memory_write_u32(memory,node+36u,transformed.ebx);}
        node=next;
    }
    if(memory->fault || !geometry_project_visible_vertices(memory,
        mesh->transformed_vertices,mesh->vertex_count,state))return 0;
    head=gameplay_geometry_filter_backfaces(memory,head,state->backface_threshold);
    head=state->cursors?gameplay_clipper_screen_chain_state(memory,head,&tail,
        state->bounds,state->cursors,&status):
        gameplay_clipper_screen_chain(memory,head,&tail,state->bounds,
            state->clip_vertices,state->clip_packets,state->clip_uv,
            state->clip_indices,&status);
    if(head && !memory->fault){
        RenderArenaPacket *packet=geometry_packet_view(memory,tail);
        GameAddress previous=state->queue_head_value?*state->queue_head_value:
            game_memory_read_u32(memory,state->queue_head);
        if(packet){if(!render_packet_set_next(tail,previous))packet->next=previous;}
        else game_memory_write_u32(memory,tail+12u,previous);
        if(state->queue_head_value)*state->queue_head_value=head;
        else game_memory_write_u32(memory,state->queue_head,head);
    }
    return !memory->fault;
}

static int32_t geometry_truncate_screen(float value){
    if(!isfinite(value) || value>=(float)INT32_MAX ||
       value<(float)INT32_MIN)return INT32_MIN;
    return (int32_t)value;
}

/* The visible-scene queue is shared by model submission and the later draw
 * sweep. Bit 8 marks a packet whose vertices span all four view boundaries;
 * the x86 view table remains an external frame input for now. */
int gameplay_geometry_classify_visible_queue(GameMemory *memory,GameAddress head,
    GameAddress view_begin,GameAddress view_end){
    GameAddress packet_address;
    uint32_t guard=0;
    if(!memory || view_end<view_begin || ((view_end-view_begin)&7u))return 0;
    for(packet_address=head;packet_address && !memory->fault;){
        RenderArenaPacket *packet=geometry_packet_view(memory,packet_address);
        GameAddress next=geometry_packet_next(memory,packet_address,packet);
        uint32_t flags=packet?packet->flags:
            game_memory_read_u32(memory,packet_address+8u);
        if(++guard>65536u){memory->fault=1;break;}
        if((flags&0x40000000u) && view_begin!=view_end){
            int32_t count=(int32_t)(packet?packet->count:
                game_memory_read_u32(memory,packet_address));
            GameAddress links=packet?packet->vertices:
                game_memory_read_u32(memory,packet_address+4u);
            GameAddress rect;int accepted=0;
            if(count>65536){memory->fault=1;break;}
            for(rect=view_begin;rect<view_end && !memory->fault;rect+=8u){
                int32_t y_min=(int16_t)game_memory_read_u16(memory,rect);
                int32_t x_min=(int16_t)game_memory_read_u16(memory,rect+2u);
                int32_t y_max=(int16_t)game_memory_read_u16(memory,rect+4u);
                int32_t x_max=(int16_t)game_memory_read_u16(memory,rect+6u);
                uint32_t sides=0,index;
                if(memory->fault)break;
                for(index=0;index<(count>0?(uint32_t)count:0u) &&
                    sides!=15u && !memory->fault;index++){
                    GameAddress vertex=game_memory_read_u32(memory,links+index*4u);
                    int32_t x=geometry_truncate_screen(geometry_read_f32(memory,vertex));
                    int32_t y=geometry_truncate_screen(geometry_read_f32(memory,vertex+4u));
                    if(x>=x_min)sides|=1u;
                    if(x<=x_max)sides|=2u;
                    if(y>=y_min)sides|=4u;
                    if(y<=y_max)sides|=8u;
                }
                if(sides==15u){accepted=1;break;}
            }
            if(accepted && !memory->fault){
                flags|=8u;
                if(packet && scene_projection_storage_packet(packet_address)){
                    if(!scene_projection_storage_set_packet_flags(packet_address,
                        flags))memory->fault=1;
                }else if(packet)packet->flags=flags;
                else game_memory_write_u32(memory,packet_address+8u,flags);
            }
        }
        packet_address=next;
    }
    return !memory->fault;
}

static int geometry_owned_visible_link(GameAddress links,uint32_t index,
    GameAddress *vertex){
    uint32_t *arena=render_arena_indices(links+index*4u,1u);
    if(arena){*vertex=*arena;return 1;}
    return scene_projection_storage_read(links+index*4u,vertex,4u);
}
static int geometry_owned_visible_point(GameAddress address,float *x,float *y){
    RenderArenaVertex *arena=render_arena_vertices(address,1u);
    uint32_t bits[2];
    if(arena){*x=arena->position[0];*y=arena->position[1];return 1;}
    if(!scene_projection_storage_read(address,bits,sizeof(bits)))return 0;
    memcpy(x,&bits[0],4u);memcpy(y,&bits[1],4u);
    return 1;
}
static GameAddress geometry_owned_packet_next(GameAddress address,
    RenderArenaPacket *packet,RenderArenaPacket **next_owned){
    GameAddress next;
    if(render_packet_link(address,&next,next_owned))return next;
    *next_owned=NULL;
    return packet->next;
}
int gameplay_geometry_classify_owned_visible_queue(GameAddress head,
    const GameplayViewRect *views,size_t view_count){
    GameAddress address;uint32_t guard=0;
    RenderArenaPacket *owned_packet=NULL;
    if(view_count && !views)return -1;
    /* Preflight the entire queue so an external link can use the unchanged
     * legacy path without partially classifying owned packets. */
    for(address=head;address;){
        RenderArenaPacket *packet=owned_packet?owned_packet:render_packet_view(address);
        uint32_t index;int32_t count;
        if(++guard>65536u)return -1;
        if(!packet)return 0;
        if((packet->flags&0x40000000u) && view_count){
            count=(int32_t)packet->count;
            if(count>65536)return -1;
            for(index=0;count>0 && index<(uint32_t)count;index++){
                GameAddress vertex;float x,y;
                if(!geometry_owned_visible_link(packet->vertices,index,&vertex) ||
                   !geometry_owned_visible_point(vertex,&x,&y))return 0;
            }
        }
        address=geometry_owned_packet_next(address,packet,&owned_packet);
    }
    guard=0;owned_packet=NULL;
    for(address=head;address;){
        RenderArenaPacket *packet=owned_packet?owned_packet:render_packet_view(address);
        GameAddress next=geometry_owned_packet_next(address,packet,&owned_packet);
        uint32_t flags=packet->flags;
        size_t rect;
        if(++guard>65536u)return -1;
        if((flags&0x40000000u) && view_count){
            int accepted=0;
            int32_t count=(int32_t)packet->count;
            for(rect=0;rect<view_count;rect++){
                uint32_t sides=0,index;
                for(index=0;count>0 && index<(uint32_t)count &&
                    sides!=15u;index++){
                    GameAddress vertex;float fx,fy;int32_t x,y;
                    if(!geometry_owned_visible_link(packet->vertices,index,&vertex) ||
                       !geometry_owned_visible_point(vertex,&fx,&fy))return -1;
                    x=geometry_truncate_screen(fx);
                    y=geometry_truncate_screen(fy);
                    if(x>=views[rect].x_min)sides|=1u;
                    if(x<=views[rect].x_max)sides|=2u;
                    if(y>=views[rect].y_min)sides|=4u;
                    if(y<=views[rect].y_max)sides|=8u;
                }
                if(sides==15u){accepted=1;break;}
            }
            if(accepted){
                flags|=8u;
                if(scene_projection_storage_packet(address)){
                    if(!scene_projection_storage_set_packet_flags(address,flags))
                        return -1;
                }else packet->flags=flags;
            }
        }
        address=next;
    }
    return 1;
}
static RenderArenaPacket *surface_queue_packet(SceneSurface *surface,
    GameAddress address){
    if(surface && address>=surface->address[0] &&
       address-surface->address[0]<sizeof(surface->polygons) &&
       (address-surface->address[0])%sizeof(RenderArenaPacket)==0u)
        return &surface->polygons[(address-surface->address[0])/
            sizeof(RenderArenaPacket)];
    return render_packet_view(address);
}
static uint32_t surface_queue_index(GameMemory *memory,SceneSurface *surface,
    GameAddress address){
    uint32_t *index;
    uint32_t value;
    if(surface && address>=surface->address[3] &&
       address-surface->address[3]<sizeof(surface->links) &&
       (address-surface->address[3])%4u==0u)
        return ((uint32_t *)surface->links)[(address-surface->address[3])/4u];
    index=render_arena_indices(address,1u);
    if(index)return *index;
    if(surface && memory->surface &&
       render_arena_read(address,&value,sizeof(value)))return value;
    return game_memory_read_u32(memory,address);
}
static float surface_queue_component(GameMemory *memory,SceneSurface *surface,
    GameAddress address){
    float *component;
    if(surface && address>=surface->address[2] &&
       address-surface->address[2]<sizeof(surface->transformed) &&
       (address-surface->address[2])%4u==0u){
        float value;
        memcpy(&value,(const uint8_t *)surface->transformed+
            address-surface->address[2],4u);
        return value;
    }
    if(surface && address>=surface->address[4] &&
       address-surface->address[4]<sizeof(surface->uv) &&
       (address-surface->address[4])%4u==0u){
        float value;
        memcpy(&value,(const uint8_t *)surface->uv+
            address-surface->address[4],4u);
        return value;
    }
    component=render_arena_float_component(address);
    return component?*component:geometry_read_f32(memory,address);
}
static int surface_owned_index(SceneSurface *surface,GameAddress address,
    GameAddress *value,const GameplaySurfaceStorageOps *storage){
    const GameAddress *link=scene_surface_link(surface,address);
    uint32_t *index;
    if(link){*value=*link;return 1;}
    index=render_arena_indices(address,1u);
    if(index){*value=*index;return 1;}
    if(storage && storage->index){
        const uint32_t *owned=storage->index(storage->context,address);
        if(owned){*value=*owned;return 1;}
    }
    if(render_arena_read(address,value,sizeof(*value)))return 1;
    return storage && storage->read &&
        storage->read(storage->context,address,value,sizeof(*value));
}
static int surface_owned_component(SceneSurface *surface,GameAddress address,
    float *value,const GameplaySurfaceStorageOps *storage){
    float *component=scene_surface_transformed_component(surface,address);
    if(component){*value=*component;return 1;}
    if(address>=surface->address[4] &&
       address-surface->address[4]<sizeof(surface->uv) &&
       scene_surface_uv_component(surface,address,value))return 1;
    component=render_arena_float_component(address);
    if(component){*value=*component;return 1;}
    if(storage && storage->component){
        const float *owned=storage->component(storage->context,address);
        if(owned){*value=*owned;return 1;}
    }
    return storage && storage->read &&
        storage->read(storage->context,address,value,sizeof(*value));
}
static int surface_draw_owned_queue(SceneSurface *surface,GameAddress node,
    float magic,GameplaySurfacePolygon draw,void *context,
    const GameplaySurfaceStorageOps *storage){
    uint32_t guard=0;
    while(node){
        const RenderArenaPacket *packet=surface_queue_packet(surface,node);
        RenderArenaPacket stored;
        GameRenderVertex vertices[32]={{0}};
        unsigned i,count;
        if(!packet && storage && storage->packet)
            packet=storage->packet(storage->context,node);
        if(!packet && storage && storage->read &&
           storage->read(storage->context,node,&stored,sizeof(stored)))
            packet=&stored;
        if(!packet || ++guard>65536u)return 0;
        count=packet->count;
        if(count<3u || count>32u)return 0;
        if(scene_surface_packet(surface,node))++surface->native_queue_packets;
        ++surface->typed_queue_packets;
        for(i=0;i<count;i++){
            GameAddress point;
            float x,y,z,s,t;
            volatile float rounded;
            if(!surface_owned_index(surface,packet->vertices+i*4u,&point,storage) ||
               !surface_owned_component(surface,point,&x,storage) ||
               !surface_owned_component(surface,point+4u,&y,storage) ||
               !surface_owned_component(surface,point+8u,&z,storage) ||
               !surface_owned_component(surface,packet->attributes+i*8u,&s,storage) ||
               !surface_owned_component(surface,packet->attributes+i*8u+4u,&t,storage))
                return 0;
            rounded=x+magic;vertices[i].x=rounded-magic;
            rounded=y+magic;vertices[i].y=rounded-magic;
            vertices[i].oow=z;
            vertices[i].sow0=s;vertices[i].tow0=t;
        }
        if(!draw(context,vertices,count))return 0;
        node=packet->next;
    }
    return 1;
}
static int geometry_render_surface_frame_impl(GameMemory *memory,const GameAddress tables[5],
    const GameplayRenderFrame *state,const SceneSurfaceAnimationInput *animation,
    GameplaySurfacePolygon draw,void *context,SceneSurface *owned,
    const GameplaySurfaceStorageOps *storage) {
    GameAddress polygons=tables[0],source=tables[1];
    GameAddress transformed=tables[2],indices=tables[3],uv=tables[4];
    float phase,period;
    float camera[3],u,v,u1,v1,matrix[12]={0},view[12],composed[12];unsigned i,row,column;GameAddress node;
    long double advanced;
    GameplayMeshSubmission mesh={0};
    GameplayClipPolygonInput native_inputs[25];
    int native_surface=owned && owned->links_clean;
    if(native_surface)for(i=0;i<5u;i++)
        if(tables[i]!=owned->address[i]){native_surface=0;break;}
    if(memory->fault)return 0;
    if(native_surface){
        if(!animation || !scene_surface_animate(owned,animation,matrix,&phase))return 0;
    }else{
    phase=geometry_read_f32(memory,0x59611cu);
    period=geometry_read_f32(memory,0x574a24u);
    advanced=(long double)phase+geometry_read_f32(memory,0x574a20u);
    phase=(float)advanced;if(advanced>period)phase=(float)((long double)phase-period);
    geometry_write_f32(memory,0x59611cu,phase);
    for(i=0;i<3;i++)camera[i]=geometry_read_f32(memory,0x629088u+i*4);
    {float factor=geometry_read_f32(memory,0x574a28u),scale=geometry_read_f32(memory,0x574a34u);
     float gx=(float)((long double)camera[0]*factor),gz=(float)((long double)camera[2]*factor);
     int32_t ix=(int32_t)gx,iz=(int32_t)gz;if(gx<0)--ix;if(gz<0)--iz;
     u=(float)(((long double)ix*geometry_read_f32(memory,0x574a30u)+camera[0])*scale);
     v=(float)(((long double)iz*geometry_read_f32(memory,0x574a38u)+camera[2])*scale+phase);
     u=(float)((long double)u+phase);u1=(float)((long double)u+period);v1=(float)((long double)v+period);
    }
    for(i=0;i<25;i++){
        const float values[8]={u,v,u1,v,u1,v1,u,v1};
        game_memory_write_u32(memory,polygons+i*40,4);
        game_memory_write_u32(memory,polygons+i*40+4,indices+i*16);
        game_memory_write_u32(memory,polygons+i*40+16,uv+i*32);
        for(column=0;column<8;column++)
            geometry_write_f32(memory,uv+i*32+column*4,values[column]);
    }
    {float origin=geometry_read_f32(memory,0x574a3cu),step=geometry_read_f32(memory,0x574a44u);
     float x0=(float)((long double)camera[0]-origin),z=(float)((long double)camera[2]-origin);
     float y=(float)((long double)camera[1]+geometry_read_f32(memory,0x574a40u));
     for(row=0;row<6;row++) {float x=x0;for(column=0;column<6;column++){
        GameAddress vertex=source+(row*6+column)*12;
        geometry_write_f32(memory,vertex,x);
        geometry_write_f32(memory,vertex+4,y);
        geometry_write_f32(memory,vertex+8,z);
        x=(float)((long double)x+step);
     }z=(float)((long double)z+step);}
    }
    {uint32_t player=game_memory_read_u32(memory,0x58f890u);uint16_t yaw=game_memory_read_u16(memory,player+10);
     long double c=geometry_read_f32(memory,0x6298a8u+((yaw+512u)&2047u)*4),s=geometry_read_f32(memory,0x6298a8u+(yaw&2047u)*4);
     long double cn=geometry_read_f32(memory,0x6298a8u+((0u-yaw+512u)&2047u)*4),sn=geometry_read_f32(memory,0x6298a8u+((0u-yaw)&2047u)*4);
     long double p=geometry_read_f32(memory,0x629a38u),q=geometry_read_f32(memory,0x62a238u);
     float ns=(float)-s,qs=(float)(q*s),qc=(float)(q*c),nsn=(float)-sn;
     matrix[0]=(float)((long double)qs*nsn+cn*c);matrix[1]=(float)((long double)ns*cn+(long double)qc*nsn);
     matrix[2]=(float)(-p*nsn);matrix[4]=(float)((long double)qs*cn+sn*c);matrix[5]=(float)((long double)qc*cn+(long double)ns*sn);
     matrix[6]=(float)(cn*-p);matrix[8]=(float)(s*p);matrix[9]=(float)(p*c);matrix[10]=(float)q;
    }
    }
    if(native_surface)memcpy(view,animation->view,sizeof(view));
    else for(i=0;i<12;i++)view[i]=geometry_read_f32(memory,0x5cedf0u+i*4);
    game_matrix_compose_values(composed,matrix,view);
    if(state->queue_head_value)*state->queue_head_value=0;
    else game_memory_write_u32(memory,state->queue_head,0);
    mesh.source_vertices=source;mesh.transformed_vertices=transformed;mesh.polygons=polygons;
    mesh.vertex_count=36;mesh.polygon_count=25;mesh.flags=5;
    mesh.far_depth=native_surface?animation->far_depth:
        (float)((long double)(int32_t)game_memory_read_u32(memory,0x597f7cu)-geometry_read_f32(memory,0x574a4cu));
    if(native_surface && !memory->fault){
        memset(native_inputs,0,sizeof(native_inputs));
        for(i=0;i<25u;i++){
            uint32_t offsets[4],corner,row=i/5u,column=i%5u;
            GameplayClipPolygonInput *input=&native_inputs[i];
            offsets[0]=row*96u+column*16u;
            offsets[1]=offsets[0]+16u;
            offsets[2]=offsets[0]+112u;
            offsets[3]=offsets[0]+96u;
            input->count=4;
            input->flags=owned->polygons[i].flags;
            for(corner=0;corner<4u;corner++){
                float *pair=input->uv[corner];
                uint32_t scalar_bits=(corner&1u)?
                    owned->uv[i][corner/2u].v_bits:
                    owned->uv[i][corner/2u].u_bits;
                input->vertices[corner]=transformed+offsets[corner];
                memcpy(&pair[0],&owned->uv[i][corner].u_bits,4);
                memcpy(&pair[1],&owned->uv[i][corner].v_bits,4);
                memcpy(&input->scalars[corner],&scalar_bits,4);
            }
        }
    }
    if(memory->fault || !geometry_submit_mesh(memory,&mesh,composed,state,
        native_surface?owned->source:NULL,
        native_surface?native_inputs:NULL,native_surface?owned:NULL))return 0;
    if(native_surface)owned->native_submissions++;
    node=state->queue_head_value?*state->queue_head_value:
        game_memory_read_u32(memory,state->queue_head);
    if(native_surface && !memory->fault &&
       (gameplay_geometry_owned_surface_ready(owned,state) || storage))
        return surface_draw_owned_queue(owned,node,animation->rounding_magic,
            draw,context,storage);
    while(node && !memory->fault){
        RenderArenaPacket *packet=surface_queue_packet(native_surface?owned:NULL,node);
        unsigned count=packet?packet->count:game_memory_read_u32(memory,node);
        GameAddress points=packet?packet->vertices:game_memory_read_u32(memory,node+4u);
        GameAddress attrs=packet?packet->attributes:game_memory_read_u32(memory,node+16u);
        GameRenderVertex vertices[32]={{0}};
        float magic=native_surface?animation->rounding_magic:
            geometry_read_f32(memory,0x574a18u);
        if(native_surface && packet && node>=owned->address[0] &&
           node-owned->address[0]<sizeof(owned->polygons))
            ++owned->native_queue_packets;
        if(count<3 || count>32)return 0;
        for(i=0;i<count;i++){
            GameAddress point=surface_queue_index(memory,native_surface?owned:NULL,
                points+i*4u);volatile float rounded;
            rounded=surface_queue_component(memory,native_surface?owned:NULL,
                point)+magic;rounded=rounded-magic;vertices[i].x=rounded;
            rounded=surface_queue_component(memory,native_surface?owned:NULL,
                point+4u)+magic;rounded=rounded-magic;vertices[i].y=rounded;
            vertices[i].oow=surface_queue_component(memory,native_surface?owned:NULL,
                point+8u);
            vertices[i].sow0=surface_queue_component(memory,native_surface?owned:NULL,
                attrs+i*8u);
            vertices[i].tow0=surface_queue_component(memory,native_surface?owned:NULL,
                attrs+i*8u+4u);
        }
        if(memory->fault || !draw(context,vertices,count))return 0;
        packet=surface_queue_packet(native_surface?owned:NULL,node);
        node=packet?packet->next:game_memory_read_u32(memory,node+12u);
    }
    return !memory->fault;
}

int gameplay_geometry_render_surface_frame(GameMemory *memory,const GameAddress tables[5],
    const GameplayRenderFrame *state,const SceneSurfaceAnimationInput *animation,
    GameplaySurfacePolygon draw,void *context,SceneSurface *owned) {
    return geometry_render_surface_frame_impl(memory,tables,state,animation,
        draw,context,owned,NULL);
}

int gameplay_geometry_render_surface_tables(GameMemory *memory,const GameAddress tables[5],
    GameplaySurfacePolygon draw,void *context){
    GameplayRenderFrame state=geometry_legacy_render_frame(memory);
    return gameplay_geometry_render_surface_frame(memory,tables,&state,NULL,draw,context,NULL);
}
int gameplay_geometry_owned_surface_ready(const SceneSurface *surface,
    const GameplayRenderFrame *frame){
    uint8_t probe;
    if(!surface || !surface->active || !surface->links_clean || !frame ||
       !frame->cursors || !frame->queue_head_value)return 0;
    return (frame->cursors->vertex&0xfff00000u)==render_arena_view(0) &&
        (frame->cursors->packet&0xfff00000u)==render_arena_view(3) &&
        (frame->cursors->uv&0xfff00000u)==render_arena_view(2) &&
        (frame->cursors->scalar&0xfff00000u)==render_arena_view(1) &&
        render_arena_read(frame->cursors->vertex,&probe,1u) &&
        render_arena_read(frame->cursors->packet,&probe,1u) &&
        render_arena_read(frame->cursors->uv,&probe,1u) &&
        render_arena_read(frame->cursors->scalar,&probe,1u);
}
int gameplay_geometry_render_owned_surface(SceneSurface *surface,
    const GameplayRenderFrame *frame,const SceneSurfaceAnimationInput *animation,
    GameplaySurfacePolygon draw,void *context){
    GameMemory memory;
    if(!gameplay_geometry_owned_surface_ready(surface,frame) ||
       !animation || !draw)return 0;
    game_memory_initialize(&memory,NULL,0u,0u);
    memory.surface=surface;
    return geometry_render_surface_frame_impl(&memory,surface->address,
        frame,animation,draw,context,surface,NULL) && !memory.fault;
}
int gameplay_geometry_render_owned_surface_storage(SceneSurface *surface,
    const GameplayRenderFrame *frame,const SceneSurfaceAnimationInput *animation,
    GameplaySurfacePolygon draw,void *context,
    const GameplaySurfaceStorageOps *storage){
    GameMemory memory;
    if(!surface || !surface->active || !surface->links_clean || !frame ||
       !frame->cursors || !frame->queue_head_value || !animation || !draw ||
       !storage || !storage->read || !storage->write ||
       !storage->cursors_ready ||
       !storage->cursors_ready(storage->context,frame->cursors))return 0;
    game_memory_initialize(&memory,NULL,0u,0u);
    memory.surface=surface;
    memory.fallback_context=storage->context;
    memory.read_fallback=storage->read;
    memory.write_fallback=storage->write;
    memory.index_view=storage->index;
    memory.float_view=storage->component;
    memory.packet_view=storage->packet;
    memory.note_typed_write=storage->note_write;
    return geometry_render_surface_frame_impl(&memory,surface->address,
        frame,animation,draw,context,surface,storage) && !memory.fault;
}
int gameplay_geometry_render_surface(GameMemory *memory,GameplaySurfacePolygon draw,void *context){
    GameAddress tables[5];unsigned i;
    for(i=0;i<5;i++)tables[i]=game_memory_read_u32(memory,0x596108u+i*4u);
    return memory->fault?0:gameplay_geometry_render_surface_tables(memory,tables,draw,context);
}

/*532250: ordinary projection construction, shared by previews and game frames. */
int gameplay_geometry_projection_setup(GameMemory *m,GameAddress output,const float center[2],
        float width,float height,const float bounds[4]){
    static const unsigned offsets[4]={12,8,20,16},integers[4]={28,24,36,32};
    uint64_t bits=(uint64_t)game_memory_read_u32(m,0x575c28)|
        ((uint64_t)game_memory_read_u32(m,0x575c2c)<<32);double radians;
    long double vertical,tangent,scale;unsigned i;
    if(m->fault)return 0;
    memcpy(&radians,&bits,8);
    tangent=tanl((long double)geometry_read_f32(m,0x59d43c)*radians);
    vertical=((long double)height*geometry_read_f32(m,0x575c1c))/
        ((long double)width*geometry_read_f32(m,0x575c20));
    scale=((long double)width*geometry_read_f32(m,0x575c30))/tangent;
    geometry_write_f32(m,output,center[0]);geometry_write_f32(m,output+4,center[1]);
    for(i=0;i<4;i++){
        int32_t value=!isfinite(bounds[i]) || bounds[i]< -0x1p31f || bounds[i]>=0x1p31f?INT32_MIN:(int32_t)bounds[i];
        geometry_write_f32(m,output+offsets[i],bounds[i]);
        game_memory_write_u32(m,output+integers[i],(uint32_t)value);
    }
    geometry_write_f32(m,output+40,(float)vertical);geometry_write_f32(m,output+44,(float)scale);
    return !m->fault;
}
