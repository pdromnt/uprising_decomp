#include "vulkan_device.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int result(VkResult r,const char *operation){
    if(r==VK_SUCCESS)return 1;
    fprintf(stderr,"Vulkan %s failed: VkResult=%d\n",operation,(int)r);return 0;
}
#define CHECK(call) do {if(!result((call),#call))goto fail;} while(0)
static VKAPI_ATTR VkBool32 VKAPI_CALL debug_message(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        VkDebugUtilsMessageTypeFlagsEXT type,const VkDebugUtilsMessengerCallbackDataEXT *message,void *context){
    VulkanDevice *v=context;(void)type;
    if(severity&VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)++v->validation_errors;
    fprintf(stderr,"Vulkan validation: %s\n",message->pMessage);return VK_FALSE;
}
int vulkan_device_open(VulkanDevice *v,SDL_Window *window,int validation){
    VkApplicationInfo app={VK_STRUCTURE_TYPE_APPLICATION_INFO};VkInstanceCreateInfo instance={VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    VkDebugUtilsMessengerCreateInfoEXT debug={VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
    PFN_vkCreateInstance create;PFN_vkEnumeratePhysicalDevices enumerate;
    PFN_vkGetPhysicalDeviceProperties2 properties;PFN_vkGetPhysicalDeviceFeatures2 features;
    PFN_vkGetPhysicalDeviceMemoryProperties memory;PFN_vkGetPhysicalDeviceQueueFamilyProperties queues;
    PFN_vkGetPhysicalDeviceSurfaceSupportKHR present;PFN_vkCreateDevice create_device;
    PFN_vkEnumerateDeviceExtensionProperties extensions;VkPhysicalDevice devices[32];uint32_t count=32,i,j;
    const char *names[32],*layer="VK_LAYER_KHRONOS_validation",*swap=VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    const char *const *required;uint32_t required_count=0;float priority=1;
    memset(v,0,sizeof(*v));
    v->get_instance=(PFN_vkGetInstanceProcAddr)SDL_Vulkan_GetVkGetInstanceProcAddr();
    if(!v->get_instance){fprintf(stderr,"Vulkan loader unavailable: %s\n",SDL_GetError());goto fail;}
    create=(PFN_vkCreateInstance)v->get_instance(VK_NULL_HANDLE,"vkCreateInstance");
    required=SDL_Vulkan_GetInstanceExtensions(&required_count);
    if(!create || !required || required_count+2>32)goto fail;
    for(i=0;i<required_count;i++)names[i]=required[i];
    app.pApplicationName="Uprising Vulkan/FSR3 dependency proof";app.apiVersion=VK_API_VERSION_1_2;
    instance.pApplicationInfo=&app;instance.ppEnabledExtensionNames=names;instance.enabledExtensionCount=required_count;
#ifdef __APPLE__
    names[instance.enabledExtensionCount++]=VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME;
    instance.flags=VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif
    if(validation){
        names[instance.enabledExtensionCount++]=VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
        instance.enabledLayerCount=1;instance.ppEnabledLayerNames=&layer;
        debug.messageSeverity=VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debug.messageType=VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debug.pfnUserCallback=debug_message;debug.pUserData=v;instance.pNext=&debug;
    }
    CHECK(create(&instance,NULL,&v->instance));
#define INSTANCE(variable,type,name) variable=(type)v->get_instance(v->instance,name);if(!variable)goto fail
    INSTANCE(v->destroy_instance,PFN_vkDestroyInstance,"vkDestroyInstance");
    if(validation){
        PFN_vkCreateDebugUtilsMessengerEXT make;
        INSTANCE(make,PFN_vkCreateDebugUtilsMessengerEXT,"vkCreateDebugUtilsMessengerEXT");
        INSTANCE(v->destroy_debug,PFN_vkDestroyDebugUtilsMessengerEXT,"vkDestroyDebugUtilsMessengerEXT");
        CHECK(make(v->instance,&debug,NULL,&v->debug));
    }
    if(!SDL_Vulkan_CreateSurface(window,v->instance,NULL,&v->surface)){
        fprintf(stderr,"Vulkan surface failed: %s\n",SDL_GetError());goto fail;
    }
    INSTANCE(enumerate,PFN_vkEnumeratePhysicalDevices,"vkEnumeratePhysicalDevices");
    INSTANCE(properties,PFN_vkGetPhysicalDeviceProperties2,"vkGetPhysicalDeviceProperties2");
    INSTANCE(features,PFN_vkGetPhysicalDeviceFeatures2,"vkGetPhysicalDeviceFeatures2");
    INSTANCE(memory,PFN_vkGetPhysicalDeviceMemoryProperties,"vkGetPhysicalDeviceMemoryProperties");
    INSTANCE(queues,PFN_vkGetPhysicalDeviceQueueFamilyProperties,"vkGetPhysicalDeviceQueueFamilyProperties");
    INSTANCE(present,PFN_vkGetPhysicalDeviceSurfaceSupportKHR,"vkGetPhysicalDeviceSurfaceSupportKHR");
    INSTANCE(extensions,PFN_vkEnumerateDeviceExtensionProperties,"vkEnumerateDeviceExtensionProperties");
    INSTANCE(create_device,PFN_vkCreateDevice,"vkCreateDevice");
    INSTANCE(v->surface_caps,PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR,"vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
    INSTANCE(v->surface_formats,PFN_vkGetPhysicalDeviceSurfaceFormatsKHR,"vkGetPhysicalDeviceSurfaceFormatsKHR");
    INSTANCE(v->format_properties,PFN_vkGetPhysicalDeviceFormatProperties,"vkGetPhysicalDeviceFormatProperties");
    CHECK(enumerate(v->instance,&count,devices));
    for(i=0;i<count && !v->physical;i++){
        VkPhysicalDeviceProperties2 p={VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};uint32_t n=0;
        VkQueueFamilyProperties *families;
        properties(devices[i],&p);if(p.properties.apiVersion<VK_API_VERSION_1_2)continue;
        queues(devices[i],&n,NULL);families=calloc(n,sizeof(*families));if(!families)goto fail;
        queues(devices[i],&n,families);
        for(j=0;j<n;j++){
            VkBool32 supported=0;VkQueueFlags needed=VK_QUEUE_GRAPHICS_BIT|VK_QUEUE_COMPUTE_BIT;
            if(present(devices[i],j,v->surface,&supported)!=VK_SUCCESS)continue;
            if((families[j].queueFlags&needed)==needed && families[j].queueCount && supported){v->physical=devices[i];v->family=j;break;}
        }free(families);
    }
    if(!v->physical){fprintf(stderr,"No Vulkan 1.2 graphics/compute/present device\n");goto fail;}
    {
        VkPhysicalDeviceProperties2 p={VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
        VkPhysicalDeviceFeatures2 f={VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
        VkDeviceQueueCreateInfo q={VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};VkDeviceCreateInfo d={VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
        VkPhysicalDeviceVulkan12Features enabled={VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
        VkExtensionProperties *supported;const char *device_extensions[2]={swap};uint32_t n=0,extension_count=1;
        v->driver.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES;
        v->subgroup.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;
        v->subgroup.pNext=&v->driver;p.pNext=&v->subgroup;
        properties(v->physical,&p);v->properties=p.properties;memory(v->physical,&v->memory);
#ifdef __APPLE__
        if(!strstr(v->driver.driverName,"KosmicKrisp") && !strstr(v->driver.driverName,"kosmickrisp")){
            fprintf(stderr,"This experiment requires KosmicKrisp on macOS; found %s\n",v->driver.driverName);goto fail;
        }
#endif
        v->features12.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;f.pNext=&v->features12;features(v->physical,&f);
        CHECK(extensions(v->physical,NULL,&n,NULL));supported=calloc(n,sizeof(*supported));if(!supported)goto fail;
        if(extensions(v->physical,NULL,&n,supported)!=VK_SUCCESS){free(supported);goto fail;}
        for(i=0;i<n;i++)if(!strcmp(supported[i].extensionName,"VK_KHR_portability_subset"))device_extensions[extension_count++]="VK_KHR_portability_subset";
        free(supported);
        enabled.timelineSemaphore=v->features12.timelineSemaphore;enabled.shaderFloat16=v->features12.shaderFloat16;
        q.queueFamilyIndex=v->family;q.queueCount=1;q.pQueuePriorities=&priority;
        d.queueCreateInfoCount=1;d.pQueueCreateInfos=&q;d.enabledExtensionCount=extension_count;d.ppEnabledExtensionNames=device_extensions;d.pNext=&enabled;
        CHECK(create_device(v->physical,&d,NULL,&v->device));
    }
    INSTANCE(v->get_device,PFN_vkGetDeviceProcAddr,"vkGetDeviceProcAddr");
#define LOAD(name) v->name=(PFN_vk##name)v->get_device(v->device,"vk" #name);if(!v->name){fprintf(stderr,"Missing Vulkan function: %s\n",#name);goto fail;}
    VK_DEVICE_FUNCTIONS(LOAD)
#undef LOAD
#undef INSTANCE
    v->GetDeviceQueue(v->device,v->family,0,&v->queue);
    {VkCommandPoolCreateInfo pool={VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};pool.queueFamilyIndex=v->family;pool.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
     CHECK(v->CreateCommandPool(v->device,&pool,NULL,&v->commands));}
    printf("VULKAN_DEVICE name=%s driver=%s api=%u.%u.%u subgroup=%u subgroup_ops=0x%x float16=%u timeline=%u\n",
        v->properties.deviceName,v->driver.driverName,VK_VERSION_MAJOR(v->properties.apiVersion),VK_VERSION_MINOR(v->properties.apiVersion),
        VK_VERSION_PATCH(v->properties.apiVersion),v->subgroup.subgroupSize,v->subgroup.supportedOperations,v->features12.shaderFloat16,v->features12.timelineSemaphore);
    return 1;
fail:vulkan_device_close(v);return 0;
}
void vulkan_device_close(VulkanDevice *v){
    if(v->device && v->DeviceWaitIdle)v->DeviceWaitIdle(v->device);
    if(v->commands && v->DestroyCommandPool)v->DestroyCommandPool(v->device,v->commands,NULL);
    if(v->device && v->DestroyDevice)v->DestroyDevice(v->device,NULL);
    if(v->surface)SDL_Vulkan_DestroySurface(v->instance,v->surface,NULL);
    if(v->debug && v->destroy_debug)v->destroy_debug(v->instance,v->debug,NULL);
    if(v->instance && v->destroy_instance)v->destroy_instance(v->instance,NULL);
    {unsigned errors=v->validation_errors;memset(v,0,sizeof(*v));v->validation_errors=errors;}
}
static int memory_type(VulkanDevice *v,uint32_t bits,VkMemoryPropertyFlags flags,uint32_t *index){
    uint32_t i;for(i=0;i<v->memory.memoryTypeCount;i++)if((bits&(1u<<i)) && (v->memory.memoryTypes[i].propertyFlags&flags)==flags){*index=i;return 1;}
    return 0;
}
int vulkan_buffer_create(VulkanDevice *v,VulkanBuffer *b,VkDeviceSize size,VkBufferUsageFlags usage){
    VkBufferCreateInfo create={VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};VkMemoryRequirements r;VkMemoryAllocateInfo a={VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    memset(b,0,sizeof(*b));create.size=size;create.usage=usage;create.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
    CHECK(v->CreateBuffer(v->device,&create,NULL,&b->buffer));v->GetBufferMemoryRequirements(v->device,b->buffer,&r);a.allocationSize=r.size;
    b->coherent=memory_type(v,r.memoryTypeBits,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,&a.memoryTypeIndex);
    if(!b->coherent && !memory_type(v,r.memoryTypeBits,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,&a.memoryTypeIndex))goto fail;
    CHECK(v->AllocateMemory(v->device,&a,NULL,&b->memory));CHECK(v->BindBufferMemory(v->device,b->buffer,b->memory,0));b->size=size;return 1;
fail:vulkan_buffer_destroy(v,b);return 0;
}
void vulkan_buffer_destroy(VulkanDevice *v,VulkanBuffer *b){
    if(b->buffer)v->DestroyBuffer(v->device,b->buffer,NULL);
    if(b->memory)v->FreeMemory(v->device,b->memory,NULL);
    memset(b,0,sizeof(*b));
}
static int buffer_transfer(VulkanDevice *v,VulkanBuffer *b,void *data,size_t size,int write){
    VkMappedMemoryRange range={VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};void *mapped;VkResult r=VK_SUCCESS;
    if(size>b->size || !result(v->MapMemory(v->device,b->memory,0,VK_WHOLE_SIZE,0,&mapped),"map buffer"))return 0;
    range.memory=b->memory;range.size=VK_WHOLE_SIZE;
    if(write){memcpy(mapped,data,size);if(!b->coherent)r=v->FlushMappedMemoryRanges(v->device,1,&range);}
    else {if(!b->coherent)r=v->InvalidateMappedMemoryRanges(v->device,1,&range);if(r==VK_SUCCESS)memcpy(data,mapped,size);}
    v->UnmapMemory(v->device,b->memory);return result(r,"mapped buffer synchronization");
}
int vulkan_buffer_write(VulkanDevice *v,VulkanBuffer *b,const void *data,size_t size){return buffer_transfer(v,b,(void *)data,size,1);}
int vulkan_buffer_read(VulkanDevice *v,VulkanBuffer *b,void *data,size_t size){return buffer_transfer(v,b,data,size,0);}
int vulkan_image_create(VulkanDevice *v,VulkanImage *image,VkFormat format,VkImageUsageFlags usage,uint32_t width,uint32_t height){
    return vulkan_image_create_aspect(v,image,format,usage,width,height,VK_IMAGE_ASPECT_COLOR_BIT);
}
int vulkan_image_create_aspect(VulkanDevice *v,VulkanImage *image,VkFormat format,VkImageUsageFlags usage,uint32_t width,uint32_t height,VkImageAspectFlags aspect){
    VkImageCreateInfo create={VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};VkMemoryRequirements r;VkMemoryAllocateInfo a={VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    VkImageViewCreateInfo view={VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};VkFormatProperties supported;VkFormatFeatureFlags required=0;
    memset(image,0,sizeof(*image));v->format_properties(v->physical,format,&supported);
    if(usage&VK_IMAGE_USAGE_STORAGE_BIT)required|=VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT;
    if(usage&VK_IMAGE_USAGE_SAMPLED_BIT)required|=VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
    if(usage&VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)required|=VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;
    if(usage&VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)required|=VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
    if((supported.optimalTilingFeatures&required)!=required){fprintf(stderr,"Vulkan format %u lacks required features\n",format);return 0;}
    create.imageType=VK_IMAGE_TYPE_2D;create.format=format;create.extent=(VkExtent3D){width,height,1};create.mipLevels=create.arrayLayers=1;
    create.samples=VK_SAMPLE_COUNT_1_BIT;create.tiling=VK_IMAGE_TILING_OPTIMAL;create.usage=usage;create.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
    CHECK(v->CreateImage(v->device,&create,NULL,&image->image));v->GetImageMemoryRequirements(v->device,image->image,&r);a.allocationSize=r.size;
    if(!memory_type(v,r.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,&a.memoryTypeIndex))goto fail;
    CHECK(v->AllocateMemory(v->device,&a,NULL,&image->memory));CHECK(v->BindImageMemory(v->device,image->image,image->memory,0));
    view.image=image->image;view.viewType=VK_IMAGE_VIEW_TYPE_2D;view.format=format;view.subresourceRange=(VkImageSubresourceRange){aspect,0,1,0,1};
    CHECK(v->CreateImageView(v->device,&view,NULL,&image->view));return 1;
fail:vulkan_image_destroy(v,image);return 0;
}
void vulkan_image_destroy(VulkanDevice *v,VulkanImage *image){
    if(image->view)v->DestroyImageView(v->device,image->view,NULL);
    if(image->image)v->DestroyImage(v->device,image->image,NULL);
    if(image->memory)v->FreeMemory(v->device,image->memory,NULL);
    memset(image,0,sizeof(*image));
}
int vulkan_command_begin(VulkanDevice *v,VkCommandBuffer *command){
    VkCommandBufferAllocateInfo a={VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};VkCommandBufferBeginInfo b={VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    a.commandPool=v->commands;a.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;a.commandBufferCount=1;
    if(!result(v->AllocateCommandBuffers(v->device,&a,command),"allocate command buffer"))return 0;
    b.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if(result(v->BeginCommandBuffer(*command,&b),"begin command buffer"))return 1;
    v->FreeCommandBuffers(v->device,v->commands,1,command);return 0;
}
int vulkan_command_submit(VulkanDevice *v,VkCommandBuffer command,VkSemaphore wait,VkSemaphore signal){
    VkFenceCreateInfo create={VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};VkFence fence=VK_NULL_HANDLE;int ok=0;
    VkSubmitInfo submit={VK_STRUCTURE_TYPE_SUBMIT_INFO};VkPipelineStageFlags stage=VK_PIPELINE_STAGE_TRANSFER_BIT;
    if(!result(v->EndCommandBuffer(command),"end command buffer"))goto fail;
    CHECK(v->CreateFence(v->device,&create,NULL,&fence));submit.commandBufferCount=1;submit.pCommandBuffers=&command;
    if(wait){submit.waitSemaphoreCount=1;submit.pWaitSemaphores=&wait;submit.pWaitDstStageMask=&stage;}
    if(signal){submit.signalSemaphoreCount=1;submit.pSignalSemaphores=&signal;}
    CHECK(v->QueueSubmit(v->queue,1,&submit,fence));CHECK(v->WaitForFences(v->device,1,&fence,VK_TRUE,5000000000ull));ok=1;
fail:
    if(!ok)v->DeviceWaitIdle(v->device);
    if(fence)v->DestroyFence(v->device,fence,NULL);
    v->FreeCommandBuffers(v->device,v->commands,1,&command);
    return ok;
}
