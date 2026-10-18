#include "gameplay_methods.h"

uint32_t gameplay_method_scalar_u32(uint32_t method_id) {
    switch (method_id) {
        case 0x00408c70u: case 0x00408fb0u: return 55u;
        case 0x00408c80u: case 0x00408fc0u:
        case 0x0040eb50u: case 0x004e83b0u: return 100u;
        case 0x00408c90u: case 0x00408fd0u: return 10u;
        case 0x0040eb40u: case 0x004e83d0u: return 50u;
        case 0x0040eb60u: return 70u;
        case 0x0040eb70u: case 0x004e83c0u: return 60u;
        case 0x0040eb80u: return 0x8000u;
        case 0x004e83a0u: return 35u;
        case 0x004e94b0u: case 0x005399e0u: return 0x2af8u;
        case 0x00512060u: return 1u;
        case 0x005399d0u: return 100u;
        case 0x005399f0u: case 0x00539a00u: return 50u;
        case 0x00539a10u: return 30u;
        default: return 0u;
    }
}

uint16_t gameplay_method_scalar_u16(uint32_t method_id) {
    switch (method_id) {
        case 0x00408fe0u: case 0x00408ca0u: case 0x004e8460u: return 0xfe00u;
        case 0x00408ff0u: case 0x00408cb0u: case 0x004e8470u: return 0x0100u;
        default: return 0u;
    }
}

void gameplay_method_set_pair(GameMemory *memory, GameAddress self,
    uint32_t first, uint32_t second) {
    game_memory_write_u32(memory, self + 0x1f4u, first);
    if (!memory->fault) game_memory_write_u32(memory, self + 0x1f0u, second);
    if (!memory->fault) game_memory_write_u32(memory, self + 0x1ecu, 1u);
    if (!memory->fault) game_memory_write_u32(memory, self + 0x1f8u, 1u);
}

void gameplay_method_set_dimensions(GameMemory *memory, GameAddress self,
    uint16_t first, uint16_t second) {
    uint8_t flags;
    game_memory_write_u16(memory, self + 0x148u, first);
    if (!memory->fault) game_memory_write_u16(memory, self + 0x14au, second);
    flags = game_memory_read_u8(memory, self + 0xd8u);
    if (!memory->fault) game_memory_write_u32(memory, self + 0x1ecu, 1u);
    if (!memory->fault) game_memory_write_u8(memory, self + 0xd8u, (uint8_t)(flags | 8u));
}

void gameplay_method_publish_launch_value(GameMemory *memory, uint32_t value) {
    game_memory_write_u32(memory, 0x005efeecu, value);
}

void gameplay_method_set_extent_pair(GameMemory *memory, GameAddress self,
    uint32_t first, uint32_t second) {
    game_memory_write_u32(memory, self + 0x18cu, first);
    if (!memory->fault) game_memory_write_u32(memory, self + 0x190u, second);
}

GameAddress gameplay_method_store_frame_slots(GameMemory *memory,
        GameAddress stack, uint32_t eax, uint32_t ecx, int store_all) {
    if (store_all) {
        game_memory_write_u32(memory, stack + 0x1cu, ecx);
        if (!memory->fault) game_memory_write_u32(memory, stack + 0x28u, ecx);
        if (!memory->fault) game_memory_write_u32(memory, stack + 0x2cu, ecx);
        if (!memory->fault) game_memory_write_u32(memory, stack + 0x20u, ecx);
    }
    if (!memory->fault) game_memory_write_u32(memory, stack + 0x24u, eax);
    return 0x0049b88eu;
}

uint32_t gameplay_method_read_global(GameMemory *memory,
        GameAddress address) {
    return game_memory_read_u32(memory, address);
}

GameplayMethodVirtualTargetResult gameplay_method_virtual_target(
        GameMemory *memory, GameAddress self, uint32_t slot_offset) {
    GameplayMethodVirtualTargetResult result = {0u, 0u};
    result.vtable = game_memory_read_u32(memory, self);
    if (!memory->fault)
        result.target = game_memory_read_u32(memory,
            result.vtable + slot_offset);
    return result;
}

