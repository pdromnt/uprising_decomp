#include "vulkan_device.h"
#include "vulkan_draw.h"
#include "vulkan_present.h"
#include "fsr3_reactive_spv.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

static int draw_quad(VulkanDraw *d,float left,float top,float right,float bottom,float depth,
        float r,float g,float b,float a,VulkanDrawTexture *texture,uint32_t flags){
    VulkanDrawVertex quad[4]={0},triangles[6];unsigned i;const unsigned index[6]={0,1,2,0,2,3};
    for(i=0;i<4;i++){
        int on_right=i==1 || i==2;float q=texture && on_right?2.0f:1.0f;
        quad[i].position[0]=on_right?right:left;quad[i].position[1]=i>=2?bottom:top;quad[i].position[2]=depth;
        quad[i].color[0]=r;quad[i].color[1]=g;quad[i].color[2]=b;quad[i].color[3]=a;
        quad[i].uv[0]=on_right?q:0;quad[i].uv[1]=0.5f*q;quad[i].uv[2]=q;
    }
    for(i=0;i<6;i++)triangles[i]=quad[index[i]];
    return vulkan_draw_triangles(d,triangles,6,texture,flags|VULKAN_DRAW_LEQUAL);
}
static int pixel(const uint8_t *rgba,unsigned x,unsigned y,unsigned r,unsigned g,unsigned b){
    const uint8_t *p=rgba+(y*64+x)*4;
    if(abs((int)p[0]-(int)r)>1 || abs((int)p[1]-(int)g)>1 || abs((int)p[2]-(int)b)>1){
        fprintf(stderr,"Vulkan draw pixel %u,%u: %u,%u,%u expected %u,%u,%u\n",x,y,p[0],p[1],p[2],r,g,b);return 0;
    }return 1;
}
static int graphics_pass(VulkanDevice *v,SDL_Window *window){
    VulkanDraw draw={0};VulkanDrawTexture texture={0};VulkanPresent present={0};uint8_t rgba[64*64*4];unsigned frame;int ok=0;
    const uint8_t texels[8]={255,255,255,255,255,255,255,0};const float clear[4]={0,0,0,1};
    if(!vulkan_draw_open(&draw,v,64,64) || !vulkan_draw_texture_create(&draw,&texture,texels,2,1,0,0) || !vulkan_present_open(&present,v,window))goto done;
    for(frame=0;frame<3;frame++){
        if(!vulkan_draw_begin(&draw,clear) ||
           !draw_quad(&draw,0,0,64,64,0.6f,1,0,0,1,NULL,3) ||
           !draw_quad(&draw,8,8,32,32,0.3f,0,1,0,1,NULL,3) ||
           !draw_quad(&draw,0,0,64,64,0.8f,0,0,1,1,NULL,3) ||
           !vulkan_draw_finish(&draw,rgba,sizeof(rgba)) || !vulkan_draw_resume(&draw) ||
           !draw_quad(&draw,0,0,64,64,0.8f,1,1,1,1,NULL,3) ||
           !draw_quad(&draw,32,0,64,32,0.2f,0,1,1,1,&texture,3) ||
           !draw_quad(&draw,0,32,32,64,0.9f,0,0,1,0.5f,NULL,VULKAN_DRAW_BLEND) ||
           !draw_quad(&draw,48,48,64,64,0.9f,1,1,0,1,NULL,0) ||
           !vulkan_draw_finish(&draw,rgba,sizeof(rgba)))goto done;
        if(!pixel(rgba,2,2,255,0,0) || !pixel(rgba,16,16,0,255,0) ||
           !pixel(rgba,36,16,0,255,255) || !pixel(rgba,44,16,255,0,0) ||
           !pixel(rgba,16,48,128,0,128) || !pixel(rgba,56,56,255,255,0))goto done;
        if(frame==1)present.recreate=1;
        if(!vulkan_present_image(&present,draw.color.image,64,64))goto done;
    }
    printf("VULKAN_GRAPHICS_PASS frames=3 depth=checked alpha=checked projective_uv=checked hud_order=checked resume=checked\n");ok=1;
    printf("VULKAN_PRESENT_PASS frames=3 swapchain_rebuild=checked aspect=preserved\n");
    {
        const uint8_t keyed[8]={255,0,255,255,0,255,0,255};const float black[4]={0,0,0,1};
        if(!vulkan_draw_texture_update(&draw,&texture,keyed,sizeof(keyed)) || !vulkan_draw_begin(&draw,black)){ok=0;goto done;}
        draw.material.color_mode=2;draw.material.alpha_mode=2;draw.material.chroma=0x1ff00ff;
        draw.material.texture_sampling=1;draw.texture_sampler=1;
        if(!draw_quad(&draw,0,0,64,64,0,1,1,1,1,&texture,VULKAN_DRAW_BLEND) || !vulkan_draw_finish(&draw,rgba,sizeof(rgba)) ||
           !pixel(rgba,2,16,0,0,0) || !pixel(rgba,60,16,0,255,0)){ok=0;goto done;}
        /* At x=21.5, q-weighted UV is 86/171. Key alpha is filtered before
         * blending, preserving the comparator's chroma fringe semantics. */
        if(!pixel(rgba,21,16,64,65,64) || !vulkan_draw_resume(&draw)){ok=0;goto done;}
        draw.material.chroma=0;draw.material.color_mode=1;draw.material.alpha_mode=3;
        draw.material.constant[0]=draw.material.constant[1]=0;draw.material.constant[2]=draw.material.constant[3]=1;
        if(!draw_quad(&draw,0,0,16,16,0,1,1,1,1,NULL,0)){ok=0;goto done;}
        draw.material.constant[0]=1;draw.material.constant[2]=0;
        if(!draw_quad(&draw,16,0,32,16,0,1,1,1,1,NULL,0) || !vulkan_draw_finish(&draw,rgba,sizeof(rgba)) ||
           !pixel(rgba,8,8,0,0,255) || !pixel(rgba,24,8,255,0,0)){ok=0;goto done;}
        printf("VULKAN_MATERIAL_PASS keyed_filter=checked batch_constants=checked\n");
    }
    {
        VulkanDrawVertex points[2]={0},lines[4]={0};unsigned i;
        const float black[4]={0,0,0,1};
        if(!vulkan_draw_begin(&draw,black)){ok=0;goto done;}
        draw.material.color_mode=0;draw.material.alpha_mode=1;draw.material.alpha_function=7;
        draw.material.chroma=draw.material.fog_mode=0;
        for(i=0;i<2;i++){
            points[i].position[0]=8.2f+i*4.6f;points[i].position[1]=8.2f+i*0.6f;points[i].position[2]=0.25f;
            points[i].color[i]=points[i].color[3]=1;points[i].uv[2]=1;
        }
        if(!vulkan_draw_points(&draw,points,2,NULL,3|VULKAN_DRAW_LEQUAL)){ok=0;goto done;}
        /* A later farther point must retain the near red point's depth. */
        points[0].position[2]=0.75f;points[0].color[0]=0;points[0].color[2]=1;
        if(!vulkan_draw_points(&draw,points,1,NULL,3|VULKAN_DRAW_LEQUAL)){ok=0;goto done;}
        for(i=0;i<4;i++){
            lines[i].position[0]=i<2?(i?24.5f:4.5f):32.5f;
            lines[i].position[1]=i<2?24.5f:(i==2?4.5f:24.5f);
            lines[i].color[1]=lines[i].color[3]=1;lines[i].color[2]=i>=2?1:0;lines[i].uv[2]=1;
        }
        if(!vulkan_draw_lines(&draw,lines,4,NULL,0) ||
           vulkan_draw_lines(&draw,lines,3,NULL,0) || vulkan_draw_triangles(&draw,points,2,NULL,0) ||
           !vulkan_draw_finish(&draw,rgba,sizeof(rgba)) ||
           !pixel(rgba,8,8,255,0,0) || !pixel(rgba,12,8,0,255,0) ||
           !pixel(rgba,7,8,0,0,0) || !pixel(rgba,8,9,0,0,0) ||
           !pixel(rgba,14,24,0,255,0) || !pixel(rgba,32,14,0,255,255) ||
           !pixel(rgba,14,23,0,0,0) || !pixel(rgba,31,14,0,0,0)){ok=0;goto done;}
        printf("VULKAN_PRIMITIVES_PASS native_points=checked native_lines=checked mixed_batches=checked depth=checked\n");
    }
    {
        VulkanDrawVertex triangle[3]={0};float depths[64*64];unsigned i;
        const float black[4]={0,0,0,1};
        draw.capture_depth=1;
        draw.material.color_mode=0;draw.material.alpha_mode=1;draw.material.alpha_function=7;
        if(!vulkan_draw_begin(&draw,black)){ok=0;goto done;}
        for(i=0;i<3;i++){
            triangle[i].position[0]=i==1?64:0;triangle[i].position[1]=i==2?64:0;
            triangle[i].position[2]=0.4f;triangle[i].color[0]=triangle[i].color[3]=1;
            triangle[i].uv[2]=i==1?0.375f:0.125f;
        }
        if(!vulkan_draw_triangles(&draw,triangle,3,NULL,3|VULKAN_DRAW_LEQUAL)){ok=0;goto done;}
        for(i=0;i<3;i++){triangle[i].position[2]=0.8f;triangle[i].uv[2]=0.5f;}
        if(!vulkan_draw_triangles(&draw,triangle,3,NULL,3|VULKAN_DRAW_LEQUAL) ||
           !vulkan_draw_finish(&draw,NULL,0) || !vulkan_draw_resume(&draw)){ok=0;goto done;}
        /* Opaque screen artwork may replace color and hardware depth while
         * retaining the world input, including across a readback/resume. */
        draw.capture_depth=0;
        if(!draw_quad(&draw,0,0,32,32,0.1f,0,0,1,1,NULL,3) ||
           !vulkan_draw_finish(&draw,rgba,sizeof(rgba)) || !pixel(rgba,8,8,0,0,255) ||
           !vulkan_draw_read_reciprocal_depth(&draw,depths,sizeof(depths)) ||
           fabsf(depths[8+8*64]-0.158203125f)>0.0000001f || depths[48+48*64]!=0){
            fprintf(stderr,"Vulkan reciprocal depth: interpolation/occlusion/UI preservation failed\n");ok=0;goto done;
        }
        /* Alpha-discarded texels must leave far depth, not invisible occluders. */
        if(!vulkan_draw_texture_update(&draw,&texture,texels,sizeof(texels)) || !vulkan_draw_begin(&draw,black)){ok=0;goto done;}
        draw.capture_depth=1;draw.texture_sampler=0;
        draw.material.color_mode=4;draw.material.alpha_mode=4;draw.material.alpha_function=4;
        if(!draw_quad(&draw,0,0,64,64,0.2f,1,1,1,1,&texture,3) ||
           !vulkan_draw_finish(&draw,NULL,0) || !vulkan_draw_read_reciprocal_depth(&draw,depths,sizeof(depths)) ||
           depths[4+16*64]!=1 || depths[60+16*64]!=0){
            fprintf(stderr,"Vulkan reciprocal depth: alpha rejection/clear failed\n");ok=0;goto done;
        }
        draw.capture_depth=0;
        if(!vulkan_draw_resume(&draw) || !vulkan_draw_reset_reciprocal_depth(&draw) ||
           !vulkan_draw_finish(&draw,rgba,sizeof(rgba)) || !pixel(rgba,4,16,255,255,255) ||
           !vulkan_draw_read_reciprocal_depth(&draw,depths,sizeof(depths))){ok=0;goto done;}
        for(i=0;i<64*64;i++)if(depths[i]!=0){fprintf(stderr,"Vulkan reciprocal depth: stale resumed world pixel\n");ok=0;goto done;}
        printf("VULKAN_WORLD_DEPTH_PASS interpolation=checked occlusion=checked hud_preserved=checked alpha_cutout=checked reset=checked\n");
    }
    {
        VulkanDrawVertex triangle[3]={0};float motion[64*64*4];unsigned i;
        const float black[4]={0,0,0,1};
        if(!vulkan_draw_begin(&draw,black)){ok=0;goto done;}
        draw.capture_depth=1;draw.material.color_mode=0;draw.material.alpha_mode=1;draw.material.alpha_function=7;
        for(i=0;i<3;i++){
            triangle[i].position[0]=i==1?64:0;triangle[i].position[1]=i==2?64:0;
            triangle[i].position[2]=0.4f;triangle[i].uv[2]=1;triangle[i].color[0]=triangle[i].color[3]=1;
            triangle[i].previous[0]=triangle[i].position[0]-4;triangle[i].previous[1]=triangle[i].position[1]+2;
            triangle[i].previous[2]=triangle[i].previous[3]=1;
        }
        if(!vulkan_draw_triangles(&draw,triangle,3,NULL,3|VULKAN_DRAW_LEQUAL) || !vulkan_draw_finish(&draw,NULL,0) ||
           !vulkan_draw_resume(&draw)){ok=0;goto done;}
        draw.capture_depth=0;
        if(!draw_quad(&draw,0,0,32,32,0.1f,0,0,1,1,NULL,0) || !vulkan_draw_finish(&draw,NULL,0) ||
           !vulkan_draw_read_motion(&draw,motion,sizeof(motion))){ok=0;goto done;}
        {const float *m=motion+(8+8*64)*4;
        if(fabsf(m[0]+4)>0.00001f || fabsf(m[1]-2)>0.00001f || m[2]!=1 || m[3]!=0){
            fprintf(stderr,"Vulkan motion: translation/UI preservation failed\n");ok=0;goto done;}}
        /* Surviving translucent fragments reject history, preserving solid
         * vectors/validity. Occluded fragments and later UI must not alter it. */
        draw.capture_depth=1;
        if(!vulkan_draw_resume(&draw) ||
           !draw_quad(&draw,0,0,24,24,0.1f,1,0,0,0.5f,NULL,VULKAN_DRAW_DEPTH_TEST|VULKAN_DRAW_BLEND) ||
           !draw_quad(&draw,32,0,48,16,0.9f,1,0,0,0.5f,NULL,VULKAN_DRAW_DEPTH_TEST|VULKAN_DRAW_BLEND)){ok=0;goto done;}
        draw.capture_depth=0;
        if(!draw_quad(&draw,0,0,16,16,0,1,1,1,1,NULL,0) || !vulkan_draw_finish(&draw,NULL,0) ||
           !vulkan_draw_read_motion(&draw,motion,sizeof(motion))){ok=0;goto done;}
        {const float *m=motion+(8+8*64)*4,*occluded=motion+(40+8*64)*4;
        if(fabsf(m[0]+4)>0.00001f || fabsf(m[1]-2)>0.00001f || m[2]!=1 || m[3]!=1 || occluded[2]!=1 || occluded[3]!=0){
            fprintf(stderr,"Vulkan reactive coverage: occlusion/vector preservation failed\n");ok=0;goto done;}}
        printf("VULKAN_REACTIVE_COVERAGE_PASS translucent=checked occlusion=checked motion_preserved=checked ui_excluded=checked\n");
        /* Different previous w across the triangle must use projective division,
         * not interpolation of already divided previous pixel positions. */
        triangle[1].previous[0]*=2;triangle[1].previous[1]*=2;triangle[1].previous[2]=2;
        draw.capture_depth=1;
        if(!vulkan_draw_begin(&draw,black) || !vulkan_draw_triangles(&draw,triangle,3,NULL,3|VULKAN_DRAW_LEQUAL) ||
           !vulkan_draw_finish(&draw,NULL,0) || !vulkan_draw_read_motion(&draw,motion,sizeof(motion))){ok=0;goto done;}
        {const float *m=motion+(8+8*64)*4;
        if(fabsf(m[0]-(12.46875f/1.1328125f-8.5f))>0.00001f ||
           fabsf(m[1]-(10.765625f/1.1328125f-8.5f))>0.00001f || m[2]!=1){ok=0;goto done;}}
        if(!vulkan_draw_resume(&draw) || !vulkan_draw_reset_reciprocal_depth(&draw) ||
           !vulkan_draw_finish(&draw,NULL,0) || !vulkan_draw_read_motion(&draw,motion,sizeof(motion))){ok=0;goto done;}
        if(motion[(8+8*64)*4+2]!=0 || motion[(8+8*64)*4+3]!=1){ok=0;goto done;}
        for(i=0;i<3;i++)memset(triangle[i].previous,0,sizeof(triangle[i].previous));
        if(!vulkan_draw_begin(&draw,black) || !vulkan_draw_triangles(&draw,triangle,3,NULL,3|VULKAN_DRAW_LEQUAL) ||
           !vulkan_draw_finish(&draw,NULL,0) || !vulkan_draw_read_motion(&draw,motion,sizeof(motion))){ok=0;goto done;}
        {const float *m=motion+(8+8*64)*4;
        if(m[0]!=0 || m[1]!=0 || m[2]!=0 || m[3]!=1){ok=0;goto done;}}
        draw.capture_depth=0;
        printf("VULKAN_MOTION_PASS translation=checked projective_previous=checked hud_preserved=checked invalidity=checked\n");
    }
    {
        VulkanDrawVertex triangle[3]={0},point={0};float depths[64*64],motion[64*64*4];unsigned i,frame;
        const float black[4]={0,0,0,1};
        if(vulkan_draw_projection(&draw,0,1,0,0) || vulkan_draw_projection(&draw,10,0,0,0) ||
           vulkan_draw_projection(&draw,10,1,NAN,0) || vulkan_draw_projection(&draw,10,1,0.6f,0)){ok=0;goto done;}
        draw.material.color_mode=0;draw.material.alpha_mode=1;draw.material.alpha_function=7;
        for(i=0;i<3;i++){
            triangle[i].position[0]=i==1?64:0;triangle[i].position[1]=i==2?64:0;
            triangle[i].position[2]=0.4f;triangle[i].uv[2]=i==1?0.15f:0.05f;
            triangle[i].color[0]=triangle[i].color[3]=1;
            triangle[i].previous[0]=triangle[i].position[0]-4;triangle[i].previous[1]=triangle[i].position[1]+2;
            triangle[i].previous[2]=triangle[i].previous[3]=1;
        }
        for(frame=0;frame<2;frame++){
            float jx=frame?-0.25f:0.375f,jy=frame?0.375f:-0.25f;
            float expected=(0.05f+0.1f*(8.5f-jx)/64.0f)*5.0f;const float *m;
            if(!vulkan_draw_projection(&draw,10,2,jx,jy) || !vulkan_draw_begin(&draw,black)){ok=0;goto done;}
            draw.capture_depth=1;
            if(!vulkan_draw_triangles(&draw,triangle,3,NULL,3|VULKAN_DRAW_LEQUAL)){ok=0;goto done;}
            /* Change projection after queueing: batches must own their values. */
            if(!vulkan_draw_projection(&draw,1,1,0,0)){ok=0;goto done;}
            draw.capture_depth=0;
            if(!draw_quad(&draw,0,0,16,16,0,0,0,1,1,NULL,0) || !vulkan_draw_finish(&draw,rgba,sizeof(rgba)) ||
               !vulkan_draw_read_reciprocal_depth(&draw,depths,sizeof(depths)) ||
               !vulkan_draw_read_motion(&draw,motion,sizeof(motion))){ok=0;goto done;}
            m=motion+(8+8*64)*4;
            if(!pixel(rgba,8,8,0,0,255) || fabsf(depths[8+8*64]-expected)>0.000001f ||
               fabsf(m[0]+4)>0.00001f || fabsf(m[1]-2)>0.00001f || m[2]!=1 || m[3]!=0){
                fprintf(stderr,"Vulkan projection: jitter/depth/motion snapshot failed frame=%u\n",frame);ok=0;goto done;}
        }
        /* Pixel coverage moves for world geometry, never for screen artwork. */
        if(!vulkan_draw_projection(&draw,10,1,0.375f,0) || !vulkan_draw_begin(&draw,black)){ok=0;goto done;}
        point.position[0]=8.75f;point.position[1]=8.5f;point.color[0]=point.color[3]=1;point.uv[2]=0.1f;
        draw.capture_depth=1;
        if(!vulkan_draw_points(&draw,&point,1,NULL,3|VULKAN_DRAW_LEQUAL)){ok=0;goto done;}
        point.color[0]=0;point.color[1]=1;draw.capture_depth=0;
        if(!vulkan_draw_points(&draw,&point,1,NULL,0) || !vulkan_draw_finish(&draw,rgba,sizeof(rgba)) ||
           !pixel(rgba,8,8,0,255,0) || !pixel(rgba,9,8,255,0,0)){ok=0;goto done;}
        printf("VULKAN_PROJECTION_PASS reversed_infinite_depth=checked jitter_coverage=checked unjittered_vectors=checked hud=checked snapshots=checked\n");
    }
    {
        uint8_t scene[64*64*4];float depths[64*64],motion[64*64*4];unsigned i;
        const float black[4]={0,0,0,1};VulkanDrawVertex point={0};
        if(!vulkan_draw_projection(&draw,1,1,0.375f,-0.25f) || !vulkan_draw_begin(&draw,black)){ok=0;goto done;}
        draw.capture_depth=draw.capture_scene=0;
        draw.material.color_mode=0;draw.material.alpha_mode=1;draw.material.alpha_function=7;
        if(!draw_quad(&draw,0,0,64,64,0,0,1,0,1,NULL,0)){ok=0;goto done;}
        /* Masked sky color is captured independently of artwork, with jitter,
         * while sky depth/motion remain at their cleared background values. */
        draw.capture_scene=1;draw.mask_texture=&texture;draw.material.mask_enabled=1;
        draw.material.mask_bounds[0]=draw.material.mask_bounds[1]=0;
        draw.material.mask_bounds[2]=draw.material.mask_bounds[3]=64;
        if(!draw_quad(&draw,0,0,64,64,0.8f,0,0,1,1,NULL,0)){ok=0;goto done;}
        if(vulkan_draw_reset_reciprocal_depth(&draw)){fprintf(stderr,"Vulkan scene: accepted reset after sky submission\n");ok=0;goto done;}
        draw.capture_scene=0;draw.mask_texture=NULL;draw.material.mask_enabled=0;
        draw.capture_depth=1;
        if(!draw_quad(&draw,8,8,24,24,0.4f,0,0,1,1,NULL,3) ||
           !draw_quad(&draw,8,8,24,24,0.2f,1,0,0,0.5f,NULL,VULKAN_DRAW_DEPTH_TEST|VULKAN_DRAW_BLEND) ||
           !draw_quad(&draw,8,8,24,24,0.9f,0,1,0,1,NULL,3) ||
           !vulkan_draw_finish(&draw,NULL,0) || !vulkan_draw_resume(&draw)){ok=0;goto done;}
        draw.capture_depth=0;
        if(!draw_quad(&draw,0,0,32,32,0,1,1,1,1,NULL,0) || !vulkan_draw_finish(&draw,rgba,sizeof(rgba)) ||
           !vulkan_draw_read_scene(&draw,scene,sizeof(scene)) ||
           !vulkan_draw_read_reciprocal_depth(&draw,depths,sizeof(depths)) ||
           !vulkan_draw_read_motion(&draw,motion,sizeof(motion)) ||
           !pixel(rgba,8,8,255,255,255) || !pixel(scene,8,8,128,0,128) ||
           !pixel(scene,2,2,0,0,255) || !pixel(scene,48,48,0,0,0) ||
           depths[2+2*64]!=0 || motion[(2+2*64)*4+2]!=0 || motion[(2+2*64)*4+3]!=1){ok=0;goto done;}
        if(!vulkan_draw_resume(&draw) || !vulkan_draw_reset_reciprocal_depth(&draw) ||
           !vulkan_draw_finish(&draw,rgba,sizeof(rgba)) || !pixel(rgba,8,8,255,255,255) ||
           !vulkan_draw_read_scene(&draw,scene,sizeof(scene))){ok=0;goto done;}
        for(i=0;i<sizeof(scene);i++)if(scene[i]){fprintf(stderr,"Vulkan scene: stale resumed color\n");ok=0;goto done;}
        if(!vulkan_draw_begin(&draw,black)){ok=0;goto done;}
        point.position[0]=50.75f;point.position[1]=8.75f;point.color[0]=point.color[3]=1;point.uv[2]=1;
        draw.capture_scene=1;
        if(!vulkan_draw_points(&draw,&point,1,NULL,0)){ok=0;goto done;}
        draw.capture_scene=0;point.color[0]=0;point.color[1]=1;
        if(!vulkan_draw_points(&draw,&point,1,NULL,0) || !vulkan_draw_finish(&draw,rgba,sizeof(rgba)) ||
           !vulkan_draw_read_scene(&draw,scene,sizeof(scene)) || !pixel(rgba,50,8,0,255,0) ||
           !pixel(scene,50,8,0,0,0) || !pixel(scene,51,8,255,0,0)){ok=0;goto done;}
        printf("VULKAN_SCENE_COLOR_PASS sky_jitter=checked mask=checked blend=checked occlusion=checked ui_excluded=checked resume_reset=checked\n");
    }
done:
    vulkan_present_close(&present);vulkan_draw_texture_destroy(&draw,&texture);vulkan_draw_close(&draw);return ok;
}

