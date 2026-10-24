#include "vulkan_draw.h"
#include "projected_vert_spv.h"
#include "projected_frag_spv.h"
#include "fullscreen_vert_spv.h"
#include "depth_copy_frag_spv.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
_Static_assert(sizeof(VulkanDrawMaterial)==128,"Vulkan push block layout");
_Static_assert(offsetof(VulkanDrawMaterial,jitter)==104 && offsetof(VulkanDrawMaterial,depth_scale)==112,"Vulkan projection offsets");
_Static_assert(offsetof(VulkanDrawMaterial,background_motion)==120,"Vulkan background motion offsets");
enum {CAPTURE_RECIPROCAL_DEPTH=256,CAPTURE_REACTIVE=512,CAPTURE_SCENE=1024};

#define VK(call) do {VkResult status=(call);if(status!=VK_SUCCESS){fprintf(stderr,"Vulkan draw %s: %d\n",#call,(int)status);goto fail;}}while(0)
#define TRY(call) do {if(!(call))goto fail;}while(0)

int vulkan_draw_texture_create(VulkanDraw *d,VulkanDrawTexture *t,const uint8_t *rgba,uint32_t width,uint32_t height,int linear,int repeat){
    VulkanDevice *v=d->device;
    VkSamplerCreateInfo sampler={VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    VkDescriptorSetAllocateInfo allocate={VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    VkDescriptorImageInfo info;VkWriteDescriptorSet write={VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    size_t bytes;
    if(!width || !height || !rgba || width>v->properties.limits.maxImageDimension2D ||
       height>v->properties.limits.maxImageDimension2D || (uint64_t)width*height>SIZE_MAX/4)return 0;
    memset(t,0,sizeof(*t));bytes=(size_t)width*height*4;
    t->width=width;t->height=height;
    TRY(vulkan_buffer_create(v,&t->upload,bytes,VK_BUFFER_USAGE_TRANSFER_SRC_BIT));
    TRY(vulkan_image_create(v,&t->image,VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,width,height));
    sampler.magFilter=sampler.minFilter=linear?VK_FILTER_LINEAR:VK_FILTER_NEAREST;
    sampler.mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sampler.addressModeU=sampler.addressModeV=sampler.addressModeW=repeat?VK_SAMPLER_ADDRESS_MODE_REPEAT:VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    VK(v->CreateSampler(v->device,&sampler,NULL,&t->sampler));
    t->pool=d->descriptors;allocate.descriptorPool=t->pool;allocate.descriptorSetCount=1;allocate.pSetLayouts=&d->texture_layout;
    VK(v->AllocateDescriptorSets(v->device,&allocate,&t->descriptor));
    info=(VkDescriptorImageInfo){t->sampler,t->image.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    write.dstSet=t->descriptor;write.descriptorCount=1;write.descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;write.pImageInfo=&info;
    v->UpdateDescriptorSets(v->device,1,&write,0,NULL);
    TRY(vulkan_draw_texture_update(d,t,rgba,bytes));return 1;
fail:
    vulkan_draw_texture_destroy(d,t);return 0;
}
int vulkan_draw_texture_update(VulkanDraw *d,VulkanDrawTexture *t,const uint8_t *rgba,size_t bytes){
    if(bytes!=(size_t)t->width*t->height*4)return 0;
    return vulkan_draw_texture_region(d,t,rgba,0,0,t->width,t->height);
}
static void texture_copy_record(VulkanDevice *v,VkCommandBuffer command,VulkanDrawTexture *t,uint32_t x,uint32_t y,uint32_t width,uint32_t height){
    VkImageMemoryBarrier barrier={VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};VkBufferImageCopy copy={0};
    barrier.oldLayout=t->initialized?VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.srcAccessMask=t->initialized?VK_ACCESS_SHADER_READ_BIT:0;barrier.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;barrier.srcQueueFamilyIndex=barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
    barrier.image=t->image.image;barrier.subresourceRange=(VkImageSubresourceRange){VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
    v->CmdPipelineBarrier(command,t->initialized?VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT:VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,NULL,0,NULL,1,&barrier);
    copy.imageSubresource=(VkImageSubresourceLayers){VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent=(VkExtent3D){width,height,1};copy.imageOffset=(VkOffset3D){(int32_t)x,(int32_t)y,0};
    v->CmdCopyBufferToImage(command,t->upload.buffer,t->image.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&copy);
    barrier.oldLayout=barrier.newLayout;barrier.newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
    v->CmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,0,0,NULL,0,NULL,1,&barrier);
    t->initialized=1;
}
int vulkan_draw_texture_region(VulkanDraw *d,VulkanDrawTexture *t,const uint8_t *rgba,uint32_t x,uint32_t y,uint32_t width,uint32_t height){
    VulkanDevice *v=d->device;VkCommandBuffer command;
    if(!t->image.image || !rgba || !width || !height || x>t->width || y>t->height || width>t->width-x || height>t->height-y)return 0;
    TRY(vulkan_buffer_write(v,&t->upload,rgba,(size_t)width*height*4));
    TRY(vulkan_command_begin(v,&command));texture_copy_record(v,command,t,x,y,width,height);
    return vulkan_command_submit(v,command,VK_NULL_HANDLE,VK_NULL_HANDLE);
fail:return 0;
}
void vulkan_draw_texture_destroy(VulkanDraw *d,VulkanDrawTexture *t){
    VulkanDevice *v=d->device;unsigned i;
    if(t->descriptor)vulkan_queue_drain(v);
    if(t->descriptor)v->FreeDescriptorSets(v->device,t->pool,1,&t->descriptor);
    for(i=0;i<8;i++)if(t->variants[i])v->FreeDescriptorSets(v->device,t->pool,1,t->variants+i);
    if(t->sampler)v->DestroySampler(v->device,t->sampler,NULL);
    vulkan_image_destroy(v,&t->image);vulkan_buffer_destroy(v,&t->upload);memset(t,0,sizeof(*t));
}
static VkDescriptorSet texture_binding(VulkanDraw *d,VulkanDrawTexture *t){
    VulkanDevice *v=d->device;unsigned key;VkDescriptorImageInfo image;
    VkDescriptorSetAllocateInfo allocate={VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    VkWriteDescriptorSet write={VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};VkSamplerCreateInfo sampler={VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    if(d->texture_sampler<0)return t->descriptor;
    key=(unsigned)d->texture_sampler;if(key>=8)return VK_NULL_HANDLE;if(t->variants[key])return t->variants[key];
    if(!d->samplers[key]){
        sampler.magFilter=sampler.minFilter=key&1?VK_FILTER_LINEAR:VK_FILTER_NEAREST;sampler.mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST;
        sampler.addressModeU=key&2?VK_SAMPLER_ADDRESS_MODE_REPEAT:VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler.addressModeV=key&4?VK_SAMPLER_ADDRESS_MODE_REPEAT:VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;sampler.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        VK(v->CreateSampler(v->device,&sampler,NULL,d->samplers+key));
    }
    allocate.descriptorPool=t->pool;allocate.descriptorSetCount=1;allocate.pSetLayouts=&d->texture_layout;
    VK(v->AllocateDescriptorSets(v->device,&allocate,t->variants+key));
    image=(VkDescriptorImageInfo){d->samplers[key],t->image.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    write.dstSet=t->variants[key];write.descriptorCount=1;write.descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;write.pImageInfo=&image;
    v->UpdateDescriptorSets(v->device,1,&write,0,NULL);return t->variants[key];
fail:return VK_NULL_HANDLE;
}
static int draw_pipeline(VulkanDraw *d,uint32_t flags){
    VulkanDevice *v=d->device;
    if(d->pipelines[flags])return 1;

    VkPipelineShaderStageCreateInfo stages[2]={{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO},{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO}};
    VkVertexInputBindingDescription vertex_binding={0,sizeof(VulkanDrawVertex),VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription attributes[5]={{0,0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(VulkanDrawVertex,position)},
        {1,0,VK_FORMAT_R32G32B32A32_SFLOAT,offsetof(VulkanDrawVertex,color)},{2,0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(VulkanDrawVertex,uv)},
        {3,0,VK_FORMAT_R32_SFLOAT,offsetof(VulkanDrawVertex,fog)},
        {4,0,VK_FORMAT_R32G32B32A32_SFLOAT,offsetof(VulkanDrawVertex,previous)}};
    VkPipelineVertexInputStateCreateInfo input={VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    VkPipelineInputAssemblyStateCreateInfo assembly={VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    VkPipelineViewportStateCreateInfo viewport={VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    VkViewport rect={0,0,(float)d->width,(float)d->height,0,1};VkRect2D scissor={{0,0},{d->width,d->height}};
    VkPipelineRasterizationStateCreateInfo raster={VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    VkPipelineMultisampleStateCreateInfo samples={VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    VkPipelineDepthStencilStateCreateInfo z={VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    VkPipelineColorBlendAttachmentState blend[4]={{0}};VkPipelineColorBlendStateCreateInfo blending={VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    VkGraphicsPipelineCreateInfo pipeline={VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};unsigned i;
    stages[0].stage=VK_SHADER_STAGE_VERTEX_BIT;stages[1].stage=VK_SHADER_STAGE_FRAGMENT_BIT;
    for(i=0;i<2;i++){stages[i].module=d->shaders[i];stages[i].pName="main";}
    input.vertexBindingDescriptionCount=1;input.pVertexBindingDescriptions=&vertex_binding;input.vertexAttributeDescriptionCount=5;input.pVertexAttributeDescriptions=attributes;
    assembly.topology=flags&VULKAN_DRAW_POINTS?VK_PRIMITIVE_TOPOLOGY_POINT_LIST:
        flags&VULKAN_DRAW_LINES?VK_PRIMITIVE_TOPOLOGY_LINE_LIST:VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    viewport.viewportCount=viewport.scissorCount=1;viewport.pViewports=&rect;viewport.pScissors=&scissor;
    raster.polygonMode=VK_POLYGON_MODE_FILL;raster.cullMode=VK_CULL_MODE_NONE;raster.frontFace=VK_FRONT_FACE_COUNTER_CLOCKWISE;raster.lineWidth=1;
    samples.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;z.depthCompareOp=VK_COMPARE_OP_LESS_OR_EQUAL;
    blend[0].colorWriteMask=VK_COLOR_COMPONENT_R_BIT|VK_COLOR_COMPONENT_G_BIT|VK_COLOR_COMPONENT_B_BIT|VK_COLOR_COMPONENT_A_BIT;
    blend[0].srcColorBlendFactor=blend[0].srcAlphaBlendFactor=VK_BLEND_FACTOR_SRC_ALPHA;
    blend[0].dstColorBlendFactor=blend[0].dstAlphaBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blend[0].colorBlendOp=blend[0].alphaBlendOp=VK_BLEND_OP_ADD;
    blend[1].colorWriteMask=flags&CAPTURE_RECIPROCAL_DEPTH?VK_COLOR_COMPONENT_R_BIT:0;
    blend[2].colorWriteMask=flags&CAPTURE_RECIPROCAL_DEPTH?blend[0].colorWriteMask:0;
    if(flags&CAPTURE_REACTIVE){
        blend[2].colorWriteMask=VK_COLOR_COMPONENT_A_BIT;blend[2].blendEnable=VK_TRUE;
        blend[2].alphaBlendOp=VK_BLEND_OP_MAX;
        blend[2].srcAlphaBlendFactor=blend[2].dstAlphaBlendFactor=VK_BLEND_FACTOR_ONE;
    }
    blending.attachmentCount=4;blending.pAttachments=blend;
    pipeline.stageCount=2;pipeline.pStages=stages;pipeline.pVertexInputState=&input;pipeline.pInputAssemblyState=&assembly;
    pipeline.pViewportState=&viewport;pipeline.pRasterizationState=&raster;pipeline.pMultisampleState=&samples;
    pipeline.pDepthStencilState=&z;pipeline.pColorBlendState=&blending;pipeline.layout=d->layout;pipeline.renderPass=d->pass;
    z.depthTestEnable=(flags&VULKAN_DRAW_DEPTH_TEST)!=0;z.depthWriteEnable=(flags&VULKAN_DRAW_DEPTH_WRITE)!=0;
    z.depthCompareOp=(VkCompareOp)((flags>>3)&7);blend[0].blendEnable=(flags&VULKAN_DRAW_BLEND)!=0;
    blend[3]=blend[0];if(!(flags&CAPTURE_SCENE))blend[3].colorWriteMask=0;
    VK(v->CreateGraphicsPipelines(v->device,VK_NULL_HANDLE,1,&pipeline,NULL,d->pipelines+flags));return 1;
fail:return 0;
}
int vulkan_draw_open(VulkanDraw *d,VulkanDevice *v,uint32_t width,uint32_t height){
    VkAttachmentDescription attachment[5]={{0}};VkAttachmentReference color[4]={{0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},{2,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},{3,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},{4,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}},depth={1,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
    VkSubpassDescription subpass={0};VkSubpassDependency dependency[2]={{0}};
    VkRenderPassCreateInfo pass={VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};VkFramebufferCreateInfo fb={VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};VkImageView views[5];
    VkDescriptorSetLayoutBinding binding={0,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT,NULL};
    VkDescriptorSetLayoutCreateInfo texture={VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    VkDescriptorPoolSize size={VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,4096};VkDescriptorPoolCreateInfo pool={VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    VkPushConstantRange push={VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(VulkanDrawMaterial)};VkPipelineLayoutCreateInfo layout={VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    VkShaderModuleCreateInfo shader={VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};VkDescriptorSetLayout layouts[2];
    const uint8_t white[4]={255,255,255,255};
    memset(d,0,sizeof(*d));d->device=v;d->width=width;d->height=height;
    d->texture_sampler=-1;d->scene_depth_scale=1;
    d->clear_depth=1;d->material.extent[0]=(float)width;d->material.extent[1]=(float)height;
    d->material.color_mode=d->material.alpha_mode=4;d->material.alpha_function=4;
    if(!width || !height || width>v->properties.limits.maxFramebufferWidth || height>v->properties.limits.maxFramebufferHeight || (uint64_t)width*height>SIZE_MAX/16)goto fail;
    TRY(vulkan_image_create(v,&d->color,VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,width,height));
    TRY(vulkan_image_create_aspect(v,&d->depth,VK_FORMAT_D32_SFLOAT,VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,width,height,VK_IMAGE_ASPECT_DEPTH_BIT));
    TRY(vulkan_image_create(v,&d->reciprocal_depth,VK_FORMAT_R32_SFLOAT,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,width,height));
    TRY(vulkan_image_create(v,&d->motion,VK_FORMAT_R32G32B32A32_SFLOAT,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,width,height));
    TRY(vulkan_image_create(v,&d->scene,VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,width,height));
    TRY(vulkan_buffer_create(v,&d->readback,(VkDeviceSize)width*height*16,VK_BUFFER_USAGE_TRANSFER_DST_BIT));
    attachment[0].format=VK_FORMAT_R8G8B8A8_UNORM;attachment[0].samples=VK_SAMPLE_COUNT_1_BIT;
    attachment[0].loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;attachment[0].storeOp=VK_ATTACHMENT_STORE_OP_STORE;
    attachment[0].stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;attachment[0].stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment[0].initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;attachment[0].finalLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    attachment[1]=attachment[0];attachment[1].format=VK_FORMAT_D32_SFLOAT;
    attachment[1].finalLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    attachment[2]=attachment[0];attachment[2].format=VK_FORMAT_R32_SFLOAT;
    attachment[3]=attachment[0];attachment[3].format=VK_FORMAT_R32G32B32A32_SFLOAT;
    attachment[4]=attachment[0];
    subpass.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;subpass.colorAttachmentCount=4;subpass.pColorAttachments=color;subpass.pDepthStencilAttachment=&depth;
    dependency[0].srcSubpass=VK_SUBPASS_EXTERNAL;dependency[0].dstSubpass=0;
    dependency[0].srcStageMask=VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;dependency[0].srcAccessMask=VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT;
    dependency[0].dstStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT|VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT|VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependency[0].dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependency[1].srcSubpass=0;dependency[1].dstSubpass=VK_SUBPASS_EXTERNAL;
    dependency[1].srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;dependency[1].dstStageMask=VK_PIPELINE_STAGE_TRANSFER_BIT;
    dependency[1].srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;dependency[1].dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
    pass.attachmentCount=5;pass.pAttachments=attachment;pass.subpassCount=1;pass.pSubpasses=&subpass;pass.dependencyCount=2;pass.pDependencies=dependency;
    VK(v->CreateRenderPass(v->device,&pass,NULL,&d->pass));views[0]=d->color.view;views[1]=d->depth.view;views[2]=d->reciprocal_depth.view;views[3]=d->motion.view;
    views[4]=d->scene.view;
    attachment[0].loadOp=attachment[1].loadOp=attachment[2].loadOp=attachment[3].loadOp=attachment[4].loadOp=VK_ATTACHMENT_LOAD_OP_LOAD;
    attachment[0].initialLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;attachment[1].initialLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    attachment[2].initialLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    attachment[3].initialLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    attachment[4].initialLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    VK(v->CreateRenderPass(v->device,&pass,NULL,&d->resume_pass));
    fb.renderPass=d->pass;fb.attachmentCount=5;fb.pAttachments=views;fb.width=width;fb.height=height;fb.layers=1;
    VK(v->CreateFramebuffer(v->device,&fb,NULL,&d->framebuffer));
    texture.bindingCount=1;texture.pBindings=&binding;VK(v->CreateDescriptorSetLayout(v->device,&texture,NULL,&d->texture_layout));
    pool.flags=VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;pool.maxSets=4096;pool.poolSizeCount=1;pool.pPoolSizes=&size;
    VK(v->CreateDescriptorPool(v->device,&pool,NULL,&d->descriptors));
    layouts[0]=layouts[1]=d->texture_layout;
    layout.setLayoutCount=2;layout.pSetLayouts=layouts;layout.pushConstantRangeCount=1;layout.pPushConstantRanges=&push;
    VK(v->CreatePipelineLayout(v->device,&layout,NULL,&d->layout));
    shader.codeSize=sizeof(projected_vert_spv);shader.pCode=projected_vert_spv;VK(v->CreateShaderModule(v->device,&shader,NULL,d->shaders));
    shader.codeSize=sizeof(projected_frag_spv);shader.pCode=projected_frag_spv;VK(v->CreateShaderModule(v->device,&shader,NULL,d->shaders+1));
    TRY(vulkan_draw_texture_create(d,&d->white,white,1,1,0,0));return 1;
fail:

    vulkan_draw_close(d);return 0;
}
void vulkan_draw_close(VulkanDraw *d){
    if(d->device)vulkan_queue_drain(d->device);
    VulkanDevice *v=d->device;unsigned i;if(!v)return;
    v->DeviceWaitIdle(v->device);vulkan_draw_texture_destroy(d,&d->white);
    for(i=0;i<1728;i++)if(d->pipelines[i])v->DestroyPipeline(v->device,d->pipelines[i],NULL);
    for(i=0;i<2;i++)if(d->shaders[i])v->DestroyShaderModule(v->device,d->shaders[i],NULL);
    if(d->depth_copy_pipeline)v->DestroyPipeline(v->device,d->depth_copy_pipeline,NULL);
    for(i=0;i<2;i++)if(d->depth_copy_shaders[i])v->DestroyShaderModule(v->device,d->depth_copy_shaders[i],NULL);
    for(i=0;i<8;i++)if(d->samplers[i])v->DestroySampler(v->device,d->samplers[i],NULL);
    if(d->layout)v->DestroyPipelineLayout(v->device,d->layout,NULL);
    if(d->descriptors)v->DestroyDescriptorPool(v->device,d->descriptors,NULL);
    if(d->texture_layout)v->DestroyDescriptorSetLayout(v->device,d->texture_layout,NULL);
    if(d->framebuffer)v->DestroyFramebuffer(v->device,d->framebuffer,NULL);
    if(d->pass)v->DestroyRenderPass(v->device,d->pass,NULL);
    if(d->resume_pass)v->DestroyRenderPass(v->device,d->resume_pass,NULL);
    vulkan_image_destroy(v,&d->color);vulkan_image_destroy(v,&d->depth);
    vulkan_image_destroy(v,&d->reciprocal_depth);
    vulkan_image_destroy(v,&d->motion);
    vulkan_image_destroy(v,&d->scene);
    vulkan_buffer_destroy(v,&d->vertices);vulkan_buffer_destroy(v,&d->readback);
    free(d->staging);free(d->batches);memset(d,0,sizeof(*d));
}
int vulkan_draw_begin(VulkanDraw *d,const float clear[4]){
    if(!d->pass || d->recording)return 0;
    d->pending_upload=(VulkanDrawUpload){0};
    d->vertex_count=d->batch_count=0;memcpy(d->clear,clear,sizeof(d->clear));d->recording=1;d->resuming=0;d->clear_reciprocal=d->clear_depth_on_resume=0;return 1;
}
int vulkan_draw_resume(VulkanDraw *d){
    if(d->recording || !d->has_frame)return 0;
    d->vertex_count=d->batch_count=0;d->recording=1;d->resuming=1;return 1;
}
static int depth_copy_pipeline(VulkanDraw *d){
    VulkanDevice *v=d->device;
    VkShaderModuleCreateInfo module={VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    VkPipelineShaderStageCreateInfo stages[2]={{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO},{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO}};
    VkPipelineVertexInputStateCreateInfo input={VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    VkPipelineInputAssemblyStateCreateInfo assembly={VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    VkPipelineViewportStateCreateInfo viewport={VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    VkViewport rect={0,0,(float)d->width,(float)d->height,0,1};VkRect2D scissor={{0,0},{d->width,d->height}};
    VkPipelineRasterizationStateCreateInfo raster={VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    VkPipelineMultisampleStateCreateInfo samples={VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    VkPipelineDepthStencilStateCreateInfo depth={VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    VkPipelineColorBlendAttachmentState blend[4]={{0}};
    VkPipelineColorBlendStateCreateInfo blending={VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    VkGraphicsPipelineCreateInfo pipeline={VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    VkDescriptorSetAllocateInfo allocate={VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    if(d->depth_copy_pipeline)return 1;
    module.codeSize=sizeof(fullscreen_vert_spv);module.pCode=fullscreen_vert_spv;
    VK(v->CreateShaderModule(v->device,&module,NULL,d->depth_copy_shaders));
    module.codeSize=sizeof(depth_copy_frag_spv);module.pCode=depth_copy_frag_spv;
    VK(v->CreateShaderModule(v->device,&module,NULL,d->depth_copy_shaders+1));
    stages[0].stage=VK_SHADER_STAGE_VERTEX_BIT;stages[1].stage=VK_SHADER_STAGE_FRAGMENT_BIT;
    for(unsigned i=0;i<2;i++){stages[i].module=d->depth_copy_shaders[i];stages[i].pName="main";}
    assembly.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    viewport.viewportCount=viewport.scissorCount=1;viewport.pViewports=&rect;viewport.pScissors=&scissor;
    raster.polygonMode=VK_POLYGON_MODE_FILL;raster.lineWidth=1;samples.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
    depth.depthTestEnable=depth.depthWriteEnable=VK_TRUE;depth.depthCompareOp=VK_COMPARE_OP_ALWAYS;
    blending.attachmentCount=4;blending.pAttachments=blend; /* Depth only. */
    pipeline.stageCount=2;pipeline.pStages=stages;pipeline.pVertexInputState=&input;pipeline.pInputAssemblyState=&assembly;
    pipeline.pViewportState=&viewport;pipeline.pRasterizationState=&raster;pipeline.pMultisampleState=&samples;
    pipeline.pDepthStencilState=&depth;pipeline.pColorBlendState=&blending;pipeline.layout=d->layout;pipeline.renderPass=d->pass;
    VK(v->CreateGraphicsPipelines(v->device,VK_NULL_HANDLE,1,&pipeline,NULL,&d->depth_copy_pipeline));
    allocate.descriptorPool=d->descriptors;allocate.descriptorSetCount=1;allocate.pSetLayouts=&d->texture_layout;
    VK(v->AllocateDescriptorSets(v->device,&allocate,&d->depth_copy_set));return 1;
fail:return 0;
}
int vulkan_draw_copy_depth(VulkanDraw *d,const VulkanDraw *source){
    VulkanDevice *v=d->device;VkCommandBuffer command;
    VkImageMemoryBarrier barrier={VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    VkRenderPassBeginInfo begin={VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    const float view[4]={(float)d->width,(float)d->height,source->scene_jitter[0],source->scene_jitter[1]};
    if(!v || source->device!=v || d==source || d->recording || source->recording || !d->has_frame || !source->has_frame)return 0;
    if(!depth_copy_pipeline(d))return 0;
    if(d->depth_copy_source!=source->depth.view){
        VkDescriptorImageInfo image={d->white.sampler,source->depth.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkWriteDescriptorSet write={VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        if(d->depth_copy_source && !vulkan_queue_drain(v))return 0;
        write.dstSet=d->depth_copy_set;write.descriptorCount=1;write.descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;write.pImageInfo=&image;
        v->UpdateDescriptorSets(v->device,1,&write,0,NULL);d->depth_copy_source=source->depth.view;
    }
    if(!vulkan_command_begin(v,&command))return 0;
    barrier.oldLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;barrier.newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask=VK_ACCESS_MEMORY_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
    barrier.srcQueueFamilyIndex=barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;barrier.image=source->depth.image;
    barrier.subresourceRange=(VkImageSubresourceRange){VK_IMAGE_ASPECT_DEPTH_BIT,0,1,0,1};
    v->CmdPipelineBarrier(command,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,0,0,NULL,0,NULL,1,&barrier);
    begin.renderPass=d->resume_pass;begin.framebuffer=d->framebuffer;begin.renderArea.extent=(VkExtent2D){d->width,d->height};
    v->CmdBeginRenderPass(command,&begin,VK_SUBPASS_CONTENTS_INLINE);
    v->CmdBindPipeline(command,VK_PIPELINE_BIND_POINT_GRAPHICS,d->depth_copy_pipeline);
    v->CmdBindDescriptorSets(command,VK_PIPELINE_BIND_POINT_GRAPHICS,d->layout,0,1,&d->depth_copy_set,0,NULL);
    v->CmdPushConstants(command,d->layout,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(view),view);
    v->CmdDraw(command,3,1,0,0);v->CmdEndRenderPass(command);
    barrier.oldLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;barrier.newLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    barrier.srcAccessMask=VK_ACCESS_SHADER_READ_BIT;barrier.dstAccessMask=VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    v->CmdPipelineBarrier(command,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT|VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,0,0,NULL,0,NULL,1,&barrier);
    if(!vulkan_command_submit(v,command,VK_NULL_HANDLE,VK_NULL_HANDLE))return 0;
    d->clear_depth_on_resume=0;return 1;
}
int vulkan_draw_blit_color(VulkanDraw *d,const VulkanImage *source,uint32_t width,uint32_t height){
    VulkanDevice *v=d->device;VkCommandBuffer command;VkImageBlit copy={0};
    VkImageMemoryBarrier b={VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    if(d->recording || !d->has_frame || !source || !source->image || source->image==d->color.image || !width || !height)return 0;
    if(!vulkan_command_begin(v,&command))return 0;
    b.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;b.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;b.image=d->color.image;
    b.subresourceRange=(VkImageSubresourceRange){VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
    b.srcAccessMask=VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT;b.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
    v->CmdPipelineBarrier(command,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,NULL,0,NULL,1,&b);
    copy.srcSubresource=copy.dstSubresource=(VkImageSubresourceLayers){VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};
    copy.srcOffsets[1]=(VkOffset3D){(int32_t)width,(int32_t)height,1};
    copy.dstOffsets[1]=(VkOffset3D){(int32_t)d->width,(int32_t)d->height,1};
    v->CmdBlitImage(command,source->image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,d->color.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&copy,VK_FILTER_NEAREST);
    b.oldLayout=b.newLayout;b.newLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    b.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;b.dstAccessMask=VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT;
    v->CmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,0,0,NULL,0,NULL,1,&b);
    return vulkan_command_submit(v,command,VK_NULL_HANDLE,VK_NULL_HANDLE);
}
int vulkan_draw_reset_reciprocal_depth(VulkanDraw *d){
    size_t i;if(!d->recording)return 0;
    for(i=0;i<d->batch_count;i++)if(d->batches[i].flags&CAPTURE_SCENE)return 0;
    d->clear_reciprocal=1;return 1;
}
static void *grow(void *data,size_t *capacity,size_t needed,size_t element){
    size_t count;void *p;if(needed<=*capacity)return data;
    count=*capacity?*capacity:64;while(count<needed){if(count>SIZE_MAX/2)return NULL;count*=2;}
    if(count>SIZE_MAX/element)return NULL;
    p=realloc(data,count*element);if(!p)return NULL;*capacity=count;return p;
}
int vulkan_draw_projection(VulkanDraw *d,float near_z,float numerator,float jitter_x,float jitter_y){
    float scale;
    if(!isfinite(near_z) || near_z<=0 || !isfinite(numerator) || numerator<=0 ||
       !isfinite(jitter_x) || !isfinite(jitter_y) ||
       fabsf(jitter_x)>0.5f || fabsf(jitter_y)>0.5f)return 0;
    scale=near_z/numerator;if(!isfinite(scale) || scale<=0)return 0;
    d->scene_depth_scale=scale;d->scene_jitter[0]=jitter_x;d->scene_jitter[1]=jitter_y;return 1;
}
static int draw_primitives(VulkanDraw *d,const VulkanDrawVertex *vertices,uint32_t count,VulkanDrawTexture *texture,uint32_t flags){
    VkDescriptorSet descriptor;VulkanDrawBatch *last,*batches;VulkanDrawVertex *staging;
    VulkanDrawMaterial material=d->material;
    if(!d->recording || flags>=192 || !vertices || !count || count>UINT32_MAX-d->vertex_count)return 0;
    if(texture && texture->atlas){
        if(texture->width!=1024 || texture->height!=1024 || !material.padding[1] || material.padding[1]>1024)return 0;
    }else material.padding[1]=0;
    if(d->capture_background || (d->capture_depth && (flags&3)==3))flags|=CAPTURE_RECIPROCAL_DEPTH;
    else if(d->capture_depth || d->capture_scene)flags|=CAPTURE_REACTIVE;
    if(d->capture_depth || d->capture_scene || d->capture_background)flags|=CAPTURE_SCENE;
    /* Blended effects can also write depth. Keep their solid depth/vector
     * output, but reject their changing color just like no-write translucency. */
    material.padding[0]=(flags&CAPTURE_REACTIVE?1u:0u)|
        (flags&VULKAN_DRAW_BLEND?2u:0u)|
        (d->capture_background?4u:0u);
    material.depth_scale=d->scene_depth_scale;
    if(!d->capture_background)material.background_valid=0;
    material.jitter[0]=flags&CAPTURE_SCENE?d->scene_jitter[0]:0;
    material.jitter[1]=flags&CAPTURE_SCENE?d->scene_jitter[1]:0;
    descriptor=texture?texture_binding(d,texture):d->white.descriptor;if(!descriptor)return 0;
    staging=grow(d->staging,&d->vertex_capacity,d->vertex_count+count,sizeof(*vertices));if(!staging)return 0;d->staging=staging;
    batches=grow(d->batches,&d->batch_capacity,d->batch_count+1,sizeof(*last));if(!batches)return 0;d->batches=batches;
    memcpy(d->staging+d->vertex_count,vertices,count*sizeof(*vertices));
    last=d->batch_count?d->batches+d->batch_count-1:NULL;
    {VkDescriptorSet mask=d->mask_texture?d->mask_texture->descriptor:d->white.descriptor;
    if(last && last->texture==descriptor && last->mask==mask && last->flags==flags && !memcmp(&last->material,&material,sizeof(material)))last->count+=count;
    else d->batches[d->batch_count++]=(VulkanDrawBatch){(uint32_t)d->vertex_count,count,flags,descriptor,mask,material};}
    d->vertex_count+=count;return 1;
}
int vulkan_draw_triangles(VulkanDraw *d,const VulkanDrawVertex *v,uint32_t count,VulkanDrawTexture *t,uint32_t flags){
    return flags<64 && !(count%3) && draw_primitives(d,v,count,t,flags);
}
int vulkan_draw_points(VulkanDraw *d,const VulkanDrawVertex *v,uint32_t count,VulkanDrawTexture *t,uint32_t flags){
    return flags<64 && draw_primitives(d,v,count,t,flags|VULKAN_DRAW_POINTS);
}
int vulkan_draw_lines(VulkanDraw *d,const VulkanDrawVertex *v,uint32_t count,VulkanDrawTexture *t,uint32_t flags){
    return flags<64 && !(count%2) && draw_primitives(d,v,count,t,flags|VULKAN_DRAW_LINES);
}
int vulkan_draw_finish(VulkanDraw *d,uint8_t *rgba,size_t bytes){
    VulkanDevice *v=d->device;VkCommandBuffer command;VkDeviceSize offset=0;size_t i,vertex_bytes=d->vertex_count*sizeof(*d->staging);
    VkRenderPassBeginInfo begin={VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};VkClearValue clear[5]={0};
    VkBufferImageCopy copy={0};VkBufferMemoryBarrier host={VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    if(!d->recording || (rgba && bytes<(size_t)d->width*d->height*4))return 0;
    VulkanDrawUpload upload=d->pending_upload;
    d->pending_upload=(VulkanDrawUpload){0};d->recording=0;
    for(i=0;i<d->batch_count;i++)TRY(draw_pipeline(d,d->batches[i].flags));
    if(vertex_bytes>d->vertices.size){vulkan_buffer_destroy(v,&d->vertices);TRY(vulkan_buffer_create(v,&d->vertices,d->vertex_capacity*sizeof(*d->staging),VK_BUFFER_USAGE_VERTEX_BUFFER_BIT));}
    if(vertex_bytes)TRY(vulkan_buffer_write(v,&d->vertices,d->staging,vertex_bytes));
    if(upload.texture){
        if(!upload.pixels || !upload.width || !upload.height || upload.width>upload.texture->width || upload.height>upload.texture->height)goto fail;
        TRY(vulkan_buffer_write(v,&upload.texture->upload,upload.pixels,(size_t)upload.width*upload.height*4));
    }
    TRY(vulkan_command_begin(v,&command));memcpy(clear[0].color.float32,d->clear,sizeof(d->clear));clear[1].depthStencil.depth=d->clear_depth;
    if(upload.texture)texture_copy_record(v,command,upload.texture,0,0,upload.width,upload.height);
    clear[3].color.float32[3]=1;
    begin.renderPass=d->resuming?d->resume_pass:d->pass;begin.framebuffer=d->framebuffer;begin.renderArea.extent=(VkExtent2D){d->width,d->height};begin.clearValueCount=5;begin.pClearValues=clear;
    v->CmdBeginRenderPass(command,&begin,VK_SUBPASS_CONTENTS_INLINE);
    if(d->clear_depth_on_resume){
        VkClearAttachment reset={0};VkClearRect area={{{0,0},{d->width,d->height}},0,1};
        reset.aspectMask=VK_IMAGE_ASPECT_DEPTH_BIT;reset.clearValue.depthStencil.depth=d->clear_depth;
        v->CmdClearAttachments(command,1,&reset,1,&area);d->clear_depth_on_resume=0;
    }
    if(d->clear_reciprocal){
        VkClearAttachment reset[3]={{0}};VkClearRect area={{{0,0},{d->width,d->height}},0,1};
        reset[0].aspectMask=reset[1].aspectMask=reset[2].aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
        reset[0].colorAttachment=1;reset[1].colorAttachment=2;reset[1].clearValue.color.float32[3]=1;
        reset[2].colorAttachment=3;
        v->CmdClearAttachments(command,3,reset,1,&area);d->clear_reciprocal=0;
    }
    if(vertex_bytes)v->CmdBindVertexBuffers(command,0,1,&d->vertices.buffer,&offset);
    for(i=0;i<d->batch_count;i++){
        VulkanDrawBatch *b=d->batches+i,*previous=i?b-1:NULL;
        /* All pipelines share this owner's layout. Keep draw order, but retain
         * compatible state across adjacent batches. First batch binds everything
         * anew: no state is borrowed from another command buffer or render pass. */
        if(!previous || previous->flags!=b->flags){
            v->CmdBindPipeline(command,VK_PIPELINE_BIND_POINT_GRAPHICS,d->pipelines[b->flags]);d->pipeline_binds++;
        }
        if(!previous || memcmp(&previous->material,&b->material,sizeof(b->material))){
            v->CmdPushConstants(command,d->layout,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(b->material),&b->material);
            d->material_pushes++;
        }
        VkDescriptorSet sets[2]={b->texture,b->mask};
        if(!previous || previous->texture!=b->texture || previous->mask!=b->mask){
            v->CmdBindDescriptorSets(command,VK_PIPELINE_BIND_POINT_GRAPHICS,d->layout,0,2,sets,0,NULL);d->descriptor_binds++;
        }
        v->CmdDraw(command,b->count,1,b->first,0);
    }
    v->CmdEndRenderPass(command);
    if(rgba){
        copy.imageSubresource=(VkImageSubresourceLayers){VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent=(VkExtent3D){d->width,d->height,1};
        v->CmdCopyImageToBuffer(command,d->color.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,d->readback.buffer,1,&copy);
        host.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
        host.srcQueueFamilyIndex=host.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;host.buffer=d->readback.buffer;host.size=VK_WHOLE_SIZE;
        v->CmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,NULL,1,&host,0,NULL);
    }
    TRY(vulkan_command_submit(v,command,VK_NULL_HANDLE,VK_NULL_HANDLE));
    if(rgba)TRY(vulkan_buffer_read(v,&d->readback,rgba,(size_t)d->width*d->height*4));
    d->has_frame=1;return 1;
fail:return 0;
}
static int draw_read_image(VulkanDraw *d,VkImage image,void *pixels,size_t bytes,size_t pixel_size){
    VulkanDevice *v=d->device;VkCommandBuffer command;VkBufferImageCopy copy={0};
    VkBufferMemoryBarrier host={VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    if(d->recording || !d->has_frame || !pixels || bytes<(size_t)d->width*d->height*pixel_size)return 0;
    TRY(vulkan_command_begin(v,&command));
    copy.imageSubresource=(VkImageSubresourceLayers){VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent=(VkExtent3D){d->width,d->height,1};
    v->CmdCopyImageToBuffer(command,image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,d->readback.buffer,1,&copy);
    host.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
    host.srcQueueFamilyIndex=host.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;host.buffer=d->readback.buffer;host.size=VK_WHOLE_SIZE;
    v->CmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,NULL,1,&host,0,NULL);
    TRY(vulkan_command_submit(v,command,VK_NULL_HANDLE,VK_NULL_HANDLE));
    return vulkan_buffer_read(v,&d->readback,pixels,(size_t)d->width*d->height*pixel_size);
fail:return 0;
}
int vulkan_draw_readback(VulkanDraw *d,uint8_t *rgba,size_t bytes){return draw_read_image(d,d->color.image,rgba,bytes,4);}
int vulkan_draw_read_scene(VulkanDraw *d,uint8_t *rgba,size_t bytes){return draw_read_image(d,d->scene.image,rgba,bytes,4);}
int vulkan_draw_read_reciprocal_depth(VulkanDraw *d,float *depth,size_t bytes){return draw_read_image(d,d->reciprocal_depth.image,depth,bytes,4);}
int vulkan_draw_read_motion(VulkanDraw *d,float *motion,size_t bytes){return draw_read_image(d,d->motion.image,motion,bytes,16);}
