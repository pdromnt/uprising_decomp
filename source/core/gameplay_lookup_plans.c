#include "gameplay_lookup_plans.h"

typedef struct GameplayOwnerLookupDescriptor {
    GameAddress target;
    GameAddress address;
    uint8_t indexed;
} GameplayOwnerLookupDescriptor;

static const GameplayOwnerLookupDescriptor owner_lookup_descriptors[] = {
    {0x00447922u, 0x005ada60u, 1u}, {0x0044792fu, 0x005ada20u, 1u},
    {0x0044793cu, 0x005adb10u, 1u}, {0x00447949u, 0x005adce4u, 0u},
    {0x0044794fu, 0x005adda8u, 1u}, {0x0044795cu, 0x005adb50u, 1u},
    {0x00447969u, 0x005adaf8u, 1u}, {0x00447976u, 0x005adad4u, 1u},
    {0x00447983u, 0x005adb18u, 1u}, {0x00447990u, 0x005adc64u, 1u},
    {0x0044799du, 0x005adcfcu, 1u}, {0x004479aau, 0x005adaf4u, 1u},
    {0x004479b7u, 0x005adbfcu, 1u}, {0x004479c4u, 0x005adafcu, 1u},
    {0x004479d1u, 0x005adb78u, 0u}, {0x004479d7u, 0x005adbf4u, 1u},
    {0x004479e4u, 0x005adcdcu, 1u}, {0x004479f1u, 0x005adce0u, 1u},
    {0x004479feu, 0x005adb40u, 1u}, {0x00447a0bu, 0x005add80u, 1u},
    {0x00447a18u, 0x005adcd8u, 1u}, {0x00447a25u, 0x005add98u, 1u},
    {0x00447a32u, 0x005adb98u, 1u}, {0x00447a3fu, 0x005adb90u, 1u},
    {0x00447a4cu, 0x005adc68u, 1u}, {0x00447a59u, 0x005adb80u, 1u},
    {0x00447a66u, 0x005adb70u, 1u}, {0x00447a73u, 0x005ada78u, 1u},
    {0x00447a80u, 0x005adbc8u, 1u}, {0x00447a8du, 0x005add10u, 1u},
    {0x00447a9au, 0x005adae0u, 1u}, {0x00447aa7u, 0x005add88u, 1u},
    {0x00447ab4u, 0x005adbf8u, 1u}, {0x00447ac1u, 0x005adcf4u, 0u},
    {0x00447ac7u, 0x005adc74u, 0u}, {0x00447acdu, 0x005adcecu, 0u},
    {0x00447ad3u, 0x005adaf0u, 0u}, {0x00447ad9u, 0x005adc0cu, 0u},
    {0x00447adfu, 0x005adb74u, 1u}, {0x00447aecu, 0x005add8cu, 1u},
    {0x00447af9u, 0x005ada30u, 1u}, {0x00447b06u, 0x005add84u, 1u},
    {0x00447b13u, 0x005adb00u, 1u}, {0x00447b20u, 0x005adc78u, 1u},
    {0x00447b2du, 0x005add58u, 1u}, {0x00447b3au, 0x005adc10u, 1u},
    {0x00447b47u, 0x005ada38u, 1u}, {0x00447b54u, 0x005ada70u, 1u},
    {0x00447b61u, 0x005adb48u, 1u}, {0x00447b6eu, 0x005ada28u, 1u},
    {0x00447b7bu, 0x005adcd0u, 1u}, {0x00447b88u, 0x005add90u, 1u},
    {0x00447b95u, 0x005adb28u, 1u}, {0x00447ba2u, 0x005add9cu, 0u},
    {0x00447ba8u, 0x005adb24u, 0u}, {0x00447baeu, 0x005adb64u, 0u},
    {0x00447bb4u, 0x005add18u, 1u}
};

