#include "game_memory.h"
#include "game_text_rasterizer.h"
#include "host_file.h"
#include "player_attachment.h"
#include "game_initial_data.h"
#include "render_texture_cache.h"
#include "ui_geometry.h"
#include "asset_bitmap.h"
#include "game_matrix.h"
#include "game_string_search.h"
#include "resource_path.h"
#include "resource_tree.h"
#include "resource_payload.h"
#include "scene_child_registry.h"
#include "scene_model.h"
#include "scene_submission.h"
#include "mesh_buffer.h"
#include "projectile.h"
#include "level_objects.h"
#include "gameplay_root.h"
#include "terrain_asset.h"
#include "scene_instance_owner.h"
#include "application_service_roster.h"
#include "application_mode.h"
#include "application_frame.h"
#include "level_clock.h"
#include "application_shutdown.h"
#include "player_runtime_pool.h"
#include "event_queue.h"
#include "effect_resource_pool.h"
#include "effect_state.h"
#include "scene_projection_storage.h"
#include "player_events.h"
#include "gameplay_spatial.h"
#include "game_intrusive_list.h"
#include "projection_owner_pages.h"
#include "software_raster.h"
#include "render_packet_view.h"
#include "portable_crt.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "core check failed at line %d: %s\n", __LINE__, #condition); \
    return 1; } } while (0)

typedef struct Tail { unsigned calls; uint32_t address; uint8_t value; } Tail;
static int read_tail(void *context, GameAddress address, void *destination, size_t size) {
    Tail *tail = context;
    ++tail->calls;
    if (address != tail->address || size != 1) return 0;
    *(uint8_t *)destination = tail->value;
    return 1;
}

typedef struct ServiceVisitState {
    unsigned calls[4];
    int fail_second_once;
} ServiceVisitState;
static uint32_t next_service(void *context, uint32_t handle) {
    (void)context;
    return handle == 1u ? 2u : 0u;
}
static int destroy_service(void *context, uint32_t handle) {
    ServiceVisitState *state = context;
    ++state->calls[handle];
    if (handle == 2u && state->fail_second_once) {
        state->fail_second_once = 0;
        return 0;
    }
    return 1;
}

typedef struct EventReleaseState {
    GameAddress released[2];
    unsigned count;
} EventReleaseState;
typedef struct PartsReleaseState {
    unsigned children, auxiliary, projection, events, base;
    int fail_auxiliary_once;
} PartsReleaseState;
static int release_part(void *context, GameAddress child, unsigned part) {
    PartsReleaseState *state = context;
    if (part == PLAYER_RUNTIME_PARTS) {
        if (child != 0x1900u) return 0;
        if (state->fail_auxiliary_once) {
            state->fail_auxiliary_once = 0;
            return 0;
        }
        ++state->auxiliary;
    } else {
        if (part != 0u || child != 0x1800u) return 0;
        ++state->children;
    }
    return 1;
}
static int clear_parts_projection(void *context, GameAddress clone) {
    PartsReleaseState *state = context;
    if (clone != 0x1500u) return 0;
    ++state->projection;
    return 1;
}
static int clear_parts_events(void *context, GameAddress clone) {
    PartsReleaseState *state = context;
    if (clone != 0x1500u) return 0;
    ++state->events;
    return 1;
}
static int release_parts_base(void *context, GameAddress clone) {
    PartsReleaseState *state = context;
    if (clone != 0x1500u) return 0;
    ++state->base;
    return 1;
}
static int release_event(void *context, GameAddress node) {
    EventReleaseState *state = context;
    if (state->count >= 2u) return 0;
    state->released[state->count++] = node;
    return 1;
}

typedef struct ModeScenario {
    uint32_t fields[APPLICATION_MODE_SAVED_SCALE + 1u];
    ApplicationModeAction actions[16];
    unsigned count;
} ModeScenario;
static int mode_read(void *context, ApplicationModeField field,
        uint32_t *value) {
    ModeScenario *scenario = context;
    *value = scenario->fields[field];
    return 1;
}
static int mode_write(void *context, ApplicationModeField field,
        uint32_t value) {
    ModeScenario *scenario = context;
    scenario->fields[field] = value;
    return 1;
}
static int mode_action(void *context, ApplicationModeAction action) {
    ModeScenario *scenario = context;
    if (scenario->count == 16u) return 0;
    scenario->actions[scenario->count++] = action;
    return 1;
}

typedef struct FrameScenario {
    ApplicationFrameAction actions[40];
    uint32_t values[40], now[2], published, mode, sync_result;
    unsigned count, clock_calls;
} FrameScenario;
static int frame_now(void *context, uint32_t *now) {
    FrameScenario *scenario = context;
    if (scenario->clock_calls >= 2u) return 0;
    *now = scenario->now[scenario->clock_calls++];
    return 1;
}
static int frame_mode(void *context, uint32_t *mode) {
    *mode = ((FrameScenario *)context)->mode;
    return 1;
}
static int frame_action(void *context, ApplicationFrameAction action,
        uint32_t value, uint32_t *result) {
    FrameScenario *scenario = context;
    if (scenario->count >= 40u) return 0;
    scenario->actions[scenario->count] = action;
    scenario->values[scenario->count++] = value;
    if (result) *result = scenario->sync_result;
    return 1;
}
static int frame_publish(void *context, uint32_t tick) {
    ((FrameScenario *)context)->published = tick;
    return 1;
}

typedef struct ClockStopScenario {
    uint32_t times[4];
    unsigned samples, pending_calls, refreshes, releases, unpublished;
    int fail_unpublish_once;
} ClockStopScenario;
typedef struct ClockCreateScenario {
    unsigned allocations, initialized, configured, projected;
    uint32_t channel;
} ClockCreateScenario;
static int clock_allocate(void *context, uint32_t bytes,
        uint32_t *object) {
    ClockCreateScenario *scenario = context;
    if ((scenario->allocations == 0u && bytes != 0xa8u) ||
        (scenario->allocations == 1u && bytes != 0xa4u) ||
        scenario->allocations > 1u) return 0;
    *object = scenario->allocations++ == 0u ? 7u : 8u;
    return 1;
}
static int clock_initialize_pool(void *context, uint32_t pool) {
    ClockCreateScenario *scenario = context;
    if (pool != 8u || scenario->allocations != 2u) return 0;
    ++scenario->initialized;
    return 1;
}
static int clock_configure_pool(void *context, uint32_t pool,
        uint32_t count, uint32_t stride, uint32_t lanes,
        uint32_t capacity) {
    ClockCreateScenario *scenario = context;
    if (pool != 8u || count != 20u || stride != 4u ||
        lanes != 10u || capacity != 64u ||
        scenario->initialized != 1u) return 0;
    ++scenario->configured;
    return 1;
}
static int clock_project(void *context, uint32_t object,
        uint32_t pool, uint32_t channel) {
    ClockCreateScenario *scenario = context;
    if (object != 7u || pool != 8u || channel != 19u ||
        scenario->configured != 1u) return 0;
    scenario->channel = channel;
    ++scenario->projected;
    return 1;
}
static int clock_sample(void *context, uint32_t *now) {
    ClockStopScenario *scenario = context;
    if (scenario->samples >= 4u) return 0;
    *now = scenario->times[scenario->samples++];
    return 1;
}
static int clock_pending(void *context, uint32_t object,
        uint32_t *pending) {
    ClockStopScenario *scenario = context;
    if (object != 7u) return 0;
    *pending = ++scenario->pending_calls == 3u;
    return 1;
}
static int clock_refresh(void *context, uint32_t object) {
    ClockStopScenario *scenario = context;
    if (object != 7u) return 0;
    ++scenario->refreshes;
    return 1;
}
static int clock_release(void *context, uint32_t object) {
    ClockStopScenario *scenario = context;
    if (object != 7u) return 0;
    ++scenario->releases;
    return 1;
}
static int clock_unpublish(void *context) {
    ClockStopScenario *scenario = context;
    ++scenario->unpublished;
    if (scenario->fail_unpublish_once) {
        scenario->fail_unpublish_once = 0;
        return 0;
    }
    return 1;
}

typedef struct ShutdownScenario {
    unsigned calls[APPLICATION_SHUTDOWN_DONE];
    unsigned pools[5], services[12];
    int fail_pool_two_once;
} ShutdownScenario;
static int shutdown_step(void *context, ApplicationShutdownStep step,
        unsigned index, uint32_t players[6], uint8_t *mask) {
    ShutdownScenario *scenario = context;
    ++scenario->calls[step];
    if (step == APPLICATION_SHUTDOWN_RELEASE_LEVEL)
        players[0] = 42u;
    if (step == APPLICATION_SHUTDOWN_RELEASE_POOL) {
        ++scenario->pools[index];
        if (index == 2u && scenario->fail_pool_two_once) {
            scenario->fail_pool_two_once = 0;
            return 0;
        }
    }
    if (step == APPLICATION_SHUTDOWN_NOTIFY_PLAYER && index == 0u) {
        if (players[0] != 42u) return 0;
        *mask = 1u;
    }
    if (step == APPLICATION_SHUTDOWN_RELEASE_SERVICE)
        ++scenario->services[index];
    return 1;
}

