#include "sound_samples.h"
#include <string.h>

static uint16_t wave_u16(const uint8_t *p){return (uint16_t)((uint16_t)p[0]|(uint16_t)p[1]<<8);}
static uint32_t wave_u32(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
int sound_wave_read(const ResourceBlob *blob,SoundWave *wave){
    size_t cursor=12u,end;int format=0,data=0;SoundWave result={0};
    if(!blob || !blob->data || blob->size<12u || wave_u32(blob->data)!=0x46464952u || wave_u32(blob->data+8u)!=0x45564157u)return 0;
    end=(size_t)wave_u32(blob->data+4u)+8u;
    if(end<12u || end>blob->size)return 0;
    while(cursor<end){
        const uint8_t *chunk;uint32_t id,size;size_t padded;
        if(end-cursor<8u)return 0;
        id=wave_u32(blob->data+cursor);size=wave_u32(blob->data+cursor+4u);cursor+=8u;
        if(size>end-cursor)return 0;
        chunk=blob->data+cursor;
        if(id==0x20746d66u && !format){
            if(size<16u)return 0;
            result.format_data=chunk;result.format_bytes=size;
            result.format=wave_u16(chunk);result.channels=wave_u16(chunk+2u);
            result.frequency=wave_u32(chunk+4u);result.bytes_per_second=wave_u32(chunk+8u);
            result.block_alignment=wave_u16(chunk+12u);result.bits_per_sample=wave_u16(chunk+14u);format=1;
        }else if(id==0x61746164u && !data){result.pcm=chunk;result.pcm_bytes=size;data=1;}
        if(format && data){*wave=result;return 1;}
        padded=(size_t)size+(size&1u);if(padded>end-cursor)return 0;cursor+=padded;
    }
    return 0;
}
void sound_bank_initialize(GameMemory *m,GameAddress bank,GameAddress device){
    unsigned i;
    for(i=0;i<276u;i++)game_memory_write_u32(m,bank+12u+i*4u,0u);
    for(i=0;i<9u;i++)game_memory_write_u16(m,bank+0x46cu+i*2u,0xffffu);
    game_memory_write_u32(m,bank+0x45cu,device);
    game_memory_write_u32(m,bank+0x460u,0x59c68cu);
    game_memory_write_u32(m,bank+0x468u,device!=0u);
}
int sound_sample_release(GameMemory *m,GameAddress sample,const SoundSampleServices *s){
    GameAddress table=game_memory_read_u32(m,sample+12u);uint32_t count=game_memory_read_u32(m,sample+8u),i;
    if(m->fault || count>INT32_MAX)return 0;
    for(i=0;i<count && !m->fault;i++){
        GameAddress buffer=game_memory_read_u32(m,table+i*4u);
        if(buffer && !s->release_buffer(s->context,buffer))return 0;
        game_memory_write_u32(m,table+i*4u,0u);
    }
    if(m->fault || (table && !s->free_record(s->context,table)))return 0;
    game_memory_write_u32(m,sample+12u,0u);game_memory_write_u32(m,sample+8u,0u);
    return !m->fault;
}
int sound_sample_initialize(GameMemory *m,GameAddress sample,GameAddress bank,uint32_t voices,float radius,const ResourceBlob *blob,const SoundSampleServices *s){
    SoundWave wave;GameAddress table,device,first;uint32_t i,bits;float range=(float)((long double)radius*radius);
    if(!voices || voices>UINT32_MAX/4u || voices>INT32_MAX || !sound_wave_read(blob,&wave))return 0;
    device=game_memory_read_u32(m,bank+0x45cu);if(m->fault || !device)return 0;
    table=s->allocate(s->context,voices*4u);if(!table)return 0;
    for(i=0;i<voices;i++)game_memory_write_u32(m,table+i*4u,0u);
    /* Retail left +0 pointing into the freed file. Its consumers use the byte
     * count, frequency and buffer IDs; clear that dangling publication. */
    game_memory_write_u32(m,sample,0u);game_memory_write_u32(m,sample+4u,wave.pcm_bytes);
    game_memory_write_u32(m,sample+8u,voices);game_memory_write_u32(m,sample+12u,table);
    game_memory_write_u32(m,sample+16u,wave.frequency);game_memory_write_u32(m,sample+20u,bank);
    memcpy(&bits,&range,4u);game_memory_write_u32(m,sample+24u,bits);
    if(m->fault)goto fail;
    first=s->create_buffer(s->context,device,&wave);if(!first)goto fail;
    game_memory_write_u32(m,table,first);
    for(i=1;i<voices && !m->fault;i++){
        GameAddress buffer=s->duplicate_buffer(s->context,device,first);
        if(!buffer)buffer=s->create_buffer(s->context,device,&wave);
        if(!buffer)goto fail;
        game_memory_write_u32(m,table+i*4u,buffer);
    }
    if(!m->fault){uint32_t frequency=wave.frequency;
        if(s->frequency(s->context,first,&frequency)==0)game_memory_write_u32(m,sample+16u,frequency);
    }
    if(!m->fault)return 1;
fail:
    if(!m->fault)(void)sound_sample_release(m,sample,s);
    return 0;
}
int sound_bank_unload(GameMemory *m,GameAddress bank,int category,const SoundSampleServices *s){
    unsigned i;
    for(i=0;i<276u && !m->fault;i++){
        GameAddress sample=game_memory_read_u32(m,bank+12u+i*4u);
        if(sample && (category<0 || game_memory_read_u8(m,0x59ac64u+i*16u)==(uint32_t)category)){
            if(!sound_sample_release(m,sample,s) || !s->free_record(s->context,sample))return 0;
            game_memory_write_u32(m,bank+12u+i*4u,0u);
        }
    }
    if(category<0 && !m->fault){
        GameAddress device=game_memory_read_u32(m,bank+0x45cu);
        if(device && !s->release_device(s->context,device))return 0;
        game_memory_write_u32(m,bank+0x45cu,0u);game_memory_write_u32(m,bank+0x468u,0u);
    }
    return !m->fault;
}
