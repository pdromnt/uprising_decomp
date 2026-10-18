#ifndef UPRISING_NATIVE_ENTITY_QUERIES_H
#define UPRISING_NATIVE_ENTITY_QUERIES_H

#include "game_memory.h"

#include <stdint.h>

GameAddress native_entity_resolve_lists(GameMemory *,GameAddress owner,uint32_t identifier);
GameAddress native_entity_nearest_hostile(GameMemory *,GameAddress source);
int native_entity_descriptor_target(GameMemory *,GameAddress descriptor,GameAddress *target);

uint32_t native_entity_class_limit(unsigned class_index);
int native_entity_range_contains(float source, float first, float second,
    float lower_scale, float upper_scale, float upper_bias);

typedef enum NativeEntityRelationshipDecision {
    NATIVE_ENTITY_RELATION_REJECT_TABLE = 0,
    NATIVE_ENTITY_RELATION_REJECT_CATEGORY,
    NATIVE_ENTITY_RELATION_REJECT_FIRST_FLAG,
    NATIVE_ENTITY_RELATION_REJECT_SECOND_FLAG,
    NATIVE_ENTITY_RELATION_ACCEPT_NULL_SECOND,
    NATIVE_ENTITY_RELATION_ACCEPT_FIRST_FLAG,
    NATIVE_ENTITY_RELATION_ACCEPT_SECOND_FLAG
} NativeEntityRelationshipDecision;

typedef struct NativeEntityRelationshipResult {
    NativeEntityRelationshipDecision decision;
    uint32_t first_type;
    uint32_t first_flags;
    uint32_t second_flags;
    uint8_t first_category;
} NativeEntityRelationshipResult;

int native_entity_relationship_query(GameMemory *memory, GameAddress first,
    GameAddress second, NativeEntityRelationshipResult *result);

typedef enum NativeEntityTypeRewardDecision {
    NATIVE_ENTITY_TYPE_REWARD_ZERO,
    NATIVE_ENTITY_TYPE_REWARD_VALUE,
    NATIVE_ENTITY_TYPE_REWARD_DEFER
} NativeEntityTypeRewardDecision;

typedef struct NativeEntityTypeRewardResult {
    NativeEntityTypeRewardDecision decision;
    uint32_t state;
    uint32_t type;
    uint32_t adjusted_type;
    uint32_t class_index;
    GameAddress target;
    GameAddress child;
    uint32_t child_state;
    uint32_t source_reward;
    uint32_t reward;
} NativeEntityTypeRewardResult;

NativeEntityTypeRewardResult native_entity_type_reward(GameMemory *memory,
    GameAddress object, GameAddress source);

typedef enum NativeEntityTypeActivityDecision {
    NATIVE_ENTITY_TYPE_ACTIVITY_DISABLED,
    NATIVE_ENTITY_TYPE_ACTIVITY_DIRECT,
    NATIVE_ENTITY_TYPE_ACTIVITY_PROBE_DEFAULT,
    NATIVE_ENTITY_TYPE_ACTIVITY_DEFER
} NativeEntityTypeActivityDecision;

typedef struct NativeEntityTypeActivityPlan {
    NativeEntityTypeActivityDecision decision;
    uint32_t state;
    uint32_t type;
    uint32_t adjusted_type;
    uint32_t class_index;
    GameAddress target;
    GameAddress child;
} NativeEntityTypeActivityPlan;

NativeEntityTypeActivityPlan native_entity_type_activity_plan(
    GameMemory *memory, GameAddress object);
int native_entity_type_activity(GameMemory *,GameAddress,int *);

typedef struct NativeEntityAttachmentResetResult {
    uint32_t first_attachment;
    uint32_t second_attachment;
    uint32_t first_flags;
    uint32_t second_flags;
} NativeEntityAttachmentResetResult;

int native_entity_reset_attachments(GameMemory *memory, GameAddress player,
    NativeEntityAttachmentResetResult *result);

typedef enum NativeCategoryScalarKind {
    NATIVE_CATEGORY_SCALAR_RAW = 0,
    NATIVE_CATEGORY_SCALAR_SCALED,
    NATIVE_CATEGORY_SCALAR_DEFER
} NativeCategoryScalarKind;

typedef struct NativeCategoryScalarResult {
    NativeCategoryScalarKind kind;
    uint32_t selector;
    GameAddress target;
    uint32_t stored_value;
} NativeCategoryScalarResult;

typedef enum NativeEntityResolveSource {
    NATIVE_ENTITY_RESOLVE_NONE = 0,
    NATIVE_ENTITY_RESOLVE_PRIMARY_PAYLOAD,
    NATIVE_ENTITY_RESOLVE_AUXILIARY,
    NATIVE_ENTITY_RESOLVE_PAYLOAD_OWNER,
    NATIVE_ENTITY_RESOLVE_PRIMARY_DIRECT
} NativeEntityResolveSource;

typedef struct NativeEntityResolveResult {
    NativeEntityResolveSource source;
    GameAddress match;
    GameAddress primary_node;
    uint32_t auxiliary_tail_id;
    uint32_t owner_tail_id;
    uint32_t last_compared_id;
    int compared;
} NativeEntityResolveResult;

typedef struct NativeObjectSlotResult {
    uint32_t slot;
    uint32_t slot_index;
    uint32_t compare_left;
    uint32_t compare_right;
    unsigned compare_width;
    int found;
} NativeObjectSlotResult;

typedef struct NativeEntityClassification {
    GameAddress descriptor;
    uint32_t ordinal;
    GameAddress high_table;
    GameAddress low_table;
    uint32_t high;
    uint32_t low;
} NativeEntityClassification;

typedef struct NativeEntityStatusDispatch {
    GameAddress entry;
    GameAddress vtable;
    GameAddress target;
} NativeEntityStatusDispatch;

int native_entity_store_category_scalar(GameMemory *memory, GameAddress self,
    uint32_t category, uint32_t value, NativeCategoryScalarResult *result);
NativeEntityResolveResult native_entity_resolve(GameMemory *memory,
    GameAddress owner, uint32_t identifier);
NativeObjectSlotResult native_object_slot_resolve(GameMemory *memory,
    GameAddress object);
NativeEntityClassification native_entity_classification(GameMemory *memory,
    GameAddress object);
void native_entity_store_table_object(GameMemory *memory,
    GameAddress manager, int32_t index, GameAddress object);
void native_entity_clear_table(GameMemory *memory, GameAddress table,
    uint32_t count, int reverse);
NativeEntityStatusDispatch native_entity_status_dispatch(GameMemory *memory,
    uint32_t index);

#endif
