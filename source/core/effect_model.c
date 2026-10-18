#include "effect_model.h"
#include "effect_profile.h"
#include "scene_projection_storage.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
static uint32_t float_bits(float value){uint32_t bits;memcpy(&bits,&value,4u);return bits;}
static float bits_float(uint32_t bits){float value;memcpy(&value,&bits,4u);return value;}
static float model_float(GameMemory *m,GameAddress a){return bits_float(game_memory_read_u32(m,a));}
typedef struct EffectProfileBank {
    GameAddress projection;uint32_t references;uint16_t count;
    GameAddress profiles[7];
    struct EffectProfileBank *next;
} EffectProfileBank;
static EffectProfileBank *profile_banks;
static EffectProfileBank *profile_bank(GameAddress profile){
    EffectProfileBank *bank;unsigned i;
    for(bank=profile_banks;bank;bank=bank->next)for(i=0;i<bank->count;i++)if(bank->profiles[i]==profile)return bank;
    return NULL;
}
static int bank_read(void *context,uint32_t offset,void *data,size_t size){
    EffectProfileBank *bank=context;uint8_t *out=data;size_t i;
    if(offset>bank->count*4u || size>bank->count*4u-offset)return 0;
    for(i=0;i<size;i++)out[i]=(uint8_t)(bank->profiles[(offset+i)/4u]>>(((offset+i)&3u)*8u));
    return 1;
}
static int bank_write(void *context,uint32_t offset,const void *data,size_t size){
    (void)context;(void)offset;(void)data;(void)size;return 0;
}
static int bank_release(EffectProfileBank *bank){
    unsigned i;uint8_t bytes[4];GameAddress table;EffectProfileBank **link;
    if(!bank)return 1;
    if(bank->references>1){bank->references--;return 1;}
    for(i=0;i<bank->count;i++)if(bank->profiles[i]){
        if(!scene_projection_storage_read(bank->profiles[i],bytes,4))return 0;
        table=(uint32_t)bytes[0]|(uint32_t)bytes[1]<<8u|(uint32_t)bytes[2]<<16u|(uint32_t)bytes[3]<<24u;
        if(table && !scene_projection_storage_free(table))return 0;
        if(!scene_projection_storage_free(bank->profiles[i]))return 0;
        bank->profiles[i]=0;
    }
    if(!scene_projection_storage_free(bank->projection))return 0;
    for(link=&profile_banks;*link && *link!=bank;link=&(*link)->next){}
    if(*link)*link=bank->next;
    free(bank);return 1;
}
static uint32_t model_word(EffectModel *s,unsigned o){EffectQuad *q=&s->quad;
    if(o>=4u && o<=0x30u)return float_bits(s->local[(o-4u)/4u]);
    if(o>=0x44u && o<=0x80u){unsigned n=(o-0x44u)/4u;const EffectQuadVertex *v=&q->vertices[n/4u];return float_bits(n%4u==0u?v->x:n%4u==1u?v->y:n%4u==2u?v->reciprocal_depth:v->w);}
    if(o>=0x84u && o<=0x90u)return q->vertex_refs[(o-0x84u)/4u];
    if(o>=0x94u && o<=0xb0u){unsigned n=(o-0x94u)/4u;return float_bits(q->uv[n/2u][n%2u]);}
    if(o>=0xb4u && o<=0xd0u){unsigned n=(o-0xb4u)/4u;return float_bits(q->shading[n/4u][n%4u]);}
    switch(o){
    case 0:return s->methods;case 0x34:return s->descriptor;case 0x38:return s->lighting;case 0x3c:return s->mode;case 0x40:return s->parent;
    case 0xd4:return (uint16_t)q->texture_width|((uint32_t)(uint16_t)q->texture_height<<16);
    case 0xd8:return q->texture;case 0xdc:return q->count;case 0xe0:return q->vertex_array;case 0xe4:return q->flags;case 0xe8:return q->next;
    case 0xec:return q->uv_ref;case 0xf0:return q->material_ref;case 0xf4:return q->material_padding;case 0xf8:return q->tint;
    case 0xfc:return q->shade_source;case 0x100:return q->shade_alternate;
    case 0x104:return float_bits(q->width);case 0x108:return float_bits(q->height);case 0x10c:return float_bits(q->bound_radius);
    case 0x110:return s->bank?s->bank->projection:s->profile;
    case 0x114:return s->bank?s->bank->count:s->start_tick;
    case 0x118:return s->bank?s->profile:0;
    case 0x11c:return s->bank?s->start_tick:0;
    case 0x120:return s->bank_owner;
    }return 0;
}
static void model_set_word(EffectModel *s,unsigned o,uint32_t value){EffectQuad *q=&s->quad;
    if(o>=4u && o<=0x30u){s->local[(o-4u)/4u]=bits_float(value);return;}
    if(o>=0x44u && o<=0x80u){unsigned n=(o-0x44u)/4u;EffectQuadVertex *v=&q->vertices[n/4u];float f=bits_float(value);if(n%4u==0u)v->x=f;else if(n%4u==1u)v->y=f;else if(n%4u==2u)v->reciprocal_depth=f;else v->w=f;return;}
    if(o>=0x84u && o<=0x90u){q->vertex_refs[(o-0x84u)/4u]=value;return;}
    if(o>=0x94u && o<=0xb0u){unsigned n=(o-0x94u)/4u;q->uv[n/2u][n%2u]=bits_float(value);return;}
    if(o>=0xb4u && o<=0xd0u){unsigned n=(o-0xb4u)/4u;q->shading[n/4u][n%4u]=bits_float(value);return;}
    switch(o){
    case 0:s->methods=value;break;case 0x34:s->descriptor=value;break;case 0x38:s->lighting=value;break;case 0x3c:s->mode=value;break;case 0x40:s->parent=value;break;
    case 0xd4:q->texture_width=(int16_t)value;q->texture_height=(int16_t)(value>>16);break;
    case 0xd8:q->texture=value;break;case 0xdc:q->count=value;break;case 0xe0:q->vertex_array=value;break;case 0xe4:q->flags=value;break;case 0xe8:q->next=value;break;
    case 0xec:q->uv_ref=value;break;case 0xf0:q->material_ref=value;break;case 0xf4:q->material_padding=value;break;case 0xf8:q->tint=value;break;
    case 0xfc:q->shade_source=value;break;case 0x100:q->shade_alternate=value;break;
    case 0x104:q->width=bits_float(value);break;case 0x108:q->height=bits_float(value);break;case 0x10c:q->bound_radius=bits_float(value);break;
    case 0x110:if(!s->bank)s->profile=value;break;
    case 0x114:if(!s->bank)s->start_tick=value;break;
    case 0x118:if(s->bank)s->profile=value;break;
    case 0x11c:if(s->bank)s->start_tick=value;break;
    }
}
static int model_transfer(void *context,uint32_t offset,void *data,size_t size,int writing){
    EffectModel *s=context;uint8_t *bytes=data;size_t i;
    unsigned limit=s && s->bank?0x124u:0x118u;
    if(!s || !data || offset>limit || size>limit-offset)return 0;
    for(i=0;i<size;++i){unsigned o=(offset+(unsigned)i)&~3u,shift=((offset+(unsigned)i)&3u)*8u;uint32_t value=model_word(s,o);
        if(writing)model_set_word(s,o,(value&~(255u<<shift))|((uint32_t)bytes[i]<<shift));else bytes[i]=(uint8_t)(value>>shift);
    }return 1;
}
static int model_read(void *c,uint32_t o,void *b,size_t n){return model_transfer(c,o,b,n,0);}
static int model_write(void *c,uint32_t o,const void *b,size_t n){return model_transfer(c,o,(void *)b,n,1);}
EffectModel *effect_model_find(GameAddress a){return scene_projection_storage_state(a,model_read);}
void effect_quad_clear_flags(EffectQuad *q,uint32_t flags){q->flags&=~flags;}
void effect_quad_set_flags(EffectQuad *q,uint32_t flags,uint32_t tint,GameAddress shade,GameAddress alternate){
    q->flags|=flags&~4u;if(flags&0x800u)q->flags&=~8u;
    if(flags&0x200u)q->shade_source=shade?shade:0x604380u;
    if(flags&0xc0u)q->shade_alternate=alternate?alternate:0x604278u;
    if(flags&0x20u)q->tint=tint;
}
int effect_model_construct(GameMemory *m,GameAddress object,GameAddress source){
    EffectQuad quad;uint32_t flags;unsigned i;
    memset(&quad,0,sizeof(quad));quad.flags=0x11du;
    /* Only fields written by the source constructor are published. Asset
     * prototypes still borrow their allocator; runtime clones below own C state. */
    game_memory_write_u32(m,object,0x571ab8u);
    for(i=0;i<12u;++i)game_memory_write_u32(m,object+4u+i*4u,float_bits(i==0u || i==5u || i==10u?1.0f:0.0f));
    game_memory_write_u32(m,object+0x34u,0);game_memory_write_u32(m,object+0x38u,0);
    game_memory_write_u8(m,object+0x3cu,0);
    game_memory_write_u32(m,object+0xe0u,object+0x84u);
    for(i=0;i<4u;++i)game_memory_write_u32(m,object+0x84u+i*4u,object+0x44u+i*16u);
    game_memory_write_u32(m,object+0xecu,object+0x94u);game_memory_write_u32(m,object+0xf0u,object+0xd4u);
    game_memory_write_u32(m,object+0xe8u,0);
    flags=game_memory_read_u32(m,source+0xe4u);
    effect_quad_set_flags(&quad,flags,game_memory_read_u32(m,source+0xf8u),game_memory_read_u32(m,source+0xfcu),game_memory_read_u32(m,source+0x100u));
    game_memory_write_u32(m,object+0xe4u,quad.flags);
    if(flags&0x20u)game_memory_write_u32(m,object+0xf8u,quad.tint);
    if(flags&0x200u)game_memory_write_u32(m,object+0xfcu,quad.shade_source);
    if(flags&0xc0u)game_memory_write_u32(m,object+0x100u,quad.shade_alternate);
    game_memory_write_u8(m,object+0x3cu,game_memory_read_u8(m,source+0x3cu));
    game_memory_write_u32(m,object+0x40u,0);return !m->fault;
}
static GameAddress model_create(unsigned mode,unsigned bytes){
    EffectModel *s=calloc(1,sizeof(*s));unsigned i;
    if(!s)return 0;
    s->methods=0x571ab8u;s->local[0]=s->local[5]=s->local[10]=1.0f;s->quad.flags=0x11du;
    s->mode=mode;
    s->projection=scene_projection_storage_allocate(bytes,"effect model projection");
    if(!s->projection){free(s);return 0;}
    for(i=0;i<4;++i)s->quad.vertex_refs[i]=s->projection+0x44u+i*16u;
    s->quad.vertex_array=s->projection+0x84u;s->quad.uv_ref=s->projection+0x94u;s->quad.material_ref=s->projection+0xd4u;
    if(!scene_projection_storage_bind_state(s->projection,s,model_read,model_write)){scene_projection_storage_free(s->projection);free(s);return 0;}
    return s->projection;
}
GameAddress effect_model_create(unsigned mode){return model_create(mode,0x118u);}
GameAddress effect_model_create_bank(const GameAddress *profiles,uint16_t count){
    EffectProfileBank *bank;EffectModel *model;GameAddress address;
    if(!profiles || !count || count>7)return 0;
    bank=calloc(1,sizeof(*bank));if(!bank)return 0;
    bank->count=count;bank->references=1;
    bank->projection=scene_projection_storage_allocate(count*4u,"billboard profile bank");
    if(!bank->projection){free(bank);return 0;}
    if(!scene_projection_storage_bind_state(bank->projection,bank,bank_read,bank_write))goto failed;
    address=model_create(0,0x124u);if(!address)goto failed;
    memcpy(bank->profiles,profiles,count*sizeof(*profiles));
    model=effect_model_find(address);model->methods=0x574288;model->bank=bank;model->bank_owner=1;
    bank->next=profile_banks;profile_banks=bank;
    return address;
failed:scene_projection_storage_free(bank->projection);free(bank);return 0;
}
int effect_model_select_profile(GameMemory *m,EffectModel *model,uint16_t index,uint32_t tick){
    if(!model || !model->bank || index>=model->bank->count)return 0;
    if(model->profile==model->bank->profiles[index])return !m->fault;
    model->start_tick=tick;
    return effect_model_configure(m,model,model->bank->profiles[index],0,0);
}
GameAddress effect_model_clone(GameMemory *memory,GameAddress source){
    EffectModel *original=effect_model_find(source);
    GameAddress address=model_create(game_memory_read_u8(memory,source+0x3cu),original && original->bank?0x124u:0x118u);
    EffectModel *s=effect_model_find(address);
    if(!s)return 0;
    effect_quad_set_flags(&s->quad,game_memory_read_u32(memory,source+0xe4u),
        game_memory_read_u32(memory,source+0xf8u),game_memory_read_u32(memory,source+0xfcu),game_memory_read_u32(memory,source+0x100u));
    if(memory->fault){effect_model_release(address);return 0;}
    if(original && original->bank){s->bank=original->bank;s->bank->references++;s->methods=0x574288;}
    return address;
}
int effect_model_release(GameAddress address){EffectModel *s=effect_model_find(address);if(!s)return 0;
    if(!bank_release(s->profile_owner))return 0;
    s->profile_owner=NULL;
    if(!bank_release(s->bank))return 0;
    s->bank=NULL;if(!scene_projection_storage_free(address))return 0;free(s);return 1;}
