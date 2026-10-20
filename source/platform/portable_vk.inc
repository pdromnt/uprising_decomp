/* Experimental native device consuming the established projected vertices and
 * owned texture bytes. The GL comparator shares those material semantics;
 * this path submits Vulkan buffers/shaders and never creates a GL context. */
typedef struct Vk1Device {
    VulkanMenu *menu;VulkanDraw frame[2];VulkanDrawTexture sky[3],remote_mask;
    VulkanDrawTexture tiles;
    struct {uint64_t hash;uint32_t next;uint8_t pixels[4096];} tile[1024];
    uint32_t tile_buckets[1024],tile_count;
    uint64_t frames,submissions,uploads,atlas_uploads;
    Uint64 frame_start;
    uint32_t written_range[2];
    int track_text,scratch_clean;
    RenderMotionHistory history;
    RenderMotionPlane sky_history[2];uint64_t sky_motion_valid;
    uint32_t history_camera,history_mode;
    float history_projection[5];
    int jitter_enabled;
    uint64_t motion_samples,motion_valid;
    uint64_t motion_clipped;
    VulkanDraw output[2];int composed[2],fsr_requested,fsr_view,fsr_reset;
    Uint64 fsr_time;uint64_t fsr_frames,fsr_resets;
    uint32_t fsr_crop[4];int fsr_monitor;uint64_t fsr_monitor_frames;
    uint32_t fsr_output[2],fsr_context_output[2];
#ifdef UPRISING_FSR3_UPSCALER
    Fsr3Upscaler *upscaler;
    VulkanTemporal temporal;
#endif
} Vk1Device;
static void vk1_history_reset(Vk1Device *n){
    render_motion_reset(&n->history);memset(n->sky_history,0,sizeof(n->sky_history));
}
static VulkanDraw *vk1_target(unsigned index){
    Vk1Device *n=gl1_current->vulkan;return n->composed[index]?n->output+index:n->frame+index;
}
static VulkanDraw *vk1_back(void){return vk1_target(gl1_current->back);}
static int vk1_flush(VulkanDraw *d,int resume){
    if(d->recording)gl1_current->vulkan->submissions++;
    if(d->recording && !vulkan_draw_finish(d,NULL,0)){gl1_fail("Vulkan submission");return 0;}
    if(resume && !d->recording && !vulkan_draw_resume(d)){gl1_fail("Vulkan render target continuation");return 0;}
    return 1;
}
static int vk1_ready_draw(void){
    VulkanDraw *d=vk1_back();return gl1_ready() && (d->recording || vulkan_draw_resume(d));
}
static int vk1_flush_all(void){
    Vk1Device *n=gl1_current->vulkan;unsigned i;
    for(i=0;i<2;i++)if(!vk1_flush(n->frame+i,0) || !vk1_flush(n->output+i,0))return 0;
    return 1;
}
static int vk1_scene_complete(void){
#ifdef UPRISING_FSR3_UPSCALER
    Gl1Device *g=gl1_current;Vk1Device *n=g->vulkan;VulkanDraw *scene=n->frame+g->back,*target=n->output+g->back;
    VulkanDevice *v=vulkan_menu_device(n->menu);VkCommandBuffer command;const VulkanImage *image;
    const float clear[4]={0,0,0,1};float focal,fov,elapsed;Uint64 now;
    uint32_t width=n->fsr_crop[2],height=n->fsr_crop[3];const VulkanImage *color,*depth,*motion;
    uint32_t output_width=(uint32_t)(((uint64_t)width*n->fsr_output[0]+g->width-1)/g->width);
    uint32_t output_height=(uint32_t)(((uint64_t)height*n->fsr_output[1]+g->height-1)/g->height);
    if(!n->fsr_view){
        if(!n->fsr_requested && n->upscaler){fsr3_upscaler_close(n->upscaler);n->upscaler=NULL;vulkan_temporal_close(&n->temporal);}
        return 1;
    }
    focal=n->history_projection[2]*n->history_projection[3]*n->history_projection[4];
    if(!isfinite(focal) || focal<=0)return 0;
    fov=2*atanf((float)height/(2*focal));
    if(!isfinite(fov) || fov<=0 || fov>=3.14159265f)return 0;
    if(n->upscaler && (n->temporal.width!=width || n->temporal.height!=height || n->fsr_context_output[0]!=output_width || n->fsr_context_output[1]!=output_height)){
        fsr3_upscaler_close(n->upscaler);n->upscaler=NULL;vulkan_temporal_close(&n->temporal);n->fsr_reset=1;
    }
    if(!n->upscaler){
        n->upscaler=fsr3_upscaler_open(v,width,height,output_width,output_height);if(!n->upscaler)return 0;n->fsr_reset=1;
        n->fsr_context_output[0]=output_width;n->fsr_context_output[1]=output_height;
    }
    if(target->device && (target->width!=n->fsr_output[0] || target->height!=n->fsr_output[1])){
        if(!vk1_flush(target,0))return 0;vulkan_draw_close(target);
    }
    if(!target->device){
        if(!vulkan_draw_open(target,v,n->fsr_output[0],n->fsr_output[1]) || !vulkan_draw_begin(target,clear) || !vulkan_draw_finish(target,NULL,0))return 0;
        /* Geometry stays in legacy screen coordinates; the viewport maps HUD
         * edges directly onto the larger output, after temporal reconstruction. */
        target->material.extent[0]=(float)g->width;target->material.extent[1]=(float)g->height;
    }
    if(!vk1_flush(scene,0))return 0;
    if(!n->temporal.device && !vulkan_temporal_open(&n->temporal,v,width,height))return 0;
    if(n->fsr_monitor && !n->temporal.cropped[0].image && !vulkan_temporal_crop_open(&n->temporal))return 0;
    now=SDL_GetTicksNS();elapsed=n->fsr_time?(float)((now-n->fsr_time)/1000000.0):16.667f;
    if(elapsed>250){n->fsr_reset=1;elapsed=250;}if(elapsed<=0)elapsed=.001f;
    if(!vulkan_command_begin(v,&command))return 0;
    color=&scene->scene;depth=&scene->reciprocal_depth;motion=&scene->motion;
    if(n->fsr_monitor){
        if(!vulkan_temporal_crop(&n->temporal,command,color,depth,motion,n->fsr_crop[0],n->fsr_crop[1],g->width,g->height))goto command_fail;
        color=n->temporal.cropped;depth=n->temporal.cropped+1;motion=n->temporal.cropped+2;
    }
    if(!vulkan_temporal_record(&n->temporal,command,motion))goto command_fail;
    image=fsr3_upscaler_dispatch(n->upscaler,command,color,depth,motion,&n->temporal.rejection,
        scene->scene_jitter[0],scene->scene_jitter[1],elapsed,10,fov,n->fsr_reset,1);
    if(!image || !vulkan_temporal_compose(&n->temporal,command,image,&scene->scene,&scene->color,&target->color,
        n->fsr_crop[0],n->fsr_crop[1],target->width,target->height))goto command_fail;
    if(!vulkan_command_submit(v,command,VK_NULL_HANDLE,VK_NULL_HANDLE))return 0;
    n->fsr_time=now;n->fsr_frames++;if(n->fsr_reset)n->fsr_resets++;
    n->fsr_reset=0;n->composed[g->back]=1;
    if(n->fsr_monitor)n->fsr_monitor_frames++;
    if(n->fsr_frames==1){char message[224];snprintf(message,sizeof(message),
        "FSR3_GAME_DISPATCH: %ux%u -> %ux%u fov=%g near=10 phases=%u masks=conservative-R8; HUD follows dispatch\n",
        width,height,output_width,output_height,fov,fsr3_upscaler_jitter_phases(width,output_width));
        game_application_log(g->application,"%s",message);}
    return 1;
command_fail:v->FreeCommandBuffers(v->device,v->commands,1,&command);return 0;
#endif
    return 1;
}
static int vk1_open(SDL_Window *window,VulkanMenu *menu){
    Gl1Device *g=gl1_current;unsigned i;const float clear[4]={0,0,0,0};
    g->vulkan=calloc(1,sizeof(*g->vulkan));if(!g->vulkan)return 0;
    g->vulkan->menu=menu;g->failed=0;g->thread=SDL_GetCurrentThreadID();g->window=window;
    {const char *jitter=SDL_getenv("UPRISING_VULKAN_JITTER");g->vulkan->jitter_enabled=jitter && !strcmp(jitter,"1");}
    g->width=640;g->height=480;g->front=0;g->back=1;
    g->lfb=malloc((size_t)g->width*g->height*2);g->rgba=malloc((size_t)g->width*g->height*4);
    if(!g->lfb || !g->rgba)goto fail;
    for(i=0;i<2;i++)if(!vulkan_draw_open(g->vulkan->frame+i,vulkan_menu_device(menu),g->width,g->height) ||
       !vulkan_draw_begin(g->vulkan->frame+i,clear) || !vulkan_draw_finish(g->vulkan->frame+i,NULL,0))goto fail;
    g->palette_generation=1;g->min_filter=g->mag_filter=GL_LINEAR;g->wrap_s=g->wrap_t=GL_REPEAT;
    g->constant[0]=g->constant[1]=g->constant[2]=g->constant[3]=1;g->alpha_function=7;
    g->native_depth_write=1;g->native_depth_func=1;g->native_blend=g->native_fog=0;
    g->projection_center=320;g->projection_scale=381.3607f;
    game_application_log(g->application,"%s\n","VULKAN_LEVEL_DEVICE_READY: native projected geometry and owned targets");return 1;
fail:gl1_fail("Vulkan level device allocation");gl1_close();return 0;
}
static void vk1_texture_release(Gl1Texture *texture){
    if(!texture->vulkan_texture)return;
    if(!vk1_flush_all())vk1_back()->recording=0;
    vulkan_draw_texture_destroy(&gl1_current->vulkan->frame[0],texture->vulkan_texture);
    free(texture->vulkan_texture);texture->vulkan_texture=NULL;
    free(texture->vulkan_pixels);texture->vulkan_pixels=NULL;
}
static void vk1_sky_release(void){
    Gl1Device *g=gl1_current;unsigned i;if(!vk1_flush(vk1_back(),0))return;
    memset(g->vulkan->sky_history,0,sizeof(g->vulkan->sky_history));
    for(i=0;i<3;i++)vulkan_draw_texture_destroy(g->vulkan->frame,g->vulkan->sky+i);
}
static void vk1_close(void){
    Gl1Device *g=gl1_current;unsigned i;
    vk1_flush_all();
    vulkan_menu_device(g->vulkan->menu)->DeviceWaitIdle(vulkan_menu_device(g->vulkan->menu)->device);
    for(i=0;i<4096;i++)vk1_texture_release(g->textures+i);
    vk1_sky_release();vulkan_draw_texture_destroy(g->vulkan->frame,&g->vulkan->remote_mask);vulkan_draw_texture_destroy(g->vulkan->frame,&g->vulkan->tiles);
#ifdef UPRISING_FSR3_UPSCALER
    fsr3_upscaler_close(g->vulkan->upscaler);
    vulkan_temporal_close(&g->vulkan->temporal);
#endif
    for(i=0;i<2;i++){vulkan_draw_close(g->vulkan->output+i);vulkan_draw_close(g->vulkan->frame+i);}
    render_motion_release(&g->vulkan->history);
    free(g->vulkan);g->vulkan=NULL;
}
static void vk1_refresh(Gl1Texture *texture){
    Gl1Device *g=gl1_current;VulkanDrawTexture *t=texture->vulkan_texture;VulkanDraw *owner=g->vulkan->frame;int chroma=g->chroma_enabled;
    if(texture->ui_tile){
        Vk1Device *n=g->vulkan;uint64_t hash=UINT64_C(14695981039346656037);unsigned i,slot,bucket;
        g->chroma_enabled=0;gl1_convert(texture,texture->pixels,1024,g->rgba);g->chroma_enabled=chroma;
        for(i=0;i<4096;i++){hash^=g->rgba[i];hash*=UINT64_C(1099511628211);}bucket=(unsigned)(hash&1023u);
        for(slot=n->tile_buckets[bucket];slot;slot=n->tile[slot-1].next)
            if(n->tile[slot-1].hash==hash && !memcmp(n->tile[slot-1].pixels,g->rgba,4096))break;
        if(!slot){
            if(!n->tiles.image.image){
                uint8_t *empty=calloc(1024u*1024u,4);int ok=empty && vulkan_draw_texture_create(owner,&n->tiles,empty,1024,1024,0,0);free(empty);
                if(!ok){gl1_fail("Vulkan tile atlas");return;}
            }
            if(n->tile_count==1024){
                if(!vk1_flush_all())return;
                n->tile_count=0;memset(n->tile_buckets,0,sizeof(n->tile_buckets));
                /* Cached CPU texture records must reacquire an atlas version. */
                for(i=0;i<4096;i++)if(g->textures[i].ui_tile)g->textures[i].dirty=1;
            }
            i=n->tile_count++;slot=i+1;n->tile[i].hash=hash;
            memcpy(n->tile[i].pixels,g->rgba,4096);n->tile[i].next=n->tile_buckets[bucket];n->tile_buckets[bucket]=slot;
            if(!vulkan_draw_texture_region(owner,&n->tiles,g->rgba,(i%32)*32,(i/32)*32,32,32)){gl1_fail("Vulkan tile atlas update");return;}
            n->atlas_uploads++;
        }
        texture->native_tile=slot;texture->palette_generation=g->palette_generation;texture->dirty=0;
        texture->gpu_width=texture->width;texture->gpu_height=texture->height;return;
    }
    /* Like the comparator's GL_LINEAR minification, the current native sampler
     * consumes the selected base LOD. Retain all original mip bytes in staging. */
    g->chroma_enabled=0;gl1_convert(texture,texture->pixels,texture->width*texture->height,g->rgba);g->chroma_enabled=chroma;
    if(t && t->image.image && t->width==texture->width && t->height==texture->height &&
       texture->vulkan_pixels && !memcmp(texture->vulkan_pixels,g->rgba,(size_t)texture->width*texture->height*4))goto current;
    if(!vk1_flush_all())return;
    g->vulkan->uploads++;
    if(!t){t=calloc(1,sizeof(*t));if(!t){gl1_fail("Vulkan texture owner");return;}texture->vulkan_texture=t;}
    if(t->image.image && (t->width!=texture->width || t->height!=texture->height))vulkan_draw_texture_destroy(owner,t);
    if(!t->image.image){
        if(!vulkan_draw_texture_create(owner,t,g->rgba,texture->width,texture->height,1,1)){gl1_fail("Vulkan texture creation");return;}
    }else if(!vulkan_draw_texture_update(owner,t,g->rgba,(size_t)texture->width*texture->height*4)){gl1_fail("Vulkan texture update");return;}
    {
        /* Upload heaps may be write-combined: reading them for every palette
         * comparison is prohibitively slow on discrete GPUs. Keep this cache
         * in ordinary CPU memory and retire it with the texture owner. */
        size_t bytes=(size_t)texture->width*texture->height*4;
        uint8_t *pixels=realloc(texture->vulkan_pixels,bytes);
        if(!pixels){gl1_fail("Vulkan texture comparison cache");return;}
        texture->vulkan_pixels=pixels;memcpy(pixels,g->rgba,bytes);
    }
current:
    texture->palette_generation=g->palette_generation;texture->chroma_enabled=g->chroma_enabled;texture->chroma=g->chroma;texture->dirty=0;
    texture->ncc_table=g->ncc_selected;texture->ncc_generation=g->ncc_generation[g->ncc_selected];
    texture->gpu_width=texture->width;texture->gpu_height=texture->height;
}
static int vk1_motion_source(void *context,uint32_t reference,RenderMotionSource *source){
    RenderArenaVertex *v=render_arena_vertices(reference,1);const RenderArenaMotionClip *clip;
    (void)context;
    if(v){
        source->key=render_arena_motion_key(reference);clip=render_arena_motion_clip(reference);
        if(clip){source->parents[0]=clip->previous;source->parents[1]=clip->current;source->fraction=clip->fraction;source->screen_space=clip->screen_space;}
    }else{
        v=scene_projection_storage_transformed(reference,1);
        source->key=scene_projection_storage_vertex_key(reference);
    }
    if(!v)return 0;
    memcpy(source->current,v->position,sizeof(source->current));return 1;
}
static VulkanDrawVertex vk1_vertex(const GameRenderVertex *v){
    Gl1Device *g=gl1_current;VulkanDrawVertex out={0};float depth,q=v->oow,divisor=g->color_mode==RENDER_COLOR_INVERSE_TEXTURE?256.0f:255.0f;
    if(g->depth_mode==2 || g->depth_mode==4){
        int value=gl1_w_depth(q)+(g->depth_mode==2?(int16_t)g->depth_bias:0);depth=(value<0?0:value>65535?65535:value)/65535.0f;
    }else depth=(v->ooz+(g->depth_mode==1?g->depth_bias:0))/65535.0f;
    out.position[0]=v->x;out.position[1]=v->y;out.position[2]=fminf(1,fmaxf(0,depth));
    out.color[0]=v->r/divisor;out.color[1]=v->g/divisor;out.color[2]=v->b/divisor;out.color[3]=v->a/255.0f;
    if(g->constant_enabled)memcpy(out.color,g->constant,sizeof(out.color));
    if(!isfinite(q) || !q)q=v->oow0;if(!isfinite(q) || !q)q=1;
    out.uv[2]=q;
    if(g->textured && g->bound){
        float s=256,t=256;if(g->bound->width<g->bound->height)s*=((float)g->bound->width/g->bound->height);
        else if(g->bound->height<g->bound->width)t*=((float)g->bound->height/g->bound->width);
        out.uv[0]=v->sow0/s;out.uv[1]=v->tow0/t;
        if(g->bound->ui_tile){unsigned slot=g->bound->native_tile-1;out.uv[0]=(out.uv[0]*32+(slot%32)*32*q)/1024;out.uv[1]=(out.uv[1]*32+(slot/32)*32*q)/1024;}
    }
    {float lateral=(v->x-g->projection_center)/g->projection_scale;out.fog=q>0 && q<0.5f?sqrtf(1+lateral*lateral)/q:0;}
    if(g->world_depth_capture && g->motion_references){
        unsigned i;
        for(i=0;i<g->motion_count;i++)if(v==g->motion_vertices+i){
            Vk1Device *n=g->vulkan;uint32_t reference=g->motion_references[i];
            n->motion_samples++;
            if(render_motion_resolve(&n->history,n->frames+1,reference,vk1_motion_source,NULL,out.previous)){
                n->motion_valid++;if(render_arena_motion_clip(reference))n->motion_clipped++;
            }
            break;
        }
    }
    return out;
}
static void vk1_motion_triangle(VulkanDrawVertex v[3]){
    if(!v[0].previous[3] || !v[1].previous[3] || !v[2].previous[3])
        memset(v[0].previous,0,sizeof(v[0].previous)),memset(v[1].previous,0,sizeof(v[1].previous)),memset(v[2].previous,0,sizeof(v[2].previous));
}
static uint32_t vk1_material(VulkanDraw *d){
    Gl1Device *g=gl1_current;VulkanDrawMaterial *m=&d->material;
    d->capture_depth=g->world_depth_capture;
    m->color_mode=g->textured?(uint32_t)g->color_mode:g->constant_enabled?1:0;
    m->mask_enabled=0;d->mask_texture=NULL;
    m->alpha_mode=!g->textured && g->alpha_mode==RENDER_ALPHA_TEXTURE?1:(uint32_t)g->alpha_mode;
    memcpy(m->constant,g->constant,sizeof(m->constant));memcpy(m->fog_color,g->fog_color,sizeof(m->fog_color));
    m->chroma=g->chroma_enabled && g->textured?(0x1000000u|g->chroma):0;m->alpha_function=m->chroma?4:g->alpha_function;
    m->alpha_reference=m->chroma?0:g->alpha_reference;m->fog_mode=g->native_fog;
    m->fog_range[0]=g->native_fog_start;m->fog_range[1]=g->native_fog_end;
    if(m->fog_mode==2)m->fog_range[0]=g->density;
    d->texture_sampler=(g->min_filter==GL_LINEAR?1:0)|(g->wrap_s==GL_REPEAT?2:0)|(g->wrap_t==GL_REPEAT?4:0);
    m->texture_sampling=(uint32_t)d->texture_sampler;
    return (g->depth_mode?VULKAN_DRAW_DEPTH_TEST:0)|(g->native_depth_write?VULKAN_DRAW_DEPTH_WRITE:0)|
        (g->native_blend?VULKAN_DRAW_BLEND:0)|(g->native_depth_func<<3);
}
static void vk1_triangle(const GameRenderVertex *a,const GameRenderVertex *b,const GameRenderVertex *c){
    Gl1Device *g=gl1_current;VulkanDraw *d=vk1_back();VulkanDrawVertex vertex[3];float area;
    if(!vk1_ready_draw())return;
    area=(b->x-a->x)*(c->y-a->y)-(b->y-a->y)*(c->x-a->x);
    if((g->native_cull==1 && area>=0) || (g->native_cull==2 && area<=0))return;
    vertex[0]=vk1_vertex(a);vertex[1]=vk1_vertex(b);vertex[2]=vk1_vertex(c);
    vk1_motion_triangle(vertex);
    if(!vulkan_draw_triangles(d,vertex,3,g->textured && g->bound?(g->bound->ui_tile?&g->vulkan->tiles:g->bound->vulkan_texture):NULL,vk1_material(d)))gl1_fail("Vulkan triangle batch");
    ++g->triangles;
}
static void vk1_point(const GameRenderVertex *v){
    Gl1Device *g=gl1_current;VulkanDraw *d=vk1_back();VulkanDrawVertex vertex;
    if(!vk1_ready_draw())return;
    vertex=vk1_vertex(v);
    if(!vulkan_draw_points(d,&vertex,1,g->textured && g->bound?
        (g->bound->ui_tile?&g->vulkan->tiles:g->bound->vulkan_texture):NULL,vk1_material(d)))gl1_fail("Vulkan point batch");
}
static void vk1_line(const GameRenderVertex *a,const GameRenderVertex *b){
    Gl1Device *g=gl1_current;VulkanDraw *d=vk1_back();VulkanDrawVertex vertex[2];
    if(!vk1_ready_draw())return;
    vertex[0]=vk1_vertex(a);vertex[1]=vk1_vertex(b);
    if(!vulkan_draw_lines(d,vertex,2,g->textured && g->bound?
        (g->bound->ui_tile?&g->vulkan->tiles:g->bound->vulkan_texture):NULL,vk1_material(d)))gl1_fail("Vulkan line batch");
}
static void vk1_clear(uint32_t color,uint32_t alpha,uint32_t depth){
    Gl1Device *g=gl1_current;VulkanDraw *d;float clear[4]={((color>>16)&255)/255.0f,((color>>8)&255)/255.0f,(color&255)/255.0f,(alpha&255)/255.0f};
    if(g->vulkan->composed[g->back] && !vk1_flush(vk1_back(),0))return;
    g->vulkan->composed[g->back]=0;d=vk1_back();
    d->recording=0;d->clear_depth=(depth&65535)/65535.0f;if(!vulkan_draw_begin(d,clear))gl1_fail("Vulkan clear");
    gl1_current->vulkan->frame_start=SDL_GetTicksNS();
}
static void vk1_swap(void){
    Gl1Device *g=gl1_current;unsigned old;Uint64 before=SDL_GetTicksNS(),submitted;
    if(!vk1_flush(vk1_back(),0))return;submitted=SDL_GetTicksNS();
    if(!vulkan_menu_present_image(g->vulkan->menu,vk1_back())){gl1_fail("Vulkan level presentation");return;}
    old=g->front;g->front=g->back;g->back=old;
    /* The next frame prepares its legacy depth/viewport mask before the scene
     * scope opens. Route that work back to the low-resolution owner now;
     * retaining the previous output selection leaves stale occlusion depth. */
    g->vulkan->composed[g->back]=0;
    ++g->vulkan->frames;
    if((g->vulkan->frames<=4 || !(g->vulkan->frames&63u)) && SDL_getenv("UPRISING_RECOMP_NATIVE_OBSERVE"))
        fprintf(stderr,"native Vulkan counters: frames=%llu submits=%llu uploads=%llu atlas_uploads=%llu tiles=%u cpu_ms=%.2f submit_ms=%.2f present_ms=%.2f\n",
            (unsigned long long)g->vulkan->frames,(unsigned long long)g->vulkan->submissions,(unsigned long long)g->vulkan->uploads,(unsigned long long)g->vulkan->atlas_uploads,g->vulkan->tile_count,
            (before-g->vulkan->frame_start)/1000000.0,(submitted-before)/1000000.0,(SDL_GetTicksNS()-submitted)/1000000.0);
    g->vulkan->frame_start=SDL_GetTicksNS();
}
static int vk1_map(int write,unsigned buffer,RenderPixelView *info){
    Gl1Device *g=gl1_current;unsigned i;VulkanDraw *d;
    if(!gl1_ready() || !info || g->locked || buffer>1)return 0;
    d=g->vulkan->frame+(buffer?g->back:g->front);
    g->vulkan->track_text=0;
    if(write){if(!g->vulkan->scratch_clean)for(i=0;i<g->width*g->height;i++)g->lfb[i]=0xf81f;}
    else{
        VulkanDraw *shown=vk1_target(buffer?g->back:g->front);
        if(shown!=d && (!vk1_flush(shown,0) || !vk1_flush(d,0) || !vulkan_draw_blit_color(d,&shown->color,shown->width,shown->height)))return 0;
        if(!vk1_flush(d,0) || !vulkan_draw_readback(d,g->rgba,(size_t)g->width*g->height*4))return 0;
        for(i=0;i<g->width*g->height;i++){const uint8_t *p=g->rgba+i*4;g->lfb[i]=(uint16_t)((p[0]>>3)<<11|(p[1]>>2)<<5|(p[2]>>3));}
    }
    g->vulkan->scratch_clean=0;
    g->locked=1;g->write_lock=write;g->lock_buffer=buffer;info->pixels=g->lfb;info->stride=g->width*2;return 1;
}
static int vk1_quad(VulkanDraw *d,VulkanDrawTexture *texture,float l,float t,float r,float b,float z,float u0,float v0,float u1,float v1,float alpha,uint32_t flags){
    VulkanDrawVertex v[6]={0};float previous[4][4]={{0}};unsigned i;int ok;const unsigned corner[6]={0,1,2,0,2,3};
    Gl1Device *g=gl1_current;Vk1Device *n=g->vulkan;
    if(!d->recording && !vulkan_draw_resume(d))return 0;
    d->capture_depth=0; /* Sky contributes color, never solid world depth. */
    d->capture_background=texture==n->sky && !(flags&VULKAN_DRAW_BLEND);
    if(d->capture_background){
        const float bounds[4]={l,t,r,b},uv[4]={u0,v0,u1,v1};
        if(render_motion_plane(n->sky_history+(d->material.mask_enabled!=0),n->frames+1,
                (uint64_t)g->sky_tick+1,bounds,uv,previous))n->sky_motion_valid++;
    }
    for(i=0;i<6;i++){unsigned c=corner[i];int right=c==1 || c==2,bottom=c>=2;
        v[i].position[0]=right?r:l;v[i].position[1]=bottom?b:t;v[i].position[2]=z;
        v[i].uv[0]=right?u1:u0;v[i].uv[1]=bottom?v1:v0;v[i].uv[2]=1;
        v[i].color[0]=v[i].color[1]=v[i].color[2]=1;v[i].color[3]=alpha;
        memcpy(v[i].previous,previous[c],sizeof(v[i].previous));
    }
    d->material.color_mode=texture?4:1;d->material.alpha_mode=texture?4:3;d->material.fog_mode=0;d->material.chroma=0;
    d->material.alpha_function=4;d->material.alpha_reference=0;
    /* Opaque base sky has texture-space motion at infinite depth. Independently
     * scrolling layers/horizon reject history without replacing its vectors. */
    d->capture_scene=1;ok=vulkan_draw_triangles(d,v,6,texture,flags);
    d->capture_scene=d->capture_background=0;return ok;
}
static int vk1_unmap(void){
    Gl1Device *g=gl1_current;VulkanDraw *d=vk1_target(g->lock_buffer?g->back:g->front);unsigned x,y,i,left=g->width,top=g->height,right=0,bottom=0;
    static const unsigned corner[6]={0,1,2,0,2,3};
    if(!g->locked)return 0;
    if(g->write_lock){
        unsigned first=0,end=g->width*g->height;
        if(g->vulkan->track_text){first=g->vulkan->written_range[0];end=g->vulkan->written_range[1];}
        if(first>=end){g->vulkan->scratch_clean=1;g->locked=0;return 1;}
        for(i=first;i<end;i++)if(g->lfb[i]!=0xf81f){
            x=i%g->width;y=i/g->width;
            if(x<left)left=x;if(x>=right)right=x+1;if(y<top)top=y;if(y>=bottom)bottom=y+1;
        }
        if(left==g->width){g->vulkan->scratch_clean=1;g->locked=0;return 1;}
        if(!d->recording && !vulkan_draw_resume(d))return 0;
        /* Convert opaque RGB565 ink runs directly to native rectangles. This
         * preserves the existing bounded text lock without a texture upload,
         * render-pass flush or GPU fence for every string. Magenta is untouched. */
        d->material.color_mode=0;d->material.alpha_mode=1;d->material.chroma=d->material.fog_mode=d->material.mask_enabled=0;d->material.alpha_function=7;d->mask_texture=NULL;
        d->capture_depth=0;
        for(y=top;y<bottom;y++)for(x=left;x<right;){
            uint16_t color=g->lfb[(size_t)y*g->width+x];unsigned end=x+1;
            if(color==0xf81f){x++;continue;}
            VulkanDrawVertex vertex[6]={0};
            while(end<right && g->lfb[(size_t)y*g->width+end]==color)end++;
            for(i=0;i<6;i++){
                unsigned c=corner[i];vertex[i].position[0]=(float)(c==1 || c==2?end:x);vertex[i].position[1]=(float)(c>=2?y+1:y);
                vertex[i].color[0]=(float)(((color>>11)&31)*255/31)/255;
                vertex[i].color[1]=(float)(((color>>5)&63)*255/63)/255;vertex[i].color[2]=(float)((color&31)*255/31)/255;
                vertex[i].color[3]=1;vertex[i].uv[2]=1;
            }
            if(!vulkan_draw_triangles(d,vertex,6,NULL,0))return 0;x=end;
        }
        /* Retire exactly the written scratch span after its vertices were copied.
         * Generic pixel locks still scan the complete buffer; glyph locks report
         * their validated writes, including multiple strings and clipped text. */
        for(i=first;i<end;i++)g->lfb[i]=0xf81f;
        g->vulkan->scratch_clean=1;
    }
    g->locked=0;return 1;
}
static int vk1_sky_upload(unsigned layer,unsigned width,unsigned height){
    Gl1Device *g=gl1_current;
    if(!vk1_flush(vk1_back(),0) || !vulkan_draw_texture_create(g->vulkan->frame,g->vulkan->sky+layer,g->rgba,width,height,layer!=2,1))return 0;
    g->sky_layers[layer]=1;return 1;
}
static int vk1_sky_end(void){
    Gl1Device *g=gl1_current;VulkanDraw *d=vk1_back();unsigned layer;float bottom,top,repeats,u;
    d->material.mask_enabled=0;d->mask_texture=NULL;
    if(g->sky_triangles)for(layer=0;layer<2;layer++)if(g->sky_layers[layer]){
        float u0=-g->sky_yaw*g->sky_yaw_scale+(layer?g->sky_offset:0),v0=-g->sky_horizon/256+0.5f+(layer?g->sky_offset*0.6f:0);
        d->texture_sampler=7;
        if(!vk1_quad(d,g->vulkan->sky+layer,0,0,(float)g->width,(float)g->height,(1-g->sky_depth)*0.5f,u0,v0,u0+g->width/256.0f,v0+g->height/256.0f,layer?0.5f:1,(g->native_depth_func<<3)|VULKAN_DRAW_DEPTH_TEST|(layer?VULKAN_DRAW_BLEND:0)))return 0;
    }
    if(g->sky_triangles && g->fog_pending){
        int row=(int)floorf(fminf((float)g->height-3,fmaxf(1,g->sky_horizon-142)));unsigned x,y;uint64_t sum[3]={0},count=0;
        if(!vk1_flush(d,0) || !vulkan_draw_readback(d,g->rgba,(size_t)g->width*g->height*4))return 0;
        for(y=0;y<3;y++)for(x=0;x<g->width;x++){
            const uint8_t *p=g->rgba+((size_t)(row+(int)y)*g->width+x)*4;
            if((p[0]==0x83 && p[1]==0xb9 && p[2]==0xbe) || (p[0]<80 && p[1]>220 && p[2]<80))continue;
            sum[0]+=p[0];sum[1]+=p[1];sum[2]+=p[2];count++;
        }
        if(count>=g->width){for(x=0;x<3;x++)g->fog_color[x]=(float)sum[x]/(count*255.0f);g->fog_pending=0;}
    }
    if(g->sky_triangles && g->sky_layers[2]){
        bottom=g->sky_horizon+19;top=bottom-128;repeats=2*atanf(g->projection_center/g->projection_scale)/6.28318530718f*16;u=-g->sky_yaw*16;
        d->texture_sampler=2;
        if(!vk1_quad(d,g->vulkan->sky+2,0,top,(float)g->width,bottom,(1-g->sky_depth)*0.5f,u-repeats*0.5f,1,u+repeats*0.5f,0,1,(g->native_depth_func<<3)|VULKAN_DRAW_DEPTH_TEST|VULKAN_DRAW_BLEND))return 0;
    }
    g->sky_active=0;return !g->failed;
}
static int vk1_remote_read(const int32_t bounds[4]){
    Gl1Device *g=gl1_current;unsigned width=(unsigned)(bounds[2]-bounds[0]),height=(unsigned)(bounds[3]-bounds[1]),x,y;
    uint8_t *pixels=malloc((size_t)width*height*3);if(!pixels)return 0;
    if(!vk1_flush(vk1_back(),0) || !vulkan_draw_readback(vk1_back(),g->rgba,(size_t)g->width*g->height*4)){free(pixels);return 0;}
    for(y=0;y<height;y++)for(x=0;x<width;x++)memcpy(pixels+((size_t)y*width+x)*3,g->rgba+((size_t)(bounds[3]-1-(int)y)*g->width+bounds[0]+x)*4,3);
    memcpy(g->rgba,pixels,(size_t)width*height*3);free(pixels);return 1;
}
static int vk1_remote_upload(unsigned width,unsigned height){
    Gl1Device *g=gl1_current;VulkanDrawTexture *t=&g->vulkan->remote_mask;
    if(!vk1_flush(vk1_back(),0))return 0;
    vulkan_draw_texture_destroy(g->vulkan->frame,t);
    if(!vulkan_draw_texture_create(g->vulkan->frame,t,g->rgba,width,height,0,0))return 0;
    g->remote_mask=1;return 1;
}
static int vk1_remote_sky(const GameRemoteSkyFrame *frame){
    Gl1Device *g=gl1_current;VulkanDraw *d=vk1_back();int32_t bounds[4];float width,height,scale,fov,u0,u1,offset=0,bottom;unsigned i;
    memcpy(bounds,frame->bounds,sizeof(bounds));if(bounds[0]<0)bounds[0]=0;if(bounds[1]<0)bounds[1]=0;
    if(bounds[2]>(int)g->width)bounds[2]=(int)g->width;if(bounds[3]>(int)g->height)bounds[3]=(int)g->height;
    if(bounds[2]<=bounds[0] || bounds[3]<=bounds[1])return 1;
    gl1_sky_select(frame->name);if(!g->sky_loaded || !gl1_remote_mask(bounds))return !g->failed;
    width=(float)(bounds[2]-bounds[0]);height=(float)(bounds[3]-bounds[1]);
    scale=isfinite(frame->scale) && frame->scale>1 && frame->scale<=100000?frame->scale:width*0.596f;
    fov=2*atanf(width*0.5f/scale);if(!(fov>0.01f))fov=6.28318530718f*0.22f;
    u0=-frame->yaw*(width/256)*6.28318530718f/fov;u1=u0+width/256;
    if(isfinite(frame->speed) && fabsf(frame->speed)<=1000)offset=(float)(uint32_t)((uint32_t)SDL_GetTicks()-g->sky_tick)*frame->speed/100000;
    g->projection_center=(bounds[0]+bounds[2])*0.5f;g->projection_scale=scale;
    d->mask_texture=&g->vulkan->remote_mask;d->material.mask_enabled=1;for(i=0;i<4;i++)d->material.mask_bounds[i]=(float)bounds[i];d->texture_sampler=7;
    if(!vk1_quad(d,g->vulkan->sky,bounds[0],bounds[1],bounds[2],bounds[3],0,u0,0,u1,height/256,1,0))return 0;
    if(g->sky_layers[1] && !vk1_quad(d,g->vulkan->sky+1,bounds[0],bounds[1],bounds[2],bounds[3],0,u0+offset,offset*0.6f,u1+offset,height/256+offset*0.6f,0.5f,VULKAN_DRAW_BLEND))return 0;
    if(g->sky_layers[2]){
        bottom=bounds[1]+height*0.58f;memcpy(d->material.constant,g->fog_color,sizeof(g->fog_color));
        if(!vk1_quad(d,NULL,bounds[0],bottom,bounds[2],bounds[3],0,0,0,1,1,1,0))return 0;
        d->texture_sampler=2;
        if(!vk1_quad(d,g->vulkan->sky+2,bounds[0],bottom-height*0.42f,bounds[2],bottom,0,-frame->yaw*16,1,-frame->yaw*16+4,0,1,VULKAN_DRAW_BLEND))return 0;
    }
    d->material.mask_enabled=0;d->mask_texture=NULL;return !g->failed;
}
