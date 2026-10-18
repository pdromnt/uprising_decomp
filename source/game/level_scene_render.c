#include "level_scene_render.h"
#include "level_routes.h"
#include "vehicle_presentation.h"
#include "level_run.h"
#include "level_factory.h"
#include "level_world_surface.h"
#include "level_world_render.h"
#include "model_assets.h"
#include "level_effect_runtime.h"
#include "gameplay_spatial.h"
#include "game_initializers.h"
#include "gameplay_leaves.h"
#include "scaled_effect_model.h"
#include "curve_effect_model.h"
#include "game_matrix.h"
#include "gameplay_geometry.h"
#include "scene_projection_storage.h"
#include "radar_compositor.h"
#include "sprite_fragment.h"
#include "attachment_effect.h"
#include "descent_payload.h"
#include "trail_model.h"
#include "player_rig.h"
#include "entity_vertex_transform.h"
#include "spatial_sample.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdio.h>

static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
static float real(GameApplication *a,GameAddress p){uint32_t word=get(a,p);float v;memcpy(&v,&word,4);return v;}
int game_level_scene_invalidate(GameApplication *a){
    put(a,0x5ad7a8,0x80000000);put(a,0x5ad7ac,0x80000000);return !a->memory.fault;
}
int game_level_scene_release(GameApplication *a){
    if(a->selection_texture && !game_model_texture_release(a,a->selection_texture))return 0;
    a->selection_texture=0;return game_level_scene_invalidate(a);
}
/*43ae90/43b070: retain the bounded original publications. Drawing uses the
 *existing TextureStore instead of turning those words into host pointers. */
int game_level_scene_prepare(GameApplication *a){
    if(!game_level_scene_release(a))return 0;
    put(a,0x5ad7b0,0);put(a,0x5ad7b4,0);put(a,0x5ad7bc,16384);
    put(a,0x5ad7c0,0x05030101);
    put(a,0x5ad388,0);put(a,0x5ad38c,0);put(a,0x5ad394,1024);
    put(a,0x5ad398,0x05030303);put(a,0x5ad39c,0x5acf48);
    return !a->memory.fault;
}
/*43a240: each packet borrows positions and palette entries from their owner.
 *Unused SDK vertex lanes are defined once rather than inherited from a guest
 *stack. The next packet retains the lanes written by the preceding packet. */
int game_level_palette_polygon(GameApplication *a,const float positions[][3],
    const uint8_t *colors,unsigned count,uint32_t flags){
    GameRenderVertex vertices[32]={{0}};float epsilon=real(a,0x572d70);unsigned i;int ok;
    if(!positions || !colors || count<3 || count>32 || a->memory.fault ||
       !game_application_scene_polygon_begin(a,0))return 0;
    for(i=0;i<count;i++){
        GameAddress color=0x5ad3a8+(uint32_t)colors[i]*4;uint32_t depth;
        GameRenderVertex inherited=vertices[i];memcpy(&depth,&positions[i][2],4);
        game_screen_build_colored_polygon_vertex(vertices+i,&inherited,positions[i][0],positions[i][1],epsilon,
            game_memory_read_u8(&a->memory,color+2),game_memory_read_u8(&a->memory,color+1),
            game_memory_read_u8(&a->memory,color),depth);
    }
    if(a->memory.fault || ((flags&0x800) && !game_application_scene_translucency(a,1)))return 0;
    ok=game_application_world_state(a,WORLD_DEPTH_WRITE,(flags&8)!=0) &&
        game_application_surface_polygon(a,vertices,count);
    if((flags&0x800) && !game_application_scene_translucency(a,0))ok=0;
    return ok;
}
int game_level_colored_render(GameApplication *a,GameAddress head){
    GameRenderVertex vertices[32]={{0}};float epsilon=real(a,0x572d70);unsigned visits=0;
    if(a->memory.fault || !game_application_scene_polygon_begin(a,0))return 0;
    while(head){
        unsigned i;uint8_t byte=game_memory_read_u8(&a->memory,head);
        int count=byte<128?byte:(int)byte-256;
        GameAddress positions=get(a,head+0x14);uint32_t flags=get(a,head+0x20);
        if(a->memory.fault || count<3 || count>32 || ++visits>65536)return 0;
        for(i=0;i<(unsigned)count;++i){
            uint16_t index=game_memory_read_u16(&a->memory,head+2+i*2);
            int32_t signed_index=index<32768?index:(int32_t)index-65536;
            int64_t at=(int64_t)positions+(int64_t)signed_index*16;
            uint8_t entry=game_memory_read_u8(&a->memory,head+0x18+i);
            GameAddress color=0x5ad3a8+entry*4;GameRenderVertex inherited=vertices[i];
            float x,y;uint32_t depth;
            if(at<0 || at>UINT32_MAX-12)return 0;
            x=real(a,(GameAddress)at);y=real(a,(GameAddress)at+4);depth=get(a,(GameAddress)at+8);
            game_screen_build_colored_polygon_vertex(vertices+i,&inherited,x,y,epsilon,
                game_memory_read_u8(&a->memory,color+2),game_memory_read_u8(&a->memory,color+1),
                game_memory_read_u8(&a->memory,color),depth);
        }
        if(a->memory.fault)return 0;
        if((flags&0x800) && !game_application_scene_translucency(a,1))return 0;
        if(!game_application_world_state(a,WORLD_DEPTH_WRITE,(flags&8)!=0) ||
           !game_application_surface_polygon(a,vertices,(unsigned)count))return 0;
        if((flags&0x800) && !game_application_scene_translucency(a,0))return 0;
        head=get(a,head+0x24);if(a->memory.fault)return 0;
    }
    return 1;
}
/*43aef0: one128x128 scratch stays in the existing texture/residency owner.
 *The two original keys govern refresh. Cache hits borrow its pixels directly;
 *key changes refresh the real SDK allocation without a serialized mirror. */
