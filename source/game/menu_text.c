#include "menu_widgets.h"
#include "scene_projection_storage.h"
#include "state_helper_plans.h"
#include "game_text_rasterizer.h"
#include <stdlib.h>
#include <string.h>
/* The accelerated frame target is an actual renderer service. It remains a
 * genuine link boundary until its native framebuffer owner is connected. */
extern int game_menu_accelerated_text_begin(GameApplication *,GameAddress,const GameUiRect *,const GameUiRect *,GameTextTarget *,int32_t *);
extern int game_menu_accelerated_text_end(GameApplication *);

static uint32_t get(GameApplication *a,GameAddress p,unsigned offset) {return game_memory_read_u32(&a->memory,p+offset);}
static void put(GameApplication *a,GameAddress p,unsigned offset,uint32_t value) {game_memory_write_u32(&a->memory,p+offset,value);}
static void dirty(GameApplication *a,GameAddress p) {
    unsigned i;
    if(!get(a,p,0x1cu))for(i=0;i<16u;i+=4u)put(a,p,0x30u+i,get(a,p,0x20u+i));
    put(a,p,0x1cu,1u);
}
static char *text_read(GameApplication *a,GameAddress source,size_t *length) {
    size_t size;char *text;
    for(size=0;size<65536u && !a->memory.fault;++size)
        if(!game_memory_read_u8(&a->memory,source+(uint32_t)size))break;
    if(size==65536u || a->memory.fault)return 0;
    text=malloc(size+1u);if(!text)return 0;
    if(!game_memory_read(&a->memory,source,text,size+1u)){free(text);return 0;}
    *length=size;return text;
}
/* 0040bd30: replace the owned string, then fill/paint a nonempty value. Empty
 * strings intentionally keep the previous pixels, as in the original branch.
 * The string page is authoritative; no retained serialized/typed text mirror. */
static int text_set_bytes(GameApplication *a,GameAddress p,const char *text,size_t length,uint32_t layout) {
    GameAddress replacement=scene_projection_storage_allocate(length+1u,"portable menu text string");
    GameAddress previous=get(a,p,0x16cu);uint32_t color;int result=1,selected;
    if(!replacement)return 0;
    if(!game_memory_write(&a->memory,replacement,text,length+1u)) {scene_projection_storage_free(replacement);return 0;}
    selected=previous && get(a,0x598118,0)==previous;
    if(previous && !scene_projection_storage_free(previous)) {scene_projection_storage_free(replacement);return 0;}
    put(a,p,0x16cu,replacement);
    if(selected)put(a,0x598118,0,replacement);
    if(length) {
        color=0x02000000u|(get(a,p,0x178u)&255u)|((get(a,p,0x17cu)&255u)<<8)|((get(a,p,0x180u)&255u)<<16);
        result=game_menu_text_surface_fill(a,p,get(a,p,0x170u)) &&
            game_menu_text_surface_paint(a,p,get(a,p,0x174u),text,color,(int32_t)get(a,p,0x184u),
                (int32_t)get(a,p,0x188u),layout,game_memory_read_u32(&a->memory,0x5840f8u)!=0u);
    }
    put(a,p,0x18u,1u);dirty(a,p);
    return result && !a->memory.fault;
}
int game_menu_text_image_set(GameApplication *a,GameAddress p,GameAddress source,uint32_t layout) {
    size_t length;char *text=text_read(a,source,&length);int result;
    if(!text || get(a,p,0)!=0x571c80u){free(text);return 0;}
    result=text_set_bytes(a,p,text,length,layout);free(text);return result;
}
int game_menu_text_image_set_bytes(GameApplication *a,GameAddress p,const char *text,uint32_t layout) {
    if(!text || get(a,p,0)!=0x571c80u || strlen(text)>4096)return 0;
    return text_set_bytes(a,p,text,strlen(text),layout);
}
/* 0040ba80 -> 0040b020 -> 0040bd30. Legacy bounds/style remain in this owned
 * record. The window owns SDL pixel surfaces under fixed-width identities. */