GameplayMethodByteCompareResult gameplay_method_select_transport(
        GameMemory *memory, GameAddress source) {
    GameplayMethodByteCompareResult result = {16u, 0u, 0u, 1};
    GameAddress expected = 0x00577cd8u;
    game_memory_write_u32(memory, 0x005eff2cu, source);
    while (!memory->fault && result.remaining) {
        result.left = game_memory_read_u8(memory, source++);
        result.right = game_memory_read_u8(memory, expected++);
        if (memory->fault) break;
        --result.remaining;
        if (result.left != result.right) {
            result.equal = 0;
            break;
        }
    }
    if (!memory->fault)
        game_memory_write_u32(memory, 0x005eff20u,
            result.equal ? 1u : 0u);
    return result;
}

uint16_t gameplay_method_adjust_selection_index(GameMemory *memory,
        GameAddress self, int32_t delta) {
    uint16_t old_value = game_memory_read_u16(memory, self + 0x15cu);
    uint16_t new_value = (uint16_t)(old_value + delta);
    if (!memory->fault)
        game_memory_write_u16(memory, self + 0x15cu, new_value);
    return old_value;
}

static GameplayMethodCallStep gameplay_method_call_step(GameAddress target,
        GameAddress return_address) {
    GameplayMethodCallStep step = {0};
    step.target = target;
    step.return_address = return_address;
    return step;
}

