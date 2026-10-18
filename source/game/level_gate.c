#include "level_routes.h"
#include "level_objects.h"
#include "game_initializers.h"
#include "gameplay_geometry.h"
#include "gameplay_catalog.h"
#include "gameplay_spatial.h"
#include "presentation_pose.h"
#include "presentation_state_copy.h"
#include "scene_instance_owner.h"
#include "scene_projection_storage.h"
#include "level_entity_store.h"
#include "terrain_asset.h"
#include "model_assets.h"
#include "menu_sound.h"
#include <math.h>
#include <string.h>
extern int game_level_gate_detach_event(GameApplication *,GameAddress);
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
static float value(GameApplication *a,GameAddress p) {uint32_t bits=get(a,p);float f;memcpy(&f,&bits,4);return f;}
static void store(GameApplication *a,GameAddress p,float f){uint32_t bits;memcpy(&bits,&f,4);put(a,p,bits);}
static int32_t signed_word(uint32_t bits){int32_t n;memcpy(&n,&bits,4);return n;}
static unsigned cell_coordinate(float v){
    int64_t n=!isfinite(v) || v< -0x1p63L || v>=0x1p63L?INT64_MIN:(int64_t)v;
    return ((uint32_t)(uint64_t)n>>9)&255u;
}
static GameAddress allocate(void *context,size_t size){(void)context;return scene_projection_storage_allocate(size,"gate presentation bytes");}
static int release(void *context,GameAddress p){(void)context;return scene_projection_storage_free(p);}
static int clone(void *context,GameAddress source,GameAddress *result) {
    GameApplication *a=context;return scene_instance_owner_clone(&a->memory,source,result)==1;
}
static int release_model(void *context,GameAddress model){(void)context;return scene_instance_owner_release_graph(model)==1;}

/*442a10 ->449290 ->45e230/534b90/42d180. Fresh gate inputs and
 *controller storage transfer to the same template copied by live gates. */
GameAddress game_level_gate_template_load(GameApplication *a){
    GameModelDefinition d={0};GameAddress object=get(a,0x5adb64),dimensions=0,buffer=0;unsigned i;
    if(object || a->memory.fault)return object;
    if(!game_model_definition_read(a,ASSET_MODEL_SIMPLE,0,0x5848f0,0,0,0,&d))return 0;
    if(d.marker_count>32767u)goto fail;
    object=allocate(a,0xdc);dimensions=allocate(a,12);
    if(!object || !dimensions)goto fail;
    for(i=0;i<3;++i)store(a,dimensions+i*4,d.dimensions[i]);
    game_initializer_0042d180(&a->memory,object,dimensions,d.radius,d.markers,d.marker_count,d.model);
    release(a,dimensions);dimensions=0;
    if(d.marker_count){buffer=allocate(a,d.marker_count*12u);if(!buffer)goto fail;}
    put(a,object+0x70,buffer);put(a,object+0xb0,0);
    put(a,object,0x5737e0);game_memory_write_u8(&a->memory,object+0xc,0xa2);
    put(a,object+0x10,get(a,object+0x10)|0x801);
    /*The factory publication owns its template. Orphan cleanup owns only
     *live copies; registering the template there would invalidate borrowers. */
    if(a->memory.fault)goto fail;
    put(a,0x5adb64,object);return object;
fail:
    if(dimensions)release(a,dimensions);if(buffer)release(a,buffer);if(object)release(a,object);
    (void)game_model_definition_release(a,&d);return 0;
}

/*45e2c0/534af0/42d070: ordinary copy construction from the retained template.
 * Shared input vertices stay borrowed; output/controller arrays and model clone
 * belong to the new record and existing entity owner. */
