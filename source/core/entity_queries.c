#include "entity_queries.h"
#include <math.h>
#include <string.h>

static float query_float(GameMemory *m,GameAddress p){uint32_t bits=game_memory_read_u32(m,p);float value;memcpy(&value,&bits,4);return value;}
static int32_t query_integer(float value){
    uint32_t bits=(uint32_t)(uint64_t)(!isfinite(value)||value< -0x1p63L||value>=0x1p63L?INT64_MIN:(int64_t)value);
    int32_t result;memcpy(&result,&bits,4);return result;
}
/*40f640 searches the existing spatial buckets in their original row order.
 *Candidates are borrowed; visibility/relationship state has one existing owner. */
GameAddress native_entity_nearest_hostile(GameMemory *m,GameAddress source){
    int32_t x=query_integer(query_float(m,source+24)),z=query_integer(query_float(m,source+32));
    int32_t range;uint32_t word=game_memory_read_u32(m,source+0x120);int row,column,steps,low_x,high_x,low_z,high_z;
    unsigned visits=0;float maximum,best=0x1.fffffep127f;GameAddress result=0;
    memcpy(&range,&word,4);steps=range/4096+(range%4096>0);
    x=x/4096-(x%4096<0);z=z/4096-(z%4096<0);
    low_x=x-steps;if(low_x<0)low_x=0;high_x=x+steps;if(high_x>32)high_x=32;
    low_z=z-steps;if(low_z<0)low_z=0;high_z=z+steps;if(high_z>32)high_z=32;
    maximum=(float)((long double)range*range);
    for(row=low_x;row<high_x && !m->fault;row++)for(column=low_z;column<high_z && !m->fault;column++){
        GameAddress other=game_memory_read_u32(m,0x5f0804u+(unsigned)row*0x180u+(unsigned)column*12u);
        while(other && !m->fault){
            uint32_t flags=game_memory_read_u32(m,other+16);NativeEntityRelationshipResult relation;
            if(++visits>65536u){m->fault=1;m->fault_address=other;return 0;}
            if(!(flags&0x800u)){
                long double dx=(long double)query_float(m,source+24)-query_float(m,other+24);
                long double dy=(long double)query_float(m,source+28)-query_float(m,other+28);
                long double dz=(long double)query_float(m,source+32)-query_float(m,other+32);
                long double distance=(dy*dy+dz*dz)+dx*dx;float stored=(float)distance;
                if((distance<=maximum || isnan(distance)) && (stored<best || isnan(stored) || isnan(best)) &&
                   native_entity_relationship_query(m,other,source,&relation) &&
                   relation.decision>=NATIVE_ENTITY_RELATION_ACCEPT_NULL_SECOND){result=other;best=stored;}
            }
            other=game_memory_read_u32(m,other+4);
        }
    }
    return m->fault?0:result;
}
/*53e470/53f170/53fc40 share the active descriptor-payload query. */
int native_entity_descriptor_target(GameMemory *m,GameAddress descriptor,GameAddress *target){
    GameAddress method=game_memory_read_u32(m,game_memory_read_u32(m,descriptor)+0x10),payload;
    if(!target || (method!=0x53e470u && method!=0x53f170u && method!=0x53fc40u) || m->fault)return 0;
    payload=game_memory_read_u32(m,descriptor+0x48);
    *target=game_memory_read_u32(m,payload+20)?payload:0;return !m->fault;
}

uint32_t native_entity_class_limit(unsigned class_index) {
    static const uint32_t limits[] = {60u, 100u, 40u, 70u};
    return class_index < sizeof(limits) / sizeof(limits[0]) ?
        limits[class_index] : 0u;
}

int native_entity_range_contains(float source, float first, float second,
    float lower_scale, float upper_scale, float upper_bias) {
    float lower = (float)((long double)source * (long double)lower_scale);
    float upper = (float)((long double)source * (long double)upper_scale +
        (long double)upper_bias);
    return lower <= first && first <= upper &&
        lower <= second && second <= upper;
}