GameplayMethodCallbackPlan gameplay_method_callback_prepare(
        GameMemory *memory, uint32_t method_id, uint32_t input) {
    GameplayMethodCallbackPlan plan = {0};
    plan.return_value = 1u;
    plan.set_return_value = 1;
    switch (method_id) {
        case 0x005248e0u:
            plan.steps[0] = gameplay_method_call_step(0x0048fd50u,
                0x005248e5u);
            plan.step_count = 1u;
            break;
        case 0x00520eb0u:
            plan.steps[0] = gameplay_method_call_step(0x004ce220u,
                0x00520ebbu);
            plan.steps[0].self = game_memory_read_u32(memory, input + 0xa0u);
            plan.steps[0].set_self = 1;
            plan.step_count = 1u;
            plan.outer_cleanup = 4u;
            plan.set_return_value = 0;
            break;
        case 0x00522880u:
            plan.steps[0] = gameplay_method_call_step(0x004ce220u,
                0x0052288bu);
            plan.steps[0].self = game_memory_read_u32(memory, 0x00627ab4u);
            plan.steps[0].set_self = 1;
            plan.step_count = 1u;
            plan.outer_cleanup = 4u;
            plan.set_return_value = 0;
            break;
        case 0x00523680u:
            plan.steps[0] = gameplay_method_call_step(0x0048fd70u,
                0x00523685u);
            plan.steps[1] = gameplay_method_call_step(0x0048fcd0u,
                0x0052368au);
            plan.step_count = 2u;
            break;
        case 0x005236a0u:
            plan.steps[0] = gameplay_method_call_step(0x0048fd70u,
                0x005236a5u);
            plan.step_count = 1u;
            break;
        case 0x00475750u:
            plan.steps[0] = gameplay_method_call_step(0x004ce220u,
                0x0047575bu);
            plan.steps[0].self = game_memory_read_u32(memory, 0x0058917cu);
            plan.steps[0].set_self = 1;
            plan.step_count = 1u;
            break;
        case 0x005321f0u:
            plan.steps[0] = gameplay_method_call_step(0x00531a50u,
                0x005321f7u);
            plan.steps[0].arguments[0] = 4u;
            plan.steps[0].argument_count = 1u;
            plan.steps[0].caller_cleanup = 4u;
            plan.step_count = 1u;
            break;
        case 0x0054d3a0u:
            game_memory_write_u32(memory, 0x005977c0u, input);
            plan.steps[0] = gameplay_method_call_step(0x00512620u,
                0x0054d3aeu);
            plan.step_count = 1u;
            break;
        case 0x0052c660u:
        case 0x004756f0u:
            plan.steps[0] = gameplay_method_call_step(0x004cddc0u,
                method_id == 0x0052c660u ? 0x0052c674u : 0x00475704u);
            plan.steps[0].self = game_memory_read_u32(memory, 0x0059d4c0u);
            plan.steps[0].set_self = 1;
            plan.steps[0].arguments[0] = method_id == 0x0052c660u ? 4u : 22u;
            plan.steps[0].arguments[1] = 1000u;
            plan.steps[0].arguments[2] = 0x45u;
            plan.steps[0].argument_count = 3u;
            plan.step_count = 1u;
            break;
        case 0x0052cae0u:
            plan.steps[0] = gameplay_method_call_step(0x0052e1b0u,
                0x0052caf3u);
            plan.steps[0].self = game_memory_read_u32(memory, 0x00628a90u);
            plan.steps[0].set_self = 1;
            plan.steps[0].arguments[0] = game_memory_read_u32(memory,
                0x00627fa0u);
            plan.steps[0].arguments[1] = 0x38u;
            plan.steps[0].argument_count = 2u;
            plan.step_count = 1u;
            break;
        case 0x00475480u:
            game_memory_write_u32(memory, 0x005eff30u, 1u);
            plan.steps[0] = gameplay_method_call_step(0x00521c30u,
                0x0047549eu);
            plan.steps[0].self = input;
            plan.steps[0].set_self = 1;
            plan.steps[0].arguments[0] =
                game_memory_read_u32(memory, 0x005f0208u) + 0x31cu;
            plan.steps[0].argument_count = 1u;
            plan.step_count = 1u;
            break;
        case 0x0052fce0u:
            plan.steps[0] = gameplay_method_call_step(0x00531a50u,
                0x0052fce7u);
            plan.steps[0].arguments[0] = 0u;
            plan.steps[0].argument_count = 1u;
            plan.steps[0].caller_cleanup = 4u;
            plan.steps[1] = gameplay_method_call_step(0x00531a50u,
                0x0052fcf1u);
            plan.steps[1].arguments[0] = 1u;
            plan.steps[1].argument_count = 1u;
            plan.steps[1].caller_cleanup = 4u;
            plan.step_count = 2u;
            break;
        case 0x004c7570u:
            plan.steps[0] = gameplay_method_call_step(0x0051f650u,
                0x004c757du);
            plan.steps[0].self_source_address = 0x005910fcu;
            plan.steps[0].arguments[0] = 0u;
            plan.steps[0].argument_count = 1u;
            plan.steps[1] = gameplay_method_call_step(0x0051f670u,
                0x004c758au);
            plan.steps[1].self_source_address = 0x005910fcu;
            plan.steps[1].arguments[0] = 0u;
            plan.steps[1].argument_count = 1u;
            plan.step_count = 2u;
            break;
        case 0x004c7550u:
            plan.steps[0] = gameplay_method_call_step(0x0051dcb0u,
                0x004c755au);
            plan.steps[0].arguments[0] = input;
            plan.steps[0].argument_count = 1u;
            plan.steps[0].caller_cleanup = 4u;
            plan.steps[1] = gameplay_method_call_step(0x00521d30u,
                0x004c7569u);
            plan.steps[1].self_source_address = 0x005910f0u;
            plan.steps[1].argument_count = 1u;
            plan.steps[1].argument0_from_eax = 1u;
            plan.step_count = 2u;
            break;
        case 0x00532200u:
            plan.steps[0] = gameplay_method_call_step(0x00531a50u,
                0x00532207u);
            plan.steps[0].arguments[0] = 4u;
            plan.steps[0].argument_count = 1u;
            plan.steps[0].caller_cleanup = 4u;
            plan.steps[1] = gameplay_method_call_step(0x004cddc0u,
                0x0053221eu);
            plan.steps[1].self_source_address = 0x0059d4c0u;
            plan.steps[1].arguments[0] = 3u;
            plan.steps[1].arguments[1] = 1000u;
            plan.steps[1].arguments[2] = 0x45u;
            plan.steps[1].argument_count = 3u;
            plan.step_count = 2u;
            break;
        case 0x00471a70u:
            plan.steps[0] = gameplay_method_call_step(0x00470860u,
                0x00471a85u);
            plan.steps[0].self = game_memory_read_u32(memory, input + 0x54u);
            plan.steps[0].set_self = 1;
            plan.steps[0].arguments[0] = 0x461c4000u;
            plan.steps[0].arguments[1] = 0x447a0000u;
            plan.steps[0].argument_count = 2u;
            plan.steps[1] = gameplay_method_call_step(0x005362d0u,
                0x00471a8cu);
            plan.steps[1].self = input;
            plan.steps[1].set_self = 1;
            plan.step_count = 2u;
            plan.set_return_value = 0;
            break;
        case 0x004754b0u:
            game_memory_write_u32(memory, 0x005eff30u, 1u);
            plan.steps[0] = gameplay_method_call_step(0x00521d30u,
                0x004754cbu);
            plan.steps[0].self = input;
            plan.steps[0].set_self = 1;
            plan.steps[0].arguments[0] = 0x0058970cu;
            plan.steps[0].argument_count = 1u;
            plan.steps[1] = gameplay_method_call_step(0x00521c30u,
                0x004754ddu);
            plan.steps[1].self = input;
            plan.steps[1].set_self = 1;
            plan.steps[1].argument_count = 1u;
            plan.steps[1].argument0_source_address = 0x005f0208u;
            plan.steps[1].argument0_addend = 0x31cu;
            plan.step_count = 2u;
            break;
        case 0x0052cab0u:
            plan.steps[0] = gameplay_method_call_step(0x0052e000u,
                0x0052cacau);
            plan.steps[0].self = game_memory_read_u32(memory, 0x00628a90u);
            plan.steps[0].set_self = 1;
            plan.steps[0].arguments[0] = 0u;
            plan.steps[0].arguments[1] = 1u;
            plan.steps[0].arguments[2] = 0u;
            plan.steps[0].arguments[3] = 0x3f800000u;
            plan.steps[0].arguments[4] = 0u;
            plan.steps[0].arguments[5] = 0x38u;
            plan.steps[0].argument_count = 6u;
            plan.step_count = 1u;
            break;
        case 0x004c3890u:
        case 0x004c38c0u:
        case 0x004c4dd0u:
        case 0x004c5490u: {
            GameAddress first = method_id == 0x004c3890u ?
                0x004c3110u : method_id == 0x004c38c0u ?
                0x004c3110u : method_id == 0x004c4dd0u ?
                0x004c4170u : 0x004c4e00u;
            GameAddress next = method_id == 0x004c3890u ?
                0x004c4e00u : method_id == 0x004c38c0u ?
                0x004c4170u : 0x004c3110u;
            GameAddress base = method_id;
            plan.steps[0] = gameplay_method_call_step(first, base + 7u);
            plan.steps[0].arguments[0] = 4u;
            plan.steps[0].argument_count = 1u;
            plan.steps[0].caller_cleanup = 4u;
            plan.steps[1] = gameplay_method_call_step(next, base + 0x11u);
            plan.steps[1].arguments[0] = 0u;
            plan.steps[1].argument_count = 1u;
            plan.steps[1].caller_cleanup = 4u;
            plan.steps[2] = gameplay_method_call_step(next, base + 0x1bu);
            plan.steps[2].arguments[0] = 1u;
            plan.steps[2].argument_count = 1u;
            plan.steps[2].caller_cleanup = 4u;
            plan.step_count = 3u;
            break;
        }
        case 0x00532190u:
        case 0x005321c0u: {
            GameAddress toggle = method_id == 0x00532190u ?
                0x004c18b0u : 0x004c2330u;
            plan.steps[0] = gameplay_method_call_step(0x00531a50u,
                method_id + 7u);
            plan.steps[0].arguments[0] = 4u;
            plan.steps[0].argument_count = 1u;
            plan.steps[0].caller_cleanup = 4u;
            plan.steps[0].post_write_address = 0x0059c6a0u;
            plan.steps[0].post_write_value = 1u;
            plan.steps[1] = gameplay_method_call_step(toggle,
                method_id + 0x1bu);
            plan.steps[1].arguments[0] = 0u;
            plan.steps[1].argument_count = 1u;
            plan.steps[1].caller_cleanup = 4u;
            plan.steps[2] = gameplay_method_call_step(toggle,
                method_id + 0x25u);
            plan.steps[2].arguments[0] = 1u;
            plan.steps[2].argument_count = 1u;
            plan.steps[2].caller_cleanup = 4u;
            plan.step_count = 3u;
            break;
        }
        case 0x00475710u: {
            uint32_t selector = game_memory_read_u32(memory, 0x005f0150u);
            plan.initial_eax = selector;
            plan.set_initial_eax = 1;
            plan.steps[0] = gameplay_method_call_step(0x004cddc0u,
                selector ? 0x0047572du : 0x00475747u);
            plan.steps[0].self_source_address = 0x0059d4c0u;
            plan.steps[0].arguments[0] = selector ? 13u : 14u;
            plan.steps[0].arguments[1] = 1000u;
            plan.steps[0].arguments[2] = 0x45u;
            plan.steps[0].argument_count = 3u;
            plan.step_count = 1u;
            break;
        }
        case 0x005120d0u: {
            uint32_t enabled =
                game_memory_read_u32(memory, input + 0x94u) != 0u;
            GameAddress manager =
                game_memory_read_u32(memory, 0x005977c4u);
            game_memory_write_u32(memory, 0x005977d8u, enabled);
            if (!memory->fault)
                game_memory_write_u32(memory, manager + 0x170u, enabled);
            plan.initial_eax = enabled;
            plan.set_initial_eax = 1;
            plan.steps[0] = gameplay_method_call_step(0x00512620u,
                enabled ? 0x005120f9u : 0x00512117u);
            plan.steps[0].self = manager;
            plan.steps[0].set_self = 1;
            plan.step_count = 1u;
            break;
        }
        case 0x004c7500u: {
            uint32_t enabled = game_memory_read_u32(memory, 0x0059c6a0u);
            plan.initial_eax = enabled;
            plan.set_initial_eax = 1;
            plan.steps[0] = gameplay_method_call_step(0x004c18b0u,
                enabled ? 0x004c7510u : 0x004c7528u);
            plan.steps[0].arguments[0] = 4u;
            plan.steps[0].argument_count = 1u;
            plan.steps[0].caller_cleanup = 4u;
            plan.step_count = 1u;
            if (!enabled) {
                plan.steps[1] = gameplay_method_call_step(0x004c0b80u,
                    0x004c7532u);
                plan.steps[1].arguments[0] = 1u;
                plan.steps[1].argument_count = 1u;
                plan.steps[1].caller_cleanup = 4u;
                plan.step_count = 2u;
            }
            plan.final_write_address = 0x005910a4u;
            plan.final_write_value = enabled ? 0u : 1u;
            break;
        }
        case 0x00524ab0u: {
            GameAddress manager = game_memory_read_u32(memory, 0x0058abe8u);
            plan.initial_ecx = manager;
            plan.set_initial_ecx = 1;
            plan.initial_test_left = manager;
            plan.initial_test_right = manager;
            plan.initial_test_width = 32u;
            if (manager) {
                plan.steps[0] = gameplay_method_call_step(0x0040bd30u,
                    0x00524ac6u);
                plan.steps[0].self = manager;
                plan.steps[0].set_self = 1;
                plan.steps[0].arguments[0] = 0u;
                plan.steps[0].arguments[1] = 0x00627ca8u;
                plan.steps[0].argument_count = 2u;
                plan.step_count = 1u;
            }
            plan.return_value = 1u;
            plan.set_return_value = 1;
            break;
        }
        default:
            plan.return_value = 0u;
            break;
    }
    return plan;
}

