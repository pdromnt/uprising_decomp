#ifndef UPRISING_VULKAN_MENU_H
#define UPRISING_VULKAN_MENU_H
#include <SDL3/SDL.h>
typedef struct VulkanMenu VulkanMenu;
struct VulkanDevice;
struct VulkanDraw;
VulkanMenu *vulkan_menu_open(SDL_Window *window,int validation,const char *cache_path);
void vulkan_menu_close(VulkanMenu *);
int vulkan_menu_present(VulkanMenu *,const SDL_Surface *rgba);
int vulkan_menu_clear(VulkanMenu *);
int vulkan_menu_readback(VulkanMenu *,uint8_t *rgba,size_t bytes);
struct VulkanDevice *vulkan_menu_device(VulkanMenu *);
int vulkan_menu_present_image(VulkanMenu *,struct VulkanDraw *);
#endif
