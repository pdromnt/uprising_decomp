#ifndef UPRISING_VULKAN_DEVICE_H
#define UPRISING_VULKAN_DEVICE_H
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#define VK_DEVICE_FUNCTIONS(X) \
 X(DestroyDevice) X(GetDeviceQueue) X(DeviceWaitIdle) X(QueueWaitIdle) \
 X(CreateCommandPool) X(DestroyCommandPool) X(AllocateCommandBuffers) X(FreeCommandBuffers) X(ResetCommandPool) \
 X(BeginCommandBuffer) X(EndCommandBuffer) X(QueueSubmit) X(CreateFence) X(DestroyFence) X(WaitForFences) \
 X(CreateBuffer) X(DestroyBuffer) X(GetBufferMemoryRequirements) X(BindBufferMemory) \
 X(AllocateMemory) X(FreeMemory) X(MapMemory) X(UnmapMemory) X(FlushMappedMemoryRanges) X(InvalidateMappedMemoryRanges) \
 X(CreateImage) X(DestroyImage) X(GetImageMemoryRequirements) X(BindImageMemory) X(CreateImageView) X(DestroyImageView) \
 X(CreateDescriptorSetLayout) X(DestroyDescriptorSetLayout) X(CreateDescriptorPool) X(DestroyDescriptorPool) \
 X(AllocateDescriptorSets) X(FreeDescriptorSets) X(UpdateDescriptorSets) X(CreatePipelineLayout) X(DestroyPipelineLayout) \
 X(CreateShaderModule) X(DestroyShaderModule) X(CreateComputePipelines) X(DestroyPipeline) \
 X(CmdPipelineBarrier) X(CmdClearColorImage) X(CmdBindPipeline) X(CmdBindDescriptorSets) X(CmdDispatch) X(CmdCopyImageToBuffer) \
 X(CreateSwapchainKHR) X(DestroySwapchainKHR) X(GetSwapchainImagesKHR) X(AcquireNextImageKHR) X(QueuePresentKHR) \
 X(CreateSemaphore) X(DestroySemaphore) X(CreateSampler) X(DestroySampler) \
 X(CreateRenderPass) X(DestroyRenderPass) X(CreateFramebuffer) X(DestroyFramebuffer) \
 X(CreateGraphicsPipelines) X(CmdBeginRenderPass) X(CmdEndRenderPass) \
 X(CmdBindVertexBuffers) X(CmdPushConstants) X(CmdDraw) X(CmdCopyBufferToImage) X(CmdBlitImage)

typedef struct VulkanDevice {
    VkInstance instance;VkPhysicalDevice physical;VkDevice device;VkQueue queue;
    VkSurfaceKHR surface;VkCommandPool commands;uint32_t family;
    VkPhysicalDeviceProperties properties;VkPhysicalDeviceMemoryProperties memory;
    VkPhysicalDeviceVulkan12Features features12;VkPhysicalDeviceSubgroupProperties subgroup;
    VkPhysicalDeviceDriverProperties driver;
    PFN_vkGetInstanceProcAddr get_instance;PFN_vkGetDeviceProcAddr get_device;
    PFN_vkDestroyInstance destroy_instance;PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR surface_caps;
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR surface_formats;
    PFN_vkGetPhysicalDeviceFormatProperties format_properties;
    PFN_vkDestroyDebugUtilsMessengerEXT destroy_debug;VkDebugUtilsMessengerEXT debug;
    unsigned validation_errors;
#define DECLARE(name) PFN_vk##name name;
    VK_DEVICE_FUNCTIONS(DECLARE)
#undef DECLARE
} VulkanDevice;

typedef struct VulkanBuffer {VkBuffer buffer;VkDeviceMemory memory;VkDeviceSize size;int coherent;} VulkanBuffer;
typedef struct VulkanImage {VkImage image;VkImageView view;VkDeviceMemory memory;} VulkanImage;
int vulkan_device_open(VulkanDevice *,SDL_Window *,int validation);
void vulkan_device_close(VulkanDevice *);
int vulkan_buffer_create(VulkanDevice *,VulkanBuffer *,VkDeviceSize,VkBufferUsageFlags);
void vulkan_buffer_destroy(VulkanDevice *,VulkanBuffer *);
int vulkan_buffer_write(VulkanDevice *,VulkanBuffer *,const void *,size_t);
int vulkan_buffer_read(VulkanDevice *,VulkanBuffer *,void *,size_t);
int vulkan_image_create(VulkanDevice *,VulkanImage *,VkFormat,VkImageUsageFlags,uint32_t,uint32_t);
int vulkan_image_create_aspect(VulkanDevice *,VulkanImage *,VkFormat,VkImageUsageFlags,uint32_t,uint32_t,VkImageAspectFlags);
void vulkan_image_destroy(VulkanDevice *,VulkanImage *);
int vulkan_command_begin(VulkanDevice *,VkCommandBuffer *);
int vulkan_command_submit(VulkanDevice *,VkCommandBuffer,VkSemaphore,VkSemaphore);
#endif
