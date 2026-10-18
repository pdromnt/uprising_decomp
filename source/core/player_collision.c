/* Player collision response owns its rollback pose and retry matrices in C.
 * Object collision queries remain a resource port; they cannot own these arrays. */
#include "player_collision.h"
#include "game_matrix.h"
#include "gameplay_geometry.h"
#include "player_state.h"
#include "scene_instance_owner.h"
#include "entity_vertex_transform.h"
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

typedef struct MovementVector { float x,y,z; } MovementVector;
typedef struct MovementTransformResult { MovementVector stored;long double extended_x; } MovementTransformResult;
static uint32_t movement_float_bits(float value){uint32_t bits;memcpy(&bits,&value,4u);return bits;}
static float movement_read_float(GameMemory *memory,uint32_t address){uint32_t bits=game_memory_read_u32(memory,address);float value;memcpy(&value,&bits,4u);return value;}
static float movement_store_float(long double value){volatile float stored=(float)value;return stored;}
static float movement_add(float left,float right){volatile long double value=(long double)left+right;return movement_store_float(value);}
static uint32_t movement_truncate(long double value){
    if(!isfinite(value) || value<(long double)INT64_MIN || value>=9223372036854775808.0L)return 0u;
    return (uint32_t)(uint64_t)(int64_t)value;
}
static long double movement_dot_row(const float matrix[12],
    unsigned row, MovementVector vector, int with_translation) {
    unsigned base=row*4u;
    float m0 = matrix[base];
    float m1 = matrix[base+1u];
    float m2 = matrix[base+2u];
    volatile long double value = (long double)m2 * vector.z;
    value += (long double)m1 * vector.y;
    value += (long double)m0 * vector.x;
    if (with_translation)
        value += matrix[base+3u];
    return value;
}

static MovementTransformResult movement_transform(const float matrix[12], MovementVector vector, int with_translation) {
    MovementTransformResult result;
    long double y = movement_dot_row(matrix, 1u, vector,
        with_translation);
    long double z = movement_dot_row(matrix, 2u, vector,
        with_translation);
    long double x = movement_dot_row(matrix, 0u, vector,
        with_translation);

    result.stored.y = movement_store_float(y);
    result.stored.z = movement_store_float(z);
    result.stored.x = movement_store_float(x);
    result.extended_x = x;
    return result;
}

static int movement_build_horizontal_matrix(GameMemory *memory,
    float matrix[12], MovementVector origin, MovementVector velocity,
    const float contact[3]) {
    float contact_x = contact[0];
    float contact_z = contact[2];
    volatile long double dz = (long double)contact_z - origin.z;
    volatile long double dx = (long double)contact_x - origin.x;
    volatile long double cross = dz * velocity.x;
    float cosine;
    float sine;

    cross -= dx * velocity.z;
    if (cross > 0.0L) {
        cosine = movement_read_float(memory, 0x0062a188u);
        sine = movement_read_float(memory, 0x00629988u);
    } else {
        cosine = movement_read_float(memory, 0x00629fc8u);
        sine = movement_read_float(memory, 0x0062b7c8u);
    }
    if (memory->fault) return 0;
    matrix[0] = cosine; matrix[1] = 0.0f; matrix[2] = sine;
    matrix[3] = 0.0f;
    matrix[4] = 0.0f; matrix[5] = 1.0f; matrix[6] = 0.0f;
    matrix[7] = 0.0f;
    matrix[8] = -sine; matrix[9] = 0.0f; matrix[10] = cosine;
    matrix[11] = 0.0f;
    return !memory->fault;
}

