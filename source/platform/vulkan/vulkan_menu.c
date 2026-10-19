#include "vulkan_menu.h"
#include "vulkan_draw.h"
#include "vulkan_present.h"
#include <stdlib.h>
#include <string.h>
struct VulkanMenu {
    VulkanDevice device;VulkanDraw draw;VulkanPresent present;
    VulkanDrawTexture texture;int loader;
};
VulkanMenu *vulkan_menu_open(SDL_Window *window,int validation){
    VulkanMenu *m=calloc(1,sizeof(*m));if(!m)return NULL;
    if(!SDL_Vulkan_LoadLibrary(NULL))goto fail;
    m->loader=1;
    if(!vulkan_device_open(&m->device,window,validation) || !vulkan_draw_open(&m->draw,&m->device,640,480) ||
       !vulkan_present_open(&m->present,&m->device,window))goto fail;
    return m;
fail:vulkan_menu_close(m);return NULL;
}
void vulkan_menu_close(VulkanMenu *m){
    if(!m)return;
    vulkan_present_close(&m->present);
    if(m->device.device)vulkan_draw_texture_destroy(&m->draw,&m->texture);
    vulkan_draw_close(&m->draw);vulkan_device_close(&m->device);
    if(m->loader)SDL_Vulkan_UnloadLibrary();
    free(m);
}
int vulkan_menu_present(VulkanMenu *m,const SDL_Surface *rgba){
    const float clear[4]={0,0,0,1};VulkanDrawVertex vertex[6]={0};unsigned i;
    const unsigned corner[6]={0,1,2,0,2,3};
    if(!rgba || rgba->format!=SDL_PIXELFORMAT_RGBA32 || rgba->w!=640 || rgba->h!=480 || rgba->pitch!=640*4)return 0;
    if(!m->texture.image.image){if(!vulkan_draw_texture_create(&m->draw,&m->texture,rgba->pixels,640,480,0,0))return 0;}
    else if(!vulkan_draw_texture_update(&m->draw,&m->texture,rgba->pixels,640*480*4))return 0;
    for(i=0;i<6;i++){
        unsigned c=corner[i];int right=c==1 || c==2,bottom=c>=2;
        vertex[i].position[0]=right?640:0;vertex[i].position[1]=bottom?480:0;
        vertex[i].color[0]=vertex[i].color[1]=vertex[i].color[2]=vertex[i].color[3]=1;
        vertex[i].uv[0]=right?1:0;vertex[i].uv[1]=bottom?1:0;vertex[i].uv[2]=1;
    }
    return vulkan_draw_begin(&m->draw,clear) && vulkan_draw_triangles(&m->draw,vertex,6,&m->texture,0) &&
        vulkan_draw_finish(&m->draw,NULL,0) && vulkan_present_image(&m->present,m->draw.color.image,640,480);
}
int vulkan_menu_clear(VulkanMenu *m){
    const float clear[4]={0,0,0,1};
    return vulkan_draw_begin(&m->draw,clear) && vulkan_draw_finish(&m->draw,NULL,0) &&
        vulkan_present_image(&m->present,m->draw.color.image,640,480);
}
int vulkan_menu_readback(VulkanMenu *m,uint8_t *rgba,size_t bytes){return vulkan_draw_readback(&m->draw,rgba,bytes);}