static int resource_projection_read(void *context,GameAddress address,void *bytes,size_t size) {
    (void)context;return scene_projection_storage_read(address,bytes,size) || render_arena_read(address,bytes,size);
}
static int resource_projection_write(void *context,GameAddress address,const void *bytes,size_t size) {
    (void)context;return scene_projection_storage_write(address,bytes,size) || render_arena_write(address,bytes,size);
}
typedef struct RenderCacheProbe {
    TextureRecord records[40];unsigned uploads,releases,resets;int reject;
} RenderCacheProbe;
static int cache_stats(void *context,uint32_t out[5]){(void)context;memset(out,0,20);return 1;}
static int cache_upload(void *context,uint32_t key,const uint8_t format[4],GameAddress data,uint32_t bytes){
    RenderCacheProbe *p=context;(void)key;(void)format;(void)data;
    if(!bytes || p->reject)return 0;++p->uploads;return 1;
}
static TextureRecord *cache_record(void *context,uint32_t id){
    RenderCacheProbe *p=context;unsigned i;
    for(i=0;i<40;i++)if(p->records[i].id==id)return p->records+i;
    return NULL;
}
static int cache_owned(void *context,uint32_t key,const AssetTexture *image){
    RenderCacheProbe *p=context;(void)key;
    if(!image->pixels || !image->size || p->reject)return 0;++p->uploads;return 1;
}
static void cache_release(void *context,uint32_t key){RenderCacheProbe *p=context;(void)key;++p->releases;}
static void cache_reset(void *context){++((RenderCacheProbe *)context)->resets;}
static int check_render_texture_lifetime(void){
    RenderCacheProbe probe={0};GameMemory memory;uint8_t pixel[8]={0},format[4]={8,8,3,5};
    uint8_t *bytes=calloc(1,0x280000);GameAddress streamed;unsigned i;
    GameRenderTextureServices services={&probe,cache_stats,cache_upload,cache_record,cache_owned,cache_release,cache_reset};
    CHECK(bytes);game_memory_initialize(&memory,bytes,0x400000,0x280000);
    memory.read_fallback=resource_projection_read;memory.write_fallback=resource_projection_write;
    CHECK(game_render_texture_cache_begin(&memory,&services));
    /* More than the retired 2MiB budget remains resident without ring eviction. */
    for(i=0;i<40;i++){
        TextureRecord *t=probe.records+i;t->id=0xfd000001u+i;t->image.pixels=pixel;t->image.size=131072;
        CHECK(game_render_texture_use(&memory,t->id,&services));CHECK(t->address==t->id);
    }
    CHECK(probe.uploads==40);
    for(i=0;i<40;i++)CHECK(game_render_texture_use(&memory,probe.records[i].id,&services));
    CHECK(probe.uploads==40);
    CHECK(game_render_texture_forget(&memory,probe.records[0].id));
    CHECK(probe.releases==1 && !probe.records[0].address && !(probe.records[0].flags&0x80000000u));
    probe.reject=1;CHECK(!game_render_texture_use(&memory,probe.records[0].id,&services));
    CHECK(!(probe.records[0].flags&0x80000000u));probe.reject=0;
    CHECK(game_render_texture_use(&memory,probe.records[0].id,&services));CHECK(probe.uploads==41);
    streamed=game_render_texture_create(&memory,0,format,pixel,sizeof(pixel));CHECK(streamed);
    CHECK(game_render_texture_use(&memory,streamed,&services));CHECK(probe.uploads==42);
    CHECK(game_memory_read_u32(&memory,streamed+8)==streamed);
    CHECK(game_render_texture_forget(&memory,streamed));
    CHECK(game_render_texture_use(&memory,streamed,&services));CHECK(probe.uploads==43);
    CHECK(game_render_texture_cache_frame(&memory,&services));
    CHECK(game_render_texture_cache_end(&memory));CHECK(probe.resets==1);
    CHECK(game_render_texture_cache_end(&memory));CHECK(probe.resets==1);
    CHECK(game_render_texture_cache_begin(&memory,&services));
    CHECK(game_render_texture_cache_end(&memory));CHECK(probe.resets==2 && !memory.fault);
    free(bytes);return 0;
}
/* Chain ordering, retained words and mixed kind accounting were absent from
 * the prior core fixtures. No CPU or guest execution supplies these records. */
static int check_resource_lifetime(void) {
    unsigned family;
    for(family=0u;family<2u;family++){
        uint8_t header[144]={0u};GameMemory memory;EffectResourcePool *pool;
        GameAddress owner=family?0x005ad7d0u:0x00631910u,a,b,c,address;
        EffectResource *first,*second;uint32_t words[17]={0u},expected[17];unsigned i;
        GameStateRecordConfig config={7u,11u,13u,-17,19u,23u,29u,31u,37u,41u,43u,47u,-53,59u};
        game_memory_initialize(&memory,header,owner,sizeof(header));
        memory.read_fallback=resource_projection_read;memory.write_fallback=resource_projection_write;
        CHECK(effect_resource_pool_begin(&memory,owner,0u,family?5u:0u,3u));
        pool=effect_resource_pool_find(owner);CHECK(pool);
        a=effect_resource_pool_acquire_kind(&memory,pool,2u);
        b=effect_resource_pool_acquire_kind(&memory,pool,4u);CHECK(a && b && pool->head);
        c=pool->free_head;first=effect_resource_find(a);second=effect_resource_find(b);CHECK(first && second);
        first->next=second;
        if(family){
            CHECK(effect_resource_configure(first,&config));
            first->reserved_type[0]=0xa5u;first->reserved_ninth=0xbeefu;first->reserved_twelfth=0xcafeu;
        }else {first->contact=0x1234u;first->contact_vector[0]=-17.0f;}
        CHECK(scene_projection_storage_read(a,words,family?68u:28u));
        memcpy(expected,words,sizeof(words));
        if(family){
            ((uint8_t *)expected)[12]=0u;
            for(i=4u;i<=10u;i++)expected[i]=0u;
            expected[11]&=0xffff0000u;expected[12]=expected[13]=0u;
            expected[14]=(expected[14]&0xffff0000u)|1u;expected[15]=0u;
        }else for(i=3u;i<7u;i++)expected[i]=0u;
        CHECK(effect_resource_pool_return_chain(&memory,pool,a)==1);
        CHECK(pool->head==first && first->next==second && second->next==effect_resource_find(c));
        CHECK(pool->live_by_kind[2]==0u && pool->live_by_kind[4]==0u);
        CHECK(scene_projection_storage_read(a,words,family?68u:28u));
        CHECK(memcmp(words,expected,family?68u:28u)==0);
        CHECK(effect_resource_pool_acquire_kind(&memory,pool,9u)==a);
        CHECK(effect_resource_pool_return(&memory,pool,a));
        while((address=effect_resource_pool_take_free(&memory,pool))!=0u)CHECK(scene_projection_storage_free(address));
        CHECK(!memory.fault);effect_resource_pool_end(pool);
        CHECK(!effect_resource_find(a));
    }
    return 0;
}

/* A released head must not release its queued successor. Trimming deliberately
 * retains the historical per-kind counters; these are not live-object counts. */
static int check_event_lifetime(void){
    static const GameAddress owners[]={0x005cee40u,0x005cef90u,0x005ceee8u};
    unsigned family;
    for(family=0u;family<3u;family++){
        uint8_t header[164]={0u};GameMemory memory;EffectResourcePool *pool;
        EffectResource *first,*second;GameAddress a,b,address;float expected=2.9f;
        game_memory_initialize(&memory,header,owners[family],sizeof(header));
        memory.read_fallback=resource_projection_read;memory.write_fallback=resource_projection_write;
        CHECK(effect_resource_pool_initialize_events(&memory,owners[family],3u,(uint16_t)(family+1u),3u));
        pool=effect_resource_pool_find(owners[family]);CHECK(pool);
        a=effect_resource_pool_acquire_kind(&memory,pool,2u);b=effect_resource_pool_acquire_kind(&memory,pool,4u);
        first=effect_resource_find(a);second=effect_resource_find(b);CHECK(first && second);
        CHECK(first->event_route==family && pool->free_count==1u && !pool->release_sampled);
        first->next=second;first->deadline=99u;first->event_code=20u;
        first->event_padding[0]=0xa5u;first->packet_padding=0xb6u;first->packet_tail=0xc7d8u;
        first->event_arguments[0]=123u;first->packet_data[0]=456u;first->packet_mask=0xffu;
        CHECK(effect_resource_pool_return(&memory,pool,a));
        CHECK(!second->in_pool && pool->live_by_kind[4]==1u && pool->free_count==2u);
        CHECK(memcmp(&pool->average_used,&expected,4u)==0 && pool->last_used==2u);
        CHECK(first->deadline==0u && first->event_code==0u && first->event_padding[0]==0xa5u);
        if(family==1u)CHECK(first->event_arguments[0]==0u);
        if(family==2u)CHECK(first->packet_data[0]==0u && first->packet_mask==0u &&
            first->packet_padding==0xb6u && first->packet_tail==0xc7d8u);
        CHECK(effect_resource_pool_return(&memory,pool,b));CHECK(pool->free_count==3u);
        a=effect_resource_pool_acquire_kind(&memory,pool,7u);CHECK(a);
        pool->average_used=0.0f;pool->release_sampled=1u;
        CHECK(effect_resource_pool_return(&memory,pool,a));
        CHECK(!effect_resource_find(a) && pool->initial_count==2u && pool->free_count==2u && pool->live_by_kind[7]==1u);
        while((address=effect_resource_pool_take_free(&memory,pool))!=0u)CHECK(scene_projection_storage_free(address));
        CHECK(!memory.fault);effect_resource_pool_end(pool);
    }
    return 0;
}

static int check_event_cross_family(void){
    uint8_t headers[500]={0u};GameMemory memory;
    EffectResourcePool *plain,*packet;EffectResource *first,*second;GameAddress a,b,address;
    game_memory_initialize(&memory,headers,0x005cee40u,sizeof(headers));
    memory.read_fallback=resource_projection_read;memory.write_fallback=resource_projection_write;
    CHECK(effect_resource_pool_initialize_events(&memory,0x005cee40u,1u,1u,1u));
    CHECK(effect_resource_pool_initialize_events(&memory,0x005ceee8u,1u,3u,1u));
    plain=effect_resource_pool_find(0x005cee40u);packet=effect_resource_pool_find(0x005ceee8u);
    a=effect_resource_pool_acquire_kind(&memory,plain,2u);b=effect_resource_pool_acquire_kind(&memory,packet,4u);
    first=effect_resource_find(a);second=effect_resource_find(b);CHECK(first && second);
    game_memory_write_u32(&memory,a+4u,b);
    CHECK(!memory.fault && first->next==second);
    CHECK(effect_resource_pool_return(&memory,plain,a));
    CHECK(!second->in_pool && packet->live_by_kind[4]==1u && packet->free_count==0u);
    CHECK(effect_resource_pool_return(&memory,packet,b));
    address=effect_resource_pool_take_free(&memory,plain);CHECK(address==a && scene_projection_storage_free(address));
    address=effect_resource_pool_take_free(&memory,packet);CHECK(address==b && scene_projection_storage_free(address));
    effect_resource_pool_end(plain);effect_resource_pool_end(packet);return 0;
}