GameplayMethodCallbackPlan gameplay_method_destructor_callback_prepare(
        uint32_t method_id, GameAddress owner, uint32_t flags) {
    GameplayMethodCallbackPlan plan = {0};
    GameAddress base_target = 0x004ccbe0u;
    GameAddress base_return = 0x0044e0a8u;
    GameAddress delete_return = 0x0044e0b6u;
    if (method_id == 0x00483620u) {
        base_target = 0x004821d0u;
        base_return = 0x00483628u;
        delete_return = 0x00483636u;
    } else if (method_id == 0x004f9ba0u) {
        base_target = 0x004f0850u;
        base_return = 0x004f9ba8u;
        delete_return = 0x004f9bb6u;
    }
    plan.steps[0] = gameplay_method_call_step(base_target, base_return);
    plan.steps[0].self = owner;
    plan.steps[0].set_self = 1;
    plan.steps[0].post_test_left = flags & 0xffu;
    plan.steps[0].post_test_right = 1u;
    plan.steps[0].post_test_width = 8u;
    plan.step_count = 1u;
    if (flags & 1u) {
        plan.steps[1] = gameplay_method_call_step(0x00540170u,
            delete_return);
        plan.steps[1].arguments[0] = owner;
        plan.steps[1].argument_count = 1u;
        plan.steps[1].caller_cleanup = 4u;
        plan.step_count = 2u;
    }
    plan.return_value = owner;
    plan.set_return_value = 1;
    plan.outer_cleanup = 4u;
    return plan;
}