int game_level_selection_vertices(GameApplication *a,GameAddress object,const float positions[][2],
    const float coordinates[][2],unsigned count,uint32_t key_a,uint32_t key_b){
    GameRenderVertex vertices[32]={{0}};TextureRecord *texture;unsigned i;
    GameAddress source=get(a,object+4);int resident,changed;
    uint32_t width=game_memory_read_u16(&a->memory,object),height=game_memory_read_u16(&a->memory,object+2),dimensions;
    size_t bytes;const uint8_t *pixels;
    float epsilon=real(a,0x572d70),scale=real(a,0x572d78);
    if(a->memory.fault || !positions || !coordinates || count<3 || count>32 ||
       !width || !height || (uint64_t)width*height>SIZE_MAX)return 0;
    bytes=(size_t)width*height;dimensions=width|(height<<16);
    if(!a->selection_texture){
        AssetTexture image={0};MaterialValue material={0};
        image.width=image.height=image.render_width=image.render_height=128;
        image.lod=1;image.aspect=3;image.format=5;image.size=16384;image.pixels=calloc(1,16384);
        if(!image.pixels || !game_model_texture_take(a,&image,0,&material)){
            asset_texture_release(&image);return 0;
        }
        a->selection_texture=material.backend_handle;
    }
    texture=game_model_texture_record(a,a->selection_texture);if(!texture)return 0;
    resident=(texture->flags&0x80000000)!=0;
    changed=!resident || key_a!=get(a,0x5ad7a8) || key_b!=get(a,0x5ad7ac) ||
        !texture->cache_valid || texture->cache_a!=source || texture->cache_b!=dimensions;
    if(changed){
        /*The radar scratch can be56x56 after a layout/control transition.
         *Borrow its actual extent rather than reading a128x128 allocation.
         *The original128 upload remains an exact copy; smaller images use
         *nearest scaling in the existing texture owner. Small-mode visual
         *parity is separate from the original128-byte-row upload contract. */
        pixels=scene_projection_storage_borrow_bytes(source,bytes);
        if(!pixels)pixels=game_radar_map_borrow(source,bytes);
        if(!pixels && game_memory_contains(&a->memory,source,bytes))pixels=a->memory.data+(source-a->memory.base);
        if(!pixels || !texture_store_resize_indexed(texture,pixels,bytes,width,height))return 0;
    }
    if(!resident){if(!game_level_texture_use(a,texture->id))return 0;}
    else if(changed && !game_application_texture_update(a,texture))return 0;
    texture->cache_a=source;texture->cache_b=dimensions;texture->cache_valid=1;
    put(a,0x5ad7a8,key_a);put(a,0x5ad7ac,key_b);
    if(a->memory.fault || !game_application_scene_polygon_begin(a,1) ||
       !game_application_surface_texture(a,texture))return 0;
    for(i=0;i<count;++i){
        GameRenderVertex inherited=vertices[i];
        game_screen_build_textured_polygon_vertex(vertices+i,&inherited,positions[i][0],positions[i][1],epsilon,
            coordinates[i][0],coordinates[i][1],scale);
    }
    return !a->memory.fault && game_application_surface_polygon(a,vertices,count);
}
int game_level_selection_render(GameApplication *a,GameAddress polygon,uint32_t key_a,uint32_t key_b){
    float positions[32][2],coordinates[32][2];unsigned i;uint32_t count=get(a,polygon);
    GameAddress points=get(a,polygon+4),uv=get(a,polygon+16),object=get(a,polygon+20);
    if(a->memory.fault || count<3 || count>32)return 0;
    for(i=0;i<count;i++){
        GameAddress point=get(a,points+i*4);
        positions[i][0]=real(a,point);positions[i][1]=real(a,point+4);
        coordinates[i][0]=real(a,uv+i*8);coordinates[i][1]=real(a,uv+i*8+4);
    }
    return !a->memory.fault && game_level_selection_vertices(a,object,(const float (*)[2])positions,
        (const float (*)[2])coordinates,count,key_a,key_b);
}

/*Remaining actual camera/class/overlay/terrain algorithms are mandatory native
 *interfaces. None may enter CpuContext or turn a missing route into success. */
extern int game_level_camera_present(GameApplication *,GameAddress,GameAddress);
extern int game_level_camera_subject(GameApplication *,GameAddress *); /*4a4aa0*/
extern int game_level_scene_overlay(GameApplication *,GameAddress); /*4b2f00*/
extern int game_level_scene_overlay_flush(GameApplication *); /*4b2fe0*/
extern int game_level_terrain_present(GameApplication *,GameAddress,GameAddress); /*49c670*/
/*4e71b0: the actual shared grouped-vehicle presentation consumer. Geometry
 *and submission use the existing model/vertex owners; no guest callback. */
