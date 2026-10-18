#include "scene_surface.h"
#include <stdlib.h>
#include <string.h>
/* Legacy table addresses serialize the typed surface. Clipping can touch the
 * tail of a page, so only out-of-range writes allocate a spill page. */
static const size_t table_window=0x1000u;
void scene_surface_release(SceneSurface *surface){
    unsigned i;if(!surface)return;
    if(surface->texture_handle && surface->release_texture)
        surface->release_texture(surface->texture_context,surface->texture_handle);
    surface->release_texture=NULL;surface->texture_context=NULL;
    for(i=0;i<5;i++){free(surface->table[i]);surface->table[i]=NULL;surface->address[i]=0;}
    surface->texture_handle=0;
    surface->links_clean=0;surface->active=0;
}
int scene_surface_take_texture(SceneSurface *surface,uint32_t handle,
    void (*release)(void *,uint32_t),void *context){
    if(!surface || !surface->active || !handle || !release ||
       surface->texture_handle)return 0;
    surface->texture_handle=handle;
    surface->release_texture=release;
    surface->texture_context=context;
    return 1;
}
int scene_surface_initialize(SceneSurface *surface){
    unsigned i,row,column;
    if(!surface)return 0;
    scene_surface_release(surface);
    memset(surface->polygons,0,sizeof(surface->polygons));
    memset(surface->source,0,sizeof(surface->source));
    memset(surface->transformed,0,sizeof(surface->transformed));
    memset(surface->links,0,sizeof(surface->links));
    memset(surface->uv,0,sizeof(surface->uv));
    surface->native_submissions=0;
    surface->native_queue_packets=0;
    surface->typed_queue_packets=0;
    surface->owned_ready_frames=0;
    memset(surface->last_cursors,0,sizeof(surface->last_cursors));
    surface->phase=0.0f;surface->published_phase_bits=0u;
    surface->phase_active=0;
    for(i=0;i<5;i++)
        surface->address[i]=SCENE_SURFACE_BASE+i*0x1000u;
    for(row=0;row<5;row++)for(column=0;column<5;column++){
        uint32_t offsets[4]={row*96+column*16,row*96+column*16+16,
            row*96+column*16+112,row*96+column*16+96};
        unsigned corner;
        for(corner=0;corner<4;corner++){
            surface->links[row*5+column][corner]=
                surface->address[2]+offsets[corner];
        }
    }
    surface->links_clean=1;surface->active=1;
    return 1;
}
static int locate(const SceneSurface *surface,GameAddress address,size_t size,
                  unsigned *index,size_t *offset){
    unsigned i;if(!surface || !surface->active ||
        address<SCENE_SURFACE_BASE)return 0;
    i=(address-SCENE_SURFACE_BASE)/0x1000u;
    if(i>=5)return 0;
    *offset=(size_t)(address-surface->address[i]);
    if(*offset>table_window || size>table_window-*offset)return 0;
    *index=i;return 1;
}
int scene_surface_read(const SceneSurface *surface,GameAddress address,void *data,size_t size){
    unsigned i;size_t offset,owned_size,count;
    const uint8_t *owned;
    if(!data || !locate(surface,address,size,&i,&offset))return 0;
    owned=i==0u?(const uint8_t *)surface->polygons:
        i==1u?(const uint8_t *)surface->source:
        i==2u?(const uint8_t *)surface->transformed:
        i==3u?(const uint8_t *)surface->links:(const uint8_t *)surface->uv;
    owned_size=i==0u?sizeof(surface->polygons):
        i==1u?sizeof(surface->source):
        i==2u?sizeof(surface->transformed):
        i==3u?sizeof(surface->links):sizeof(surface->uv);
    count=offset<owned_size?owned_size-offset:0u;
    if(count>size)count=size;
    if(count)memcpy(data,owned+offset,count);
    if(count<size){
        if(surface->table[i])memcpy((uint8_t *)data+count,
            surface->table[i]+offset+count,size-count);
        else memset((uint8_t *)data+count,0,size-count);
    }
    return 1;
}
static int write_surface(SceneSurface *surface,GameAddress address,
    const void *data,size_t size,int pipeline){
    unsigned i;size_t offset,owned_size,count;
    uint8_t *owned;
    if(!data || !locate(surface,address,size,&i,&offset))return 0;
    owned=i==0u?(uint8_t *)surface->polygons:
        i==1u?(uint8_t *)surface->source:
        i==2u?(uint8_t *)surface->transformed:
        i==3u?(uint8_t *)surface->links:(uint8_t *)surface->uv;
    owned_size=i==0u?sizeof(surface->polygons):
        i==1u?sizeof(surface->source):
        i==2u?sizeof(surface->transformed):
        i==3u?sizeof(surface->links):sizeof(surface->uv);
    count=offset<owned_size?owned_size-offset:0u;
    if(count>size)count=size;
    if(count)memcpy(owned+offset,data,count);
    if(count<size){
        if(!surface->table[i]){
            surface->table[i]=calloc(1,table_window);
            if(!surface->table[i])return 0;
        }
        memcpy(surface->table[i]+offset+count,
            (const uint8_t *)data+count,size-count);
    }
    /* Only the pipeline may update packet and UV work in place. External
     * edits can change topology without updating the typed frame. */
    if(size && (i==1u || i==3u || (!pipeline && (i==0u || i==4u))))
        surface->links_clean=0;
    return 1;
}
int scene_surface_write(SceneSurface *surface,GameAddress address,
    const void *data,size_t size){
    return write_surface(surface,address,data,size,0);
}
int scene_surface_write_pipeline(SceneSurface *surface,GameAddress address,
    const void *data,size_t size){
    return write_surface(surface,address,data,size,1);
}
int scene_surface_set_polygon(SceneSurface *surface,unsigned index,
    const float values[8]){
    RenderArenaPacket *packet;
    if(!surface || index>=25u || !values || !surface->links_clean)return 0;
    packet=&surface->polygons[index];
    packet->count=4u;
    packet->vertices=surface->address[3]+index*16u;
    packet->attributes=surface->address[4]+index*32u;
    memcpy(surface->uv[index],values,32u);
    return 1;
}
int scene_surface_set_vertex(SceneSurface *surface,unsigned index,float x,
    float y,float z){
    AssetMeshVertex *vertex;
    if(!surface || index>=36u || !surface->links_clean)return 0;
    vertex=&surface->source[index];
    memcpy(&vertex->x_bits,&x,4);memcpy(&vertex->y_bits,&y,4);
    memcpy(&vertex->z_bits,&z,4);
    return 1;
}
RenderArenaVertex *scene_surface_transformed(SceneSurface *surface,
    GameAddress address,size_t count){
    size_t offset;
    if(!surface || !surface->active || !surface->links_clean ||
       address<surface->address[2])return NULL;
    offset=(size_t)(address-surface->address[2]);
    if(offset%sizeof(RenderArenaVertex) ||
       offset>sizeof(surface->transformed) ||
       count>(sizeof(surface->transformed)-offset)/sizeof(RenderArenaVertex))
        return NULL;
    return surface->transformed+offset/sizeof(RenderArenaVertex);
}
float *scene_surface_transformed_component(SceneSurface *surface,
    GameAddress address){
    size_t offset;
    RenderArenaVertex *vertex;
    if(!surface || !surface->active || !surface->links_clean ||
       address<surface->address[2])return NULL;
    offset=(size_t)(address-surface->address[2]);
    if(offset%sizeof(float) || offset>=sizeof(surface->transformed))return NULL;
    vertex=surface->transformed+offset/sizeof(RenderArenaVertex);
    return offset%sizeof(RenderArenaVertex)==12u?&vertex->light:
        &vertex->position[(offset%sizeof(RenderArenaVertex))/sizeof(float)];
}
RenderArenaPacket *scene_surface_packet(SceneSurface *surface,
    GameAddress address){
    size_t offset;
    if(!surface || !surface->active || !surface->links_clean ||
       address<surface->address[0])return NULL;
    offset=(size_t)(address-surface->address[0]);
    if(offset%sizeof(RenderArenaPacket) ||
       offset>=sizeof(surface->polygons))return NULL;
    return surface->polygons+offset/sizeof(RenderArenaPacket);
}
const GameAddress *scene_surface_link(const SceneSurface *surface,
    GameAddress address){
    size_t offset;
    if(!surface || !surface->active || !surface->links_clean ||
       address<surface->address[3])return NULL;
    offset=(size_t)(address-surface->address[3]);
    if(offset%sizeof(GameAddress) || offset>=sizeof(surface->links))return NULL;
    return (const GameAddress *)((const uint8_t *)surface->links+offset);
}
int scene_surface_uv_component(const SceneSurface *surface,
    GameAddress address,float *value){
    size_t offset;
    if(!surface || !surface->active || !surface->links_clean || !value ||
       address<surface->address[4])return 0;
    offset=(size_t)(address-surface->address[4]);
    if(offset%sizeof(uint32_t) || offset>table_window-sizeof(*value))return 0;
    if(offset<sizeof(surface->uv))
        memcpy(value,(const uint8_t *)surface->uv+offset,sizeof(*value));
    else if(surface->table[4])
        memcpy(value,surface->table[4]+offset,sizeof(*value));
    else *value=0.0f;
    return 1;
}
int scene_surface_set_uv_component(SceneSurface *surface,
    GameAddress address,float value){
    size_t offset;
    if(!surface || !surface->active || !surface->links_clean ||
       address<surface->address[4])return 0;
    offset=(size_t)(address-surface->address[4]);
    if(offset%sizeof(uint32_t) || offset>table_window-sizeof(value))return 0;
    if(offset<sizeof(surface->uv)){
        memcpy((uint8_t *)surface->uv+offset,&value,sizeof(value));
        return 1;
    }
    return scene_surface_write_pipeline(surface,address,&value,sizeof(value));
}
int scene_surface_animate(SceneSurface *surface,
    const SceneSurfaceAnimationInput *input,float matrix[12],float *phase){
    float u,v,u1,v1,x0,y,z;uint32_t external_bits;
    long double advanced,c,s,cn,sn,p,q;
    unsigned i,row,column;
    if(!surface || !input || !matrix || !phase || !surface->links_clean)
        return 0;
    memcpy(&external_bits,&input->external_phase,4u);
    if(!surface->phase_active || external_bits!=surface->published_phase_bits)
        surface->phase=input->external_phase;
    advanced=(long double)surface->phase+input->phase_step;
    surface->phase=(float)advanced;
    if(advanced>input->period)
        surface->phase=(float)((long double)surface->phase-input->period);
    memcpy(&surface->published_phase_bits,&surface->phase,4u);
    surface->phase_active=1;
    *phase=surface->phase;
    {
        float gx=(float)((long double)input->camera[0]*input->grid_factor);
        float gz=(float)((long double)input->camera[2]*input->grid_factor);
        int32_t ix=(int32_t)gx,iz=(int32_t)gz;
        if(gx<0)--ix;
        if(gz<0)--iz;
        u=(float)(((long double)ix*input->x_wrap+input->camera[0])*
            input->uv_scale);
        v=(float)(((long double)iz*input->z_wrap+input->camera[2])*
            input->uv_scale+surface->phase);
        u=(float)((long double)u+surface->phase);
        u1=(float)((long double)u+input->period);
        v1=(float)((long double)v+input->period);
    }
    for(i=0;i<25u;i++){
        const float values[8]={u,v,u1,v,u1,v1,u,v1};
        if(!scene_surface_set_polygon(surface,i,values))return 0;
    }
    x0=(float)((long double)input->camera[0]-input->origin);
    z=(float)((long double)input->camera[2]-input->origin);
    y=(float)((long double)input->camera[1]+input->height);
    for(row=0;row<6u;row++){
        float x=x0;
        for(column=0;column<6u;column++){
            if(!scene_surface_set_vertex(surface,row*6u+column,x,y,z))return 0;
            x=(float)((long double)x+input->grid_step);
        }
        z=(float)((long double)z+input->grid_step);
    }
    c=input->cosine;s=input->sine;
    cn=input->inverse_cosine;sn=input->inverse_sine;
    p=input->pitch_scale;q=input->vertical_scale;
    {
        float ns=(float)-s,qs=(float)(q*s),qc=(float)(q*c),nsn=(float)-sn;
        memset(matrix,0,12u*sizeof(float));
        matrix[0]=(float)((long double)qs*nsn+cn*c);
        matrix[1]=(float)((long double)ns*cn+(long double)qc*nsn);
        matrix[2]=(float)(-p*nsn);
        matrix[4]=(float)((long double)qs*cn+sn*c);
        matrix[5]=(float)((long double)qc*cn+(long double)ns*sn);
        matrix[6]=(float)(cn*-p);
        matrix[8]=(float)(s*p);matrix[9]=(float)(p*c);matrix[10]=(float)q;
    }
    return 1;
}
