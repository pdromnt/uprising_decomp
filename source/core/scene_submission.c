#include "scene_submission.h"
#include "scene_instance_owner.h"
#include "scene_projection_storage.h"
#include "render_arena.h"
#include "game_matrix.h"
#include "mesh_buffer.h"
#include <stdlib.h>
#include <string.h>

typedef struct SubmissionNode {
    const SceneInstance *instance; /* Borrowed for this synchronous walk. */
    uint32_t child,next;
} SubmissionNode;
typedef struct SubmissionWalk {
    GameMemory *memory;
    SceneSubmissionState *state;
    SubmissionNode *nodes;
    uint32_t count,capacity,leaves;
    int failed;
} SubmissionWalk;

static int capture(SubmissionWalk *w,GameAddress holder,unsigned depth,uint32_t *index){
    SceneInstance *instance=scene_instance_owner_find(holder);SceneDescriptorState descriptor;
    SubmissionNode *node;uint32_t child,previous=UINT32_MAX,i,slot;
    if(!holder || depth>=8u || w->count>=4096u ||
       !instance ||
       ((instance->flags&2u) && game_memory_read_u32(w->memory,0x597008) && !w->state->shadow_queue) || (instance->last_child && !instance->last_child_node)){
        w->state->reject=2u;return 0;
    }
    if(instance->extension && !w->state->visible_queue){w->state->reject=3u;return 0;}
    if(instance->descriptor){
        const SceneRenderDescriptor *render=&instance->render;
        const uint32_t *links;SceneMesh *mesh=instance->mesh;
        /* Descriptor edits invalidate the cached association, not the retained
         * source geometry. Resolve the current source through its existing owner. */
        if(!instance->mesh || !instance->mesh_render_valid)
            mesh=scene_projection_storage_bound_mesh(render->source_vertices,render->vertex_count);
        if(!instance->has_render || !mesh ||
           render->polygons!=instance->presentation_polygon_base ||
           render->uv!=instance->presentation_uv_base ||
           render->polygon_count!=instance->presentation_count ||
           !render->polygon_count || !instance->presentation ||
           mesh->vertex_count!=render->vertex_count ||
           mesh->polygon_count!=render->polygon_count ||
           !scene_projection_storage_descriptor(instance->descriptor,&descriptor) ||
           descriptor.auxiliary[1] || !scene_projection_storage_links_clean(render->links)){
            w->state->reject=4u;return 0;
        }
        links=NULL;
        if(!instance->mesh_render_valid){
            links=scene_projection_storage_links(render->links,(size_t)render->polygon_count*3u);
            if(!links || !scene_projection_storage_transformed(render->transformed_vertices,render->vertex_count)){
                w->state->reject=4u;return 0;
            }
        }
        /* Clipping rewrites these output packets (count and vertex list).
         * Rebuild them from the retained presentation before validating this
         * frame, rather than treating last frame's clipped quad as source. */
        if(!scene_projection_storage_prepare_instance(instance)){w->state->reject=4u;return 0;}
        for(i=0;i<render->polygon_count;i++){
            unsigned j;
            RenderArenaPacket *packet=scene_projection_storage_packet(render->polygons+i*40u);
            if(!packet || packet->count!=3u || packet->vertices!=render->links+i*12u){
                w->state->reject=4u;return 0;
            }
            for(j=0;links && j<3u;j++)if(links[i*3u+j]!=render->transformed_vertices+mesh->polygons[i].vertex[j]*16u){
                w->state->reject=4u;return 0;
            }
        }
        if(!instance->mesh_render_valid){instance->mesh=mesh;instance->mesh_render_valid=1u;}
        ++w->leaves;
    }
    if(w->count==w->capacity){
        uint32_t capacity=w->capacity?w->capacity*2u:32u;
        SubmissionNode *nodes=realloc(w->nodes,(size_t)capacity*sizeof(*nodes));
        if(!nodes){w->failed=1;return 0;}w->nodes=nodes;w->capacity=capacity;
    }
    slot=w->count++;node=&w->nodes[slot];node->instance=instance;
    if(!node->instance){w->state->reject=2u;return 0;}
    node->child=UINT32_MAX;node->next=UINT32_MAX;
    child=instance->last_child_node?instance->last_child_node->holder:0u;
    for(i=0;child;i++){
        const SceneInstance *descendant=scene_instance_owner_find(child);uint32_t child_index;
        if(i>=4096u || !descendant ||
           (descendant->previous_child && !descendant->previous_child_node) ||
           !capture(w,child,depth+1u,&child_index))return 0;
        if(previous==UINT32_MAX)w->nodes[slot].child=child_index;
        else w->nodes[previous].next=child_index;
        previous=child_index;
        child=descendant->previous_child_node?descendant->previous_child_node->holder:0u;
    }
    *index=slot;return 1;
}