GameplayMethodCallbackPlan gameplay_method_pair_callback_prepare(
        GameMemory *memory, uint32_t method_id, GameAddress owner,
        uint32_t first, uint32_t second) {
    GameplayMethodCallbackPlan plan = {0};
    uint32_t variant = method_id == 0x0044d9d0u ? 0u : 1u;
    game_memory_write_u32(memory, owner + (variant ? 0x1acu : 0x1a8u), 1u);
    plan.steps[0] = gameplay_method_call_step(0x0044d870u,
        variant ? 0x0044da8eu : 0x0044d9eeu);
    plan.steps[0].self = owner;
    plan.steps[0].set_self = 1;
    plan.steps[0].arguments[0] = second;
    plan.steps[0].arguments[1] = first;
    plan.steps[0].argument_count = 2u;
    plan.steps[1] = gameplay_method_call_step(0x004cd630u,
        variant ? 0x0044daadu : 0x0044da0du);
    plan.steps[1].self_source_address = 0x0059d4c0u;
    plan.steps[1].arguments[0] = 0u;
    plan.steps[1].arguments[1] = variant ? 0x0044dac0u : 0x0044da20u;
    plan.steps[1].arguments[2] = variant ? 0x205u : 0x202u;
    plan.steps[1].arguments[3] = owner + 0x194u;
    plan.steps[1].arguments[4] = owner;
    plan.steps[1].argument_count = 5u;
    plan.step_count = 2u;
    plan.set_return_value = 0;
    plan.outer_cleanup = 16u;
    return plan;
}

