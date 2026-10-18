#include "glide_texture_cache.h"
#include "scene_projection_storage.h"
#include <string.h>
static uint32_t get(GameMemory *m,GameAddress p){return game_memory_read_u32(m,p);}
static void put(GameMemory *m,GameAddress p,uint32_t v){game_memory_write_u32(m,p,v);}
/*559480: the existing32-byte texture records and16-byte VRAM spans have one
 *native owner. Their legacy globals hold bounded IDs, including aligned views. */
int game_glide_texture_cache_begin(GameMemory *m,const GameGlideTextureServices *s){
    uint32_t first,last,bytes;GameAddress records,nodes,aligned_records,aligned_nodes;unsigned i;
    if(!s || !s->range || !s->statistics || !s->upload || get(m,0x63a168) || get(m,0x63a164) ||
       !s->range(s->context,&first,&last) || last<first)return 0;
    bytes=last-first;if(bytes>0x200000)bytes=0x200000;
    if(!bytes)return 0;
    records=scene_projection_storage_allocate(0x2001f,"Glide texture records");if(!records)return 0;
    nodes=scene_projection_storage_allocate(0x1001f,"Glide texture spans");
    if(!nodes){scene_projection_storage_free(records);return 0;}
    aligned_records=(records+31u)&~31u;aligned_nodes=(nodes+31u)&~31u;
    game_memory_fill_u8(m,records,0x2001f,0,0);game_memory_fill_u8(m,nodes,0x1001f,0,0);
    put(m,aligned_nodes,aligned_nodes);put(m,aligned_nodes+8,first);put(m,aligned_nodes+12,bytes);
    for(i=16;i<0xfff0;i+=16)put(m,aligned_nodes+i,aligned_nodes+i+16);
    if(m->fault){scene_projection_storage_free(nodes);scene_projection_storage_free(records);return 0;}
    put(m,0x63a168,records);put(m,0x63a164,nodes);put(m,0x63a184,aligned_records);
    put(m,0x63a158,aligned_records);put(m,0x63a160,aligned_nodes);put(m,0x63a15c,aligned_nodes);
    put(m,0x63a150,aligned_nodes+16);put(m,0x63a154,1);return !m->fault;
}
/*559590: bit4 distinguishes borrowed pixels from owned pixels/auxiliary data.
 *GPU spans borrow records; repeated close leaves external data owners intact. */
int game_glide_texture_cache_end(GameMemory *m){
    GameAddress records=get(m,0x63a168),nodes=get(m,0x63a164),p=get(m,0x63a184),end=get(m,0x63a158);
    static const GameAddress slots[]={0x63a168,0x63a164,0x63a184,0x63a158,0x63a160,0x63a15c,0x63a150};unsigned i;
    if(records){
        if(end<p || end-p>0x20000 || ((end-p)&31))return 0;
        for(;p<end;p+=32){GameAddress data=get(m,p+20),aux=get(m,p+24);
            if(!(get(m,p)&4u)){
                if(data && !scene_projection_storage_free(data))return 0;
                if(aux && !scene_projection_storage_free(aux))return 0;
            }
            put(m,p+20,0);put(m,p+24,0);
        }
        if(!scene_projection_storage_free(records))return 0;
    }
    if(nodes && !scene_projection_storage_free(nodes))return 0;
    for(i=0;i<sizeof(slots)/sizeof(slots[0]);++i)put(m,slots[i],0);
    return !m->fault;
}
/*The texture loader publishes directly into the existing cache owner. The
 *payload copy has the same lifetime as its record; no mutable serialized twin. */
GameAddress game_glide_texture_create(GameMemory *m,uint32_t flags,const uint8_t format[4],const uint8_t *pixels,uint32_t bytes){
    GameAddress p=get(m,0x63a158),base=get(m,0x63a184),data;
    if(!get(m,0x63a168) || !format || !pixels || !bytes || bytes>0x200000 || p<base || p-base>=0x20000)return 0;
    data=scene_projection_storage_allocate(bytes,"Glide texture payload");if(!data)return 0;
    if(!game_memory_write(m,data,pixels,bytes) || !game_memory_write(m,p+16,format,4)){
        scene_projection_storage_free(data);return 0;
    }
    put(m,p,flags&0x7ffffffbu);put(m,p+4,0);put(m,p+8,0);put(m,p+12,bytes);put(m,p+20,data);
    put(m,0x63a158,p+32);return m->fault?0:p;
}
/*Original bit4 distinguishes a borrowed payload. It remains with its external
 *owner through cache retirement; only the cache record is owned here. */
