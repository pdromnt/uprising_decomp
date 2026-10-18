#include "entity_vertex_transform.h"
#include "game_matrix.h"
#include "scene_instance_owner.h"

#include <stdint.h>
#include <string.h>

#define ENTITY_MATRIX_FLOATS 12u
#define ENTITY_MATRIX_BYTES (ENTITY_MATRIX_FLOATS * 4u)
#define ENTITY_VERTEX_INPUT_STRIDE 0x1cu
#define ENTITY_VERTEX_OUTPUT_STRIDE 0x0cu

static float entity_read_float(GameMemory *memory, GameAddress address) {
    uint32_t bits = game_memory_read_u32(memory, address);
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static void entity_write_float(GameMemory *memory, GameAddress address,
    float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    game_memory_write_u32(memory, address, bits);
}

static float entity_transform_component(float x, float y, float z,
    const float *matrix, unsigned row) {
    volatile long double sum;
    sum = (long double)x * (long double)matrix[row];
    sum = sum + (long double)y * (long double)matrix[row + 1u];
    sum = sum + (long double)z * (long double)matrix[row + 2u];
    sum = sum + (long double)matrix[row + 3u];
    return (float)sum;
}

static int entity_load_matrix(GameMemory *memory, GameAddress address,
    float matrix[ENTITY_MATRIX_FLOATS]) {
    unsigned index;
    for (index = 0u; index < ENTITY_MATRIX_FLOATS; ++index) {
        matrix[index] = entity_read_float(memory, address + index * 4u);
        if (memory->fault) return 0;
    }
    return 1;
}

static int entity_transform_range(GameMemory *memory, GameAddress source,
    GameAddress output, int start, int end,
    const float matrix[ENTITY_MATRIX_FLOATS]) {
    int index;
    for (index = start; index < end; ++index) {
        GameAddress input_address = source +
            (uint32_t)index * ENTITY_VERTEX_INPUT_STRIDE;
        GameAddress output_address = output +
            (uint32_t)index * ENTITY_VERTEX_OUTPUT_STRIDE;
        const ModelHullSphere *sphere=scene_projection_storage_hull(input_address,1u);
        float x = sphere?sphere->position[0]:entity_read_float(memory, input_address + 0x0cu);
        float y = sphere?sphere->position[1]:entity_read_float(memory, input_address + 0x10u);
        float z = sphere?sphere->position[2]:entity_read_float(memory, input_address + 0x14u);
        if (memory->fault) return 0;
        entity_write_float(memory, output_address,
            entity_transform_component(x, y, z, matrix, 0u));
        entity_write_float(memory, output_address + 4u,
            entity_transform_component(x, y, z, matrix, 4u));
        entity_write_float(memory, output_address + 8u,
            entity_transform_component(x, y, z, matrix, 8u));
        if (memory->fault) return 0;
    }
    return 1;
}

int entity_transform_vertex_ranges(GameMemory *memory, GameAddress object,
    GameAddress scratch) {
    GameAddress source, output;
    float attachment[ENTITY_MATRIX_FLOATS];
    float turret[ENTITY_MATRIX_FLOATS];
    float base[ENTITY_MATRIX_FLOATS];
    int base_end, attachment_end, vertex_count;

    if (!game_matrix_compose(memory, scratch, object + 0x80u,
            object + 0x278u) ||
        !game_matrix_compose(memory, scratch + ENTITY_MATRIX_BYTES,
            scratch, object + 0x230u)) return 0;

    source = game_memory_read_u32(memory, object + 0x3cu);
    output = game_memory_read_u32(memory, object + 0x40u);
    vertex_count = (int16_t)game_memory_read_u16(memory, object + 0x44u);
    attachment_end = (int16_t)game_memory_read_u16(memory, object + 0x22cu);
    base_end = (int16_t)game_memory_read_u16(memory, object + 0x274u);
    if (memory->fault ||
        !entity_load_matrix(memory, object + 0x80u, base) ||
        !entity_load_matrix(memory, scratch, attachment) ||
        !entity_load_matrix(memory, scratch + ENTITY_MATRIX_BYTES, turret))
        return 0;

    return entity_transform_range(memory, source, output, 0, base_end, base) &&
        entity_transform_range(memory, source, output, base_end,
            attachment_end, attachment) &&
        entity_transform_range(memory, source, output, attachment_end,
            vertex_count, turret);
}

static int player_vertex_matrix(GameMemory *memory,GameAddress model,SceneInstance *instance,float matrix[12]) {
    if(instance){memcpy(matrix,instance->local,48u);return 1;}
    return entity_load_matrix(memory,model+4u,matrix);
}
static float player_vertex_component(const float point[3],const float matrix[12],
        unsigned row,unsigned schedule) {
    volatile long double x=(long double)point[0]*matrix[row];
    volatile long double y=(long double)point[1]*matrix[row+1u];
    volatile long double z=(long double)point[2]*matrix[row+2u];
    volatile long double sum;
    /* Body, mount and turret have distinct retail addition schedules. */
    if(schedule==2u){sum=z+y;sum=sum+x;}
    else if(schedule==1u){sum=x+z;sum=sum+y;}
    else{sum=x+y;sum=sum+z;}
    sum=sum+matrix[row+3u];return (float)sum;
}
static int player_vertex_range(GameMemory *memory,PresentationPose *pose,PlayerRig *rig,
        int rendered,unsigned part,const float composed[12]) {
    int index=part==0u?0:(int16_t)(part==1u?rig->mount_part:rig->turret_part);
    GameAddress source_key=0;unsigned source_count=0;
    SceneMesh *mesh=NULL;
    const ModelHullSphere *hull=NULL;
    for(;;++index){
        int end=(int16_t)(part==0u?rig->mount_part:part==1u?rig->turret_part:pose->vertex_count);
        float point[3],output[3],live_body[12];const float *matrix=composed;
        GameAddress destination;float *owned_output;unsigned row;
        if(index>=end)return !memory->fault;
        /* Re-read owner state each iteration: old address views may alias output. */
        if(source_key!=pose->hull || source_count!=pose->vertex_count){
            source_key=pose->hull;source_count=pose->vertex_count;
            mesh=scene_projection_storage_bound_mesh(source_key,source_count);
            hull=scene_projection_storage_hull(source_key,source_count);
        }
        if(hull && index>=0)memcpy(point,hull[index].position,sizeof(point));
        else if(mesh && index>=0 && (uint32_t)index<mesh->vertex_count){
            const AssetMeshVertex *vertex=&mesh->vertices[index];
            memcpy(point,&vertex->x_bits,4u);memcpy(point+1,&vertex->y_bits,4u);
            memcpy(point+2,&vertex->z_bits,4u);
        }else for(row=0;row<3u;++row)
            point[row]=entity_read_float(memory,pose->hull+(uint32_t)index*28u+12u+row*4u);
        if(memory->fault)return 0;
        if(part==0u){
            if(rendered){
                if(pose->model_instance)matrix=pose->model_instance->local;
                else{if(!entity_load_matrix(memory,pose->model+4u,live_body))return 0;matrix=live_body;}
            }
            else matrix=pose->world_matrix;
        }
        for(row=0;row<3u;++row)
            output[row]=player_vertex_component(point,matrix,row*4u,
                rendered?(part==0u?2u:part==2u?1u:0u):0u);
        destination=(rendered?pose->controller:pose->vertices)+(uint32_t)index*12u;
        owned_output=scene_projection_storage_positions(destination,1u);
        if(owned_output)memcpy(owned_output,output,sizeof(output));
        else for(row=0;row<3u;++row)entity_write_float(memory,destination+row*4u,output[row]);
        if(memory->fault)return 0;
    }
}
int player_transform_vertex_ranges(GameMemory *memory,PresentationPose *pose,PlayerRig *rig,int rendered) {
    float body[12],mount[12],turret[12],attachment[12],tail[12];
    if(!memory || !pose || !rig)return 0;
    if(rendered){
        if(!player_vertex_matrix(memory,pose->model,pose->model_instance,body) ||
           !player_vertex_matrix(memory,rig->mount_model,rig->mount_instance,mount) ||
           !player_vertex_matrix(memory,rig->turret_model,rig->turret_instance,turret))return 0;
    }else{
        memcpy(body,pose->world_matrix,48u);memcpy(mount,rig->inline_mount,48u);
        memcpy(turret,rig->inline_turret,48u);
    }
    game_matrix_compose_values(attachment,body,mount);
    game_matrix_compose_values(tail,attachment,turret);
    if(rendered)return player_vertex_range(memory,pose,rig,1,2u,tail) &&
        player_vertex_range(memory,pose,rig,1,1u,attachment) &&
        player_vertex_range(memory,pose,rig,1,0u,body);
    return player_vertex_range(memory,pose,rig,0,0u,body) &&
        player_vertex_range(memory,pose,rig,0,1u,attachment) &&
        player_vertex_range(memory,pose,rig,0,2u,tail);
}
