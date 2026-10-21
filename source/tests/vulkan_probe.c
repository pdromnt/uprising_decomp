#include "vulkan_device.h"
#include "vulkan_draw.h"
#include "vulkan_present.h"
#include "vulkan_temporal.h"
#include "fsr3_upscaler.h"
#ifdef UPRISING_FSR3_UPSCALER
#include "fsr3_reactive_spv.h"
#endif
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
static int ui_atlas_pass(VulkanDevice *v){
    VulkanDraw draw={0};VulkanDrawTexture atlas={0},standalone={0};
    uint8_t *pixels=calloc(1024u*1024u,4),tile[32*32*4],reference[64*64*4],actual[64*64*4];
    const float blue[4]={0,0,1,1};unsigned test,x,y,i,pass;int ok=0;
    if(!pixels || !vulkan_draw_open(&draw,v,64,64))goto done;
    for(i=3;i<1024u*1024u*4;i+=4)pixels[i]=255; /* Opaque black neighboring tiles. */
    if(!vulkan_draw_texture_create(&draw,&atlas,pixels,1024,1024,1,0))goto done;
    atlas.atlas=1;
    for(test=0;test<4;test++){
        for(y=0;y<32;y++)for(x=0;x<32;x++){
            uint8_t *p=tile+(x+y*32)*4;int border=x==0 || y==0 || x==31 || y==31;
            p[0]=border?(test>=2?255:0):(uint8_t)(x*6);
            p[1]=border?0:(uint8_t)(y*6);p[2]=border?(test>=2?255:0):32;
            p[3]=border && test<2?0:255;
        }
        if(!standalone.image.image){if(!vulkan_draw_texture_create(&draw,&standalone,tile,32,32,1,0))goto done;}
        else if(!vulkan_draw_texture_update(&draw,&standalone,tile,sizeof(tile)))goto done;
        if(!vulkan_draw_texture_region(&draw,&atlas,tile,32,32,32,32))goto done;
        for(pass=0;pass<2;pass++){
            float low=test&1?-.25f:0,high=test&1?1.25f:1;VulkanDrawTexture *texture=pass?&atlas:&standalone;
            if(!vulkan_draw_begin(&draw,blue))goto done;
            draw.texture_sampler=test&1?7:1;draw.material.texture_sampling=(uint32_t)draw.texture_sampler;
            draw.material.color_mode=draw.material.alpha_mode=4;draw.material.alpha_function=4;draw.material.alpha_reference=0;
            draw.material.chroma=test>=2?0x1ff00ffu:0;draw.material.padding[1]=34; /* Tile33, origin32/32. */
            if(!draw_quad(&draw,8,8,56,56,0,1,1,1,1,texture,0))goto done;
            for(i=0;i<draw.vertex_count;i++){
                VulkanDrawVertex *p=draw.staging+i;float u=p->position[0]==56?high:low,w=p->position[1]==56?high:low;
                p->uv[0]=pass?(32+u*32)/1024:u;p->uv[1]=pass?(32+w*32)/1024:w;p->uv[2]=1;
            }
            if(!vulkan_draw_finish(&draw,pass?actual:reference,sizeof(actual)))goto done;
        }
        if((test&1) && !pixel(reference,15,8,0,0,255))goto done;
        for(i=0;i<sizeof(actual);i++)if(abs((int)actual[i]-(int)reference[i])>2){
            fprintf(stderr,"Vulkan UI atlas mismatch test=%u pixel=%u,%u channel=%u atlas=%u standalone=%u\n",test,(i/4)%64,(i/4)/64,i%4,actual[i],reference[i]);goto done;
        }
    }
    printf("VULKAN_UI_ATLAS_PASS fractional=1.5 clamp=checked repeat=checked alpha=checked chroma=checked standalone_comparison=all_pixels\n");ok=1;
done:
    free(pixels);vulkan_draw_texture_destroy(&draw,&standalone);vulkan_draw_texture_destroy(&draw,&atlas);vulkan_draw_close(&draw);return ok;
}
static int temporal_mask_pass(VulkanDraw *draw,const float *motion){
    VulkanDevice *v=draw->device;VulkanTemporal temporal={0};VulkanBuffer buffer={0};
    uint8_t pixels[64*64];unsigned frame,i,accepted=0,rejected=0;int ok=0;
    if(!vulkan_temporal_open(&temporal,v,64,64) || !vulkan_buffer_create(v,&buffer,sizeof(pixels),VK_BUFFER_USAGE_TRANSFER_DST_BIT))goto done;
    for(frame=0;frame<2;frame++){
        VkCommandBuffer command;VkBufferImageCopy copy={0};VkBufferMemoryBarrier host={VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
        if(!vulkan_command_begin(v,&command))goto done;
        if(!vulkan_temporal_record(&temporal,command,&draw->motion)){v->FreeCommandBuffers(v->device,v->commands,1,&command);goto done;}
        copy.imageSubresource=(VkImageSubresourceLayers){VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent=(VkExtent3D){64,64,1};
        v->CmdCopyImageToBuffer(command,temporal.rejection.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,buffer.buffer,1,&copy);
        host.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
        host.srcQueueFamilyIndex=host.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;host.buffer=buffer.buffer;host.size=VK_WHOLE_SIZE;
        v->CmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,NULL,1,&host,0,NULL);
        if(!vulkan_command_submit(v,command,VK_NULL_HANDLE,VK_NULL_HANDLE) || !vulkan_buffer_read(v,&buffer,pixels,sizeof(pixels)))goto done;
        for(i=0;i<64*64;i++){
            unsigned expected=motion[i*4+2]<.99999f || motion[i*4+3]>=1?255:0;
            if(frame && !expected){
                float px=(float)(i%64)+motion[i*4],py=(float)(i/64)+motion[i*4+1];
                if(px>=0 && py>=0 && px<64 && py<64){
                    int x,y;for(y=-1;y<=1;y++)for(x=-1;x<=1;x++){
                        int sx=(int)floorf(px)+x,sy=(int)floorf(py)+y;unsigned sample;
                        if(sx<0)sx=0;if(sx>63)sx=63;if(sy<0)sy=0;if(sy>63)sy=63;
                        sample=(unsigned)(sx+sy*64);
                        if(motion[sample*4+2]<.99999f || motion[sample*4+3]>=1)expected=255;
                    }
                }
            }
            if(pixels[i]!=expected){fprintf(stderr,"Temporal mask mismatch pixel=%u got=%u expected=%u\n",i,pixels[i],expected);goto done;}
            if(pixels[i])rejected++;else accepted++;
        }
    }
    if(!accepted || !rejected)goto done;
    printf("VULKAN_TEMPORAL_MASK_PASS stable=%u rejected=%u translucency=checked occlusion=checked hud_excluded=checked reuse=checked\n",accepted,rejected);ok=1;
done:vulkan_temporal_close(&temporal);vulkan_buffer_destroy(v,&buffer);return ok;
}
static int read_image(VulkanDevice *v,const VulkanImage *image,uint32_t width,uint32_t height,void *pixels,size_t bytes){
    VulkanBuffer buffer={0};VkCommandBuffer command;VkBufferImageCopy copy={0};int ok=0;
    VkBufferMemoryBarrier host={VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    if(!vulkan_buffer_create(v,&buffer,bytes,VK_BUFFER_USAGE_TRANSFER_DST_BIT) || !vulkan_command_begin(v,&command))goto done;
    copy.imageSubresource=(VkImageSubresourceLayers){VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent=(VkExtent3D){width,height,1};
    v->CmdCopyImageToBuffer(command,image->image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,buffer.buffer,1,&copy);
    host.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
    host.srcQueueFamilyIndex=host.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;host.buffer=buffer.buffer;host.size=VK_WHOLE_SIZE;
    v->CmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,NULL,1,&host,0,NULL);
    ok=vulkan_command_submit(v,command,VK_NULL_HANDLE,VK_NULL_HANDLE) && vulkan_buffer_read(v,&buffer,pixels,bytes);
done:vulkan_buffer_destroy(v,&buffer);return ok;
}
static int temporal_monitor_pass(VulkanDevice *v){
    VulkanDraw source={0},upscaled={0},output={0};VulkanTemporal temporal={0};int ok=0;unsigned i,x,y,frame=0,stage=0;
    const float blue[4]={0,0,1,1},black[4]={0,0,0,1};uint8_t native[64*64*4],scene[64*64*4],crop[31*23*4],final[128*128*4];
    float depth[64*64],motion[64*64*4],crop_depth[31*23],crop_motion[31*23*4];
    if(!vulkan_draw_open(&source,v,64,64) || !vulkan_draw_open(&upscaled,v,62,46) || !vulkan_draw_open(&output,v,128,128) ||
       !vulkan_temporal_open(&temporal,v,31,23) || !vulkan_temporal_crop_open(&temporal) ||
       !vulkan_draw_begin(&source,blue) || !vulkan_draw_projection(&source,10,1,0,0))goto done;
    source.material.color_mode=0;source.material.alpha_mode=1;source.material.alpha_function=7;
    /* Green artwork island inside an irregular glass opening; translucent red
     * over blue also exercises the retained pre-scene background contribution. */
    if(!draw_quad(&source,20,20,26,26,0,0,1,0,1,NULL,0))goto done;
    source.capture_depth=1;
    if(!draw_quad(&source,8,12,39,20,.4f,1,0,0,.5f,NULL,VULKAN_DRAW_BLEND) ||
       !draw_quad(&source,8,26,39,35,.4f,1,0,0,1,NULL,3) ||
       !draw_quad(&source,8,20,20,26,.4f,1,0,0,1,NULL,3) ||
       !draw_quad(&source,26,20,39,26,.4f,1,0,0,1,NULL,3))goto done;
    {
        VulkanDrawVertex edge[3]={0};unsigned k;
        for(k=0;k<3;k++){
            edge[k].position[0]=k==1?38:30;edge[k].position[1]=k==2?34:28;edge[k].position[2]=.1f;
            edge[k].color[0]=edge[k].color[1]=edge[k].color[2]=edge[k].color[3]=edge[k].uv[2]=1;
        }
        if(!vulkan_draw_triangles(&source,edge,3,NULL,3|VULKAN_DRAW_LEQUAL))goto done;
    }
    for(i=0;i<source.vertex_count;i++){
        VulkanDrawVertex *vertex=source.staging+i;
        vertex->previous[0]=vertex->position[0]+4;vertex->previous[1]=vertex->position[1]-2;
        vertex->previous[2]=vertex->previous[3]=1;
    }
    if(!vulkan_draw_finish(&source,native,sizeof(native)) || !vulkan_draw_read_scene(&source,scene,sizeof(scene)) ||
       !vulkan_draw_read_reciprocal_depth(&source,depth,sizeof(depth)) || !vulkan_draw_read_motion(&source,motion,sizeof(motion)) ||
       !vulkan_draw_begin(&upscaled,black) || !vulkan_draw_finish(&upscaled,NULL,0) ||
       !vulkan_draw_begin(&output,black) || !vulkan_draw_finish(&output,NULL,0))goto done;
    output.material.extent[0]=output.material.extent[1]=64;
    for(frame=0;frame<3;frame++){
        stage=1;
        VkCommandBuffer command;
        if(frame==2){
            vulkan_draw_close(&output);vulkan_draw_close(&upscaled);
            if(!vulkan_draw_open(&output,v,100,100) || !vulkan_draw_open(&upscaled,v,49,36) ||
               !vulkan_draw_begin(&output,black) || !vulkan_draw_finish(&output,NULL,0) ||
               !vulkan_draw_begin(&upscaled,black) || !vulkan_draw_finish(&upscaled,NULL,0))goto done;
            output.material.extent[0]=output.material.extent[1]=64;
        }
        if(!vulkan_command_begin(v,&command))goto done;
        if(vulkan_temporal_crop(&temporal,command,&source.scene,&source.reciprocal_depth,&source.motion,64,12,64,64))goto done;
        if(!vulkan_temporal_crop(&temporal,command,&source.scene,&source.reciprocal_depth,&source.motion,8,12,64,64) ||
           !vulkan_command_submit(v,command,VK_NULL_HANDLE,VK_NULL_HANDLE) ||
           !read_image(v,temporal.cropped,31,23,crop,sizeof(crop)) ||
           !read_image(v,temporal.cropped+1,31,23,crop_depth,sizeof(crop_depth)) ||
           !read_image(v,temporal.cropped+2,31,23,crop_motion,sizeof(crop_motion)))goto done;
        for(y=0;y<23;y++){
            stage=2;
            if(memcmp(crop+y*31*4,scene+((y+12)*64+8)*4,31*4) ||
               memcmp(crop_depth+y*31,depth+(y+12)*64+8,31*sizeof(float)) ||
               memcmp(crop_motion+y*31*4,motion+((y+12)*64+8)*4,31*4*sizeof(float)))goto done;
        }
        /* Nearest reconstruction isolates composition from SDK quality here;
         * the separate upscaler pass and live game exercise actual FSR. */
        stage=3;
        if(!vulkan_draw_blit_color(&upscaled,temporal.cropped,31,23) || !vulkan_command_begin(v,&command) ||
           !vulkan_temporal_compose(&temporal,command,&upscaled.color,&source.scene,&source.color,&output.color,8,12,output.width,output.height) ||
           !vulkan_command_submit(v,command,VK_NULL_HANDLE,VK_NULL_HANDLE) || !vulkan_draw_resume(&output))goto done;
        output.material.color_mode=0;output.material.alpha_mode=1;output.material.alpha_function=7;
        if(!draw_quad(&output,0,0,4,4,0,1,1,1,1,NULL,0) || !vulkan_draw_finish(&output,final,sizeof(final)))goto done;
        for(y=0;y<output.height;y++)for(x=0;x<output.width;x++)for(i=0;i<3;i++){
            unsigned sx=(2*x+1)*64/(2*output.width),sy=(2*y+1)*64/(2*output.height);
            int hud=(2*x+1)*64<8*output.width && (2*y+1)*64<8*output.height;
            if(frame==2 && !hud && sx>=8 && sx<39 && sy>=12 && sy<35 && scene[(sy*64+sx)*4+3])continue;
            int expected=hud?255:native[(sy*64+sx)*4+i];
            if(abs(final[(y*output.width+x)*4+i]-expected)>1){fprintf(stderr,"Monitor composition mismatch %u/%u channel=%u\n",x,y,i);goto done;}
        }
        if(frame==2 && (!pixel(final,0,0,255,255,255) || final[(45*100+20)*4]!=255 || final[(45*100+20)*4+2]!=0 ||
           abs((int)final[(22*100+22)*4]-128)>1 || abs((int)final[(22*100+22)*4+2]-128)>1)){fprintf(stderr,"Monitor fractional landmarks failed\n");goto done;}
    }
    {
        stage=4;
        VkCommandBuffer command;unsigned changed=0;
        if(!vulkan_command_begin(v,&command) ||
           !vulkan_temporal_antialias(&temporal,command,&source.scene,&source.color,&output.color,8,12,output.width,output.height) ||
           !vulkan_command_submit(v,command,VK_NULL_HANDLE,VK_NULL_HANDLE) || !vulkan_draw_resume(&output) ||
           !draw_quad(&output,0,0,4,4,0,1,1,1,1,NULL,0) || !vulkan_draw_finish(&output,final,sizeof(final))){fprintf(stderr,"Spatial AA dispatch/readback failed\n");goto done;}
        for(y=0;y<output.height;y++)for(x=0;x<output.width;x++)for(i=0;i<3;i++){
            unsigned sx=(2*x+1)*64/(2*output.width),sy=(2*y+1)*64/(2*output.height);
            int hud=(2*x+1)*64<8*output.width && (2*y+1)*64<8*output.height;
            int covered=sx>=8 && sx<39 && sy>=12 && sy<35 && scene[(sy*64+sx)*4+3];
            int difference=abs(final[(y*output.width+x)*4+i]-(hud?255:native[(sy*64+sx)*4+i]));
            if(!hud && covered)changed+=difference>2;
            else if(difference>1){fprintf(stderr,"Spatial AA changed artwork/HUD at %u,%u\n",x,y);goto done;}
        }
        if(!changed || !pixel(final,0,0,255,255,255)){fprintf(stderr,"Spatial AA diagonal unchanged: %u\n",changed);goto done;}
        printf("VULKAN_SPATIAL_AA_PASS diagonal=filtered flat_artwork=preserved irregular_holes=preserved hud=preserved fractional_output=checked\n");
    }
    printf("VULKAN_MONITOR_COMPOSITION_PASS crop=31x23 offset=8/12 depth_motion_exact=checked irregular_artwork=checked translucency=checked hud=checked reuse=checked fractional_resize=checked\n");ok=1;
done:if(!ok)fprintf(stderr,"Monitor check failed stage=%u frame=%u\n",stage,frame);vulkan_temporal_close(&temporal);vulkan_draw_close(&output);vulkan_draw_close(&upscaled);vulkan_draw_close(&source);return ok;
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
        if(!vulkan_draw_triangles(&draw,triangle,3,NULL,3|VULKAN_DRAW_BLEND|VULKAN_DRAW_LEQUAL) || !vulkan_draw_finish(&draw,NULL,0) ||
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
        if(!temporal_mask_pass(&draw,motion)){ok=0;goto done;}
        {
            VulkanDrawVertex blended[3];memcpy(blended,triangle,sizeof(blended));
            for(i=0;i<3;i++){
                blended[i].position[0]=i==1?16:0;blended[i].position[1]=i==2?48:32;
                blended[i].position[2]=.1f;blended[i].color[3]=.5f;
                blended[i].previous[0]=blended[i].position[0]-4;blended[i].previous[1]=blended[i].position[1]+2;
            }
            draw.capture_depth=1;
            if(!vulkan_draw_resume(&draw) || !vulkan_draw_triangles(&draw,blended,3,NULL,3|VULKAN_DRAW_BLEND|VULKAN_DRAW_LEQUAL) ||
               !vulkan_draw_finish(&draw,NULL,0) || !vulkan_draw_read_motion(&draw,motion,sizeof(motion)) ||
               motion[(4+36*64)*4+2]!=1 || motion[(4+36*64)*4+3]!=1 || !temporal_mask_pass(&draw,motion)){
                fprintf(stderr,"Vulkan depth-writing translucency lost motion/rejection\n");ok=0;goto done;
            }
            printf("VULKAN_DEPTH_WRITING_TRANSLUCENCY_PASS vector_and_rejection=checked\n");
        }
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
    {
        const float black[4]={0,0,0,1};VulkanDrawVertex sky[3]={0};
        float motion[64*64*4],depths[64*64];unsigned i;
        if(!vulkan_draw_projection(&draw,1,1,.375f,-.25f) || !vulkan_draw_begin(&draw,black)){ok=0;goto done;}
        draw.material.color_mode=0;draw.material.alpha_mode=1;draw.material.alpha_function=7;
        for(i=0;i<3;i++){
            sky[i].position[0]=i==1?128:0;sky[i].position[1]=i==2?128:0;
            sky[i].color[2]=sky[i].color[3]=sky[i].uv[2]=1;
            sky[i].previous[0]=sky[i].position[0]+4;sky[i].previous[1]=sky[i].position[1]-2;
            sky[i].previous[2]=sky[i].previous[3]=1;
        }
        draw.capture_background=draw.capture_scene=1;
        if(!vulkan_draw_triangles(&draw,sky,3,NULL,0)){ok=0;goto done;}
        draw.capture_background=0;
        /* Independent cloud layer preserves base motion but rejects history. */
        if(!draw_quad(&draw,8,8,24,24,0,1,0,0,.5f,NULL,VULKAN_DRAW_BLEND)){ok=0;goto done;}
        draw.capture_scene=0;draw.capture_depth=1;
        /* A world effect with depth testing disabled must reject history too. */
        if(!draw_quad(&draw,32,32,48,48,0,0,1,0,.5f,NULL,VULKAN_DRAW_BLEND)){ok=0;goto done;}
        /* Zero-alpha blending contributes nothing. An unblended zero-alpha
         * draw still replaces RGB and must reject the previous color. */
        if(!draw_quad(&draw,32,0,48,16,0,1,1,1,0,NULL,VULKAN_DRAW_BLEND) ||
           !draw_quad(&draw,0,40,16,56,0,1,1,1,0,NULL,0)){ok=0;goto done;}
        draw.capture_depth=0;
        if(!draw_quad(&draw,0,0,64,64,0,1,1,1,1,NULL,0) || !vulkan_draw_finish(&draw,NULL,0) ||
           !vulkan_draw_read_motion(&draw,motion,sizeof(motion)) ||
           !vulkan_draw_read_reciprocal_depth(&draw,depths,sizeof(depths))){ok=0;goto done;}
        for(i=0;i<4;i++){
            unsigned p=i==0?40+8*64:i==1?8+8*64:i==2?40+40*64:8+44*64;const float *m=motion+p*4;
            if(depths[p]!=0 || fabsf(m[0]-4)>.001f || fabsf(m[1]+2)>.001f || m[2]!=1 || m[3]!=(i?1:0)){
                fprintf(stderr,"Vulkan sky/effect: infinite depth, motion or rejection failed sample=%u\n",i);ok=0;goto done;
            }
        }
        printf("VULKAN_SKY_MOTION_PASS infinite_depth=checked jitter=checked layers=checked no_depth_effect=checked invisible_effect=checked unblended_alpha=checked hud=checked\n");
    }
done:
    vulkan_present_close(&present);vulkan_draw_texture_destroy(&draw,&texture);vulkan_draw_close(&draw);return ok;
}

/* Runs AMD's unmodified FSR3 reactive-mask shader. This proves a real SDK
 * compute pass, not temporal upscaling or frame generation. No game assets. */
#ifdef UPRISING_FSR3_UPSCALER
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
static int moving_quad(VulkanDraw *d,float left,float top,float right,float bottom,
        float depth,float r,float g,float b,float q,float previous_dx){
    size_t first=d->vertex_count,i;
    if(!draw_quad(d,left,top,right,bottom,depth,r,g,b,1,NULL,3))return 0;
    for(i=first;i<d->vertex_count;i++){
        VulkanDrawVertex *p=d->staging+i;p->uv[2]=q;
        p->previous[0]=p->position[0]+previous_dx;p->previous[1]=p->position[1];
        p->previous[2]=p->previous[3]=1;
    }
    return 1;
}
static int upscale_motion_pass(VulkanDevice *v){
    VulkanDraw draw={0},result={0};VulkanTemporal temporal={0};Fsr3Upscaler *upscaler=NULL;
    const float black[4]={0,0,0,1};uint8_t rgba[128*128*4],mask[64*64];float depths[64*64],motion[64*64*4];
    unsigned frame,i;int ok=0;float previous_left=8;
    if(!vulkan_draw_open(&draw,v,64,64) || !vulkan_draw_open(&result,v,128,128) ||
       !vulkan_temporal_open(&temporal,v,64,64) || !vulkan_draw_begin(&result,black) ||
       !vulkan_draw_finish(&result,NULL,0))goto done;
    upscaler=fsr3_upscaler_open(v,64,64,128,128);if(!upscaler)goto done;
    for(frame=0;frame<10;frame++){
        VkCommandBuffer command;const VulkanImage *output;float jitter[2];
        float left=frame<4?8:frame==4?24:40;int cut=frame==9;
        /* Deterministic subpixel raster movement; vectors themselves are unjittered. */
        jitter[0]=(frame&1)?.25f:-.25f;jitter[1]=(frame&2)?.125f:-.125f;
        if(!vulkan_draw_projection(&draw,10,1,jitter[0],jitter[1]) || !vulkan_draw_begin(&draw,black))goto done;
        draw.capture_depth=1;draw.material.color_mode=0;draw.material.alpha_mode=1;draw.material.alpha_function=7;
        if(!moving_quad(&draw,0,0,64,64,.8f,0,cut?1:0,cut?0:1,.001f,0))goto done;
        if(frame<6 && !moving_quad(&draw,left,24,left+8,48,.2f,1,0,0,.01f,previous_left-left))goto done;
        if(frame==2 || frame==3){
            if(!draw_quad(&draw,48,8,60,20,.1f,1,0,0,.5f,NULL,VULKAN_DRAW_DEPTH_TEST|VULKAN_DRAW_BLEND))goto done;
        }
        if(!vulkan_draw_finish(&draw,NULL,0) || !vulkan_draw_read_reciprocal_depth(&draw,depths,sizeof(depths)) ||
           !vulkan_draw_read_motion(&draw,motion,sizeof(motion)))goto done;
        if(fabsf(depths[4+32*64]-.01f)>.00001f ||
           (frame<6 && (fabsf(depths[(unsigned)(left+4)+32*64]-.1f)>.00001f ||
            fabsf(motion[((unsigned)(left+4)+32*64)*4]-(previous_left-left))>.001f)))goto done;
        if((frame==2 || frame==3) && motion[(54+14*64)*4+3]!=1)goto done;
        if(!vulkan_command_begin(v,&command))goto done;
        if(!vulkan_temporal_record(&temporal,command,&draw.motion)){v->FreeCommandBuffers(v->device,v->commands,1,&command);goto done;}
        output=fsr3_upscaler_dispatch(upscaler,command,&draw.scene,&draw.reciprocal_depth,&draw.motion,&temporal.rejection,
            jitter[0],jitter[1],16.667f,10,1,frame==0 || cut,0);
        if(!output || !vulkan_command_submit(v,command,VK_NULL_HANDLE,VK_NULL_HANDLE) ||
           !vulkan_draw_blit_color(&result,output,128,128) || !vulkan_draw_readback(&result,rgba,sizeof(rgba)))goto done;
        /* A sprite/marker leaves no solid depth footprint. Protect its vacated
         * pixels for the following frame, without feeding rejection back forever. */
        if(!read_image(v,&temporal.rejection,64,64,mask,sizeof(mask)) ||
           mask[54+14*64]!=(frame>=2 && frame<=4?255:0)){
            fprintf(stderr,"FSR3 vacated effect rejection failed frame=%u\n",frame);goto done;
        }
        for(i=0;i<3;i++){
            /* Foreground's vacated footprint, active foreground and an effect
             * appearing/disappearing over stable blue. Stay clear of filter edges. */
            unsigned x=i==0?(unsigned)(previous_left+4):i==1?(unsigned)(left+4):54,y=i==2?14:36;
            const uint8_t *p=rgba+(x*2+y*2*128)*4;float expected[3]={0,cut?1:0,cut?0:1};unsigned channel;
            if(i==0 && frame<4)continue;
            if(i==1 && frame<6){expected[0]=1;expected[1]=expected[2]=0;}
            if(i==2 && (frame==2 || frame==3)){expected[0]=expected[2]=.5f;}
            for(channel=0;channel<3;channel++)if(fabsf(p[channel]/255.0f-expected[channel])>.08f){
                fprintf(stderr,"FSR3 moving mismatch frame=%u sample=%u channel=%u value=%u expected=%g\n",frame,i,channel,p[channel],expected[channel]);goto done;
            }
        }
        previous_left=left;
    }
    printf("FSR3_MOVING_PASS frames=10 depth=checked unjittered_motion=checked disocclusion=checked effect_appearance=checked effect_removal=checked vacated_coverage=checked coverage_expiry=checked reset=checked\n");ok=1;
done:
    fsr3_upscaler_close(upscaler);vulkan_temporal_close(&temporal);vulkan_draw_close(&result);vulkan_draw_close(&draw);return ok;
}
static int upscale_pass(VulkanDevice *v){
    VulkanDraw draw={0},composed={0};VulkanTemporal temporal={0};VulkanBuffer readback={0};Fsr3Upscaler *upscaler=NULL;int ok=0;unsigned frame,i;
    const float black[4]={0,0,0,1};float pixels[128*128*4];
    uint8_t rgba[128*128*4];
    if(!vulkan_draw_open(&draw,v,64,64) || !vulkan_temporal_open(&temporal,v,64,64) || !vulkan_buffer_create(v,&readback,sizeof(pixels),VK_BUFFER_USAGE_TRANSFER_DST_BIT))goto done;
    if(fsr3_upscaler_jitter_phases(64,128)!=32 || fsr3_upscaler_jitter_phases(64,64)!=8 ||
       !vulkan_draw_open(&composed,v,128,128) || !vulkan_draw_begin(&composed,black) || !vulkan_draw_finish(&composed,NULL,0))goto done;
    composed.material.extent[0]=composed.material.extent[1]=64;
    upscaler=fsr3_upscaler_open(v,64,64,128,128);if(!upscaler)goto done;
    if(fsr3_upscaler_open(v,64,64,128,128)){fprintf(stderr,"FSR3 accepted competing context ownership\n");goto done;}
    for(frame=0;frame<3;frame++){
        VkCommandBuffer command;const VulkanImage *output;VkBufferImageCopy copy={0};
        VkBufferMemoryBarrier host={VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};float jitter=frame?0.25f:-0.25f;
        if(!vulkan_draw_projection(&draw,10,1,jitter,-jitter) || !vulkan_draw_begin(&draw,black))goto done;
        draw.capture_depth=1;draw.material.color_mode=0;draw.material.alpha_mode=1;draw.material.alpha_function=7;
        if(!draw_quad(&draw,0,0,64,64,0.4f,0.25f,0.5f,0.75f,1,NULL,3))goto done;
        if(frame)for(i=0;i<draw.vertex_count;i++){
            VulkanDrawVertex *vertex=draw.staging+i;
            vertex->previous[0]=vertex->position[0];vertex->previous[1]=vertex->position[1];vertex->previous[2]=vertex->previous[3]=1;
        }
        if(frame==2 && !draw_quad(&draw,0,0,32,32,0.1f,0.25f,0.5f,0.75f,.5f,NULL,VULKAN_DRAW_DEPTH_TEST|VULKAN_DRAW_BLEND))goto done;
        if(!vulkan_draw_finish(&draw,NULL,0) || !vulkan_command_begin(v,&command))goto done;
        if(!vulkan_temporal_record(&temporal,command,&draw.motion)){v->FreeCommandBuffers(v->device,v->commands,1,&command);goto done;}
        output=fsr3_upscaler_dispatch(upscaler,command,&draw.scene,&draw.reciprocal_depth,&draw.motion,&temporal.rejection,jitter,-jitter,16.667f,10,1,frame==2,frame==1);
        if(!output)goto done;
        copy.imageSubresource=(VkImageSubresourceLayers){VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent=(VkExtent3D){128,128,1};
        v->CmdCopyImageToBuffer(command,output->image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,readback.buffer,1,&copy);
        host.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
        host.srcQueueFamilyIndex=host.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;host.buffer=readback.buffer;host.size=VK_WHOLE_SIZE;
        v->CmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,NULL,1,&host,0,NULL);
        if(!vulkan_command_submit(v,command,VK_NULL_HANDLE,VK_NULL_HANDLE) || !vulkan_buffer_read(v,&readback,pixels,sizeof(pixels)))goto done;
        for(i=0;i<128*128*4;i++)if(!isfinite(pixels[i])){fprintf(stderr,"FSR3 nonfinite output frame=%u index=%u\n",frame,i);goto done;}
        for(i=0;i<3;i++)if(fabsf(pixels[(64+64*128)*4+i]-(i==0?64:i==1?128:191)/255.0f)>0.025f){
            fprintf(stderr,"FSR3 output mismatch frame=%u channel=%u value=%g\n",frame,i,pixels[(64+64*128)*4+i]);goto done;}
        /* Real output is converted on the GPU, then UI is drawn in legacy
         * coordinates at output resolution. It must not enter scene history. */
        if(!vulkan_draw_blit_color(&composed,output,128,128) || !vulkan_draw_resume(&composed))goto done;
        composed.material.color_mode=0;composed.material.alpha_mode=1;composed.material.alpha_function=7;
        if(!draw_quad(&composed,2,2,10,10,0,1,0,0,1,NULL,0) || !vulkan_draw_finish(&composed,rgba,sizeof(rgba)))goto done;
        if(rgba[(8+8*128)*4]!=255 || rgba[(8+8*128)*4+1]!=0 || rgba[(8+8*128)*4+2]!=0 ||
           abs((int)rgba[(64+64*128)*4]-64)>7)goto done;
        if(!vulkan_draw_read_scene(&composed,rgba,sizeof(rgba)))goto done;
        for(i=0;i<sizeof(rgba);i++)if(rgba[i])goto done;
        /* Legacy CPU consumers still receive their original dimensions. */
        if(!vulkan_draw_blit_color(&draw,&composed.color,128,128) || !vulkan_draw_readback(&draw,rgba,sizeof(rgba)) ||
           rgba[(4+4*64)*4]!=255 || rgba[(4+4*64)*4+1]!=0)goto done;
    }
    fsr3_upscaler_close(upscaler);upscaler=fsr3_upscaler_open(v,64,64,128,128);if(!upscaler)goto done;
    printf("FSR3_UPSCALE_PASS frames=3 render=64x64 output=128x128 upstream_host=1 sharpen=checked reset=checked finite=checked recreate=checked composition=checked legacy_readback=checked masks=invalid/opaque/translucent\n");ok=1;
done:
    fsr3_upscaler_close(upscaler);vulkan_temporal_close(&temporal);vulkan_buffer_destroy(v,&readback);vulkan_draw_close(&composed);vulkan_draw_close(&draw);return ok;
}
#endif
int main(int argc,char **argv){
    SDL_Window *window=NULL;VulkanDevice device={0};int show=0,validation=0,ok=0,i,loaded=0,bundle_check=0;float reactive=0;
    for(i=1;i<argc;i++){
        if(!strcmp(argv[i],"--show"))show=1;
        else if(!strcmp(argv[i],"--validation"))validation=1;
        else if(!strcmp(argv[i],"--bundle-loader-check"))bundle_check=1;
        else {fprintf(stderr,"Usage: %s [--show] [--validation] [--bundle-loader-check]\n",argv[0]);return 2;}
    }
    SDL_SetHint(SDL_HINT_WINDOW_ACTIVATE_WHEN_SHOWN,"0");
    if(!SDL_Init(SDL_INIT_VIDEO))goto done;
    if(!vulkan_runtime_prepare())goto done;
    if(!SDL_Vulkan_LoadLibrary(NULL))goto done;
    loaded=1;
    if(bundle_check){
        const char *loader=SDL_GetHint(SDL_HINT_VULKAN_LIBRARY),*driver=SDL_getenv("VK_DRIVER_FILES");
        if(SDL_getenv("VULKAN_SDK") || SDL_getenv("DYLD_LIBRARY_PATH"))goto done;
        if(!loader || !driver || !strstr(loader,".app/Contents/Frameworks/") || !strstr(driver,".app/Contents/Resources/"))goto done;
        printf("VULKAN_BUNDLE_LOADER_PASS sdk_environment=absent loader=owned icd=owned gpu_execution=not-tested\n");ok=1;goto done;
    }
    window=SDL_CreateWindow("Uprising Vulkan/FSR3 probe",640,480,SDL_WINDOW_VULKAN|SDL_WINDOW_HIDDEN);
    if(!window || !vulkan_device_open(&device,window,validation))goto done;
    if(show && !SDL_ShowWindow(window))goto done;
    if(!graphics_pass(&device,window) || !ui_atlas_pass(&device) || !temporal_monitor_pass(&device))goto done;
    device.defer_submissions=1;
    for(i=0;i<65;i++){
        VkCommandBuffer command;
        if(!vulkan_command_begin(&device,&command) || !vulkan_command_submit(&device,command,VK_NULL_HANDLE,VK_NULL_HANDLE) ||
           device.pending_count!=(unsigned)(i%64+1))goto done;
    }
    if(!graphics_pass(&device,window) || !temporal_monitor_pass(&device) || !vulkan_queue_drain(&device) || device.pending_count)goto done;
    printf("VULKAN_DEFERRED_QUEUE_PASS bounded_queue=checked draw=checked readback=checked reuse=checked destruction=checked retirement=checked\n");
#ifdef UPRISING_FSR3_UPSCALER
    if(!reactive_pass(&device,&reactive) || !upscale_pass(&device) || !upscale_motion_pass(&device))goto done;
#else
    (void)reactive;
    printf("VULKAN_ONLY_TARGET pointer_bits=32 fsr3=not-built\n");
#endif
    if(show){SDL_PumpEvents();SDL_Delay(750);}
    if(device.validation_errors){fprintf(stderr,"Vulkan validation errors=%u\n",device.validation_errors);goto done;}
    printf("VULKAN_FSR3_PROBE_PASS validation=%s\n",validation?"enabled":"not-enabled");ok=1;
done:
    if(!ok)fprintf(stderr,"Vulkan/FSR3 probe failed; SDL: %s\n",SDL_GetError());
    vulkan_device_close(&device);
    if(device.validation_errors){fprintf(stderr,"Vulkan total validation errors=%u\n",device.validation_errors);ok=0;}
    SDL_DestroyWindow(window);if(loaded)SDL_Vulkan_UnloadLibrary();SDL_Quit();return ok?0:1;
}
