#ifndef UPRISING_VULKAN_SPATIAL_H
#define UPRISING_VULKAN_SPATIAL_H
#include "vulkan_device.h"
/* Presentation-only spatial scaling. No temporal history or readback. */
typedef struct VulkanSpatial {
    VulkanDevice *device;uint32_t width,height;int initialized;
    VulkanImage output;VkSampler sampler;VkDescriptorPool pool;
    VkDescriptorSetLayout descriptors;VkDescriptorSet set;
    VkPipelineLayout layout;VkPipeline pipeline;
} VulkanSpatial;
int vulkan_spatial_open(VulkanSpatial *,VulkanDevice *,uint32_t,uint32_t);
void vulkan_spatial_close(VulkanSpatial *);
/* Caller retires earlier uses of this descriptor before recording again.
 * Both source and output finish in TRANSFER_SRC_OPTIMAL. */
int vulkan_spatial_record(VulkanSpatial *,VkCommandBuffer,const VulkanImage *);
#endif
