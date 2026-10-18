#include "menu_movie.h"
#include "scene_projection_storage.h"
#include "state_helper_plans.h"
#include "gameplay_state.h"
#include <stdlib.h>
#include <string.h>
extern int game_menu_register_tick(GameApplication *,GameAddress,GameAddress,uint32_t,uint32_t);
extern int game_menu_unregister(GameApplication *,GameAddress,GameAddress,int);
extern int game_menu_palette(GameApplication *,GameAddress);
typedef struct GameMenuMovie {
    GameAddress identity;GameMovieDecoder *decoder;struct GameMenuMovie *next;
} GameMenuMovie;
static uint32_t get(GameApplication *a,GameAddress p,unsigned o){return game_memory_read_u32(&a->memory,p+o);}
static void put(GameApplication *a,GameAddress p,unsigned o,uint32_t v){game_memory_write_u32(&a->memory,p+o,v);}
static GameMenuMovie *find(GameApplication *a,GameAddress identity) {
    GameMenuMovie *movie;for(movie=a->menu_movies;movie;movie=movie->next)if(movie->identity==identity)return movie;return NULL;
}
static void dirty(GameApplication *a,GameAddress p) {
    unsigned o;if(!get(a,p,0x1c))for(o=0;o<16;o+=4)put(a,p,0x30+o,get(a,p,0x20+o));put(a,p,0x1c,1);
}
/*0040a5c0: ordinary movie owner, not a guest Smack handle stored in a word.
 * The historical50 field now holds its stable native owner ID. The decoder owns
 * one pixel buffer; existing geometry/state stays in the bounded E8 widget. */
GameAddress game_menu_movie_create(GameApplication *a,const char *name,uint32_t width,uint32_t height) {
    GameMenuMovie *movie=NULL;const AssetBitmap *frame;GameAddress p=0;size_t length;
    if(!name || (length=strlen(name))>=128 || !width || !height)return 0;
    movie=calloc(1,sizeof(*movie));if(!movie)return 0;
    movie->decoder=game_movie_decoder_open(a,name);if(!movie->decoder)goto fail;
    frame=game_movie_decoder_frame(movie->decoder);if(!frame)goto fail;
    p=scene_projection_storage_allocate(0xe8,"portable movie widget");if(!p)goto fail;
    game_memory_fill_u8(&a->memory,p,0xe8,0,0);state_helper_construct_packet(&a->memory,p,UINT32_MAX);
    gameplay_state_initialize_presentation_packet(&a->memory,p);
    put(a,p,0,0x571c40);put(a,p,0x50,p);put(a,p,0xe0,width);put(a,p,0xe4,height);
    put(a,p,0xd8,width!=UINT32_MAX);put(a,p,0x28,(width==UINT32_MAX?frame->width:width)-1);
    put(a,p,0x2c,(width==UINT32_MAX?frame->height:height)-1);
    if(!game_memory_write(&a->memory,p+0x54,name,length+1) || a->memory.fault)goto fail;
    movie->identity=p;movie->next=a->menu_movies;a->menu_movies=movie;return p;
fail:if(movie){game_movie_decoder_close(movie->decoder);free(movie);}if(p)scene_projection_storage_free(p);return 0;
}
/*0040a810/0040a8e0: register the actual native playback tick and mark dirty
 * only when the codec has a frame ready. Last-frame presentation ends playback
 * but the owner keeps its tick registration until destruction, as originally. */
