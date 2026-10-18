#include "level_run.h"
#include "menu_widgets.h"
#include <math.h>

extern uint32_t game_menu_milliseconds(GameApplication *);
extern int game_menu_window_clear(GameApplication *);
extern int game_menu_palette_load_named(GameApplication *,GameAddress);
extern int game_menu_surface_present_rectangle(GameApplication *,const GameUiRect *);
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
static int release_images(GameApplication *a){
    static const GameAddress slots[]={0x597ed8,0x597edc};unsigned i;
    for(i=0;i<2;++i){GameAddress p=get(a,slots[i]);
        if(p && !game_menu_widget_destroy(a,p))return 0;
        put(a,slots[i],0);
    }
    return !a->memory.fault;
}
/*515550: the loading page owns its two existing image records and borrows the
 *window/palette owner. No extra screen, renderer or serialized image mirror. */
int game_level_progress_begin(GameApplication *a,unsigned steps){
    uint32_t old_width=get(a,0x626750),x=320u-(uint32_t)((int32_t)old_width/2);GameAddress p;
    if(!steps || steps>INT32_MAX || get(a,0x597ed8) || get(a,0x597edc))return 0;
    put(a,0x626728,x);put(a,0x62672c,230);
    put(a,0x626738,x-3);put(a,0x62673c,227);put(a,0x626740,x+old_width+4);put(a,0x626744,254);
    put(a,0x62674c,steps);put(a,0x626730,0);
    if(!game_menu_window_clear(a) || !game_menu_palette_load_named(a,0x597ee0))return 0;
    p=game_menu_image_create(a,UINT32_MAX,0x597efc);if(!p)goto fail;put(a,0x597edc,p);
    p=game_menu_image_create(a,UINT32_MAX,0x597f18);if(!p)goto fail;put(a,0x597ed8,p);
    put(a,0x626750,get(a,p+0x28)-get(a,p+0x20));
    game_memory_write_u8(&a->memory,0x59d448,253);
    put(a,0x59d468,1);put(a,0x59d46c,0);put(a,0x626748,game_menu_milliseconds(a));
    return !a->memory.fault;
fail:
    (void)release_images(a);return 0;
}
/*5156c0: the original ignores both message arguments. It advances the strip,
 *draws the existing background, clips the full strip and presents that region.
 *The fractional width uses signed32 inputs and original int64 truncation/low
 *word behavior. Steps beyond the nominal total clip to the actual image. */
int game_level_progress(GameApplication *a,GameAddress message,GameAddress detail){
    GameAddress bg=get(a,0x597edc),bar=get(a,0x597ed8);uint32_t count,total,extent,right;
    GameUiRect region;long double width;(void)message;(void)detail;
    if(!bg || !bar || !(total=get(a,0x62674c)))return 0;
    count=get(a,0x626730)+1;put(a,0x626730,count);
    if(!game_menu_widget_position(a,bg,0,0,0,0) || !game_menu_widget_draw(a,bg,NULL) ||
       !game_menu_widget_position(a,bar,228,388,0,0))return 0;
    width=((long double)(int32_t)get(a,0x626750)/(int32_t)total)*(int32_t)count;
    extent=!isfinite(width) || width< -9223372036854775808.0L || width>=9223372036854775808.0L?
        0:(uint32_t)(uint64_t)(int64_t)truncl(width);
    right=get(a,bar+0x20)+extent;
    put(a,bar+0x154,get(a,bar+0x20));put(a,bar+0x158,get(a,bar+0x24));
    put(a,bar+0x15c,right);put(a,bar+0x160,get(a,bar+0x2c));put(a,bar+0x164,1);
    if(!game_menu_widget_draw(a,bar,NULL))return 0;
    put(a,bar+0x18,1);
    region.left=(int32_t)get(a,bg+0x20);region.top=(int32_t)get(a,bg+0x24);
    region.right=(int32_t)get(a,bg+0x28);region.bottom=(int32_t)get(a,bg+0x2c);
    return game_menu_surface_present_rectangle(a,&region) && !a->memory.fault;
}
/*5157c0: restore the existing draw lane, record wrapped elapsed time and retire
 *both assets. Retrying a closed page does not affect unrelated image owners. */
int game_level_progress_end(GameApplication *a){
    int ok;
    if(!get(a,0x597ed8) && !get(a,0x597edc))return !a->memory.fault;
    put(a,0x597ed4,game_menu_milliseconds(a)-get(a,0x626748));put(a,0x59d46c,1);
    ok=game_menu_window_clear(a);
    if(!release_images(a))ok=0;
    return ok && !a->memory.fault;
}
/*5160e0: publish ordinary values from the authoritative display
 *lanes; callers no longer create guest pointers for output arguments. */
int game_level_viewport_size(GameApplication *a,uint32_t *width,uint32_t *height){
    if(!width || !height)return 0;
    *width=get(a,0x597f68);*height=get(a,0x597f6c);return !a->memory.fault;
}