int native_entity_relationship_query(GameMemory *memory, GameAddress first,
    GameAddress second, NativeEntityRelationshipResult *result) {
    uint32_t first_type;
    uint32_t second_type;
    uint32_t first_flags;
    uint32_t second_flags = 0u;
    uint8_t first_category = 0u;
    uint32_t table_entry;

    if (!result) return 0;
    second_type = game_memory_read_u32(memory, second + 0x14u) & 0x3fu;
    first_type = game_memory_read_u32(memory, first + 0x14u) & 0x3fu;
    table_entry = game_memory_read_u32(memory,
        0x005f0160u + first_type * 4u + second_type * 24u);
    if (memory->fault) return 0;

    result->first_type = first_type;
    result->first_flags = 0u;
    result->second_flags = 0u;
    result->first_category = 0u;
    if (table_entry != 0u) {
        result->decision = NATIVE_ENTITY_RELATION_REJECT_TABLE;
        return 1;
    }

    first_flags = game_memory_read_u32(memory, first + 0x10u);
    if (memory->fault) return 0;
    result->first_flags = first_flags;
    if ((first_flags & 0x8040u) == 0u) {
        first_category = game_memory_read_u8(memory, first + 0x0cu);
        if (memory->fault) return 0;
        result->first_category = first_category;
        if (first_category != 0x20u) {
            result->decision = NATIVE_ENTITY_RELATION_REJECT_CATEGORY;
            return 1;
        }
    }

    if (second == 0u) {
        result->decision = NATIVE_ENTITY_RELATION_ACCEPT_NULL_SECOND;
        return 1;
    }
    second_flags = game_memory_read_u32(memory, second + 0x10u);
    if (memory->fault) return 0;
    result->second_flags = second_flags;
    if ((first_flags & 0x4000u) != 0u) {
        result->decision = (second_flags & 0x1000u) != 0u ?
            NATIVE_ENTITY_RELATION_ACCEPT_FIRST_FLAG :
            NATIVE_ENTITY_RELATION_REJECT_FIRST_FLAG;
        return 1;
    }
    result->decision = (second_flags & 0x2000u) != 0u ?
        NATIVE_ENTITY_RELATION_ACCEPT_SECOND_FLAG :
        NATIVE_ENTITY_RELATION_REJECT_SECOND_FLAG;
    return 1;
}

/*53fd78/53fe68 classify the same descriptor-bearing kinds. These were
 *instruction-section jump tables, not mutable game data. */
static uint32_t descriptor_class(uint32_t type){
    if(type==0x23u)return 0u;
    if(type>=0x80u && type<=0x87u)return 1u;
    if(type==0x88u || type==0x89u)return 2u;
    return 3u;
}
NativeEntityTypeRewardResult native_entity_type_reward(GameMemory *memory,
        GameAddress object, GameAddress source) {
    NativeEntityTypeRewardResult result = {
        NATIVE_ENTITY_TYPE_REWARD_ZERO, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
        0u
    };
    result.state = game_memory_read_u32(memory, object + 0x10u);
    if (memory->fault || (result.state & 0x00020000u) != 0u) return result;
    result.type = game_memory_read_u8(memory, object + 0x0cu);
    result.adjusted_type = result.type - 35u;
    if (result.adjusted_type <= 102u) {
        static const GameAddress targets[]={0x53fd22u,0x53fd31u,0x53fd40u,0x53fd52u};
        result.class_index = descriptor_class(result.type);
        result.target = targets[result.class_index];
    } else {
        result.target = 0x0053fd52u;
    }
    if (memory->fault) return result;
    switch (result.target) {
        case 0x0053fd22u:
            result.child = game_memory_read_u32(memory, object + 0x294u);
            break;
        case 0x0053fd31u:
            result.child = game_memory_read_u32(memory, object + 0xa0u);
            break;
        case 0x0053fd40u:
            result.child = game_memory_read_u32(memory, object + 0xb4u);
            break;
        case 0x0053fd52u:
            return result;
        default:
            result.decision = NATIVE_ENTITY_TYPE_REWARD_DEFER;
            return result;
    }
    result.child_state = game_memory_read_u32(memory, result.child + 0x10u);
    if (memory->fault || result.child_state != 0u) return result;
    result.source_reward = game_memory_read_u32(memory, source + 0x58u);
    result.reward = result.source_reward * 2u;
    if (!memory->fault) result.decision = NATIVE_ENTITY_TYPE_REWARD_VALUE;
    return result;
}