int game_menu_movie_play(GameApplication *a,GameAddress p) {
    if(!find(a,p))return 0;
    if(!get(a,p,0xd4)) {
        if(!get(a,p,0x44) && !game_menu_register_tick(a,get(a,0x59d4c0,0),p,0x40a8e0,0))return 0;
        put(a,p,0xd4,1);put(a,p,0x44,1);
    }
    return !a->memory.fault;
}
int game_menu_movie_volume(GameApplication *a,GameAddress p,uint32_t volume) {
    GameMenuMovie *movie=find(a,p);if(!movie)return 0;
    game_movie_decoder_volume(movie->decoder,volume);return !a->memory.fault;
}
int game_menu_movie_tick(GameApplication *a,GameAddress p) {
    GameMenuMovie *movie=find(a,p);if(!movie)return 0;
    if(get(a,p,0xd4) && game_movie_decoder_ready(movie->decoder))dirty(a,p);
    return !a->memory.fault;
}
/*0040a980/0040aad0: decode into the one native frame buffer, update palette
 * only on codec changes, present the actual frame and invoke its native completion
 * callback. Source orientation avoids a second flip/copy buffer each frame. */
int game_menu_movie_draw(GameApplication *a,GameAddress p,const GameUiRect *clip) {
    GameMenuMovie *movie=find(a,p);const AssetBitmap *frame;GameUiRect source,destination;
    uint32_t x,y,width,height,callback;int last,changed;unsigned i;(void)clip;
    if(!movie)return 0;
    if(!get(a,p,0xd4))return !a->memory.fault;
    if(!game_movie_decoder_decode(movie->decoder,&last,&changed))return 0;
    frame=game_movie_decoder_frame(movie->decoder);
    if(changed) {
        for(i=0;i<256;++i) {
            game_memory_write_u8(&a->memory,0x5ab0f0+i*4,frame->palette_bgra[i*4+2]);
            game_memory_write_u8(&a->memory,0x5ab0f1+i*4,frame->palette_bgra[i*4+1]);
            game_memory_write_u8(&a->memory,0x5ab0f2+i*4,frame->palette_bgra[i*4]);
        }
        if(!game_menu_palette(a,0x5ab0f0))return 0;
    }
    x=get(a,p,0x20);y=get(a,p,0x24);width=get(a,p,0xd8)?get(a,p,0xe0):frame->width;height=get(a,p,0xd8)?get(a,p,0xe4):frame->height;
    source.left=source.top=0;source.right=(int32_t)frame->width;source.bottom=(int32_t)frame->height;
    destination.left=(int32_t)x;destination.top=(int32_t)y;destination.right=(int32_t)(x+width);destination.bottom=(int32_t)(y+height);
    if(!game_menu_draw_bitmap(a,frame,&source,&destination,0,0))return 0;
    if(last) {
        put(a,0x58fd3c,0,1);put(a,p,0x18,1);put(a,p,0xd4,0);callback=get(a,p,0x48);
        if(callback && !game_menu_video_action(a,callback,get(a,p,0x4c)))return 0;
    }
    return !a->memory.fault;
}
/*0040a760/0040ab30: callbacks and visibility end before the native decoder and
 * its widget. Neither SDK handles nor borrowed frames survive their owner. */
int game_menu_movie_destroy(GameApplication *a,GameAddress p) {
    GameMenuMovie **link=&a->menu_movies,*movie;
    while(*link && (*link)->identity!=p)link=&(*link)->next;
    if(!*link)return 0;
    if(get(a,p,0x44) && !game_menu_unregister(a,get(a,0x59d4c0,0),p,0))return 0;
    put(a,p,0x44,0);
    if(get(a,p,0x14) && !game_menu_widget_visible(a,p,0))return 0;
    movie=*link;*link=movie->next;game_movie_decoder_close(movie->decoder);free(movie);
    return scene_projection_storage_free(p);
}
void game_menu_movies_dispose(GameApplication *a) {
    while(a->menu_movies) {
        GameMenuMovie *movie=a->menu_movies;GameAddress identity=movie->identity;
        if(!game_menu_movie_destroy(a,identity) && (movie=find(a,identity))!=NULL) {
            a->menu_movies=movie->next;game_movie_decoder_close(movie->decoder);
            scene_projection_storage_free(movie->identity);free(movie);
        }
    }
}
