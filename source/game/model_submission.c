#include "model_assets.h"
#include "scene_submission.h"
#include "mesh_buffer.h"
#include "scene_instance_owner.h"
#include "render_arena.h"
#include "effect_model.h"
#include "gameplay_state.h"
#include "descent_payload.h"
#include <string.h>
#include <stdio.h>
int game_level_ground_shadow(GameApplication *,const float[3],float);
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
static float real(GameApplication *a,GameAddress p){uint32_t bits=get(a,p);float v;memcpy(&v,&bits,4);return v;}
/*40cdc0 selects real animation state, not an emulated virtual call. */
static uint16_t actor_profile(GameApplication *a,GameAddress parent){
    uint32_t state=get(a,parent+0xe0);uint8_t posture;
    if(state==2)return 1;
    if(state==5)return 5;
    if(state!=1)return 0;
    posture=game_memory_read_u8(&a->memory,parent+0x148);
    return posture==2?2:posture==4?4:3;
}
/*409c50/49afd0 use the same native quad projection, clipping and frame queues.
 *413680 is the original no-op; stored lighting remains the shade input. */
static int billboard_present(GameApplication *a,EffectModel *model){
    EffectModelView view;uint16_t direction;uint32_t tick=get(a,0x5ac6ac),age;
    float position[3],bounds[4];GameAddress head,tail;GameplayClipStatus status={0};
    unsigned i;int visible;
    if(!render_arena_current() && !render_arena_initialize())return 0;
    if(model->bank && !model->profile){
        if(!model->parent || game_memory_read_u8(&a->memory,model->parent+12)!=0x40 ||
           !effect_model_select_profile(&a->memory,model,actor_profile(a,model->parent),tick))return 0;
    }
    /*New effects can be published during this frame's death/arrival callbacks.
     *Their first animation sample is age1 -> frame0. Sampling age0 as UINT_MAX
     *can produce a negative frame and read outside the owned texture table.
     *Wait for that first tick; do not clamp or alter the profile's arithmetic. */
    age=tick-model->start_tick;
    if(!age || age>INT32_MAX)return !a->memory.fault;
    if(!effect_model_direction(&a->memory,model,&direction) ||
       !effect_model_sample_frame(&a->memory,model,direction,age-1u,&visible))return 0;
    if(!visible && model->bank){
        /*49afd0 restarts an exhausted non-looping profile even when the
         *selector returns the same profile;49af80 itself preserves its tick. */
        model->profile=0;
        if(!model->parent || game_memory_read_u8(&a->memory,model->parent+12)!=0x40 ||
           !effect_model_select_profile(&a->memory,model,actor_profile(a,model->parent),tick) ||
           !effect_model_sample_frame(&a->memory,model,direction,0,&visible))return 0;
    }
    if(visible || model->bank){
        position[0]=model->local[3];position[1]=model->local[7];position[2]=model->local[11];
        if(model->mode&1u)effect_quad_clear_flags(&model->quad,0x200u);
        else effect_quad_set_flags(&model->quad,0x200u,0,model->lighting,0);
        for(i=0;i<12;i++)view.world_to_view[i]=real(a,0x5cedf0+i*4);
        view.focal=real(a,0x6290f4);view.vertical_scale=real(a,0x6290f0);
        view.center[0]=real(a,0x6290c8);view.center[1]=real(a,0x6290cc);
        view.roll_sine=real(a,0x5ac508);view.roll_cosine=real(a,0x5ac510);
        view.near_depth=real(a,0x571f7c);view.far_bias=real(a,0x571f80);
        view.unit=real(a,0x571f70);view.half=real(a,0x571f74);view.far_distance=(int32_t)get(a,0x597f7c);
        if(a->memory.fault)return 0;
        if(effect_quad_project(&model->quad,position,&view)){
            /*412d15/412d21 republish the model's canonical inputs each draw.
             *Screen clipping may replace these references with frame storage. */
            model->quad.vertex_array=model->projection+0x84u;
            model->quad.uv_ref=model->projection+0x94u;
            bounds[0]=real(a,0x6290d4);bounds[1]=real(a,0x6290d0);bounds[2]=real(a,0x6290dc);bounds[3]=real(a,0x6290d8);
            head=gameplay_clipper_screen_chain(&a->memory,model->projection+0xdc,&tail,bounds,
                0x604378,0x604270,0x604480,0x604264,&status);
            if(a->memory.fault)return 0;
            if(head){effect_quad_clear_flags(&model->quad,0x400u);
                if(!gameplay_state_queue_packet(&a->memory,head))return 0;}
        }
        if(model->bank && !game_level_ground_shadow(a,position,
            (float)((long double)real(a,model->parent+0x38)*real(a,0x57429c))))return 0;
    }
    return !a->memory.fault;
}
int game_level_ground_shadow(GameApplication *a,const float position[3],float diameter){
    SceneSubmissionState s={0};GameplayClipCursors c,source;GameAddress queue;unsigned i;int ok;
    if(!a || a->memory.fault || (!render_arena_current() && !render_arena_initialize()))return 0;
    c.vertex=get(a,0x604378);c.packet=get(a,0x604268);c.uv=get(a,0x604480);c.scalar=get(a,0x604270);
    queue=get(a,0x624efc);s.shadow_queue=&queue;s.frame.cursors=&c;
    source.vertex=get(a,0x624f30);source.packet=get(a,0x624f40);source.uv=get(a,0x624f00);source.scalar=get(a,0x624f44);
    s.shadow_sources=&source;
    s.frame.center_x=real(a,0x6290c8);s.frame.center_y=real(a,0x6290cc);
    s.frame.scale=real(a,0x6290f4);s.frame.vertical=real(a,0x6290f0);s.frame.numerator=real(a,0x574cb0);s.frame.backface_threshold=real(a,0x574cb4);
    s.frame.bounds[0]=real(a,0x6290d4);s.frame.bounds[1]=real(a,0x6290d0);s.frame.bounds[2]=real(a,0x6290dc);s.frame.bounds[3]=real(a,0x6290d8);
    for(i=0;i<12;i++)s.camera[i]=real(a,0x5cedf0+i*4);
    ok=scene_submission_ground_shadow(&a->memory,position,diameter,&s);
    if(!ok)return 0;
    put(a,0x604378,c.vertex);put(a,0x604268,c.packet);put(a,0x604480,c.uv);put(a,0x604270,c.scalar);put(a,0x624efc,queue);
    put(a,0x624f30,source.vertex);put(a,0x624f40,source.packet);put(a,0x624f00,source.uv);put(a,0x624f44,source.scalar);
    return !a->memory.fault;
}
/*450460: remove the CPU frame/graph adapter around the existing owned scene
 * algorithm. The bounded frame bank stays authoritative; local cursor values
 * are ordinary algorithm locals and are published once after the graph walk.
 * Unsupported graph shapes fail rather than re-entering generated execution. */
