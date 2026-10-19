#ifndef UPRISING_RENDER_ARENA_H
#define UPRISING_RENDER_ARENA_H
#include <stdint.h>
#include <stddef.h>
#define RENDER_ARENA_BASE 0xf9000000u
#define RENDER_ARENA_END 0xf9400000u
typedef struct RenderArenaVertex {float position[3],light;} RenderArenaVertex;
typedef struct RenderArenaMotionClip {uint32_t previous,current;float fraction;int screen_space;} RenderArenaMotionClip;
typedef struct RenderArenaUv {float u,v;} RenderArenaUv;
typedef struct RenderArenaPacket {uint32_t count,vertices,flags,next,attributes,material,extra[4];} RenderArenaPacket;
typedef struct RenderArenaPacketLink {RenderArenaPacket *next_owned;uint32_t next_ref;} RenderArenaPacketLink;
typedef struct RenderArenaPaletteMap RenderArenaPaletteMap;
typedef struct RenderArena {RenderArenaVertex *vertices;uint64_t *motion_keys;RenderArenaMotionClip *motion_clips;uint32_t *indices;RenderArenaUv *uv;RenderArenaPacket *packets;RenderArenaPacketLink *packet_links;RenderArenaPaletteMap *palette_maps;size_t palette_count,palette_capacity;} RenderArena;
typedef struct RenderArenaStats {uint32_t loads,releases,frames,live,reads,writes,palette_captures,palette_reads,native_link_reads;} RenderArenaStats;
int render_arena_initialize(void);
void render_arena_release(void);
void render_arena_begin_frame(void);
RenderArena *render_arena_current(void);
void render_arena_stats(RenderArenaStats *);
uint32_t render_arena_view(unsigned bank);
int render_arena_read(uint32_t,void *,size_t);
int render_arena_write(uint32_t,const void *,size_t);
RenderArenaVertex *render_arena_vertices(uint32_t reference,size_t count);
uint64_t render_arena_motion_key(uint32_t reference);
const RenderArenaMotionClip *render_arena_motion_clip(uint32_t reference);
void render_arena_record_motion_clip(uint32_t reference,uint32_t previous,uint32_t current,float fraction,int screen_space);
uint32_t *render_arena_indices(uint32_t reference,size_t count);
RenderArenaUv *render_arena_uv(uint32_t reference,size_t count);
RenderArenaPacket *render_arena_packets(uint32_t reference,size_t count);
/* Packet links are owned by the arena; next_ref is its x86 projection. */
int render_arena_packet_set_next(uint32_t packet_ref,uint32_t next_ref);
int render_arena_packet_link(uint32_t packet_ref,uint32_t *next_ref,RenderArenaPacket **next_owned);
float *render_arena_float_component(uint32_t reference);
int render_arena_palette_register(uint32_t key,const uint8_t entries[4]);
int render_arena_palette_lookup(uint32_t key,uint8_t entries[4]);
#endif