GameplayCapabilityPlan gameplay_lookup_capability_plan(GameAddress target) {
    GameplayCapabilityPlan plan = {0u, 0u, 1, 0};
    switch (target) {
        case 0x00478b63u: plan.immediate = 1; plan.immediate_result = 1u; break;
        case 0x00478b6bu: plan.field_offset = 0x228u; break;
        case 0x00478b7cu: plan.field_offset = 0x204u; break;
        case 0x00478b8du: plan.field_offset = 0x208u; break;
        case 0x00478b9eu: plan.field_offset = 0x20cu; break;
        case 0x00478bafu: plan.field_offset = 0x210u; break;
        default: plan.valid = 0; break;
    }
    return plan;
}

GameplayEventRoute gameplay_lookup_event_route(uint32_t type) {
    static const GameplayEventRoute routes[] = {
        {0x005cee40u, 0x00452873u, 1},
        {0x005cef90u, 0x0045287fu, 1},
        {0x005ceee8u, 0x0045288bu, 1}
    };
    GameplayEventRoute empty = {0u, 0u, 0};
    return type < sizeof(routes) / sizeof(routes[0]) ? routes[type] : empty;
}

GameplayResourceSelectorPlan gameplay_lookup_resource_selector(
        GameAddress target, int value_only) {
    GameplayResourceSelectorPlan plan = {0u, 0u, 0, 1};
    switch (target) {
        case 0x004781a5u: case 0x00478372u:
            plan.field_offset = 0x200u; plan.base = 0x005ebf28u; break;
        case 0x004781bcu: case 0x0047837bu:
            plan.field_offset = 0x220u; plan.base = 0x005ecdd8u; break;
        case 0x004781d3u: case 0x00478384u:
            plan.field_offset = 0x22cu; plan.base = 0x005e7b40u; break;
        case 0x004781eau: case 0x0047838du:
            plan.field_offset = 0x224u; plan.base = 0x005e2d98u;
            plan.use_ecx = !value_only; break;
        case 0x00478202u: case 0x00478396u:
            plan.field_offset = 0x210u; plan.base = 0x005e2a28u; break;
        case 0x00478219u: case 0x0047839fu:
            plan.field_offset = 0x204u; plan.base = 0x005e8660u; break;
        case 0x00478230u: case 0x004783a8u:
            plan.field_offset = 0x228u; plan.base = 0x005ece88u; break;
        case 0x00478247u: case 0x004783b1u:
            plan.field_offset = 0x218u; plan.base = 0x005ed098u; break;
        case 0x0047825eu: case 0x004783bau:
            plan.field_offset = 0x1fcu; plan.base = 0x005e1948u; break;
        case 0x00478275u: case 0x004783c3u:
            plan.field_offset = 0x1fcu; plan.base = 0x005eeac8u; break;
        case 0x0047828cu: case 0x004783ccu:
            plan.field_offset = 0x204u; plan.base = 0x005e8df0u; break;
        case 0x004782a3u: case 0x004783d5u:
            plan.field_offset = 0x200u; plan.base = 0x005e61b0u; break;
        case 0x004782bau: case 0x004783deu:
            plan.field_offset = 0x210u; plan.base = 0x005eeee8u; break;
        default: plan.valid = 0; break;
    }
    if (value_only) {
        plan.base = 0u;
        plan.use_ecx = 0;
    }
    return plan;
}

GameplayGridPointPlan gameplay_lookup_grid_point(unsigned index) {
    static const GameplayGridPointPlan points[] = {
        {0x0cu, 0x005739b8u, 0x005739b0u, 0},
        {0x18u, 0x005739bcu, 0x005739b0u, 0},
        {0x24u, 0u,          0x005739b0u, 1},
        {0x30u, 0x005739c0u, 0x005739b0u, 0},
        {0x3cu, 0x005739c4u, 0x005739b0u, 0},
        {0x48u, 0x005739c8u, 0x005739b0u, 0},
        {0x54u, 0x005739b0u, 0x005739c8u, 0},
        {0x60u, 0x005739b8u, 0x005739c8u, 0},
        {0x6cu, 0x005739bcu, 0x005739c8u, 0},
        {0x78u, 0u,          0x005739c8u, 1},
        {0x84u, 0x005739c0u, 0x005739c8u, 0},
        {0x90u, 0x005739c4u, 0x005739c8u, 0},
        {0x9cu, 0x005739c8u, 0x005739c8u, 0}
    };
    GameplayGridPointPlan empty = {0u, 0u, 0u, 0};
    return index < sizeof(points) / sizeof(points[0]) ? points[index] : empty;
}

