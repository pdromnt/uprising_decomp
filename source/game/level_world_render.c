#include "level_world_render.h"
#include "level_world_surface.h"
#include "level_run.h"
#include "model_assets.h"
#include <string.h>
#include <stdio.h>

static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static float real(GameApplication *a,GameAddress p){uint32_t bits=get(a,p);float v;memcpy(&v,&bits,4);return v;}
static int begin(void *context){return game_application_world_begin(context);}
static int texture_owned(void *context,uint32_t id){
    const TextureRecord *record=game_model_texture(context,id);
    return record && !(record->flags&0x40000000u);
}
static int texture(void *context,uint32_t id,uint32_t frame,WorldTextureProperties *out){
    GameApplication *a=context;TextureRecord *record=game_model_texture_record(a,id);
    if(record && (record->flags&0x40000000u)){
        fprintf(stderr,"native world texture has no NCC owner: id=%08x flags=%08x\n",id,record->flags);return 0;}
    if(!game_level_texture_use(a,id)){
        fprintf(stderr,"native world texture use failed: id=%08x flags=%08x\n",id,record?record->flags:0);return 0;}
    record=game_model_texture_record(a,id);
    if(record){record->last_use=frame;out->flags=record->flags;}
    else{game_memory_write_u32(&a->memory,id+4,frame);out->flags=get(a,id);}
    if(out->flags&0x40000000u){
        GameAddress palette=get(a,id+24);
        /*Native bitmap records cannot claim an external palette owner. */
        if(!game_memory_read(&a->memory,palette,out->ncc,sizeof(out->ncc)))return 0;
    }
    if(a->memory.fault || !game_application_render_texture(a,id))return 0;
    world_renderer_note_texture_owner(record!=NULL);return 1;
}
static int state(void *context,WorldRenderState kind,int value){return game_application_world_state(context,kind,value);}
static int palette(void *context,const uint32_t *colors,unsigned first,unsigned last,int full){
    return game_application_world_palette(context,colors,first,last,full);
}
static int polygon(void *context,const GameRenderVertex *vertices,unsigned count){return game_application_surface_polygon(context,vertices,count);}
static int point(void *context,const GameRenderVertex *vertex){return game_application_world_point(context,vertex);}
static int ncc(void *context,const uint8_t table[112]){return game_application_world_ncc(context,table);}
static int material(void *context,uint32_t reference,MaterialValue *value){
    GameApplication *a=context;
    value->width=game_memory_read_u16(&a->memory,reference);
    value->height=game_memory_read_u16(&a->memory,reference+2);
    value->backend_handle=get(a,reference+4);return !a->memory.fault;
}
static int palette_map(void *context,GameAddress reference,uint8_t entries[4]){
    GameApplication *a=context;return game_memory_read(&a->memory,reference,entries,4);
}
/*43a630 consumes actual owned arena/scene/terrain commands and bounded legacy
 *records. Both paths execute the same ordinary C shader/geometry algorithms.
 *Unsupported records fail; there is no generated execution fallback. Frame
 *values are short-lived snapshots of their existing owners, not new state. */
int game_level_world_render(GameApplication *a,GameAddress head){
    WorldRenderFrame frame={0};int result;unsigned i;
    WorldRendererOps ops={a,begin,texture,state,palette,polygon,point,material,texture_owned,palette_map,ncc};
    frame.projection_magic=real(a,0x572d70);frame.depth_scale=real(a,0x572d7c);
    frame.fog_max=real(a,0x572d88);frame.near_offset=real(a,0x572d8c);frame.fog_scale=real(a,0x572d90);
    frame.depth_limit=(int32_t)get(a,0x597f7c);frame.texture_frame=get(a,0x63a154);
    for(i=0;i<4;++i)frame.base_palette_entries[i]=game_memory_read_u8(&a->memory,0x594458+i);
    frame.palette_bytes=get(a,0x5a07f8);frame.triangles=get(a,0x5a07fc);
    for(i=0;i<256;++i)frame.palette[i]=get(a,0x5ad3a8+i*4);
    frame.points.x_center=real(a,0x6290c8);frame.points.y_center=real(a,0x6290cc);
    frame.points.y_scale=real(a,0x6290f0);frame.points.projection=real(a,0x6290f4);
    /*43a420 uses FILD for these pixel bounds, unlike the projection floats. */
    frame.points.x_min=(float)(int32_t)get(a,0x6290e4);frame.points.y_min=(float)(int32_t)get(a,0x6290e0);
    frame.points.x_max=(float)(int32_t)get(a,0x6290ec);frame.points.y_max=(float)(int32_t)get(a,0x6290e8);
    frame.points.rounding_magic=frame.projection_magic;frame.points.palette=frame.palette;
    if(a->memory.fault)return 0;
    /* The final overlay queue uses the same polygon consumer as the world.
     * Its 2D HUD art must keep center-sample coverage. */
    game_application_world_antialias(a,head!=get(a,0x604f08));
    result=world_renderer_submit_owned(head,a->memory.terrain,&frame,&ops);
    if(result<0)result=world_renderer_submit(&a->memory,head,&frame,&ops);
    game_application_world_antialias(a,0);
    if(result!=1){WorldRendererStats stats;world_renderer_stats(&stats);
        fprintf(stderr,"native world submission failed: head=%08x result=%d packet_reason=%u address=%08x\n",
            head,result,stats.owned_reject_kind,stats.owned_reject_address);return 0;}
    game_memory_write_u32(&a->memory,0x5a07f8,frame.palette_bytes);
    game_memory_write_u32(&a->memory,0x5a07fc,frame.triangles);return !a->memory.fault;
}
static int projected_begin(void *context,ProjectedQueueKind kind){
    GameApplication *a=context;
    return game_application_projected_begin(a,kind) && (kind!=PROJECTED_TEXTURED ||
        (game_level_texture_use(a,get(a,0x597068)) && game_application_render_texture(a,get(a,0x597068))));
}
static int projected_end(void *context,ProjectedQueueKind kind){return game_application_projected_end(context,kind);}
int game_level_projected_render(GameApplication *a,ProjectedQueueKind kind){
    static const GameAddress roots[]={0x624f18,0x624efc,0x624f2c};
    ProjectedRendererOps ops={a,projected_begin,polygon,projected_end};GameAddress head;
    float magic,scale;int result;
    if((unsigned)kind>=3)return 0;
    head=get(a,roots[kind]);magic=real(a,0x575244);scale=real(a,0x575248);
    if(a->memory.fault)return 0;
    result=world_renderer_projected_native(head,kind,magic,scale,&ops);
    if(result<0)result=world_renderer_projected(&a->memory,head,kind,magic,scale,&ops);
    return result==1 && !a->memory.fault;
}
