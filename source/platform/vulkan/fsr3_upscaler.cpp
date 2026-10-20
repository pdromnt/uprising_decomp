// The game-facing interface remains C; AMD's host implementation is isolated.
#include "fsr3_upscaler.h"
#include <FidelityFX/host/ffx_fsr3upscaler.h>
#include <FidelityFX/host/backends/vk/ffx_vk.h>
#include <cstdlib>
#include <cstdio>
#include <new>
#include <cmath>
#include <cfloat>
#include <cstring>
static_assert(sizeof(void*)==8 && sizeof(VkImage)==8,"SDK external image handles require 64-bit pointers");
extern "C" int fsr3_backend_load(VkInstance,VkDevice,PFN_vkGetInstanceProcAddr,PFN_vkGetDeviceProcAddr);
extern "C" PFN_vkVoidFunction VKAPI_CALL fsr3_backend_device_proc(VkDevice,const char *);
struct Fsr3Upscaler {
    VulkanDevice *device;alignas(32) FfxFsr3UpscalerContext context{};void *scratch=nullptr;bool created=false,initialized=false;
    uint32_t width,height,output_width,output_height;VulkanImage shared[3]{},output{};
};
namespace {
Fsr3Upscaler *active=nullptr;FfxGetDeviceCapabilitiesFunc original_capabilities=nullptr;
Fsr3Upscaler *creating=nullptr;FfxCreateBackendContextFunc original_create=nullptr;
FfxCreateResourceFunc original_resource=nullptr;
FfxErrorCode create_resource(FfxInterface *b,const FfxCreateResourceDescription *desc,FfxUInt32 id,FfxResourceInternal *out){
    if(std::getenv("UPRISING_FSR3_TRACE"))std::fprintf(stderr,"FSR3 resource %ls format=%u size=%ux%u\n",desc->name,desc->resourceDescription.format,desc->resourceDescription.width,desc->resourceDescription.height);
    FfxErrorCode result=original_resource(b,desc,id,out);
    if(result!=FFX_OK)std::fprintf(stderr,"FSR3 resource failed: %ls code=%x\n",desc->name,result);return result;
}
FfxErrorCode create_backend(FfxInterface *b,FfxEffect effect,FfxEffectBindlessConfig *config,FfxUInt32 *id){
    FfxErrorCode result=original_create(b,effect,config,id);
    if(result!=FFX_OK)std::fprintf(stderr,"FSR3 backend context failed: %x\n",result);
    if(result==FFX_OK && creating)creating->created=true;return result;
}
FfxErrorCode capabilities(FfxInterface *backend,FfxDeviceCapabilities *out){
    FfxErrorCode result=original_capabilities(backend,out);
    if(result==FFX_OK){out->fp16Supported=false;out->waveLaneCountMax=out->waveLaneCountMin;}
    return result;
}
}
extern "C" Fsr3Upscaler *fsr3_upscaler_open(VulkanDevice *v,uint32_t width,uint32_t height,uint32_t output_width,uint32_t output_height){
    if(active || !v || !width || !height || output_width<width || output_height<height)return nullptr;
    if(!v->fsr3_storage_supported){std::fprintf(stderr,"FSR3 requires extended storage formats, formatless image writes and extended gathers\n");return nullptr;}
    Fsr3Upscaler *s=new(std::nothrow) Fsr3Upscaler;if(!s)return nullptr;s->device=v;
    s->width=width;s->height=height;s->output_width=output_width;s->output_height=output_height;
    FfxFsr3UpscalerContextDescription desc{};VkDeviceContext device{v->device,v->physical,fsr3_backend_device_proc};
    if(!fsr3_backend_load(v->instance,v->device,v->get_instance,v->get_device))goto fail;
    desc.maxRenderSize={width,height};desc.maxUpscaleSize={output_width,output_height};
    desc.flags=FFX_FSR3UPSCALER_ENABLE_DEPTH_INVERTED|FFX_FSR3UPSCALER_ENABLE_DEPTH_INFINITE|FFX_FSR3UPSCALER_ENABLE_AUTO_EXPOSURE;
    desc.backendInterface.scratchBufferSize=ffxGetScratchMemorySizeVK(v->physical,1);
    s->scratch=::operator new(desc.backendInterface.scratchBufferSize,std::align_val_t(32),std::nothrow);if(!s->scratch)goto fail;
    std::memset(s->scratch,0,desc.backendInterface.scratchBufferSize);
    if(ffxGetInterfaceVK(&desc.backendInterface,ffxGetDeviceVK(&device),s->scratch,desc.backendInterface.scratchBufferSize,1)!=FFX_OK)goto fail;
    original_capabilities=desc.backendInterface.fpGetDeviceCapabilities;desc.backendInterface.fpGetDeviceCapabilities=capabilities;
    original_create=desc.backendInterface.fpCreateBackendContext;desc.backendInterface.fpCreateBackendContext=create_backend;
    original_resource=desc.backendInterface.fpCreateResource;desc.backendInterface.fpCreateResource=create_resource;
    creating=s;
    {FfxErrorCode result=ffxFsr3UpscalerContextCreate(&s->context,&desc);
    creating=nullptr;
    if(result!=FFX_OK){std::fprintf(stderr,"FSR3 context creation failed: %x\n",result);goto fail;}}
    s->created=true;active=s;
    {const VkFormat formats[3]={VK_FORMAT_R32_SFLOAT,VK_FORMAT_R16G16_SFLOAT,VK_FORMAT_R32_UINT};
    // The SDK clears reconstructed depth during reset using a transfer job.
    for(unsigned i=0;i<3;i++)if(!vulkan_image_create(v,s->shared+i,formats[i],VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,width,height))goto fail;
    if(!vulkan_image_create(v,&s->output,VK_FORMAT_R32G32B32A32_SFLOAT,VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT,output_width,output_height))goto fail;}
    return s;
fail:fsr3_upscaler_close(s);return nullptr;
}
extern "C" void fsr3_upscaler_close(Fsr3Upscaler *s){
    if(!s)return;s->device->DeviceWaitIdle(s->device->device);
    if(s->created)ffxFsr3UpscalerContextDestroy(&s->context);
    for(auto &image:s->shared)vulkan_image_destroy(s->device,&image);
    vulkan_image_destroy(s->device,&s->output);
    ::operator delete(s->scratch,std::align_val_t(32));if(active==s)active=nullptr;delete s;
}
namespace {
FfxResource resource(const VulkanImage *image,FfxSurfaceFormat format,uint32_t width,uint32_t height,FfxResourceStates state){
    FfxResourceDescription desc{};desc.type=FFX_RESOURCE_TYPE_TEXTURE2D;desc.format=format;
    desc.width=width;desc.height=height;desc.depth=desc.mipCount=1;
    desc.usage=state==FFX_RESOURCE_STATE_UNORDERED_ACCESS?FFX_RESOURCE_USAGE_UAV:FFX_RESOURCE_USAGE_READ_ONLY;
    return ffxGetResourceVK(reinterpret_cast<void*>(image->image),desc,L"Uprising temporal input",state);
}
void transition(VulkanDevice *v,VkCommandBuffer command,VkImage image,VkImageLayout from,VkImageLayout to){
    VkImageMemoryBarrier b{};b.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;b.oldLayout=from;b.newLayout=to;
    b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;b.image=image;
    b.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
    b.srcAccessMask=from==VK_IMAGE_LAYOUT_UNDEFINED?0:VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT;
    b.dstAccessMask=VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT;
    v->CmdPipelineBarrier(command,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,0,0,nullptr,0,nullptr,1,&b);
}
}
extern "C" const VulkanImage *fsr3_upscaler_dispatch(Fsr3Upscaler *s,VkCommandBuffer command,
        const VulkanImage *color,const VulkanImage *depth,const VulkanImage *motion,
        float jitter_x,float jitter_y,float elapsed_ms,float near_z,float vertical_fov,int reset,int sharpen){
    if(!s || !command || !color || !depth || !motion || !std::isfinite(jitter_x) || !std::isfinite(jitter_y) ||
       std::fabs(jitter_x)>0.5f || std::fabs(jitter_y)>0.5f || !std::isfinite(elapsed_ms) || elapsed_ms<=0 ||
       !std::isfinite(near_z) || near_z<=0 || !std::isfinite(vertical_fov) || vertical_fov<=0 || vertical_fov>=3.14159265f)return nullptr;
    const VulkanImage *inputs[3]={color,depth,motion};
    for(auto image:inputs)transition(s->device,command,image->image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    if(!s->initialized)for(auto &image:s->shared)transition(s->device,command,image.image,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_GENERAL);
    transition(s->device,command,s->output.image,s->initialized?VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_GENERAL);
    FfxFsr3UpscalerDispatchDescription d{};d.commandList=ffxGetCommandListVK(command);
    d.color=resource(color,FFX_SURFACE_FORMAT_R8G8B8A8_UNORM,s->width,s->height,FFX_RESOURCE_STATE_COMPUTE_READ);
    d.depth=resource(depth,FFX_SURFACE_FORMAT_R32_FLOAT,s->width,s->height,FFX_RESOURCE_STATE_COMPUTE_READ);
    d.motionVectors=resource(motion,FFX_SURFACE_FORMAT_R32G32B32A32_FLOAT,s->width,s->height,FFX_RESOURCE_STATE_COMPUTE_READ);
    d.dilatedDepth=resource(s->shared,FFX_SURFACE_FORMAT_R32_FLOAT,s->width,s->height,FFX_RESOURCE_STATE_UNORDERED_ACCESS);
    d.dilatedMotionVectors=resource(s->shared+1,FFX_SURFACE_FORMAT_R16G16_FLOAT,s->width,s->height,FFX_RESOURCE_STATE_UNORDERED_ACCESS);
    d.reconstructedPrevNearestDepth=resource(s->shared+2,FFX_SURFACE_FORMAT_R32_UINT,s->width,s->height,FFX_RESOURCE_STATE_UNORDERED_ACCESS);
    d.output=resource(&s->output,FFX_SURFACE_FORMAT_R32G32B32A32_FLOAT,s->output_width,s->output_height,FFX_RESOURCE_STATE_UNORDERED_ACCESS);
    d.renderSize={s->width,s->height};d.upscaleSize={s->output_width,s->output_height};d.jitterOffset={jitter_x,jitter_y};
    d.motionVectorScale={1,1};d.frameTimeDelta=elapsed_ms;d.preExposure=1;d.reset=reset || !s->initialized;
    // The SDK names describe device-depth endpoints, hence reversed order.
    d.cameraNear=FLT_MAX;d.cameraFar=near_z;d.cameraFovAngleVertical=vertical_fov;d.viewSpaceToMetersFactor=1;
    d.enableSharpening=sharpen!=0;d.sharpness=0.2f;
    FfxErrorCode result=ffxFsr3UpscalerContextDispatch(&s->context,&d);
    for(auto image:inputs)transition(s->device,command,image->image,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    transition(s->device,command,s->output.image,VK_IMAGE_LAYOUT_GENERAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    s->initialized=true;
    if(result!=FFX_OK){std::fprintf(stderr,"FSR3 dispatch failed: %x\n",result);return nullptr;}
    return &s->output;
}