int game_level_group_vehicle_present(GameApplication *a,GameAddress object){
    static const GameAddress controllers[3]={0x4e68f0,0x408100,0x5541d0},vertices[3]={0x4e6ee0,0x4086d0,0x4086d0};
    GameAddress table;uint32_t tick,last,word;int32_t now,expiry,elapsed;float recoil=0;unsigned enabled,role;uint8_t kind;
    if(!a || a->memory.fault)return 0;
    kind=game_memory_read_u8(&a->memory,object+12);if(!vehicle_presentation_family(kind))return 0;
    table=get(a,object);tick=get(a,0x5acbe8);last=get(a,object+0x1e4);
    memcpy(&now,&tick,4);word=last+15;memcpy(&expiry,&word,4);
    if(expiry>=now){word=tick-last;memcpy(&elapsed,&word,4);
        recoil=(float)((long double)elapsed*real(a,kind==0x23?0x577928:0x574ccc)+real(a,kind==0x23?0x57792c:0x574cd0));}
    memcpy(&word,&recoil,4);put(a,object+0x1e8,word);
    /*553ac0: input has already prepared the currently controlled payload. */
    if(kind==0x23 && get(a,get(a,0x5f0208)+0x448)==object)
        return !a->memory.fault && game_model_graph_update(a,get(a,object+0x54));
    enabled=get(a,0x5eff04)!=0;
    if(enabled && get(a,table+0x8c)!=0x4e7280)return 0;
    if(!vehicle_presentation_local_pose(&a->memory,object,enabled))return 0;
    role=(kind==0x24 || kind==0x25 || kind==0x28)?1u:(unsigned)(kind-0x21);
    if(get(a,table+0x90)!=controllers[role] || get(a,table+0x94)!=vertices[role])return 0;
    return vehicle_presentation_group_geometry(&a->memory,object,1) &&
        vehicle_presentation_group_geometry(&a->memory,object,3) && game_model_graph_update(a,get(a,object+0x54));
}
/*536960/42fc00: the existing native pose receives extrapolated local values;
 *controller/vertex builders and model submission execute directly.430c90 adds
 *its display-tick spin only to the controller's input, not the stored angle. */
int game_level_projectile_present(GameApplication *a,GameAddress object){
    PresentationPose *pose=presentation_instance_pose(object);GameAddress table=get(a,object),controller=get(a,table+0x90);
    uint32_t tick=get(a,0x5acbe8),word;int32_t base,delta;unsigned i;uint16_t roll;int ok;
    if(!pose || a->memory.fault || (controller!=0x5353a0 && controller!=0x430c90) || get(a,table+0x94)!=0x5355d0)return 0;
    memcpy(pose->local_position,pose->position,sizeof(pose->position));
    for(i=0;i<3;i++)pose->local_angles[2-i]=pose->angles[i];
    if(get(a,0x5eff04)){
        long double anchor;word=get(a,0x5acc80);memcpy(&base,&word,4);
        anchor=(long double)base+real(a,0x5ac928);
        word=(uint32_t)(uint64_t)(!isfinite(anchor) || anchor< -0x1p63L || anchor>=0x1p63L?INT64_MIN:(int64_t)anchor);
        word=tick-word;memcpy(&delta,&word,4);
        for(i=0;i<3;i++){
            uint16_t velocity=game_memory_read_u16(&a->memory,object+(i==0?0xcc:i==1?0xd0:0xce));
            pose->local_position[i]=(float)((long double)pose->local_position[i]+(long double)delta*real(a,object+0xb4+i*4));
            pose->local_angles[2-i]=(uint16_t)((uint32_t)pose->angles[i]+(uint32_t)velocity*(uint16_t)word);
        }
    }
    roll=pose->local_angles[0];
    if(controller==0x430c90)pose->local_angles[0]=(uint16_t)(roll+(uint16_t)(tick<<7));
    ok=presentation_pose_build_controller(&a->memory,pose);
    pose->local_angles[0]=roll;
    return ok && presentation_pose_transform_vertices(&a->memory,pose,1) && game_model_graph_update(a,get(a,object+0x54));
}
extern int game_level_ground_shadow(GameApplication *,const float[3],float);
/*535f70: ordinary shared presentation for classes using the base controller
 *and vertices. Clocked interpolation uses the existing authoritative pose. */
int game_level_simple_scene_present(GameApplication *a,GameAddress object){
    PresentationPose *pose=presentation_instance_pose(object);GameAddress table=get(a,object);unsigned i;
    int sprite=game_memory_read_u8(&a->memory,object+12)==0x40;
    if(!pose || a->memory.fault || get(a,table+0x90)!=(sprite?0x40ee10:0x5353a0) || get(a,table+0x94)!=(sprite?0x40eee0:0x5355d0))return 0;
    if(get(a,0x5eff04)){
        if(get(a,table+0x8c)!=0x535fd0 || !presentation_pose_interpolate(&a->memory,pose))return 0;
    }else{
        memcpy(pose->local_position,pose->position,sizeof(pose->position));
        for(i=0;i<3;i++)pose->local_angles[2-i]=pose->angles[i];
    }
    if(sprite)return game_level_class40_geometry(a,object,1) && game_level_class40_geometry(a,object,3) && game_model_graph_update(a,pose->model);
    return presentation_pose_build_controller(&a->memory,pose) && presentation_pose_transform_vertices(&a->memory,pose,1) &&
        game_model_graph_update(a,get(a,object+0x54));
}
/*432fe0: after ordinary presentation, size the terrain shadow using the
 *authoritative height. Preserve the original stored-float saturation test. */
