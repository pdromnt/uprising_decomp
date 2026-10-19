#ifndef UPRISING_FSR3_UPSCALER_H
#define UPRISING_FSR3_UPSCALER_H
#include "vulkan_device.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct Fsr3Upscaler Fsr3Upscaler;
/* Renderer-thread only; the upstream backend has one global device binding. */
Fsr3Upscaler *fsr3_upscaler_open(VulkanDevice *,uint32_t width,uint32_t height,uint32_t output_width,uint32_t output_height);
void fsr3_upscaler_close(Fsr3Upscaler *);
/* Borrow rendered inputs in TRANSFER_SRC_OPTIMAL, restore them after dispatch.
 * Output remains renderer-owned RGBA32F in TRANSFER_SRC_OPTIMAL. Submit/wait
 * before destroying the context; commands use the caller's Vulkan device. */
const VulkanImage *fsr3_upscaler_dispatch(Fsr3Upscaler *,VkCommandBuffer,
    const VulkanImage *color,const VulkanImage *depth,const VulkanImage *motion,
    float jitter_x,float jitter_y,float elapsed_ms,float near_z,float vertical_fov,int reset,int sharpen);
#ifdef __cplusplus
}
#endif
#endif