static float shadow_real(GameMemory *m,GameAddress p){uint32_t bits=game_memory_read_u32(m,p);float value;memcpy(&value,&bits,4);return value;}
static void shadow_transform(float output[3],const float matrix[12],const float point[3]){
    unsigned row;for(row=0;row<3;row++)output[row]=(float)((((long double)point[0]*matrix[row*4]+
        (long double)point[1]*matrix[row*4+1])+(long double)point[2]*matrix[row*4+2])+matrix[row*4+3]);
}
static int shadow_word(GameMemory *m,GameAddress p,uint32_t word){
    uint8_t bytes[4];unsigned i;for(i=0;i<4;i++)bytes[i]=(uint8_t)(word>>(i*8));
    return render_arena_write(p,bytes,4) || game_memory_write(m,p,bytes,4);
}
static int shadow_vertex(GameMemory *m,GameAddress p,const float point[3]){
    RenderArenaVertex *v=render_arena_vertices(p,1);unsigned i;
    if(v){memcpy(v->position,point,12);v->light=0;return 1;}
    for(i=0;i<3;i++){uint32_t bits;memcpy(&bits,point+i,4);if(!shadow_word(m,p+i*4,bits))return 0;}
    return shadow_word(m,p+12,0);
}
static int shadow_triangle(GameMemory *m,const GameplayClipCursors *c,const GameplayClipPolygonInput *input,uint32_t material){
    RenderArenaPacket *packet=render_arena_packets(c->packet,1);unsigned i,j;
    if(packet){memset(packet,0,sizeof(*packet));packet->count=3;packet->vertices=c->scalar;
        packet->flags=0x811;packet->attributes=c->uv;packet->material=material;}
    else {
        for(i=0;i<10;i++)if(!shadow_word(m,c->packet+i*4,0))return 0;
        if(!shadow_word(m,c->packet,3) || !shadow_word(m,c->packet+4,c->scalar) ||
           !shadow_word(m,c->packet+8,0x811) || !shadow_word(m,c->packet+16,c->uv) ||
           !shadow_word(m,c->packet+20,material))return 0;
    }
    for(i=0;i<3;i++){
        if(!shadow_word(m,c->scalar+i*4,input->vertices[i]))return 0;
        for(j=0;j<2;j++){uint32_t bits;memcpy(&bits,&input->uv[i][j],4);
            if(!shadow_word(m,c->uv+i*8+j*4,bits))return 0;}
    }
    return !m->fault;
}
static int shadow_link(GameMemory *m,GameAddress tail,GameAddress next){
    return render_arena_packet_set_next(tail,next) || shadow_word(m,tail+12,next);
}
/*5037b0: three light-facing box faces, split into paired triangles, then
 *terrain projection, camera transform and the existing near/screen clippers.
 *Transient data stays in the caller's bounded frame storage or native arena. */
