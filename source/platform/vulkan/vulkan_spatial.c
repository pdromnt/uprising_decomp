#include "vulkan_spatial.h"
#include "spatial_scale_comp_spv.h"
#include <stdio.h>
#include <string.h>
#define VK(call) do {VkResult r=(call);if(r!=VK_SUCCESS){fprintf(stderr,"Vulkan spatial %s: %d\n",#call,(int)r);goto fail;}}while(0)
int vulkan_spatial_open(VulkanSpatial *s,VulkanDevice *v,uint32_t width,uint32_t height){
    VkDescriptorSetLayoutBinding bindings[2]={{0,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT,NULL},{1,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1,VK_SHADER_STAGE_COMPUTE_BIT,NULL}};
    VkDescriptorSetLayoutCreateInfo descriptors={VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    VkDescriptorPoolSize sizes[2]={{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1},{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1}};
    VkDescriptorPoolCreateInfo pool={VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    VkDescriptorSetAllocateInfo allocation={VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    VkSamplerCreateInfo sampler={VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    VkPipelineLayoutCreateInfo layout={VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    VkShaderModuleCreateInfo module={VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};VkShaderModule shader=VK_NULL_HANDLE;
    VkComputePipelineCreateInfo pipeline={VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    if(s->device)return 0;
    memset(s,0,sizeof(*s));s->device=v;s->width=width;s->height=height;
    if(!width || !height || !vulkan_image_create(v,&s->output,VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT,width,height))goto fail;
    descriptors.bindingCount=2;descriptors.pBindings=bindings;
    VK(v->CreateDescriptorSetLayout(v->device,&descriptors,NULL,&s->descriptors));
    pool.maxSets=1;pool.poolSizeCount=2;pool.pPoolSizes=sizes;
    VK(v->CreateDescriptorPool(v->device,&pool,NULL,&s->pool));
    allocation.descriptorPool=s->pool;allocation.descriptorSetCount=1;allocation.pSetLayouts=&s->descriptors;
    VK(v->AllocateDescriptorSets(v->device,&allocation,&s->set));
    sampler.magFilter=sampler.minFilter=VK_FILTER_NEAREST;sampler.mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sampler.addressModeU=sampler.addressModeV=sampler.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    VK(v->CreateSampler(v->device,&sampler,NULL,&s->sampler));
    layout.setLayoutCount=1;layout.pSetLayouts=&s->descriptors;
    VK(v->CreatePipelineLayout(v->device,&layout,NULL,&s->layout));
    module.codeSize=sizeof(spatial_scale_comp_spv);module.pCode=spatial_scale_comp_spv;
    VK(v->CreateShaderModule(v->device,&module,NULL,&shader));
    pipeline.layout=s->layout;pipeline.stage.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipeline.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT;pipeline.stage.module=shader;pipeline.stage.pName="main";
    VK(v->CreateComputePipelines(v->device,VK_NULL_HANDLE,1,&pipeline,NULL,&s->pipeline));
    v->DestroyShaderModule(v->device,shader,NULL);return 1;
fail:if(shader)v->DestroyShaderModule(v->device,shader,NULL);vulkan_spatial_close(s);return 0;
}
void vulkan_spatial_close(VulkanSpatial *s){
    VulkanDevice *v=s->device;if(!v)return;
    vulkan_queue_drain(v);
    if(s->pipeline)v->DestroyPipeline(v->device,s->pipeline,NULL);
    if(s->layout)v->DestroyPipelineLayout(v->device,s->layout,NULL);
    if(s->pool)v->DestroyDescriptorPool(v->device,s->pool,NULL);
    if(s->descriptors)v->DestroyDescriptorSetLayout(v->device,s->descriptors,NULL);
    if(s->sampler)v->DestroySampler(v->device,s->sampler,NULL);
    vulkan_image_destroy(v,&s->output);memset(s,0,sizeof(*s));
}
static void transition(VulkanDevice *v,VkCommandBuffer command,VkImage image,VkImageLayout from,VkImageLayout to){
    VkImageMemoryBarrier b={VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    b.oldLayout=from;b.newLayout=to;b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;b.image=image;
    b.subresourceRange=(VkImageSubresourceRange){VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
    b.srcAccessMask=from==VK_IMAGE_LAYOUT_UNDEFINED?0:VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT;
    b.dstAccessMask=VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT;
    v->CmdPipelineBarrier(command,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,0,0,NULL,0,NULL,1,&b);
}
int vulkan_spatial_record(VulkanSpatial *s,VkCommandBuffer command,const VulkanImage *source){
    VulkanDevice *v=s->device;VkDescriptorImageInfo images[2];
    VkWriteDescriptorSet writes[2]={{0}};
    if(!s->pipeline || !command || !source || !source->view || source->image==s->output.image)return 0;
    images[0]=(VkDescriptorImageInfo){s->sampler,source->view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    images[1]=(VkDescriptorImageInfo){VK_NULL_HANDLE,s->output.view,VK_IMAGE_LAYOUT_GENERAL};
    for(unsigned i=0;i<2;i++){
        writes[i].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;writes[i].dstSet=s->set;writes[i].dstBinding=i;
        writes[i].descriptorCount=1;writes[i].descriptorType=i?VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;writes[i].pImageInfo=images+i;
    }
    v->UpdateDescriptorSets(v->device,2,writes,0,NULL);
    transition(v,command,source->image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    transition(v,command,s->output.image,s->initialized?VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_GENERAL);
    v->CmdBindPipeline(command,VK_PIPELINE_BIND_POINT_COMPUTE,s->pipeline);
    v->CmdBindDescriptorSets(command,VK_PIPELINE_BIND_POINT_COMPUTE,s->layout,0,1,&s->set,0,NULL);
    v->CmdDispatch(command,(s->width+7)/8,(s->height+7)/8,1);
    transition(v,command,source->image,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    transition(v,command,s->output.image,VK_IMAGE_LAYOUT_GENERAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    s->initialized=1;return 1;
}
