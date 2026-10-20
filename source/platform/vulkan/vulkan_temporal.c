#include "vulkan_temporal.h"
#include "temporal_mask_spv.h"
#include <stdio.h>
#include <string.h>
#define CHECK(call) do {VkResult result=(call);if(result!=VK_SUCCESS){fprintf(stderr,"Vulkan temporal %s: %d\n",#call,(int)result);goto fail;}}while(0)
int vulkan_temporal_open(VulkanTemporal *t,VulkanDevice *v,uint32_t width,uint32_t height){
    VkDescriptorSetLayoutBinding bindings[2]={{0}};
    VkDescriptorSetLayoutCreateInfo descriptors={VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    VkDescriptorPoolSize sizes[2]={{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1},{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1}};
    VkDescriptorPoolCreateInfo pool={VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    VkDescriptorSetAllocateInfo allocation={VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    VkSamplerCreateInfo sampler={VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    VkPipelineLayoutCreateInfo layout={VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    VkShaderModuleCreateInfo module={VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};VkShaderModule shader=VK_NULL_HANDLE;
    VkComputePipelineCreateInfo pipeline={VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    if(t->device)return 0;
    memset(t,0,sizeof(*t));t->device=v;t->width=width;t->height=height;
    if(!width || !height || !vulkan_image_create(v,&t->rejection,VK_FORMAT_R8_UNORM,
       VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT,width,height))goto fail;
    bindings[0].binding=0;bindings[0].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[1].binding=1;bindings[1].descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[0].descriptorCount=bindings[1].descriptorCount=1;
    bindings[0].stageFlags=bindings[1].stageFlags=VK_SHADER_STAGE_COMPUTE_BIT;
    descriptors.bindingCount=2;descriptors.pBindings=bindings;
    CHECK(v->CreateDescriptorSetLayout(v->device,&descriptors,NULL,&t->descriptors));
    pool.maxSets=1;pool.poolSizeCount=2;pool.pPoolSizes=sizes;
    CHECK(v->CreateDescriptorPool(v->device,&pool,NULL,&t->pool));
    allocation.descriptorPool=t->pool;allocation.descriptorSetCount=1;allocation.pSetLayouts=&t->descriptors;
    CHECK(v->AllocateDescriptorSets(v->device,&allocation,&t->set));
    sampler.magFilter=sampler.minFilter=VK_FILTER_NEAREST;sampler.mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sampler.addressModeU=sampler.addressModeV=sampler.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    CHECK(v->CreateSampler(v->device,&sampler,NULL,&t->sampler));
    layout.setLayoutCount=1;layout.pSetLayouts=&t->descriptors;
    CHECK(v->CreatePipelineLayout(v->device,&layout,NULL,&t->layout));
    module.codeSize=sizeof(temporal_mask_spv);module.pCode=temporal_mask_spv;
    CHECK(v->CreateShaderModule(v->device,&module,NULL,&shader));
    pipeline.layout=t->layout;pipeline.stage.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipeline.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT;pipeline.stage.module=shader;pipeline.stage.pName="main";
    CHECK(v->CreateComputePipelines(v->device,VK_NULL_HANDLE,1,&pipeline,NULL,&t->pipeline));
    v->DestroyShaderModule(v->device,shader,NULL);return 1;
fail:if(shader)v->DestroyShaderModule(v->device,shader,NULL);vulkan_temporal_close(t);return 0;
}
void vulkan_temporal_close(VulkanTemporal *t){
    VulkanDevice *v=t->device;if(!v)return;
    if(t->pipeline)v->DestroyPipeline(v->device,t->pipeline,NULL);
    if(t->layout)v->DestroyPipelineLayout(v->device,t->layout,NULL);
    if(t->pool)v->DestroyDescriptorPool(v->device,t->pool,NULL);
    if(t->descriptors)v->DestroyDescriptorSetLayout(v->device,t->descriptors,NULL);
    if(t->sampler)v->DestroySampler(v->device,t->sampler,NULL);
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
    VulkanDevice *v=t->device;VkDescriptorImageInfo images[2]={{0}};VkWriteDescriptorSet writes[2]={{0}};unsigned i;
    if(!t->pipeline || !command || !motion || !motion->image || !motion->view)return 0;
    images[0].sampler=t->sampler;images[0].imageView=motion->view;images[0].imageLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    images[1].imageView=t->rejection.view;images[1].imageLayout=VK_IMAGE_LAYOUT_GENERAL;
    for(i=0;i<2;i++){
        writes[i].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;writes[i].dstSet=t->set;writes[i].dstBinding=i;
        writes[i].descriptorCount=1;writes[i].descriptorType=i?VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[i].pImageInfo=images+i;
    }
    v->UpdateDescriptorSets(v->device,2,writes,0,NULL);
    transition(v,command,motion->image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    transition(v,command,t->rejection.image,t->initialized?VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_GENERAL);
    v->CmdBindPipeline(command,VK_PIPELINE_BIND_POINT_COMPUTE,t->pipeline);
    v->CmdBindDescriptorSets(command,VK_PIPELINE_BIND_POINT_COMPUTE,t->layout,0,1,&t->set,0,NULL);
    v->CmdDispatch(command,(t->width+7)/8,(t->height+7)/8,1);
    transition(v,command,motion->image,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    transition(v,command,t->rejection.image,VK_IMAGE_LAYOUT_GENERAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    t->initialized=1;return 1;
}