int scene_submission_bounds_shadow(GameMemory *m,GameAddress descriptor,const float world[12],SceneSubmissionState *s){
    static const uint8_t faces[3][2][4]={{{0,3,2,1},{4,5,6,7}},{{0,1,5,4},{3,7,6,2}},{{0,4,7,3},{1,2,6,5}}};
    static const int8_t signs[8][3]={{1,1,1},{-1,1,1},{-1,1,-1},{1,1,-1},{1,-1,1},{-1,-1,1},{-1,-1,-1},{1,-1,-1}};
    GameplayClipCursors *c,*source;GameplayClipPolygonInput input[6]={{0}};GameplayClipStatus status={0};
    float corners[8][3],extent[3],light[3],ratio[2],bounds[4];GameAddress begin,end,clip_begin,polygons,head,tail=0;
    unsigned axis,face,triangle,count=0;float half,threshold;
    if(!m || m->fault || !s || !world)return 0;
    c=s->frame.cursors;
    source=s->shadow_sources?s->shadow_sources:c;
    if(!game_memory_read_u32(m,0x597008))return !m->fault;
    if(!s->shadow_queue || !c || !render_arena_current())return 0;
    half=shadow_real(m,0x575230);threshold=shadow_real(m,0x575220);
    for(axis=0;axis<3;axis++){extent[axis]=(float)((long double)shadow_real(m,descriptor+0x24+axis*4)*half);light[axis]=shadow_real(m,0x624f20+axis*4);}
    ratio[0]=(float)((long double)light[0]*(shadow_real(m,0x575228)/(long double)light[1]));
    ratio[1]=(float)((long double)light[2]*(shadow_real(m,0x575228)/(long double)light[1]));
    for(axis=0;axis<8;axis++){float p[3]={signs[axis][0]*extent[0],signs[axis][1]*extent[1],signs[axis][2]*extent[2]};shadow_transform(corners[axis],world,p);}
    begin=source->vertex;polygons=source->packet;
    for(face=0;face<3;face++){
        const uint8_t *order=faces[face][0];float a[3],b[3];long double dot;unsigned vertex;
        for(axis=0;axis<3;axis++){a[axis]=(float)((long double)corners[order[1]][axis]-corners[order[0]][axis]);b[axis]=(float)((long double)corners[order[3]][axis]-corners[order[0]][axis]);}
        dot=((long double)a[1]*b[2]-(long double)a[2]*b[1])*light[0]+
            ((long double)a[2]*b[0]-(long double)a[0]*b[2])*light[1]+
            ((long double)a[0]*b[1]-(long double)a[1]*b[0])*light[2];
        if(dot>=threshold)order=faces[face][1];
        for(vertex=0;vertex<4;vertex++){
            float p[3],view[3];long double height,delta;
            memcpy(p,corners[order[vertex]],sizeof p);height=gameplay_geometry_terrain_height(m,p[0],p[2]);delta=height-p[1];
            p[0]=(float)(delta*ratio[0]+p[0]);p[2]=(float)(delta*ratio[1]+p[2]);p[1]=(float)height;
            shadow_transform(view,s->camera,p);if(!shadow_vertex(m,source->vertex+vertex*16,view))return 0;
        }
        for(triangle=0;triangle<2;triangle++){
            static const uint8_t split[2][3]={{0,1,2},{0,2,3}};unsigned corner;
            input[count].count=3;input[count].flags=0x811;
            for(corner=0;corner<3;corner++){
                input[count].vertices[corner]=source->vertex+split[triangle][corner]*16;
                input[count].uv[corner][0]=input[count].uv[corner][1]=.5f;}
            if(!shadow_triangle(m,source,&input[count],0x624f08))return 0;
            ++count;source->packet+=40;source->scalar+=12;source->uv+=24;
        }
        source->vertex+=64;
    }
    if(m->fault)return 0;
    end=source->vertex;clip_begin=c->vertex;
    {uint32_t far_bits=game_memory_read_u32(m,0x597f7c);int32_t far_integer;memcpy(&far_integer,&far_bits,4);
     float far_z=(float)((long double)far_integer+shadow_real(m,0x575234));
     head=gameplay_clipper_clip_polygon_inputs_state(m,polygons,6,input,10.0f,far_z,c);}
    gameplay_geometry_project_vertices(m,begin,end,s->frame.center_x,s->frame.center_y,s->frame.scale,s->frame.vertical,s->frame.numerator);
    gameplay_geometry_project_vertices(m,clip_begin,c->vertex,s->frame.center_x,s->frame.center_y,s->frame.scale,s->frame.vertical,s->frame.numerator);
    head=gameplay_geometry_filter_backfaces(m,head,s->frame.backface_threshold);
    for(axis=0;axis<4;axis++)bounds[axis]=(float)((long double)s->frame.bounds[axis]-shadow_real(m,axis<2?0x57522c:0x575228));
    head=gameplay_clipper_screen_chain_state(m,head,&tail,bounds,c,&status);
    if(m->fault)return 0;
    if(head){if(!shadow_link(m,tail,*s->shadow_queue))return 0;*s->shadow_queue=head;}
    return 1;
}
/*504580: a terrain-following textured quad, split with the original winding
 *and256-unit UVs. Existing arena/near-plane/screen-clipping owners are reused. */