NativeEntityTypeActivityPlan native_entity_type_activity_plan(
        GameMemory *memory, GameAddress object) {
    NativeEntityTypeActivityPlan plan = {
        NATIVE_ENTITY_TYPE_ACTIVITY_DISABLED, 0u, 0u, 0u, 0u, 0u, 0u
    };
    plan.state = game_memory_read_u32(memory, object + 0x10u);
    if (memory->fault || ((plan.state >> 8u) & 1u) == 0u) return plan;
    plan.type = game_memory_read_u8(memory, object + 0x0cu);
    plan.adjusted_type = plan.type - 35u;
    if (plan.adjusted_type <= 102u) {
        static const GameAddress targets[]={0x53fe20u,0x53fe28u,0x53fe30u,0x53fe0cu};
        plan.class_index = descriptor_class(plan.type);
        plan.target = targets[plan.class_index];
    } else {
        plan.target = 0x0053fe0cu;
    }
    if (memory->fault) return plan;
    switch (plan.target) {
        case 0x0053fe20u:
            plan.child = game_memory_read_u32(memory, object + 0x294u);
            plan.decision = NATIVE_ENTITY_TYPE_ACTIVITY_DIRECT;
            break;
        case 0x0053fe28u:
            plan.child = game_memory_read_u32(memory, object + 0xa0u);
            plan.decision = NATIVE_ENTITY_TYPE_ACTIVITY_DIRECT;
            break;
        case 0x0053fe30u:
            plan.child = game_memory_read_u32(memory, object + 0xb4u);
            plan.decision = NATIVE_ENTITY_TYPE_ACTIVITY_DIRECT;
            break;
        case 0x0053fe0cu:
            plan.decision = NATIVE_ENTITY_TYPE_ACTIVITY_PROBE_DEFAULT;
            break;
        default:
            plan.decision = NATIVE_ENTITY_TYPE_ACTIVITY_DEFER;
            break;
    }
    return plan;
}

int native_entity_type_activity(GameMemory *memory,GameAddress object,int *active){
    NativeEntityTypeActivityPlan plan=native_entity_type_activity_plan(memory,object);
    GameAddress child=plan.child;*active=0;
    if(memory->fault || plan.decision==NATIVE_ENTITY_TYPE_ACTIVITY_DEFER)return 0;
    /* 403370 is an empty diagnostic hook. The default checks the object itself. */
    if(plan.decision==NATIVE_ENTITY_TYPE_ACTIVITY_PROBE_DEFAULT)child=object;
    if(child)*active=game_memory_read_u32(memory,child+0x10u)==0u;
    return !memory->fault;
}
int native_entity_reset_attachments(GameMemory *memory, GameAddress player,
    NativeEntityAttachmentResetResult *result) {
    uint32_t first;
    uint32_t second;
    uint32_t first_flags;
    uint32_t second_flags;

    if (!result) return 0;
    first = game_memory_read_u32(memory, player + 0x21cu);
    if (memory->fault) return 0;
    if (first != 0u) {
        game_memory_write_u32(memory, first + 0xe4u, 0u);
        game_memory_write_u32(memory, player + 0x21cu, 0u);
    }
    second = game_memory_read_u32(memory, player + 0x220u);
    if (memory->fault) return 0;
    if (second != 0u) {
        game_memory_write_u32(memory, second + 0xe4u, 0u);
        game_memory_write_u32(memory, player + 0x220u, 0u);
    }
    first_flags = game_memory_read_u32(memory, player + 0x140u);
    game_memory_write_u16(memory, player + 0x14au, 0u);
    game_memory_write_u16(memory, player + 0x14cu, 0u);
    second_flags = game_memory_read_u32(memory, player + 0x144u);
    game_memory_write_u16(memory, player + 0x26cu, 0u);
    game_memory_write_u16(memory, player + 0x26eu, 0u);
    game_memory_write_u32(memory, player + 0x140u, first_flags & 0x3fu);
    game_memory_write_u32(memory, player + 0x144u, second_flags & 0x3fu);
    if (memory->fault) return 0;

    result->first_attachment = first;
    result->second_attachment = second;
    result->first_flags = first_flags;
    result->second_flags = second_flags;
    return 1;
}