GameAddress game_glide_texture_borrow(GameMemory *m,uint32_t flags,const uint8_t format[4],GameAddress data,uint32_t bytes){
    GameAddress p=get(m,0x63a158),base=get(m,0x63a184);
    if(!get(m,0x63a168) || !format || !bytes || bytes>0x200000 || p<base || p-base>=0x20000 ||
       !scene_projection_storage_borrow_bytes(data,bytes) || !game_memory_write(m,p+16,format,4))return 0;
    put(m,p,(flags&0x7fffffffu)|4u);put(m,p+4,0);put(m,p+8,0);put(m,p+12,bytes);put(m,p+20,data);
    put(m,0x63a158,p+32);return m->fault?0:p;
}
int game_glide_texture_cache_frame(GameMemory *m,const GameGlideTextureServices *s){
    uint32_t statistics[5];unsigned i;
    if(!get(m,0x63a168) || !s || !s->statistics || !s->statistics(s->context,statistics))return 0;
    put(m,0x63a154,get(m,0x63a154)+1);
    for(i=0;i<5;++i)put(m,0x63a170+i*4,statistics[i]);
    put(m,0x5a07f4,0);put(m,0x5a07f8,0);put(m,0x5a07fc,0);return !m->fault;
}
/*559630 and its callers: touch the frame, reuse resident data, or find a
 *contiguous span outside this/previous frame. Circular order, address wrap,
 *coalescing, eviction bits and split remainder follow the original allocator. */
int game_glide_texture_use(GameMemory *m,GameAddress texture,const GameGlideTextureServices *s){
    GameAddress base=get(m,0x63a184),end=get(m,0x63a158),start=get(m,0x63a15c),node=start,run=0,last,owner,p,free_node;
    uint32_t extent=0,previous_address,needed,frame=get(m,0x63a154),flags;unsigned visits=0;uint8_t format[4];
    TextureRecord *record=s && s->native_record?s->native_record(s->context,texture):NULL;
    if(!s || !s->range || !start || (!record && (!s->upload || texture<base || texture>=end || ((texture-base)&31))))return 0;
    if(record){if(!s->upload_owned || record->image.size>UINT32_MAX || !record->image.size)return 0;
        flags=record->flags;record->last_use=frame;}
    else{flags=get(m,texture);put(m,texture+4,frame);}
    if(flags&0x80000000u)return !m->fault;
    needed=record?(uint32_t)record->image.size:get(m,texture+12);
    /*559630's record size is the device reservation. Tiny native images
     *retain their logical pixel payload but Glide reserves at least8 bytes. */
    if(record && needed<8u)needed=8u;
    previous_address=get(m,node+8);
    do {
        uint32_t address=get(m,node+8);owner=get(m,node+4);
        if(previous_address>address)extent=0;
        {TextureRecord *cached=owner && s->native_record?s->native_record(s->context,owner):NULL;
        if(owner && (cached?cached->last_use:get(m,owner+4))>=frame-1u)extent=0;
        else {if(!extent)run=node;extent+=get(m,node+12);if(extent>=needed)break;}
        }
        previous_address=address;node=get(m,node);
        if(!node || ++visits>4096)return 0;
    }while(node!=start);
    if(extent<needed){uint32_t first,maximum;
        if(!s->range(s->context,&first,&maximum))return 0;
        if(record)record->address=first;else put(m,texture+8,first);
        return !m->fault; /*Original leaves residency clear. */
    }
    /*The device binding is checked before publication; pixel data stays owned
     *by the record throughout upload. Native SDK pointers never enter records. */
    if(record){if(!s->upload_owned(s->context,get(m,run+8),&record->image))return 0;}
    else if(!game_memory_read(m,texture+16,format,4) ||
       !s->upload(s->context,get(m,run+8),format,get(m,texture+20),needed))return 0;
    last=get(m,node);p=run;visits=0;
    do {owner=get(m,p+4);if(owner){TextureRecord *cached=s->native_record?s->native_record(s->context,owner):NULL;
            if(cached)cached->flags&=0x7fffffffu;else put(m,owner,get(m,owner)&0x7fffffffu);}
        p=get(m,p);if(!p || ++visits>4096)return 0;
    }while(p!=last);
    put(m,run+4,texture);put(m,run+12,needed);
    if(run!=node){p=get(m,run);put(m,run,last);put(m,node,get(m,0x63a150));put(m,0x63a150,p);}
    if(extent>needed){
        free_node=get(m,0x63a150);if(!free_node)return 0;
        p=get(m,free_node);put(m,free_node,get(m,run));put(m,free_node+4,0);
        put(m,free_node+12,extent-needed);put(m,free_node+8,get(m,run+8)+needed);
        put(m,run,free_node);put(m,0x63a150,p);
    }
    put(m,0x63a15c,get(m,run));
    if(record){record->address=get(m,run+8);record->flags=flags|0x80000000u;}
    else{put(m,texture+8,get(m,run+8));put(m,texture,flags|0x80000000u);}
    put(m,0x5a07f4,get(m,0x5a07f4)+needed);return !m->fault;
}
/*A VRAM span borrows its texture identity. Detach it before that owner is
 *destroyed, so later eviction never looks up a retired native record. */
int game_glide_texture_forget(GameMemory *m,GameAddress texture){
    GameAddress start=get(m,0x63a15c),node=start;unsigned visits=0;
    if(!start)return !m->fault;
    do{if(get(m,node+4)==texture)put(m,node+4,0);
        node=get(m,node);if(!node || ++visits>4096)return 0;
    }while(node!=start && !m->fault);
    return !m->fault;
}