int scene_submission_ground_shadow(GameMemory *m,const float position[3],float diameter,SceneSubmissionState *s){
    static const unsigned split[2][3]={{0,2,1},{0,3,2}};
    static const float uv_corners[4][2]={{0,0},{256,0},{256,256},{0,256}};
    GameplayClipPolygonInput input[2]={{0}};GameplayClipStatus status={0};GameplayClipCursors *c,*source;
    float p[4][3],bounds[4];long double half;GameAddress begin,end,clip_begin,polygons,head,tail=0;unsigned i,j;
    if(!m || !s || !position || m->fault || !(c=s->frame.cursors) || !s->shadow_queue || !render_arena_current())return 0;
    source=s->shadow_sources?s->shadow_sources:c;
    half=(long double)diameter*shadow_real(m,0x57522c);
    p[0][0]=(float)((long double)position[0]-half);p[0][2]=(float)((long double)position[2]-half);
    p[1][0]=(float)((long double)p[0][0]+diameter);p[1][2]=p[0][2];
    p[2][0]=p[1][0];p[2][2]=(float)((long double)p[0][2]+diameter);
    p[3][0]=(float)((long double)p[1][0]-diameter);p[3][2]=p[2][2];
    begin=source->vertex;polygons=source->packet;
    for(i=0;i<4;i++){
        float view[3];
        p[i][1]=(float)gameplay_geometry_terrain_height(m,p[i][0],p[i][2]);
        shadow_transform(view,s->camera,p[i]);if(!shadow_vertex(m,begin+i*16,view))return 0;
    }
    for(i=0;i<2;i++){
        input[i].count=3;input[i].flags=0x811;
        for(j=0;j<3;j++){
            unsigned corner=split[i][j];input[i].vertices[j]=begin+corner*16;
            input[i].uv[j][0]=uv_corners[corner][0];input[i].uv[j][1]=uv_corners[corner][1];
        }
        if(!shadow_triangle(m,source,&input[i],0x624f38))return 0;
        source->packet+=40;source->scalar+=12;source->uv+=24;
    }
    source->vertex+=64;end=source->vertex;clip_begin=c->vertex;
    {uint32_t bits=game_memory_read_u32(m,0x597f7c);int32_t distance;memcpy(&distance,&bits,4);
     head=gameplay_clipper_clip_polygon_inputs_state(m,polygons,2,input,10.0f,(float)((long double)distance+shadow_real(m,0x575234)),c);}
    gameplay_geometry_project_vertices(m,begin,end,s->frame.center_x,s->frame.center_y,s->frame.scale,s->frame.vertical,s->frame.numerator);
    gameplay_geometry_project_vertices(m,clip_begin,c->vertex,s->frame.center_x,s->frame.center_y,s->frame.scale,s->frame.vertical,s->frame.numerator);
    head=gameplay_geometry_filter_backfaces(m,head,s->frame.backface_threshold);
    for(i=0;i<4;i++)bounds[i]=(float)((long double)s->frame.bounds[i]-shadow_real(m,i<2?0x57522c:0x575228));
    head=gameplay_clipper_screen_chain_state(m,head,&tail,bounds,c,&status);
    if(m->fault)return 0;
    if(head){if(!shadow_link(m,tail,*s->shadow_queue))return 0;*s->shadow_queue=head;}
    return 1;
}
static int submit(SubmissionWalk *w,const SceneInstance *instance,const float active[12]){
    GameplayMeshSubmission mesh={0};GameplayClipPolygonInput *inputs;
    const SceneRenderDescriptor *render=&instance->render;SceneMesh *scene=instance->mesh;
    float composed[12];uint32_t i,j;int result;
    inputs=calloc(render->polygon_count,sizeof(*inputs));if(!inputs)return 0;
    mesh.source_vertices=render->source_vertices;mesh.transformed_vertices=render->transformed_vertices;
    mesh.vertex_count=(int32_t)render->vertex_count;mesh.polygons=render->polygons;
    mesh.polygon_count=(int32_t)render->polygon_count;mesh.texture_state=instance->texture_state;
    mesh.flags=(instance->flags&1u)?0x1du:0x21du;
    mesh.far_depth=(instance->flags&0x20u)?61000.0f:w->state->far_depth;
    for(i=0;i<render->polygon_count;i++){
        const ScenePolygonPresentation *presentation=&instance->presentation[i];
        inputs[i].count=3;inputs[i].flags=presentation->flags;
        for(j=0;j<3u;j++){
            uint32_t vertex=scene->polygons[i].vertex[j];AssetMeshUV texture=presentation->uv[j];
            if(vertex>=render->vertex_count){free(inputs);return 0;}
            if(scene->polygons[i].uv[j].u_bits!=texture.u_bits ||
               scene->polygons[i].uv[j].v_bits!=texture.v_bits){
                if(!scene_mesh_write_polygon_uv(scene,i,j,&texture)){free(inputs);return 0;}
                ++w->state->uv_updates;
            }
            inputs[i].vertices[j]=render->transformed_vertices+vertex*16u;
            memcpy(&inputs[i].uv[j][0],&texture.u_bits,4u);
            memcpy(&inputs[i].uv[j][1],&texture.v_bits,4u);
        }
    }
    game_matrix_compose_values(composed,w->state->camera,active);
    if((instance->flags&2u) && !scene_submission_bounds_shadow(w->memory,instance->descriptor,active,w->state)){free(inputs);return 0;}
    if(instance->extension && !(instance->flags&1u)){
        GameplayRenderFrame visible=w->state->frame;
        visible.numerator=w->state->visible_numerator;
        visible.queue_head_value=w->state->visible_queue;
        visible.bounds[0]-=w->state->visible_offset;
        visible.bounds[1]-=w->state->visible_offset;
        visible.bounds[2]-=visible.numerator;
        visible.bounds[3]-=visible.numerator;
        result=gameplay_geometry_submit_visible_scene_mesh(w->memory,&mesh,&visible,
            composed,scene->vertices,mesh.polygons,mesh.polygon_count,scene->polygons);
    }else result=gameplay_geometry_submit_scene_mesh(w->memory,&mesh,&w->state->frame,
        composed,scene->vertices,inputs);
    free(inputs);
    if(result && !w->memory->fault){++w->state->submitted;++w->state->render_nodes;}
    return result && !w->memory->fault;
}