int native_entity_store_category_scalar(GameMemory *memory, GameAddress self,
    uint32_t category, uint32_t value, NativeCategoryScalarResult *result) {
    uint32_t selector = 0u;
    GameAddress target = 0u;
    uint32_t stored = value;
    NativeCategoryScalarKind kind = NATIVE_CATEGORY_SCALAR_RAW;

    if (!result) return 0;
    if (category <= 0x11u) {
        selector = game_memory_read_u8(memory, 0x00470044u + category);
        target = game_memory_read_u32(memory,
            0x00470034u + selector * 4u);
        if (memory->fault) return 0;
        if (target == 0x00470018u) {
            kind = NATIVE_CATEGORY_SCALAR_SCALED;
            stored = value * 10u;
        } else if (target != 0x00470028u) {
            kind = NATIVE_CATEGORY_SCALAR_DEFER;
        }
    }
    if (kind != NATIVE_CATEGORY_SCALAR_DEFER)
        game_memory_write_u32(memory, self + 0x54u + category * 4u,
            stored);
    if (memory->fault) return 0;
    result->kind = kind;
    result->selector = selector;
    result->target = target;
    result->stored_value = stored;
    return 1;
}

NativeEntityResolveResult native_entity_resolve(GameMemory *memory,
        GameAddress owner, uint32_t identifier) {
    NativeEntityResolveResult result;
    GameAddress primary = game_memory_read_u32(memory, owner + 0x12cu);
    GameAddress node;
    result.source = NATIVE_ENTITY_RESOLVE_NONE;
    result.match = 0u;
    result.primary_node = 0u;
    result.auxiliary_tail_id = 0u;
    result.owner_tail_id = owner;
    result.last_compared_id = 0u;
    result.compared = 0;
    for (node = primary; node != 0u && !memory->fault;
            node = game_memory_read_u32(memory, node + 0x48u)) {
        uint32_t flags = game_memory_read_u32(memory, node + 0x10u);
        if ((flags & 0x00100000u) != 0u) {
            GameAddress payload = game_memory_read_u32(memory, node + 0xa8u);
            if (payload != 0u && game_memory_read_u32(memory,
                    payload + 0x14u) == identifier) {
                result.source = NATIVE_ENTITY_RESOLVE_PRIMARY_PAYLOAD;
                result.match = payload;
                result.primary_node = node;
                return result;
            }
            if (payload != 0u) {
                result.last_compared_id = game_memory_read_u32(memory,
                    payload + 0x14u);
                result.compared = 1;
            }
        }
    }
    node = game_memory_read_u32(memory, owner + 0x64u);
    while (node != 0u && !memory->fault) {
        result.auxiliary_tail_id = game_memory_read_u32(memory, node + 0x14u);
        result.last_compared_id = result.auxiliary_tail_id;
        result.compared = 1;
        if (result.auxiliary_tail_id == identifier) {
            result.source = NATIVE_ENTITY_RESOLVE_AUXILIARY;
            result.match = node;
            return result;
        }
        node = game_memory_read_u32(memory, node + 0x48u);
    }
    node = game_memory_read_u32(memory, owner + 0xc8u);
    while (node != 0u && !memory->fault) {
        result.owner_tail_id = game_memory_read_u32(memory, node + 0x14u);
        result.last_compared_id = result.owner_tail_id;
        result.compared = 1;
        if (result.owner_tail_id == identifier) {
            result.source = NATIVE_ENTITY_RESOLVE_PAYLOAD_OWNER;
            result.match = node;
            return result;
        }
        node = game_memory_read_u32(memory, node + 0x48u);
    }
    for (node = primary; node != 0u && !memory->fault;
            node = game_memory_read_u32(memory, node + 0x48u)) {
        result.last_compared_id = game_memory_read_u32(memory, node + 0x14u);
        result.compared = 1;
        if (result.last_compared_id == identifier) {
            result.source = NATIVE_ENTITY_RESOLVE_PRIMARY_DIRECT;
            result.match = node;
            return result;
        }
    }
    return result;
}

GameAddress native_entity_resolve_lists(GameMemory *memory,GameAddress owner,uint32_t identifier){
    static const uint32_t heads[3]={0x64u,0xc8u,0x12cu};unsigned list;
    for(list=0u;list<3u && !memory->fault;list++){
        GameAddress node=game_memory_read_u32(memory,owner+heads[list]);unsigned visits=0u;
        while(node && !memory->fault){
            if(game_memory_read_u32(memory,node+0x14u)==identifier)return node;
            if(++visits>65536u){memory->fault=1;return 0u;}
            node=game_memory_read_u32(memory,node+0x48u);
        }
    }
    return 0u;
}