GameAddress game_level_gate_create(GameApplication *a) {
    GameAddress source=get(a,0x5adb64),object;uint32_t count;
    GamePresentationStorageServices storage={a,allocate,release,clone,release_model};
    if(!source)source=game_level_gate_template_load(a);
    if(!source || a->memory.fault)return 0;
    object=allocate(a,0xdc);if(!object)return 0;
    if(!game_presentation_base_copy(&a->memory,object,source,&storage))goto fail;
    count=game_memory_read_u16(&a->memory,source+0x44);
    if(count) {GameAddress buffer=allocate(a,count*12u);put(a,object+0x70,buffer);if(!buffer)goto fail;}
    put(a,object+0xb0,0);put(a,object,0x5737e0);
    game_memory_write_u8(&a->memory,object+0xc,0xa2);
    put(a,object+0x10,get(a,object+0x10)|0x801);
    if(a->memory.fault || !level_entity_store_track_guest(object))goto fail;
    return object;
fail:
    if(get(a,object+0x70))release(a,get(a,object+0x70));
    (void)game_presentation_base_destroy(&a->memory,object,&storage);
    release(a,object);return 0;
}
/*42ed30: settle onto the lowest center/corner terrain sample, then add the
 * vertical dimension once. Its x87 compare/swap/pop retains the smaller value;
 * selecting the maximum lifts scenery above slopes. */
int game_level_object_footprint(GameApplication *a,GameAddress object,float p[3],uint16_t heading) {
    float sine=value(a,0x6298a8+(heading&2047u)*4),cosine=value(a,0x6298a8+((heading+512u)&2047u)*4);
    float width=value(a,object+0x2c),depth=value(a,object+0x34);
    unsigned i;p[1]=(float)gameplay_geometry_terrain_height(&a->memory,p[0],p[2]);
    /*406ea0 settles the unit onto the lowest terrain sample under its actual
     * rotated hull vertices, rather than42ed30's four corners.53b6a0
     * uses the same alternating-triangle height interpolation. */
    if(get(a,get(a,object)+0x68)==0x406ea0){
        GameAddress hull=get(a,get(a,object+0x54)+0x50),vertices=get(a,hull);
        int32_t count=(int32_t)get(a,hull+0x14);
        if(count>1048576 || (count>0 && (!vertices || (uint64_t)vertices+(uint64_t)(uint32_t)count*12>UINT32_MAX)))return 0;
        for(i=0;count>0 && i<(unsigned)count;i++){
            float x=value(a,vertices+i*12),z=value(a,vertices+i*12+8);
            float wx=(float)((long double)x*cosine+(long double)z*sine+p[0]);
            float wz=(float)((long double)x*-sine+(long double)z*cosine+p[2]);
            long double height=gameplay_geometry_terrain_height(&a->memory,wx,wz);
            if(height<p[1])p[1]=(float)height;
        }
        p[1]=(float)((long double)p[1]+value(a,object+0x30));return !a->memory.fault;
    }
    /* The original reuses stored binary32 products between corners. Preserve
     * those boundaries and addition order when sampling terrain triangles. */
    {
    float wc=(float)((long double)width*cosine),nws=(float)((long double)width*-sine);
    float nds=(float)((long double)-depth*sine),ndc=(float)((long double)-depth*cosine);
    float nwc=(float)((long double)-width*cosine),ws=(float)((long double)-width*-sine);
    float corners[4][2]={
        {(float)(((long double)depth*sine+p[0])+wc),(float)(((long double)depth*cosine+p[2])+nws)},
        {(float)(((long double)-depth*sine+p[0])+wc),(float)(((long double)p[2]+nws)+ndc)},
        {(float)(((long double)p[0]+nds)+nwc),(float)(((long double)-width*-sine+p[2])+ndc)},
        {(float)(((long double)depth*sine+p[0])+nwc),(float)(((long double)depth*cosine+p[2])+ws)}
    };
    for(i=0;i<4;++i) {
        float world_x=corners[i][0],world_z=corners[i][1];
        float height=(float)gameplay_geometry_terrain_height(&a->memory,world_x,world_z);
        if(!(height>p[1]))p[1]=height;
    }
    }
    p[1]=(float)((long double)p[1]+value(a,object+0x30));
    return !a->memory.fault;
}
typedef struct GatePlacement {GameApplication *a;GameAddress object;} GatePlacement;
static int property(void *context,unsigned index,uint32_t *result) {
    GatePlacement *p=context;return gameplay_catalog_object_property(&p->a->memory,p->object,index,result)==1;
}
static int spatial(void *context) {
    GatePlacement *p=context;GameApplication *a=p->a;GameAddress bucket;float x=value(a,p->object+0x18),z=value(a,p->object+0x20);
    if(!isfinite(x)||!isfinite(z)||!gameplay_spatial_query_group(0x5f0800,x,z,&bucket))return 0;
    (void)gameplay_spatial_insert(&a->memory,0x5f0800,p->object,(int32_t)x>>12,(int32_t)z>>12,(get(a,p->object+0x10)&0x400)!=0);
    return !a->memory.fault;
}
/*45e810: gate controller matrices share the actual model holder; its cache
 * bit is set only when local/world position and yaw coincide in idle states. */
