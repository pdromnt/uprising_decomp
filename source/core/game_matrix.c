#include "game_matrix.h"

#include <string.h>
#include <math.h>

static float matrix_read_f32(GameMemory *memory, GameAddress address) {
    uint32_t bits = game_memory_read_u32(memory, address);
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static void matrix_write_f32(GameMemory *memory, GameAddress address,
    float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    game_memory_write_u32(memory, address, bits);
}

static uint32_t matrix_float_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}
static float matrix_sine(GameMemory *memory,uint16_t angle){
    return matrix_read_f32(memory,0x6298a8u+((uint32_t)angle&2047u)*4u);
}
int game_matrix_camera_projection(GameMemory *memory,GameAddress output,
    const float position[3],uint16_t roll,uint16_t pitch,uint16_t heading,float scale){
    float sr=matrix_sine(memory,roll),cr=matrix_sine(memory,(uint16_t)(roll+512u));
    float sp=matrix_sine(memory,pitch),cp=matrix_sine(memory,(uint16_t)(pitch+512u));
    float sh=matrix_sine(memory,heading),ch=matrix_sine(memory,(uint16_t)(heading+512u));
    float combined[12]={cr,-sr,0,0,(float)((long double)sr*cp),(float)((long double)cr*cp),-sp,0,
        (float)((long double)sr*sp),(float)((long double)cr*sp),cp,0};
    float yaw[12]={ch,0,sh,0,0,1,0,0,-sh,0,ch,0},matrix[12];
    uint64_t bits=(uint64_t)game_memory_read_u32(memory,0x575c28u)|((uint64_t)game_memory_read_u32(memory,0x575c2cu)<<32u);
    double radians;float radius,negative_radius;long double factor;unsigned point,axis;
    memcpy(&radians,&bits,8u);
    factor=tanl((long double)matrix_read_f32(memory,0x59d43cu)*radians)*scale;
    radius=(float)factor;negative_radius=(float)-factor;
    game_matrix_compose_values(matrix,yaw,combined);
    matrix[3]=position[0];matrix[7]=position[1];matrix[11]=position[2];
    for(axis=0;axis<3;axis++)matrix_write_f32(memory,output+axis*4u,position[axis]);
    for(point=0;point<4;point++){
        float ray[3]={point==1u || point==2u?negative_radius:radius,point>=2u?negative_radius:radius,scale},world[3];
        game_matrix_transform_point_values(world,matrix,ray);
        for(axis=0;axis<3;axis++)matrix_write_f32(memory,output+12u+point*12u+axis*4u,world[axis]);
    }
    return !memory->fault;
}
int game_matrix_camera_view(GameMemory *memory,GameAddress output,
    const float position[3],uint16_t roll,uint16_t pitch,uint16_t heading){
    uint16_t yaw=(uint16_t)(0u-heading),tilt=(uint16_t)(0u-pitch),bank=(uint16_t)(0u-roll);
    float sy=matrix_sine(memory,yaw),cy=matrix_sine(memory,(uint16_t)(yaw+512u));
    float sp=matrix_sine(memory,tilt),cp=matrix_sine(memory,(uint16_t)(tilt+512u));
    float sr=matrix_sine(memory,bank),cr=matrix_sine(memory,(uint16_t)(bank+512u));
    float nx=-position[0],ny=-position[1],nz=-position[2];
    float turn[12]={cy,0,sy,(float)((long double)sy*nz+(long double)cy*nx),
        0,1,0,ny,-sy,0,cy,(float)((long double)(-sy)*nx+(long double)cy*nz)};
    float elevation[12]={1,0,0,0,0,cp,-sp,0,0,sp,cp,0};
    float rotation[12]={cr,-sr,0,0,sr,cr,0,0,0,0,1,0};
    float temporary[12],result[12];unsigned index;
    game_matrix_compose_values(temporary,elevation,turn);
    game_matrix_compose_values(result,rotation,temporary);
    for(index=0;index<12;index++)matrix_write_f32(memory,output+index*4u,result[index]);
    return !memory->fault;
}

static uint32_t matrix_truncate(long double value) {
    if (!isfinite(value) || value < (long double)INT64_MIN ||
        value >= (long double)INT64_MAX) return 0x80000000u;
    return (uint32_t)(int64_t)value;
}

/* Both tables are initialized by the shared angle resource. Keep its exact
 * quantization, including the high-resolution tails near a quarter turn. */
