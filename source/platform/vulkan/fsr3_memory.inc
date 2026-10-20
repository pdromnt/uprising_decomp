// SDK adapter: prefer a non-host-visible local heap, but support unified and
// software devices. A candidate must satisfy every requested property bit.
uint32_t findMemoryTypeIndex(VkPhysicalDevice physicalDevice,VkMemoryRequirements requirements,
        VkMemoryPropertyFlags requested,VkMemoryPropertyFlags &outProperties){
    VkPhysicalDeviceMemoryProperties memory{};vkGetPhysicalDeviceMemoryProperties(physicalDevice,&memory);
    uint32_t selected=UINT32_MAX;unsigned best=0;
    for(uint32_t i=0;i<memory.memoryTypeCount;i++){
        VkMemoryPropertyFlags flags=memory.memoryTypes[i].propertyFlags;
        if(!(requirements.memoryTypeBits&(UINT32_C(1)<<i)) || (flags&requested)!=requested)continue;
        unsigned preference=1;
        if(requested==VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT && !(flags&VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT))preference+=2;
        if((requested&VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) && (flags&VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))preference+=2;
        if(selected==UINT32_MAX || preference>best){selected=i;best=preference;outProperties=flags;}
    }
    return selected;
}
