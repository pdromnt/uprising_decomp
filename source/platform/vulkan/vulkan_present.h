#ifndef UPRISING_VULKAN_PRESENT_H
#define UPRISING_VULKAN_PRESENT_H
#include "vulkan_device.h"
#include "vulkan_spatial.h"
typedef struct VulkanPresent {
    VulkanDevice *device;SDL_Window *window;
    VkSwapchainKHR swapchain;VkImage *images;uint32_t count;
    VkExtent2D extent;VkSemaphore acquired,finished;int recreate;
    VulkanSpatial spatial;
} VulkanPresent;
int vulkan_present_open(VulkanPresent *,VulkanDevice *,SDL_Window *);
void vulkan_present_close(VulkanPresent *);
/* Source must be RGBA8 in TRANSFER_SRC_OPTIMAL, with completed drawing.
 * Preserve aspect ratio and black borders; spatially filter scaled pixels.
 * A minimized/out-of-date surface skips a frame and rebuilds on restoration. */
int vulkan_present_image(VulkanPresent *,const VulkanImage *,uint32_t width,uint32_t height);
#endif
