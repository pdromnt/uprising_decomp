#include "campaign_world.h"
#include "model_assets.h"
#include "menu_widgets.h"
#include "game_matrix.h"
#include "gameplay_geometry.h"
#include "render_packet_view.h"
#include "software_raster.h"
#include <math.h>
#include <string.h>
extern int game_campaign_planet_present(GameApplication *,GameAddress);
extern int game_model_graph_update(GameApplication *,GameAddress);
extern int game_application_render_line(GameApplication *,const int32_t[4],uint8_t);
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
static float real(GameApplication *a,GameAddress p){uint32_t b=get(a,p);float v;memcpy(&v,&b,4);return v;}
static void store(GameApplication *a,GameAddress p,float v){uint32_t b;memcpy(&b,&v,4);put(a,p,b);}
/*532250, with the previews' actual rectangle arguments. Arithmetic retains
 * the original extended intermediates and explicit float stores. */
static int projection(GameApplication *a,int32_t width,int32_t height){
    float half=real(a,0x5731b0),center[2],bounds[4]={0,0,(float)width,(float)height};
    center[0]=(float)((long double)width*half);center[1]=(float)((long double)height*half);
    return gameplay_geometry_projection_setup(&a->memory,0x6290c8,center,(float)width,(float)height,bounds);
}
static int frame_begin(GameApplication *a,GameAddress image,GameMenuPixelSpan *span){
    if(!game_menu_text_surface_fill(a,image,254) || !game_menu_surface_dimensions(a,image,span))return 0;
    if(!render_arena_current() && !render_arena_initialize())return 0;
    render_arena_begin_frame();
    /*The same authoritative native banks used by model submission. Reset
     * cursors once per offscreen frame, without serialized pool mirrors. */
    put(a,0x604378,render_arena_view(0));put(a,0x604268,render_arena_view(3));
    put(a,0x604480,render_arena_view(2));put(a,0x604270,render_arena_view(1));
    put(a,0x604484,0);put(a,0x6252f8,0);
    put(a,0x627928,(uint32_t)span->width);put(a,0x62792c,(uint32_t)span->height);
    return projection(a,span->width,span->height);
}
static int packet_draw(GameApplication *a,const SoftwareRasterFrame *frame,GameAddress ref,int draw){
    const RenderArenaPacket *p=render_packet_view(ref);const uint32_t *indices;
    const RenderArenaVertex *vertices[25];const RenderArenaUv *uv=NULL;
    SoftwareRasterTexture texture={0};const TextureRecord *record;MaterialValue material;unsigned i;uint32_t clipped_indices[32];
    if(!p || p->count<3 || p->count>25 || (p->flags&~0x119u))return 0;
    indices=render_packet_indices(p->vertices,p->count,clipped_indices);
    if(!indices)return 0;
    for(i=0;i<p->count;++i){vertices[i]=render_arena_vertices(indices[i],1);
        if(!vertices[i])vertices[i]=scene_projection_storage_transformed(indices[i],1);
        if(!vertices[i])return 0;
    }
    if(p->flags&1u){
        uv=render_arena_uv(p->attributes,p->count);
        if(!uv)uv=scene_projection_storage_work_uv(p->attributes,p->count);
        if(!uv)return 0;
        if(!scene_projection_storage_material(ref,&material)){
            material.width=game_memory_read_u16(&a->memory,p->material);
            material.height=game_memory_read_u16(&a->memory,p->material+2);
            material.backend_handle=get(a,p->material+4);
        }
        record=game_model_texture(a,material.backend_handle);
        if(a->memory.fault || !record || record->image.format!=5 || !material.width || !material.height)return 0;
        texture.pixels=record->image.pixels;texture.bytes=record->image.size;
        texture.width=material.width;texture.height=material.height;
        if((uint64_t)material.width*material.height>texture.bytes)return 0;
    }
    return !draw || software_raster_polygon(frame,p->flags,(uint8_t)p->material,vertices,uv,p->count,&texture)==1;
}
static int queue_draw(GameApplication *a,const SoftwareRasterFrame *frame,GameAddress head){
    unsigned pass,n;GameAddress ref,next;RenderArenaPacket *owned;
    /*Preflight owners/modes before emitting pixels; bound cycles and oversized
     * queues by the actual packet bank capacity. */
    for(pass=0;pass<2;++pass){
        for(ref=head,n=0;ref;ref=next){
            if(++n>15000 || !packet_draw(a,frame,ref,pass!=0) || !render_packet_link(ref,&next,&owned))return 0;
        }
    }
    return 1;
}
/*51a2f0 is a wire outline, not the world polygon renderer. Its software and
 * SDK branches consume the same packet edges, including the closing edge. */