static int controller(void *context,unsigned operation) {
    GatePlacement *p=context;GameApplication *a=p->a;GameAddress o=p->object,model=get(a,o+0x54);float matrix[12]={0};unsigned i;
    uint16_t yaw=game_memory_read_u16(&a->memory,o+0x78);uint32_t flags=get(a,o+0x10),state=get(a,o+0xb8);
    if(operation!=1 || !model)return 0;
    if(flags&0x40000)return !a->memory.fault;
    matrix[0]=matrix[10]=value(a,0x6298a8+((yaw+512u)&2047u)*4);
    matrix[2]=value(a,0x6298a8+(yaw&2047u)*4);matrix[8]=-matrix[2];matrix[5]=1;
    for(i=0;i<3;++i)matrix[i*4+3]=value(a,o+0x64+i*4);
    for(i=0;i<12;++i)store(a,model+4+i*4,matrix[i]);
    if((state==0 || state==1) && value(a,o+0x18)==value(a,o+0x64) &&
       value(a,o+0x1c)==value(a,o+0x68) && value(a,o+0x20)==value(a,o+0x6c) &&
       game_memory_read_u16(&a->memory,o+0x24)==yaw)put(a,o+0x10,flags|0x40000);
    return !a->memory.fault;
}
/*The existing record/model/controller buffers stay authoritative. Borrow only
 *the inputs needed by the shared vertex algorithm; do not import/export the
 *whole legacy pose on every gate update and rendered frame. */
int game_level_gate_frame(GameApplication *a,GameAddress o,int rendered){
    PresentationPose pose={0};GatePlacement context={a,o};unsigned i;
    if(!a || a->memory.fault || get(a,o)!=0x5737e0)return 0;
    if(rendered){
        if(!controller(&context,1))return 0;
        pose.model=get(a,o+0x54);pose.controller=get(a,o+0x70);
    }else{
        for(i=0;i<3;i++){pose.position[i]=value(a,o+24+i*4);pose.angles[i]=game_memory_read_u16(&a->memory,o+36+i*2);}
        gameplay_geometry_orientation_values(&a->memory,pose.angles,pose.position,pose.world_matrix);
        for(i=0;i<12;i++)store(a,o+0x80+i*4,pose.world_matrix[i]);
        pose.vertices=get(a,o+0x40);
    }
    pose.flags=get(a,o+0x10);
    pose.hull=get(a,o+0x3c);pose.vertex_count=game_memory_read_u16(&a->memory,o+0x44);
    if(a->memory.fault || !presentation_pose_transform_vertices(&a->memory,&pose,rendered))return 0;
    put(a,o+0x10,pose.flags);return !a->memory.fault;
}
int game_level_gate_controller(GameApplication *a,GameAddress o){GatePlacement p={a,o};return controller(&p,1);}
static void occupancy_bit(GameApplication *a,GameAddress p,unsigned bit,int enabled){
    uint8_t v=game_memory_read_u8(&a->memory,p),mask=(uint8_t)(1u<<bit);
    game_memory_write_u8(&a->memory,p,enabled?(uint8_t)(v|mask):(uint8_t)(v&~mask));
}
/*45ebc0/45eff0: the full and half-resolution occupancy planes are the same
 *owned work/visibility grids used by routes and path finding. Preserve bits
 *which belonged to the terrain before this gate was placed. */
