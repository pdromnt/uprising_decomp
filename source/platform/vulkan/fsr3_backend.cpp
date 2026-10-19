// Supply the SDK's global entry points from SDL's existing Vulkan loader.
// Upstream backend bytes remain unchanged, including their license header.
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <math.h>
#include <string.h>
#include <vector>
#define FSR_INSTANCE_FUNCTIONS(X) \
    X(vkEnumerateDeviceExtensionProperties) X(vkGetPhysicalDeviceMemoryProperties) \
    X(vkGetPhysicalDeviceProperties) X(vkGetPhysicalDeviceProperties2) \
    X(vkGetPhysicalDeviceFeatures) X(vkGetPhysicalDeviceFeatures2)
#define DECLARE(name) static PFN_##name name;
FSR_INSTANCE_FUNCTIONS(DECLARE)
static PFN_vkCreateBuffer vkCreateBuffer;
static PFN_vkGetDeviceProcAddr vkGetDeviceProcAddr;
extern "C" PFN_vkVoidFunction VKAPI_CALL fsr3_backend_device_proc(VkDevice device,const char *name){
    PFN_vkVoidFunction function=vkGetDeviceProcAddr(device,name);if(function)return function;
    if(!strcmp(name,"vkGetBufferMemoryRequirements2KHR"))return vkGetDeviceProcAddr(device,"vkGetBufferMemoryRequirements2");
    if(!strcmp(name,"vkGetImageMemoryRequirements2KHR"))return vkGetDeviceProcAddr(device,"vkGetImageMemoryRequirements2");
    return nullptr;
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
#include <ffx_vk.cpp>
#undef ffxSetFrameGenerationConfigToSwapchainVK
#undef vkEnumerateDeviceExtensionProperties
extern "C" int fsr3_backend_load(VkInstance instance,VkDevice device,PFN_vkGetInstanceProcAddr instance_proc,PFN_vkGetDeviceProcAddr device_proc){
    vkGetDeviceProcAddr=device_proc;
#define LOAD(name) name=reinterpret_cast<PFN_##name>(instance_proc(instance,#name));if(!name)return 0;
    FSR_INSTANCE_FUNCTIONS(LOAD)
#undef LOAD
    vkCreateBuffer=reinterpret_cast<PFN_vkCreateBuffer>(device_proc(device,"vkCreateBuffer"));return vkCreateBuffer!=nullptr;
}