static int projectile_ground_present(GameApplication *a,GameAddress object){
    PresentationPose *pose=presentation_instance_pose(object);long double fraction;float stored,diameter;uint32_t bits;int32_t signed_bits;
    if(!pose || !game_level_projectile_present(a,object))return 0;
    fraction=((long double)pose->position[1]-gameplay_geometry_terrain_height(&a->memory,pose->position[0],pose->position[2]))*real(a,0x572394)+real(a,0x5722fc);
    stored=(float)fraction;
    if(isnan(fraction) || fraction<real(a,0x5722f8))stored=0;
    else{memcpy(&bits,&stored,4);memcpy(&signed_bits,&bits,4);if(signed_bits>0x3f800000)stored=1;}
    diameter=(float)((long double)real(a,object+0x38)*stored*real(a,0x572398));
    return game_level_ground_shadow(a,pose->position,diameter);
}
/*4fc1a0/4fcc10: join adjacent rendered hull points, or the shooter's two
 *attachment transforms. The ray keeps the same borrowed neighbors and model. */
static int scaled_present(GameApplication *a,GameAddress o){
    PresentationPose *pose=presentation_instance_pose(o);GameAddress previous,next,points;float from[3],to[3],own[2][3];unsigned i;
    uint32_t word;int32_t age,duration;float elapsed,phase;
    if(!pose || get(a,o)!=0x575138 || a->memory.fault)return 0;
    if(get(a,0x5eff04)){
        if(!presentation_pose_interpolate(&a->memory,pose))return 0;
    }else{memcpy(pose->local_position,pose->position,12);for(i=0;i<3;i++)pose->local_angles[2-i]=pose->angles[i];}
    if(!presentation_pose_build_controller(&a->memory,pose) || !presentation_pose_transform_vertices(&a->memory,pose,1))return 0;
    points=get(a,o+0x40);
    for(i=0;i<3;i++){own[0][i]=real(a,points+i*4);own[1][i]=real(a,points+12+i*4);from[i]=own[0][i];to[i]=own[1][i];}
    previous=get(a,o+0xe4);
    if(previous){
        if(game_memory_read_u8(&a->memory,previous+12)==0x0f){
            GameAddress p=get(a,previous+0x40)+12;
            for(i=0;i<3;i++)from[i]=(float)(((long double)real(a,p+i*4)+own[0][i])*real(a,0x5750d8));
        }else{
            float local[3],attached[3],matrix[12];
            long double y=(long double)real(a,previous+0x30)*real(a,0x575110);
            long double z=(long double)real(a,previous+0x34)*real(a,0x5750d8);
            for(i=0;i<3;i++)local[i]=(float)((z*real(a,previous+0x238+i*16)+y*real(a,previous+0x234+i*16))+real(a,previous+0x23c+i*16));
            for(i=0;i<12;i++)matrix[i]=real(a,previous+0x278+i*4);
            game_matrix_transform_point_values(attached,matrix,local);
            for(i=0;i<12;i++)matrix[i]=real(a,previous+0x80+i*4);
            game_matrix_transform_point_values(from,matrix,attached);
        }
    }
    next=get(a,o+0xe8);
    if(next && get(a,next+0x58)){
        GameAddress p;
        if(game_memory_read_u8(&a->memory,next+12)==0x0f)p=get(a,next+0x40);
        else p=0;
        if(p)for(i=0;i<3;i++)to[i]=real(a,p+i*4);
        else{float origin[3];GameAddress shooter=get(a,o+0xd4);
            for(i=0;i<3;i++)origin[i]=real(a,shooter+24+i*4);
            if(!presentation_target_position(&a->memory,next,origin,to))return 0;}
        for(i=0;i<3;i++)to[i]=(float)(((long double)to[i]+own[1][i])*real(a,0x5750d8));
    }
    word=get(a,0x5ac6ac)-get(a,o+0xdc);memcpy(&age,&word,4);elapsed=(float)age;
    word=get(a,get(a,o+0xe0)+0x24);memcpy(&duration,&word,4);phase=(float)((long double)elapsed/duration);
    return !a->memory.fault && scaled_effect_model_present(&a->memory,get(a,o+0x54),from,to,phase);
}
/*50caf0/50d8f0: the two hull points are the inner Bezier controls. Borrowed
 *neighbors supply the endpoints; the three ring offsets spin around yaw. */