static uint16_t matrix_inverse_sine(GameMemory *memory, float value,
        int negate) {
    uint32_t bits = matrix_float_bits(value), index;
    uint16_t angle;
    long double scaled;
    if (bits > 0xbf666666u) {
        scaled = (long double)matrix_read_f32(memory, 0x00571f4cu) - value;
        scaled *= matrix_read_f32(memory, 0x00571f50u);
        index = matrix_truncate(scaled);
        angle = game_memory_read_u16(memory, 0x0062b8a8u + index * 2u);
        return negate ? angle : (uint16_t)-angle;
    }
    if ((int32_t)bits > (int32_t)0x3f666666u) {
        scaled = (long double)value - matrix_read_f32(memory, 0x00571f54u);
        scaled *= matrix_read_f32(memory, 0x00571f50u);
        index = matrix_truncate(scaled);
        angle = game_memory_read_u16(memory, 0x0062b8a8u + index * 2u);
        return negate ? (uint16_t)-angle : angle;
    }
    scaled = (long double)value * matrix_read_f32(memory,
        negate ? 0x00571f58u : 0x00571f34u);
    scaled += matrix_read_f32(memory, 0x00571f34u);
    index = matrix_truncate(scaled);
    return game_memory_read_u16(memory, 0x0062f8b8u + index * 2u);
}

static uint16_t matrix_phase_angle(GameMemory *memory, long double sine,
        long double cosine) {
    float rounded_sine = (float)sine;
    float rounded_cosine = (float)cosine;
    uint16_t angle;
    long double scaled;
    uint32_t index;
    if (sine < matrix_read_f32(memory, 0x00571f5cu) &&
        matrix_float_bits(rounded_sine) < 0xbf3504f2u) {
        angle = matrix_inverse_sine(memory, rounded_sine, 0);
        if (cosine < 0.0L) angle = (uint16_t)(0x400u - angle);
    } else {
        if (cosine > matrix_read_f32(memory, 0x00571f54u)) {
            scaled = (long double)rounded_cosine -
                matrix_read_f32(memory, 0x00571f54u);
            scaled *= matrix_read_f32(memory, 0x00571f50u);
            index = matrix_truncate(scaled);
            angle = (uint16_t)(0x200u - game_memory_read_u16(memory,
                0x0062b8a8u + index * 2u));
        } else if (matrix_float_bits(rounded_cosine) > 0xbf666666u) {
            scaled = (long double)matrix_read_f32(memory, 0x00571f4cu) -
                rounded_cosine;
            scaled *= matrix_read_f32(memory, 0x00571f50u);
            index = matrix_truncate(scaled);
            angle = (uint16_t)(0x200u + game_memory_read_u16(memory,
                0x0062b8a8u + index * 2u));
        } else {
            scaled = (long double)rounded_cosine *
                matrix_read_f32(memory, 0x00571f34u);
            scaled += matrix_read_f32(memory, 0x00571f34u);
            index = matrix_truncate(scaled);
            angle = (uint16_t)(0x200u - game_memory_read_u16(memory,
                0x0062f8b8u + index * 2u));
        }
        if (sine < 0.0L) angle = (uint16_t)(0x800u - angle);
    }
    return angle & 0x7ffu;
}

static int matrix_extract_angles(GameMemory *memory,const float matrix[12],uint16_t angles[3],int roll){
    uint16_t pitch;
    float pitch_cosine;
    if (!memory || !matrix || !angles) return 0;
    pitch = matrix_inverse_sine(memory, matrix[6], 1);
    if (matrix_float_bits(matrix[5]) > 0x80000000u)
        pitch = (uint16_t)(0x400u - pitch);
    pitch &= 0x7ffu;
    if (pitch > 0x400u) pitch = (uint16_t)(pitch - 0x800u);
    angles[1] = pitch;
    pitch_cosine = matrix_read_f32(memory, 0x006298a8u +
        (((uint32_t)(int32_t)(int16_t)pitch + 0x200u) & 0x7ffu) * 4u);
    if (!(matrix_float_bits(pitch_cosine) & 0x7fffffffu)) {
        angles[0] = matrix_inverse_sine(memory, matrix[8], 1);
        if (matrix_float_bits(matrix[0]) > 0x80000000u)
            angles[0] = (uint16_t)(0x400u - angles[0]);
        angles[0] &= 0x7ffu;
        angles[2] = 0u;
    } else {
        angles[0] = matrix_phase_angle(memory,
            (long double)matrix[2] / pitch_cosine,
            (long double)matrix[10] / pitch_cosine);
        if(roll)angles[2] = matrix_phase_angle(memory,
            (long double)matrix[4] / pitch_cosine,
            (long double)matrix[5] / pitch_cosine);
        if(roll && angles[2] > 0x400u) angles[2] = (uint16_t)(angles[2] - 0x800u);
    }
    return !memory->fault;
}
int game_matrix_extract_angles(GameMemory *memory,const float matrix[12],uint16_t angles[3]){return matrix_extract_angles(memory,matrix,angles,1);}
int game_matrix_extract_heading_pitch(GameMemory *memory,const float matrix[12],uint16_t angles[2]){
    uint16_t result[3]={0u,0u,0u};int success=matrix_extract_angles(memory,matrix,result,0);
    if(success){angles[0]=result[0];angles[1]=result[1];}return success;
}