NativeObjectSlotResult native_object_slot_resolve(GameMemory *memory,
        GameAddress object) {
    NativeObjectSlotResult result;
    GameAddress type_map = game_memory_read_u32(memory, 0x0058973cu);
    uint32_t required_type = game_memory_read_u32(memory, 0x005f0220u);
    int32_t entity_count = (int32_t)(int16_t)game_memory_read_u16(memory,
        0x00639a74u);
    GameAddress entity_table = game_memory_read_u32(memory, 0x00639a70u);
    uint32_t special_mode = game_memory_read_u32(memory, object + 0x2a0u);
    uint32_t slot_index;
    result.slot = 0u;
    result.slot_index = 11u;
    result.compare_left = 0u;
    result.compare_right = 0u;
    result.compare_width = 32u;
    result.found = 0;
    for (slot_index = 0u; slot_index < 11u && !memory->fault; ++slot_index) {
        int32_t slot = (int32_t)game_memory_read_u32(memory,
            object + 0xd68u + slot_index * 4u);
        uint32_t entity_index;
        if (slot <= 0 || game_memory_read_u32(memory,
                type_map + (uint32_t)slot * 4u + 0x200u) != 0u)
            continue;
        if (slot != 10) {
            result.slot = (uint32_t)slot;
            result.slot_index = slot_index;
            result.compare_left = (uint32_t)slot;
            result.compare_right = 10u;
            result.found = 1;
            return result;
        }
        for (entity_index = 0u;
                entity_index < (uint32_t)(entity_count > 0 ? entity_count : 0);
                ++entity_index) {
            GameAddress entity = game_memory_read_u32(memory,
                entity_table + entity_index * 4u);
            GameAddress payload;
            uint32_t owner;
            uint32_t payload_value;
            if (game_memory_read_u32(memory, entity + 0x54u) != required_type)
                continue;
            owner = game_memory_read_u32(memory, entity + 0x14u);
            payload = game_memory_read_u32(memory, entity + 0x18u);
            payload_value = game_memory_read_u32(memory, payload);
            if (special_mode != 0u) {
                int16_t left;
                int16_t right;
                if (payload_value == 0u || owner == 0u) continue;
                left = (int16_t)game_memory_read_u16(memory, entity + 0x24u);
                right = (int16_t)game_memory_read_u16(memory, entity + 0x26u);
                if (left <= right) continue;
                result.compare_left = (uint16_t)left;
                result.compare_right = (uint16_t)right;
                result.compare_width = 16u;
            } else {
                if (owner == 0u || payload_value == 0u) continue;
                result.compare_left = payload_value;
                result.compare_right = 0u;
            }
            result.slot = 10u;
            result.slot_index = slot_index;
            result.found = 1;
            return result;
        }
    }
    return result;
}

NativeEntityClassification native_entity_classification(GameMemory *memory,
        GameAddress object) {
    NativeEntityClassification result = {0};
    result.descriptor = game_memory_read_u32(memory, object + 0x10u);
    if (!memory->fault)
        result.ordinal = game_memory_read_u8(memory,
            result.descriptor + 0x0du);
    if (!memory->fault)
        result.high_table = game_memory_read_u32(memory,
            result.descriptor + 0x10u);
    if (!memory->fault)
        result.high = game_memory_read_u8(memory,
            result.high_table + result.ordinal - 1u);
    if (!memory->fault)
        result.low_table = game_memory_read_u32(memory,
            result.descriptor + 0x14u);
    if (!memory->fault)
        result.low = game_memory_read_u8(memory,
            result.low_table + result.ordinal - 1u);
    return result;
}

void native_entity_store_table_object(GameMemory *memory,
        GameAddress manager, int32_t index, GameAddress object) {
    game_memory_write_u32(memory,
        manager + (uint32_t)index * 4u + 0x20u, object);
}

void native_entity_clear_table(GameMemory *memory, GameAddress table,
        uint32_t count, int reverse) {
    uint32_t index;
    for (index = 0u; index < count && !memory->fault; ++index)
        game_memory_write_u32(memory, reverse ? table - index * 4u :
            table + index * 4u, 0u);
}

NativeEntityStatusDispatch native_entity_status_dispatch(GameMemory *memory,
        uint32_t index) {
    NativeEntityStatusDispatch result = {0u, 0u, 0u};
    result.entry = game_memory_read_u32(memory,
        0x005f0210u + (index & 0x3fu) * 4u);
    if (!memory->fault)
        result.vtable = game_memory_read_u32(memory, result.entry);
    if (!memory->fault)
        result.target = game_memory_read_u32(memory, result.vtable + 0x4cu);
    return result;
}
