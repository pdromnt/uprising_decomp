#include "terrain_asset.h"
/* Native player terrain-contact and suspension update for retail 0x004f3240.

   The original routine integrates horizontal motion, samples the four hull
   contact points, chooses the supporting terrain triangle, resolves vertical
   motion, filters the chassis attitude, applies turret controls, and finally
   rebuilds the player pose.  Float stores and raw-bit comparisons are kept
   explicit because several retail branches depend on their exact results. */
#include "player_terrain_contact.h"
#include "gameplay_geometry.h"
#include "entity_vertex_transform.h"

typedef struct TerrainFrame { PresentationPose *pose; PlayerControls *controls; PlayerRig *rig; float clearance; } TerrainFrame;

#include <math.h>
#include <stdint.h>
#include <string.h>

typedef struct TerrainContactPoint {
    float x;
    float y;
    float z;
} TerrainContactPoint;

typedef struct TerrainNormal {
    float x;
    float y;
    float z;
} TerrainNormal;

static uint32_t terrain_float_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static float terrain_bits_float(uint32_t bits) {
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static float terrain_read_float(GameMemory *memory, uint32_t address) {
    return terrain_bits_float(game_memory_read_u32(memory, address));
}


/* Force the same 80-bit-to-32-bit store boundary used by the x87 code. */
static float terrain_store_float(long double value) {
    volatile float stored = (float)value;
    return stored;
}

static float terrain_add(float left, float right) {
    volatile long double value = (long double)left + (long double)right;
    return terrain_store_float(value);
}

static float terrain_rotated_coordinate(float first, float first_scale,
    float second, float second_scale, float origin) {
    volatile long double value = (long double)first * first_scale;
    value += (long double)second * second_scale;
    value += origin;
    return terrain_store_float(value);
}

static float terrain_difference(float left, float right) {
    volatile long double value = (long double)left - (long double)right;
    return terrain_store_float(value);
}

static float terrain_cross_component(float a, float b, float c, float d) {
    volatile long double value = (long double)a * b;
    value -= (long double)c * d;
    return terrain_store_float(value);
}

static float terrain_trig(GameMemory *memory, int32_t angle) {
    return terrain_read_float(memory, 0x006298a8u +
        (((uint32_t)angle & 0x07ffu) * 4u));
}

static int32_t terrain_truncate(long double value) {
    return (int32_t)value;
}

static int16_t terrain_divide_four(int32_t value) {
    /* C99 and VC6 both truncate signed division toward zero. */
    return (int16_t)(value / 4);
}

static void terrain_integrate_horizontal(GameMemory *memory, TerrainFrame *frame) {
    float x = frame->pose->local_position[0];
    float z = frame->pose->local_position[2];
    float velocity_x = frame->rig->controller_velocity[0];
    float velocity_z = frame->rig->controller_velocity[2];
    long double detail_value = gameplay_geometry_detail_height(memory, x, z);
    float detail = terrain_store_float(detail_value);

    if (memory->fault) return;
    if (detail_value == 0.0L) {
        x = terrain_add(velocity_x, x);
        z = terrain_add(z, velocity_z);
    } else {
        float clearance = frame->clearance;
        float obstruction_scale;
        float candidate_x;
        float candidate_z;
        volatile long double value = (long double)detail *
            terrain_read_float(memory, 0x00574d98u);
        value /= (long double)clearance *
            terrain_read_float(memory, 0x00574d9cu);
        value = (long double)terrain_read_float(memory, 0x00574d3cu) - value;
        if (value < (long double)terrain_read_float(memory, 0x00574da0u))
            value = terrain_read_float(memory, 0x00574da0u);
        obstruction_scale = terrain_store_float(value);
        candidate_x = terrain_store_float((long double)velocity_x *
            obstruction_scale + x);
        candidate_z = terrain_store_float((long double)velocity_z *
            obstruction_scale + z);

        value = (long double)clearance *
            terrain_read_float(memory, 0x00574d70u);
        if ((long double)detail <= value) {
            long double candidate_detail = gameplay_geometry_detail_height(memory,
                candidate_x, candidate_z);
            if (!memory->fault && (long double)detail <= candidate_detail) {
                x = candidate_x;
                z = candidate_z;
            }
        } else {
            x = candidate_x;
            z = candidate_z;
        }
    }

    if ((int32_t)terrain_float_bits(x) < (int32_t)0x3f800000u)
        x = terrain_bits_float(0x3f800000u);
    else if (terrain_read_float(memory, 0x00574da4u) <= x)
        x = terrain_read_float(memory, 0x00574da4u);
    if ((int32_t)terrain_float_bits(z) < (int32_t)0x3f800000u)
        z = terrain_bits_float(0x3f800000u);
    else if (terrain_read_float(memory, 0x00574da4u) <= z)
        z = terrain_read_float(memory, 0x00574da4u);
    frame->pose->local_position[0] = x;
    frame->pose->local_position[2] = z;
}

static void terrain_sample_footprint(GameMemory *memory, TerrainFrame *frame,
    TerrainContactPoint points[4], float *yaw_cos, float *yaw_sin) {

    uint16_t yaw = (uint16_t)(frame->pose->local_angles[2] +
        frame->rig->motion.yaw_step);
    float origin_x = frame->pose->local_position[0];
    float origin_z = frame->pose->local_position[2];
    float cosine;
    float sine;
    unsigned index;

    yaw &= 0x07ffu;
    frame->pose->local_angles[2] = yaw;
    cosine = terrain_trig(memory, (int16_t)yaw + 0x200);
    sine = terrain_trig(memory, yaw);
    for (index = 0u; index < 4u; index++) {
        float local_x = frame->rig->ground_corners[index][0];
        float local_z = frame->rig->ground_corners[index][2];
        float detail;
        long double height;
        points[index].x = terrain_rotated_coordinate(local_x, cosine,
            local_z, sine, origin_x);
        points[index].z = terrain_rotated_coordinate(local_z, cosine,
            local_x, -sine, origin_z);
        height = gameplay_geometry_detail_height(memory, points[index].x,
            points[index].z);
        detail = terrain_store_float(height);
        height = gameplay_geometry_terrain_height(memory, points[index].x,
            points[index].z);
        points[index].y = terrain_store_float(height +
            (long double)detail);
        if (memory->fault) return;
    }
    *yaw_cos = cosine;
    *yaw_sin = sine;
}

static TerrainNormal terrain_triangle_normal(const TerrainContactPoint *a,
    const TerrainContactPoint *b, const TerrainContactPoint *c) {
    TerrainNormal normal;
    float ab_x = terrain_difference(a->x, b->x);
    float ab_y = terrain_difference(a->y, b->y);
    float ab_z = terrain_difference(a->z, b->z);
    float cb_x = terrain_difference(c->x, b->x);
    float cb_y = terrain_difference(c->y, b->y);
    float cb_z = terrain_difference(c->z, b->z);
    float length_squared;
    float length;
    long double sum;

    normal.x = terrain_cross_component(cb_z, ab_y, cb_y, ab_z);
    normal.y = terrain_cross_component(cb_x, ab_z, cb_z, ab_x);
    normal.z = terrain_cross_component(cb_y, ab_x, cb_x, ab_y);
    sum = (long double)normal.z * normal.z;
    sum += (long double)normal.y * normal.y;
    sum += (long double)normal.x * normal.x;
    length_squared = terrain_store_float(sum);
    {
        long double root = sqrtl((long double)length_squared);
        length = terrain_store_float(root);
        if (root != 1.0L &&
            (terrain_float_bits(length) & 0x7fffffffu) != 0u) {
            volatile long double inverse = 1.0L / (long double)length;
            volatile long double scaled = (long double)normal.x * inverse;
            normal.x = terrain_store_float(scaled);
            scaled = (long double)normal.y * inverse;
            normal.y = terrain_store_float(scaled);
            scaled = (long double)normal.z * inverse;
            normal.z = terrain_store_float(scaled);
        }
    }
    return normal;
}

static TerrainNormal terrain_supporting_normal(
    const TerrainContactPoint points[4]) {
    TerrainNormal normal = terrain_triangle_normal(&points[0], &points[1],
        &points[2]);
    volatile long double side =
        (long double)(points[3].z - points[1].z) * normal.z;
    side += (long double)(points[3].y - points[1].y) * normal.y;
    side += (long double)(points[3].x - points[1].x) * normal.x;
    if (side > 0.0L)
        normal = terrain_triangle_normal(&points[1], &points[2], &points[3]);
    return normal;
}

static float terrain_ground_height(TerrainFrame *frame,
    const TerrainContactPoint *point, const TerrainNormal *normal) {
    volatile long double value = (long double)normal->y * point->y;
    value += (long double)normal->z * point->z;
    value += (long double)normal->x * point->x;
    value -= (long double)frame->pose->local_position[2] *
        normal->z;
    value -= (long double)frame->pose->local_position[0] *
        normal->x;
    value += frame->clearance;
    value /= normal->y;
    return terrain_store_float(value);
}

static void terrain_emit_landing_sound(GameMemory *memory, TerrainFrame *frame,
    float velocity_y, const GamePlayerTerrainContactOps *ops) {
    uint32_t arguments[6] = {0u, 0u, 0u, 0u, 0u, 0u};
    int32_t x = terrain_truncate(
        (long double)frame->pose->position[0]);
    int32_t z = terrain_truncate(
        (long double)frame->pose->position[2]);
    uint32_t terrain = game_memory_read_u32(memory, 0x00597d90u);
    uint32_t x_offset = (uint32_t)((int32_t)((uint32_t)x &
        0xfffffe3fu) >> 6);
    uint32_t z_offset = ((uint32_t)z & 0xfffffe00u) * 4u;
    uint8_t material = memory->terrain && x_offset+z_offset<65536u*8u ?
        memory->terrain->cells[(x_offset+z_offset)/8u].flags :
        game_memory_read_u8(memory,terrain+x_offset+z_offset+5u);

    if (memory->fault) return;
    if (material & 0x50u) {
        arguments[0] = 0x85u;
        arguments[2] = terrain_float_bits(1.0f);
    } else {
        float limited = velocity_y <= terrain_read_float(memory, 0x00574db0u)
            ? terrain_read_float(memory, 0x00574db0u) : velocity_y;
        volatile long double gain = (long double)limited +
            terrain_read_float(memory, 0x00574d6cu);
        float stored_gain;
        gain *= terrain_read_float(memory, 0x00574db4u);
        if (gain < 0.0L) gain = -gain;
        stored_gain = terrain_store_float(gain);
        if (stored_gain < terrain_read_float(memory, 0x00574db8u))
            stored_gain = terrain_read_float(memory, 0x00574db8u);
        arguments[2] = terrain_float_bits(stored_gain);
    }
    if(!ops || !ops->landing_sound || !ops->landing_sound(ops->context,
            game_memory_read_u32(memory,0x00628a90u),arguments))memory->fault=1;
}

static void terrain_resolve_vertical(GameMemory *memory, TerrainFrame *frame,
    const TerrainContactPoint points[4], TerrainNormal *normal,
    const GamePlayerTerrainContactOps *ops) {
    uint32_t normal_y_bits = terrain_float_bits(normal->y);
    float ground;
    float current_y = frame->pose->local_position[1];
    float velocity_y = frame->rig->controller_velocity[1];
    float proposed_y;
    uint32_t flags;

    if ((int32_t)normal_y_bits <= 0 || normal_y_bits > 0x3f800000u) {
        normal->x = frame->rig->motion.surface_normal[0];
        normal->y = frame->rig->motion.surface_normal[1];
        normal->z = frame->rig->motion.surface_normal[2];
    }
    ground = terrain_ground_height(frame, &points[1], normal);
    proposed_y = terrain_add(current_y, velocity_y);
    flags = frame->controls->mode_flags;
    if (proposed_y > ground) {
        frame->pose->local_position[1] = proposed_y;
        frame->controls->mode_flags = flags & ~0x20u;
        normal->x = frame->rig->motion.surface_normal[0];
        normal->z = frame->rig->motion.surface_normal[2];
        return;
    }

    {
        volatile long double speed_squared = (long double)velocity_y *
            velocity_y;
        float velocity_x = frame->rig->controller_velocity[0];
        float velocity_z = frame->rig->controller_velocity[2];
        float stored_speed;
        int32_t mode = (int32_t)game_memory_read_u32(memory, 0x00594d38u);
        speed_squared += (long double)velocity_x * velocity_x;
        speed_squared += (long double)velocity_z * velocity_z;
        stored_speed = terrain_store_float(speed_squared);
        if (!(flags & 0x20u) &&
            terrain_float_bits(velocity_y) > 0xc2a00000u &&
            (long double)(mode == 2 ? 2 : 1) *
                terrain_read_float(memory, 0x00574dacu) < stored_speed)
            terrain_emit_landing_sound(memory, frame, velocity_y, ops);
        if (memory->fault) return;

        if (current_y > ground) {
            volatile long double response = (long double)current_y - ground;
            response = (long double)terrain_read_float(memory,
                mode == 2 ? 0x00574dbcu : 0x00574dc0u) - response;
            frame->rig->controller_velocity[1] = terrain_store_float(response);
        } else {
            (frame->rig->controller_velocity[1] = terrain_read_float(memory,
                    mode == 2 ? 0x00574dbcu : 0x00574dc0u));
        }
    }
    frame->pose->local_position[1] = ground;
    frame->controls->mode_flags = flags | 0x20u;
    frame->rig->motion.surface_normal[0] = normal->x;
    frame->rig->motion.surface_normal[1] = normal->y;
    frame->rig->motion.surface_normal[2] = normal->z;
}

static int16_t terrain_inverse_sine(GameMemory *memory, float value) {
    uint32_t bits = terrain_float_bits(value);
    int32_t index;
    if ((int32_t)bits > (int32_t)0x3f666666u) {
        volatile long double scaled = (long double)value -
            terrain_read_float(memory, 0x00574dc8u);
        scaled *= terrain_read_float(memory, 0x00574dccu);
        index = terrain_truncate(scaled);
        return (int16_t)game_memory_read_u16(memory,
            0x0062b8a8u + (uint32_t)index * 2u);
    }
    if (bits > 0xbf666666u) {
        volatile long double scaled = (long double)terrain_read_float(memory,
            0x00574d48u) - value;
        scaled *= terrain_read_float(memory, 0x00574dccu);
        index = terrain_truncate(scaled);
        return (int16_t)-(int16_t)game_memory_read_u16(memory,
            0x0062b8a8u + (uint32_t)index * 2u);
    }
    {
        volatile long double scaled = (long double)value *
            terrain_read_float(memory, 0x00574d94u);
        scaled += terrain_read_float(memory, 0x00574d94u);
        index = terrain_truncate(scaled);
    }
    return (int16_t)game_memory_read_u16(memory,
        0x0062f8b8u + (uint32_t)index * 2u);
}

static void terrain_update_attitude(GameMemory *memory, TerrainFrame *frame,
    TerrainNormal normal, float yaw_cos, float yaw_sin) {
    volatile long double slope_value = (long double)normal.z * yaw_sin;
    float slope;
    float ratio;
    int16_t slope_angle;
    int16_t pitch_angle;
    int16_t filtered_roll;
    int16_t filtered_pitch;
    int16_t turret_elevation;
    int16_t correction;

    slope_value -= (long double)normal.x * yaw_cos;
    slope = terrain_store_float(slope_value);
    if (slope < terrain_read_float(memory, 0x00574dc4u))
        slope = terrain_read_float(memory, 0x00574dc4u);
    else if (terrain_read_float(memory, 0x00574d3cu) <= slope)
        slope = terrain_read_float(memory, 0x00574d3cu);
    slope_angle = terrain_inverse_sine(memory, slope);

    if ((terrain_float_bits(yaw_sin) & 0x7fffffffu) == 0u) {
        volatile long double numerator = (long double)normal.z -
            (long double)yaw_sin * slope;
        volatile long double denominator = (long double)yaw_cos *
            terrain_trig(memory, (int16_t)(slope_angle + 0x200));
        ratio = terrain_store_float(numerator / denominator);
    } else {
        volatile long double numerator = (long double)yaw_cos * slope;
        volatile long double denominator;
        numerator += normal.x;
        denominator = (long double)yaw_sin *
            terrain_trig(memory, (int16_t)(slope_angle + 0x200));
        ratio = terrain_store_float(numerator / denominator);
    }
    pitch_angle = terrain_inverse_sine(memory, ratio);

    filtered_roll = terrain_divide_four(
        (int32_t)(int16_t)frame->rig->motion.filtered_roll * 3 +
        slope_angle);
    filtered_pitch = terrain_divide_four(
        (int32_t)(int16_t)frame->pose->local_angles[1] * 3 +
        pitch_angle);
    frame->rig->motion.filtered_roll = (uint16_t)filtered_roll;
    frame->pose->local_angles[1] = (uint16_t)filtered_pitch;

    turret_elevation = (int16_t)frame->controls->previous_aim;
    if (turret_elevation > -113) {
        correction = (int16_t)frame->rig->motion.lean;
    } else if (turret_elevation < -284) {
        correction = 0;
    } else {
        volatile long double scaled = (long double)(-113 -
            turret_elevation) * terrain_read_float(memory, 0x00574dd0u);
        scaled *= (int16_t)frame->rig->motion.lean;
        correction = (int16_t)terrain_truncate(scaled);
    }
    frame->pose->local_angles[0] = (uint16_t)(int16_t)(correction + filtered_roll);
}

static void terrain_apply_turret_controls(GameMemory *memory,
    TerrainFrame *frame) {
    int16_t elevation_delta = (int16_t)frame->controls->input_axes[0];
    int16_t azimuth_delta = (int16_t)frame->controls->input_axes[1];
    uint32_t pressed = frame->controls->input_flags;
    uint32_t blocked = frame->controls->previous_input_flags;
    int16_t elevation = (int16_t)frame->controls->previous_aim;

    if ((pressed & 0x40u) && !(blocked & 0x80u)) {
        volatile long double amount =
            (long double)frame->rig->motion.turret_scale *
            terrain_read_float(memory, 0x00574dd4u);
        elevation_delta = (int16_t)(elevation_delta -
            (int16_t)terrain_truncate(amount));
    }
    if ((pressed & 0x80u) && !(blocked & 0x40u)) {
        volatile long double amount =
            (long double)frame->rig->motion.turret_scale *
            terrain_read_float(memory, 0x00574dd4u);
        elevation_delta = (int16_t)(elevation_delta +
            (int16_t)terrain_truncate(amount));
    }
    if (elevation_delta || azimuth_delta) {
        uint16_t azimuth = (uint16_t)(frame->controls->previous_heading + (uint16_t)azimuth_delta);
        int16_t bias = (int16_t)frame->rig->smoothed_heading;
        int32_t adjusted;
        azimuth &= 0x07ffu;
        frame->controls->previous_heading = azimuth;
        elevation = (int16_t)(elevation + elevation_delta);
        frame->controls->previous_aim = (uint16_t)elevation;
        adjusted = elevation;
        if (adjusted < -398 - (int32_t)bias)
            elevation = (int16_t)(-398 - bias);
        else if (adjusted > 227 - (int32_t)bias)
            elevation = (int16_t)(227 - bias);
        frame->controls->previous_aim = (uint16_t)elevation;
    }
}

int player_update_terrain_contact(GameMemory *memory,
    PresentationPose *pose, PlayerControls *controls, PlayerRig *rig,
    float clearance, uint32_t initial_result,
    const GamePlayerTerrainContactOps *ops, uint32_t *final_result) {
    TerrainContactPoint points[4];
    TerrainNormal normal;
    float yaw_cos;
    float yaw_sin;
    TerrainFrame state={pose,controls,rig,clearance},*frame=&state;
    uint32_t result = initial_result;

    if(!pose || !controls || !rig)return 0;
    terrain_integrate_horizontal(memory, frame);
    if (memory->fault) return 0;
    terrain_sample_footprint(memory, frame, points, &yaw_cos, &yaw_sin);
    if (memory->fault) return 0;
    normal = terrain_supporting_normal(points);
    terrain_resolve_vertical(memory, frame, points, &normal, ops);
    if (memory->fault) return 0;
    terrain_update_attitude(memory, frame, normal, yaw_cos, yaw_sin);
    if (memory->fault) return 0;
    terrain_apply_turret_controls(memory, frame);
    if (memory->fault) return 0;

    if (!player_rig_build(memory,pose,controls,rig,1) ||
        !player_transform_vertex_ranges(memory,pose,rig,1))return 0;
    result=(uint32_t)(int32_t)(int16_t)rig->mount_part;
    if (final_result) *final_result = result;
    return !memory->fault;
}