void game_matrix_attachment_orientation(GameMemory *memory, float output[12],
        const uint16_t angles[3]) {
    float sa, sb, sc, ca, cb, cc, cross;
    sa = matrix_read_f32(memory, 0x006298a8u + (angles[0] & 0x7ffu) * 4u);
    sb = matrix_read_f32(memory, 0x006298a8u + (angles[1] & 0x7ffu) * 4u);
    sc = matrix_read_f32(memory, 0x006298a8u + (angles[2] & 0x7ffu) * 4u);
    ca = matrix_read_f32(memory, 0x006298a8u + ((angles[0] + 0x200u) & 0x7ffu) * 4u);
    cb = matrix_read_f32(memory, 0x006298a8u + ((angles[1] + 0x200u) & 0x7ffu) * 4u);
    cc = matrix_read_f32(memory, 0x006298a8u + ((angles[2] + 0x200u) & 0x7ffu) * 4u);
    cross = (float)((long double)sc * sb);
    output[0] = (float)((long double)cc * ca + (long double)cross * sa);
    output[1] = (float)(((long double)sb * sa) * cc - (long double)sc * ca);
    output[2] = (float)((long double)sa * cb);
    output[3] = 0.0f;
    output[4] = (float)((long double)sc * cb);
    output[5] = (float)((long double)cc * cb);
    output[6] = -sb;
    output[7] = 0.0f;
    output[8] = (float)((long double)cross * ca - (long double)sa * cc);
    output[9] = (float)((long double)sa * sc + ((long double)sb * cc) * ca);
    output[10] = (float)((long double)cb * ca);
    output[11] = 0.0f;
}

static float dot3_reverse(float a0, float b0, float a1, float b1,
    float a2, float b2) {
    long double value = (long double)a2 * (long double)b2;
    value += (long double)a1 * (long double)b1;
    value += (long double)a0 * (long double)b0;
    return (float)value;
}

void game_matrix_compose_values(float output[12], const float left[12],
    const float right[12]) {
    float product[12];
    float *destination=output;
    unsigned row;
    unsigned column;
    /* Camera chains append in place. Delay their stores until both complete
     * inputs have been consumed; ordinary distinct outputs keep the fast path. */
    if(output==left || output==right)output=product;
    for (row = 0u; row < 3u; ++row) {
        unsigned base = row * 4u;
        for (column = 0u; column < 3u; ++column)
            output[base + column] = dot3_reverse(left[base], right[column],
                left[base + 1u], right[4u + column], left[base + 2u],
                right[8u + column]);
        {
            long double translation = (long double)left[base + 2u] *
                (long double)right[11u];
            translation += (long double)left[base + 1u] *
                (long double)right[7u];
            translation += (long double)left[base] *
                (long double)right[3u];
            translation += (long double)left[base + 3u];
            output[base + 3u] = (float)translation;
        }
    }
    if(output==product)memcpy(destination,product,sizeof(product));
}

void game_matrix_apply_presentation(float active[12],
    const float incoming[12], uint32_t policy) {
    float result[12];
    if (policy == 0u)
        game_matrix_compose_values(result, incoming, active);
    else if (policy == 1u)
        game_matrix_compose_values(result, active, incoming);
    else
        memcpy(result, incoming, sizeof(result));
    memcpy(active, result, sizeof(result));
}

static float affine_component(float a, float x, float b, float y,
    float c, float z, float translation) {
    return (float)((((long double)a * (long double)x +
        (long double)c * (long double)z) +
        (long double)b * (long double)y) +
        (long double)translation);
}

void game_matrix_transform_point_values(float output[3],
    const float matrix[12], const float point[3]) {
    output[0] = affine_component(matrix[0], point[0], matrix[1], point[1],
        matrix[2], point[2], matrix[3]);
    output[1] = affine_component(matrix[4], point[0], matrix[5], point[1],
        matrix[6], point[2], matrix[7]);
    output[2] = affine_component(matrix[8], point[0], matrix[9], point[1],
        matrix[10], point[2], matrix[11]);
}

int game_matrix_compose(GameMemory *memory, GameAddress output,
    GameAddress left, GameAddress right) {
    float a[12], b[12], result[12];
    unsigned index;
    for (index = 0u; index < 12u; ++index) {
        a[index] = matrix_read_f32(memory, left + index * 4u);
        b[index] = matrix_read_f32(memory, right + index * 4u);
    }
    if (memory->fault) return 0;
    game_matrix_compose_values(result, a, b);
    for (index = 0u; index < 12u && !memory->fault; ++index)
        matrix_write_f32(memory, output + index * 4u, result[index]);
    return !memory->fault;
}

int game_matrix_transform_point(GameMemory *memory, GameAddress output,
    GameAddress matrix, GameAddress point) {
    float matrix_values[12], point_values[3], result[3];
    unsigned index;
    for (index = 0u; index < 12u; ++index)
        matrix_values[index] = matrix_read_f32(memory, matrix + index * 4u);
    for (index = 0u; index < 3u; ++index)
        point_values[index] = matrix_read_f32(memory, point + index * 4u);
    if (memory->fault) return 0;
    game_matrix_transform_point_values(result, matrix_values, point_values);
    for (index = 0u; index < 3u && !memory->fault; ++index)
        matrix_write_f32(memory, output + index * 4u, result[index]);
    return !memory->fault;
}