static int wire_packet(GameApplication *a,const SoftwareRasterFrame *frame,GameAddress ref,uint32_t color,int hardware,int draw){
    const RenderArenaPacket *p=render_packet_view(ref);const uint32_t *indices;
    const RenderArenaVertex *vertices[32];int32_t xy[32][2],bounds[4]={0,0,frame->width,frame->height};unsigned i,axis;uint32_t clipped_indices[32];
    if(!p)return 0;
    if(p->flags&0x1000u)return 1;
    if(!p->count || p->count>32)return 0;
    indices=render_packet_indices(p->vertices,p->count,clipped_indices);
    if(!indices)return 0;
    if(color==UINT32_MAX){
        if(p->flags&1u){MaterialValue material;const TextureRecord *texture;
            if(!scene_projection_storage_material(ref,&material))material.backend_handle=get(a,p->material+4);
            texture=game_model_texture(a,material.backend_handle);
            if(!texture || !texture->image.pixels || !texture->image.size)return 0;
            color=texture->image.pixels[0];
        }else if(p->flags&2u){
            const RenderArenaUv *uv=render_arena_uv(p->attributes,1);float value;
            if(!uv)uv=scene_projection_storage_work_uv(p->attributes,1);
            value=uv?uv->u:real(a,p->attributes);
            color=isfinite(value) && value>=INT32_MIN && value<2147483648.0f?(uint32_t)(int32_t)truncl(value):0x80000000u;
        }else color=p->material;
    }
    for(i=0;i<p->count;++i){
        vertices[i]=render_arena_vertices(indices[i],1);
        if(!vertices[i])vertices[i]=scene_projection_storage_transformed(indices[i],1);
        if(!vertices[i])return 0;
        for(axis=0;axis<2;++axis){float value=vertices[i]->position[axis];
            if(!isfinite(value) || value<INT32_MIN || value>=2147483648.0f)return 0;
            xy[i][axis]=(int32_t)truncl(value);
        }
    }
    if(a->memory.fault)return 0;
    if(draw)for(i=0;i<p->count;++i){unsigned next=(i+1)%p->count;
        int32_t coordinates[4]={xy[i][0],xy[i][1],xy[next][0],xy[next][1]};
        if(software_raster_clip_line(coordinates,bounds) &&
           !(hardware?game_application_render_line(a,coordinates,(uint8_t)color):software_raster_line(frame,coordinates,(uint8_t)color)))return 0;
    }
    return 1;
}
int game_menu_preview_hardware_draw(GameApplication *a,GameAddress image,GameAddress head,uint32_t color){
    GameMenuPixelSpan span;SoftwareRasterFrame frame={0};GameAddress ref,next;RenderArenaPacket *owned;
    uint32_t saved[4];unsigned i,pass,n;int hardware,result=1,unlocked;
    if(!game_menu_surface_pixels_begin(a,image,&span))return 0;
    frame.color=span.pixels;frame.color_bytes=span.bytes;frame.width=span.width;frame.height=span.height;frame.color_stride=span.stride;
    hardware=get(a,0x5840f0)!=0;
    for(i=0;i<4;++i)saved[i]=get(a,0x597f50+i*4);
    put(a,0x597f50,0);put(a,0x597f54,0);put(a,0x597f58,(uint32_t)span.width);put(a,0x597f5c,(uint32_t)span.height);
    for(pass=0;pass<2 && result;++pass)for(ref=head,n=0;ref;ref=next){
        if(++n>15000 || !wire_packet(a,&frame,ref,color,hardware,pass!=0) || !render_packet_link(ref,&next,&owned)){result=0;break;}
    }
    for(i=0;i<4;++i)put(a,0x597f50+i*4,saved[i]);
    unlocked=game_menu_surface_pixels_end(a,image);
    return result && unlocked && !a->memory.fault;
}
static int frame_finish(GameApplication *a,GameAddress image,GameAddress depth_id,const GameMenuPixelSpan *dimensions,int hardware){
    size_t bytes=(size_t)dimensions->width*(size_t)dimensions->height*2;uint8_t *depth;
    GameMenuPixelSpan span;SoftwareRasterFrame frame;int result,unlocked;
    depth=scene_projection_storage_borrow_bytes(depth_id,bytes);if(!depth)return 0;
    memset(depth,255,bytes);
    if(hardware)return game_menu_preview_hardware_draw(a,image,get(a,0x6252f8),251);
    if(!game_menu_surface_pixels_begin(a,image,&span))return 0;
    frame.color=span.pixels;frame.color_bytes=span.bytes;frame.depth=depth;frame.depth_bytes=bytes;
    frame.width=dimensions->width;frame.height=dimensions->height;frame.color_stride=span.stride;frame.depth_stride=dimensions->width*2;
    result=span.width==frame.width && span.height==frame.height && queue_draw(a,&frame,get(a,0x6252f8));
    unlocked=game_menu_surface_pixels_end(a,image);return result && unlocked && !a->memory.fault;
}
/*44cb30: actual campaign camera, planets, animated textures and visible queue. */
int game_campaign_preview_render(GameApplication *a,GameAddress preview){
    GameAddress image=get(a,preview+0x178),camera=get(a,preview+0x170),system,planet;
    GameMenuPixelSpan span;float position[3];uint32_t count,i;unsigned axis;
    if(!camera || !frame_begin(a,image,&span))return 0;
    for(axis=0;axis<3;++axis)position[axis]=real(a,camera+0xc+axis*4);
    if(!game_matrix_camera_view(&a->memory,0x5cedf0,position,game_memory_read_u16(&a->memory,camera+10),
        game_memory_read_u16(&a->memory,camera+8),game_memory_read_u16(&a->memory,camera+6)))return 0;
    system=game_campaign_system(a,get(a,get(a,0x585408)+0x1c));if(!system)return 0;
    count=get(a,system+4);if(count>4096)return 0;
    for(i=1;i<count;++i){planet=get(a,get(a,system)+i*4);if(planet && !game_campaign_planet_present(a,planet))return 0;}
    return frame_finish(a,image,get(a,preview+0x174),&span,0);
}
/*44de80: research model shares the frame/queue algorithm and existing owners. */
int game_menu_preview_render(GameApplication *a,GameAddress preview){
    GameAddress image=get(a,preview+0x178),model=get(a,preview+0x180);GameMenuPixelSpan span;
    if(!frame_begin(a,image,&span) || (model && !game_model_graph_update(a,model)))return 0;
    return frame_finish(a,image,get(a,preview+0x17c),&span,get(a,preview+0x170)!=0);
}