static int curve_present(GameApplication *a,GameAddress o){
    PresentationPose *pose=presentation_instance_pose(o);
    GameAddress points,previous,next;float controls[4][3],offsets[3][3],yaw[12]={0},spin[12]={0},matrix[12];
    float vectors[3][3]={{0,-1,0},{0,0.5f,0},{0,0.5f,0}},side;
    uint32_t bits=0x3f5db22d,angle,tick;unsigned i,j;
    if(!pose || get(a,o)!=0x5752e0 || a->memory.fault)return 0;
    if(get(a,0x5eff04)){if(!presentation_pose_interpolate(&a->memory,pose))return 0;}
    else{memcpy(pose->local_position,pose->position,12);for(i=0;i<3;i++)pose->local_angles[2-i]=pose->angles[i];}
    if(!presentation_pose_build_controller(&a->memory,pose) || !presentation_pose_transform_vertices(&a->memory,pose,1))return 0;
    points=get(a,o+0x40);
    for(i=0;i<3;i++){
        controls[0][i]=controls[1][i]=real(a,points+i*4);
        controls[3][i]=controls[2][i]=real(a,points+12+i*4);
    }
    previous=get(a,o+0xe4);
    if(previous){
        if(game_memory_read_u8(&a->memory,previous+12)==3){
            GameAddress p=get(a,previous+0x40)+12;
            for(i=0;i<3;i++)controls[0][i]=(float)(((long double)real(a,p+i*4)+controls[1][i])*real(a,0x5752a0));
        }else{
            float local[3],attached[3];
            long double y=(long double)real(a,previous+0x30)*real(a,0x5752a4);
            long double z=(long double)real(a,previous+0x34)*real(a,0x5752a0);
            for(i=0;i<3;i++)local[i]=(float)((z*real(a,previous+0x238+i*16)+y*real(a,previous+0x234+i*16))+real(a,previous+0x23c+i*16));
            for(i=0;i<12;i++)matrix[i]=real(a,previous+0x278+i*4);
            game_matrix_transform_point_values(attached,matrix,local);
            for(i=0;i<12;i++)matrix[i]=real(a,previous+0x80+i*4);
            game_matrix_transform_point_values(controls[0],matrix,attached);
        }
    }
    next=get(a,o+0xe8);
    if(next && get(a,next+0x58)){
        GameAddress p;
        if(game_memory_read_u8(&a->memory,next+12)==3)p=get(a,next+0x40);
        else p=0;
        if(p)for(i=0;i<3;i++)controls[3][i]=real(a,p+i*4);
        else{float origin[3];GameAddress shooter=get(a,o+0xd4);
            for(i=0;i<3;i++)origin[i]=real(a,shooter+24+i*4);
            if(!presentation_target_position(&a->memory,next,origin,controls[3]))return 0;}
        for(i=0;i<3;i++)controls[3][i]=(float)(((long double)controls[3][i]+controls[2][i])*real(a,0x5752a0));
    }
    angle=pose->angles[0]&2047u;tick=(get(a,0x5ac6ac)<<6)&2047u;
    yaw[0]=yaw[10]=real(a,0x6298a8+((angle+512)&2047u)*4);
    yaw[2]=real(a,0x6298a8+angle*4);yaw[8]=-yaw[2];yaw[5]=1;
    spin[0]=spin[5]=real(a,0x6298a8+((tick+512)&2047u)*4);
    spin[4]=real(a,0x6298a8+tick*4);spin[1]=-spin[4];spin[10]=1;
    game_matrix_compose_values(matrix,yaw,spin);
    memcpy(&side,&bits,4);vectors[1][0]=-side;vectors[2][0]=side;
    for(j=0;j<3;j++)game_matrix_transform_point_values(offsets[j],matrix,vectors[j]);
    return !a->memory.fault && curve_effect_model_present(&a->memory,get(a,o+0x54),controls,offsets);
}
/*4fa870: descriptor-driven ground objects borrow their existing model. Clock
 *presentation changes its transform only for submission, never simulation pose.
 *A followed payload supplies position; otherwise descriptor velocity/spin predict
 *between simulation ticks. Restore the same owner even when submission fails. */
static int descriptor_present(GameApplication *a,GameAddress o){
    GameAddress descriptor=get(a,o+0xa0),model=get(a,o+0x54),following;
    SceneInstance *instance;float saved[12],matrix[12],position[3];
    uint16_t angles[3];uint32_t elapsed_word;int32_t elapsed;unsigned i;int ok;
    if(a->memory.fault)return 0;
    if(!get(a,0x5eff04) || !descriptor || get(a,descriptor+0x10))
        return !a->memory.fault && game_model_graph_update(a,model);
    instance=scene_instance_owner_find(model);if(!instance)return 0;
    memcpy(saved,instance->local,sizeof(saved));following=get(a,descriptor+0x3c);
    for(i=0;i<3;i++)angles[i]=game_memory_read_u16(&a->memory,o+0x24+i*2);
    if(following){
        for(i=0;i<3;i++)position[i]=real(a,following+0x18+i*4);
        position[1]=(float)((long double)position[1]-real(a,descriptor+0x44));
    }else{
        elapsed_word=get(a,0x5acbe8)-get(a,0x5acc80);memcpy(&elapsed,&elapsed_word,4);
        position[0]=(float)((long double)real(a,descriptor+0x20)*elapsed+real(a,o+0x18));
        position[1]=(float)((long double)real(a,descriptor+0x24)*elapsed+real(a,o+0x1c));
        position[2]=(float)((long double)real(a,descriptor+0x28)*elapsed+real(a,o+0x20));
        for(i=0;i<3;i++)angles[i]=(uint16_t)(angles[i]+
            (uint32_t)game_memory_read_u16(&a->memory,descriptor+0x2c+i*2)*elapsed_word);
    }
    gameplay_geometry_orientation_values(&a->memory,angles,position,matrix);
    if(following){
        /*This inline original variant stores sc*sb before its first row. */
        float sc=real(a,0x6298a8+(angles[2]&2047u)*4),sb=real(a,0x6298a8+(angles[1]&2047u)*4);
        float sc_sb=(float)((long double)sc*sb);
        matrix[0]=(float)((long double)real(a,0x6298a8+((angles[2]+512u)&2047u)*4)*
            real(a,0x6298a8+((angles[0]+512u)&2047u)*4)+(long double)sc_sb*
            real(a,0x6298a8+(angles[0]&2047u)*4));
    }
    if(a->memory.fault)return 0;
    memcpy(instance->local,matrix,sizeof(matrix));
    ok=scene_projection_storage_project_holder(instance) && game_model_graph_update(a,model);
    memcpy(instance->local,saved,sizeof(saved));
    return scene_projection_storage_project_holder(instance) && ok;
}
/*43d560/5560e0: rendered extrapolation borrows the same owned pose/model.
 *It never changes authoritative motion or adds a synchronized copy. */