/* Runs AMD's unmodified FSR3 reactive-mask shader. This proves a real SDK
 * compute pass, not temporal upscaling or frame generation. No game assets. */
static int reactive_pass(VulkanDevice *v,float *reactive){
    enum {SIDE=8,PIXELS=SIDE*SIDE};int ok=0;unsigned i;
    VulkanImage image[3]={{0}};VulkanBuffer constants={0},settings={0},readback={0};
    VkDescriptorSetLayout layout=VK_NULL_HANDLE;VkDescriptorPool pool=VK_NULL_HANDLE;VkDescriptorSet set;
    VkPipelineLayout pipeline_layout=VK_NULL_HANDLE;VkPipeline pipeline=VK_NULL_HANDLE;VkShaderModule shader=VK_NULL_HANDLE;
    VkDescriptorSetLayoutBinding bindings[6]={{0}};VkDescriptorSetLayoutCreateInfo l={VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    VkDescriptorPoolSize sizes[3]={{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,2},{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,2},{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,2}};
    VkDescriptorPoolCreateInfo p={VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};VkDescriptorSetAllocateInfo a={VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    VkPipelineLayoutCreateInfo pl={VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};VkShaderModuleCreateInfo sm={VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    VkComputePipelineCreateInfo cp={VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};VkDescriptorImageInfo views[4]={{0}};
    VkDescriptorBufferInfo buffers[2];VkWriteDescriptorSet writes[6]={{0}};VkCommandBuffer command;
    VkImageMemoryBarrier barrier[3]={{0}};VkBufferImageCopy copy={0};VkBufferMemoryBarrier host={VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    float main_constants[64]={0},output[PIXELS];struct {float scale,threshold,binary;uint32_t flags;} parameters={1,0.2f,1,0};
#define TRY(call) do {if(!(call)){fprintf(stderr,"FSR3 probe failed: %s\n",#call);goto done;}}while(0)
#define VK(call) do {VkResult status=(call);if(status!=VK_SUCCESS){fprintf(stderr,"FSR3 %s failed: VkResult=%d\n",#call,(int)status);goto done;}}while(0)
    TRY(vulkan_image_create(v,&image[0],VK_FORMAT_R32G32B32A32_SFLOAT,VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,SIDE,SIDE));
    TRY(vulkan_image_create(v,&image[1],VK_FORMAT_R32G32B32A32_SFLOAT,VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,SIDE,SIDE));
    TRY(vulkan_image_create(v,&image[2],VK_FORMAT_R32_SFLOAT,VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT,SIDE,SIDE));
    TRY(vulkan_buffer_create(v,&constants,sizeof(main_constants),VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT));
    TRY(vulkan_buffer_create(v,&settings,sizeof(parameters),VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT));
    TRY(vulkan_buffer_create(v,&readback,sizeof(output),VK_BUFFER_USAGE_TRANSFER_DST_BIT));
    TRY(vulkan_buffer_write(v,&constants,main_constants,sizeof(main_constants)));
    TRY(vulkan_buffer_write(v,&settings,&parameters,sizeof(parameters)));
    for(i=0;i<6;i++){
        bindings[i].binding=i;bindings[i].descriptorCount=1;bindings[i].stageFlags=VK_SHADER_STAGE_COMPUTE_BIT;
        bindings[i].descriptorType=i<2?VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:i<4?VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    }
    l.bindingCount=6;l.pBindings=bindings;VK(v->CreateDescriptorSetLayout(v->device,&l,NULL,&layout));
    p.maxSets=1;p.poolSizeCount=3;p.pPoolSizes=sizes;VK(v->CreateDescriptorPool(v->device,&p,NULL,&pool));
    a.descriptorPool=pool;a.descriptorSetCount=1;a.pSetLayouts=&layout;VK(v->AllocateDescriptorSets(v->device,&a,&set));
    buffers[0]=(VkDescriptorBufferInfo){constants.buffer,0,sizeof(main_constants)};
    buffers[1]=(VkDescriptorBufferInfo){settings.buffer,0,sizeof(parameters)};
    for(i=0;i<6;i++){
        writes[i].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;writes[i].dstSet=set;writes[i].dstBinding=i;
        writes[i].descriptorCount=1;writes[i].descriptorType=bindings[i].descriptorType;
        if(i<4){views[i].imageView=image[i<2?i:2].view;views[i].imageLayout=i<2?VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:VK_IMAGE_LAYOUT_GENERAL;writes[i].pImageInfo=views+i;}
        else writes[i].pBufferInfo=buffers+i-4;
    }
    v->UpdateDescriptorSets(v->device,6,writes,0,NULL);
    pl.setLayoutCount=1;pl.pSetLayouts=&layout;VK(v->CreatePipelineLayout(v->device,&pl,NULL,&pipeline_layout));
    sm.codeSize=sizeof(fsr3_reactive_spv);sm.pCode=fsr3_reactive_spv;VK(v->CreateShaderModule(v->device,&sm,NULL,&shader));
    cp.stage.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;cp.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT;
    cp.stage.module=shader;cp.stage.pName="main";cp.layout=pipeline_layout;
    VK(v->CreateComputePipelines(v->device,VK_NULL_HANDLE,1,&cp,NULL,&pipeline));
    TRY(vulkan_command_begin(v,&command));
    for(i=0;i<3;i++){
        barrier[i].sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;barrier[i].oldLayout=VK_IMAGE_LAYOUT_UNDEFINED;
        barrier[i].newLayout=i<2?VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:VK_IMAGE_LAYOUT_GENERAL;
        barrier[i].dstAccessMask=i<2?VK_ACCESS_TRANSFER_WRITE_BIT:VK_ACCESS_SHADER_WRITE_BIT;
        barrier[i].srcQueueFamilyIndex=barrier[i].dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
        barrier[i].image=image[i].image;barrier[i].subresourceRange=(VkImageSubresourceRange){VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
    }
    v->CmdPipelineBarrier(command,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT|VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,0,NULL,0,NULL,3,barrier);
    for(i=0;i<2;i++){
        float value=i?0.5f:0.25f;VkClearColorValue color={{value,value,value,1}};
        v->CmdClearColorImage(command,image[i].image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,&color,1,&barrier[i].subresourceRange);
        barrier[i].oldLayout=barrier[i].newLayout;barrier[i].newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier[i].srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;barrier[i].dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
    }
    v->CmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,0,NULL,0,NULL,2,barrier);
    v->CmdBindPipeline(command,VK_PIPELINE_BIND_POINT_COMPUTE,pipeline);
    v->CmdBindDescriptorSets(command,VK_PIPELINE_BIND_POINT_COMPUTE,pipeline_layout,0,1,&set,0,NULL);v->CmdDispatch(command,1,1,1);
    barrier[2].oldLayout=VK_IMAGE_LAYOUT_GENERAL;barrier[2].newLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier[2].srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;barrier[2].dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
    v->CmdPipelineBarrier(command,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,NULL,0,NULL,1,barrier+2);
    copy.imageSubresource=(VkImageSubresourceLayers){VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent=(VkExtent3D){SIDE,SIDE,1};
    v->CmdCopyImageToBuffer(command,image[2].image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,readback.buffer,1,&copy);
    host.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
    host.srcQueueFamilyIndex=host.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;host.buffer=readback.buffer;host.size=VK_WHOLE_SIZE;
    v->CmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,NULL,1,&host,0,NULL);
    TRY(vulkan_command_submit(v,command,VK_NULL_HANDLE,VK_NULL_HANDLE));
    TRY(vulkan_buffer_read(v,&readback,output,sizeof(output)));
    for(i=0;i<PIXELS;i++)if(!isfinite(output[i]) || fabsf(output[i]-sqrtf(3.0f)*0.25f)>0.0001f){
        fprintf(stderr,"FSR3 reactive mismatch pixel=%u value=%f\n",i,output[i]);goto done;
    }
    *reactive=output[0];printf("FSR3_REACTIVE_PASS pixels=%u value=%.7f sdk=1.1.4 fp32\n",PIXELS,output[0]);ok=1;
done:
    v->DeviceWaitIdle(v->device);
    if(pipeline)v->DestroyPipeline(v->device,pipeline,NULL);
    if(shader)v->DestroyShaderModule(v->device,shader,NULL);
    if(pipeline_layout)v->DestroyPipelineLayout(v->device,pipeline_layout,NULL);
    if(pool)v->DestroyDescriptorPool(v->device,pool,NULL);
    if(layout)v->DestroyDescriptorSetLayout(v->device,layout,NULL);
    for(i=0;i<3;i++)vulkan_image_destroy(v,image+i);
    vulkan_buffer_destroy(v,&constants);vulkan_buffer_destroy(v,&settings);vulkan_buffer_destroy(v,&readback);return ok;
#undef VK
#undef TRY
}
int main(int argc,char **argv){
    SDL_Window *window=NULL;VulkanDevice device={0};int show=0,validation=0,ok=0,i,loaded=0;float reactive=0;
    for(i=1;i<argc;i++){
        if(!strcmp(argv[i],"--show"))show=1;
        else if(!strcmp(argv[i],"--validation"))validation=1;
        else {fprintf(stderr,"Usage: %s [--show] [--validation]\n",argv[0]);return 2;}
    }
    SDL_SetHint(SDL_HINT_WINDOW_ACTIVATE_WHEN_SHOWN,"0");
    if(!SDL_Init(SDL_INIT_VIDEO))goto done;
    if(!SDL_Vulkan_LoadLibrary(NULL))goto done;
    loaded=1;
    window=SDL_CreateWindow("Uprising Vulkan/FSR3 probe",640,480,SDL_WINDOW_VULKAN|SDL_WINDOW_HIDDEN);
    if(!window || !vulkan_device_open(&device,window,validation))goto done;
    if(show && !SDL_ShowWindow(window))goto done;
    if(!graphics_pass(&device,window) || !reactive_pass(&device,&reactive))goto done;
    if(show){SDL_PumpEvents();SDL_Delay(750);}
    if(device.validation_errors){fprintf(stderr,"Vulkan validation errors=%u\n",device.validation_errors);goto done;}
    printf("VULKAN_FSR3_PROBE_PASS validation=%s\n",validation?"enabled":"not-enabled");ok=1;
done:
    if(!ok)fprintf(stderr,"Vulkan/FSR3 probe failed; SDL: %s\n",SDL_GetError());
    vulkan_device_close(&device);
    if(device.validation_errors){fprintf(stderr,"Vulkan total validation errors=%u\n",device.validation_errors);ok=0;}
    SDL_DestroyWindow(window);if(loaded)SDL_Vulkan_UnloadLibrary();SDL_Quit();return ok?0:1;
}
