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
    float jitter[2],depth_scale;uint32_t projection_padding;
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
    VulkanImage color,depth,reciprocal_depth,motion,scene;VulkanBuffer vertices,readback;
    VkRenderPass pass,resume_pass;VkFramebuffer framebuffer;VkDescriptorPool descriptors;
    VkDescriptorSetLayout texture_layout;VkPipelineLayout layout;VkPipeline pipelines[1728];VkShaderModule shaders[2];
    VulkanDrawMaterial material;
    VkSampler samplers[8];int texture_sampler;
    VulkanDrawTexture *mask_texture;
    VulkanDrawTexture white;
    VulkanDrawVertex *staging;size_t vertex_count,vertex_capacity;
    VulkanDrawBatch *batches;size_t batch_count,batch_capacity;
    float clear[4],clear_depth;int recording,has_frame,resuming;
    /* R32F world reversed-infinite perspective depth (near/z), derived from
     * unquantized q. UI/skies and draws without depth writes preserve it. */
    int capture_depth,capture_scene,clear_reciprocal;
    float scene_jitter[2],scene_depth_scale;
} VulkanDraw;
/* q=numerator/z becomes reversed infinite perspective depth near/z. Hardware
 * W-depth remains unchanged. Jitter is in top-left pixel coordinates, applied
 * only to captured world draws; previous positions remain unjittered. */
int vulkan_draw_projection(VulkanDraw *,float near_z,float numerator,float jitter_x,float jitter_y);
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
/* Reset scene color/depth/motion on a resumed target without clearing final
 * screen artwork or legacy hardware depth. Must precede captured scene draws. */
int vulkan_draw_reset_reciprocal_depth(VulkanDraw *);
int vulkan_draw_triangles(VulkanDraw *,const VulkanDrawVertex *,uint32_t count,VulkanDrawTexture *,uint32_t flags);
int vulkan_draw_points(VulkanDraw *,const VulkanDrawVertex *,uint32_t count,VulkanDrawTexture *,uint32_t flags);
int vulkan_draw_lines(VulkanDraw *,const VulkanDrawVertex *,uint32_t count,VulkanDrawTexture *,uint32_t flags);
/* Synchronous completion for parity/readback consumers. rgba may be NULL.
 * A later frame ring can overlap work without changing submission semantics. */
int vulkan_draw_finish(VulkanDraw *,uint8_t *rgba,size_t bytes);
int vulkan_draw_readback(VulkanDraw *,uint8_t *rgba,size_t bytes);
/* Scene-only color: world and sky under their actual depth/alpha/mask tests.
 * Screen artwork never writes it. Retained across flush/resume, reset with
 * temporal world inputs. Final post-upscale composition remains separate work. */
int vulkan_draw_read_scene(VulkanDraw *,uint8_t *rgba,size_t bytes);
/* Historical name: returns normalized near/z, not raw numerator/z. */
int vulkan_draw_read_reciprocal_depth(VulkanDraw *,float *,size_t bytes);
/* RGBA32F: previous-minus-current pixels, validity, conservative history rejection.
 * Unknown geometry rejects history. Translucent scene coverage raises rejection
 * without replacing the underlying solid motion. Composition work remains. */
int vulkan_draw_read_motion(VulkanDraw *,float *,size_t bytes);
#endif
