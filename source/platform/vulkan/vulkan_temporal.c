#include "vulkan_temporal.h"
#include "temporal_mask_spv.h"
#include "temporal_compose_spv.h"
#include <stdio.h>
#include <string.h>
#define CHECK(call) do {VkResult result=(call);if(result!=VK_SUCCESS){fprintf(stderr,"Vulkan temporal %s: %d\n",#call,(int)result);goto fail;}}while(0)
int vulkan_temporal_open(VulkanTemporal *t,VulkanDevice *v,uint32_t width,uint32_t height){
    VkDescriptorSetLayoutBinding bindings[4]={{0}};
    VkDescriptorSetLayoutCreateInfo descriptors={VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    VkDescriptorPoolSize sizes[2]={{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,5},{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,3}};
    VkDescriptorPoolCreateInfo pool={VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    VkDescriptorSetAllocateInfo allocation={VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    VkSamplerCreateInfo sampler={VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    VkPipelineLayoutCreateInfo layout={VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    VkShaderModuleCreateInfo module={VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};VkShaderModule shader=VK_NULL_HANDLE;
    VkComputePipelineCreateInfo pipeline={VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    VkPushConstantRange view={VK_SHADER_STAGE_COMPUTE_BIT,0,32};unsigned i;
    if(t->device)return 0;
    memset(t,0,sizeof(*t));t->device=v;t->width=width;t->height=height;
    if(!width || !height || !vulkan_image_create(v,&t->rejection,VK_FORMAT_R8_UNORM,
       VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT,width,height))goto fail;
    for(i=0;i<2;i++)if(!vulkan_image_create(v,t->coverage_history+i,VK_FORMAT_R8_UNORM,
        VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT,width,height))goto fail;
    for(i=0;i<4;i++){
        bindings[i].binding=i;bindings[i].descriptorCount=1;
        bindings[i].descriptorType=(i&1)?VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[i].stageFlags=VK_SHADER_STAGE_COMPUTE_BIT;
    }
    descriptors.bindingCount=4;descriptors.pBindings=bindings;
    CHECK(v->CreateDescriptorSetLayout(v->device,&descriptors,NULL,&t->descriptors));
    pool.maxSets=2;pool.poolSizeCount=2;pool.pPoolSizes=sizes;
    CHECK(v->CreateDescriptorPool(v->device,&pool,NULL,&t->pool));
    allocation.descriptorPool=t->pool;allocation.descriptorSetCount=1;allocation.pSetLayouts=&t->descriptors;
    CHECK(v->AllocateDescriptorSets(v->device,&allocation,&t->set));
    sampler.magFilter=sampler.minFilter=VK_FILTER_NEAREST;sampler.mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sampler.addressModeU=sampler.addressModeV=sampler.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    CHECK(v->CreateSampler(v->device,&sampler,NULL,&t->sampler));
    layout.setLayoutCount=1;layout.pSetLayouts=&t->descriptors;
    view.size=16;layout.pushConstantRangeCount=1;layout.pPushConstantRanges=&view;
    CHECK(v->CreatePipelineLayout(v->device,&layout,NULL,&t->layout));
    module.codeSize=sizeof(temporal_mask_spv);module.pCode=temporal_mask_spv;
    CHECK(v->CreateShaderModule(v->device,&module,NULL,&shader));
    pipeline.layout=t->layout;pipeline.stage.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipeline.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT;pipeline.stage.module=shader;pipeline.stage.pName="main";
    CHECK(v->CreateComputePipelines(v->device,VK_NULL_HANDLE,1,&pipeline,NULL,&t->pipeline));
    v->DestroyShaderModule(v->device,shader,NULL);shader=VK_NULL_HANDLE;
    for(i=0;i<4;i++){
        bindings[i].binding=i;bindings[i].descriptorCount=1;bindings[i].stageFlags=VK_SHADER_STAGE_COMPUTE_BIT;
        bindings[i].descriptorType=i==3?VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    }
    descriptors.bindingCount=4;
    CHECK(v->CreateDescriptorSetLayout(v->device,&descriptors,NULL,&t->composition_descriptors));
    allocation.pSetLayouts=&t->composition_descriptors;
    CHECK(v->AllocateDescriptorSets(v->device,&allocation,&t->composition_set));
    view.size=32;layout.pSetLayouts=&t->composition_descriptors;layout.pushConstantRangeCount=1;layout.pPushConstantRanges=&view;
    CHECK(v->CreatePipelineLayout(v->device,&layout,NULL,&t->composition_layout));
    module.codeSize=sizeof(temporal_compose_spv);module.pCode=temporal_compose_spv;
    CHECK(v->CreateShaderModule(v->device,&module,NULL,&shader));
    pipeline.layout=t->composition_layout;pipeline.stage.module=shader;
    CHECK(v->CreateComputePipelines(v->device,VK_NULL_HANDLE,1,&pipeline,NULL,&t->composition_pipeline));
    v->DestroyShaderModule(v->device,shader,NULL);return 1;
fail:if(shader)v->DestroyShaderModule(v->device,shader,NULL);vulkan_temporal_close(t);return 0;
}
void vulkan_temporal_close(VulkanTemporal *t){
    if(t->device)vulkan_queue_drain(t->device);
    VulkanDevice *v=t->device;if(!v)return;
    if(t->pipeline)v->DestroyPipeline(v->device,t->pipeline,NULL);
    if(t->composition_pipeline)v->DestroyPipeline(v->device,t->composition_pipeline,NULL);
    if(t->composition_layout)v->DestroyPipelineLayout(v->device,t->composition_layout,NULL);
    if(t->layout)v->DestroyPipelineLayout(v->device,t->layout,NULL);
    if(t->pool)v->DestroyDescriptorPool(v->device,t->pool,NULL);
    if(t->descriptors)v->DestroyDescriptorSetLayout(v->device,t->descriptors,NULL);
    if(t->composition_descriptors)v->DestroyDescriptorSetLayout(v->device,t->composition_descriptors,NULL);
    if(t->sampler)v->DestroySampler(v->device,t->sampler,NULL);
    for(unsigned i=0;i<3;i++)vulkan_image_destroy(v,t->cropped+i);
    for(unsigned i=0;i<2;i++)vulkan_image_destroy(v,t->coverage_history+i);
    vulkan_image_destroy(v,&t->rejection);memset(t,0,sizeof(*t));
}
static void transition(VulkanDevice *v,VkCommandBuffer command,VkImage image,VkImageLayout from,VkImageLayout to){
    VkImageMemoryBarrier b={VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    b.oldLayout=from;b.newLayout=to;b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
    b.image=image;b.subresourceRange=(VkImageSubresourceRange){VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
    b.srcAccessMask=from==VK_IMAGE_LAYOUT_UNDEFINED?0:VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT;
    b.dstAccessMask=VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT;
    v->CmdPipelineBarrier(command,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,0,0,NULL,0,NULL,1,&b);
}
int vulkan_temporal_record(VulkanTemporal *t,VkCommandBuffer command,const VulkanImage *motion){
    VulkanDevice *v=t->device;VkDescriptorImageInfo images[4]={{0}};VkWriteDescriptorSet writes[4]={{0}};unsigned i;
    unsigned current=t->initialized?(t->coverage_index^1u):0,previous=current^1u;
    const int32_t history[4]={t->initialized,0,0,0};
    if(!t->pipeline || !command || !motion || !motion->image || !motion->view)return 0;
    images[0].sampler=t->sampler;images[0].imageView=motion->view;images[0].imageLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    images[1].imageView=t->rejection.view;images[1].imageLayout=VK_IMAGE_LAYOUT_GENERAL;
    images[2]=(VkDescriptorImageInfo){t->sampler,t->coverage_history[previous].view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    images[3]=(VkDescriptorImageInfo){VK_NULL_HANDLE,t->coverage_history[current].view,VK_IMAGE_LAYOUT_GENERAL};
    for(i=0;i<4;i++){
        writes[i].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;writes[i].dstSet=t->set;writes[i].dstBinding=i;
        writes[i].descriptorCount=1;writes[i].descriptorType=(i&1)?VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[i].pImageInfo=images+i;
    }
    v->UpdateDescriptorSets(v->device,4,writes,0,NULL);
    transition(v,command,motion->image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    transition(v,command,t->rejection.image,t->initialized?VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_GENERAL);
    transition(v,command,t->coverage_history[previous].image,t->initialized?VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    transition(v,command,t->coverage_history[current].image,t->initialized?VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_GENERAL);
    v->CmdBindPipeline(command,VK_PIPELINE_BIND_POINT_COMPUTE,t->pipeline);
    v->CmdBindDescriptorSets(command,VK_PIPELINE_BIND_POINT_COMPUTE,t->layout,0,1,&t->set,0,NULL);
    v->CmdPushConstants(command,t->layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(history),history);
    v->CmdDispatch(command,(t->width+7)/8,(t->height+7)/8,1);
    transition(v,command,motion->image,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    transition(v,command,t->rejection.image,VK_IMAGE_LAYOUT_GENERAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    transition(v,command,t->coverage_history[previous].image,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    transition(v,command,t->coverage_history[current].image,VK_IMAGE_LAYOUT_GENERAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    t->coverage_index=current;t->initialized=1;return 1;
}
int vulkan_temporal_crop_open(VulkanTemporal *t){
    static const VkFormat formats[3]={VK_FORMAT_R8G8B8A8_UNORM,VK_FORMAT_R32_SFLOAT,VK_FORMAT_R32G32B32A32_SFLOAT};
    unsigned i;if(!t->device || t->cropped[0].image)return 0;
    for(i=0;i<3;i++)if(!vulkan_image_create(t->device,t->cropped+i,formats[i],
        VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,t->width,t->height))return 0;
    return 1;
}
int vulkan_temporal_crop(VulkanTemporal *t,VkCommandBuffer command,const VulkanImage *color,
        const VulkanImage *depth,const VulkanImage *motion,uint32_t x,uint32_t y,uint32_t source_width,uint32_t source_height){
    const VulkanImage *inputs[3]={color,depth,motion};VulkanDevice *v=t->device;VkImageCopy copy={0};unsigned i;
    if(!command || !t->cropped[2].image || x>source_width || y>source_height || t->width>source_width-x || t->height>source_height-y)return 0;
    for(i=0;i<3;i++)if(!inputs[i] || !inputs[i]->image)return 0;
    copy.srcSubresource=copy.dstSubresource=(VkImageSubresourceLayers){VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};
    copy.srcOffset=(VkOffset3D){(int32_t)x,(int32_t)y,0};copy.extent=(VkExtent3D){t->width,t->height,1};
    for(i=0;i<3;i++){
        transition(v,command,t->cropped[i].image,t->crop_initialized?VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        v->CmdCopyImage(command,inputs[i]->image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,t->cropped[i].image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&copy);
        transition(v,command,t->cropped[i].image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    }
    t->crop_initialized=1;return 1;
}
static int compose(VulkanTemporal *t,VkCommandBuffer command,const VulkanImage *upscaled,
        const VulkanImage *scene,const VulkanImage *native_final,const VulkanImage *output,uint32_t x,uint32_t y,uint32_t output_width,uint32_t output_height,int spatial){
    const VulkanImage *inputs[3]={upscaled,scene,native_final};VulkanDevice *v=t->device;
    VkDescriptorImageInfo images[4]={{0}};VkWriteDescriptorSet writes[4]={{0}};unsigned i;
    int32_t crop[8]={(int32_t)x,(int32_t)y,(int32_t)t->width,(int32_t)t->height,spatial,0,0,0};
    if(!command || !t->composition_pipeline || !output || !output->image || !output_width || !output_height ||
       x>INT32_MAX || y>INT32_MAX || t->width>INT32_MAX || t->height>INT32_MAX)return 0;
    for(i=0;i<3;i++){
        if(!inputs[i] || !inputs[i]->view || inputs[i]->image==output->image)return 0;
        images[i]=(VkDescriptorImageInfo){t->sampler,inputs[i]->view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    }
    if((!spatial && inputs[0]->image==inputs[1]->image) || inputs[0]->image==inputs[2]->image || inputs[1]->image==inputs[2]->image)return 0;
    images[3]=(VkDescriptorImageInfo){VK_NULL_HANDLE,output->view,VK_IMAGE_LAYOUT_GENERAL};
    for(i=0;i<4;i++){
        writes[i].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;writes[i].dstSet=t->composition_set;writes[i].dstBinding=i;
        writes[i].descriptorCount=1;writes[i].descriptorType=i==3?VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[i].pImageInfo=images+i;
    }
    v->UpdateDescriptorSets(v->device,4,writes,0,NULL);
    for(i=spatial?1:0;i<3;i++)transition(v,command,inputs[i]->image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    transition(v,command,output->image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_LAYOUT_GENERAL);
    v->CmdBindPipeline(command,VK_PIPELINE_BIND_POINT_COMPUTE,t->composition_pipeline);
    v->CmdBindDescriptorSets(command,VK_PIPELINE_BIND_POINT_COMPUTE,t->composition_layout,0,1,&t->composition_set,0,NULL);
    v->CmdPushConstants(command,t->composition_layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(crop),crop);
    v->CmdDispatch(command,(output_width+7)/8,(output_height+7)/8,1);
    for(i=spatial?1:0;i<3;i++)transition(v,command,inputs[i]->image,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    transition(v,command,output->image,VK_IMAGE_LAYOUT_GENERAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    return 1;
}
int vulkan_temporal_compose(VulkanTemporal *t,VkCommandBuffer command,const VulkanImage *upscaled,
        const VulkanImage *scene,const VulkanImage *native_final,const VulkanImage *output,uint32_t x,uint32_t y,uint32_t width,uint32_t height){
    return compose(t,command,upscaled,scene,native_final,output,x,y,width,height,0);
}
int vulkan_temporal_antialias(VulkanTemporal *t,VkCommandBuffer command,const VulkanImage *scene,
        const VulkanImage *native_final,const VulkanImage *output,uint32_t x,uint32_t y,uint32_t width,uint32_t height){
    return compose(t,command,scene,scene,native_final,output,x,y,width,height,1);
}