GameplayMethodCallbackPlan gameplay_method_render_callback_prepare(
        uint32_t first, uint32_t second, uint32_t third, uint32_t fourth,
        uint32_t fifth) {
    GameplayMethodCallbackPlan plan = {0};
    plan.initial_eax = second;
    plan.set_initial_eax = 1;
    plan.initial_ecx = first;
    plan.set_initial_ecx = 1;
    plan.initial_edx = third;
    plan.set_initial_edx = 1;
    plan.steps[0] = gameplay_method_call_step(0x00439e30u, 0x00439e23u);
    plan.steps[0].arguments[0] = 0x3f000000u;
    plan.steps[0].arguments[1] = fifth;
    plan.steps[0].arguments[2] = fourth;
    plan.steps[0].arguments[3] = third;
    plan.steps[0].arguments[4] = second;
    plan.steps[0].arguments[5] = first;
    plan.steps[0].argument_count = 6u;
    plan.steps[0].caller_cleanup = 24u;
    plan.step_count = 1u;
    return plan;
}

GameplayMethodStreamReadResult gameplay_method_stream_read(
        GameMemory *memory, GameAddress stream) {
    GameplayMethodStreamReadResult result = {0};
    result.original_count = game_memory_read_u32(memory, stream + 4u);
    if (memory->fault) return result;
    result.decremented_count = result.original_count - 1u;
    game_memory_write_u32(memory, stream + 4u, result.decremented_count);
    result.refill = (int32_t)result.decremented_count < 0;
    if (memory->fault || result.refill) return result;
    result.original_cursor = game_memory_read_u32(memory, stream);
    if (memory->fault) return result;
    result.byte_value = game_memory_read_u8(memory, result.original_cursor);
    result.incremented_cursor = result.original_cursor + 1u;
    if (!memory->fault)
        game_memory_write_u32(memory, stream, result.incremented_cursor);
    return result;
}

