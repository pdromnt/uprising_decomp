#include "vulkan_present.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define VK(call) do {VkResult status=(call);if(status!=VK_SUCCESS){fprintf(stderr,"Vulkan present %s: %d\n",#call,(int)status);goto fail;}}while(0)

int vulkan_present_open(VulkanPresent *p,VulkanDevice *v,SDL_Window *window){
    VkSurfaceCapabilitiesKHR caps;VkSurfaceFormatKHR *formats=NULL,format;VkFormatProperties features;uint32_t n=0,i;
    VkSwapchainCreateInfoKHR create={VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};VkSemaphoreCreateInfo semaphore={VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    memset(p,0,sizeof(*p));p->device=v;p->window=window;
    VK(v->surface_caps(v->physical,v->surface,&caps));VK(v->surface_formats(v->physical,v->surface,&n,NULL));
    if(!n)goto fail;
    formats=calloc(n,sizeof(*formats));if(!formats)goto fail;
    VK(v->surface_formats(v->physical,v->surface,&n,formats));format=formats[0];
    for(i=0;i<n;i++)if(formats[i].format==VK_FORMAT_B8G8R8A8_UNORM || formats[i].format==VK_FORMAT_R8G8B8A8_UNORM){format=formats[i];break;}
    if(format.format==VK_FORMAT_UNDEFINED)format.format=VK_FORMAT_B8G8R8A8_UNORM;
    v->format_properties(v->physical,format.format,&features);
    if(!(caps.supportedUsageFlags&VK_IMAGE_USAGE_TRANSFER_DST_BIT) || !(features.optimalTilingFeatures&VK_FORMAT_FEATURE_BLIT_DST_BIT))goto fail;
    v->format_properties(v->physical,VK_FORMAT_R8G8B8A8_UNORM,&features);
    if(!(features.optimalTilingFeatures&VK_FORMAT_FEATURE_BLIT_SRC_BIT))goto fail;
    p->extent=caps.currentExtent;
    if(p->extent.width==UINT32_MAX){
        int w,h;if(!SDL_GetWindowSizeInPixels(window,&w,&h))goto fail;
        p->extent=(VkExtent2D){w>0?(uint32_t)w:0,h>0?(uint32_t)h:0};
        if(p->extent.width && p->extent.height){
            if(p->extent.width<caps.minImageExtent.width)p->extent.width=caps.minImageExtent.width;
            if(p->extent.height<caps.minImageExtent.height)p->extent.height=caps.minImageExtent.height;
            if(p->extent.width>caps.maxImageExtent.width)p->extent.width=caps.maxImageExtent.width;
            if(p->extent.height>caps.maxImageExtent.height)p->extent.height=caps.maxImageExtent.height;
        }
    }
    if(!p->extent.width || !p->extent.height){p->recreate=1;free(formats);return 1;}
    create.surface=v->surface;create.minImageCount=caps.minImageCount+1;
    if(caps.maxImageCount && create.minImageCount>caps.maxImageCount)create.minImageCount=caps.maxImageCount;
    create.imageFormat=format.format;create.imageColorSpace=format.colorSpace;create.imageExtent=p->extent;
    create.imageArrayLayers=1;create.imageUsage=VK_IMAGE_USAGE_TRANSFER_DST_BIT;create.imageSharingMode=VK_SHARING_MODE_EXCLUSIVE;
    create.preTransform=caps.currentTransform;create.compositeAlpha=VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    if(!(caps.supportedCompositeAlpha&create.compositeAlpha))create.compositeAlpha=(VkCompositeAlphaFlagBitsKHR)(caps.supportedCompositeAlpha&(~caps.supportedCompositeAlpha+1u));
    create.presentMode=VK_PRESENT_MODE_FIFO_KHR;create.clipped=VK_TRUE;
    VK(v->CreateSwapchainKHR(v->device,&create,NULL,&p->swapchain));
    VK(v->GetSwapchainImagesKHR(v->device,p->swapchain,&p->count,NULL));
    p->images=calloc(p->count,sizeof(*p->images));if(!p->images)goto fail;
    VK(v->GetSwapchainImagesKHR(v->device,p->swapchain,&p->count,p->images));
    VK(v->CreateSemaphore(v->device,&semaphore,NULL,&p->acquired));VK(v->CreateSemaphore(v->device,&semaphore,NULL,&p->finished));
    free(formats);return 1;
fail:free(formats);vulkan_present_close(p);return 0;
}
void vulkan_present_close(VulkanPresent *p){
    VulkanDevice *v=p->device;if(!v)return;
    v->DeviceWaitIdle(v->device);
    if(p->acquired)v->DestroySemaphore(v->device,p->acquired,NULL);
    if(p->finished)v->DestroySemaphore(v->device,p->finished,NULL);
    if(p->swapchain)v->DestroySwapchainKHR(v->device,p->swapchain,NULL);
    free(p->images);memset(p,0,sizeof(*p));
}
int vulkan_present_image(VulkanPresent *p,VkImage image,uint32_t width,uint32_t height){
    VulkanDevice *v=p->device;VkResult status;uint32_t index,w,h;VkCommandBuffer command;int x,y;
    VkImageMemoryBarrier barrier={VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};VkClearColorValue black={{0,0,0,1}};VkImageBlit blit={0};
    VkPresentInfoKHR present={VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    if(!width || !height || !image)return 0;
    {int pixels_w,pixels_h;
     if(!SDL_GetWindowSizeInPixels(p->window,&pixels_w,&pixels_h))return 0;
     if(pixels_w<=0 || pixels_h<=0)return 1;
     if((uint32_t)pixels_w!=p->extent.width || (uint32_t)pixels_h!=p->extent.height)p->recreate=1;
    }
    if(p->recreate){SDL_Window *window=p->window;vulkan_present_close(p);if(!vulkan_present_open(p,v,window))return 0;}
    if(!p->swapchain)return 1;
    status=v->AcquireNextImageKHR(v->device,p->swapchain,5000000000ull,p->acquired,VK_NULL_HANDLE,&index);
    if(status==VK_ERROR_OUT_OF_DATE_KHR){p->recreate=1;return 1;}
    if(status==VK_SUBOPTIMAL_KHR)p->recreate=1;else if(status!=VK_SUCCESS){fprintf(stderr,"Vulkan acquire: %d\n",(int)status);return 0;}
    if(!vulkan_command_begin(v,&command))return 0;
    barrier.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED;barrier.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;barrier.srcQueueFamilyIndex=barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
    barrier.image=p->images[index];barrier.subresourceRange=(VkImageSubresourceRange){VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
    v->CmdPipelineBarrier(command,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,NULL,0,NULL,1,&barrier);
    v->CmdClearColorImage(command,p->images[index],VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,&black,1,&barrier.subresourceRange);
    /* Clear and blit write overlapping regions. Keep the clear's write before
     * the blit even though both commands use the transfer stage. */
    barrier.oldLayout=barrier.newLayout;barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
    v->CmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,NULL,0,NULL,1,&barrier);
    w=p->extent.width;h=(uint32_t)((uint64_t)w*height/width);
    if(h>p->extent.height){h=p->extent.height;w=(uint32_t)((uint64_t)h*width/height);}
    if(!w)w=1;
    if(!h)h=1;
    x=(int)((p->extent.width-w)/2);y=(int)((p->extent.height-h)/2);
    blit.srcSubresource=blit.dstSubresource=(VkImageSubresourceLayers){VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};
    blit.srcOffsets[1]=(VkOffset3D){(int32_t)width,(int32_t)height,1};
    blit.dstOffsets[0]=(VkOffset3D){x,y,0};blit.dstOffsets[1]=(VkOffset3D){x+(int32_t)w,y+(int32_t)h,1};
    v->CmdBlitImage(command,image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,p->images[index],VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&blit,VK_FILTER_NEAREST);
    barrier.newLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;barrier.dstAccessMask=0;
    v->CmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,0,0,NULL,0,NULL,1,&barrier);
    if(!vulkan_command_submit(v,command,p->acquired,p->finished))return 0;
    present.waitSemaphoreCount=1;present.pWaitSemaphores=&p->finished;present.swapchainCount=1;present.pSwapchains=&p->swapchain;present.pImageIndices=&index;
    status=v->QueuePresentKHR(v->queue,&present);
    if(status==VK_ERROR_OUT_OF_DATE_KHR || status==VK_SUBOPTIMAL_KHR)p->recreate=1;
    else if(status!=VK_SUCCESS){fprintf(stderr,"Vulkan present: %d\n",(int)status);return 0;}
    /* Current parity path is deliberately synchronous; do not recycle a
     * binary semaphore until presentation has consumed its signal. */
    VK(v->QueueWaitIdle(v->queue));return 1;
fail:return 0;
}