static int walk_node(SubmissionWalk *w,uint32_t index,unsigned depth){
    SceneSubmissionState *s=w->state;const SubmissionNode *node=&w->nodes[index];
    uint32_t child;float *active;
    if(depth>=8u || s->depth>=9u)return 0;
    memcpy(s->matrices[s->depth+1u],s->matrices[s->depth],sizeof(s->matrices[0]));
    ++s->depth;s->dirty[s->depth]=1u;active=s->matrices[s->depth];
    game_matrix_apply_presentation(active,node->instance->local,s->presentation_policy);
    if(node->instance->descriptor && !submit(w,node->instance,active))return 0;
    for(child=node->child;child!=UINT32_MAX;child=w->nodes[child].next)
        if(!walk_node(w,child,depth+1u))return 0;
    --s->depth;return !w->memory->fault;
}

/*450460 mutable leaf: packed source geometry remains borrowed from the bound
 *asset; the mesh owns stride16 transformed vertices, packets, links and UVs.
 *Use bounded record access directly, without constructing a second scene. */
static int submit_mutable(GameMemory *m,GameAddress root,SceneSubmissionState *s){
    GameplayMeshSubmission mesh={0};GameplayClipCursors *c=s->frame.cursors;
    GameAddress container=game_memory_read_u32(m,root+0x50),key;
    float local[12],active[12],composed[12];uint32_t flags,i;int zero,result;
    /*450460 stops at the submitted root before following its parent/sibling.
     *Attachment models retain those back links (and optional names). They do
     *not turn this leaf into a child graph;4c is the actual child link. */
    if(!c || !s->frame.queue_head_value || s->depth>=9 ||
       game_memory_read_u32(m,root+0x4c))return 0;
    mesh.source_vertices=game_memory_read_u32(m,container);
    mesh.transformed_vertices=game_memory_read_u32(m,container+4);
    mesh.vertex_count=(int32_t)game_memory_read_u32(m,container+0x14);
    mesh.polygons=game_memory_read_u32(m,container+0x18);
    mesh.polygon_count=(int32_t)game_memory_read_u32(m,container+0x1c);
    if(m->fault || mesh.vertex_count<0 || mesh.polygon_count<0 ||
       mesh.vertex_count>game_memory_read_u16(m,root+0x5c) ||
       mesh.polygon_count>game_memory_read_u16(m,root+0x5e))return 0;
    /* Mutable clones currently have no animated texture table. A foreign
     *animation binding remains explicit until its owner is reconstructed. */
    if(game_memory_read_u32(m,container+0x38))return 0;
    /* Near/screen clipping rewrites packets, while perspective mapping rewrites
     * work UVs. Neither is next frame's triangle input. The mutable owner keeps
     * canonical links and UVs separately; restore only its derived workspace. */
    for(i=0;i<(uint32_t)mesh.polygon_count;i++){
        GameAddress packet=mesh.polygons+i*40u;
        GameAddress links=game_memory_read_u32(m,container+0x10)+i*12u;
        GameAddress uv=game_memory_read_u32(m,container+8)+i*24u;
        GameAddress work=game_memory_read_u32(m,container+12)+i*24u;
        unsigned j;
        game_memory_write_u32(m,packet,3);
        game_memory_write_u32(m,packet+4,links);
        game_memory_write_u32(m,packet+16,work);
        for(j=0;j<24;j+=4)game_memory_write_u32(m,work+j,game_memory_read_u32(m,uv+j));
    }
    flags=game_memory_read_u8(m,root+0x3c);
    if((flags&2) && game_memory_read_u32(m,0x597008) && !s->shadow_queue)return 0;
    mesh.flags=(flags&1)?0x1d:0x21d;
    mesh.far_depth=(flags&0x20)?61000.0f:s->far_depth;
    mesh.texture_state=game_memory_read_u32(m,root+0x38);
    if(!(flags&1)){
        uint8_t palette[4];GameMemory *pm=s->palette_memory?s->palette_memory:m;
        key=mesh.texture_state?mesh.texture_state:0x604380;
        for(i=0;i<4;i++)palette[i]=game_memory_read_u8(pm,key+i);
        if(pm->fault || !render_arena_palette_register(key,palette))return 0;
    }
    zero=!c->vertex && !c->packet && !c->uv && !c->scalar;
    if(zero){if(!render_arena_current())return 0;
        c->vertex=render_arena_view(0);c->packet=render_arena_view(3);
        c->uv=render_arena_view(2);c->scalar=render_arena_view(1);}
    memset(s->dirty,0,sizeof(s->dirty));
    memset(s->matrices[s->depth],0,sizeof(s->matrices[0]));
    s->matrices[s->depth][0]=s->matrices[s->depth][5]=s->matrices[s->depth][10]=1;
    s->dirty[s->depth]=1;
    memcpy(active,s->matrices[s->depth],sizeof(active));
    for(i=0;i<12;i++){uint32_t bits=game_memory_read_u32(m,root+4+i*4);memcpy(local+i,&bits,4);}
    game_matrix_apply_presentation(active,local,s->presentation_policy);
    memcpy(s->matrices[s->depth+1],active,sizeof(active));s->dirty[s->depth+1]=1;
    game_matrix_compose_values(composed,s->camera,active);
    if((flags&2) && !scene_submission_bounds_shadow(m,container,active,s))return 0;
    result=!m->fault && gameplay_geometry_submit_scene_mesh(m,&mesh,&s->frame,composed,NULL,NULL);
    s->nodes=1;s->submitted=s->render_nodes=result && mesh.polygon_count>0;s->uv_updates=0;s->reject=0;
    if(zero && c->vertex==render_arena_view(0) && c->packet==render_arena_view(3) &&
       c->uv==render_arena_view(2) && c->scalar==render_arena_view(1))memset(c,0,sizeof(*c));
    return result && !m->fault?1:-1;
}