int effect_model_configure(GameMemory *m,EffectModel *s,GameAddress profile,float scale,int explicit_scale){
    EffectProfileBank *owner=profile_bank(profile);
    int32_t width,height;long double one,half,twice,ratio,scaled_height,half_width,half_height,sum;
    s->profile=profile;width=(int32_t)game_memory_read_u32(m,profile+0xcu);height=(int32_t)game_memory_read_u32(m,profile+0x10u);
    if(!explicit_scale)scale=model_float(m,profile+0x18u);
    one=model_float(m,0x571f70u);half=model_float(m,0x571f74u);twice=model_float(m,0x571f78u);
    ratio=one/(long double)width;ratio*=(long double)height;scaled_height=ratio*scale;
    s->quad.width=scale;s->quad.height=(float)scaled_height;
    half_width=(long double)scale*half;half_width*=half_width;half_width*=twice;
    half_height=scaled_height*half;half_height*=half_height;sum=half_height+half_width;
    s->quad.bound_radius=(float)sqrtl((long double)(float)sum);
    s->quad.texture_width=(int16_t)width;s->quad.texture_height=(int16_t)height;
    if(m->fault)return 0;
    if(owner==s->bank)owner=NULL;
    if(owner!=s->profile_owner){
        if(owner)owner->references++;
        if(!bank_release(s->profile_owner)){if(owner)owner->references--;return 0;}
        s->profile_owner=owner;
    }
    return 1;
}
static uint32_t model_truncate(long double value){
    int64_t integer=!isfinite(value) || value < -0x1p63L || value >= 0x1p63L?INT64_MIN:(int64_t)value;
    return (uint32_t)(uint64_t)integer;
}
int effect_model_sample_frame(GameMemory *m,EffectModel *s,uint16_t direction,uint32_t tick_delta,int *visible){
    EffectProfileSample sample;uint8_t flags;
    if(!effect_profile_sample(m,s->profile,direction,tick_delta,&sample,visible))return 0;
    if(!*visible)return 1;
    flags=sample.flags;s->quad.texture=sample.texture;
    if(flags&4u)effect_quad_set_flags(&s->quad,0x20u,0,0,0);else effect_quad_clear_flags(&s->quad,0x20u);
    if(flags&0x10u)effect_quad_set_flags(&s->quad,0x800u,0,0,0);else effect_quad_clear_flags(&s->quad,0x800u);
    if(sample.mirrored>=0){if(sample.mirrored)effect_quad_set_flags(&s->quad,0x80000000u,0,0,0);else effect_quad_clear_flags(&s->quad,0x80000000u);}
    if(m->fault)return 0;
    *visible=1;return 1;
}
int effect_model_direction(GameMemory *m,const EffectModel *s,uint16_t *direction){
    GameAddress camera;float dx,dz,scale;uint32_t index;uint16_t angle;
    *direction=0;
    if(game_memory_read_u16(m,s->profile+6u)==1u)return !m->fault;
    camera=game_memory_read_u32(m,0x58f890u);
    dx=(float)((long double)s->local[3]-model_float(m,camera+12u));
    dz=(float)((long double)s->local[11]-model_float(m,camera+20u));
    scale=model_float(m,s->bank?0x574298u:0x571ac8u);
    if(!(fabsl(dx)>=fabsl(dz))){
        index=model_truncate((long double)dx*scale/dz+scale);
        angle=game_memory_read_u16(m,0x62e8b0u+index*2u);
        angle=(int32_t)float_bits(dz)>0?(uint16_t)(angle&2047u):(uint16_t)(angle+1024u);
    }else{
        index=model_truncate((long double)dz*scale/dx+scale);
        angle=(uint16_t)(((int32_t)float_bits(dx)>0?512u:1536u)-game_memory_read_u16(m,0x62e8b0u+index*2u));
    }
    if(s->parent){
        uint32_t flags=game_memory_read_u32(m,s->parent+16u);
        *direction=(uint16_t)(angle-game_memory_read_u16(m,s->parent+((flags&1u)?0x78u:0x24u))+1024u);
    }
    return !m->fault;
}
/* Billboard projection keeps the original stored-float boundaries, while
 * camera and viewport inputs are independent of platform/guest state. */