int game_level_gate_occupancy(GameApplication *a,GameAddress o,int enabled){
    unsigned x=cell_coordinate(value(a,o+24)),z=cell_coordinate(value(a,o+32));
    if(enabled || !get(a,o+0xd0))occupancy_bit(a,0x610520+z*32+x/8,x&7,enabled);
    if(enabled || !get(a,o+0xd4))occupancy_bit(a,0x5e086c+(z/2)*16+(x/2)/8,(x/2)&7,enabled);
    return !a->memory.fault;
}
/*45e9e0/45eb60 also serve the13-tick propagation from45e570. */
int game_level_gate_begin(GameApplication *a,GameAddress o,uint32_t state){
    if(!a || a->memory.fault || (state!=2 && state!=3))return 0;
    if(get(a,o+0xd8) || !(get(a,o+20)&64u)){
        SoundPlaybackRequest sound={0};GameAddress camera=get(a,0x58f890);
        long double x=(long double)value(a,o+24)-value(a,camera+12),z=(long double)value(a,o+32)-value(a,camera+20);
        sound.sound=132;sound.volume=1;sound.distance_squared=(float)(x*x+z*z);
        if(a->memory.fault || !game_menu_sound_play(a,&sound))return 0;
    }
    put(a,o+0xb8,state);put(a,o+0xb4,get(a,0x5ac6ac));put(a,o+16,get(a,o+16)&~0x40000u);
    return state==2?game_level_gate_occupancy(a,o,1):!a->memory.fault;
}
/*45e570: advance the existing height/yaw and propagate to the borrowed next
 *segment exactly13 ticks after activation. No host allocation or CPU callback. */
int game_level_gate_update(GameApplication *a,GameAddress o){
    uint32_t state=get(a,o+0xb8);GameAddress next;float y,limit;uint16_t yaw;
    if(a->memory.fault)return 0;
    if(state!=2 && state!=3)return 1;
    y=(float)((long double)value(a,o+28)+(state==2?(long double)value(a,o+0xc4):-(long double)value(a,o+0xc4)));
    store(a,o+28,y);yaw=game_memory_read_u16(&a->memory,o+36);
    game_memory_write_u16(&a->memory,o+36,(uint16_t)((yaw+(state==2?16u:0xfff0u))&2047u));
    limit=value(a,o+(state==2?0xbc:0xc0));
    if(state==2 && y>limit){
        put(a,o+28,get(a,o+0xbc));put(a,o+0xb8,0);
        game_memory_write_u16(&a->memory,o+36,game_memory_read_u16(&a->memory,o+0xc8));
    }else if(state==3 && !(y>=limit)){
        put(a,o+28,get(a,o+0xc0));put(a,o+0xb8,1);
        if(!game_level_gate_occupancy(a,o,0))return 0;
    }
    next=get(a,o+0xcc);
    if(next && get(a,o+0xb4)-get(a,0x5ac6ac)==0xfffffff3u && !game_level_gate_begin(a,next,state))return 0;
    return game_level_gate_frame(a,o,0);
}
/*45e8e0: the hidden state skips submission. Moving states extrapolate from
 *the clock into existing local fields, then rebuild controller vertices. */
int game_level_gate_present(GameApplication *a,GameAddress o){
    uint32_t state=get(a,o+0xb8),delta;unsigned i;uint16_t yaw;
    if(a->memory.fault)return 0;
    if(state==1)return 1;
    for(i=0;i<3;i++)put(a,o+0x64+i*4,get(a,o+24+i*4));
    yaw=game_memory_read_u16(&a->memory,o+36)&2047u;
    if(get(a,0x5eff04) && (state==2 || state==3)){
        long double p,limit;delta=get(a,0x5acbe8)-get(a,0x5acc80);
        p=(long double)value(a,o+0x68)+(state==2?1:-1)*(long double)signed_word(delta)*value(a,o+0xc4);
        limit=value(a,o+(state==2?0xbc:0xc0));
        /*Original ordered/unordered min/max decisions, before float store. */
        p=state==2?(limit>p?p:limit):(limit<p || isnan(limit) || isnan(p)?p:limit);
        store(a,o+0x68,(float)p);
        yaw=(uint16_t)((yaw+(state==2?delta*16u:0u-delta*16u))&2047u);
    }
    game_memory_write_u16(&a->memory,o+0x78,yaw);
    return !a->memory.fault && game_level_gate_frame(a,o,1) && game_model_graph_update(a,get(a,o+0x54));
}
/*45e340 ->536250/42d420: recover real placement arguments and use existing
 * pose/world/controller/vertex, catalog and spatial algorithms directly. */
