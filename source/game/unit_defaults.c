#include "application_entry.h"
#include "game_state_ops.h"
#include "scene_projection_storage.h"
#include "unit_defaults.h"

typedef struct UnitDefault {GameAddress address;uint32_t value;} UnitDefault;
typedef struct UnitInheritance {GameAddress base,source;unsigned width,records;} UnitInheritance;
static const UnitDefault unit_fields[]={
#include "unit_defaults.inc"
};
static const UnitInheritance unit_inheritance[]={
#include "unit_inheritance.inc"
    {0x5e7d50u,0u,21u,4u},{0x5e4fc0u,0u,21u,15u},{0x5e6d80u,0u,21u,1u}
};

/* 00460f50 and its seven builders. These are immutable field recipes and
 * ordered predecessor chains, not an execution table. Existing bounded records
 * stay authoritative; fields omitted by the original constructors stay intact.
 * 00460e70/00460ee0 fill only the inherited (-2) fields, including the second
 * bank's inheritance from the resolved first bank, never from the parent's
 * second bank. Configured values survive subsequent initialization. */
int game_menu_game_defaults(GameApplication *a) {
    static const UnitDefault shared[]={

        {0x005e9210u,250u},{0x005e9214u,0u},{0x005e9218u,0u},
        {0x005e921cu,0u},{0x005e9220u,10u},{0x005e9224u,10u},
        {0x005e9228u,10u},{0x005e922cu,10u},{0x005e9238u,100u},
        {0x005e923cu,0u},{0x005e9240u,0u},{0x005e9244u,100u},
        {0x005e9248u,0u},{0x005e924cu,0u},{0x005e9250u,0xffffffffu},
        {0x005e9254u,0u},{0x005e9258u,0u},{0x005e925cu,0u},
        {0x005e9260u,1u},{0x005e4678u,100u},{0x005e467cu,10u},
        {0x005e4680u,0u},{0x005e4684u,0u},{0x005e4688u,100u},
        {0x005e468cu,0u},{0x005e4690u,0xffffffffu},
        {0x005e4694u,0u},{0x005e4698u,0xffffffffu},
        {0x005e469cu,0u},{0x005e46a0u,0u},{0x005e46a4u,0u},
        {0x005e46a8u,1u},{0x005e9230u,50u},{0x005e9234u,2u}
    };
    GameMemory *m=&a->memory;unsigned i,j;
    for(i=0;i<sizeof(shared)/sizeof(shared[0]) && !m->fault;++i)
        game_memory_write_u32(m,shared[i].address,shared[i].value);
    for(i=0;i<sizeof(unit_fields)/sizeof(unit_fields[0]) && !m->fault;++i)
        game_memory_write_u32(m,unit_fields[i].address,unit_fields[i].value);
    for(i=0;i<sizeof(unit_inheritance)/sizeof(unit_inheritance[0]) && !m->fault;++i) {
        const UnitInheritance *c=&unit_inheritance[i];GameAddress source=c->source;
        uint32_t stride=c->width==21u?0xb0u:0x70u;
        for(j=0;j<c->records && !m->fault;++j) {
            GameAddress object=c->base+j*stride;
            game_state_apply_default_pair_array(m,object,source,c->width,c->width*4u,
                c->width==21u?0x5e9210u:0x5e4678u);
            source=object;
        }
    }
    return !m->fault;
}
/* 0045fb00. The later configuration loader owns the optional string array.
 * Reject foreign/misaligned owners rather than releasing arbitrary storage.
 * Clear completed entries so practical cleanup can retry after a bad member. */
int game_menu_game_defaults_end(GameApplication *a) {
    GameMemory *m=&a->memory;GameAddress array=game_memory_read_u32(m,0x585640u);
    int32_t count=(int32_t)game_memory_read_u32(m,0x5e1484u);
    uint32_t base,size;int32_t i;
    if(m->fault)return 0;
    if(!array)return 1;
    if(!scene_projection_storage_describe(array,&base,&size,0) || base!=array ||
        (count>0 && (uint32_t)count>size/4u)) {m->fault=1;return 0;}
    for(i=0;i<count && !m->fault;++i) {
        GameAddress member=game_memory_read_u32(m,array+(uint32_t)i*4u);
        if(member && !scene_projection_storage_free(member)) {m->fault=1;return 0;}
        game_memory_write_u32(m,array+(uint32_t)i*4u,0u);
    }
    if(m->fault || !scene_projection_storage_free(array))return 0;
    game_memory_write_u32(m,0x585640u,0u);
    return !m->fault;
}