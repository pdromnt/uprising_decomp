#ifndef UPRISING_GAMEPLAY_LOOKUP_PLANS_H
#define UPRISING_GAMEPLAY_LOOKUP_PLANS_H

#include "game_memory.h"

#include <stdint.h>

typedef struct GameplayCapabilityPlan {
    uint32_t field_offset;
    uint32_t immediate_result;
    int valid;
    int immediate;
} GameplayCapabilityPlan;

typedef struct GameplayEventRoute {
    GameAddress pool;
    GameAddress return_address;
    int valid;
} GameplayEventRoute;

typedef struct GameplayResourceSelectorPlan {
    uint32_t field_offset;
    GameAddress base;
    int use_ecx;
    int valid;
} GameplayResourceSelectorPlan;

typedef struct GameplayGridPointPlan {
    uint32_t output_offset;
    GameAddress x_scale;
    GameAddress y_scale;
    int zero_x;
} GameplayGridPointPlan;

typedef struct GameplayPresentationDimensions {
    uint32_t width;
    uint32_t height;
} GameplayPresentationDimensions;

typedef struct GameplayProjectionScalarPlan {
    GameAddress first;
    GameAddress second;
    GameAddress output;
    uint32_t operation;
} GameplayProjectionScalarPlan;

typedef struct GameplayGlobalInitializerPlan {
    GameAddress owner;
    GameAddress constructor;
    GameAddress constructor_return;
    GameAddress callback;
    GameAddress callback_register;
    GameAddress callback_return;
} GameplayGlobalInitializerPlan;

typedef enum GameplayOwnerLookupDecision {
    GAMEPLAY_OWNER_LOOKUP_OUT_OF_RANGE,
    GAMEPLAY_OWNER_LOOKUP_EMPTY,
    GAMEPLAY_OWNER_LOOKUP_FOUND,
    GAMEPLAY_OWNER_LOOKUP_UNKNOWN_TARGET
} GameplayOwnerLookupDecision;

typedef struct GameplayOwnerLookupResult {
    GameplayOwnerLookupDecision decision;
    GameAddress target;
    uint32_t selector;
    uint32_t index;
    uint32_t value;
    int indexed;
} GameplayOwnerLookupResult;

GameplayCapabilityPlan gameplay_lookup_capability_plan(GameAddress target);
GameplayEventRoute gameplay_lookup_event_route(uint32_t type);
GameplayResourceSelectorPlan gameplay_lookup_resource_selector(
    GameAddress target, int value_only);
GameplayGridPointPlan gameplay_lookup_grid_point(unsigned index);
void gameplay_lookup_initialize_presentation(GameMemory *memory);
GameplayPresentationDimensions gameplay_lookup_publish_dimensions(
    GameMemory *memory);
void gameplay_lookup_publish_presentation_center(GameMemory *memory,
    GameplayPresentationDimensions dimensions);
GameplayProjectionScalarPlan gameplay_lookup_projection_scalar(unsigned index);
GameplayGlobalInitializerPlan gameplay_lookup_global_initializer(void);
GameplayOwnerLookupResult gameplay_lookup_object_owner(GameMemory *memory,
    uint8_t kind, GameAddress index_address);

#endif