static int movement_build_vertical_matrix(GameMemory *memory,
    MovementVector velocity, float matrix[12]) {
    uint32_t x_bits = movement_float_bits(velocity.x);
    uint32_t z_bits = movement_float_bits(velocity.z);
    float magnitude_x;
    float magnitude_z;
    uint32_t table_index;
    uint16_t heading;
    uint16_t table_value;
    float sine;
    float cosine;
    float turn_inverse[12];
    float pitch[12];
    float turn_forward[12];
    float pitched[12];


    if (((x_bits | z_bits) & 0x7fffffffu) == 0u) return 0;
    magnitude_x = fabsf(velocity.x);
    magnitude_z = fabsf(velocity.z);
    if (magnitude_z <= magnitude_x) {
        uint16_t base = (int32_t)x_bits > 0 ? 0x0200u : 0x0600u;
        volatile long double value = (long double)velocity.z *
            movement_read_float(memory, 0x00574d94u);
        value /= velocity.x;
        value += movement_read_float(memory, 0x00574d94u);
        if(memory->fault)return -1;
        table_index=movement_truncate(value);
        table_value = game_memory_read_u16(memory, 0x0062e8b0u + table_index * 2u);
        heading = (uint16_t)(base - table_value);
    } else {
        volatile long double value = (long double)velocity.x *
            movement_read_float(memory, 0x00574d94u);
        value /= velocity.z;
        value += movement_read_float(memory, 0x00574d94u);
        if(memory->fault)return -1;
        table_index=movement_truncate(value);
        table_value = game_memory_read_u16(memory, 0x0062e8b0u + table_index * 2u);
        heading = (int32_t)z_bits > 0 ? (uint16_t)(table_value & 0x07ffu) :
            (uint16_t)(table_value + 0x0400u);
    }
    if (memory->fault) return -1;
    heading = (uint16_t)-heading;
    sine = movement_read_float(memory, 0x006298a8u +
        ((uint32_t)heading & 0x07ffu) * 4u);
    cosine = movement_read_float(memory, 0x006298a8u +
        (((uint32_t)(int32_t)(int16_t)heading + 0x0200u) & 0x07ffu) * 4u);
    if (memory->fault) return -1;

    turn_forward[0] = cosine; turn_forward[1] = 0.0f;
    turn_forward[2] = sine; turn_forward[3] = 0.0f;
    turn_forward[4] = 0.0f; turn_forward[5] = 1.0f;
    turn_forward[6] = 0.0f; turn_forward[7] = 0.0f;
    turn_forward[8] = -sine; turn_forward[9] = 0.0f;
    turn_forward[10] = cosine; turn_forward[11] = 0.0f;

    turn_inverse[0] = cosine; turn_inverse[1] = 0.0f;
    turn_inverse[2] = -sine; turn_inverse[3] = 0.0f;
    turn_inverse[4] = 0.0f; turn_inverse[5] = 1.0f;
    turn_inverse[6] = 0.0f; turn_inverse[7] = 0.0f;
    turn_inverse[8] = sine; turn_inverse[9] = 0.0f;
    turn_inverse[10] = cosine; turn_inverse[11] = 0.0f;

    cosine = movement_read_float(memory, 0x0062a038u);
    sine = movement_read_float(memory, 0x0062b838u);
    if (memory->fault) return -1;
    pitch[0] = 1.0f; pitch[1] = 0.0f; pitch[2] = 0.0f;
    pitch[3] = 0.0f;
    pitch[4] = 0.0f; pitch[5] = cosine; pitch[6] = -sine;
    pitch[7] = 0.0f;
    pitch[8] = 0.0f; pitch[9] = sine; pitch[10] = cosine;
    pitch[11] = 0.0f;

    game_matrix_compose_values(pitched,turn_inverse,pitch);
    game_matrix_compose_values(matrix,pitched,turn_forward);
    return 1;
}
static int movement_refresh(GameMemory *memory,PresentationPose *pose,PlayerControls *controls,PlayerRig *rig){
    return player_rig_build(memory,pose,controls,rig,1) && player_transform_vertex_ranges(memory,pose,rig,1);
}
static int movement_query(GameMemory *memory,PresentationPose *pose,PlayerRig *rig,
        const PlayerCollisionServices *services,const float origin[3],float contact[3],GameAddress *collider,int *blocked){
    GameplayGeometryFootprintResult footprint;
    if(!services || !services->query || !services->query(services->context,origin,contact,*collider,collider))return 0;
    *blocked=gameplay_geometry_footprint_values(memory,pose->local_position,(const float (*)[3])rig->terrain_ring,&footprint);
    return !memory->fault;
}
static int movement_retry_arc(GameMemory *memory,PresentationPose *pose,PlayerControls *controls,PlayerRig *rig,
        const PlayerCollisionServices *services,const float matrix[12],int translation,
        MovementVector origin,MovementVector velocity,GameAddress *collider){
    MovementVector candidate=velocity;unsigned attempt;float previous[3]={origin.x,origin.y,origin.z},contact[3]={0};
    for(attempt=0u;attempt<9u;++attempt){
        MovementTransformResult transformed=movement_transform(matrix,candidate,translation);int blocked;
        candidate=transformed.stored;
        pose->local_position[0]=movement_store_float(transformed.extended_x+(long double)origin.x);
        pose->local_position[1]=movement_add(origin.y,candidate.y);
        pose->local_position[2]=movement_add(origin.z,candidate.z);
        if(!movement_refresh(memory,pose,controls,rig) ||
            !movement_query(memory,pose,rig,services,previous,contact,collider,&blocked))return -1;
        if(!*collider && !blocked){
            rig->controller_velocity[0]=candidate.x;rig->controller_velocity[1]=candidate.y;rig->controller_velocity[2]=candidate.z;
            return 1;
        }
    }
    return 0;
}
static void movement_snapshot_matrix(GameMemory *memory,GameAddress model,SceneInstance *node,float values[12]){
    unsigned i;
    if(node){memcpy(values,node->local,12u*sizeof(float));return;}
    for(i=0u;i<12u;++i)values[i]=movement_read_float(memory,model+4u+i*4u);
}
static int movement_restore_matrix(GameMemory *memory,GameAddress model,SceneInstance *node,const float values[12]){
    unsigned i;
    if(node){memcpy(node->local,values,12u*sizeof(float));return scene_projection_storage_project_holder(node);}
    for(i=0u;i<12u;++i)game_memory_write_u32(memory,model+4u+i*4u,movement_float_bits(values[i]));
    return !memory->fault;
}
int player_resolve_movement_collision(GameMemory *memory,GameAddress identity,
        PresentationPose *pose,PlayerControls *controls,PlayerRig *rig,const PlayerCollisionParameters *parameters,
        const PlayerCollisionServices *services,uint32_t *final_result){
    MovementVector origin,velocity;float previous[3],matrices[3][12],horizontal[12],inverse[12],contact[3]={0},first_contact[3]={0};
    GameAddress models[3],collider=0u;SceneInstance *nodes[3];uint16_t angles[5];uint32_t result;int blocked,retry;unsigned i,j;
    GamePlayerDropEdgeResult drop;
    if(!pose || !controls || !rig || !parameters || !services)return 0;
    result=pose->properties[0];
    if((int32_t)result<=0)goto finished;
    result=controls->mode_flags;
    if(result&1u)goto finished;
    result=(uint32_t)parameters->death_tick;
    if(result!=0xffffffffu)goto finished;
    memcpy(previous,pose->local_position,sizeof(previous));origin.x=previous[0];origin.y=previous[1];origin.z=previous[2];
    memcpy(angles,pose->local_angles,3u*sizeof(uint16_t));angles[3]=controls->previous_heading;angles[4]=controls->previous_aim;
    models[0]=pose->model;models[1]=rig->turret_model;models[2]=rig->mount_model;
    nodes[0]=pose->model_instance;nodes[1]=rig->turret_instance;nodes[2]=rig->mount_instance;
    for(i=0u;i<3u;++i)movement_snapshot_matrix(memory,models[i],nodes[i],matrices[i]);
    if(memory->fault)return 0;
    if(services->observe_terrain)services->observe_terrain(services->context,1);
    if(!player_update_terrain_contact(memory,pose,controls,rig,parameters->clearance,result,&services->terrain,&result))return 0;
    if(services->observe_terrain)services->observe_terrain(services->context,0);
    if(!gameplay_geometry_world_bounds_values(parameters->radius,pose->local_position)){
        float x=pose->local_position[0],z=pose->local_position[2];
        volatile long double lower_extended=(long double)parameters->radius*movement_read_float(memory,0x574d78u);
        float lower=movement_store_float(lower_extended);
        volatile long double upper_extended=(long double)parameters->radius*movement_read_float(memory,0x574d7cu);
        float upper;
        controls->input_flags&=0xffffffccu;controls->previous_input_flags&=0xffffffccu;
        if(lower_extended>(long double)x)rig->controller_velocity[0]=100.0f;
        if(!(z>=lower))rig->controller_velocity[2]=100.0f;
        upper_extended+=movement_read_float(memory,0x574d80u);upper=movement_store_float(upper_extended);
        if(!(upper_extended>=(long double)x))rig->controller_velocity[0]=-100.0f;
        if(z>upper)rig->controller_velocity[2]=-100.0f;
    }
    drop=player_drop_edge_values(game_player_ref(memory,identity),pose->local_position,previous,(const float (*)[3])rig->terrain_ring);
    rig->motion.slope_drag=drop.blocked?1000.0f:1.0f;rig->motion.speed_drag=drop.blocked?100.0f:1.0f;
    if(memory->fault || !player_integrate_motion(memory,pose,controls,rig,parameters->max_speed,&services->motion) ||
        !movement_query(memory,pose,rig,services,previous,first_contact,&collider,&blocked))return 0;
    if(!collider && !blocked){controls->mode_flags&=~0x10u;result=controls->mode_flags;goto finished;}
    controls->mode_flags|=0x10u;
    velocity.x=rig->controller_velocity[0];velocity.y=rig->controller_velocity[1];velocity.z=rig->controller_velocity[2];
    pose->local_position[0]=movement_add(origin.x,velocity.x);pose->local_position[1]=movement_add(origin.y,velocity.y);pose->local_position[2]=movement_add(origin.z,velocity.z);
    if(!movement_refresh(memory,pose,controls,rig) ||
        !movement_query(memory,pose,rig,services,previous,contact,&collider,&blocked))return 0;
    if(collider || blocked){
        if(!movement_build_horizontal_matrix(memory,horizontal,origin,velocity,first_contact))return 0;
        retry=movement_retry_arc(memory,pose,controls,rig,services,horizontal,1,origin,velocity,&collider);
        if(retry<0)return 0;
        if(!retry){
            for(i=0u;i<3u;++i)for(j=0u;j<3u;++j)inverse[i*4u+j]=horizontal[j*4u+i];
            retry=movement_retry_arc(memory,pose,controls,rig,services,inverse,0,origin,velocity,&collider);
            if(retry<0)return 0;
        }
        if(!retry){
            int built=movement_build_vertical_matrix(memory,velocity,horizontal);
            if(built<0)return 0;
            if(built){retry=movement_retry_arc(memory,pose,controls,rig,services,horizontal,1,origin,velocity,&collider);if(retry<0)return 0;}
        }
        if(!retry)for(i=0u;i<3u;++i){uint32_t bits=movement_float_bits(rig->controller_velocity[i])^0x80000000u;memcpy(&rig->controller_velocity[i],&bits,4u);}
    }
    memcpy(pose->local_position,previous,sizeof(previous));memcpy(pose->local_angles,angles,3u*sizeof(uint16_t));
    controls->previous_heading=angles[3];controls->previous_aim=angles[4];
    for(i=0u;i<3u;++i)if(!movement_restore_matrix(memory,models[i],nodes[i],matrices[i]))return 0;
    if(!player_transform_vertex_ranges(memory,pose,rig,1))return 0;
    result=(uint32_t)(int32_t)(int16_t)rig->mount_part;
finished:
    if(final_result)*final_result=result;
    return !memory->fault;
}