int game_level_gate_place(GameApplication *a,GameAddress o,uint32_t id,const float input[3],uint16_t yaw,int first) {
    float p[3];PresentationPose pose;PresentationPlacement request={0};
    GatePlacement context={a,o};PresentationPlacementServices services={&context,2u,controller,property,spatial,0};
    uint32_t ix,iz;GameAddress cell;uint8_t cell_flags;
    memcpy(p,input,sizeof(p));if(!game_level_object_footprint(a,o,p,yaw))return 0;
    p[1]=(float)((long double)p[1]+gameplay_geometry_detail_height(&a->memory,p[0],p[2]));
    if(!isfinite(p[0]) || !isfinite(p[2]) || p[0]<-2147483648.0f || p[0]>=2147483648.0f ||
       p[2]<-2147483648.0f || p[2]>=2147483648.0f)return 0;
    ix=(uint32_t)(int32_t)p[0];iz=(uint32_t)(int32_t)p[2];
    cell=get(a,0x597d90)+((iz&0xfffffe00u)*4)+((ix&0xfffffe3fu)>>6);
    cell_flags=a->memory.terrain? a->memory.terrain->cells[((iz>>9)&255u)*256+((ix>>9)&255u)].flags:
        game_memory_read_u8(&a->memory,cell+5);
    if(cell_flags&0x40)p[1]=(float)((long double)p[1]-value(a,0x57387c));
    else if(cell_flags&0x10)p[1]=(float)((long double)p[1]-value(a,0x573880));
    put(a,o+0xd8,first!=0);put(a,o+0xb8,0);put(a,o+0xb4,0);
    put(a,o+0xd4,(uint32_t)game_memory_read_u8(&a->memory,0x610520+((iz>>9)&255u)*32+((ix>>9)&255u)/8)&(1u<<((ix>>9)&7u)));
    put(a,o+0xd0,(uint32_t)game_memory_read_u8(&a->memory,0x5e086c+(uint32_t)((int32_t)(iz&0xfffffc3fu)>>6)+(uint32_t)((int32_t)ix>>13))&(1u<<((ix>>10)&7u)));
    if(!presentation_pose_import(&a->memory,o,&pose))return 0;
    request.identity=id;memcpy(request.position,p,sizeof(p));request.angles[0]=yaw;request.controller=1;
    request.descriptor=get(a,0x5f0210+(id&63u)*4)+0x10;
    if(!presentation_pose_place(&a->memory,o,&pose,&request,&services))return 0;
    store(a,o+0xc0,(float)((long double)value(a,o+0x30)*value(a,0x573884)+value(a,o+0x1c)));
    store(a,o+0xc4,(float)((long double)value(a,o+0x30)*value(a,0x573888)*value(a,0x57388c)));
    game_memory_write_u16(&a->memory,o+0xc8,game_memory_read_u16(&a->memory,o+0x24)&2047u);
    put(a,o+0xbc,get(a,o+0x1c));return !a->memory.fault;
}
int game_level_gate_destroy(GameApplication *a,GameAddress o) {
    GamePresentationStorageServices storage={a,allocate,release,clone,release_model};GameAddress buffer=get(a,o+0x70),event=get(a,o+0xb0);
    if(event && !game_level_gate_detach_event(a,event))return 0;
    put(a,o+0xb0,0);
    if(buffer && !release(a,buffer))return 0;
    put(a,o+0x70,0);
    return game_presentation_base_destroy(&a->memory,o,&storage) && release(a,o);
}
