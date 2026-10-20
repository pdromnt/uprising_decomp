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
    VulkanImage cropped[3];int crop_initialized;
    VkDescriptorSetLayout composition_descriptors;VkDescriptorSet composition_set;
    VkPipelineLayout composition_layout;VkPipeline composition_pipeline;
} VulkanTemporal;
/* Zero-initialize the owner; close it before reopening. */
int vulkan_temporal_open(VulkanTemporal *,VulkanDevice *,uint32_t width,uint32_t height);
void vulkan_temporal_close(VulkanTemporal *);
/* Borrow completed RGBA32F motion in TRANSFER_SRC_OPTIMAL. Record extraction
 * and restore it; the owned R8 mask also ends in TRANSFER_SRC_OPTIMAL. Caller
 * submits/waits before reusing descriptors, closing or reading the images. */
int vulkan_temporal_record(VulkanTemporal *,VkCommandBuffer,const VulkanImage *motion);
/* Optional exact-pixel crop of completed color/depth/motion, preserving pixel
 * displacement vectors. Allocated once at open, on request, never per frame. */
int vulkan_temporal_crop_open(VulkanTemporal *);
int vulkan_temporal_crop(VulkanTemporal *,VkCommandBuffer,const VulkanImage *color,
    const VulkanImage *depth,const VulkanImage *motion,uint32_t x,uint32_t y,uint32_t source_width,uint32_t source_height);
/* Compose temporal crop into a completed full-screen color target. Source
 * images and output enter/leave TRANSFER_SRC_OPTIMAL. Coverage and retained
 * pre-scene artwork come from the actual matching native MRTs. */
int vulkan_temporal_compose(VulkanTemporal *,VkCommandBuffer,const VulkanImage *upscaled,
    const VulkanImage *scene,const VulkanImage *native_final,const VulkanImage *output,uint32_t x,uint32_t y,uint32_t output_width,uint32_t output_height);
int vulkan_temporal_antialias(VulkanTemporal *,VkCommandBuffer,const VulkanImage *scene,
    const VulkanImage *native_final,const VulkanImage *output,uint32_t x,uint32_t y,uint32_t output_width,uint32_t output_height);
#endif
