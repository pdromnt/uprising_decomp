#ifndef UPRISING_VULKAN_DRAW_H
#define UPRISING_VULKAN_DRAW_H
#include "vulkan_device.h"

/* Buffered projected geometry. Depth is normalized by the material adapter;
 * texture coordinates retain s/w,t/w,1/w. No guest addresses or GL state. */
typedef struct VulkanDrawVertex {float position[3],color[4],uv[3];} VulkanDrawVertex;
typedef struct VulkanDrawTexture {
    VulkanImage image;VkSampler sampler;VkDescriptorSet descriptor;
    VulkanBuffer upload;uint32_t width,height;int initialized;
} VulkanDrawTexture;
enum {VULKAN_DRAW_DEPTH_TEST=1,VULKAN_DRAW_DEPTH_WRITE=2,VULKAN_DRAW_BLEND=4};
typedef struct VulkanDrawBatch {
    uint32_t first,count,flags;VkDescriptorSet texture;
} VulkanDrawBatch;
typedef struct VulkanDraw {
    VulkanDevice *device;uint32_t width,height;
    VulkanImage color,depth;VulkanBuffer vertices,readback;
    VkRenderPass pass;VkFramebuffer framebuffer;VkDescriptorPool descriptors;
    VkDescriptorSetLayout texture_layout;VkPipelineLayout layout;VkPipeline pipelines[8];
    VulkanDrawTexture white;
    VulkanDrawVertex *staging;size_t vertex_count,vertex_capacity;
    VulkanDrawBatch *batches;size_t batch_count,batch_capacity;
    float clear[4];int recording,has_frame;
} VulkanDraw;
int vulkan_draw_open(VulkanDraw *,VulkanDevice *,uint32_t width,uint32_t height);
void vulkan_draw_close(VulkanDraw *);
/* Texture mutation/destruction occurs between frames. Each texture owns its
 * descriptor until destroyed; queued batches borrow it through finish(). */
int vulkan_draw_texture_create(VulkanDraw *,VulkanDrawTexture *,const uint8_t *rgba,uint32_t width,uint32_t height,int linear,int repeat);
void vulkan_draw_texture_destroy(VulkanDraw *,VulkanDrawTexture *);
int vulkan_draw_texture_update(VulkanDraw *,VulkanDrawTexture *,const uint8_t *rgba,size_t bytes);
int vulkan_draw_begin(VulkanDraw *,const float clear[4]);
int vulkan_draw_triangles(VulkanDraw *,const VulkanDrawVertex *,uint32_t count,const VulkanDrawTexture *,uint32_t flags);
/* Synchronous completion for parity/readback consumers. rgba may be NULL.
 * A later frame ring can overlap work without changing submission semantics. */
int vulkan_draw_finish(VulkanDraw *,uint8_t *rgba,size_t bytes);
int vulkan_draw_readback(VulkanDraw *,uint8_t *rgba,size_t bytes);
#endif
