#include "render_arena.h"
#include <stdlib.h>
#include <string.h>
static RenderArena arena;static RenderArenaStats stats;
struct RenderArenaPaletteMap {uint32_t key;uint8_t entries[4];};
static const uint32_t sizes[4]={160000,180000,360000,600000};
static void refresh_packet_link(uint32_t index);
RenderArena *render_arena_current(void){return stats.live?&arena:NULL;}
uint32_t render_arena_view(unsigned bank){return bank<4?RENDER_ARENA_BASE+bank*0x100000u:0;}
void render_arena_stats(RenderArenaStats *out){*out=stats;}
void render_arena_release(void){
    if(!stats.live)return;
    free(arena.vertices);free(arena.indices);free(arena.uv);free(arena.packets);
    free(arena.packet_links);
    free(arena.motion_keys);
    free(arena.palette_maps);
    memset(&arena,0,sizeof(arena));stats.live=0;++stats.releases;
}
int render_arena_initialize(void){
    RenderArena next={0};next.vertices=calloc(10000,sizeof(*next.vertices));next.indices=calloc(45000,sizeof(*next.indices));
    next.uv=calloc(45000,sizeof(*next.uv));next.packets=calloc(15000,sizeof(*next.packets));
    next.packet_links=calloc(15000,sizeof(*next.packet_links));
    next.motion_keys=calloc(10000,sizeof(*next.motion_keys));
    if(!next.vertices || !next.indices || !next.uv || !next.packets || !next.packet_links || !next.motion_keys){
        free(next.vertices);free(next.indices);free(next.uv);free(next.packets);
        free(next.packet_links);free(next.motion_keys);return 0;}
    render_arena_release();arena=next;stats.live=1;++stats.loads;return 1;
}
void render_arena_begin_frame(void){
    /* World queues can be drawn after the scene frame that produced them.
     * Palette snapshots therefore live until arena teardown or key refresh. */
    if(stats.live){++stats.frames;memset(arena.motion_keys,0,10000*sizeof(*arena.motion_keys));}
}
int render_arena_palette_register(uint32_t key,const uint8_t entries[4]){
    size_t i;
    if(!stats.live || !key || !entries)return 0;
    for(i=0;i<arena.palette_count;i++)
        if(arena.palette_maps[i].key==key)break;
    if(i==arena.palette_count){
        if(arena.palette_count==arena.palette_capacity){
            size_t capacity;
            RenderArenaPaletteMap *grown;
            if(arena.palette_capacity>SIZE_MAX/2u)return 0;
            capacity=arena.palette_capacity?arena.palette_capacity*2u:16u;
            if(capacity>SIZE_MAX/sizeof(*grown))return 0;
            grown=realloc(arena.palette_maps,capacity*sizeof(*grown));
            if(!grown)return 0;
            arena.palette_maps=grown;arena.palette_capacity=capacity;
        }
        arena.palette_maps[i].key=key;
        ++arena.palette_count;
    }
    memcpy(arena.palette_maps[i].entries,entries,4u);
    ++stats.palette_captures;
    return 1;
}
int render_arena_palette_lookup(uint32_t key,uint8_t entries[4]){
    size_t i;
    if(!stats.live || !key || !entries)return 0;
    for(i=0;i<arena.palette_count;i++)
        if(arena.palette_maps[i].key==key){
            memcpy(entries,arena.palette_maps[i].entries,4u);
            ++stats.palette_reads;
            return 1;
        }
    return 0;
}
static int location(uint32_t address,size_t size,unsigned *bank,uint32_t *offset){
    if(!stats.live || address<RENDER_ARENA_BASE || address>=RENDER_ARENA_END)return 0;
    *bank=(address-RENDER_ARENA_BASE)/0x100000u;*offset=(address-RENDER_ARENA_BASE)%0x100000u;
    return *offset<=sizes[*bank] && size<=sizes[*bank]-*offset;
}
/* Explicit little-endian views for remaining address-based render consumers.
 * Float bits are transferred through memcpy, never native-pointer truncation. */
