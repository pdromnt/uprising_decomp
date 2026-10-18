#include "gameplay_constructors.h"
#include "game_initializers.h"
#include "gameplay_state.h"

void gameplay_constructor_initialize_command_owner(GameMemory *memory,
        GameAddress object, uint16_t identifier) {
    static const uint16_t zero_offsets[] = {
        0x0004u, 0x0014u, 0x019cu, 0x01a4u, 0x01acu, 0x01b0u,
        0x01b4u, 0x01a0u, 0x01a8u, 0x01b8u, 0x01bcu, 0x01c0u,
        0x01c4u, 0x01c8u, 0x01ccu, 0x01d0u, 0x01d4u, 0x01d8u,
        0x01e0u, 0x01e4u, 0x01e8u, 0x01ecu, 0x01f0u, 0x01f4u,
        0x01f8u
    };
    unsigned index;
    game_memory_write_u32(memory, object, 0x00573c10u);
    game_memory_write_u16(memory, object + 8u, identifier);
    for (index = 0u; index < sizeof(zero_offsets) / sizeof(zero_offsets[0]);
            ++index)
        game_memory_write_u32(memory, object + zero_offsets[index], 0u);
    game_memory_write_u32(memory, object + 0x18u, 1u);
}

void gameplay_constructor_copy_type89_fields(GameMemory *memory,
        GameAddress object, GameAddress source) {
    static const uint16_t copy32_offsets[] = {
        0x140u, 0x144u, 0x148u, 0x104u, 0x108u, 0x10cu,
        0x14cu, 0x150u, 0x154u
    };
    unsigned index;
    game_memory_write_u32(memory, object, 0x005753b0u);
    game_memory_write_u16(memory, object + 0x164u,
        game_memory_read_u16(memory, source + 0x164u));
    game_memory_write_u16(memory, object + 0x158u,
        game_memory_read_u16(memory, source + 0x158u));
    for (index = 0u; index < sizeof(copy32_offsets) /
            sizeof(copy32_offsets[0]); ++index)
        game_memory_write_u32(memory, object + copy32_offsets[index],
            game_memory_read_u32(memory, source + copy32_offsets[index]));
    game_memory_write_u16(memory, object + 0xbcu, 0u);
    game_memory_write_u32(memory, object + 0x10u,
        game_memory_read_u32(memory, object + 0x10u) | 0x8300u);
    game_memory_write_u8(memory, object + 0x0cu, 0x89u);
    game_memory_write_u32(memory, object + 0xb4u, 0u);
    game_memory_write_u32(memory, object + 0x178u, 0u);
    game_memory_write_u32(memory, object + 0x168u, 0u);
}

void gameplay_constructor_initialize_triple_list_owner(GameMemory *memory,
        GameAddress object) {
    game_memory_write_u32(memory, object + 0x10u, 0u);
    game_memory_write_u32(memory, object + 0x14u, 0u);
    game_memory_write_u32(memory, object + 0x18u, 0u);
    game_memory_write_u32(memory, object + 0x20u, 0u);
}

void gameplay_constructor_initialize_list_node(GameMemory *memory,
        GameAddress node) {
    if (node != 0u) {
        game_memory_write_u32(memory, node + 4u, 0u);
        game_memory_write_u32(memory, node, 0u);
    }
}

void gameplay_constructor_publish_triple_list(GameMemory *memory,
        GameAddress object, unsigned index, GameAddress node) {
    static const uint16_t offsets[3] = {4u, 8u, 0x0cu};
    if (index < 3u) game_memory_write_u32(memory, object + offsets[index], node);
}

