#ifndef UPRISING_VULKAN_MENU_H
#define UPRISING_VULKAN_MENU_H
#include <SDL3/SDL.h>
typedef struct VulkanMenu VulkanMenu;
VulkanMenu *vulkan_menu_open(SDL_Window *window,int validation);
void vulkan_menu_close(VulkanMenu *);
int vulkan_menu_present(VulkanMenu *,const SDL_Surface *rgba);
int vulkan_menu_clear(VulkanMenu *);
int vulkan_menu_readback(VulkanMenu *,uint8_t *rgba,size_t bytes);
#endif