void gameplay_lookup_initialize_presentation(GameMemory *memory) {
    game_memory_write_u32(memory, 0x00597f90u, 1u);
    game_memory_write_u32(memory, 0x00627928u, 640u);
    game_memory_write_u32(memory, 0x0062792cu, 480u);
    game_memory_write_u32(memory, 0x00597f80u, 1u);
}

GameplayPresentationDimensions gameplay_lookup_publish_dimensions(
        GameMemory *memory) {
    GameplayPresentationDimensions result;
    result.width = game_memory_read_u32(memory, 0x00597f68u);
    result.height = game_memory_read_u32(memory, 0x00597f6cu);
    if (!memory->fault) game_memory_write_u32(memory, 0x005ac6a0u,
        result.width);
    if (!memory->fault) game_memory_write_u32(memory, 0x005ac6a4u,
        result.height);
    return result;
}

void gameplay_lookup_publish_presentation_center(GameMemory *memory,
        GameplayPresentationDimensions dimensions) {
    game_memory_write_u32(memory, 0x005ac9d0u,
        (uint32_t)((int32_t)dimensions.width >> 1));
    game_memory_write_u32(memory, 0x005ac9d4u,
        (uint32_t)((int32_t)dimensions.height >> 1));
}

GameplayProjectionScalarPlan gameplay_lookup_projection_scalar(unsigned index) {
    static const GameplayProjectionScalarPlan plans[] = {
        {0x0059d43cu, 0x00575c18u, 0x00629080u, 0u},
        {0x005e1208u, 0x00573534u, 0x005e1204u, 0u},
        {0x00573540u, 0x00573548u, 0x005e1208u, 1u}
    };
    GameplayProjectionScalarPlan empty = {0u, 0u, 0u, 0u};
    return index < sizeof(plans) / sizeof(plans[0]) ? plans[index] : empty;
}

GameplayGlobalInitializerPlan gameplay_lookup_global_initializer(void) {
    GameplayGlobalInitializerPlan plan = {
        0x005ad7d0u, 0x0048ef80u, 0x0043b32au,
        0x0043b340u, 0x005604c0u, 0x0043b334u
    };
    return plan;
}

GameplayOwnerLookupResult gameplay_lookup_object_owner(GameMemory *memory,
    uint8_t kind, GameAddress index_address) {
    GameplayOwnerLookupResult result = {
        GAMEPLAY_OWNER_LOOKUP_OUT_OF_RANGE, 0u, 0u, 0u, 0u, 0
    };
    uint32_t adjusted;
    unsigned descriptor_index;
    if (kind < 2u) return result;
    adjusted = (uint32_t)kind - 2u;
    if (adjusted > 0xaeu) return result;
    result.selector = game_memory_read_u8(memory, 0x00447cacu + adjusted);
    result.target = game_memory_read_u32(memory,
        0x00447bc4u + result.selector * 4u);
    if (memory->fault) return result;
    if (result.target == 0x0044791fu) {
        result.decision = GAMEPLAY_OWNER_LOOKUP_EMPTY;
        return result;
    }
    for (descriptor_index = 0u;
            descriptor_index < sizeof(owner_lookup_descriptors) /
                sizeof(owner_lookup_descriptors[0]);
            ++descriptor_index) {
        const GameplayOwnerLookupDescriptor *descriptor =
            &owner_lookup_descriptors[descriptor_index];
        GameAddress address;
        if (descriptor->target != result.target) continue;
        address = descriptor->address;
        result.indexed = descriptor->indexed != 0u;
        if (result.indexed) {
            result.index = (uint32_t)(int32_t)(int16_t)
                game_memory_read_u16(memory, index_address);
            address += result.index * 4u;
        }
        result.value = game_memory_read_u32(memory, address);
        if (!memory->fault) result.decision = GAMEPLAY_OWNER_LOOKUP_FOUND;
        return result;
    }
    result.decision = GAMEPLAY_OWNER_LOOKUP_UNKNOWN_TARGET;
    return result;
}
