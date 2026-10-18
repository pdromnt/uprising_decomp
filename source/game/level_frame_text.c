#include "level_scene_render.h"
#include "level_world_surface.h"
#include "level_hud.h"
#include "game_text_rasterizer.h"
#include "game_initializers.h"
#include <stdio.h>
#include <string.h>
extern int game_application_glyph_begin(GameApplication *,GameTextTarget *,int32_t *);
extern int game_application_glyph_end(GameApplication *);
extern int game_application_palette_quad(GameApplication *,int32_t,int32_t,int32_t,int32_t,uint8_t);
extern int game_application_render_mode(GameApplication *,int);
extern int game_application_frame_present(GameApplication *);
extern int game_menu_projection_bounds(GameApplication *,uint32_t,uint32_t,uint32_t,uint32_t);
extern int game_level_texture_frame(GameApplication *);
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
static int32_t signed_word(uint32_t v){int32_t s;memcpy(&s,&v,4);return s;}
static int32_t floor_shift(int32_t v,unsigned shift){return v>=0?v/(1<<shift):-1-(int32_t)((-(int64_t)v-1)/(1<<shift));}
int game_level_frame_text_bytes(GameApplication *a,int32_t x,int32_t y,const uint8_t *bytes,size_t length){
    GameTextTarget target;int32_t fallback;int ok;
    if(!bytes || !length)return 0;
    put(a,0x62970c,(uint32_t)x);put(a,0x629708,(uint32_t)y);
    if(a->memory.fault || !game_application_glyph_begin(a,&target,&fallback)){
        game_application_log(a,"native text failed: %s\n","glyph target lock");return 0;
    }
    (void)game_text_rasterize_bytes(&a->memory,bytes,length,&target,fallback);
    ok=game_application_glyph_end(a);return ok && !a->memory.fault;
}
/*532870's real consumers supply one integer argument. Reject other formats
 * rather than interpreting a guest variadic stack. Formatted bytes stay local. */
int game_level_frame_00532870(GameApplication *a,int32_t x,int32_t y,GameAddress format,uint32_t value){
    char spec[256],bytes[256];size_t i=0;unsigned conversions=0;char conversion=0;int length;
    for(i=0;i<sizeof(spec);i++){spec[i]=(char)game_memory_read_u8(&a->memory,format+(uint32_t)i);if(!spec[i])break;}
    if(i==sizeof(spec) || a->memory.fault)return 0;
    for(i=0;spec[i];i++)if(spec[i]=='%'){
        ++i;if(spec[i]=='%')continue;
        while(spec[i] && strchr("-+ #0.123456789",spec[i]))++i;
        if(!spec[i] || !strchr("diuoxX",spec[i]) || ++conversions>1)return 0;
        conversion=spec[i];
    }
    length=(conversion=='d' || conversion=='i')?snprintf(bytes,sizeof(bytes),spec,signed_word(value)):
        snprintf(bytes,sizeof(bytes),spec,value);
    if(length<0 || (size_t)length>=sizeof(bytes))return 0;
    return game_level_frame_text_bytes(a,x,y,(const uint8_t *)bytes,(size_t)length+1);
}
int game_level_frame_00532780(GameApplication *a,uint32_t color){
    game_memory_write_u8(&a->memory,0x59d448,(uint8_t)color);return !a->memory.fault;
}
/*4187a0: real alternate frame, all256 palette cells and optional numeric
 * labels, reusing the actual scratch/frame/texture/backend owners. */
int game_level_frame_004187a0(GameApplication *a){
    int32_t width=signed_word(get(a,0x5ac6a0)),height=signed_word(get(a,0x5ac6a4));
    int32_t cell_w=floor_shift(width,4),cell_h=floor_shift(height,4);uint32_t i;
    if(a->memory.fault || !game_menu_projection_bounds(a,0,0,(uint32_t)width,(uint32_t)height) ||
       (game_initializer_004cc490(&a->memory),a->memory.fault) || !game_level_texture_frame(a) || !game_application_render_mode(a,1) ||
       (get(a,0x597f60) && !game_application_surface_begin(a)))return 0;
    for(i=0;i<256;i++){
        uint32_t left=(i&15)*(uint32_t)cell_w,top=(i>>4)*(uint32_t)cell_h;
        if(!game_application_palette_quad(a,signed_word(left+1),signed_word(top+1),
            signed_word(left+(uint32_t)cell_w-1),signed_word(top+(uint32_t)cell_h-1),(uint8_t)i))return 0;
        if(game_memory_read_u8(&a->memory,0x594d5c)){
            uint32_t center=left+(uint32_t)floor_shift(cell_w,1);
            if(!game_application_palette_quad(a,signed_word(center-6),signed_word(top+4),signed_word(center+6),signed_word(top+8),0) ||
               !game_level_frame_00532870(a,signed_word(center-5),signed_word(top+4),0x583cfc,i))return 0;
        }
    }
    return !a->memory.fault && game_application_frame_present(a);
}