GameAddress game_menu_text_image_create_bytes(GameApplication *a,const char *text,const GameMenuTextStyle *style) {
    GameAddress p;size_t length;uint32_t width,height;int32_t measured_width,measured_height;
    if(!style || style->font>=7u)return 0;
    if(!text)return 0;
    length=strlen(text);if(length>=65536)return 0;
    width=style->width;height=style->height;
    if(width==UINT32_MAX) {
        if(!game_menu_text_measure(a,style->font,text,&measured_width,&measured_height) ||
            measured_width<0 || measured_height<0)return 0;
        width=(uint32_t)measured_width;height=(uint32_t)measured_height;
    }
    p=scene_projection_storage_allocate(0x190u,"portable menu text image");
    if(!p)return 0;
    game_memory_fill_u8(&a->memory,p,0x190u,0u,0);
    state_helper_construct_packet(&a->memory,p,0u);
    put(a,p,0,0x571c80u);put(a,p,0xb0u,style->fill?100u:101u);
    put(a,p,0x28u,width);put(a,p,0x2cu,height);put(a,p,0x38u,width);put(a,p,0x3cu,height);
    put(a,p,0x44u,108u);put(a,p,0x48u,7u);put(a,p,0x4cu,height);put(a,p,0x50u,width);
    put(a,p,0xacu,game_memory_read_u32(&a->memory,0x5840f8u)?0x840u:0x40u);
    put(a,p,0x170u,style->fill);put(a,p,0x174u,style->font);
    put(a,p,0x178u,style->red);put(a,p,0x17cu,style->green);put(a,p,0x180u,style->blue);
    put(a,p,0x184u,style->x);put(a,p,0x188u,style->y);
    if(width && height) {
        if(!game_menu_text_surface_create(a,p,width,height,style->fill))goto fail;
        put(a,p,0xb4u,p);
    } else if(length)goto fail;
    if(!text_set_bytes(a,p,text,length,0u))goto fail;
    return p;
fail:
    (void)game_menu_text_image_release(a,p);scene_projection_storage_free(p);return 0;
}
GameAddress game_menu_text_image_create(GameApplication *a,GameAddress source,const GameMenuTextStyle *style) {
    size_t length;char *text=text_read(a,source,&length);GameAddress p;
    if(!text)return 0;
    p=game_menu_text_image_create_bytes(a,text,style);free(text);return p;
}
int game_menu_text_image_release(GameApplication *a,GameAddress p) {
    GameAddress text=get(a,p,0x16cu);
    if(text && !scene_projection_storage_free(text))return 0;
    /*The quick-game selection borrows this exact owned string. Retirement
     *invalidates the view before a freed slot can be reused by another label. */
    if(text && get(a,0x598118,0)==text)put(a,0x598118,0,0);
    put(a,p,0x16cu,0u);
    if(!get(a,p,0x140u) && !game_menu_text_surface_release(a,p))return 0;
    put(a,p,0xb4u,0u);return !a->memory.fault;
}
int game_menu_text_image_draw(GameApplication *a,GameAddress p,const GameUiRect *clip) {
    GameUiRect source,destination;int32_t left=(int32_t)get(a,p,0x20u),top=(int32_t)get(a,p,0x24u);
    GameAddress value=get(a,p,0x16c);size_t length;char *text;int restored,accelerated;
    if(!get(a,p,0xb4))return 0;
    text=text_read(a,value,&length);if(!text)return 0;
    accelerated=game_memory_read_u32(&a->memory,0x5840f8)!=0 && length<30u;
    if(accelerated) {
        GameTextTarget target;int32_t fallback;int result;
        game_memory_write_u8(&a->memory,0x59d448,get(a,p,0x17c)==255?246:0);
        if(get(a,p,0x17c)==255) {
            game_memory_write_u8(&a->memory,0x59d44c,0);game_memory_write_u32(&a->memory,0x59d468,0);
        } else {
            if(!game_menu_text_surface_white(a,get(a,p,0xb4))){free(text);return 0;}
            game_memory_write_u32(&a->memory,0x59d468,1);
        }
        game_memory_write_u32(&a->memory,0x59d450,(uint32_t)clip->left);
        game_memory_write_u32(&a->memory,0x59d458,(uint32_t)clip->top);
        game_memory_write_u32(&a->memory,0x59d454,(uint32_t)clip->right);
        game_memory_write_u32(&a->memory,0x59d45c,(uint32_t)clip->bottom);
        game_memory_write_u32(&a->memory,0x62970c,(uint32_t)left+5);
        game_memory_write_u32(&a->memory,0x629708,(uint32_t)top+3);
        destination.left=left;destination.top=top;destination.right=(int32_t)get(a,p,0x28);destination.bottom=(int32_t)get(a,p,0x2c);
        if(!game_menu_accelerated_text_begin(a,get(a,p,0x17c)==255?0:get(a,p,0xb4),&destination,clip,&target,&fallback)){free(text);return 0;}
        (void)game_text_rasterize(&a->memory,value,&target,fallback);
        result=game_menu_accelerated_text_end(a);
        game_memory_write_u32(&a->memory,0x59d468,1);
        game_memory_write_u32(&a->memory,0x59d450,0);game_memory_write_u32(&a->memory,0x59d458,0);
        game_memory_write_u32(&a->memory,0x59d454,639);game_memory_write_u32(&a->memory,0x59d45c,479);
        free(text);return result && !a->memory.fault;
    }
    restored=game_menu_text_surface_restore(a,get(a,p,0xb4));
    if(restored<0){free(text);return 0;}
    if(restored) {
        uint32_t color=0x02000000u|(get(a,p,0x178)&255)|((get(a,p,0x17c)&255)<<8)|((get(a,p,0x180)&255)<<16);
        if(length && (!game_menu_text_surface_fill(a,get(a,p,0xb4),get(a,p,0x170)) ||
            !game_menu_text_surface_paint(a,get(a,p,0xb4),get(a,p,0x174),text,color,(int32_t)get(a,p,0x184),
                (int32_t)get(a,p,0x188),0,game_memory_read_u32(&a->memory,0x5840f8)!=0))){free(text);return 0;}
        put(a,p,0x18,1);dirty(a,p);
    }
    free(text);
    destination.left=left;destination.top=top;
    destination.right=(int32_t)get(a,p,0x28u);destination.bottom=(int32_t)get(a,p,0x2cu);
    if(destination.left<clip->left)destination.left=clip->left;
    if(destination.top<clip->top)destination.top=clip->top;
    if(destination.right>clip->right)destination.right=clip->right;
    if(destination.bottom>clip->bottom)destination.bottom=clip->bottom;
    if(destination.left>=destination.right || destination.top>=destination.bottom)return 1;
    source.left=(int32_t)((uint32_t)destination.left-(uint32_t)left);
    source.top=(int32_t)((uint32_t)destination.top-(uint32_t)top);
    source.right=(int32_t)((uint32_t)destination.right-(uint32_t)left);
    source.bottom=(int32_t)((uint32_t)destination.bottom-(uint32_t)top);
    return game_menu_text_surface_draw(a,get(a,p,0xb4u),&source,&destination,get(a,p,0xb0u)==101u);
}
