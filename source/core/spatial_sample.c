#include "spatial_sample.h"
#include "gameplay_geometry.h"
#include "game_matrix.h"
#include "effect_initialization.h"
#include "game_intrusive_list.h"
#include "attachment_effect.h"
#include "gameplay_leaves.h"
#include <string.h>
#include <math.h>
static float real_at(GameMemory *memory,GameAddress address){uint32_t bits=game_memory_read_u32(memory,address);float value;memcpy(&value,&bits,4u);return value;}
static void store_real(GameMemory *memory,GameAddress address,float value){uint32_t bits;memcpy(&bits,&value,4u);game_memory_write_u32(memory,address,bits);}
static int32_t signed_at(GameMemory *memory,GameAddress address){uint32_t bits=game_memory_read_u32(memory,address);int32_t value;memcpy(&value,&bits,4u);return value;}
static int32_t short_at(GameMemory *memory,GameAddress address){uint32_t bits=game_memory_read_u16(memory,address);return (int32_t)bits-((bits&0x8000u)?65536:0);}
int spatial_sample_request_unit(GameMemory *memory,GameAddress object,uint32_t x,uint32_t z,const SpatialUnitRequestServices *services,uint32_t *result){
    GameAddress descriptor,owner,unit=0u;uint32_t sequence;float position[3];
    if(!result)return 0;
    *result=1u;
    if(!(game_memory_read_u8(memory,object+0xb8u)&2u))return !memory->fault;
    descriptor=game_memory_read_u32(memory,object+0xb4u);if(!descriptor)return !memory->fault;
    if(!services || !services->create || !services->sequence || !services->place || !services->publish || !services->sound || !services->remove)return 0;
    owner=game_memory_read_u32(memory,game_memory_read_u32(memory,descriptor+4u)+0x54u);
    position[0]=(float)(uint64_t)x;position[2]=(float)(uint64_t)z;
    position[1]=(float)(gameplay_geometry_terrain_height(memory,position[0],position[2])+real_at(memory,0x575498u));
    if(memory->fault || !services->create(services->context,0x0bu,0u,&unit) || !unit)return 0;
    if(!services->sequence(services->context,owner,&sequence) || !services->place(services->context,unit,sequence,position) ||
       !services->publish(services->context,owner,unit)){
        if(services->retire)(void)services->retire(services->context,unit,1);
        return 0;
    }
    if(!services->sound(services->context,position))return 0;
    game_memory_write_u32(memory,object+0xc4u,0u);
    owner=game_memory_read_u32(memory,0x58973cu);
    if(memory->fault || !services->remove(services->context,owner,0x0bu,0))return 0;
    game_memory_write_u8(memory,object+0xb8u,(uint8_t)(game_memory_read_u8(memory,object+0xb8u)&~2u));
    *result=0u;return !memory->fault;
}
static void copy_vector(GameMemory *memory,GameAddress target,GameAddress source){unsigned axis;for(axis=0u;axis<3u;axis++)game_memory_write_u32(memory,target+axis*4u,game_memory_read_u32(memory,source+axis*4u));}
static void store_matrix(GameMemory *memory,GameAddress target,const float matrix[12]){unsigned i;for(i=0u;i<12u;i++)store_real(memory,target+i*4u,matrix[i]);}
static float sine(GameMemory *memory,uint16_t angle){return real_at(memory,0x6298a8u+((uint32_t)angle&2047u)*4u);}
static int32_t as_signed(uint32_t bits){int32_t value;memcpy(&value,&bits,4u);return value;}
static void load_matrix(GameMemory *memory,GameAddress source,float matrix[12]){unsigned i;for(i=0u;i<12u;i++)matrix[i]=real_at(memory,source+i*4u);}
static void sample_point(GameMemory *memory,GameAddress target,const float matrix[12],GameAddress source,const unsigned orders[3][3]){
    float point[3],result[3];unsigned row,axis;
    for(axis=0u;axis<3u;axis++)point[axis]=real_at(memory,source+axis*4u);
    for(row=0u;row<3u;row++){
        const unsigned *order=orders[row];
        long double value=(long double)matrix[row*4u+order[0]]*point[order[0]]+(long double)matrix[row*4u+order[1]]*point[order[1]];
        result[row]=(float)(value+(long double)matrix[row*4u+order[2]]*point[order[2]]+matrix[row*4u+3u]);
    }
    for(axis=0u;axis<3u;axis++)store_real(memory,target+axis*4u,result[axis]);
}
int spatial_sample_vertices(GameMemory *memory,GameAddress object,int rendered){
    static const unsigned xyz[3][3]={{0,1,2},{0,1,2},{0,1,2}};
    static const unsigned yzx[3][3]={{1,2,0},{1,2,0},{1,2,0}};
    static const unsigned xzy[3][3]={{0,2,1},{0,2,1},{0,2,1}};
    static const unsigned base_world[3][3]={{0,2,1},{0,2,1},{0,1,2}};
    static const unsigned base_render[3][3]={{0,1,2},{0,2,1},{0,2,1}};
    static const unsigned sample[3][3]={{2,1,0},{0,1,2},{1,0,2}};
    float world[12],yaw[12],pitch[12],middle[12],tip[12];GameAddress hull,output,source;int32_t begin,end,index;unsigned group;
    load_matrix(memory,rendered?game_memory_read_u32(memory,object+0x54u)+4u:object+0x80u,world);
    load_matrix(memory,rendered?game_memory_read_u32(memory,object+0x160u)+4u:object+0x110u,yaw);
    load_matrix(memory,rendered?game_memory_read_u32(memory,object+0xd0u)+4u:object+0xd4u,pitch);
    game_matrix_compose_values(middle,world,yaw);game_matrix_compose_values(tip,middle,pitch);
    /* The rendered base group intentionally consumes the current world frame. */
    load_matrix(memory,object+0x80u,world);
    hull=game_memory_read_u32(memory,object+0x3cu);output=game_memory_read_u32(memory,object+(rendered?0x70u:0x40u));
    for(group=0u;group<3u;group++){
        const float *matrix=group==0u?tip:group==1u?middle:world;
        const unsigned (*order)[3]=group==0u?(rendered?yzx:xyz):group==1u?(rendered?xzy:yzx):(rendered?base_render:base_world);
        begin=group==0u?short_at(memory,object+0x158u):group==1u?short_at(memory,object+0x164u):0;
        end=short_at(memory,object+(group==0u?0x44u:group==1u?0x158u:0x164u));
        for(index=begin;index<end && !memory->fault;index++)sample_point(memory,output+(uint32_t)index*12u,matrix,hull+(uint32_t)index*28u+12u,order);
    }
    if(!rendered){
        source=game_memory_read_u32(memory,object+0x50u);
        sample_point(memory,object+0x16cu,world,source+0x16cu,sample);
        sample_point(memory,object+0x17cu,middle,source+0x17cu,yzx);
    }
    return !memory->fault;
}
int spatial_sample_construct(GameMemory *memory,GameAddress object,const SpatialSampleDefinition *definition){
    PresentationInstanceDefinition base=definition->base;GameAddress first,bone,part;uint32_t total;int32_t a,b;unsigned i,axis;
    total=definition->groups[0]+definition->groups[1]+definition->groups[2];memcpy(&base.count,&total,4u);
    base.type=0x89u;base.flags|=0x8300u;
    if(!presentation_instance_create(memory,object,&base,0) || !presentation_instance_enable_spatial_sample(memory,object))return 0;
    game_memory_write_u32(memory,object,0x5753b0u);
    game_memory_write_u16(memory,object+0x164u,(uint16_t)definition->groups[0]);
    game_memory_write_u16(memory,object+0x158u,(uint16_t)(definition->groups[0]+definition->groups[1]));
    for(i=0u;i<3u;i++)for(axis=0u;axis<3u;axis++)store_real(memory,object+(i==0u?0x140u:i==1u?0x104u:0x14cu)+axis*4u,definition->anchors[i][axis]);
    game_memory_write_u32(memory,object+0xb4u,0u);game_memory_write_u16(memory,object+0xbcu,0u);
    part=game_memory_read_u32(memory,base.model+0x4cu);game_memory_write_u32(memory,object+0x160u,part);
    game_memory_write_u32(memory,object+0xd0u,game_memory_read_u32(memory,part+0x4cu));
    game_memory_write_u32(memory,object+0x168u,0u);game_memory_write_u32(memory,object+0x178u,0u);
    a=short_at(memory,object+0x164u);b=short_at(memory,object+0x158u);
    first=game_memory_read_u32(memory,object+0x3cu);bone=first+(uint32_t)a*28u;
    copy_vector(memory,object+0x16cu,first+12u);
    store_real(memory,object+0x170u,(float)((long double)real_at(memory,first+24u)*real_at(memory,0x575458u)+real_at(memory,object+0x170u)));
    copy_vector(memory,object+0x17cu,bone+12u);
    /* Retail adds the second group's initial radius to the first sample. */
    store_real(memory,object+0x170u,(float)((long double)real_at(memory,bone+24u)*real_at(memory,0x575458u)+real_at(memory,object+0x170u)));
    for(i=0u;a>0 && i<(uint32_t)a;i++){
        bone=first+i*28u;
        if(real_at(memory,bone+12u)>real_at(memory,object+0x16cu)){
            copy_vector(memory,object+0x16cu,bone+12u);
            store_real(memory,object+0x170u,(float)((long double)real_at(memory,bone+24u)*real_at(memory,0x575458u)+real_at(memory,object+0x170u)));
        }
    }
    for(;a<b;a++){
        long double height;bone=first+(uint32_t)a*28u;
        height=(long double)real_at(memory,bone+24u)*real_at(memory,0x575458u)+real_at(memory,bone+16u);
        if(height>real_at(memory,object+0x180u)){copy_vector(memory,object+0x17cu,bone+12u);store_real(memory,object+0x180u,(float)height);}
    }
    return !memory->fault;
}
int spatial_sample_copy(GameMemory *memory,GameAddress object,GameAddress source,const PresentationConstructionServices *services){
    static const unsigned anchors[3]={0x140u,0x104u,0x14cu};GameAddress model,part;unsigned i;
    if(!presentation_instance_create_copy(memory,object,source,0,services) || !presentation_instance_enable_spatial_sample(memory,object))return 0;
    game_memory_write_u32(memory,object,0x5753b0u);
    game_memory_write_u16(memory,object+0x164u,game_memory_read_u16(memory,source+0x164u));
    game_memory_write_u16(memory,object+0x158u,game_memory_read_u16(memory,source+0x158u));
    for(i=0u;i<3u;i++)copy_vector(memory,object+anchors[i],source+anchors[i]);
    game_memory_write_u16(memory,object+0xbcu,0u);game_memory_write_u32(memory,object+0xb4u,0u);
    game_memory_write_u32(memory,object+0x10u,game_memory_read_u32(memory,object+0x10u)|0x8300u);game_memory_write_u8(memory,object+12u,0x89u);
    model=game_memory_read_u32(memory,object+0x54u);part=game_memory_read_u32(memory,model+0x4cu);
    game_memory_write_u32(memory,object+0x160u,part);game_memory_write_u32(memory,object+0xd0u,game_memory_read_u32(memory,part+0x4cu));
    game_memory_write_u32(memory,object+0x168u,0u);game_memory_write_u32(memory,object+0x178u,0u);return !memory->fault;
}
int spatial_sample_frame(GameMemory *memory,GameAddress object,int rendered){
    uint16_t angles[3],yaw,pitch;float position[3],matrix[12],part[12],s,c,anchor[3],pivot[3];unsigned i;
    GameAddress target=rendered?game_memory_read_u32(memory,object+0x54u)+4u:object+0x80u;
    for(i=0u;i<3u;i++){angles[i]=game_memory_read_u16(memory,object+(rendered?0x78u-i*2u:0x24u+i*2u));position[i]=real_at(memory,object+(rendered?0x64u:0x18u)+i*4u);}
    gameplay_geometry_orientation_values(memory,angles,position,matrix);
    if(rendered){
        float sa=sine(memory,angles[0]),sb=sine(memory,angles[1]),sc=sine(memory,angles[2]);
        float ca=sine(memory,(uint16_t)(angles[0]+512u)),cc=sine(memory,(uint16_t)(angles[2]+512u));
        float sc_sa=(float)((long double)sc*sa),sc_ca=(float)((long double)sc*ca);
        /* Rendered retail pose rounds these two products before reuse. */
        matrix[0]=(float)((long double)ca*cc+(long double)sc_sa*sb);
        matrix[1]=(float)(((long double)sa*sb)*cc-sc_ca);
        matrix[8]=(float)((long double)sc_ca*sb-(long double)sa*cc);
        matrix[9]=(float)(((long double)cc*sb)*ca+sc_sa);
    }
    store_matrix(memory,target,matrix);
    yaw=game_memory_read_u16(memory,object+(rendered?0x166u:0x15cu));s=sine(memory,yaw);c=sine(memory,(uint16_t)(yaw+512u));
    memset(part,0,sizeof(part));part[0]=c;part[2]=s;part[5]=1.0f;part[8]=-s;part[10]=c;
    for(i=0u;i<3u;i++)part[i*4u+3u]=real_at(memory,object+0x140u+i*4u);
    target=rendered?game_memory_read_u32(memory,object+0x160u)+4u:object+0x110u;store_matrix(memory,target,part);
    pitch=game_memory_read_u16(memory,object+(rendered?0x15au:0xccu));s=sine(memory,pitch);c=sine(memory,(uint16_t)(pitch+512u));
    for(i=0u;i<3u;i++){anchor[i]=real_at(memory,object+0x104u+i*4u);pivot[i]=real_at(memory,object+0x14cu+i*4u);}
    memset(part,0,sizeof(part));part[0]=1.0f;part[3]=anchor[0];part[5]=c;part[6]=-s;part[9]=s;part[10]=c;
    part[7]=(float)(((long double)pivot[2]*s-(long double)pivot[1]*c)+anchor[1]+pivot[1]);
    part[11]=(float)((-(long double)pivot[1]*s-(long double)pivot[2]*c)+anchor[2]+pivot[2]);
    target=rendered?game_memory_read_u32(memory,object+0xd0u)+4u:object+0xd4u;store_matrix(memory,target,part);
    return !memory->fault;
}
int spatial_sample_activate(GameMemory *memory,GameAddress object,uint32_t identity,GameAddress point,const uint16_t angles[3],GameAddress source,int prepare,const PresentationActivationServices *placement,const SpatialSampleServices *services){
    uint32_t cost,random,tick;GameAddress catalog;
    if(!services || !services->catalog_value)return 0;
    game_memory_write_u16(memory,object+0x15cu,0u);game_memory_write_u16(memory,object+0x166u,0u);
    game_memory_write_u16(memory,object+0xccu,0xfeabu);game_memory_write_u16(memory,object+0x15au,0xfeabu);
    random=effect_initialization_random(memory,real_at(memory,0x57545cu));game_memory_write_u16(memory,object+0xbcu,(uint16_t)random);
    random=effect_initialization_random(memory,real_at(memory,0x575460u));tick=game_memory_read_u32(memory,0x5ac6acu);
    game_memory_write_u32(memory,object+0xc0u,tick+random+32u);
    if(!presentation_instance_bind_source(memory,object,identity,point,angles,source,prepare,placement))return 0;
    catalog=game_memory_read_u32(memory,0x5f0210u+(game_memory_read_u32(memory,object+0x14u)&63u)*4u);
    if(!services->catalog_value(services->effects.context,catalog,0x89u,5u,&cost))return 0;
    store_real(memory,object+0xc4u,-1.0f);game_memory_write_u32(memory,object+0x168u,0u);game_memory_write_u32(memory,object+0x178u,0u);
    {int32_t signed_cost;memcpy(&signed_cost,&cost,4u);store_real(memory,object+0xc8u,(float)signed_cost);}
    return !memory->fault;
}
int spatial_sample_reset(GameMemory *memory,GameAddress object,int notify,const SpatialSampleServices *services){
    static const unsigned fields[2]={0x168u,0x178u};unsigned i;uint8_t flags;GameAddress catalog;
    if(!services || !services->base_reset)return 0;
    for(i=0u;i<2u;i++){
        GameAddress effect=game_memory_read_u32(memory,object+fields[i]);
        if(effect && (!services->effects.retire_effect || !services->effects.retire_effect(services->effects.context,effect,1)))return 0;
        game_memory_write_u32(memory,object+fields[i],0u);
    }
    flags=game_memory_read_u8(memory,object+0xb8u);
    if(flags&2u){
        catalog=game_memory_read_u32(memory,0x5f0210u+(game_memory_read_u32(memory,object+0x14u)&63u)*4u);
        if(!services->unit || !services->unit(services->effects.context,catalog,11u,0))return 0;
        game_memory_write_u8(memory,object+0xb8u,(uint8_t)(flags&0xfdu));
    }
    return !memory->fault && services->base_reset(services->effects.context,object,notify);
}
static int effect_threshold(GameMemory *memory,GameAddress object,unsigned index,int needed,const PresentationArticulatedServices *services){
    uint32_t field=0x168u+index*16u;GameAddress child=game_memory_read_u32(memory,object+field);float position[3];unsigned axis;
    if(needed && !child){
        for(axis=0u;axis<3u;axis++)position[axis]=real_at(memory,object+field+4u+axis*4u);
        if(!services->create_effect || !services->create_effect(services->context,object,position,900.0f,600.0f,&child))return 0;
        game_memory_write_u32(memory,object+field,child);
    }else if(!needed && child){
        if(!services->retire_effect || !services->retire_effect(services->context,child,0))return 0;
        game_memory_write_u32(memory,object+field,0u);
    }
    return !memory->fault;
}
int spatial_sample_advance(GameMemory *memory,GameAddress object,const SpatialSampleServices *services){
    uint32_t tick,count,limit,health,random,numerator,denominator;GameAddress child,catalog;float progress,cost;
    const PresentationArticulatedServices *effects;
    if(!services || !services->gather)return 0;
    effects=&services->effects;tick=game_memory_read_u32(memory,0x5ac6acu);
    if(signed_at(memory,object+0xc0u)<as_signed(tick)){
        random=effect_initialization_random(memory,real_at(memory,0x57545cu));game_memory_write_u16(memory,object+0xbcu,(uint16_t)random);
        random=effect_initialization_random(memory,real_at(memory,0x575460u));
        game_memory_write_u32(memory,object+0xc0u,game_memory_read_u32(memory,0x5ac6acu)+random+32u);
    }
    if(!services->gather(effects->context,object,&count))return 0;
    tick=game_memory_read_u32(memory,0x5ac6acu);
    if(!(tick&15u) && count){
        if(!effects->health_limit || !effects->health_limit(effects->context,object,&limit))return 0;
        health=game_memory_read_u32(memory,object+0x58u);
        if(!effect_threshold(memory,object,0u,as_signed(health*2u)<as_signed(limit),effects))return 0;
        health=game_memory_read_u32(memory,object+0x58u);
        if(!effect_threshold(memory,object,1u,as_signed(health*4u)<as_signed(limit),effects))return 0;
    }
    child=game_memory_read_u32(memory,object+0x168u);
    if(count && child){
        tick=game_memory_read_u32(memory,0x5ac6acu);
        if(!(tick&3u) && (!effects->surface_effect || !effects->surface_effect(effects->context,object,child+0x18u)))return 0;
        child=game_memory_read_u32(memory,object+0x178u);
        if(child && (tick&3u)==2u && (!effects->surface_effect || !effects->surface_effect(effects->context,object,child+0x18u)))return 0;
    }
    if(!spatial_sample_step(memory,object,services))return 0;
    catalog=game_memory_read_u32(memory,0x58973cu);progress=real_at(memory,object+0xc4u);cost=real_at(memory,object+0xc8u);
    if(game_memory_read_u16(memory,catalog+8u)<4u && game_memory_read_u32(memory,object+0xc4u)<0xbf800000u && !(progress>=cost)){
        denominator=game_memory_read_u32(memory,catalog+0x214u)-game_memory_read_u32(memory,catalog+0x4e4u);
        numerator=game_memory_read_u32(memory,catalog+0x228u);
        if(numerator && as_signed(denominator)>0){
            progress=(float)((long double)as_signed(numerator)/as_signed(denominator)*real_at(memory,0x575458u)+progress);
            store_real(memory,object+0xc4u,progress);
            if(progress>cost)game_memory_write_u32(memory,object+0xc4u,game_memory_read_u32(memory,object+0xc8u));
        }
    }
    progress=real_at(memory,object+0xc4u);cost=real_at(memory,object+0xc8u);
    if((cost==progress || isnan(cost) || isnan(progress)) && game_memory_read_u32(memory,object+0x58u)){
        if(!services->unit || !services->unit(effects->context,game_memory_read_u32(memory,0x58973cu),11u,1))return 0;
        progress=real_at(memory,object+0xc4u);
        game_memory_write_u8(memory,object+0xb8u,(uint8_t)(game_memory_read_u8(memory,object+0xb8u)|2u));
        store_real(memory,object+0xc4u,(float)((long double)progress+real_at(memory,0x575470u)));
    }
    return !memory->fault;
}
int spatial_sample_step(GameMemory *memory,GameAddress object,const SpatialSampleServices *services){
    int32_t mode=short_at(memory,object+0xbcu),pitch;uint16_t yaw,word;uint32_t tick;GameAddress node,head,other,child;float position[3];unsigned axis,index,visits=0u;
    const PresentationArticulatedServices *effects;
    if(!services)return 0;
    if(mode==0)return !memory->fault;
    effects=&services->effects;yaw=game_memory_read_u16(memory,object+0x15cu);word=game_memory_read_u16(memory,object+0xccu);
    if(mode==1)yaw=(uint16_t)(yaw-2u);
    else if(mode==2)yaw=(uint16_t)(yaw+2u);
    else if(mode==3 || mode==4 || mode==5){
        tick=game_memory_read_u32(memory,0x5ac6acu);
        if(mode==5)yaw=(uint16_t)(yaw-3u);
        if(mode==3 || (mode==5 && (tick&0x4fu)>0x2fu)){
            word=(uint16_t)(word-(mode==3?2u:1u));pitch=(int32_t)word-((word&0x8000u)?65536:0);
            if(pitch<=-512)word=0xfe00u;
        }else{
            word=(uint16_t)(word+(mode==4?2u:1u));pitch=(int32_t)word-((word&0x8000u)?65536:0);
            if(pitch>=-170)word=0xff56u;
        }
        game_memory_write_u16(memory,object+0xccu,word);
    }
    game_memory_write_u16(memory,object+0x15cu,(uint16_t)(yaw&2047u));
    if(!spatial_sample_frame(memory,object,0) || !spatial_sample_vertices(memory,object,0) || !services->probe || !services->probe(effects->context,object))return 0;
    node=game_memory_read_u32(memory,object+0xb0u);
    while(node && !memory->fault){
        if(++visits>65536u)return 0;
        other=game_memory_read_u32(memory,node+12u);
        for(axis=0u;axis<3u;axis++)position[axis]=real_at(memory,node+16u+axis*4u);
        if(!effects->command || !effects->command(effects->context,object,other,position) || !effects->notify_command || !effects->notify_command(effects->context,other,object,position))return 0;
        if(!game_memory_read_u32(memory,object+0x58u))return !memory->fault;
        node=game_memory_read_u32(memory,node+4u);
    }
    head=game_memory_read_u32(memory,object+0xb0u);
    if(head){if(!effects->release_commands || !effects->release_commands(effects->context,head))return 0;game_memory_write_u32(memory,object+0xb0u,0u);}
    if(game_memory_read_u32(memory,object+0x168u))for(index=0u;index<2u;index++){
        child=game_memory_read_u32(memory,object+0x168u+index*16u);if(!child)break;
        for(axis=0u;axis<3u;axis++)position[axis]=real_at(memory,object+0x16cu+index*16u+axis*4u);
        if(!effects->refresh_effect || !effects->refresh_effect(effects->context,child,position))return 0;
    }
    return !memory->fault;
}
static uint32_t truncate_value(long double value){
    if(!isfinite(value) || value < -9223372036854775808.0L || value >= 9223372036854775808.0L)return 0u;
    return (uint32_t)(int64_t)value;
}
int spatial_sample_interpolate(GameMemory *memory,GameAddress object){
    static const unsigned current[5]={0x24u,0x26u,0x28u,0x15cu,0xccu};
    static const unsigned prior[5]={0x78u,0x76u,0x74u,0x166u,0x15au};
    uint32_t frame=game_memory_read_u32(memory,0x5acbe8u),last=game_memory_read_u32(memory,object+0x7cu),delta;
    long double interval;float fraction;unsigned axis;int snap;
    game_memory_write_u32(memory,object+0x7cu,frame);
    delta=game_memory_read_u32(memory,0x5acc80u)-game_memory_read_u32(memory,0x5acbe8u);
    interval=(long double)as_signed(delta)+real_at(memory,0x5acc38u);
    snap=as_signed(frame-last)>10 || !(interval>real_at(memory,0x575470u));
    if(snap){
        copy_vector(memory,object+0x64u,object+0x18u);
        for(axis=0u;axis<5u;axis++)game_memory_write_u16(memory,object+prior[axis],game_memory_read_u16(memory,object+current[axis]));
        return !memory->fault;
    }
    interval=(float)interval;fraction=(float)((long double)real_at(memory,0x575470u)/interval);
    for(axis=0u;axis<3u;axis++){
        float old=real_at(memory,object+0x64u+axis*4u),now=real_at(memory,object+0x18u+axis*4u);
        store_real(memory,object+0x64u+axis*4u,(float)(((long double)now-old)*fraction+old));
    }
    for(axis=0u;axis<5u;axis++){
        uint16_t old=game_memory_read_u16(memory,object+prior[axis]),now=game_memory_read_u16(memory,object+current[axis]),next;int32_t difference;
        if(axis==4u)difference=short_at(memory,object+current[axis])-short_at(memory,object+prior[axis]);
        else{difference=(int32_t)((now-old)&2047u);if(difference>1024)difference-=2048;}
        next=(uint16_t)(old+truncate_value((long double)difference*fraction));next=(uint16_t)(next&2047u);
        if((axis==1u || axis==2u || axis==4u) && next>1024u)next=(uint16_t)(next-2048u);
        game_memory_write_u16(memory,object+prior[axis],next);
    }
    return !memory->fault;
}
int spatial_sample_present(GameMemory *memory,GameAddress object,void *context,int (*submit)(void *,GameAddress)){
    unsigned axis;
    if(game_memory_read_u32(memory,0x5eff04u)){
        if(!spatial_sample_interpolate(memory,object))return 0;
    }else{
        copy_vector(memory,object+0x64u,object+0x18u);
        for(axis=0u;axis<3u;axis++)game_memory_write_u16(memory,object+0x78u-axis*2u,game_memory_read_u16(memory,object+0x24u+axis*2u));
        game_memory_write_u16(memory,object+0x166u,game_memory_read_u16(memory,object+0x15cu));
        game_memory_write_u16(memory,object+0x15au,game_memory_read_u16(memory,object+0xccu));
    }
    return spatial_sample_frame(memory,object,1) && spatial_sample_vertices(memory,object,1) && submit && submit(context,game_memory_read_u32(memory,object+0x54u));
}
static int discard_inputs(const SpatialSampleDefinition *definition,const SpatialSampleFactoryServices *services){
    if(definition->base.model && (!services->retirement.release_model || !services->retirement.release_model(services->retirement.context,definition->base.model)))return 0;
    return !definition->base.resource || (services->retirement.release_storage && services->retirement.release_storage(services->retirement.context,definition->base.resource));
}
GameAddress spatial_sample_template(GameMemory *memory,uint16_t slot,const SpatialSampleFactoryServices *services){
    GameAddress publication,object;SpatialSampleDefinition definition={0};int loaded;
    if(slot>=6u || !services || !services->describe || !services->allocate || !services->release)return 0u;
    publication=0x5adb28u+(uint32_t)slot*4u;object=game_memory_read_u32(memory,publication);
    if(object || memory->fault)return object;
    if(game_memory_read_u32(memory,0x624f8cu)==10u || memory->fault)return 0u;
    loaded=services->describe(services->context,slot,&definition);
    if(loaded<=0){if(loaded<0)memory->fault=1;return 0u;}
    object=services->allocate(services->context,0x188u);
    if(!object){(void)discard_inputs(&definition,services);memory->fault=1;return 0u;}
    if(!spatial_sample_construct(memory,object,&definition)){
        if(presentation_instance_find(object))(void)presentation_instance_release(memory,object,&services->retirement,1,0);
        else (void)discard_inputs(&definition,services);
        (void)services->release(services->context,object);memory->fault=1;return 0u;
    }
    game_memory_write_u32(memory,publication,object);return memory->fault?0u:object;
}
int spatial_sample_fill(GameMemory *memory,uint16_t slot,int32_t count,const SpatialSampleFactoryServices *services){
    GameAddress source=spatial_sample_template(memory,slot,services);int32_t index;
    if(!source || memory->fault)return 0;
    if(count>65536){memory->fault=1;return 0;}
    for(index=0;index<count;index++){
        GameAddress object=services->allocate(services->context,0x188u);
        if(!object){memory->fault=1;return 0;}
        if(!spatial_sample_copy(memory,object,source,&services->construction)){
            if(presentation_instance_find(object))(void)presentation_instance_release(memory,object,&services->retirement,1,0);
            (void)services->release(services->context,object);memory->fault=1;return 0;
        }
        (void)game_list_insert_after(memory,object,source,4u,8u);if(memory->fault)return 0;
    }
    return 1;
}
GameAddress spatial_sample_take(GameMemory *memory,uint16_t slot,const SpatialSampleFactoryServices *services){
    GameAddress source=spatial_sample_template(memory,slot,services),object;
    if(!source || memory->fault)return 0u;
    if(!game_memory_read_u32(memory,source+4u) && !spatial_sample_fill(memory,slot,1,services))return 0u;
    object=game_memory_read_u32(memory,source+4u);if(!object || memory->fault)return 0u;
    (void)game_list_detach_node(memory,object,8u,4u);return memory->fault?0u:object;
}
static void death_compose(float output[12],const float left[12],const float right[12],int second){
    static const unsigned first_orders[3][4][3]={
        {{0,1,2},{1,2,0},{1,2,0},{1,2,0}},
        {{2,0,1},{1,2,0},{1,2,0},{1,2,0}},
        {{2,0,1},{1,2,0},{1,2,0},{1,2,0}}};
    static const unsigned second_orders[4][3]={{2,1,0},{2,1,0},{2,0,1},{1,0,2}};
    unsigned row,column;
    for(row=0u;row<3u;row++)for(column=0u;column<4u;column++){
        const unsigned *order=second?second_orders[column]:first_orders[row][column];
        long double value=(long double)left[row*4u+order[0]]*right[order[0]*4u+column];
        value+=(long double)left[row*4u+order[1]]*right[order[1]*4u+column];
        value+=(long double)left[row*4u+order[2]]*right[order[2]*4u+column];
        if(column==3u)value+=left[row*4u+3u];
        output[row*4u+column]=(float)value;
    }
}
int spatial_sample_death(GameMemory *memory,GameAddress object,GameAddress other,const PresentationArticulatedDeathServices *services){
    GameAddress catalog,header,child;uint32_t variant,profile,duration;unsigned part,axis;
    float world[12],local[12],middle[12],transform[12],position[3],velocity[3]={0.0f,40.0f,0.0f};
    const float upward[3]={0.0f,100.0f,0.0f};
    if(!services || !services->begin || !services->sound || !services->mesh_fragments || !services->variant || !services->anchors || !services->acquire || !services->emit)return 0;
    catalog=game_memory_read_u32(memory,0x5f0210u+(game_memory_read_u32(memory,object+0x14u)&63u)*4u);
    if(memory->fault || !services->begin(services->context,object,other))return 0;
    for(axis=0u;axis<3u;axis++)position[axis]=real_at(memory,object+0x18u+axis*4u);
    if(!services->sound(services->context,position,(game_memory_read_u32(memory,0x5ac6acu)&1u)?0x82:0x83))return 0;
    if(other && (game_memory_read_u32(memory,other+0x10u)&2u))for(axis=0u;axis<3u;axis++){
        long double value=(long double)real_at(memory,other+0xb4u+axis*4u)*real_at(memory,0x575478u);
        if(axis==1u)value+=real_at(memory,0x57547cu);
        velocity[axis]=(float)value;
    }
    load_matrix(memory,object+0x80u,world);header=game_memory_read_u32(memory,game_memory_read_u32(memory,object+0x54u)+0x50u);
    if(memory->fault || !services->mesh_fragments(services->context,object,header,world,velocity) || !services->variant(services->context,catalog,&variant) ||
       !services->anchors(services->context,object,signed_at(memory,0x626660u+variant*4u),game_memory_read_u32(memory,0x626678u+variant*4u),upward))return 0;
    for(part=0u;part<2u;part++){
        AttachmentEmission request;uint32_t start,end;GameAddress model;
        memset(&request,0,sizeof(request));model=game_memory_read_u32(memory,object+(part?0xd0u:0x160u));header=game_memory_read_u32(memory,model+0x50u);
        for(profile=0u;profile<3u;profile++)if(signed_at(memory,0x5a0488u+profile*8u)>=signed_at(memory,header+0x14u) && signed_at(memory,0x5a048cu+profile*8u)>=signed_at(memory,header+0x1cu))break;
        if(memory->fault)return 0;
        child=services->acquire(services->context,(uint16_t)profile);if(!child)return 0;
        start=(uint32_t)short_at(memory,object+(part?0x158u:0x164u));end=(uint32_t)short_at(memory,object+(part?0x44u:0x158u));
        if(!attachment_effect_configure(memory,child,header,game_memory_read_u32(memory,object+0x3cu)+start*28u,(uint16_t)(end-start)))goto failed;
        load_matrix(memory,object+0x80u,world);load_matrix(memory,object+0x110u,local);death_compose(middle,world,local,0);
        if(part){load_matrix(memory,object+0xd4u,local);death_compose(transform,middle,local,1);}else memcpy(transform,middle,sizeof(transform));
        request.angles[0]=(uint16_t)((game_memory_read_u16(memory,object+0x24u)+game_memory_read_u16(memory,object+0x15cu))&2047u);
        if(part)request.angles[1]=game_memory_read_u16(memory,object+0xccu);
        for(axis=0u;axis<3u;axis++)request.position[axis]=transform[axis*4u+3u];
        request.velocity[0]=(float)as_signed(effect_initialization_random(memory,real_at(memory,0x575488u))+30u);
        request.velocity[2]=(float)as_signed(effect_initialization_random(memory,real_at(memory,0x575488u))+30u);
        if(effect_initialization_random(memory,real_at(memory,0x57548cu)))request.velocity[0]=-request.velocity[0];
        if(effect_initialization_random(memory,real_at(memory,0x57548cu)))request.velocity[2]=-request.velocity[2];
        duration=part?0u:effect_initialization_random(memory,real_at(memory,0x575490u))+60u;
        request.velocity[1]=(float)as_signed(effect_initialization_random(memory,real_at(memory,0x575494u))+40u);
        request.duration=duration;request.mode=part?0u:3u;request.flags=0x17u;request.source=game_memory_read_u32(memory,object+0x14u);
        if(memory->fault || !services->emit(services->context,catalog,child,&request))goto failed;
        continue;
failed:
        if(services->discard)(void)services->discard(services->context,child);
        return 0;
    }
    game_terrain_impact(memory,real_at(memory,object+0x18u),real_at(memory,object+0x20u),1024);return !memory->fault;
}
