#ifndef UPRISING_VULKAN_TEMPORAL_H
#define UPRISING_VULKAN_TEMPORAL_H
#include "vulkan_device.h"

/* Renderer-owned GPU extraction, independent of the SDK. No readback or
 * allocation per frame. One conservative mask serves reactive and composition
 * rejection until separate sky/particle history is available. */
typedef struct VulkanTemporal {
    VulkanDevice *device;uint32_t width,height;int initialized;
    VulkanImage rejection;VkSampler sampler;VkDescriptorPool pool;
    VkDescriptorSetLayout descriptors;VkDescriptorSet set;
    VkPipelineLayout layout;VkPipeline pipeline;
} VulkanTemporal;
/* Zero-initialize the owner; close it before reopening. */
int vulkan_temporal_open(VulkanTemporal *,VulkanDevice *,uint32_t width,uint32_t height);
void vulkan_temporal_close(VulkanTemporal *);
/* Borrow completed RGBA32F motion in TRANSFER_SRC_OPTIMAL. Record extraction
 * and restore it; the owned R8 mask also ends in TRANSFER_SRC_OPTIMAL. Caller
 * submits/waits before reusing descriptors, closing or reading the images. */
int vulkan_temporal_record(VulkanTemporal *,VkCommandBuffer,const VulkanImage *motion);
#endif