static int moving_effect_present(GameApplication *a,GameAddress o,int fragment){
    PresentationPose *pose=presentation_instance_pose(o);uint32_t word;int32_t elapsed;
    unsigned i;uint8_t stopped;uint16_t motion;int ok;
    if(!pose || a->memory.fault)return 0;
    memcpy(pose->local_position,pose->position,12);
    for(i=0;i<3;i++)pose->local_angles[2-i]=pose->angles[i];
    stopped=fragment && (game_memory_read_u8(&a->memory,o+0xe0)&2u);
    if(stopped)pose->local_position[1]=(float)(gameplay_geometry_terrain_height(&a->memory,
        pose->local_position[0],pose->local_position[2])+(long double)pose->dimensions[1]);
    else if(get(a,0x5eff04) && (fragment || (game_memory_read_u16(&a->memory,o+0xe4)&1u))){
        word=get(a,0x5acbe8)-get(a,0x5acc80);memcpy(&elapsed,&word,4);
        for(i=0;i<3;i++){
            pose->local_position[i]=(float)((long double)real(a,o+0xb4+i*4)*elapsed+pose->local_position[i]);
            motion=game_memory_read_u16(&a->memory,o+(i==0?0xcc:i==1?0xd0:0xce));
            pose->local_angles[2-i]=(uint16_t)(pose->local_angles[2-i]+(uint32_t)motion*word);
        }
    }
    if(fragment)ok=fragment_build_model(&a->memory,o);
    else ok=attachment_effect_controller(&a->memory,o) && presentation_pose_transform_vertices(&a->memory,pose,1);
    return ok && game_model_graph_update(a,pose->model);
}
/*538210 is position-only interpolation. Its divisor is stored binary32;
 *the ratio remains extended until each final coordinate store. */
static int particle_present(GameApplication *a,GameAddress o){
    PresentationPose *pose=presentation_instance_pose(o);uint32_t now,word;int32_t elapsed,remaining;
    long double duration,factor;float limit,stored;unsigned i;
    if(!pose || a->memory.fault)return 0;
    if(get(a,0x5eff04)){
        now=get(a,0x5acbe8);word=now-pose->local_phase;memcpy(&elapsed,&word,4);pose->local_phase=now;
        word=get(a,0x5acc80)-now;memcpy(&remaining,&word,4);
        duration=(long double)remaining+real(a,0x5acc38);limit=real(a,0x576680);
        if(elapsed>10 || !(duration>limit))memcpy(pose->local_position,pose->position,12);
        else{stored=(float)duration;factor=(long double)limit/stored;
            for(i=0;i<3;i++)pose->local_position[i]=(float)(((long double)pose->position[i]-pose->local_position[i])*factor+pose->local_position[i]);}
    }else memcpy(pose->local_position,pose->position,12);
    return presentation_pose_build_billboard_controller(&a->memory,pose) && game_model_graph_update(a,pose->model);
}
/*550a80: attachment updates own the pose. Rendering caches a stationary
 *billboard and emits the actual temporary surface for large explosions. */
static int animation_present(GameApplication *a,GameAddress o){
    PresentationPose *pose=presentation_instance_pose(o);EffectAnimation *animation=presentation_instance_animation(o);
    EffectModel *model;uint32_t word;int32_t elapsed,duration,radius_bits;unsigned i;int accepted;
    long double factor,one;float stored,width,height;
    if(!pose || !animation || !(model=effect_model_find(pose->model)) || a->memory.fault)return 0;
    if(!(pose->flags&0x40000u)){
        memcpy(pose->local_position,pose->position,12);
        for(i=0;i<3;i++)pose->local_angles[2-i]=pose->angles[i];
        if(!presentation_pose_build_billboard_controller(&a->memory,pose))return 0;
        if(!animation->attachment)pose->flags|=0x40000u;
    }
    if(!game_model_graph_update(a,pose->model))return 0;
    word=get(a,0x5ac6ac)-animation->birth_tick;memcpy(&elapsed,&word,4);
    word=get(a,model->profile+8);memcpy(&duration,&word,4);one=real(a,0x57768c);
    factor=one-(long double)elapsed/duration;stored=factor>one?1.0f:(float)factor;
    width=(float)(((long double)pose->radius*stored)*real(a,0x577690));
    height=(float)((long double)stored*real(a,0x577694));
    memcpy(&radius_bits,&pose->radius,4);
    if(radius_bits>0x42c80000)return descent_surface_submit(&a->memory,pose->local_position,width,height,&accepted);
    return !a->memory.fault;
}
/*4d9990: six orientation terms are stored before radius multiplication;
 *the last row retains extended arithmetic and the stored sc*sa term. */
static int trail_present(GameApplication *a,GameAddress o){
    TrailModel *model=trail_model_find(get(a,o+0x54));float m[12],ca,cb,cc,sa,sb,sc,sc_sa,radius;
    uint16_t angles[3];unsigned i;
    if(!model || a->memory.fault)return 0;
    for(i=0;i<3;i++)angles[i]=game_memory_read_u16(&a->memory,o+0x24+i*2)&2047u;
    ca=real(a,0x6298a8+((angles[0]+512u)&2047u)*4);sa=real(a,0x6298a8+angles[0]*4);
    cb=real(a,0x6298a8+((angles[1]+512u)&2047u)*4);sb=real(a,0x6298a8+angles[1]*4);
    cc=real(a,0x6298a8+((angles[2]+512u)&2047u)*4);sc=real(a,0x6298a8+angles[2]*4);radius=real(a,o+0x38);
    sc_sa=(float)((long double)sc*sa);
    m[0]=(float)((long double)cc*ca+((long double)sc*sa)*sb);
    m[1]=(float)(((long double)sb*sa)*cc-(long double)sc*ca);m[2]=(float)((long double)sa*cb);
    m[4]=(float)((long double)sc*cb);m[5]=(float)((long double)cc*cb);m[6]=-sb;
    for(i=0;i<8;i++)if((i&3u)!=3u)m[i]=(float)((long double)m[i]*radius);
    m[8]=(float)((((long double)sc*sb)*ca-(long double)sa*cc)*radius);
    m[9]=(float)((((long double)cc*sb)*ca+sc_sa)*radius);m[10]=(float)(((long double)cb*ca)*radius);
    for(i=0;i<3;i++)m[i*4+3]=real(a,o+24+i*4);
    if(a->memory.fault)return 0;memcpy(model->local,m,sizeof(m));return trail_model_present(&a->memory,model);
}
/*4f61b0: display interpolation uses the player's existing live pose and rig.
 *Yaw/turret wrap; pitch, roll and aim approach signed values before wrapping. */
