#include "level_world_surface.h"
#include "scene_surface.h"
#include "render_arena.h"
#include "render_runtime_pools.h"
#include "render_runtime_projection.h"
#include "asset_bitmap.h"
#include <stdlib.h>
#include <string.h>

static const GameAddress table_globals[5]={0x596108,0x59610c,0x596110,0x596114,0x596118};
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
static float real(GameApplication *a,GameAddress p){uint32_t bits=get(a,p);float v;memcpy(&v,&bits,4);return v;}
static int32_t signed_word(uint32_t v){return v<=INT32_MAX?(int32_t)v:(int32_t)((int64_t)v-4294967296LL);}
static void release_texture(void *context,uint32_t id){(void)game_model_texture_release(context,id);}
/*4d7b00 retires the existing native surface and its exclusively owned texture.
 *The application memory's surface pointer owns this object; tables are bounded
 *views of that object, without serialized mutable copies of its vertices. */
int game_level_world_surface_release(GameApplication *a){
    SceneSurface *surface=a->memory.surface;unsigned i;
    if(surface){scene_surface_release(surface);free(surface);a->memory.surface=NULL;}
    for(i=0;i<5;++i)put(a,table_globals[i],0);
    return !a->memory.fault;
}
/* Keep the existing surface topology/texture owner, initialized from the same
 * BMP fallback as the layered renderer. The obsolete TempSky TGA is not needed. */
int game_level_world_load(GameApplication *a){
    ResourceBlob blob={0};AssetBitmap bitmap={0};AssetTexture image={0};MaterialValue texture={0};
    const int32_t rect[4]={0,0,256,256};SceneSurface *surface=NULL;unsigned i;int ok=0;
    if(!game_level_world_surface_release(a))return 0;
    if(!game_application_load_blob(a,"GRIDS/skies/tempsky.bmp",&blob) ||
       !asset_bitmap8_parse(&blob,&bitmap) || bitmap.width!=256 || bitmap.height!=256)goto done;
    image.width=image.height=image.render_width=image.render_height=256;
    image.aspect=3;image.format=10;image.size=256u*256u*2u;image.pixels=malloc(image.size);
    surface=calloc(1,sizeof(*surface));
    if(!image.pixels || !surface || !asset_bitmap_blit_rgb565(&bitmap,rect,rect,
       image.pixels,image.size,256,256,512,0,0,0) || !scene_surface_initialize(surface) ||
       !game_model_texture_take(a,&image,0,&texture))goto done;
    if(!scene_surface_take_texture(surface,texture.backend_handle,release_texture,a))goto done;
    texture.backend_handle=0;a->memory.surface=surface;surface=NULL;
    for(i=0;i<5;++i)put(a,table_globals[i],a->memory.surface->address[i]);
    ok=!a->memory.fault;
done:
    resource_blob_release(&blob);asset_texture_release(&image);
    if(texture.backend_handle)(void)game_model_texture_release(a,texture.backend_handle);
    if(surface){scene_surface_release(surface);free(surface);}
    if(!ok)(void)game_level_world_surface_release(a);return ok;
}

static int storage_read(void *context,GameAddress p,void *out,size_t bytes){
    GameApplication *a=context;
    return scene_surface_read(a->memory.surface,p,out,bytes) || render_runtime_pool_read(p,out,bytes) || render_arena_read(p,out,bytes);
}
static int storage_write(void *context,GameAddress p,const void *data,size_t bytes){
    GameApplication *a=context;
    return scene_surface_write_pipeline(a->memory.surface,p,data,bytes) || render_runtime_pool_write(p,data,bytes) || render_arena_write(p,data,bytes);
}
static int cursors_ready(void *context,const GameplayClipCursors *cursors){
    const GameAddress values[4]={cursors->vertex,cursors->packet,cursors->uv,cursors->scalar};
    const unsigned pools[4]={1,2,3,5};uint8_t probe;unsigned i;(void)context;
    for(i=0;i<4;++i){GameAddress begin=render_runtime_pool_view(pools[i]);
        if(values[i]<begin || values[i]-begin>=RENDER_RUNTIME_POOL_STRIDE || !render_runtime_pool_read(values[i],&probe,1))return 0;}
    return 1;
}
static uint32_t *storage_index(void *context,GameAddress p){(void)context;return render_runtime_pool_index(p);}
static float *storage_component(void *context,GameAddress p){(void)context;return render_runtime_pool_float_component(p);}
static RenderArenaPacket *storage_packet(void *context,GameAddress p){(void)context;return render_runtime_pool_packet(p);}
static void note_write(void *context,GameAddress p){(void)context;render_runtime_pool_note_typed_write(p);}
static int polygon(void *context,const GameRenderVertex *vertices,unsigned count){return game_application_surface_polygon(context,vertices,count);}
static int animation_capture(GameApplication *a,SceneSurfaceAnimationInput *input){
    uint16_t yaw=game_memory_read_u16(&a->memory,get(a,0x58f890)+10);unsigned i;
    input->external_phase=real(a,0x59611c);input->phase_step=real(a,0x574a20);input->period=real(a,0x574a24);
    for(i=0;i<3;++i)input->camera[i]=real(a,0x629088+i*4);
    input->grid_factor=real(a,0x574a28);input->x_wrap=real(a,0x574a30);input->z_wrap=real(a,0x574a38);
    input->uv_scale=real(a,0x574a34);input->origin=real(a,0x574a3c);input->height=real(a,0x574a40);input->grid_step=real(a,0x574a44);
    input->cosine=real(a,0x6298a8+((yaw+512)&2047)*4);input->sine=real(a,0x6298a8+(yaw&2047)*4);
    input->inverse_cosine=real(a,0x6298a8+((0u-yaw+512)&2047)*4);input->inverse_sine=real(a,0x6298a8+((0u-yaw)&2047)*4);
    input->pitch_scale=real(a,0x629a38);input->vertical_scale=real(a,0x62a238);
    for(i=0;i<12;++i)input->view[i]=real(a,0x5cedf0+i*4);
    input->far_depth=(float)((long double)signed_word(get(a,0x597f7c))-real(a,0x574a4c));input->rounding_magic=real(a,0x574a18);
    return !a->memory.fault;
}
/*4d7bb0 shares the real frame cursors and native clipping/animation algorithm.
 *The shader backend receives ordinary vertices and the sole texture owner. */