typedef struct EventProbe { unsigned spatial_calls,refresh_calls,refresh_mask; } EventProbe;
static int event_spatial(void *context,unsigned operation,int *alive){
    EventProbe *probe=context;++probe->spatial_calls;*alive=1;return operation<3u;
}
static int event_refresh(void *context,unsigned method){
    EventProbe *probe=context;if(method>=5u)return 0;
    ++probe->refresh_calls;probe->refresh_mask|=1u<<method;return 1;
}
static int event_catalog(void *context,unsigned method,unsigned item,unsigned variant,int32_t *value){
    (void)context;
    if(!method && item==5u)*value=100;
    else if(!method && item==1u)*value=400;
    else if(method==1u && item==2u)*value=variant?200:300;
    else return 0;
    return 1;
}
static int check_player_events(void){
    uint8_t *bytes=calloc(0x640000u,1u);GameMemory memory;
    PlayerControls controls;PresentationPose pose;EffectResource event;
    EventProbe probe={0,0,0};PlayerEventServices ports={&probe,NULL,NULL,event_refresh,event_spatial,NULL};
    unsigned i;uint32_t value=0;
    CHECK(bytes);game_memory_initialize(&memory,bytes,0u,0x640000u);
    memset(&controls,0,sizeof(controls));memset(&pose,0,sizeof(pose));memset(&event,0,sizeof(event));
    pose.properties[0]=100u;
    for(i=0;i<15u;++i){controls.weapons[i]=0x2000u+i*0x100u;
        game_memory_write_u32(&memory,controls.weapons[i]+20u,0x4000u);
        game_memory_write_u32(&memory,controls.weapons[i]+12u,i==2u?0xffffffffu:i==4u?2u:0u);
    }
    controls.selected[0]=controls.selected[1]=controls.weapons[0];
    event.event_code=7u;CHECK(player_event_apply(&memory,0x1000u,&pose,&controls,&event,&ports));
    CHECK(controls.selection[1]==4u && controls.selected[1]==controls.weapons[4]);
    event.event_code=9u;event.event_arguments[0]=2u;
    CHECK(player_event_apply(&memory,0x1000u,&pose,&controls,&event,&ports));
    CHECK(controls.selection[1]==2u); /* Direct choice accepts negative/infinite ammunition. */
    event.event_code=8u;CHECK(player_event_apply(&memory,0x1000u,&pose,&controls,&event,&ports));
    CHECK(controls.selection[1]==0u); /* Cycling skips nonpositive ammunition. */
    game_memory_write_u32(&memory,controls.selected[0]+8u,77u);
    game_memory_write_u32(&memory,controls.selected[0]+24u,123u);
    controls.input_flags=0x30000u;event.event_code=4u;
    CHECK(player_event_apply(&memory,0x1000u,&pose,&controls,&event,&ports));
    CHECK(controls.input_flags==0x20000u);
    CHECK(game_memory_read_u32(&memory,controls.selected[0]+8u)==77u);
    CHECK(game_memory_read_u32(&memory,controls.selected[0]+24u)==0u);
    game_memory_write_u32(&memory,0x5ac6acu,1234u);
    /*Original4f20b6 refreshes packet history only for a network session. */
    game_memory_write_u32(&memory,0x5eff04u,1u);
    event.event_code=1u;event.packet_mask=255u;
    for(i=0;i<8u;++i)event.packet_data[i]=(uint16_t)(10u+i);
    event.packet_data[1]=0xfff0u;
    CHECK(player_event_apply(&memory,0x1000u,&pose,&controls,&event,&ports));
    CHECK(pose.position[0]==20.0f && pose.position[1]==-16.0f && pose.position[2]==24.0f);
    CHECK(pose.local_angles[0]==15u && pose.local_angles[1]==14u && pose.local_angles[2]==13u);
    CHECK(controls.heading==16u && controls.previous_heading==16u && controls.aim==17u && controls.previous_aim==17u);
    CHECK(controls.last_packet_tick==1234u && probe.spatial_calls==3u &&
        probe.refresh_calls==2u && probe.refresh_mask==((1u<<3)|(1u<<4)));
    game_memory_write_u32(&memory,0x5eff04u,0u);event.packet_mask=0u;
    CHECK(player_event_apply(&memory,0x1000u,&pose,&controls,&event,&ports));
    CHECK(controls.last_packet_tick==1234u && probe.spatial_calls==6u &&
        probe.refresh_calls==2u && probe.refresh_mask==((1u<<3)|(1u<<4)));
    /* Scaling changes controller clearance at 0x128, not the neighboring
     * pose property at 0x60. Preserve the special-mode store/FTOL boundaries. */
    ports.catalog=event_catalog;
    game_memory_write_u32(&memory,0x58973cu,0x5000u);
    game_memory_write_u32(&memory,0x5000u+0x470u,0x6000u);
    game_memory_write_u32(&memory,0x574d3cu,0x3f800000u);
    game_memory_write_u32(&memory,0x574d40u,0xbf000000u);
    game_memory_write_u32(&memory,0x574d44u,0x3e800000u);
    game_memory_write_u32(&memory,0x574d48u,0x3f000000u);
    game_memory_write_u32(&memory,0x574d58u,0x3e800000u);
    game_memory_write_u32(&memory,0x574d5cu,0x3f000000u);
    game_memory_write_u32(&memory,0x574d60u,0x3f000000u);
    pose.properties[2]=0xdeadbeefu;event.event_code=13u;
    event.event_arguments[0]=0u;event.event_arguments[1]=0x3f800000u;
    CHECK(player_event_apply(&memory,0x1000u,&pose,&controls,&event,&ports));
    CHECK(game_memory_read_u32(&memory,0x1128u)==0xc2480000u && pose.properties[2]==0xdeadbeefu);
    CHECK(game_memory_read_u32(&memory,0x6020u)==150u && game_memory_read_u32(&memory,0x6014u)==300u && pose.properties[1]==600u);
    event.event_arguments[0]=0x3f000000u;event.event_arguments[1]=0x3e800000u;
    CHECK(player_event_apply(&memory,0x1000u,&pose,&controls,&event,&ports));
    CHECK(game_memory_read_u32(&memory,0x1128u)==0x42d48000u && pose.properties[2]==0xdeadbeefu);
    CHECK(game_memory_read_u32(&memory,0x6020u)==175u && game_memory_read_u32(&memory,0x6014u)==337u && pose.properties[1]==500u);
    value=0x12345678u;CHECK(player_controls_projection_write(&controls,0x140u,&value,4u));
    value=0;CHECK(player_controls_projection_read(&controls,0x140u,&value,4u) && value==0x12345678u);
    event.event_code=18u;CHECK(player_event_apply(&memory,0x1000u,&pose,&controls,&event,&ports));
    CHECK(!controls.input_flags && !controls.previous_input_flags && !controls.input_axes[0] && !controls.input_axes[1]);
    CHECK(!memory.fault);free(bytes);return 0;
}

static int spatial_read(void *context,GameAddress address,void *output,size_t size){
    (void)context;return gameplay_spatial_projection_read(address,output,size);
}
static int spatial_write(void *context,GameAddress address,const void *input,size_t size){
    (void)context;return gameplay_spatial_projection_write(address,input,size);
}
/* Regression: owned graph submission must accept bounded legacy frame IDs,
 * without rejecting the native graph or using a CPU/register call port. */