void gameplay_method_callback_finish(GameMemory *memory, uint32_t method_id) {
    if (method_id == 0x00523680u)
        game_memory_write_u32(memory, 0x005f003cu, 1u);
    else if (method_id == 0x005236a0u)
        game_memory_write_u32(memory, 0x005eff00u, 1u);
    else if (method_id == 0x004c3890u)
        game_memory_write_u32(memory, 0x005910a4u, 9u);
    else if (method_id == 0x004c38c0u)
        game_memory_write_u32(memory, 0x005910a4u, 10u);
    else if (method_id == 0x004c4dd0u || method_id == 0x004c5490u)
        game_memory_write_u32(memory, 0x005910a4u, 4u);
}

uint32_t gameplay_method_callback_finish_value(GameMemory *memory,
        uint32_t method_id) {
    if (method_id == 0x0052cab0u) {
        GameAddress object = game_memory_read_u32(memory, 0x00628a90u);
        uint32_t value = game_memory_read_u32(memory, object);
        if (!memory->fault)
            game_memory_write_u32(memory, 0x00627fa0u, value);
        return value;
    }
    return 0u;
}

uint32_t gameplay_method_publish_multiplayer_payload(GameMemory *memory,
        GameAddress source, uint32_t *index, uint32_t *value) {
    GameAddress manager = game_memory_read_u32(memory, 0x005f0208u);
    uint32_t selected = game_memory_read_u16(memory, manager + 8u);
    uint32_t payload = game_memory_read_u32(memory, source + 0x94u);
    GameAddress target = game_memory_read_u32(memory,
        0x005f0210u + selected * 4u);
    if (!memory->fault)
        game_memory_write_u32(memory, target + 0x4c4u, payload);
    if (!memory->fault)
        game_memory_write_u32(memory, 0x005f003cu, 1u);
    if (index) *index = selected;
    if (value) *value = payload;
    return target;
}

GameAddress gameplay_method_store_indexed_vector(GameMemory *memory,
        GameAddress self, uint32_t first, uint32_t second, uint32_t third) {
    uint32_t index = game_memory_read_u32(memory, self + 8u);
    GameAddress table = game_memory_read_u32(memory, 0x0060586cu);
    GameAddress record = game_memory_read_u32(memory, table + index * 4u);
    if (!memory->fault) game_memory_write_u32(memory, record + 0x20u, first);
    if (!memory->fault) game_memory_write_u32(memory, record + 0x24u, second);
    if (!memory->fault) game_memory_write_u32(memory, record + 0x28u, third);
    return record;
}

uint16_t gameplay_method_adjust_bounded_angle(GameMemory *memory,
        GameAddress self, int32_t delta, GameAddress *continuation) {
    uint16_t old_value = game_memory_read_u16(memory, self + 0xccu);
    uint16_t value = (uint16_t)(old_value + delta);
    if (!memory->fault)
        game_memory_write_u16(memory, self + 0xccu, value);
    if (delta < 0) {
        if ((int16_t)value <= (int16_t)0xfe00u && !memory->fault)
            game_memory_write_u16(memory, self + 0xccu, 0xfe00u);
        if (continuation) *continuation = 0x0050f205u;
    } else if (continuation) {
        *continuation = (int16_t)value < (int16_t)0xff56u ?
            0x0050f205u : 0x0050f1fcu;
    }
    return value;
}
