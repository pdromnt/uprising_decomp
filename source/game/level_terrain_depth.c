#include "level_load.h"
#include "scene_projection_storage.h"
#include <string.h>
#include <stdio.h>
#define DEPTH_BLOCKS 1024u
#define DEPTH_BLOCK_BYTES 128u
static unsigned depth_block(unsigned x,unsigned z){return (x>>4)*32+(z>>4);}
static unsigned depth_byte(unsigned x,unsigned z){return (x&15)*8+((z&15)>>1);}
static unsigned depth_get(const uint8_t *bytes,const uint8_t present[DEPTH_BLOCKS],unsigned x,unsigned z){
    unsigned block=depth_block(x,z);if(!present[block])return 0;
    return (bytes[block*DEPTH_BLOCK_BYTES+depth_byte(x,z)]>>((z&1)*4))&15;
}
static void depth_set(uint8_t *bytes,unsigned x,unsigned z,unsigned value){
    uint8_t *byte=bytes+depth_block(x,z)*DEPTH_BLOCK_BYTES+depth_byte(x,z);unsigned shift=(z&1)*4;
    *byte=(uint8_t)((*byte&~(15u<<shift))|(value<<shift));
}
/*50e050: block publications borrow one packed allocation, not32-bit host
 *pointers or1024 independent owners. Dispose even with a latched earlier error. */
int game_level_terrain_depth_end(GameApplication *a){
    GameAddress owned,saved_address;size_t saved_size;int saved_fault,saved_write,ok=1;unsigned i;
    if(!a)return 0;
    owned=a->terrain_depth;if(!owned)return 1;
    saved_fault=a->memory.fault;saved_address=a->memory.fault_address;saved_size=a->memory.fault_size;saved_write=a->memory.fault_write;a->memory.fault=0;
    for(i=0;i<DEPTH_BLOCKS;i++){
        GameAddress p=game_memory_read_u32(&a->memory,0x625660+i*4);
        if(p==owned+i*DEPTH_BLOCK_BYTES)game_memory_write_u32(&a->memory,0x625660+i*4,0);
        else if(p)ok=0;
    }
    if(ok && !a->memory.fault && scene_projection_storage_free(owned))a->terrain_depth=0;else ok=0;
    if(saved_fault){a->memory.fault=saved_fault;a->memory.fault_address=saved_address;a->memory.fault_size=saved_size;a->memory.fault_write=saved_write;}
    return ok;
}
static int depth_cache_name(GameApplication *a,char path[512]){
    char name[256],*dot;unsigned i;int length;
    for(i=0;i<sizeof(name);i++){name[i]=(char)game_memory_read_u8(&a->memory,0x5acc90+i);if(!name[i])break;}
    if(i==sizeof(name) || a->memory.fault)return 0;
    length=snprintf(path,512,"grids\\depths\\%s",name);if(length<0 || length>=512)return 0;
    /*563190 is strchr: the original replaces the first dot, not the last. */
    dot=strchr(path,'.');if(!dot || (size_t)(dot-path)>507)return 0;
    memcpy(dot,".dph",5);return 1;
}
/*50e5e0: cached words are presence tags, never loadable pointers. Rebind each
 *nonempty block to its real native allocation and check the complete payload. */
static int depth_read_cache(const ResourceBlob *blob,uint8_t *bytes,uint8_t present[DEPTH_BLOCKS]){
    size_t cursor=DEPTH_BLOCKS*4;unsigned i;
    if(blob->size<cursor)return 0;
    for(i=0;i<DEPTH_BLOCKS;i++){
        const uint8_t *tag=blob->data+i*4;present[i]=(uint8_t)((tag[0]|tag[1]|tag[2]|tag[3])!=0);
        if(!present[i])continue;
        if(blob->size-cursor<DEPTH_BLOCK_BYTES)return 0;
        memcpy(bytes+i*DEPTH_BLOCK_BYTES,blob->data+cursor,DEPTH_BLOCK_BYTES);cursor+=DEPTH_BLOCK_BYTES;
    }
    return 1;
}
/*50e090/50dd50/50e790/50e7e0: terrain flags seed packed nibble15, followed
 *by the original four-neighbor distance passes1..14. Iterate in x/z order and
 *only consume neighbors below this pass, preserving its wavefront semantics.
 *Original empty detection overreads seven bytes after128; bound it to the owned
 *block. That undefined allocation-dependent behavior is not a parity claim. */
static int depth_generate(GameApplication *a,uint8_t *bytes,uint8_t present[DEPTH_BLOCKS]){
    GameAddress terrain=game_memory_read_u32(&a->memory,0x597d90);unsigned x,z,step;
    for(x=0;x<512;x++)for(z=0;z<512;z++){
        uint8_t flags=game_memory_read_u8(&a->memory,terrain+(z>>1)*2048+(x>>1)*8+5);
        if(flags&0x50){depth_set(bytes,x,z,15);present[depth_block(x,z)]=1;}
    }
    if(a->memory.fault)return 0;
    for(step=1;step<15;step++){
        int pending=0;
        for(x=0;x<512;x++)for(z=0;z<512;z++){
            if(depth_get(bytes,present,x,z)!=15)continue;
            if((x && depth_get(bytes,present,x-1,z)<step) || (x<511 && depth_get(bytes,present,x+1,z)<step) ||
               (z && depth_get(bytes,present,x,z-1)<step) || (z<511 && depth_get(bytes,present,x,z+1)<step))depth_set(bytes,x,z,step);
            else pending=1;
        }
        if(!pending)break;
    }
    return 1;
}
/*50dd50: load the actual .dph asset or reconstruct from the loaded terrain.
 *All executable work is ordinary C. Cached input stays read-only; no retail
 *cache is rewritten. Canonical consumers borrow bounded fixed-width block IDs. */
int game_level_terrain_depth_begin(GameApplication *a){
    uint8_t present[DEPTH_BLOCKS]={0},*bytes;ResourceBlob blob={0};char path[512];GameAddress owned;unsigned i;int ok;
    if(!a || a->memory.fault || !game_level_terrain_depth_end(a) || !depth_cache_name(a,path))return 0;
    owned=scene_projection_storage_allocate(DEPTH_BLOCKS*DEPTH_BLOCK_BYTES,"terrain depth grid");if(!owned)return 0;
    a->terrain_depth=owned;bytes=scene_projection_storage_borrow_bytes(owned,DEPTH_BLOCKS*DEPTH_BLOCK_BYTES);
    if(!bytes)goto fail;
    memset(bytes,0,DEPTH_BLOCKS*DEPTH_BLOCK_BYTES);
    if(game_application_load_blob(a,path,&blob))ok=depth_read_cache(&blob,bytes,present);
    else ok=!a->memory.fault && depth_generate(a,bytes,present);
    resource_blob_release(&blob);if(!ok)goto fail;
    for(i=0;i<DEPTH_BLOCKS;i++)game_memory_write_u32(&a->memory,0x625660+i*4,present[i]?owned+i*DEPTH_BLOCK_BYTES:0);
    if(!a->memory.fault)return 1;
fail:
    (void)game_level_terrain_depth_end(a);return 0;
}