static int descent_surface(void *c,const float p[3],float width,float height){
    int accepted;return descent_surface_submit(&((GameApplication *)c)->memory,p,width,height,&accepted);
}
int game_model_graph_update(GameApplication *a,GameAddress root) {
    if(root && get(a,root)==0x573a10){
        DescentRenderServices services={a,descent_surface};
        return descent_model_present(&a->memory,root,&services);
    }
    SceneSubmissionState state={0};GameplayClipCursors cursors,source;GameAddress head,visible,shadow;uint32_t mode,second_depth;unsigned i,depth;int result;int64_t far;
    EffectModel *billboard=effect_model_find(root);
    if(billboard)return billboard_present(a,billboard);
    if(!root || (!scene_instance_owner_is_holder(root) && !mesh_buffer_is_native(&a->memory,root)))return 0;
    mode=get(a,0x626e10);state.depth=get(a,0x627074);second_depth=get(a,0x627080);
    if(a->memory.fault || mode>1 || state.depth>=9 || second_depth>=9)return 0;
    if(!render_arena_current() && !render_arena_initialize())return 0;
    state.frame.clip_vertices=0x604378;state.frame.clip_packets=0x604268;state.frame.clip_uv=0x604480;state.frame.clip_indices=0x604270;state.frame.queue_head=0x604484;
    cursors.vertex=get(a,0x604378);cursors.packet=get(a,0x604268);cursors.uv=get(a,0x604480);cursors.scalar=get(a,0x604270);head=get(a,0x604484);
    state.frame.cursors=&cursors;state.frame.queue_head_value=&head;state.palette_memory=&a->memory;
    visible=get(a,0x6252f8);state.visible_queue=&visible;
    shadow=get(a,0x624f18);state.shadow_queue=&shadow;
    source.vertex=get(a,0x624f30);source.packet=get(a,0x624f40);source.uv=get(a,0x624f00);source.scalar=get(a,0x624f44);
    state.shadow_sources=&source;
    state.visible_numerator=real(a,0x5749a8);state.visible_offset=real(a,0x5749ac);
    state.frame.center_x=real(a,0x6290c8);state.frame.center_y=real(a,0x6290cc);
    state.frame.scale=real(a,0x6290f4);state.frame.vertical=real(a,0x6290f0);state.frame.numerator=real(a,0x574cb0);state.frame.backface_threshold=real(a,0x574cb4);
    state.frame.bounds[0]=real(a,0x6290d4);state.frame.bounds[1]=real(a,0x6290d0);state.frame.bounds[2]=real(a,0x6290dc);state.frame.bounds[3]=real(a,0x6290d8);
    for(i=0;i<12;++i)state.camera[i]=real(a,0x5cedf0+i*4);
    far=get(a,0x597f7c);if(far>=0x80000000L)far-=INT64_C(4294967296);
    state.far_depth=(float)((long double)far-real(a,0x57321c));state.presentation_policy=get(a,0x627060);
    if(a->memory.fault)return 0;result=scene_submission_owned(&a->memory,root,&state);
    if(result!=1){fprintf(stderr,"native model submission failed: root=%08x status=%d reject=%u nodes=%u submitted=%u\n",root,result,state.reject,state.nodes,state.submitted);return 0;}
    put(a,0x626e10,0);
    for(depth=0;depth<10;++depth)if(state.dirty[depth])for(i=0;i<12;++i){uint32_t bits;memcpy(&bits,&state.matrices[depth][i],4);put(a,0x626768+depth*48+i*4,bits);}
    put(a,0x627074,state.depth);put(a,0x627064,1);
    put(a,0x604378,cursors.vertex);put(a,0x604268,cursors.packet);put(a,0x604480,cursors.uv);put(a,0x604270,cursors.scalar);put(a,0x604484,head);
    put(a,0x6252f8,visible);
    put(a,0x624f18,shadow);
    put(a,0x624f30,source.vertex);put(a,0x624f40,source.packet);put(a,0x624f00,source.uv);put(a,0x624f44,source.scalar);
    scene_instance_owner_note_walk(state.nodes,state.render_nodes);return !a->memory.fault;
}