int game_level_world_surface_draw(GameApplication *a){
    SceneSurface *surface=a->memory.surface;GameplayRenderFrame frame={0};SceneSurfaceAnimationInput animation;
    GameplayClipCursors cursors,previous;GameAddress head;uint32_t phase;unsigned i;int zero,arena,pool,result;
    GameSkyFrame sky;char sky_name[128];
    const TextureRecord *texture;GameplaySurfaceStorageOps storage={a,storage_read,storage_write,cursors_ready,storage_index,storage_component,storage_packet,note_write};
    if(!surface || !surface->active || !game_application_surface_begin(a) || !game_level_texture_use(a,surface->texture_handle))return 0;
    texture=game_model_texture(a,surface->texture_handle);
    if(!texture || !game_application_surface_texture(a,texture) || !game_application_surface_clamp(a,0))return 0;
    frame.clip_vertices=0x604378;frame.clip_packets=0x604268;frame.clip_uv=0x604480;frame.clip_indices=0x604270;frame.queue_head=0x604484;
    frame.center_x=real(a,0x6290c8);frame.center_y=real(a,0x6290cc);frame.scale=real(a,0x6290f4);frame.vertical=real(a,0x6290f0);
    frame.numerator=real(a,0x574cb0);frame.backface_threshold=real(a,0x574cb4);
    frame.bounds[0]=real(a,0x6290d4);frame.bounds[1]=real(a,0x6290d0);frame.bounds[2]=real(a,0x6290dc);frame.bounds[3]=real(a,0x6290d8);
    cursors.vertex=get(a,frame.clip_vertices);cursors.packet=get(a,frame.clip_packets);cursors.uv=get(a,frame.clip_uv);cursors.scalar=get(a,frame.clip_indices);head=get(a,frame.queue_head);
    frame.cursors=&cursors;frame.queue_head_value=&head;previous=cursors;
    if(!animation_capture(a,&animation))return 0;
    for(i=0;i<sizeof(sky_name);i++){sky_name[i]=(char)game_memory_read_u8(&a->memory,0x626dd0+i);if(!sky_name[i])break;}
    if(i==sizeof(sky_name) || a->memory.fault)return 0;
    sky.name=sky_name;memcpy(sky.view,animation.view,sizeof(sky.view));
    sky.center[0]=frame.center_x;sky.center[1]=frame.center_y;
    sky.scale=frame.scale;sky.vertical=frame.vertical;sky.speed=real(a,0x626d50);
    zero=!cursors.vertex && !cursors.packet && !cursors.uv && !cursors.scalar;
    if(zero && render_arena_current()){cursors.vertex=render_arena_view(0);cursors.packet=render_arena_view(3);cursors.uv=render_arena_view(2);cursors.scalar=render_arena_view(1);}
    arena=gameplay_geometry_owned_surface_ready(surface,&frame);pool=!arena && surface->active && surface->links_clean && cursors_ready(a,&cursors);
    surface->last_cursors[0]=cursors.vertex;surface->last_cursors[1]=cursors.packet;surface->last_cursors[2]=cursors.uv;surface->last_cursors[3]=cursors.scalar;
    if(arena || pool)++surface->owned_ready_frames;
    if(!game_application_sky_begin(a,&sky))return 0;
    if(arena)result=gameplay_geometry_render_owned_surface(surface,&frame,&animation,polygon,a);
    else if(pool)result=gameplay_geometry_render_owned_surface_storage(surface,&frame,&animation,polygon,a,&storage);
    else{cursors=previous;result=gameplay_geometry_render_surface_frame(&a->memory,surface->address,&frame,&animation,polygon,a,surface);}
    if(!game_application_sky_end(a))result=0;
    if(result && zero && (arena || pool) && cursors.vertex==render_arena_view(0) && cursors.packet==render_arena_view(3) &&
       cursors.uv==render_arena_view(2) && cursors.scalar==render_arena_view(1))cursors=previous;
    memcpy(&phase,&surface->phase,4);put(a,0x59611c,phase);
    {const GameAddress cells[4]={frame.clip_vertices,frame.clip_packets,frame.clip_uv,frame.clip_indices};
        const uint32_t values[4]={cursors.vertex,cursors.packet,cursors.uv,cursors.scalar};for(i=0;i<4;++i)put(a,cells[i],values[i]);}
    put(a,frame.queue_head,head);
    if(!game_application_surface_clamp(a,1))result=0;
    return result && !a->memory.fault;
}