int scene_submission_owned(GameMemory *memory,GameAddress root,SceneSubmissionState *s){
    SubmissionWalk w={0};GameplayClipCursors *cursors;uint32_t root_index,i;int zero,result;
    if(!memory || !s || memory->fault)return -1;
    if(!root){s->reject=2;return 0;}
    if(!scene_instance_owner_is_holder(root) && mesh_buffer_is_native(memory,root))return submit_mutable(memory,root,s);
    w.memory=memory;w.state=s;s->submitted=0u;s->nodes=0u;s->render_nodes=0u;s->uv_updates=0u;s->reject=0u;
    memset(s->dirty,0,sizeof(s->dirty));
    if(s->depth>=9u || !s->frame.cursors || !s->frame.queue_head_value){s->reject=5u;return 0;}
    /*450460 still walks transforms/children when a known holder has no mesh.
     *Retiring attachment graphs can be empty; capture continues to reject
     *foreign or malformed owners. An empty graph is not a missing graph. */
    if(!capture(&w,root,0u,&root_index)){
        free(w.nodes);return w.failed?-1:0;
    }
    cursors=s->frame.cursors;zero=!cursors->vertex && !cursors->packet && !cursors->uv && !cursors->scalar;
    /* The frame owner may still supply bounded legacy buffer IDs. Geometry
     * accessors validate those destinations; IDs are never host pointers. */
    if(zero && !render_arena_current()){s->reject=6u;free(w.nodes);return 0;}
    for(i=0;i<w.count;i++){
        const SceneInstance *instance=w.nodes[i].instance;uint8_t entries[4];unsigned j;
        GameMemory *palette_memory=s->palette_memory?s->palette_memory:memory;
        GameAddress key=instance->texture_state?instance->texture_state:0x604380u;
        if(!instance->descriptor || (instance->flags&1u))continue;
        for(j=0;j<4u;j++)entries[j]=game_memory_read_u8(palette_memory,key+j);
        if(palette_memory->fault){free(w.nodes);return -1;}
        if(!render_arena_palette_register(key,entries)){s->reject=7u;free(w.nodes);return 0;}
    }
    if(zero){cursors->vertex=render_arena_view(0u);cursors->packet=render_arena_view(3u);
        cursors->uv=render_arena_view(2u);cursors->scalar=render_arena_view(1u);}
    memset(s->matrices[s->depth],0,sizeof(s->matrices[0]));
    s->matrices[s->depth][0]=1.0f;s->matrices[s->depth][5]=1.0f;s->matrices[s->depth][10]=1.0f;
    s->dirty[s->depth]=1u;s->nodes=w.count;
    result=walk_node(&w,root_index,0u);
    if(result && zero && cursors->vertex==render_arena_view(0u) &&
       cursors->packet==render_arena_view(3u) && cursors->uv==render_arena_view(2u) &&
       cursors->scalar==render_arena_view(1u))memset(cursors,0,sizeof(*cursors));
    free(w.nodes);return result && !memory->fault?1:-1;
}
