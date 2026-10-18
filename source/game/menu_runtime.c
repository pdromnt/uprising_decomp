#include "application_entry.h"
#include "menu_sound.h"
#include "menu_runtime_tables.h"
#include "unit_defaults.h"
#include "gameplay_state.h"
#include "gameplay_constructors.h"
#include "gameplay_property_matrix.h"
#include "game_initializers.h"
#include "render_arena.h"
#include "render_runtime_pools.h"
#include "scene_projection_storage.h"
#include <math.h>
#include <string.h>
#include <time.h>

/* Remaining actual startup algorithms. Native arguments replace their CPU ABI;
 * these definitions remain link blockers until their behavior is reconstructed. */
extern int game_application_ui_begin(GameApplication *); /* 004398e0 */
extern void game_application_ui_shutdown(GameApplication *); /* 00439920 */
extern int game_menu_bindings_load(GameApplication *); /* 004d67c0 */
extern int game_menu_bindings_save(GameApplication *);
extern int game_menu_bindings_defaults(GameApplication *); /* 004d6420 */
extern int game_menu_bindings_reverse(GameApplication *); /* 004d7420 */
extern int game_menu_bindings_labels(GameApplication *,unsigned,unsigned); /* 004d6850 */

extern int game_application_scene_model_release(GameApplication *,GameAddress); /* unreconstructed foreign model destructors */

extern int game_menu_shutdown_systems(GameApplication *); /* 00541230: music */

/* 0054b130/0054b400. A single owned matrix is published after a valid parse.
 * The archive/loose-file blob and temporary parsing array die before consumers
 * run; the bounded record is the sole persistent writable representation. */
static int game_menu_curve_data_end(GameApplication *a) {
    GameAddress matrix=game_memory_read_u32(&a->memory,0x59dae4u);
    if(a->memory.fault || (matrix && !scene_projection_storage_free(matrix)))return 0;
    game_memory_write_u32(&a->memory,0x59dae4u,0);return !a->memory.fault;
}
static int game_menu_curve_data_begin(GameApplication *a) {
    ResourceBlob blob={0};float values[234];char name[260];unsigned i;
    GameAddress matrix;GameMemory *m=&a->memory;
    for(i=0;i<sizeof(name);++i) {name[i]=(char)game_memory_read_u8(m,0x59dae8u+i);if(!name[i])break;}
    if(i==sizeof(name) || m->fault || !game_application_load_blob(a,name,&blob))return 0;
    if(!gameplay_property_matrix_parse(blob.data,blob.size,values)) {resource_blob_release(&blob);return 0;}
    resource_blob_release(&blob);
    matrix=scene_projection_storage_allocate(sizeof(values),"portable gameplay property matrix");
    if(!matrix)return 0;
    for(i=0;i<234u && !m->fault;++i)game_menu_runtime_store_float(m,matrix+i*4u,values[i]);
    if(m->fault || !game_menu_curve_data_end(a)) {scene_projection_storage_free(matrix);return 0;}
    game_memory_write_u32(m,0x59dae4u,matrix);return !m->fault;
}
static int world_buffers(GameMemory *m) {
    unsigned i;long double divisor=sqrtl(1.0L),scale=game_menu_runtime_float(m,0x575228u)/divisor;
    for(i=0;i<4u;++i) {
        GameplayAllocationSpec spec=gameplay_state_world_buffer_spec(i);
        GameAddress buffer=scene_projection_storage_allocate(spec.size,"portable world work buffer");
        if(!buffer)return 0;
        game_memory_write_u32(m,spec.destination,buffer);
    }
    gameplay_state_initialize_world_down_vector(m);
    for(i=0;i<3u;++i)game_menu_runtime_store_float(m,0x624f20u+i*4u,
        (float)((long double)game_menu_runtime_float(m,0x624f20u+i*4u)*scale));
    return !m->fault;
}
/* Render pool IDs use verified layouts, not pointers. Actual access routes to
 * these owners in portable_platform; no serialized mutable backing copy. */
static int render_work_begin(GameMemory *m) {
    unsigned i;
    if(!render_arena_initialize())return 0;
    for(i=0;i<4u;++i)game_memory_write_u32(m,0x597724u+i*8u,render_arena_view(i));
    if(!render_runtime_pools_initialize())return 0;
    for(i=0;i<RENDER_RUNTIME_POOL_COUNT;++i) {
        GameplayAllocationSpec spec=gameplay_state_runtime_pool_spec(i);
        if(spec.size!=render_runtime_pool_size(i))return 0;
        game_memory_write_u32(m,spec.destination,0xfb000000u+i*0x10000u);
    }
    gameplay_state_reset_runtime_pool_tables(m);return !m->fault;
}
/* 00505b30: real application initialization, including the native sound bank
 * and world work owners. Missing adjacent algorithms fail the link explicitly. */