static uint16_t player_display_angle(uint16_t current,uint16_t target,float factor,int circular){
    int32_t delta;long double step;uint32_t integer;uint16_t result;
    if(circular){delta=(target-current)&2047u;if(delta>1024)delta-=2048;}
    else delta=(int32_t)(int16_t)target-(int32_t)(int16_t)current;
    step=(long double)delta*factor;
    integer=(uint32_t)(uint64_t)(!isfinite(step) || step< -0x1p63L || step>=0x1p63L?INT64_MIN:(int64_t)step);
    result=(uint16_t)(current+integer)&2047u;
    if(!circular && result>1024)result=(uint16_t)(result-2048u);
    return result;
}
static int player_present(GameApplication *a,GameAddress o){
    PresentationPose *pose=presentation_instance_pose(o);PlayerRig *rig=presentation_instance_player_rig(o);
    PlayerControls *controls=presentation_instance_player_controls(o);uint32_t tick,last,word;int32_t now,expiry,elapsed;
    long double duration;float stored,factor;unsigned i;
    if(!pose || !rig || !controls)return 0;
    tick=get(a,0x5acbe8);last=get(a,o+0x2c4);memcpy(&now,&tick,4);
    word=last+15;memcpy(&expiry,&word,4);rig->aim_offset=0;
    if(expiry>=now){word=tick-last;memcpy(&elapsed,&word,4);
        rig->aim_offset=(float)((long double)elapsed*real(a,0x574e3c)+real(a,0x574d68));}
    if(get(a,get(a,0x5f0208)+0x3bc)!=o){
        word=get(a,0x5acc80)-tick;memcpy(&elapsed,&word,4);
        duration=(long double)elapsed+real(a,0x5acc38);stored=(float)duration;
        if(!(duration>real(a,0x574d3c))){
            memcpy(pose->local_position,pose->position,12);
            for(i=0;i<3;i++)pose->local_angles[2-i]=pose->angles[i];
            controls->previous_heading=controls->heading;controls->previous_aim=controls->aim;
        }else{
            factor=(float)((long double)real(a,0x574d3c)/stored);
            for(i=0;i<3;i++){
                pose->local_position[i]=(float)(((long double)pose->position[i]-pose->local_position[i])*factor+pose->local_position[i]);
                pose->local_angles[2-i]=player_display_angle(pose->local_angles[2-i],pose->angles[i],factor,i==0);
            }
            controls->previous_heading=player_display_angle(controls->previous_heading,controls->heading,factor,1);
            controls->previous_aim=player_display_angle(controls->previous_aim,controls->aim,factor,0);
        }
        if(a->memory.fault || !player_rig_build(&a->memory,pose,controls,rig,1) ||
           !player_transform_vertex_ranges(&a->memory,pose,rig,1))return 0;
    }
    return !a->memory.fault && game_model_graph_update(a,pose->model);
}
/*438210/438280: articulation shares the existing bounded pose and hinge
 *record. All four differences take the shortest2048-angle arc; yaw stores
 *unsigned, while pitch/roll/hinge normalize to signed angles. */
