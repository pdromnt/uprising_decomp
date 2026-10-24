// Supply the SDK's global entry points from SDL's existing Vulkan loader.
// Upstream backend bytes remain unchanged, including their license header.
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <math.h>
#include <string.h>
#include <vector>
#include <FidelityFX/host/ffx_fsr3upscaler.h>
#define FSR_INSTANCE_FUNCTIONS(X) \
    X(vkEnumerateDeviceExtensionProperties) X(vkGetPhysicalDeviceMemoryProperties) \
    X(vkGetPhysicalDeviceProperties) X(vkGetPhysicalDeviceProperties2) \
    X(vkGetPhysicalDeviceFeatures) X(vkGetPhysicalDeviceFeatures2)
#define DECLARE(name) static PFN_##name name;
FSR_INSTANCE_FUNCTIONS(DECLARE)
static PFN_vkCreateBuffer vkCreateBuffer;
static PFN_vkGetDeviceProcAddr vkGetDeviceProcAddr;
static VkPipelineCache shared_pipeline_cache;
static VkResult VKAPI_CALL create_pool(VkDevice device,const VkDescriptorPoolCreateInfo *info,const VkAllocationCallbacks *allocator,VkDescriptorPool *out){
    // One upscaler context, eleven supported passes, the SDK's four-frame/ten-
    // usage descriptor rings. Bound each type by the shader interface maxima.
    const uint32_t sets=(FFX_FSR3UPSCALER_PASS_COUNT-1)*FFX_MAX_QUEUED_FRAMES*10;
    VkDescriptorPoolSize sizes[]={{VK_DESCRIPTOR_TYPE_SAMPLER,sets*2},{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,sets*FFX_MAX_NUM_SRVS},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,sets*FFX_MAX_NUM_UAVS},{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,sets*FFX_MAX_NUM_CONST_BUFFERS}};
    VkDescriptorPoolCreateInfo bounded=*info;bounded.maxSets=sets;bounded.poolSizeCount=4;bounded.pPoolSizes=sizes;
    auto function=reinterpret_cast<PFN_vkCreateDescriptorPool>(vkGetDeviceProcAddr(device,"vkCreateDescriptorPool"));
    VkResult result=function(device,&bounded,allocator,out);
    if(result!=VK_SUCCESS)fprintf(stderr,"FSR3 descriptor pool failed: %d\n",result);return result;
}
static VkResult VKAPI_CALL create_pipelines(VkDevice device,VkPipelineCache cache,uint32_t count,const VkComputePipelineCreateInfo *info,const VkAllocationCallbacks *allocator,VkPipeline *out){
    auto function=reinterpret_cast<PFN_vkCreateComputePipelines>(vkGetDeviceProcAddr(device,"vkCreateComputePipelines"));
    VkResult result=function(device,shared_pipeline_cache?shared_pipeline_cache:cache,count,info,allocator,out);
    if(result!=VK_SUCCESS)fprintf(stderr,"FSR3 compute pipeline failed: %d\n",result);return result;
}
extern "C" PFN_vkVoidFunction VKAPI_CALL fsr3_backend_device_proc(VkDevice device,const char *name){
    if(!strcmp(name,"vkCreateDescriptorPool"))return reinterpret_cast<PFN_vkVoidFunction>(create_pool);
    if(!strcmp(name,"vkCreateComputePipelines"))return reinterpret_cast<PFN_vkVoidFunction>(create_pipelines);
    if(!strcmp(name,"vkGetBufferMemoryRequirements2KHR"))return vkGetDeviceProcAddr(device,"vkGetBufferMemoryRequirements2");
    if(!strcmp(name,"vkGetImageMemoryRequirements2KHR"))return vkGetDeviceProcAddr(device,"vkGetImageMemoryRequirements2");
    // Prefer the enabled core command even if a loader/layer exposes a KHR
    // trampoline for an extension that was not enabled on this device.
    return vkGetDeviceProcAddr(device,name);
}
#undef DECLARE
#include <FidelityFX/host/backends/vk/ffx_vk.h>
// The SDK treats advertised extensions as enabled. Expose only the promoted
// memory-query/allocation functionality usable on our Vulkan 1.2 device; optional
// AMD breadcrumbs, descriptor indexing and synchronization2 were not enabled.
static VkResult VKAPI_CALL enabled_extensions(VkPhysicalDevice physical,const char *layer,uint32_t *count,VkExtensionProperties *out){
    if(layer)return VK_ERROR_LAYER_NOT_PRESENT;
    uint32_t available=0;VkResult result=vkEnumerateDeviceExtensionProperties(physical,nullptr,&available,nullptr);
    if(result!=VK_SUCCESS)return result;
    std::vector<VkExtensionProperties> properties(available);result=vkEnumerateDeviceExtensionProperties(physical,nullptr,&available,properties.data());
    if(result!=VK_SUCCESS)return result;
    uint32_t capacity=out?*count:0,total=0;
    for(uint32_t i=0;i<available;i++)if(!strcmp(properties[i].extensionName,"VK_KHR_dedicated_allocation") ||
       !strcmp(properties[i].extensionName,"VK_KHR_get_memory_requirements2")){
        if(out && total<capacity)out[total]=properties[i];total++;
    }
    *count=out && capacity<total?capacity:total;return out && capacity<total?VK_INCOMPLETE:VK_SUCCESS;
}
#define vkEnumerateDeviceExtensionProperties enabled_extensions
// The upscaler-only interface exposes no frame-generation presenter callback.
// Read its guarded declaration first, then leave that optional table slot null.
#define ffxSetFrameGenerationConfigToSwapchainVK nullptr
#include "fsr3_backend_native.inc"
#undef ffxSetFrameGenerationConfigToSwapchainVK
#undef vkEnumerateDeviceExtensionProperties
extern "C" int fsr3_backend_load(VkInstance instance,VkDevice device,PFN_vkGetInstanceProcAddr instance_proc,PFN_vkGetDeviceProcAddr device_proc,VkPipelineCache cache){
    shared_pipeline_cache=cache;
    vkGetDeviceProcAddr=device_proc;
#define LOAD(name) name=reinterpret_cast<PFN_##name>(instance_proc(instance,#name));if(!name)return 0;
    FSR_INSTANCE_FUNCTIONS(LOAD)
#undef LOAD
    vkCreateBuffer=reinterpret_cast<PFN_vkCreateBuffer>(device_proc(device,"vkCreateBuffer"));return vkCreateBuffer!=nullptr;
}