static int test_owned_submission(void){
    AssetMesh asset={0};SceneMesh mesh={0};SceneMeshProjection projection={0};
    SceneMeshProjectionPages pages={0};SceneInstance *instance;
    MaterialValue material={32u,32u,0u};uint32_t material_handle=1u,head=0u,holder;
    uint8_t flags=1u,bytes[4096]={0};GameMemory memory;
    GameplayClipCursors cursors={0x1000u,0x1100u,0x1200u,0x1300u};
    SceneSubmissionState state={0};
    asset.material_count=1u;asset.vertex_count=3u;asset.triangle_count=1u;
    asset.materials=calloc(1u,sizeof(*asset.materials));
    asset.vertices=calloc(3u,sizeof(*asset.vertices));
    asset.triangles=calloc(1u,sizeof(*asset.triangles));
    CHECK(asset.materials && asset.vertices && asset.triangles);
    asset.materials[0]=malloc(2u);CHECK(asset.materials[0]);memcpy(asset.materials[0],"x",2u);
    asset.triangles[0].vertices[1]=1u;asset.triangles[0].vertices[2]=2u;
    CHECK(scene_mesh_initialize(&mesh,&asset));
    holder=scene_projection_storage_create_mesh(&mesh,"x",2u,&material_handle,&material,&flags,1u,&pages);
    CHECK(holder);
    projection.holder=holder;projection.descriptor=pages.descriptor;
    projection.source=pages.source;projection.transformed=pages.transformed;
    projection.polygons=pages.polygons;projection.links=pages.links;
    projection.uv=pages.uv;projection.uv_work=pages.uv_work;
    CHECK(scene_instance_owner_register_source(&projection,&mesh,NULL,0u));
    instance=scene_instance_owner_find(holder);CHECK(instance);
    instance->flags=1u;instance->local[11]=-100.0f;
    game_memory_initialize(&memory,bytes,0x1000u,sizeof(bytes));
    state.frame.cursors=&cursors;state.frame.queue_head_value=&head;
    state.camera[0]=state.camera[5]=state.camera[10]=1.0f;
    state.far_depth=1000.0f;state.frame.scale=1.0f;state.frame.vertical=1.0f;
    state.frame.numerator=1.0f;state.frame.bounds[0]=state.frame.bounds[1]=-100.0f;
    state.frame.bounds[2]=state.frame.bounds[3]=100.0f;
    instance->extension=1u;
    CHECK(scene_submission_owned(&memory,holder,&state)==0 && state.reject==3u &&
        !memory.fault && !head && cursors.vertex==0x1000u);
    instance->extension=0u;
    CHECK(scene_submission_owned(&memory,holder,&state)==1 && !memory.fault);
    CHECK(state.nodes==1u && state.submitted==1u && state.render_nodes==1u &&
        state.depth==0u && state.dirty[0] && state.dirty[1] && state.matrices[1][11]==-100.0f);
    CHECK(!head && cursors.vertex==0x1000u && cursors.packet==0x1100u);
    {
        uint32_t visible=0;uint8_t palette[4]={0,1,2,3};GameMemory palette_memory;
        RenderArenaPacket *packet;
        CHECK(render_arena_initialize());memset(&cursors,0,sizeof(cursors));
        {
            /*Final-system camera travel screen-clips polygons whose index
             *lists occupy the shared packet bank, not the typed index bank. */
            const uint8_t words[12]={0x10,0,0,0xf9,0x20,0,0,0xf9,0x30,0,0,0xf9};
            uint32_t decoded[32]={0},ref=render_arena_view(3)+88;const uint32_t *indices;
            CHECK(render_arena_write(ref,words,sizeof(words)));
            indices=render_packet_indices(ref,3,decoded);
            CHECK(indices==decoded && indices[0]==render_arena_view(0)+16 &&
                indices[1]==render_arena_view(0)+32 && indices[2]==render_arena_view(0)+48);
            CHECK(!render_packet_indices(ref+1,3,decoded) && !render_packet_indices(ref,33,decoded));
            CHECK(!render_packet_indices(render_arena_view(3)+600000-4,2,decoded));
            indices=render_arena_indices(render_arena_view(1),3);CHECK(indices);
            CHECK(render_packet_indices(render_arena_view(1),3,decoded)==indices);
        }
        game_memory_initialize(&palette_memory,palette,0x604380u,sizeof(palette));state.palette_memory=&palette_memory;
        state.visible_queue=&visible;state.visible_numerator=1.0f;state.visible_offset=0;
        instance->extension=1u;instance->flags=0u;
        mesh.vertices[0].z_bits=mesh.vertices[1].z_bits=mesh.vertices[2].z_bits=0x42f00000u;
        mesh.vertices[1].x_bits=0x41200000u;mesh.vertices[2].y_bits=0x41200000u;
        CHECK(scene_submission_owned(&memory,holder,&state)==1 && !memory.fault && !palette_memory.fault);
        CHECK(!visible && !head); /*The first winding is correctly culled. */
        mesh.vertices[2].y_bits=0xc1200000u;
        CHECK(scene_submission_owned(&memory,holder,&state)==1 && !memory.fault && !palette_memory.fault);
        packet=scene_projection_storage_packet(visible);if(!packet)packet=render_arena_packets(visible,1);
        if(!visible){RenderArenaVertex *v=scene_projection_storage_transformed(pages.transformed,3);
            fprintf(stderr,"visible fixture empty: flags=%x xyz=(%g,%g,%g)/(%g,%g,%g)/(%g,%g,%g)\n",
                scene_projection_storage_packet(pages.polygons)->flags,
                v[0].position[0],v[0].position[1],v[0].position[2],v[1].position[0],v[1].position[1],v[1].position[2],v[2].position[0],v[2].position[1],v[2].position[2]);}
        CHECK(visible && packet && packet->flags==0x119u && packet->count==3u && !head);
        CHECK(state.submitted==1u && state.render_nodes==1u && state.depth==0u);
        {
            /* Live movement exposed a previous near clip's four-corner output
             * left in the instance packet. This output is not source geometry. */
            RenderArenaPacket *derived=scene_projection_storage_packet(pages.polygons);
            CHECK(derived && mesh.polygon_count==1u);
            derived->count=4u;derived->vertices=render_arena_view(1u);
            visible=0;
            CHECK(scene_submission_owned(&memory,holder,&state)==1 && !memory.fault);
            CHECK(derived->count==3u && derived->vertices==pages.links && visible);
            CHECK(state.submitted==1u && state.depth==0u);
        }
    }
    {
        /*Attachment retirement can leave an owned transform-only holder.
         *Keep its matrix walk, emit no packets, and still reject missing roots. */
        uint32_t saved_descriptor=instance->descriptor,saved_head=head;
        GameplayClipCursors saved_cursors=cursors;
        instance->descriptor=0u;
        CHECK(scene_submission_owned(&memory,holder,&state)==1 && !memory.fault);
        CHECK(state.nodes==1u && !state.submitted && !state.render_nodes &&
            state.depth==0u && state.dirty[1] && state.matrices[1][11]==-100.0f);
        CHECK(head==saved_head && !memcmp(&cursors,&saved_cursors,sizeof(cursors)));
        CHECK(scene_submission_owned(&memory,0u,&state)==0 && state.reject==2u && !memory.fault);
        instance->descriptor=saved_descriptor;
    }
    {
        const MeshBufferServices *services=mesh_buffer_owned_services();
        GameAddress clone=services->allocate(services->context,0x64),container,packet,work,uv;
        memory.read_fallback=resource_projection_read;memory.write_fallback=resource_projection_write;
        CHECK(clone && mesh_buffer_construct(&memory,clone,3,1,0x1f00,0x1f00,services));
        CHECK(mesh_buffer_bind(&memory,clone,pages.descriptor));
        {
            GameAddress source=game_memory_read_u32(&memory,pages.descriptor);unsigned v;
            for(v=0;v<3;v++){
                game_memory_write_u32(&memory,source+v*12,v==1?0x41200000:0);
                game_memory_write_u32(&memory,source+v*12+4,v==2?0xc1200000:0);
                game_memory_write_u32(&memory,source+v*12+8,0x42f00000);
            }
        }
        game_memory_write_u8(&memory,clone+0x3c,1);
        container=game_memory_read_u32(&memory,clone+0x50);
        packet=game_memory_read_u32(&memory,container+0x18);
        work=game_memory_read_u32(&memory,container+12);
        uv=game_memory_read_u32(&memory,container+8);
        head=0;
        {
            GameAddress source=game_memory_read_u32(&memory,container);
            game_memory_write_u32(&memory,source+8,0x40a00000); /* Near plane cuts one corner. */
            memset(&cursors,0,sizeof(cursors));
            CHECK(scene_submission_owned(&memory,clone,&state)==1 && !memory.fault);
            CHECK(head && game_memory_read_u32(&memory,packet)==4);
            game_memory_write_u32(&memory,source+8,0x42f00000);
            head=0;memset(&cursors,0,sizeof(cursors));
            CHECK(scene_submission_owned(&memory,clone,&state)==1 && !memory.fault);
            CHECK(head && game_memory_read_u32(&memory,packet)==3);
        }
        /* A clipped-out previous frame must not erase a mutable triangle.
         * Poison the derived packet and UVs, leaving canonical inputs intact. */
        game_memory_write_u32(&memory,packet,0);
        game_memory_write_u32(&memory,packet+4,0);
        game_memory_write_u32(&memory,packet+16,0);
        game_memory_write_u32(&memory,work,0x7fc00000);
        memset(&cursors,0,sizeof(cursors));
        CHECK(scene_submission_owned(&memory,clone,&state)==1 && !memory.fault);
        CHECK(head && game_memory_read_u32(&memory,packet)==3);
        CHECK(game_memory_read_u32(&memory,packet+4)==game_memory_read_u32(&memory,container+0x10));
        CHECK(game_memory_read_u32(&memory,work)==game_memory_read_u32(&memory,uv));
        CHECK(mesh_buffer_release(&memory,clone,services) && services->release(services->context,clone));
    }
    CHECK(scene_instance_owner_release_graph(holder)==1);
    scene_mesh_release(&mesh);asset_mesh_release(&asset);
    return 0;
}
typedef struct ProducerProbe {unsigned probes,recycled,deployed;int clear,error;} ProducerProbe;
static int producer_base(void *c,GameAddress o){(void)c;(void)o;return 1;}
static GameAddress producer_acquire(void *c,GameAddress o){(void)c;(void)o;return 0x501000;}
static int producer_clear(void *c,GameAddress o,const float p[3],int *clear){
    ProducerProbe *probe=c;(void)o;(void)p;++probe->probes;*clear=probe->clear;return !probe->error;
}
static int producer_deploy(void *c,GameAddress root,GameAddress o,const float p[3]){
    ProducerProbe *probe=c;(void)root;(void)o;(void)p;++probe->deployed;return 1;
}
static int producer_recycle(void *c,GameAddress o){ProducerProbe *probe=c;(void)o;++probe->recycled;return 1;}
typedef struct HealthDropProbe {unsigned calls;int accepted,ok;} HealthDropProbe;
static int health_drop_request(void *context,GameAddress parent,GameAddress motion,int *accepted){
    HealthDropProbe *probe=context;++probe->calls;
    if(parent!=0x1234u || motion!=0x5678u)return 0;
    *accepted=probe->accepted;return probe->ok;
}
static int check_sparse_text_lock(void){
    GameMemory memory;GameTextTarget target={0};uint32_t range[2]={UINT32_MAX,0};
    uint8_t *state=calloc(1,0x240000);uint8_t pixels[32*16*2],reference[sizeof(pixels)];
    unsigned i,pass;
    CHECK(state);game_memory_initialize(&memory,state,0x400000,0x240000);
    /* A synthetic five-row glyph with clipping, background and two writes in
     * one borrowed lock. Tracking must change neither pixels nor text state. */
    for(i=0;i<25;i++)game_memory_write_u8(&memory,0x575c40+i,(uint8_t)(i%2));
    game_memory_write_u8(&memory,0x59d448,1);
    game_memory_write_u8(&memory,0x5ad3a8+6,255);
    game_memory_write_u32(&memory,0x59d46c,1);
    game_memory_write_u32(&memory,0x59d450,3);game_memory_write_u32(&memory,0x59d454,19);
    game_memory_write_u32(&memory,0x59d458,2);game_memory_write_u32(&memory,0x59d45c,12);
    target.pixels=pixels;target.pixel_bytes=sizeof(pixels);target.stride=32;target.rgb565=1;
    for(pass=0;pass<2;pass++){
        for(i=0;i<sizeof(pixels);i+=2){pixels[i]=0x1f;pixels[i+1]=0xf8;}
        target.written_range=pass?range:NULL;
        game_memory_write_u32(&memory,0x62970c,1);game_memory_write_u32(&memory,0x629708,1);
        game_text_rasterize_bytes(&memory,(const uint8_t *)"AA",3,&target,20);
        game_memory_write_u32(&memory,0x62970c,12);game_memory_write_u32(&memory,0x629708,8);
        game_text_rasterize_bytes(&memory,(const uint8_t *)"A",2,&target,20);
        CHECK(!memory.fault);
        if(!pass)memcpy(reference,pixels,sizeof(pixels));
    }
    CHECK(!memcmp(reference,pixels,sizeof(pixels)) && range[0]<range[1] && range[1]<=32*16);
    for(i=0;i<32*16;i++)if(pixels[i*2]!=0x1f || pixels[i*2+1]!=0xf8)CHECK(i>=range[0] && i<range[1]);
    range[0]=UINT32_MAX;range[1]=0;
    game_text_rasterize_bytes(&memory,(const uint8_t *)"",1,&target,20);
    CHECK(!memory.fault && range[0]==UINT32_MAX && range[1]==0);
    target.pixel_bytes=1;
    game_text_rasterize_bytes(&memory,(const uint8_t *)"A",2,&target,20);
    CHECK(memory.fault && range[0]==UINT32_MAX && range[1]==0);
    free(state);return 0;
}
int main(void) {
    CHECK(check_sparse_text_lock()==0);
    {
        PlayerAttachmentState state={0};PlayerAttachmentFrame frame={0};GameMemory memory={0};
        HealthDropProbe probe={0,0,1};PlayerAttachmentServices services={0};
        state.kind=PLAYER_SCENE_COMMAND;state.parent=0x1234;state.motion=0x5678;state.quantity=2;
        services.context=&probe;services.command=health_drop_request;
        CHECK(player_attachment_update(&memory,&state,&frame,&services)==1);
        CHECK(probe.calls==1 && state.quantity==2 && !state.stream_cookie);
        probe.ok=0;CHECK(player_attachment_update(&memory,&state,&frame,&services)==-1);
        CHECK(state.quantity==2 && !state.stream_cookie);
        probe.ok=probe.accepted=1;
        CHECK(player_attachment_update(&memory,&state,&frame,&services)==3);
        CHECK(state.quantity==1 && state.stream_cookie==1);
        CHECK(player_attachment_update(&memory,&state,&frame,&services)==1 && probe.calls==3);
        state.stream_cookie=0; /*The existing button-release consumer clears this. */
        CHECK(player_attachment_update(&memory,&state,&frame,&services)==3 && state.quantity==0);
        state.stream_cookie=0;
        CHECK(player_attachment_update(&memory,&state,&frame,&services)==0 && probe.calls==4);
    }
    {
        FILE *file=fopen("HostFileCase.tmp","wb");char path[128];
        CHECK(file!=NULL);CHECK(fputs("case",file)>=0);CHECK(!fclose(file));
        file=host_file_open(".\\hostfilecase.tmp","rb");
        CHECK(file!=NULL);CHECK(fgetc(file)=='c');CHECK(!fclose(file));
        CHECK(host_file_path(".\\hostfilecase.tmp",path,sizeof(path)));
        CHECK(!strchr(path,'\\'));CHECK(!remove(path));
        CHECK(!host_file_path("too-long",path,2));
    }
    {
        uint8_t source[8]={1,0,3,0xaa,4,5,6,0xbb},destination[24],overlap[6]={1,2,3,4,5,6};
        GameMenuPixelSpan from={source,sizeof(source),3,2,4},to={destination,sizeof(destination),4,4,6};
        GameMenuPixelSpan same={overlap,sizeof(overlap),6,1,6};
        GameUiRect full={0,0,3,2},clipped={-2,0,4,4},mirrored={0,0,3,2};unsigned i;
        memset(destination,0xcc,sizeof(destination));
        CHECK(game_ui_indexed_copy(&to,&from,&full,&clipped,1,0,0));
        CHECK(destination[0]==0xcc && destination[1]==0xcc && destination[2]==3 && destination[3]==3);
        CHECK(destination[12]==5 && destination[13]==5 && destination[14]==6 && destination[15]==6);
        for(i=0;i<4;++i)CHECK(destination[i*6+4]==0xcc && destination[i*6+5]==0xcc);
        CHECK(game_ui_indexed_copy(&to,&from,&full,&mirrored,0,0,1));
        CHECK(destination[0]==6 && destination[1]==5 && destination[2]==4);
        CHECK(destination[6]==3 && destination[7]==0 && destination[8]==1);
        {GameUiRect a={0,0,5,1},b={1,0,6,1};
         CHECK(game_ui_indexed_copy(&same,&same,&a,&b,0,0,0));
         CHECK(!memcmp(overlap,(uint8_t[]){1,1,2,3,4,5},6));}
        {GameUiRect a={0,0,6,1};
         CHECK(game_ui_indexed_copy(&same,&same,&a,&a,0,0,1));
         CHECK(!memcmp(overlap,(uint8_t[]){5,4,3,2,1,1},6));}
        full.right=4;CHECK(!game_ui_indexed_copy(&to,&from,&full,&mirrored,0,0,0));
        full.right=3;from.bytes=6;CHECK(!game_ui_indexed_copy(&to,&from,&full,&mirrored,0,0,0));
    }
    {
        GameMemory initial={0};char path[9];
        CHECK(game_initial_data_create(&initial));
        CHECK(initial.base==0x400000u && initial.size==0x400000u);
        CHECK(game_memory_read_u32(&initial,0x571004u)==0x3f800000u);
        CHECK(game_memory_read(&initial,0x583b7cu,path,sizeof(path)));
        CHECK(!memcmp(path,"grids\\%s",sizeof(path)));
        CHECK(!game_memory_read_u32(&initial,0x400000u)); /* No PE header. */
        CHECK(!game_memory_read_u32(&initial,0x401000u)); /* No instructions. */
        CHECK(!game_memory_read_u32(&initial,0x63c000u)); /* No import table. */
        CHECK(!game_memory_read_u32(&initial,0x63e000u)); /* No PE resources. */
        CHECK(!game_memory_read_u32(&initial,0x64f000u)); /* No relocations. */
        free(initial.data);
    }
    {
        SceneInstanceStore store={0};SceneInstance node={0};unsigned i;
        for(i=0;i<128;i++){node.holder=0x1000+(127-i)*16;CHECK(scene_instance_store_take(&store,&node));}
        CHECK(store.count==128 && !scene_instance_store_take(&store,&node));
        for(i=0;i<128;i++)CHECK(scene_instance_store_find(&store,0x1000+i*16));
        scene_instance_store_find(&store,0x1000)->parent_node=scene_instance_store_find(&store,0x1010);
        for(i=1;i<128;i+=2)CHECK(scene_instance_store_remove(&store,0x1000+i*16));
        CHECK(!scene_instance_store_find(&store,0x1000)->parent_node);
        for(i=0;i<128;i++)CHECK((scene_instance_store_find(&store,0x1000+i*16)!=NULL)==!(i&1));
        scene_instance_store_clear(&store);CHECK(!store.count && !store.records && !store.by_holder);
        CHECK(scene_instance_store_take(&store,&node));scene_instance_store_clear(&store);
    }
    {
        uint8_t *bytes=calloc(1,0x240000);GameMemory memory;TerrainAsset terrain={0};
        ProducerProbe probe={0};unsigned i;
        GameplayGroundProducerServices services={&probe,producer_base,NULL,NULL,producer_acquire,producer_clear,producer_deploy,producer_recycle,NULL};
        CHECK(bytes);terrain.cells=calloc(65536,sizeof(*terrain.cells));CHECK(terrain.cells);
        game_memory_initialize(&memory,bytes,0x400000,0x240000);memory.terrain=&terrain;
        game_memory_write_u32(&memory,0x58973c,0x502000);
        game_memory_write_u32(&memory,0x5000a0,UINT32_MAX);
        game_memory_write_u32(&memory,0x5ac6ac,1234);
        game_memory_write_u32(&memory,0x5762bc,0x457a0000); /* nearby player limit4000 */
        game_memory_write_u32(&memory,0x500018,0x447a0000);game_memory_write_u32(&memory,0x500020,0x447a0000);
        for(i=0;i<4;i++)game_memory_write_u32(&memory,0x5f0210+i*4,0x503000);
        game_memory_write_u32(&memory,0x503014,1);game_memory_write_u32(&memory,0x5033bc,0x504000);
        game_memory_write_u32(&memory,0x504058,100);
        game_memory_write_u32(&memory,0x504018,0x447a0000);game_memory_write_u32(&memory,0x504020,0x447a0000);
        CHECK(gameplay_ground_producer_update(&memory,0x500000,0x533cf0,&services));
        CHECK(!memory.fault && probe.probes==1024 && probe.recycled==1 && !probe.deployed);
        CHECK(game_memory_read_u32(&memory,0x5000a0)==UINT32_MAX && !game_memory_read_u32(&memory,0x5000a4));
        probe.clear=1;
        CHECK(gameplay_ground_producer_update(&memory,0x500000,0x533cf0,&services));
        CHECK(probe.deployed==1 && probe.recycled==1 && game_memory_read_u32(&memory,0x5000a0)==1234);
        CHECK(game_memory_read_u32(&memory,0x5000a4)==1);
        game_memory_write_u32(&memory,0x5000a0,UINT32_MAX);probe.error=1;
        CHECK(!gameplay_ground_producer_update(&memory,0x500000,0x533cf0,&services) && probe.recycled==2);
        free(terrain.cells);free(bytes);
    }
    {
        /* Repeated level-sized page churn must preserve indexed ownership,
         * reject interior/double frees and reuse freed ranges safely. */
        uint32_t pages[128],round,i,base,size;SceneProjectionStorageStats before,after;
        scene_projection_storage_stats(&before);
        for(round=0;round<3;round++){
            for(i=0;i<128;i++){
                pages[i]=scene_projection_storage_allocate(16u+(i%7u)*16u,"reuse check");
                CHECK(pages[i] && !scene_projection_storage_free(pages[i]+1u));
            }
            for(i=1;i<128;i+=2){
                CHECK(scene_projection_storage_free(pages[i]));
                CHECK(!scene_projection_storage_free(pages[i]));
            }
            for(i=1;i<128;i+=2){
                pages[i]=scene_projection_storage_allocate(16u+(i%7u)*16u,"reuse check");
                CHECK(pages[i]);
            }
            for(i=0;i<128;i++){
                CHECK(scene_projection_storage_describe(pages[i]+4u,&base,&size,NULL));
                CHECK(base==pages[i] && size==16u+(i%7u)*16u);
                CHECK(scene_projection_storage_free(pages[i]));
            }
            scene_projection_storage_stats(&after);CHECK(after.live==before.live);
        }
    }
    {
        /*Ground actors ignore the editor's stored altitude. Priva includes
         *large positive/negative values that otherwise float/sink until moving. */
        CHECK(level_object_start_height(0x21,6.23f,128,-123.75L)==4.25f);
        CHECK(level_object_start_height(0x21,-13.8f,128,-123.75L)==4.25f);
        CHECK(level_object_start_height(0x25,0,32,1000)==1032);
        CHECK(level_object_start_height(0x26,2,32,1000)==1024);
        CHECK(level_object_start_height(0x27,-2,32,1000)==-1024);
        CHECK(level_object_start_height(0xb0,3,32,1000)==1536);
    }
    {
        uint8_t bytes[256]={0};GameMemory memory;PresentationPose pose={0};Projectile shot={0};
        game_memory_initialize(&memory,bytes,0x10000,sizeof(bytes));
        pose.hull=0x10000;pose.vertices=0x10040;pose.vertex_count=1;
        pose.flags=0x880000; /* Rotation and ordinary vertex cache already set. */
        pose.world_matrix[0]=pose.world_matrix[5]=pose.world_matrix[10]=1;
        shot.velocity[0]=100;
        CHECK(projectile_move_program(&memory,&shot,&pose,0x5368b0,1));
        CHECK(pose.position[0]==100 && pose.world_matrix[3]==100);
        CHECK(game_memory_read_u32(&memory,pose.vertices)==0x42c80000);
        CHECK(projectile_move_program(&memory,&shot,&pose,0x5368b0,1));
        CHECK(pose.position[0]==200 && game_memory_read_u32(&memory,pose.vertices)==0x43480000);
    }
    {
        uint8_t state_bytes[4]={0};GameMemory memory;uint32_t state=UINT32_MAX,i;
        game_memory_initialize(&memory,state_bytes,0x5a1cd0,sizeof(state_bytes));
        portable_crt_rand_seed(&memory,state);
        CHECK(!memory.fault && game_memory_read_u32(&memory,0x5a1cd0)==state);
        for(i=0;i<4096;++i){
            state=state*214013u+2531011u;
            CHECK(portable_crt_rand_step(&memory)==((state>>16)&32767));
            CHECK(game_memory_read_u32(&memory,0x5a1cd0)==state && !memory.fault);
        }
    }
    CHECK(test_owned_submission()==0);
    {
        uint8_t palette[12]={0,0,0,0,0,0,255,0,0,255,0,0};
        uint8_t source_pixels[8]={2,1,0,0,1,2,0,0},output[25];
        AssetBitmap image={2,2,4,3,palette,source_pixels,sizeof(source_pixels),0,0};
        int32_t src[4]={0,0,2,2},dst[4]={-1,0,3,2};unsigned i;
        memset(output,0xa5,sizeof(output));
        CHECK(asset_bitmap_blit_rgb565(&image,src,dst,output+1,24,3,2,12,0,0,0));
        CHECK(output[0]==0xa5 && output[1]==0 && output[2]==0xf8 && output[3]==0xe0 && output[4]==7);
        CHECK(output[13]==0xe0 && output[14]==7 && output[15]==0 && output[16]==0xf8);
        for(i=7;i<13;i++)CHECK(output[i]==0xa5);
        CHECK(output[24]==0xa5);
        dst[0]=0;dst[2]=2;memset(output,0xa5,sizeof(output));
        CHECK(asset_bitmap_blit_rgb565(&image,src,dst,output+1,24,3,2,12,1,1,0));
        CHECK(output[1]==0xa5 && output[2]==0xa5 && output[3]==0xe0 && output[4]==7);
        CHECK(!asset_bitmap_blit_rgb565(&image,src,dst,output+1,23,3,2,12,0,0,0));
        image.top_down=1;
        CHECK(asset_bitmap_blit_rgb565(&image,src,dst,output+1,24,3,2,12,0,0,0));
        CHECK(output[1]==0xe0 && output[2]==7);
        CHECK(asset_bitmap_blit_rgb565(&image,src,dst,output+1,24,3,2,12,0,0,1));
        CHECK(output[1]==0xe0 && output[2]==7 && output[13]==0 && output[14]==0xf8);
        {uint8_t bgr[8]={0,0,255,0,255,0,0,0};image.pixels=bgr;image.rgb24=1;image.stride=8;image.height=1;
         image.palette_bgra=NULL;image.palette_count=0;src[3]=dst[3]=1;
         CHECK(asset_bitmap_blit_rgb565(&image,src,dst,output+1,24,3,2,12,0,0,0));
         CHECK(output[1]==0 && output[2]==0xf8 && output[3]==0xe0 && output[4]==7);}
        {uint8_t locked[11]={0xa5,0,0xf8,255,255,0xa5,0xe0,7,0,0,0xa5};
         src[3]=dst[3]=2;dst[0]=-1;dst[2]=3;memset(output,0xa5,sizeof(output));
         CHECK(asset_rgb565_blit(locked+1,10,2,2,5,src,dst,output+1,24,3,2,12,1,65535,65535));
         CHECK(output[1]==0 && output[2]==0xf8 && output[3]==0xa5 && output[4]==0xa5);
         CHECK(output[13]==0xe0 && output[14]==7 && output[15]==0 && output[16]==0);
         CHECK(!asset_rgb565_blit(locked+1,9,2,2,5,src,dst,output+1,24,3,2,12,0,0,0));}
    }
    {
        const GameAddress manager=0x005f0800u,first=manager+0x10000u,second=first+0x100u;
        uint8_t *legacy=calloc(1u,0x12000u);GameMemory memory;GameAddress neighbors[9],group;unsigned row,column,i;
        CHECK(legacy);game_memory_initialize(&memory,legacy,manager,0x12000u);
        gameplay_spatial_initialize_neighbors(&memory,manager);
        CHECK(!memory.fault && gameplay_spatial_begin(manager)==1);
        for(row=0u;row<32u;++row)for(column=0u;column<32u;++column){
            CHECK(gameplay_spatial_query_values(manager,(float)(row*4096u+123u),(float)(column*4096u+456u),neighbors));
            CHECK(!memcmp(neighbors,legacy+0x6000u+(row*32u+column)*36u,sizeof(neighbors)));
            CHECK(gameplay_spatial_query_group(manager,(float)(row*4096u),(float)(column*4096u),&group));
            for(i=0u;i<9u;++i){GameAddress value=0u;CHECK(gameplay_spatial_projection_read(group+i*4u,&value,4u) && value==neighbors[i]);}
        }
        CHECK(!gameplay_spatial_query_values(manager,-1.0f,0.0f,neighbors));
        CHECK(!gameplay_spatial_query_values(manager,131072.0f,0.0f,neighbors));
        CHECK(gameplay_spatial_query_values(manager,-0.0f,0.0f,neighbors));
        memory.owned_pages=projection_owner_pages();memory.read_fallback=spatial_read;memory.write_fallback=spatial_write;
        CHECK(gameplay_spatial_insert(&memory,manager,first,7,9,0).previous_head==0u);
        CHECK(gameplay_spatial_insert(&memory,manager,second,7,9,0).previous_head==first);
        CHECK(gameplay_spatial_grid(manager)->heads[0][7][9]==second && !gameplay_spatial_end());
        (void)game_list_detach_node(&memory,first,4u,8u);
        CHECK(!memory.fault && game_memory_read_u32(&memory,second+4u)==0u);
        (void)game_list_detach_node(&memory,second,4u,8u);
        CHECK(!memory.fault && !gameplay_spatial_grid(manager)->heads[0][7][9]);
        CHECK(gameplay_spatial_insert(&memory,manager,first,31,0,1).previous_head==0u);
        (void)game_list_detach_node(&memory,first,4u,8u);
        CHECK(!memory.fault && gameplay_spatial_end());free(legacy);
    }
    CHECK(check_render_texture_lifetime()==0);
    CHECK(check_player_events()==0);
    CHECK(check_resource_lifetime()==0);
    CHECK(check_event_lifetime()==0);
    CHECK(check_event_cross_family()==0);
    unsigned char bytes[16] = {'a', '\\', 'b', 0};
    GameMemory memory;
    const float identity[12] = {1,0,0,0, 0,1,0,0, 0,0,1,0};
    const float transform[12] = {2,0,0,10, 0,3,0,20, 0,0,4,30};
    const float point[3] = {1,2,3};
    float composed[12], output[3];
    ResourcePathBytes span = {bytes, 2};
    ResourceScanResult scan = resource_path_find_byte(span, '\\');
    Tail tail = {0, 0x1002, '\\'};
    ResourceTree tree;
    const ResourceEntry *entry, *found;
    const uint8_t stored_path[] = "root\\leaf";
    const uint8_t query_path[] = "ROOT\\LEAF";
    ResourcePathBytes stored = {stored_path, sizeof(stored_path)};
    ResourcePathBytes query = {query_path, sizeof(query_path)};
    ResourceEntryInfo info = {100, 25, 1000};
    {
        uint8_t packed[512], decoded[32640], malformed[3]={1,2,3};
        size_t cursor=0;
        for (unsigned i=0;i<256;i++) { packed[2*i]=(uint8_t)i; packed[2*i+1]=(uint8_t)(255-i); }
        CHECK(resource_payload_decode_rle(packed,sizeof(packed),decoded,sizeof(decoded))==RESOURCE_PAYLOAD_OK);
        for (unsigned i=0;i<256;i++) for(unsigned j=0;j<i;j++) CHECK(decoded[cursor++]==255-i);
        memset(decoded,0xcd,sizeof(decoded));
        CHECK(resource_payload_decode_rle(malformed,3,decoded,1)==RESOURCE_PAYLOAD_ODD_PACKED_SIZE);
        CHECK(resource_payload_decode_rle(packed,sizeof(packed),decoded,sizeof(decoded)-1)==RESOURCE_PAYLOAD_SIZE_MISMATCH);
        CHECK(resource_payload_decode_rle(packed,2,decoded,1)==RESOURCE_PAYLOAD_SIZE_MISMATCH);
        CHECK(resource_payload_decode_rle(NULL,2,decoded,1)==RESOURCE_PAYLOAD_INVALID_BUFFER);
        CHECK(resource_payload_decode_rle(packed,2,NULL,1)==RESOURCE_PAYLOAD_INVALID_BUFFER);
        CHECK(resource_payload_decode_rle(NULL,0,NULL,0)==RESOURCE_PAYLOAD_OK);
        CHECK(resource_payload_decode_rle(packed,2,NULL,0)==RESOURCE_PAYLOAD_OK);
        for(size_t i=0;i<sizeof(decoded);i++) CHECK(decoded[i]==0xcd);
    }
    resource_tree_initialize(&tree, NULL);
    CHECK(resource_tree_insert(&tree, stored, info, &entry) == RESOURCE_TREE_OK);
    CHECK(resource_tree_lookup_c_locale(&tree, query, &found) == RESOURCE_TREE_OK);
    CHECK(found == entry && found->info.archive_header_offset == 1000);
    {
        ResourceEntryInfo copied = {7, 8, 9};
        ResourceEntry foreign = {0};
        CHECK(resource_tree_entry_info(&tree, found, &copied));
        CHECK(copied.unpacked_size == 100 && copied.packed_size == 25 && copied.archive_header_offset == 1000);
        CHECK(!resource_tree_entry_info(&tree, &foreign, &copied));
        CHECK(!resource_tree_entry_info(&tree, NULL, &copied));
        CHECK(!resource_tree_entry_info(NULL, found, &copied));
        CHECK(!resource_tree_entry_info(&tree, found, NULL));
        tree.fatal_status = RESOURCE_TREE_FATAL_ENTRY;
        CHECK(!resource_tree_entry_info(&tree, found, &copied));
        CHECK(copied.unpacked_size == 100 && copied.packed_size == 25 && copied.archive_header_offset == 1000);
        tree.fatal_status = RESOURCE_TREE_OK;
    }
    resource_tree_destroy(&tree);
    CHECK(!tree.roots && !tree.fatal_status);
    resource_tree_destroy(&tree);
    CHECK(scan.status == RESOURCE_SCAN_FOUND && scan.byte_offset == 1);
    scan = resource_path_find_byte(span, 'z');
    CHECK(scan.status == RESOURCE_SCAN_UNREADABLE && scan.fault_offset == 2);
    span.data = NULL; span.readable_size = 0;
    scan = resource_path_find_byte(span, 0);
    CHECK(scan.status == RESOURCE_SCAN_UNREADABLE && scan.fault_offset == 0);
    span.data = bytes; span.readable_size = sizeof(bytes);
    scan = resource_path_find_byte(span, 0);
    CHECK(scan.status == RESOURCE_SCAN_FOUND && scan.byte_offset == 3);
    scan = resource_path_find_byte(span, 'z');
    CHECK(scan.status == RESOURCE_SCAN_ABSENT);
    game_memory_initialize(&memory, bytes, 0x1000, sizeof(bytes));
    CHECK(game_string_find_byte(&memory, 0x1000, '\\') == 0x1001);
    CHECK(game_string_find_byte(&memory, 0x1000, 0) == 0x1003);
    CHECK(game_string_find_byte(&memory, 0x1000, 'z') == 0);
    CHECK(!memory.fault);
    game_memory_write_u32(&memory, 0x100c, 0x12345678);
    CHECK(game_memory_read_u32(&memory, 0x100c) == 0x12345678);
    CHECK(bytes[12] == 0x78 && bytes[15] == 0x12);
    CHECK(!game_memory_contains(&memory, 0xffffffff, 2));
    (void)game_memory_read_u8(&memory, 0x1010);
    CHECK(memory.fault && memory.fault_address == 0x1010 &&
        memory.fault_size == 1 && !memory.fault_write);
    CHECK(game_string_find_byte(&memory, 0x1000, 'a') == 0 && memory.fault);
    bytes[1] = 'b';
    game_memory_initialize(&memory, bytes, 0x1000, 2);
    memory.read_fallback = read_tail; memory.fallback_context = &tail;
    CHECK(game_string_find_byte(&memory, 0x1000, '\\') == 0x1002);
    CHECK(!memory.fault && tail.calls == 1);
    tail.calls = 0; tail.value = 0;
    CHECK(game_string_find_byte(&memory, 0x1000, 'z') == 0);
    CHECK(!memory.fault && tail.calls == 1);
    tail.calls = 0; tail.value = 'c';
    CHECK(game_string_find_byte(&memory, 0x1000, 'z') == 0);
    CHECK(memory.fault && memory.fault_address == 0x1003 && tail.calls == 2);
    /* Numeric address wrapping is an adapter concern, never a span offset. */
    game_memory_initialize(&memory, bytes, UINT32_MAX - 1, 2);
    tail.calls = 0; tail.address = 0; tail.value = 0;
    memory.read_fallback = read_tail; memory.fallback_context = &tail;
    CHECK(game_string_find_byte(&memory, UINT32_MAX - 1, 'z') == 0);
    CHECK(!memory.fault && tail.calls == 1);
    game_matrix_compose_values(composed, identity, transform);
    CHECK(memcmp(composed, transform, sizeof(composed)) == 0);
    {
        float left[12]={0.6f,0,0.8f,12,0,1,0,-7,-0.8f,0,0.6f,30};
        float right[12]={1,0,0,4,0,0.8f,-0.6f,5,0,0.6f,0.8f,6};
        float expected[12],in_place[12];
        game_matrix_compose_values(expected,left,right);
        memcpy(in_place,left,sizeof(in_place));
        game_matrix_compose_values(in_place,in_place,right);
        CHECK(memcmp(in_place,expected,sizeof(expected))==0);
        memcpy(in_place,right,sizeof(in_place));
        game_matrix_compose_values(in_place,left,in_place);
        CHECK(memcmp(in_place,expected,sizeof(expected))==0);
        game_matrix_compose_values(expected,left,left);
        memcpy(in_place,left,sizeof(in_place));
        game_matrix_compose_values(in_place,in_place,in_place);
        CHECK(memcmp(in_place,expected,sizeof(expected))==0);
    }
    game_matrix_transform_point_values(output, composed, point);
    CHECK(output[0] == 12 && output[1] == 26 && output[2] == 42);
    {
        SceneChildRegistry children={0};SceneChildRecord child;
        SceneMesh child_mesh={0};
        CHECK(scene_child_registry_initialize(&children,36));
        for(unsigned i=0;i<40;i++){
            child.external_id=0x1000u+i;child.category=i%3u;child.mesh=NULL;
            CHECK(scene_child_registry_append(&children,child));
        }
        CHECK(children.count==40 && children.capacity>=40);
        CHECK(scene_child_registry_get(&children,36,&child));
        CHECK(child.external_id==0x1024u && child.category==0);
        CHECK(scene_child_registry_bind_mesh(&children,36,child.external_id,
            &child_mesh));
        CHECK(scene_child_registry_get(&children,36,&child) &&
            child.mesh==&child_mesh && child.external_id==0u);
        scene_child_registry_remove_last(&children);
        CHECK(children.count==39 && !scene_child_registry_get(&children,39,&child));
        scene_child_registry_release(&children);
        CHECK(!children.records && !children.count);
    }
    {
        ServiceVisitState services = {{0}, 1};
        ApplicationServiceRosterStats stats;
        CHECK(application_service_roster_build(1u, 3u,
            next_service, &services));
        CHECK(application_service_roster_count() == 3u);
        CHECK(!application_service_roster_release(destroy_service,
            &services));
        application_service_roster_stats(&stats);
        CHECK(stats.live_count == 2u && services.calls[1] == 1u);
        CHECK(!application_service_roster_build(1u, 3u,
            next_service, &services));
        CHECK(!application_service_roster_tick(destroy_service, &services));
        CHECK(application_service_roster_release(destroy_service,
            &services));
        application_service_roster_stats(&stats);
        CHECK(stats.live_count == 0u && stats.destroyed == 3u &&
            services.calls[1] == 1u && services.calls[2] == 2u &&
            services.calls[3] == 1u);
    }
    {
        uint8_t scene_bytes[0x1000] = {0};
        GameAddress parts[PLAYER_RUNTIME_PARTS], auxiliary;
        GameMemory scenes;
        PartsReleaseState released = {0u, 0u, 0u, 0u, 0u, 1};
        PlayerRuntimeDestroyServices destroy = {
            &released, release_part, clear_parts_projection,
            clear_parts_events, release_parts_base
        };
        game_memory_initialize(&scenes, scene_bytes, 0x1000u,
            sizeof(scene_bytes));
        game_memory_write_u32(&scenes, 0x1500u + 0x1d4u, 0x1800u);
        game_memory_write_u32(&scenes, 0x1500u + 0x210u, 0x1900u);
        CHECK(player_runtime_pool_bind(0u, 0x1100u));
        CHECK(player_runtime_pool_attach(0u, 0x1500u));
        CHECK(player_runtime_pool_begin_shutdown(0u) == 0x1500u);
        CHECK(player_runtime_pool_bind_parts(&scenes, 0x1500u));
        CHECK(!player_runtime_pool_has_borrowed(0u));
        CHECK(!player_runtime_pool_destroy_parts(0x1500u, &destroy));
        CHECK(released.children == 1u && released.auxiliary == 0u &&
            released.projection == 0u);
        CHECK(player_runtime_pool_parts(0x1500u, parts) && !parts[0]);
        CHECK(player_runtime_pool_begin_shutdown(0u) == 0x1500u);
        CHECK(player_runtime_pool_auxiliary(0x1500u, &auxiliary) &&
            auxiliary == 0x1900u);
        CHECK(player_runtime_pool_destroy_parts(0x1500u, &destroy));
        CHECK(player_runtime_pool_destroy_parts(0x1500u, &destroy));
        CHECK(released.children == 1u && released.auxiliary == 1u &&
            released.projection == 1u && released.events == 1u &&
            released.base == 1u);
        CHECK(player_runtime_pool_auxiliary(0x1500u, &auxiliary) &&
            !auxiliary);
        CHECK(player_runtime_pool_retire(0x1500u));
        CHECK(!player_runtime_pool_begin_shutdown(0u));
        player_runtime_pool_forget(0u);
    }
    {
        uint8_t event_bytes[0x400] = {0};
        GameMemory events;
        EventReleaseState released = {{0}, 0u};
        GameEventQueueCallbacks callbacks = {&released, NULL, release_event};
        game_memory_initialize(&events, event_bytes, 0x1000u,
            sizeof(event_bytes));
        game_memory_write_u32(&events, 0x1128u, 0x1200u);
        game_memory_write_u32(&events, 0x112cu, 0x1240u);
        game_memory_write_u32(&events, 0x1204u, 0x1240u);
        CHECK(game_timed_event_queue_clear(&events, &callbacks, 0x1100u));
        CHECK(released.count == 2u && released.released[0] == 0x1200u &&
            released.released[1] == 0x1240u);
        CHECK(game_memory_read_u32(&events, 0x1128u) == 0u &&
            game_memory_read_u32(&events, 0x112cu) == 0u);
        released.count = 0u;
        game_memory_write_u32(&events, 0x1158u, 0x1200u);
        game_memory_write_u32(&events, 0x1204u, 0u);
        CHECK(game_event_list_clear_head(&events, &callbacks, 0x1158u));
        CHECK(released.count == 1u && released.released[0] == 0x1200u &&
            game_memory_read_u32(&events, 0x1158u) == 0u);
    }
    {
        ModeScenario mode = {{0}, {0}, 0u};
        ApplicationModeServices services = {
            &mode, mode_read, mode_write, mode_action
        };
        ApplicationModeStats stats;
        static const ApplicationModeAction expected[] = {
            APPLICATION_MODE_LOAD_FALLBACK, APPLICATION_MODE_START_SCENE,
            APPLICATION_MODE_RESOURCE_GAME,
            APPLICATION_MODE_STOP_SOUND, APPLICATION_MODE_STOP_INPUT,
            APPLICATION_MODE_NOTIFY, APPLICATION_MODE_CLOSE_THREE,
            APPLICATION_MODE_STOP_SOUND,
            APPLICATION_MODE_STOP_INPUT
        };
        mode.fields[APPLICATION_MODE_CURRENT] = 1u;
        mode.fields[APPLICATION_MODE_UI_ACTIVE] = 1u;
        mode.fields[APPLICATION_MODE_SOUND_ACTIVE] = 1u;
        mode.fields[APPLICATION_MODE_INPUT_ACTIVE] = 1u;
        mode.fields[APPLICATION_MODE_CLEAR_A] = 22u;
        mode.fields[APPLICATION_MODE_CLEAR_B] = 11u;
        mode.fields[APPLICATION_MODE_SCALE] = 0x40000000u;
        CHECK(application_mode_change(3u, &services));
        CHECK(mode.fields[APPLICATION_MODE_SCALE] == 0x3f800000u &&
            mode.fields[APPLICATION_MODE_CURRENT] == 3u &&
            mode.fields[APPLICATION_MODE_SAVED_A] == 11u &&
            mode.fields[APPLICATION_MODE_SAVED_B] == 22u &&
            mode.fields[APPLICATION_MODE_SAVED_SCALE] == 0x40000000u);
        CHECK(application_mode_change(0u, &services));
        CHECK(mode.fields[APPLICATION_MODE_CLEAR_A] == 22u &&
            mode.fields[APPLICATION_MODE_CLEAR_B] == 11u &&
            mode.fields[APPLICATION_MODE_SCALE] == 0x40000000u);
        CHECK(mode.count == sizeof(expected) / sizeof(expected[0]) &&
            memcmp(mode.actions, expected, sizeof(expected)) == 0);
        application_mode_stats(&stats);
        CHECK(stats.current == 0u && stats.transitions == 2u &&
            stats.entered[3] == 1u && stats.entered[0] == 1u &&
            stats.service_calls == mode.count);
    }
    {
        FrameScenario frame = {{0}, {0}, {320u, 384u}, 0u, 0u, 0u, 0u, 0u};
        ApplicationFrameInput input = {0u, 10u, 0, 0};
        ApplicationFrameServices services = {
            &frame, frame_now, frame_mode, frame_action, frame_publish
        };
        ApplicationFrameStats stats;
        static const ApplicationFrameAction expected[] = {
            APPLICATION_FRAME_PREPARE_TICKS,
            APPLICATION_FRAME_WORLD_BEGIN, APPLICATION_FRAME_SCENE_BEGIN,
            APPLICATION_FRAME_WORLD_STEP, APPLICATION_FRAME_SCENE_STEP,
            APPLICATION_FRAME_WORLD_STEP, APPLICATION_FRAME_SCENE_STEP,
            APPLICATION_FRAME_GAMEPLAY_STEP, APPLICATION_FRAME_WORLD_END,
            APPLICATION_FRAME_SCENE_END, APPLICATION_FRAME_RENDER_GAME,
            APPLICATION_FRAME_FINISH, APPLICATION_FRAME_SUSPEND,
            APPLICATION_FRAME_RENDER_MODE_SEVEN, APPLICATION_FRAME_FINISH
        };
        application_frame_begin(10u);
        CHECK(application_frame_run(&input, &services));
        CHECK(frame.published == 12u && frame.values[1] == 2u &&
            frame.values[3] == 10u && frame.values[5] == 11u &&
            frame.values[7] == 12u);
        input.suspended = 1;
        frame.mode = 7u;
        CHECK(application_frame_run(&input, &services));
        CHECK(frame.count == sizeof(expected) / sizeof(expected[0]) &&
            memcmp(frame.actions, expected, sizeof(expected)) == 0);
        application_frame_stats(&stats);
        CHECK(stats.frames == 2u && stats.suspended_frames == 1u &&
            stats.ticks == 2u && stats.gameplay_ticks == 1u &&
            stats.last_tick == 12u);
        frame.clock_calls = 0u;
        frame.now[0] = 416u;
        frame.now[1] = 448u;
        input.suspended = 0;
        input.networked = 1;
        CHECK(application_frame_run(&input, &services));
        application_frame_stats(&stats);
        CHECK(frame.published == 14u && stats.frames == 3u &&
            stats.ticks == 4u && stats.gameplay_ticks == 1u &&
            stats.last_tick == 14u);
        application_frame_end();
    }
    {
        ClockCreateScenario create = {0};
        LevelClockCreateServices create_services = {
            &create, clock_allocate, clock_initialize_pool,
            clock_configure_pool, clock_project
        };
        ClockStopScenario clock = {{100u, 120u, 140u, 160u},
            0u, 0u, 0u, 0u, 0u, 1};
        LevelClockStopServices services = {
            &clock, clock_sample, clock_pending, clock_refresh,
            clock_release, clock_unpublish
        };
        LevelClockStats stats;
        uint32_t object = 0u;
        CHECK(level_clock_create(19u, &create_services, &object));
        CHECK(object == 7u && create.allocations == 2u &&
            create.initialized == 1u && create.configured == 1u &&
            create.projected == 1u && create.channel == 19u);
        level_clock_begin(object, 32);
        CHECK(!level_clock_stop_pending(0u, &services));
        CHECK(clock.samples == 4u && clock.pending_calls == 3u &&
            clock.refreshes == 1u && clock.releases == 1u &&
            level_clock_current()->active);
        CHECK(level_clock_stop_pending(0u, &services));
        CHECK(clock.samples == 4u && clock.releases == 1u &&
            clock.unpublished == 2u && !level_clock_current()->active);
        level_clock_stats(&stats);
        CHECK(stats.starts == 1u && stats.stops == 1u &&
            stats.wait_samples == 4u && stats.refreshes == 1u);
    }
    {
        ShutdownScenario scenario = {{0}, {0}, {0}, 1};
        ApplicationShutdownServices services = {&scenario, shutdown_step};
        ApplicationShutdownStats stats;
        application_shutdown_begin();
        CHECK(!application_shutdown_execute(&services));
        application_shutdown_stats(&stats);
        CHECK(stats.step == APPLICATION_SHUTDOWN_RELEASE_POOL &&
            stats.cursor == 2u && scenario.pools[0] == 1u &&
            scenario.pools[1] == 1u && scenario.pools[2] == 1u);
        application_shutdown_begin();
        CHECK(application_shutdown_execute(&services));
        application_shutdown_stats(&stats);
        CHECK(stats.starts == 1u && stats.completes == 1u &&
            stats.step == APPLICATION_SHUTDOWN_DONE &&
            stats.active_mask == 1u && scenario.pools[0] == 1u &&
            scenario.pools[2] == 2u && scenario.services[11] == 1u);
    }
    {
        uint8_t color[16*20+2],depth[16*32+2],pixels[4]={254,17,31,47};unsigned x,y,hits=0;
        RenderArenaVertex corners[4]={{{2,2,100},1},{{8,2,100},1},{{8,8,100},1},{{2,8,100},1}};
        const RenderArenaVertex *vertices[4]={corners,corners+1,corners+2,corners+3};
        RenderArenaUv uv[4]={{1,0},{1,0},{1,0},{1,0}};
        SoftwareRasterFrame frame={color+1,depth+1,sizeof(color)-2,sizeof(depth)-2,16,16,20,32};
        SoftwareRasterTexture texture={pixels,sizeof(pixels),2,2};
        memset(color,254,sizeof(color));memset(depth,255,sizeof(depth));
        CHECK(software_raster_polygon(&frame,0x119,0,vertices,uv,4,&texture)==1);
        for(y=0;y<16;++y)for(x=0;x<16;++x){
            int covered=x>=3 && x<=8 && y>=3 && y<=8;
            CHECK(color[1+y*20+x]==(covered?17:254));
            CHECK(depth[1+y*32+x*2]==(covered?100:255));
            CHECK(depth[2+y*32+x*2]==(covered?0:255));
            hits+=color[1+y*20+x]==17;
        }
        CHECK(hits==36);
        pixels[1]=31; /* Equal depth must retain prior color. */
        CHECK(software_raster_polygon(&frame,0x119,0,vertices,uv,4,&texture)==1);
        CHECK(color[1+4*20+4]==17);
        for(x=0;x<4;++x)corners[x].position[2]=99;
        CHECK(software_raster_polygon(&frame,0x119,0,vertices,uv,4,&texture)==1);
        CHECK(color[1+4*20+4]==31 && depth[1+4*32+8]==99);
        pixels[1]=254;
        for(x=0;x<4;++x)corners[x].position[2]=98;
        CHECK(software_raster_polygon(&frame,0x119,0,vertices,uv,4,&texture)==1);
        CHECK(color[1+4*20+4]==31 && depth[1+4*32+8]==99);
        CHECK(software_raster_polygon(&frame,0x11d,0,vertices,uv,4,&texture)==-1);
        CHECK(software_raster_polygon(&frame,0x10,47,vertices,NULL,4,NULL)==1);
        CHECK(color[1+4*20+4]==47 && depth[1+4*32+8]==99); /* Test without depth write. */
        corners[0].position[0]=-2;corners[3].position[0]=-2;
        CHECK(software_raster_polygon(&frame,0,17,vertices,NULL,4,NULL)==1);
        CHECK(color[1+4*20]==17 && color[1+4*20+16]==254); /* Clip and retain row padding. */
        CHECK(color[0]==254 && color[sizeof(color)-1]==254 && depth[0]==255 && depth[sizeof(depth)-1]==255);
        frame.depth_bytes=1;
        CHECK(software_raster_polygon(&frame,0x119,0,vertices,uv,4,&texture)==0);
    }
    printf("portable core smoke PASS: pointer_bits=%u float_bytes=%u long_double_bytes=%u\n",
        (unsigned)(sizeof(void *) * 8), (unsigned)sizeof(float),
        (unsigned)sizeof(long double));
    return 0;
}