void gameplay_constructor_initialize_layered_stage(GameMemory *memory,
        GameAddress object, unsigned stage, uint32_t first, uint32_t second,
        uint32_t third) {
    if (stage == 0u) {
        game_memory_write_u32(memory, object, 0x005758b0u);
        game_memory_write_u32(memory, object + 0x44u, 0u);
        game_memory_write_u32(memory, object + 0x14u, 0u);
        game_memory_write_u32(memory, object + 0x48u, 0u);
        game_memory_write_u32(memory, object + 0x50u, UINT32_MAX);
        game_memory_write_u32(memory, object + 0x54u, UINT32_MAX);
    } else if (stage == 1u) {
        game_memory_write_u32(memory, object + 0x4cu, 0u);
        game_memory_write_u32(memory, object, 0x00575908u);
        game_memory_write_u32(memory, object + 0x68u, 0u);
        game_memory_write_u32(memory, object + 0x6cu, 0u);
        game_memory_write_u32(memory, object + 0x70u, 0u);
        game_memory_write_u32(memory, object + 0x74u, 0u);
        game_memory_write_u32(memory, object + 0x7cu, UINT32_MAX);
        game_memory_write_u32(memory, object + 0x80u, UINT32_MAX);
        game_memory_write_u32(memory, object + 0x84u, UINT32_MAX);
        game_memory_write_u32(memory, object + 0x8cu, 0u);
        game_memory_write_u32(memory, object + 0x60u, 0u);
        game_memory_write_u32(memory, object + 0x5cu, 0u);
        game_memory_write_u32(memory, object + 0x58u, 0u);
        game_memory_write_u32(memory, object + 0x90u, 0u);
        game_memory_write_u32(memory, object + 0x78u, 0u);
    } else if (stage == 2u) {
        game_memory_write_u32(memory, object + 0x6cu, first);
        game_memory_write_u32(memory, object + 0x64u, UINT32_MAX);
        game_memory_write_u32(memory, object + 0x70u, second);
        game_memory_write_u32(memory, object + 0x88u, UINT32_MAX);
        game_memory_write_u32(memory, object + 0x74u, third);
        game_memory_write_u32(memory, object, 0x00575998u);
        game_memory_write_u32(memory, object + 0x94u, 0u);
    } else if (stage == 3u) {
        game_memory_write_u32(memory, object, 0x005759e0u);
        game_memory_write_u32(memory, object + 0x98u, first);
    }
}

void gameplay_constructor_initialize_indexed_buffer(GameMemory *memory,
        GameAddress object, uint32_t count) {
    game_memory_write_u32(memory, object, 0x00574b10u);
    game_memory_write_u32(memory, object + 0x6cu, count);
}

uint32_t gameplay_constructor_indexed_buffer_size(uint32_t count,
        unsigned index) {
    return index < 3u ? count * 12u : count;
}

void gameplay_constructor_publish_indexed_buffer(GameMemory *memory,
        GameAddress object, unsigned index, GameAddress allocation) {
    static const uint16_t offsets[4] = {0x74u, 0x78u, 0x7cu, 0x80u};
    if (index >= 4u) return;
    game_memory_write_u32(memory, object + offsets[index], allocation);
    if (index == 2u) game_memory_write_u32(memory, object + 0x48u, allocation);
    if (index == 3u) {
        game_memory_write_u32(memory, object + 0x4cu, 0x1000u);
        game_memory_write_u32(memory, object + 0x54u, allocation);
    }
}

void gameplay_constructor_initialize_presentation_controller_base(
        GameMemory *memory, GameAddress object) {
    game_memory_write_u32(memory, object, 0x00571c58u);
    game_memory_write_u32(memory, object + 0xb4u, 0u);
    game_memory_write_u32(memory, object + 0x13cu, 0u);
    game_memory_write_u32(memory, object + 0x140u, 0u);
    game_memory_write_u8(memory, object + 0xb8u, 0u);
    game_memory_write_u32(memory, object + 0x164u, 0u);
    game_memory_write_u32(memory, object + 0x168u, 0u);
    game_memory_write_u32(memory, object + 0xb0u, 100u);
}

void gameplay_constructor_finish_presentation_controller(GameMemory *memory,
        GameAddress object) {
    unsigned index;
    game_memory_write_u32(memory, object, 0x00571ca0u);
    game_memory_write_u32(memory, object + 0x2fcu, 0u);
    game_memory_write_u32(memory, object + 0x300u, 0u);
    for (index = 0u; index < 100u; ++index)
        game_memory_write_u32(memory, object + 0x16cu + index * 4u, 0u);
    game_memory_write_u32(memory, object + 0x308u, 0u);
    game_memory_write_u32(memory, object + 0x304u, 1u);
}