static int articulated_present(GameApplication *a,GameAddress o){
    static const unsigned current[4]={0x24,0x26,0x28,0x2ea},local[4]={0x78,0x76,0x74,0x2ec};
    unsigned i;
    if(get(a,o)!=0x572c78 || a->memory.fault)return 0;
    if(get(a,0x5eff04)){
        if(!presentation_instance_articulated_interpolate(&a->memory,o))return 0;
    }else{
        for(i=0;i<3;i++)put(a,o+0x64+i*4,get(a,o+0x18+i*4));
        for(i=0;i<4;i++)game_memory_write_u16(&a->memory,o+local[i],game_memory_read_u16(&a->memory,o+current[i]));
    }
    return !a->memory.fault && presentation_instance_articulated_render(&a->memory,o) &&
        presentation_instance_articulated_render_vertices(&a->memory,o) && game_model_graph_update(a,get(a,o+0x54));
}
static int multipart_submit(void *context,GameAddress model){return game_model_graph_update(context,model);}
static int present_scene(GameApplication *a,GameAddress object){
    GameAddress method=get(a,get(a,object)+0x40);
    if(method==0x45e8e0)return game_level_gate_present(a,object);
    if(method==0x460020 || method==0x42f160)return !a->memory.fault; /*Actual destination/base RET; particles supply the destination effect. */
    if(method==0x4f61b0)return player_present(a,object);
    if(method==0x438210)return articulated_present(a,object);
    if(method==0x50fe90)return spatial_sample_present(&a->memory,object,a,multipart_submit);
    if(method==0x4fcc10)return scaled_present(a,object);
    if(method==0x43d560 || method==0x5560e0)return moving_effect_present(a,object,method==0x43d560);
    if(method==0x5381d0)return particle_present(a,object);
    if(method==0x550a80)return animation_present(a,object);
    if(method==0x4d9990)return trail_present(a,object);
    if(method==0x4fa870)return descriptor_present(a,object);
    if(method==0x50d8f0)return curve_present(a,object);
    if(method==0x4a6600)return !a->memory.fault && game_model_graph_update(a,get(a,object+0x54));
    if(method==0x536960)return game_level_projectile_present(a,object);
    if(method==0x548740)return game_level_simple_scene_present(a,object);
    if(method==0x48e9a0)return game_level_mole_present(a,object);
    if(method==0x41a1f0)return game_level_spawn_present(a,object);
    if(method==0x4e71b0 || method==0x553ac0)return game_level_group_vehicle_present(a,object);
    if(method==0x536960 || method==0x42fc00)return game_level_projectile_present(a,object);
    if(method==0x432fe0)return projectile_ground_present(a,object);
    if(method==0x535f70)return game_level_simple_scene_present(a,object);
    fprintf(stderr,"Unreconstructed scene presentation %08x object=%08x\n",(unsigned)method,(unsigned)object);
    a->memory.fault=1;a->memory.fault_address=method;return 0;
}
static int camera_excludes(GameApplication *a,GameAddress object,int alternate,int *skip){
    GameAddress camera=get(a,0x58f890),subject;uint8_t kind=game_memory_read_u8(&a->memory,object+12);
    *skip=0;
    if(alternate && kind!=0x41)return !a->memory.fault;
    subject=alternate?get(a,object+0xc4):object;
    if(camera==get(a,0x58f89c) && subject==get(a,get(a,0x5f0208)+0x3bc))*skip=1;
    if(!*skip && camera==get(a,0x58f8a0) && subject==get(a,camera+0x28))*skip=1;
    if(!alternate && !*skip && camera==get(a,0x58f8ac)){
        if(!game_level_camera_subject(a,&subject))return 0;
        *skip=subject==object;
    }
    return !a->memory.fault;
}
/*418880: the actual world pass, including both spatial planes and camera-owned
 *scene exclusions. The authoritative grid supplies borrowed object IDs. */
int game_level_world_present(GameApplication *a){
    const char *stage="spatial grid";
    static const GameAddress projected_sources[4]={0x597010,0x597014,0x597018,0x59701c};
    static const GameAddress projected_targets[4]={0x624f30,0x624f44,0x624f00,0x624f40};
    GameplaySpatialGrid *grid=gameplay_spatial_grid(0x5f0800);GameAddress camera,object;
    unsigned i,plane,visits=0;int32_t count;int skip;
    if(!grid || a->memory.fault)goto failed;
    put(a,0x5acbe0,0);game_initializer_004cc490(&a->memory);
    (void)game_initializer_004ac1a0(&a->memory);
    for(i=0;i<4;i++)put(a,projected_targets[i],get(a,projected_sources[i]));
    put(a,0x624f18,0);put(a,0x624efc,0);put(a,0x624f2c,0);
    game_leaf_004d94d0(&a->memory);
    camera=get(a,0x58f890);
    stage="camera";
    if(a->memory.fault || !game_level_camera_present(a,camera,get(a,get(a,camera))))goto failed;
    stage="radar reveal";
    if(!game_radar_reveal(&a->memory,get(a,get(a,0x5f0208)+0x3bc)+24,15000.0f))goto failed;
    stage="spatial scene collection";
    put(a,0x6038e4,0);put(a,0x603880,0xc8000000);
    for(i=0;(count=(int32_t)get(a,0x6020d0))>0 && i<(uint32_t)count;i++){
        int row=(int16_t)game_memory_read_u16(&a->memory,0x5f06b8+i*4);
        int column=(int16_t)game_memory_read_u16(&a->memory,0x5f06ba+i*4);
        if(i>=1024 || row<0 || row>=32 || column<0 || column>=32)return 0;
        for(plane=0;plane<2;plane++){
            object=grid->heads[plane][row][column];
            while(object && !a->memory.fault){
                if(++visits>65536)return 0;
                if(!camera_excludes(a,object,plane!=0,&skip))return 0;
                if(!skip && (plane || !(get(a,object+16)&0x200000))){
                    if(!present_scene(a,object)){
                        fprintf(stderr,"native scene presentation failed: object=%08x method=%08x\n",object,get(a,get(a,object)+0x40));goto failed;
                    }
                    if(!plane && (get(a,object+16)&0x1c0) && !game_level_scene_overlay(a,object)){
                        fprintf(stderr,"native scene overlay failed: object=%08x type=%u\n",object,game_memory_read_u8(&a->memory,object+12));return 0;
                    }
                }
                object=get(a,object+4);
            }
        }
    }
    stage="scene overlays";if(a->memory.fault || !game_level_scene_overlay_flush(a))goto failed;
    stage="terrain collection";if(!game_level_terrain_present(a,0x629088,0x5cedf0))goto failed;
    if(get(a,0x59626c) || get(a,0x58fdf0)!=get(a,0x58fdf4))
        return gameplay_geometry_classify_visible_queue(&a->memory,get(a,0x6252f8),get(a,0x594454),get(a,0x60426c));
    return !a->memory.fault;
failed:
    game_application_log(a,"native world collection failed: %s\n",stage);return 0;
}
