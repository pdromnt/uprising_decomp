#ifndef UPRISING_GAMEPLAY_CATALOG_H
#define UPRISING_GAMEPLAY_CATALOG_H
#include "game_memory.h"
/* Compatibility decoding is kept at this boundary while the configuration
 * producer and resource-table owners are reconstructed. Queries themselves
 * use catalog family/variant decisions rather than executable jump tables. */
GameAddress gameplay_catalog_record(GameMemory *, GameAddress catalog, uint8_t type);
uint32_t gameplay_catalog_value(GameMemory *, GameAddress catalog, uint8_t type,
    uint32_t item, uint32_t alternate);
uint32_t gameplay_catalog_mount_value(GameMemory *, GameAddress catalog,
    uint8_t item, uint8_t type, uint32_t variant);
int gameplay_catalog_object_property(GameMemory *,GameAddress,unsigned,uint32_t *);
typedef int (*GameplayCatalogPropertyQuery)(void *,GameAddress,unsigned,uint32_t *);
int gameplay_source_property(GameMemory *,GameAddress,unsigned,void *,GameplayCatalogPropertyQuery,uint32_t *);
int gameplay_catalog_can_spend(GameMemory *,GameAddress catalog,uint32_t cost);
int gameplay_catalog_credit(GameMemory *,GameAddress,uint32_t);
int gameplay_catalog_spend(GameMemory *,GameAddress catalog,uint32_t cost);
uint32_t gameplay_catalog_component_property(GameMemory *, GameAddress catalog,
    uint8_t type, unsigned index, int alternate, int local_only,
    GameAddress local_catalog);
typedef int (*GameplayCatalogMemberLookup)(void *,GameAddress,uint32_t,GameAddress *);
int gameplay_catalog_resolve_identity(GameMemory *,uint32_t identity,
    GameplayCatalogMemberLookup foreign_root,GameplayCatalogMemberLookup foreign_member,
    void *context,GameAddress *result);
/* Stable method IDs select ordinary native queries. The optional callback is
 * used only for member classes whose distinct lookup is not reconstructed yet.
 * Return 1 handled, 0 unknown top-level method, -1 memory/service fault. */
int gameplay_catalog_lookup(GameMemory *,GameAddress owner,uint32_t method,
    uint32_t identity,GameplayCatalogMemberLookup,void *,GameAddress *result);
/* Collection records retain their existing owner and member array. Foreign
 * catalog price/budget classes are explicit ports, with ordinary arguments. */
typedef struct GameplayUnitServices {
    void *context;
    int (*price)(void *,GameAddress owner,int16_t unit,uint32_t *);
    int (*can_afford)(void *,GameAddress owner,uint32_t cost,uint32_t *);
} GameplayUnitServices;
int gameplay_catalog_unit_price(GameMemory *,GameAddress owner,int16_t unit,uint32_t *);
int gameplay_catalog_unit_enabled(GameMemory *,GameAddress owner,int16_t unit);
/*47ede0: selected vehicle/structure capability.1 handled,0 unknown method,
 *-1 memory fault. Neutral capability fields retain the verified record layout. */
int gameplay_catalog_choice(GameMemory *,GameAddress,uint8_t,uint32_t *);
int gameplay_catalog_unit_affordable(GameMemory *,GameAddress owner,uint32_t cost,uint32_t *);
int gameplay_collection_eligibility(GameMemory *,const GameplayUnitServices *,
    GameAddress collection,int16_t unit,uint32_t *free_slot,uint16_t *reason);
GameAddress gameplay_player_choose_collection(GameMemory *,const GameplayUnitServices *,uint8_t unit);
GameAddress gameplay_collection_message(uint8_t unit);
GameAddress gameplay_unit_name(int16_t unit);
typedef struct GameplayFeedbackServices {
    void *context;
    GameplayUnitServices units;
    int (*sound)(void *,uint32_t);
    /* Exactly one of message/text is supplied. Text is borrowed for this call. */
    int (*text)(void *,GameAddress message,const char *text);
    int (*diagnostic)(void *,GameAddress message);
} GameplayFeedbackServices;
int gameplay_collection_feedback(GameMemory *,const GameplayFeedbackServices *,int16_t kind,uint8_t unit);
#endif
