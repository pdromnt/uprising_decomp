#include "vulkan_device.h"
#include "fsr3_reactive_spv.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

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
    if(!reactive_pass(&device,&reactive) || !vulkan_present_clear(&device,reactive))goto done;
    if(show){SDL_PumpEvents();SDL_Delay(750);}
    if(device.validation_errors){fprintf(stderr,"Vulkan validation errors=%u\n",device.validation_errors);goto done;}
    printf("VULKAN_FSR3_PROBE_PASS validation=%s\n",validation?"enabled":"not-enabled");ok=1;
done:
    if(!ok)fprintf(stderr,"Vulkan/FSR3 probe failed; SDL: %s\n",SDL_GetError());
    vulkan_device_close(&device);
    if(device.validation_errors){fprintf(stderr,"Vulkan total validation errors=%u\n",device.validation_errors);ok=0;}
    SDL_DestroyWindow(window);if(loaded)SDL_Vulkan_UnloadLibrary();SDL_Quit();return ok?0:1;
}
