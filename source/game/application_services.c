#include "application_entry.h"
#include "game_initializers.h"
#include "recyclable_records.h"
#include "visibility_state.h"
#include "gameplay_state.h"
#include "model_assets.h"
#include "menu_network.h"
#include "level_load.h"
#include "level_hud.h"
#include "level_run.h"
#include "gameplay_chat.h"
#include "level_script.h"
#include "level_factory.h"
#include <string.h>

extern int game_application_options_load(GameApplication *, uint32_t);
extern void game_application_ui_shutdown(GameApplication *);

/* 00457b80: both recycling modes and the embedded controller use the existing
 * native record owner and retirement algorithms. No second pool. */
int game_level_visibility_pool_reset(GameApplication *app, GameAddress owner) {
    GameMemory *m=&app->memory;
    uint32_t count;
    if (!game_visibility_state_initialize_begin(m, owner) ||
        !game_visibility_state_initialize_limits(m, owner)) return 0;
    count = game_memory_read_u32(m, owner + 0x12034u);
    while (count < 0x80000000u) {
        GameAddress record;
        if (count >= 100u) return 0;
        record = game_memory_read_u32(m, owner + 0x1203cu + count * 4u);
        if (!recyclable_outer_retire(m, record, NULL)) return 0;
        --count;
        game_memory_write_u32(m, owner + 0x12034u, count);
    }
    if (!game_memory_fill_u32(m, owner + 0x1203cu, 100u, 0u, 0)) return 0;
    game_memory_write_u32(m, owner + 0x12034u, UINT32_MAX);
    if (!recyclable_container_reset(m, owner + 0x1810u, 0u, 0u, 0u, 0, 0, NULL))
        return 0;
    return game_memory_fill_u32(m, owner, 0x200u, 0u, 0) && !m->fault;
}

/*4fe870: reset the existing traversal work area and its retained route owner. */
int game_level_work_grid_reset(GameApplication *app,GameAddress owner){
    game_initializer_large_work_area(&app->memory,owner);
    return !app->memory.fault && recyclable_container_reset(&app->memory,
        owner+0x2028u,0u,0u,0u,0,0,NULL);
}

int game_application_services_initialize(GameApplication *app,
    unsigned outer_count, unsigned inner_count) {
    GameMemory *m = &app->memory;
    char path[260];
    GameAddress name;
    unsigned index;
    ResourceCatalogStats stats;
    if (!recyclable_records_preallocate(m, outer_count, inner_count, NULL) ||
        !game_level_visibility_pool_reset(app, 0x5cf038u) ||
        !game_level_work_grid_reset(app,0x60a4c8u)) return 0;
    name = game_memory_read_u32(m, 0x585330u);
    for (index = 0; index < sizeof(path); ++index) {
        path[index] = (char)game_memory_read_u8(m, name + index);
        if (!path[index]) break;
    }
    if (m->fault || index == sizeof(path)) return 0;
    app->resource_index = resource_file_open(path);
    /* The original missing-index open failure does not create a fake catalog. */
    if (!app->resource_index) return !m->fault;
    app->resources = resource_catalog_create();
    if (!app->resources || !resource_catalog_read_index(app->resources,
            resource_file_archive(app->resource_index), &stats)) return 0;
    game_memory_write_u32(m, 0x585328u, 1u); /* owner-local stream identity */
    game_memory_write_u32(m, 0x58532cu, 1u);
    return !m->fault;
}

int game_application_load_blob(GameApplication *app,const char *name,ResourceBlob *blob) {
    ResourceId id;ResourceEntryInfo info;
    ResourcePathBytes path={(const uint8_t *)name,strlen(name)+1u};
    if(resource_blob_open_file(name,blob))return 1;
    if(!app->resources || !app->resource_index ||
        resource_catalog_find(app->resources,path,&id)!=RESOURCE_TREE_OK ||
        !resource_catalog_info(app->resources,id,&info))return 0;
    return resource_blob_load(resource_file_archive(app->resource_index),info,0,blob).status==RESOURCE_LOAD_OK;
}

int game_application_options_initialize(GameApplication *app) {
    GameplayNumericTableSpec spec = gameplay_state_numeric_table_spec(0u);
    uint32_t bits;
    float scalar;
    long double denominator = 1.0L;
    GameAddress output;
    if (!game_application_options_load(app, 0u)) return 0;
    bits = game_memory_read_u32(&app->memory, spec.scalar);
    memcpy(&scalar, &bits, sizeof(scalar));
    /* 00412690: retain the extended accumulator; round only each table store. */
    for (output = spec.output; output < spec.end; output += 4u) {
        float value = (float)((long double)scalar / denominator);
        memcpy(&bits, &value, sizeof(bits));
        game_memory_write_u32(&app->memory, output, bits);
        denominator += (long double)scalar;
    }
    return !app->memory.fault;
}

void game_application_services_dispose(GameApplication *app) {
    (void)game_level_cameras_end(app);
    (void)game_level_terrain_depth_end(app);
    (void)game_level_vehicle_factory_shutdown(app);
    (void)game_level_radar_tables_end(app);
    (void)game_level_chat_release(app);
    (void)game_level_hud_assets_release(app);
    (void)game_level_display_masks_release(app);
    (void)game_level_input_regions_release(app);
    (void)game_level_transition_assets_release(app);
    (void)game_level_navigation_routes_release(app,0x604258u);
    (void)game_level_script_release(app);
    (void)game_level_terrain_release(app);
    game_network_dispose(app);
    game_model_assets_dispose(app);
    resource_file_close(app->resource_index);
    app->resource_index = NULL;
    resource_catalog_destroy(app->resources);
    app->resources = NULL;
    /* Static visibility/container destructors still consume this shared owner.
     * The platform retires it after those destructors, alongside scene storage. */
}
void game_application_services_release(GameApplication *app) {
    game_application_ui_shutdown(app); /* 00439920; not silently omitted */
    game_application_services_dispose(app);
}
