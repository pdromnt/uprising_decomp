#ifndef UPRISING_VULKAN_PRESENT_H
#define UPRISING_VULKAN_PRESENT_H
#include "vulkan_device.h"
typedef struct VulkanPresent {
    VulkanDevice *device;SDL_Window *window;
    VkSwapchainKHR swapchain;VkImage *images;uint32_t count;
    VkExtent2D extent;VkSemaphore acquired,finished;int recreate;
} VulkanPresent;
int vulkan_present_open(VulkanPresent *,VulkanDevice *,SDL_Window *);
void vulkan_present_close(VulkanPresent *);
/* Source must be RGBA8 in TRANSFER_SRC_OPTIMAL, with completed drawing.
 * Preserve aspect ratio, clear unused area black, and use nearest blitting.
 * A minimized/out-of-date surface skips a frame and rebuilds on restoration. */
int vulkan_present_image(VulkanPresent *,VkImage,uint32_t width,uint32_t height);
#endif
