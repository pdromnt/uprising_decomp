/* Native player control/velocity integration for retail 0x004f3d10.

   This routine turns the current input bitfields into horizontal velocity,
   applies grounded drag or airborne gravity, updates steering/body lean, caps
   horizontal speed, and smooths the engine-audio scalar.  The original x87
   code deliberately keeps a few intermediates at 80-bit precision while
   spilling others to float; the helpers below make those store boundaries
   explicit so the native path remains deterministic. */
#include "player_motion.h"
#include "scene_instance_owner.h"

typedef struct MotionFrame { PlayerControls *controls; PlayerRig *rig; float max_speed; } MotionFrame;

#include <math.h>
#include <stdint.h>
#include <string.h>


enum PlayerMotionInput {
    PLAYER_INPUT_FORWARD = 0x01u,
    PLAYER_INPUT_REVERSE = 0x02u,
    PLAYER_INPUT_STEER_LEFT = 0x04u,
    PLAYER_INPUT_STEER_RIGHT = 0x08u,
    PLAYER_INPUT_STRAFE_LEFT = 0x10u,
    PLAYER_INPUT_STRAFE_RIGHT = 0x20u
};

static uint32_t motion_float_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static float motion_bits_float(uint32_t bits) {
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static float motion_read_float(GameMemory *memory, uint32_t address) {
    return motion_bits_float(game_memory_read_u32(memory, address));
}


/* Model an x87 FST/FSTP to a 32-bit destination. */
static float motion_store_float(long double value) {
    volatile float stored = (float)value;
    return stored;
}

static long double motion_square_sum(float first, float second) {
    volatile long double first_square = (long double)first * first;
    volatile long double second_square = (long double)second * second;
    return first_square + second_square;
}

static float motion_add_scaled(float value, float direction, float amount) {
    volatile long double result = (long double)direction * amount;
    result += value;
    return motion_store_float(result);
}

static float motion_subtract_scaled(float value, float direction,
    long double amount) {
    volatile long double result = (long double)direction * amount;
    result = (long double)value - result;
    return motion_store_float(result);
}

/* Retail removes a fixed amount from a signed component without crossing 0. */
static float motion_signed_dead_zone(float value, long double sign_value,
    float dead_zone) {
    volatile long double adjusted;

    if (sign_value > 0.0L) {
        adjusted = (long double)value - dead_zone;
        if (!(adjusted > 0.0L)) adjusted = 0.0L;
    } else {
        adjusted = (long double)dead_zone + value;
        if (!(adjusted < 0.0L)) adjusted = 0.0L;
    }
    return motion_store_float(adjusted);
}

static int16_t motion_approach_i16(int16_t current, int32_t target,
    int32_t step) {
    int32_t next = current;
    if (next < target) {
        next += step;
        if (next >= target) next = target;
    } else {
        next -= step;
        if (next <= target) next = target;
    }
    return (int16_t)next;
}

static int16_t motion_increment_clamped(int16_t value) {
    int32_t next = (int32_t)value + 1;
    if (next >= 12) next = 12;
    return (int16_t)next;
}

static int16_t motion_decrement_to_zero(int16_t value) {
    int32_t next = (int32_t)value - 1;
    if (next <= 0) next = 0;
    return (int16_t)next;
}

static int16_t motion_truncate_low_i16(long double value) {
    int64_t integer = (int64_t)value;
    return (int16_t)(uint16_t)integer;
}

static long double motion_horizontal_dot(float basis_x, float velocity_x,
    float basis_z, float velocity_z) {
    volatile long double first = (long double)basis_x * velocity_x;
    volatile long double second = (long double)basis_z * velocity_z;
    return first + second;
}

static void motion_remove_idle_component(GameMemory *memory, MotionFrame *frame,
    float basis_x, float basis_z) {
    float velocity_x = frame->rig->controller_velocity[0];
    float velocity_z = frame->rig->controller_velocity[2];
    long double dot_extended = motion_horizontal_dot(basis_x, velocity_x,
        basis_z, velocity_z);
    float dot = motion_store_float(dot_extended);
    float remainder = motion_signed_dead_zone(dot, dot_extended,
        motion_read_float(memory, 0x00574de8u));
    volatile long double removed = (long double)dot - remainder;

    frame->rig->controller_velocity[0] = motion_subtract_scaled(velocity_x, basis_x, removed);
    frame->rig->controller_velocity[2] = motion_subtract_scaled(velocity_z, basis_z, removed);
}

static float motion_input_amount(GameMemory *memory,
    float scale, uint32_t normal_constant,
    uint32_t fast_constant, float traction) {
    volatile long double amount =
        (long double)scale * traction;
    amount *= motion_read_float(memory,
        game_memory_read_u32(memory, 0x00594d38u) == 2u ?
            fast_constant : normal_constant);
    return motion_store_float(amount);
}

static void motion_apply_ground_controls(GameMemory *memory, MotionFrame *frame,
    const float *basis, float normalized_slope, float traction) {
    uint32_t input = frame->controls->input_flags;
    uint32_t blocked = frame->controls->previous_input_flags;
    float velocity_x = frame->rig->controller_velocity[0];
    float velocity_z = frame->rig->controller_velocity[2];
    float basis_forward_x = basis[1];
    float basis_forward_z = basis[9];
    float basis_drive_x = basis[2];
    float basis_drive_z = basis[10];
    float basis_strafe_x = basis[0];
    float basis_strafe_z = basis[8];

    if (fabsf(velocity_x) > motion_read_float(memory, 0x00574dd8u)) {
        volatile long double factor = (long double)traction *
            motion_read_float(memory, 0x00574ddcu);
        factor += motion_read_float(memory, 0x00574d3cu);
        velocity_x = motion_store_float(factor * velocity_x);
    }
    if (fabsf(velocity_z) > motion_read_float(memory, 0x00574dd8u)) {
        volatile long double factor = (long double)traction *
            motion_read_float(memory, 0x00574ddcu);
        factor += motion_read_float(memory, 0x00574d3cu);
        velocity_z = motion_store_float(factor * velocity_z);
    }

    {
        volatile long double amount = (long double)normalized_slope *
            motion_read_float(memory, 0x00574de0u);
        velocity_x = motion_add_scaled(velocity_x, basis_forward_x,
            motion_store_float(amount));
        velocity_z = motion_add_scaled(velocity_z, basis_forward_z,
            motion_store_float(amount));
    }
    frame->rig->controller_velocity[0] = velocity_x;
    frame->rig->controller_velocity[2] = velocity_z;

    if ((input & PLAYER_INPUT_FORWARD) &&
        !(blocked & PLAYER_INPUT_REVERSE)) {
        float amount = motion_input_amount(memory,
            frame->rig->motion.forward_scale, 0x00574dd4u, 0x00574de4u,
            traction);
        velocity_x = motion_add_scaled(velocity_x, basis_drive_x, amount);
        velocity_z = motion_add_scaled(velocity_z, basis_drive_z, amount);
        frame->rig->controller_velocity[0] = velocity_x;
        frame->rig->controller_velocity[2] = velocity_z;
    }
    if ((input & PLAYER_INPUT_REVERSE) &&
        !(blocked & PLAYER_INPUT_FORWARD)) {
        float amount = motion_input_amount(memory,
            frame->rig->motion.forward_scale, 0x00574dd4u, 0x00574de4u,
            traction);
        velocity_x = motion_subtract_scaled(velocity_x, basis_drive_x,
            amount);
        velocity_z = motion_subtract_scaled(velocity_z, basis_drive_z,
            amount);
        frame->rig->controller_velocity[0] = velocity_x;
        frame->rig->controller_velocity[2] = velocity_z;
    }
    if ((input & (PLAYER_INPUT_FORWARD | PLAYER_INPUT_REVERSE)) == 0u)
        motion_remove_idle_component(memory, frame, basis_drive_x,
            basis_drive_z);

    velocity_x = frame->rig->controller_velocity[0];
    velocity_z = frame->rig->controller_velocity[2];
    if ((input & PLAYER_INPUT_STRAFE_LEFT) &&
        !(blocked & PLAYER_INPUT_STRAFE_RIGHT)) {
        float amount = motion_input_amount(memory,
            frame->rig->motion.strafe_scale, 0x00574df0u, 0x00574decu,
            traction);
        velocity_x = motion_subtract_scaled(velocity_x, basis_strafe_x,
            amount);
        velocity_z = motion_subtract_scaled(velocity_z, basis_strafe_z,
            amount);
        frame->rig->controller_velocity[0] = velocity_x;
        frame->rig->controller_velocity[2] = velocity_z;
        frame->rig->motion.lean = (uint16_t)motion_approach_i16((int16_t)frame->rig->motion.lean, 28, 7);
    }
    if ((input & PLAYER_INPUT_STRAFE_RIGHT) &&
        !(blocked & PLAYER_INPUT_STRAFE_LEFT)) {
        float amount = motion_input_amount(memory,
            frame->rig->motion.strafe_scale, 0x00574df0u, 0x00574decu,
            traction);
        velocity_x = motion_add_scaled(velocity_x, basis_strafe_x, amount);
        velocity_z = motion_add_scaled(velocity_z, basis_strafe_z, amount);
        frame->rig->controller_velocity[0] = velocity_x;
        frame->rig->controller_velocity[2] = velocity_z;
        frame->rig->motion.lean = (uint16_t)motion_approach_i16((int16_t)frame->rig->motion.lean, -28, 7);
    }
    if ((input & (PLAYER_INPUT_STRAFE_LEFT |
            PLAYER_INPUT_STRAFE_RIGHT)) == 0u)
        motion_remove_idle_component(memory, frame, basis_strafe_x,
            basis_strafe_z);
}

static void motion_apply_airborne_gravity(GameMemory *memory, MotionFrame *frame) {
    float gravity = motion_read_float(memory,
        game_memory_read_u32(memory, 0x00594d38u) == 2u ?
            0x00574df4u : 0x00574df8u);
    volatile long double velocity_y =
        frame->rig->controller_velocity[1];
    velocity_y -= gravity;
    frame->rig->controller_velocity[1] = motion_store_float(velocity_y);
}

static void motion_update_steering(GameMemory *memory, MotionFrame *frame) {
    int16_t original = (int16_t)frame->rig->motion.steering;
    uint16_t updated_bits = (uint16_t)original;
    uint32_t input = frame->controls->input_flags;
    uint32_t blocked = frame->controls->previous_input_flags;
    int16_t turn_step = 0;
    int16_t updated;

    if (((input & PLAYER_INPUT_STEER_RIGHT) &&
            !(blocked & PLAYER_INPUT_STEER_LEFT)) ||
        ((input & PLAYER_INPUT_STEER_LEFT) &&
            !(blocked & PLAYER_INPUT_STEER_RIGHT))) {
        volatile long double turn_amount =
            (long double)frame->rig->motion.turn_scale *
            motion_read_float(memory, 0x00574dfcu);
        turn_step = motion_truncate_low_i16(turn_amount);
    }

    if ((input & PLAYER_INPUT_STEER_RIGHT) &&
        !(blocked & PLAYER_INPUT_STEER_LEFT))
        updated_bits = (uint16_t)(updated_bits + (uint16_t)turn_step);
    if ((input & PLAYER_INPUT_STEER_LEFT) &&
        !(blocked & PLAYER_INPUT_STEER_RIGHT))
        updated_bits = (uint16_t)(updated_bits - (uint16_t)turn_step);
    updated = (int16_t)updated_bits;

    if (updated != 0) {
        int16_t steer_ticks = (int16_t)frame->rig->motion.steer_ticks;
        frame->rig->motion.yaw_step = (uint16_t)updated;
        frame->rig->motion.steer_ticks = (uint16_t)motion_increment_clamped(steer_ticks);
        if (original == 0) {
            volatile long double half = (long double)updated *
                motion_read_float(memory, 0x00574d9cu);
            int32_t lean_target = -(int32_t)motion_truncate_low_i16(half);
            frame->rig->motion.lean = (uint16_t)motion_approach_i16((int16_t)frame->rig->motion.lean, lean_target, 7);
        }
        frame->rig->motion.idle_ticks = 0u;
    } else {
        frame->rig->motion.yaw_step = 0u;
        if ((input & (PLAYER_INPUT_STRAFE_LEFT |
                PLAYER_INPUT_STRAFE_RIGHT)) == 0u) {
            (frame->rig->motion.idle_ticks = (uint16_t)motion_increment_clamped((int16_t)
                    frame->rig->motion.idle_ticks));
            frame->rig->motion.lean = (uint16_t)motion_approach_i16((int16_t)frame->rig->motion.lean, 0, 5);
            (frame->rig->motion.steer_ticks = (uint16_t)motion_decrement_to_zero((int16_t)
                    frame->rig->motion.steer_ticks));
        }
    }
}

static float motion_cap_horizontal_speed(GameMemory *memory, MotionFrame *frame,
    float normalized_slope) {
    float velocity_x = frame->rig->controller_velocity[0];
    float velocity_z = frame->rig->controller_velocity[2];
    long double squared_extended = motion_square_sum(velocity_x, velocity_z);
    float squared = motion_store_float(squared_extended);
    float speed = motion_store_float(sqrtl((long double)squared));
    volatile long double attenuation_extended =
        (long double)frame->rig->motion.speed_drag *
        normalized_slope;
    float attenuation;
    long double speed_limit_extended;
    float speed_limit;
    long double squared_limit_extended;
    float squared_limit;

    attenuation_extended =
        (long double)motion_read_float(memory, 0x00574d3cu) -
        attenuation_extended;
    attenuation = motion_store_float(attenuation_extended);
    if (!(attenuation >= motion_read_float(memory, 0x00574e00u)))
        attenuation = motion_read_float(memory, 0x00574e00u);
    else if (attenuation >= motion_read_float(memory, 0x00574d3cu))
        attenuation = motion_read_float(memory, 0x00574d3cu);

    speed_limit_extended =
        (long double)frame->max_speed * attenuation;
    speed_limit = motion_store_float(speed_limit_extended);
    squared_limit_extended = speed_limit_extended * speed_limit;
    if (game_memory_read_u32(memory, 0x00594d38u) == 2u)
        squared_limit_extended *= motion_read_float(memory, 0x00574e04u);
    squared_limit = motion_store_float(squared_limit_extended);

    if ((long double)squared_limit < (long double)squared) {
        volatile long double ratio = (long double)speed_limit / speed;
        frame->rig->controller_velocity[0] = motion_store_float((long double)velocity_x * ratio);
        frame->rig->controller_velocity[2] = motion_store_float((long double)velocity_z * ratio);
    }
    return speed;
}

static int motion_update_engine_scalar(GameMemory *memory, MotionFrame *frame,
    float speed, const GamePlayerMotionOps *ops) {
    uint32_t flags = frame->controls->mode_flags;
    float target;
    float current = frame->rig->motion.engine_scalar;
    uint32_t controller;
    float next;

    if (!(flags & 0x20u)) {
        target = motion_read_float(memory, 0x00574e0cu);
    } else {
        float max_speed = frame->max_speed;
        float bounded_speed = speed > max_speed ? max_speed : speed;
        volatile long double value = (long double)bounded_speed / max_speed;
        value *= motion_read_float(memory, 0x00574e08u);
        value += motion_read_float(memory, 0x00574d3cu);
        target = motion_store_float(value);
    }
    if (current == target || isnan(current) || isnan(target)) return !memory->fault;

    controller = game_memory_read_u32(memory, 0x005f0208u);
    if (game_memory_read_u32(memory, controller + 0x3bcu) !=
        game_memory_read_u32(memory, controller + 0x448u))
        return !memory->fault;

    if (current <= target) {
        volatile long double candidate = (long double)current +
            motion_read_float(memory, 0x00574e10u);
        next = candidate < (long double)target ?
            motion_store_float(candidate) : target;
    } else {
        volatile long double candidate = (long double)current -
            motion_read_float(memory, 0x00574e10u);
        next = (long double)target < candidate ?
            motion_store_float(candidate) : target;
    }

    frame->rig->motion.engine_scalar = next;
    {
        GameAddress sound_manager = game_memory_read_u32(memory,
            0x00628a90u);
        if (memory->fault || !ops || !ops->update_engine_scalar)
            return 0;
        return ops->update_engine_scalar(ops->context, sound_manager,
            motion_float_bits(next));
    }
}

int player_integrate_motion(GameMemory *memory, PresentationPose *pose,
    PlayerControls *controls, PlayerRig *rig, float max_speed,
    const GamePlayerMotionOps *ops) {
    float foreign_basis[12];
    const float *basis;
    MotionFrame state={controls,rig,max_speed},*frame=&state;
    unsigned i;
    float slope_length;
    float threshold;
    float maximum;
    float dead_zone;
    long double normalized_extended;
    float normalized;
    long double traction_extended;
    float traction;
    uint32_t flags;
    float speed;

    threshold = motion_read_float(memory, 0x00629b50u);
    maximum = motion_read_float(memory, 0x00629dfcu);
    if (!pose || !controls || !rig) return 0;
    if (pose->model_instance) basis=pose->model_instance->local;
    else {
        for(i=0u;i<12u;++i)foreign_basis[i]=motion_read_float(memory,pose->model+4u+i*4u);
        basis=foreign_basis;
    }
    if (memory->fault) return 0;

    {
        float axis_x = basis[1];
        float axis_z = basis[9];
        float squared = motion_store_float(motion_square_sum(axis_z, axis_x));
        slope_length = motion_store_float(sqrtl((long double)squared));
    }
    dead_zone = motion_signed_dead_zone(slope_length, slope_length,
        threshold);
    normalized_extended =
        (long double)motion_read_float(memory, 0x00574d3cu) /
        ((long double)maximum - threshold);
    normalized_extended *= dead_zone;
    normalized = motion_store_float(normalized_extended);

    traction_extended = normalized_extended *
        frame->rig->motion.slope_drag;
    traction_extended =
        (long double)motion_read_float(memory, 0x00574d3cu) -
        traction_extended;
    traction = motion_store_float(traction_extended);
    if (!(traction >= motion_read_float(memory, 0x00574d30u)))
        traction = motion_read_float(memory, 0x00574d30u);
    else if (traction >= motion_read_float(memory, 0x00574d3cu))
        traction = motion_read_float(memory, 0x00574d3cu);

    flags = frame->controls->mode_flags;
    if ((flags & 0x30u) == 0u)
        motion_apply_airborne_gravity(memory, frame);
    else
        motion_apply_ground_controls(memory, frame, basis, normalized,
            traction);
    if (memory->fault) return 0;

    motion_update_steering(memory, frame);
    if (memory->fault) return 0;
    speed = motion_cap_horizontal_speed(memory, frame, normalized);
    if (memory->fault) return 0;
    if (!motion_update_engine_scalar(memory, frame, speed, ops)) return 0;
    return !memory->fault;
}