int effect_quad_project(EffectQuad *q,const float position[3],const EffectModelView *v){
    volatile long double products[3],depth,center_x,center_y,reciprocal,focal,half_focal;
    volatile long double horizontal_width,horizontal_height,vertical_width,vertical_height,left,right;
    float stored_depth,stored_vertical_width,stored_vertical_height,stored_left,stored_reciprocal,half_uv,right_uv,bottom_uv;
    unsigned row,axis;
    for(axis=0;axis<3;++axis)products[axis]=(long double)position[axis]*v->world_to_view[8u+axis];
    depth=products[0]+products[1];depth+=products[2];depth+=v->world_to_view[11];stored_depth=(float)depth;
    if(!(depth>v->near_depth) || !((long double)v->far_distance+v->far_bias>stored_depth))return 0;
    reciprocal=(long double)v->unit/stored_depth;focal=(long double)v->focal*reciprocal;
    for(row=0;row<2;++row){volatile long double sum;
        for(axis=0;axis<3;++axis)products[axis]=(long double)position[axis]*v->world_to_view[row*4u+axis];
        sum=products[0]+products[1];sum+=products[2];sum+=v->world_to_view[row*4u+3u];
        if(row==0)center_x=sum*focal;else{sum*=v->vertical_scale;center_y=sum*focal;}
    }
    half_focal=focal*v->half;
    horizontal_width=(long double)q->width*v->roll_cosine;horizontal_width*=half_focal;
    horizontal_height=(long double)q->height*v->roll_sine;horizontal_height*=v->vertical_scale;horizontal_height*=half_focal;
    vertical_width=(long double)q->width*v->roll_sine;vertical_width*=half_focal;stored_vertical_width=(float)vertical_width;
    vertical_height=(long double)q->height*v->roll_cosine;vertical_height*=v->vertical_scale;vertical_height*=half_focal;stored_vertical_height=(float)vertical_height;
    stored_reciprocal=(float)(1.0L/stored_depth);
    left=center_x-horizontal_height;stored_left=(float)left;
    q->vertices[0].x=(float)((long double)v->center[0]-(left-horizontal_width));
    q->vertices[0].y=(float)((long double)v->center[1]-((center_y-stored_vertical_width)+stored_vertical_height));
    q->vertices[1].x=(float)((long double)v->center[0]-((long double)stored_left+horizontal_width));
    q->vertices[1].y=(float)((long double)v->center[1]-(((long double)stored_vertical_height+center_y)+stored_vertical_width));
    right=center_x+horizontal_height;right+=horizontal_width;
    q->vertices[2].x=(float)((long double)v->center[0]-right);
    center_y-=stored_vertical_height;
    q->vertices[2].y=(float)((long double)v->center[1]-(center_y+stored_vertical_width));
    right=center_x-horizontal_width;right+=horizontal_height;
    q->vertices[3].x=(float)((long double)v->center[0]-right);
    q->vertices[3].y=(float)((long double)v->center[1]-(center_y-stored_vertical_width));
    for(axis=0;axis<4;++axis){q->vertices[axis].reciprocal_depth=stored_reciprocal;q->vertices[axis].w=1.0f;}
    half_uv=(float)((long double)v->half*reciprocal);
    right_uv=(float)(((long double)q->texture_width-v->half)*reciprocal);
    bottom_uv=(float)(((long double)q->texture_height-v->half)*reciprocal);
    q->uv[0][0]=q->uv[3][0]=(q->flags&0x80000000u)?half_uv:right_uv;
    q->uv[1][0]=q->uv[2][0]=(q->flags&0x80000000u)?right_uv:half_uv;
    q->uv[0][1]=q->uv[1][1]=bottom_uv;q->uv[2][1]=q->uv[3][1]=half_uv;
    q->count=4u;q->next=0u;return 1;
}