int game_menu_startup_systems(GameApplication *a) {
    GameMemory *m=&a->memory;unsigned i;GameAddress record;
    (void)game_application_ui_begin(a); /*00505b53 ignores missing SDK/hardware. */
    if(m->fault || !game_menu_trig_tables(m))return 0;
    if(!game_menu_bindings_load(a) && (!game_menu_bindings_defaults(a) ||
        !game_menu_bindings_reverse(a) || !game_menu_bindings_labels(a,0,256u)))return 0;
    if(!game_menu_bindings_save(a))return 0;
    if(!game_menu_game_defaults(a) || !game_menu_curve_data_begin(a))return 0;
    game_memory_write_u32(m,0x5a1cd0u,(uint32_t)time(NULL));
    game_menu_curve_tables(m);game_menu_view_tables(m);
    game_memory_fill_u32(m,0x624f48u,3u,0,0);
    if(m->fault || !game_menu_sound_bank_begin(a) || !game_menu_sound_bank_load(a,0) ||
        !game_menu_reciprocal_table(m) || !render_work_begin(m))return 0;
    game_menu_character_tables(m); /* 00481db0 is an actual empty retail function. */
    if(!game_menu_scalar_tables(m))return 0;
    record=scene_projection_storage_allocate(0x410u,"portable display service table");
    if(!record)return 0;
    game_memory_write_u32(m,0x58fdf0u,record);
    game_initializer_00519ee0(m);
    if(m->fault || !world_buffers(m) || !game_menu_neighborhood_tables(m))return 0;
    game_memory_write_u8(m,0x5efec0u,0);
    record=scene_projection_storage_allocate(0x544u,"portable runtime work area");
    if(!record)return 0;
    game_memory_write_u32(m,0x628a88u,record);
    for(i=0;i<6u;++i) {
        record=scene_projection_storage_allocate(i<4u?0x608u:i==4u?0x2e4u:0x230u,
            "portable game manager");
        if(!record)return 0;
        if(!gameplay_manager_construct(m,record,i)) {scene_projection_storage_free(record);return 0;}
        game_memory_write_u32(m,0x5f0210u+i*4u,record);
    }
    return !m->fault;
}
static int free_publication(GameMemory *m,GameAddress slot) {
    GameAddress record=game_memory_read_u32(m,slot);
    if(m->fault || (record && !scene_projection_storage_free(record)))return 0;
    game_memory_write_u32(m,slot,0);return !m->fault;
}
/* 0041ae80: retire spatial work buffers in their original owner order. Paired
 * counts are reset only when their backing allocation existed. Producers retain
 * responsibility for allocating these records through the shared owner. */
static int spatial_work_end(GameMemory *m) {
    static const uint32_t first[]={0x583e2cu,0x583e24u,0x583e28u,0x583e30u};
    static const uint32_t last[]={0x583e7cu,0x583e8cu,0x583e94u,0x583e80u,0x583e88u};
    unsigned i;
    for(i=0;i<4u;++i)if(!free_publication(m,first[i]))return 0;
    for(i=0;i<5u;++i) {
        GameAddress slot=0x583e64u+i*4u,object=game_memory_read_u32(m,slot);
        if(m->fault)return 0;
        if(object) {
            if(!free_publication(m,slot))return 0;
            if(i<4u) {
                game_memory_write_u32(m,0x583e34u+i*4u,0u);
                game_memory_write_u32(m,0x583e44u+i*4u,0u);
            } else game_memory_write_u32(m,0x583e78u,0u);
        }
        if(i<4u && !free_publication(m,0x583e54u+i*4u))return 0;
    }
    for(i=0;i<5u;++i)if(!free_publication(m,last[i]))return 0;
    return !m->fault;
}
int game_level_spatial_work_end(GameApplication *a){return spatial_work_end(&a->memory);}
static int manager_storage_release(void *context,GameAddress object) {
    (void)context;return scene_projection_storage_free(object);
}
static int manager_model_release(void *context,GameAddress model) {
    return game_application_scene_model_release(context,model);
}
/* 00505d70, reached by 004900a0 after widget retirement. */
int game_menu_profiles_release(GameApplication *a) {
    GamePresentationStorageServices retirement={a,0,manager_storage_release,0,manager_model_release};
    GameMemory *m=&a->memory;unsigned i;
    game_application_ui_shutdown(a);
    if(!game_menu_game_defaults_end(a) || !game_menu_shutdown_systems(a) ||
        !game_menu_sound_bank_end(a))return 0;
    for(i=0;i<6u;++i) {
        GameAddress slot=0x5f0210u+i*4u,record=game_memory_read_u32(m,slot);
        if(record && !gameplay_manager_destroy(m,record,&retirement))return 0;
        if(!free_publication(m,slot))return 0;
    }
    /* 004fedb0 is an actual empty retail function. */
    render_arena_release();game_memory_fill_u32(m,0x597720u,8u,0,0);
    render_runtime_pools_release();
    for(i=0;i<RENDER_RUNTIME_POOL_COUNT;++i)
        game_memory_write_u32(m,gameplay_state_runtime_pool_spec(i).destination,0);
    if(!spatial_work_end(m) || !game_menu_curve_data_end(a) ||
        !free_publication(m,0x628a88u) || !free_publication(m,0x58fdf0u))return 0;
    for(i=0;i<4u;++i)if(!free_publication(m,gameplay_state_world_buffer_spec(i).destination))return 0;
    game_memory_write_u32(m,0x597068u,0); /* 00505590 */
    return !m->fault;
}
