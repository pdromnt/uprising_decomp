#include "gameplay_leaves.h"
#include "gameplay_methods.h"
#include "entity_queries.h"
#include "game_initializers.h"
#include "game_state_ops.h"

GameLeafBatchResult game_leaf_batch(GameMemory *memory, GameAddress entry,
        GameAddress self, uint32_t eax, uint32_t stack_argument) {
    GameLeafBatchResult result = {eax, 0u, 0};
    switch (entry) {
        /* No-op virtual methods and their fixed stdcall cleanup. */
        case 0x00459e90u: case 0x004f87f0u: case 0x00409610u:
            break;
        case 0x0042ec20u: case 0x00413680u: case 0x004777c0u:
            result.cleanup = 4u; break;
        case 0x0051c130u: result.cleanup = 16u; break;
        case 0x0050dc10u: break;
        case 0x0045e800u: case 0x004718e0u: result.cleanup = 20u; break;
        case 0x0051c140u: case 0x0051c150u: case 0x0051c3f0u:
        case 0x0051e390u: result.cleanup = 16u; break;
        case 0x00521a60u: result.cleanup = 4u; break;
        case 0x004abd50u: result.cleanup = 20u; break;
        case 0x00481db0u: case 0x004b38a0u: case 0x00412720u:
        case 0x005188b0u: case 0x005188e0u: case 0x00518990u:
        case 0x004a65f0u: case 0x0053ff40u: case 0x0056c90cu:
        case 0x00403370u:
            break;

        /* Constant-valued scalar virtual methods. */
        case 0x00408c70u: case 0x00408c80u: case 0x00408c90u:
        case 0x00408fb0u: case 0x00408fc0u: case 0x00408fd0u:
        case 0x0040eb40u: case 0x0040eb50u: case 0x0040eb60u:
        case 0x0040eb70u: case 0x0040eb80u: case 0x004e83a0u:
        case 0x004e83b0u: case 0x004e83c0u: case 0x004e83d0u:
        case 0x004e94b0u: case 0x005399e0u: case 0x00512060u:
        case 0x005399d0u: case 0x005399f0u: case 0x00539a00u:
        case 0x00539a10u:
            result.eax = gameplay_method_scalar_u32(entry);
            result.writes_eax = 1;
            break;
        case 0x00408fe0u: case 0x00408ff0u: case 0x00408ca0u:
        case 0x00408cb0u: case 0x004e8460u: case 0x004e8470u:
            result.eax = (eax & 0xffff0000u) |
                gameplay_method_scalar_u16(entry);
            result.writes_eax = 1;
            break;
        case 0x00552190u: result.eax = 0x2710u; result.writes_eax = 1; break;
        case 0x005521a0u: result.eax = 0x41u; result.writes_eax = 1; break;
        case 0x005448c0u: case 0x005448d0u:
        case 0x005448e0u: case 0x005448f0u:
            result.eax = native_entity_class_limit((entry - 0x005448c0u) / 16u);
            result.writes_eax = 1;
            break;
        case 0x00518880u: result.eax = game_state_read_global(memory, 0x00597f78u); result.writes_eax = 1; break;
        case 0x00518bc0u: result.eax = game_state_read_global(memory, 0x00627904u); result.writes_eax = 1; break;
        case 0x00518bd0u: result.eax = game_state_read_global(memory, 0x00627068u); result.writes_eax = 1; break;
        case 0x00517240u:
            result.eax = stack_argument; result.writes_eax = 1;
            game_state_write_global(memory, 0x00597f60u, stack_argument);
            break;
        case 0x0053fed0u: game_state_write_global(memory, 0x00631908u, 0u); break;
        case 0x0055cd10u: game_state_write_global(memory, 0x0063a2bcu, 0x455ac000u); break;
        case 0x005587d0u:
            game_state_initialize_descriptor(memory, self);
            result.eax = memory->fault ? 0u : self;
            result.writes_eax = 1;
            break;
        case 0x00454e90u: result.eax = game_leaf_00454e90(memory, self); result.writes_eax = 1; break;
        case 0x0045f570u: game_leaf_0045f570(memory); break;
        case 0x004ccbe0u: game_state_write_global(memory, self, 0x00573180u); break;
        case 0x0049aad0u: {
            const uint32_t offset = 0xb4u, value = 3u;
            game_state_write_fields(memory, self, &offset, &value, 1u);
            break;
        }
        case 0x004129b0u:
            result.eax = stack_argument; result.writes_eax = 1;
            game_initializer_004129b0(memory, stack_argument);
            break;
        case 0x00519330u:
            result.eax = stack_argument; result.writes_eax = 1;
            game_initializer_00519330(memory, stack_argument);
            break;
        case 0x004ac1a0u:
            result.eax = game_initializer_004ac1a0(memory);
            result.writes_eax = 1;
            break;
        case 0x00413690u:
            result.eax = stack_argument; result.writes_eax = 1;
            result.cleanup = 4u;
            game_initializer_00413690(memory, self, stack_argument);
            break;
        case 0x004aa490u:
            game_initializer_004aa490(memory, self);
            result.eax = memory->fault ? 0xbf800000u : self;
            result.writes_eax = 1;
            break;

        /* Small gameplay state leaves share the same typed GameMemory API. */
        case 0x0044ed50u: game_leaf_0044ed50(memory); break;
        case 0x004d94d0u: game_leaf_004d94d0(memory); break;
        case 0x0042f110u: game_leaf_0042f110(memory, self); break;
        case 0x004765f0u: game_leaf_004765f0(memory, self); break;
        case 0x0043b4c0u: game_leaf_0043b4c0(memory, self); break;
        case 0x00502e00u: game_leaf_00502e00(memory); break;
        case 0x00441420u: game_leaf_00441420(memory); break;
        case 0x004cc1c0u: result.eax = game_leaf_004cc1c0(); result.writes_eax = 1; break;
        case 0x004f9b90u: result.eax = game_leaf_004f9b90(memory, self); result.writes_eax = 1; break;
        case 0x004341c0u: case 0x00434450u:
            result.eax = game_leaf_identity(self); result.writes_eax = 1; break;
        case 0x0048ef80u: result.eax = game_leaf_0048ef80(memory, self); result.writes_eax = 1; break;
        case 0x00416c10u:
            result.eax = game_leaf_00416c10(memory, self, stack_argument);
            result.writes_eax = 1; result.cleanup = 4u; break;
        case 0x00416c20u: result.eax = game_leaf_00416c20(memory, self); result.writes_eax = 1; break;
        case 0x00479024u:
            result.eax = game_leaf_00479024(memory, self);
            result.writes_eax = 1; result.cleanup = 4u; break;
        case 0x0046ff30u: result.eax = game_leaf_0046ff30(); result.writes_eax = 1; break;
        case 0x004073d0u: result.eax = game_leaf_004073d0(); result.writes_eax = 1; break;
        case 0x004073e0u: result.eax = game_leaf_004073e0(); result.writes_eax = 1; break;
        case 0x004abd30u: case 0x004abd40u: result.cleanup = 8u; break;
        default: break; /* Only the listed entry points use this API. */
    }
    return result;
}