GameplayOwnedSlotSpec gameplay_constructor_owned_slot_spec(unsigned index) {
    static const GameplayOwnedSlotSpec specs[3] = {
        {0x13cu, 0x005444d8u, 0x00544447u, 0x00544450u},
        {0x144u, 0x005444e0u, 0x00544479u, 0x00544482u},
        {0x140u, 0x005444f2u, 0x005444abu, 0x005444b4u}
    };
    return specs[index < 3u ? index : 0u];
}

GameplayCallbackSpec gameplay_constructor_presentation_callback_spec(
        unsigned index) {
    static const GameplayCallbackSpec specs[4] = {
        {0x202u, 0x005232b0u, 0x0051d533u},
        {0x201u, 0x005232c0u, 0x0051d54cu},
        {0x20au, 0x00523290u, 0x0051d586u},
        {0x20bu, 0x005232a0u, 0x0051d59fu}
    };
    return specs[index < 4u ? index : 0u];
}

void gameplay_constructor_initialize_text_provider(GameMemory *memory,
        GameAddress object) {
    game_memory_write_u32(memory, object, 0u);
    game_memory_write_u32(memory, object + 4u, 0u);
    game_memory_write_u32(memory, object + 0x5cu, 0u);
    game_memory_write_u32(memory, object + 0x60u, 0u);
    game_memory_write_u32(memory, object + 8u, 0u);
    game_memory_write_u32(memory, object + 0x44u, 0u);
    game_memory_write_u32(memory, object + 0x4cu, 0x3e800000u);
    game_memory_write_u32(memory, object + 0x48u, 0x3f000000u);
    game_memory_write_u32(memory, object + 0x50u, 1u);
    game_memory_write_u32(memory, object + 0x54u, UINT32_MAX);
    game_memory_write_u32(memory, object + 0x58u, UINT32_MAX);
}

void gameplay_constructor_initialize_reference_pair(GameMemory *memory,
        GameAddress object, uint32_t first_selector, uint32_t first_index,
        uint32_t second_selector, uint32_t second_index, uint32_t value) {
    game_memory_write_u32(memory, object, 0x00574b28u);
    game_memory_write_u32(memory, object, 0x00574ba8u);
    game_memory_write_u32(memory, object + 4u, first_selector);
    game_memory_write_u32(memory, object + 8u, first_index);
    game_memory_write_u32(memory, object + 0x0cu, second_selector);
    game_memory_write_u32(memory, object + 0x10u, second_index);
    game_memory_write_u32(memory, object + 0x14u, value);
}

void gameplay_constructor_publish_reference_pair(GameMemory *memory,
        GameAddress object, GameAddress first, GameAddress second) {
    game_memory_write_u32(memory, object + 0x18u, first);
    game_memory_write_u32(memory, object + 0x1cu, second);
}

/* 00476270 -> three 0042cfb0 records -> 0047a780/00479330/00477d60.
 * Record storage belongs to the caller. Constructor inputs are real arguments;
 * all gameplay consumers continue to use these authoritative bounded records. */
int gameplay_manager_construct(GameMemory *memory,GameAddress object,unsigned identifier) {
    unsigned i;
    if(identifier>=6u || !object)return 0;
    for(i=0;i<3u && !memory->fault;++i)
        game_initializer_0042cfb0(memory,object+0x1cu+i*0x64u);
    if(memory->fault)return 0;
    gameplay_constructor_initialize_command_owner(memory,object,(uint16_t)identifier);
    if(identifier<5u) {
        game_initializer_00549710(memory,object+0x230u);
        if(identifier<4u)gameplay_state_initialize_controller_fields(memory,object);
        else game_initializer_type004793_finish(memory,object);
    } else game_memory_write_u32(memory,object,0x573c98u);
    return !memory->fault;
}
/* 004763b0 -> command records e4,80,1c -> complete 0042d250 behavior.
 * This original shared destructor intentionally retires the embedded command
 * records; the outer owner releases the manager allocation afterward. */
int gameplay_manager_destroy(GameMemory *memory,GameAddress object,
    const GamePresentationStorageServices *services) {
    unsigned i;static const uint32_t records[]={0xe4u,0x80u,0x1cu};
    if(!object || !services)return 0;
    game_memory_write_u32(memory,object,0x573c10u);
    for(i=0;i<3u && !memory->fault;++i)
        if(!game_presentation_base_destroy(memory,object+records[i],services))return 0;
    return !memory->fault;
}