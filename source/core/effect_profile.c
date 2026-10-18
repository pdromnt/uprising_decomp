#include "effect_profile.h"
#include "scene_projection_storage.h"
#include "gameplay_state.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
static uint32_t word(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8u|(uint32_t)p[2]<<16u|(uint32_t)p[3]<<24u;}
static void real(GameMemory *m,GameAddress a,float value){uint32_t b;memcpy(&b,&value,4u);game_memory_write_u32(m,a,b);}
int effect_profile_construct(GameMemory *m,GameAddress object,const EffectProfileRequest *r,const EffectProfileServices *ports){
    ResourceBlob sheet={0};GameAddress table=0;char path[512];uint32_t width=0,height=0,stride=0,texture_flags=0x14u;
    uint8_t flags=r->flags;unsigned d,f;int loaded,result=0;uint64_t count;
    if(!object || !r->name || r->frames<=0 || r->directions<=0)return 0;
    count=(uint64_t)(uint16_t)r->frames*(uint16_t)r->directions;
    if(count>UINT32_MAX/4u)return 0;
    if(!game_memory_read_u32(m,0x596fb0u))flags&=0xf3u;
    game_memory_write_u16(m,object+4u,(uint16_t)r->frames);game_memory_write_u16(m,object+6u,(uint16_t)r->directions);
    game_memory_write_u32(m,object+8u,(uint32_t)r->duration);game_memory_write_u8(m,object+0x24u,flags);
    if(r->directions==1 || r->directions==2 || r->directions==4 || r->directions==8){
        unsigned shift=r->directions==1?11u:r->directions==2?10u:r->directions==4?9u:8u;
        game_memory_write_u16(m,object+0x20u,(uint16_t)shift);
        game_memory_write_u16(m,object+0x22u,(uint16_t)(r->directions==1?0u:1024u/(unsigned)r->directions));
    }
    real(m,object+0x1cu,(float)((long double)r->frames/r->duration));
    if(flags&4u)texture_flags=0x15u;
    if(flags&0x10u)texture_flags|=8u;else if(flags&0x20u)texture_flags|=0x20u;
    table=scene_projection_storage_allocate((size_t)count*4u,"effect profile frames");if(!table)return 0;
    game_memory_write_u32(m,object,table);game_memory_write_u32(m,object+0x2cu,0u);
    if(snprintf(path,sizeof(path),"media\\sprites\\%s.sqc",r->name)<0)goto done;
    loaded=ports->load(ports->context,path,&sheet);
    if(loaded<0)goto done;
    if(loaded){uint64_t payload;
        if(sheet.size<16u)goto done;
        width=word(sheet.data);height=word(sheet.data+4u);stride=word(sheet.data+12u);
        payload=(uint64_t)word(sheet.data+8u)*stride;
        if(!width || !height || (uint64_t)width*height>stride || payload>sheet.size-16u ||
           (uint64_t)((unsigned)r->directions/2u+1u)*(unsigned)r->frames*stride>payload)goto done;
    }
    for(d=0;d<(unsigned)r->directions;d++)for(f=0;f<(unsigned)r->frames;f++){
        uint32_t texture;
        if(d>(unsigned)r->directions-d){texture=game_memory_read_u32(m,table+(((unsigned)r->directions-d)*(unsigned)r->frames+f)*4u);}
        else if(loaded){texture=ports->texture(ports->context,sheet.data+16u+((size_t)d*(unsigned)r->frames+f)*stride,width,height,texture_flags);}
        else{
            ResourceBlob frame={0};AssetBitmap bitmap;int status;
            if(snprintf(path,sizeof(path),"sprites\\%s%u%u.bmp",r->name,d,f)<0)goto done;
            status=ports->load(ports->context,path,&frame);
            if(status<=0 || !asset_bitmap8_parse(&frame,&bitmap)){resource_blob_release(&frame);goto done;}
            if(width && (width!=bitmap.width || height!=bitmap.height)){resource_blob_release(&frame);goto done;}
            width=bitmap.width;height=bitmap.height;
            texture=ports->texture(ports->context,bitmap.pixels,width,height,texture_flags);
            resource_blob_release(&frame);
        }
        /* Failed individual texture creation leaves the original zero handle. */
        game_memory_write_u32(m,table+(d*(unsigned)r->frames+f)*4u,texture);
        if(m->fault)goto done;
    }
    game_memory_write_u32(m,object+0xcu,width);game_memory_write_u32(m,object+0x10u,height);
    real(m,object+0x18u,r->scale);real(m,object+0x14u,(float)((long double)r->scale/width*height));
    result=!m->fault;
done:
    resource_blob_release(&sheet);
    if(!result){scene_projection_storage_free(table);if(!m->fault)game_memory_write_u32(m,object,0u);}
    return result;
}
GameAddress effect_profile_create(GameMemory *m,const EffectProfileRequest *r,const EffectProfileServices *ports){
    GameAddress object=scene_projection_storage_allocate(0x30u,"effect profile");
    if(object && !effect_profile_construct(m,object,r,ports)){scene_projection_storage_free(object);return 0;}return object;
}
int effect_profile_release(GameMemory *m,GameAddress object){
    GameAddress table=game_memory_read_u32(m,object);
    if(m->fault || (table && !scene_projection_storage_free(table)))return 0;
    game_memory_write_u32(m,object,0u);game_memory_write_u32(m,object+0x2cu,0u);return !m->fault;
}
int effect_profile_sample(GameMemory *m,GameAddress profile,uint16_t direction,uint32_t ticks,EffectProfileSample *sample,int *visible){
    uint16_t bearing=(uint16_t)((direction+game_memory_read_u16(m,profile+0x22u))&2047u);
    unsigned shift=game_memory_read_u8(m,profile+0x20u)&31u;int32_t group=shift>=16u?0:(int32_t)(bearing>>shift);
    int32_t count=(int16_t)game_memory_read_u16(m,profile+4u),frame;uint32_t bits;int64_t integer;float rate;long double value;
    *visible=0;sample->flags=game_memory_read_u8(m,profile+0x24u);bits=game_memory_read_u32(m,profile+0x1cu);memcpy(&rate,&bits,4u);
    value=(long double)ticks*rate;integer=!isfinite(value) || value< -0x1p63L || value>=0x1p63L?INT64_MIN:(int64_t)value;
    bits=(uint32_t)(uint64_t)integer;memcpy(&frame,&bits,4u);if(m->fault)return 0;
    if(sample->flags&1u){if(!count || (frame==INT32_MIN && count==-1)){m->fault=1;return 0;}frame%=count;}
    else if(frame>=count){if(!(sample->flags&2u))return 1;frame=count-1;}
    sample->texture=game_memory_read_u32(m,game_memory_read_u32(m,profile)+((uint32_t)frame+(uint32_t)(group*count))*4u);
    if(m->fault){fprintf(stderr,"effect frame lookup failed: profile=%08x ticks=%u direction=%u group=%d frame=%d count=%d flags=%u rate=%g\n",
        profile,ticks,direction,group,frame,count,sample->flags,(double)rate);return 0;}
    count=(int16_t)game_memory_read_u16(m,profile+6u);sample->mirrored=count<=1?-1:count-group<group;
    *visible=1;return !m->fault;
}
int effect_profile_apply(GameMemory *m,GameAddress profile,GameAddress quad,uint16_t direction,uint32_t ticks,int *visible){
    EffectProfileSample sample;uint32_t flags;
    if(!effect_profile_sample(m,profile,direction,ticks,&sample,visible))return 0;
    if(!*visible)return 1;
    flags=game_memory_read_u32(m,quad+0xa0u);
    if(sample.flags&4u){flags|=0x20u;game_memory_write_u32(m,quad+0xb4u,0u);}else flags&=~0x20u;
    if(sample.flags&0x10u)flags=(flags|0x800u)&~8u;else flags&=~0x800u;
    if(sample.mirrored>=0){if(sample.mirrored)flags|=0x80000000u;else flags&=~0x80000000u;}
    game_memory_write_u32(m,quad+0x94u,sample.texture);game_memory_write_u32(m,quad+0xa0u,flags);return !m->fault;
}
static void read_name(GameMemory *m,GameAddress text,char name[256]){
    unsigned i;for(i=0;i<255u;i++){name[i]=(char)game_memory_read_u8(m,text+i);if(!name[i])break;}name[i]=0;
}
static int group_load(GameMemory *m,const GameplayRecordGroupSpec *g,uint8_t flags,const EffectProfileServices *ports){
    unsigned i;
    for(i=0;i<g->count;i++){
        EffectProfileRequest request={0};char name[256];GameAddress text,object;uint32_t o=i*g->stride;
        if(game_memory_read_u32(m,g->output+i*4u))continue;
        text=game_memory_read_u32(m,g->sources[0]+o);
        read_name(m,text,name);
        request.name=name;request.frames=(int16_t)game_memory_read_u16(m,g->sources[1]+o);
        request.directions=(int16_t)g->flag;request.duration=(int32_t)game_memory_read_u32(m,g->sources[2]+o);
        {uint32_t bits=game_memory_read_u32(m,g->sources[3]+o);memcpy(&request.scale,&bits,4u);}
        request.flags=flags;
        object=m->fault?0:effect_profile_create(m,&request,ports);if(!object)return 0;
        game_memory_write_u32(m,g->output+i*4u,object);if(m->fault)return 0;
    }return 1;
}
static int fragment_profiles_load(GameMemory *m,const EffectProfileServices *ports){
    GameplayRecordGroupSpec first=gameplay_state_record_group_spec(0u,game_memory_read_u32(m,0x5ac908u)==1u);
    GameplayRecordGroupSpec second=gameplay_state_record_group_spec(1u,0),last=gameplay_state_record_group_spec(3u,0);
    if(group_load(m,&first,1u,ports) && group_load(m,&second,1u,ports) && group_load(m,&last,1u,ports))return 1;
    if(!m->fault)effect_profile_family_release(m,5u);
    return 0;
}
static const GameplayRecordGroupSpec animation_profiles[]={
    {2u,4u,0x639ad0u,{0x5a03a0u,0x5a03a8u,0x5a03b0u,0x5a03b8u},1u,0u,0u},
    {1u,4u,0x639ae0u,{0x5a03c0u,0x5a03c4u,0x5a03c8u,0x5a03ccu},1u,0u,0u},
    {1u,4u,0x639abcu,{0x5a03d0u,0x5a03d4u,0x5a03d8u,0x5a03dcu},1u,0u,0u},
    {1u,4u,0x639ad8u,{0x5a03e0u,0x5a03e4u,0x5a03e8u,0x5a03ecu},1u,0u,0u},
    {1u,4u,0x639adcu,{0x5a03f0u,0x5a03f4u,0x5a03f8u,0x5a03fcu},1u,0u,0u},
    {1u,4u,0x639ae4u,{0x5a0400u,0x5a0404u,0x5a0408u,0x5a040cu},1u,0u,0u},
    {1u,4u,0x639ab8u,{0x5a0410u,0x5a0414u,0x5a0418u,0x5a041cu},1u,0u,0u}
};
static int release_profiles(GameMemory *m,GameAddress output,unsigned count){
    unsigned i;
    for(i=0;i<count;i++){GameAddress object=game_memory_read_u32(m,output+i*4u);
        if(!object)continue;
        if(!effect_profile_release(m,object) || !scene_projection_storage_free(object))return 0;
        game_memory_write_u32(m,output+i*4u,0u);if(m->fault)return 0;
    }return !m->fault;
}
int effect_profile_family_release(GameMemory *m,unsigned kind){
    unsigned g;
    if(kind==5u){for(g=0;g<4u;g++){GameplayRecordGroupSpec spec=gameplay_state_record_group_spec(g,0);if(!release_profiles(m,spec.output,spec.count))return 0;}return 1;}
    if(kind==0x41u){for(g=0;g<7u;g++){if(!release_profiles(m,animation_profiles[g].output,animation_profiles[g].count))return 0;}return 1;}
    if(kind==0x43u)return release_profiles(m,0x629768u,4u);
    if(kind==0x44u)return release_profiles(m,0x6020d8u,1u);
    return 0;
}
int effect_profile_family_load(GameMemory *m,unsigned kind,const EffectProfileServices *ports){
    unsigned i;
    if(kind==5u)return fragment_profiles_load(m,ports);
    if(kind==0x41u){static const uint8_t flags[7]={0x1cu,0x1cu,0x1cu,0x1cu,0xcu,0x1cu,0x14u};
        for(i=0;i<7u;i++)if(!group_load(m,animation_profiles+i,flags[i],ports))goto failed;
        return 1;
    }
    if(kind==0x43u){for(i=0;i<4u;i++){
        EffectProfileRequest request={0};char name[256];GameAddress object;
        if(game_memory_read_u32(m,0x629768u+i*4u))continue;
        read_name(m,game_memory_read_u32(m,0x59d470u+i*4u),name);request.name=name;
        request.frames=(int16_t)game_memory_read_u16(m,0x59d480u+i*2u);request.directions=1;
        request.duration=(int32_t)game_memory_read_u32(m,0x59d490u+i*4u);request.flags=game_memory_read_u8(m,0x59d488u+i);
        object=m->fault?0:effect_profile_create(m,&request,ports);if(!object)goto failed;
        game_memory_write_u32(m,0x629768u+i*4u,object);if(m->fault)goto failed;
        }
        gameplay_state_initialize_descriptor_defaults(m);
        game_memory_write_u32(m,0x629748u,0x40000000u);game_memory_write_u16(m,0x62973eu,0x14u);return !m->fault;
    }
    if(kind==0x44u){EffectProfileRequest request={0};char name[256];GameAddress object;
        if(game_memory_read_u32(m,0x6020d8u))return !m->fault;
        read_name(m,game_memory_read_u32(m,0x58de64u),name);request.name=name;
        request.frames=10;request.directions=1;request.duration=22;request.scale=200.0f;request.flags=5u;
        object=m->fault?0:effect_profile_create(m,&request,ports);if(!object)goto failed;
        game_memory_write_u32(m,0x6020d8u,object);return !m->fault;
    }
    return 0;
failed:
    if(!m->fault)effect_profile_family_release(m,kind);
    return 0;
}