static void *word(unsigned bank,uint32_t index){
    if(bank==0){RenderArenaVertex *v=&arena.vertices[index/4];return index%4==3?&v->light:&v->position[index%4];}
    if(bank==1)return &arena.indices[index];
    if(bank==2){RenderArenaUv *uv=&arena.uv[index/2];return index%2?&uv->v:&uv->u;}
    {RenderArenaPacket *p=&arena.packets[index/10];switch(index%10){
        case 0:return &p->count;case 1:return &p->vertices;case 2:return &p->flags;
        case 3:return &p->next;case 4:return &p->attributes;case 5:return &p->material;
        default:return &p->extra[index%10-6];}}
}
int render_arena_read(uint32_t address,void *output,size_t size){
    unsigned bank;uint32_t offset,bits;size_t i;uint8_t *bytes=output;
    if(!output || !location(address,size,&bank,&offset))return 0;
    for(i=0;i<size;i++){uint32_t at=offset+(uint32_t)i;memcpy(&bits,word(bank,at/4),4);bytes[i]=(uint8_t)(bits>>((at%4)*8));}
    ++stats.reads;return 1;
}
int render_arena_write(uint32_t address,const void *input,size_t size){
    unsigned bank;uint32_t offset,bits;size_t i;const uint8_t *bytes=input;
    if(!input || !location(address,size,&bank,&offset))return 0;
    for(i=0;i<size;i++){uint32_t at=offset+(uint32_t)i,shift=(at%4)*8;void *field=word(bank,at/4);
        memcpy(&bits,field,4);bits=(bits&~(255u<<shift))|((uint32_t)bytes[i]<<shift);memcpy(field,&bits,4);}
    if(bank==3u && size){
        uint32_t first=offset/40u,last=(offset+(uint32_t)size-1u)/40u,index;
        for(index=first;index<=last;index++){
            uint32_t link=index*40u+12u;
            if(offset<link+4u && offset+size>link)refresh_packet_link(index);
        }
    }
    ++stats.writes;return 1;
}

static int span(uint32_t reference,unsigned bank,size_t count,unsigned stride,uint32_t *index){
    uint32_t offset=reference-render_arena_view(bank);
    if(!stats.live || offset>sizes[bank] || offset%stride || count>(sizes[bank]-offset)/stride)return 0;
    *index=offset/stride;return 1;
}
RenderArenaVertex *render_arena_vertices(uint32_t reference,size_t count){uint32_t i;return span(reference,0,count,16,&i)?arena.vertices+i:NULL;}
uint64_t render_arena_motion_key(uint32_t reference){uint32_t i;return span(reference,0,1,16,&i)?arena.motion_keys[i]:0;}
uint32_t *render_arena_indices(uint32_t reference,size_t count){uint32_t i;return span(reference,1,count,4,&i)?arena.indices+i:NULL;}
RenderArenaUv *render_arena_uv(uint32_t reference,size_t count){uint32_t i;return span(reference,2,count,8,&i)?arena.uv+i:NULL;}
RenderArenaPacket *render_arena_packets(uint32_t reference,size_t count){uint32_t i;return span(reference,3,count,40,&i)?arena.packets+i:NULL;}
static void refresh_packet_link(uint32_t index){
    uint32_t next=arena.packets[index].next;
    arena.packet_links[index].next_ref=next;
    arena.packet_links[index].next_owned=render_arena_packets(next,1u);
}
int render_arena_packet_set_next(uint32_t packet_ref,uint32_t next_ref){
    RenderArenaPacket *packet=render_arena_packets(packet_ref,1u);
    if(!packet)return 0;
    packet->next=next_ref;
    refresh_packet_link((uint32_t)(packet-arena.packets));
    return 1;
}
int render_arena_packet_link(uint32_t packet_ref,uint32_t *next_ref,RenderArenaPacket **next_owned){
    RenderArenaPacket *packet=render_arena_packets(packet_ref,1u);
    RenderArenaPacketLink *link;
    if(!packet || !next_ref || !next_owned)return 0;
    link=&arena.packet_links[packet-arena.packets];
    *next_ref=link->next_ref;*next_owned=link->next_owned;
    if(link->next_owned)++stats.native_link_reads;
    return 1;
}

float *render_arena_float_component(uint32_t reference){
    RenderArenaVertex *vertex;RenderArenaUv *uv;
    if(reference%4)return NULL;
    vertex=render_arena_vertices(reference&~15u,1);
    if(vertex)return (reference&15u)==12u?&vertex->light:&vertex->position[(reference&15u)/4u];
    uv=render_arena_uv(reference&~7u,1);return uv?((reference&7u)?&uv->v:&uv->u):NULL;
}
