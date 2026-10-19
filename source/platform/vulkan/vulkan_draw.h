#ifndef UPRISING_VULKAN_DRAW_H
#define UPRISING_VULKAN_DRAW_H
#include "vulkan_device.h"

/* Buffered projected geometry. Depth is normalized by the material adapter;
 * texture coordinates retain s/w,t/w,1/w. No guest addresses or GL state. */
typedef struct VulkanDrawVertex {float position[3],color[4],uv[3],fog,previous[4];} VulkanDrawVertex;
/* Mirrors the shader push block with explicit fixed-width members. */
typedef struct VulkanDrawMaterial {
    float extent[2];uint32_t color_mode,alpha_mode;
    float constant[4],fog_color[4],fog_range[2];
    uint32_t fog_mode,alpha_function;float alpha_reference;uint32_t chroma;
    uint32_t padding[2];float mask_bounds[4];uint32_t mask_enabled,texture_sampling;
} VulkanDrawMaterial;
typedef struct VulkanDrawTexture {
    VulkanImage image;VkSampler sampler;VkDescriptorSet descriptor;
    VulkanBuffer upload;uint32_t width,height;int initialized;
    VkDescriptorPool pool;VkDescriptorSet variants[8];
} VulkanDrawTexture;
enum {VULKAN_DRAW_DEPTH_TEST=1,VULKAN_DRAW_DEPTH_WRITE=2,VULKAN_DRAW_BLEND=4,
    VULKAN_DRAW_LEQUAL=3<<3,VULKAN_DRAW_POINTS=64,VULKAN_DRAW_LINES=128};
/* Compare values use VkCompareOp << 3; primitive bits are mutually exclusive. */
typedef struct VulkanDrawBatch {
    uint32_t first,count,flags;VkDescriptorSet texture,mask;VulkanDrawMaterial material;
} VulkanDrawBatch;
typedef struct VulkanDraw {
    VulkanDevice *device;uint32_t width,height;
    VulkanImage color,depth,reciprocal_depth,motion;VulkanBuffer vertices,readback;
    VkRenderPass pass,resume_pass;VkFramebuffer framebuffer;VkDescriptorPool descriptors;
    VkDescriptorSetLayout texture_layout;VkPipelineLayout layout;VkPipeline pipelines[448];VkShaderModule shaders[2];
    VulkanDrawMaterial material;
    VkSampler samplers[8];int texture_sampler;
    VulkanDrawTexture *mask_texture;
    VulkanDrawTexture white;
    VulkanDrawVertex *staging;size_t vertex_count,vertex_capacity;
    VulkanDrawBatch *batches;size_t batch_count,batch_capacity;
    float clear[4],clear_depth;int recording,has_frame,resuming;
    /* Capture unquantized world 1/w alongside color. UI/skies and draws without
     * depth writes preserve it. This is not yet FSR's perspective depth. */
    int capture_depth,clear_reciprocal;
} VulkanDraw;
int vulkan_draw_open(VulkanDraw *,VulkanDevice *,uint32_t width,uint32_t height);
void vulkan_draw_close(VulkanDraw *);
/* Each texture owns its descriptor until destroyed; batches borrow it through
 * finish(). Flush before mutating texels referenced by queued batches. Disjoint
 * atlas regions may be uploaded before the queued draws are submitted. */
int vulkan_draw_texture_create(VulkanDraw *,VulkanDrawTexture *,const uint8_t *rgba,uint32_t width,uint32_t height,int linear,int repeat);
void vulkan_draw_texture_destroy(VulkanDraw *,VulkanDrawTexture *);
int vulkan_draw_texture_update(VulkanDraw *,VulkanDrawTexture *,const uint8_t *rgba,size_t bytes);
int vulkan_draw_texture_region(VulkanDraw *,VulkanDrawTexture *,const uint8_t *rgba,uint32_t x,uint32_t y,uint32_t width,uint32_t height);
int vulkan_draw_begin(VulkanDraw *,const float clear[4]);
int vulkan_draw_resume(VulkanDraw *);
/* Start a new world input on a resumed target without clearing screen artwork
 * or legacy hardware depth. Must precede captured geometry in this batch. */
int vulkan_draw_reset_reciprocal_depth(VulkanDraw *);
int vulkan_draw_triangles(VulkanDraw *,const VulkanDrawVertex *,uint32_t count,VulkanDrawTexture *,uint32_t flags);
int vulkan_draw_points(VulkanDraw *,const VulkanDrawVertex *,uint32_t count,VulkanDrawTexture *,uint32_t flags);
int vulkan_draw_lines(VulkanDraw *,const VulkanDrawVertex *,uint32_t count,VulkanDrawTexture *,uint32_t flags);
/* Synchronous completion for parity/readback consumers. rgba may be NULL.
 * A later frame ring can overlap work without changing submission semantics. */
int vulkan_draw_finish(VulkanDraw *,uint8_t *rgba,size_t bytes);
int vulkan_draw_readback(VulkanDraw *,uint8_t *rgba,size_t bytes);
int vulkan_draw_read_reciprocal_depth(VulkanDraw *,float *,size_t bytes);
/* RGBA32F: previous-minus-current pixels, correspondence validity, invalidity.
 * Unknown geometry is explicitly invalid; this is not a finished reactive mask. */
int vulkan_draw_read_motion(VulkanDraw *,float *,size_t bytes);
#endif
