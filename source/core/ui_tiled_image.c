#include "ui_tiled_image.h"
#include <string.h>
static float tile_step(int32_t destination,int32_t source) {
    volatile double quotient=(double)destination/(double)source;
    volatile float result=(float)(quotient*32.0);return result;
}
static float tile_advance(float value,float step) {
    volatile float result=(float)((long double)value+(long double)step);return result;
}
static float adjusted(float value,float epsilon) {
    long double result=(long double)value+(long double)epsilon;
    result-=(long double)epsilon;return (float)result;
}
void game_ui_tile_vertices(GameRenderVertex output[4],float x1,float y1,float x2,float y2,float s,float t,float oow,float epsilon) {
    GameRenderVertex zero={0};unsigned i;
    for(i=0;i<4;i++){output[i]=zero;output[i].oow=oow;}
    output[0].x=output[3].x=adjusted(x1,epsilon);output[1].x=output[2].x=adjusted(x2,epsilon);
    output[0].y=output[1].y=adjusted(y1,epsilon);output[2].y=output[3].y=adjusted(y2,epsilon);
    output[1].sow0=output[2].sow0=(float)((long double)oow*s);
    output[2].tow0=output[3].tow0=(float)((long double)oow*t);
}
GameUiTiledImageResult game_ui_draw_tiled_image(const GameUiIndexedImage *image,const GameUiRect *source,
    const GameUiRect *destination,float oow,float epsilon,const GameUiTileOps *ops) {
    int32_t width,height;int64_t x,y;float xstep,ystep,y1;uint8_t tile[1024]={0};
    if(!image || !source || !destination || !image->pixels || !image->stride ||
        image->width>image->stride || (uint64_t)image->stride*image->height>image->size ||
        source->left<0 || source->top<0 || source->right<=source->left || source->bottom<=source->top ||
        (uint32_t)source->right>image->width || (uint32_t)source->bottom>image->height ||
        (int64_t)destination->right-destination->left>INT32_MAX || (int64_t)destination->right-destination->left<INT32_MIN ||
        (int64_t)destination->bottom-destination->top>INT32_MAX || (int64_t)destination->bottom-destination->top<INT32_MIN)return GAME_UI_TILED_IMAGE_INVALID;
    width=source->right-source->left;height=source->bottom-source->top;
    if(!ops || !ops->begin || !ops->draw_tile || !ops->finish || !ops->begin(ops->context))return GAME_UI_TILED_IMAGE_FAULT;
    xstep=tile_step(destination->right-destination->left,width);ystep=tile_step(destination->bottom-destination->top,height);y1=(float)destination->top;
    /* Retain the original zero-height final row when height is divisible by32. */
    for(y=0;y<=height;y+=32) {
        int32_t th=(int32_t)(height-y);float y2,x1=(float)destination->left;if(th>32)th=32;
        if(y==height && height%32)break;
        y2=th==32?tile_advance(y1,ystep):(float)destination->bottom;
        for(x=0;x<width;x+=32) {
            int32_t tw=(int32_t)(width-x),row;float x2;GameRenderVertex vertices[4];if(tw>32)tw=32;
            for(row=0;row<th;row++)memcpy(tile+(size_t)row*32,image->pixels+(size_t)(source->top+y+row)*image->stride+source->left+x,(size_t)tw);
            x2=tw==32?tile_advance(x1,xstep):(float)destination->right;
            game_ui_tile_vertices(vertices,x1,y1,x2,y2,(float)(tw*8),(float)(th*8),oow,epsilon);
            if(!ops->draw_tile(ops->context,tile,vertices))return GAME_UI_TILED_IMAGE_FAULT;
            x1=x2;
        }
        y1=y2;
    }
    return ops->finish(ops->context)?GAME_UI_TILED_IMAGE_OK:GAME_UI_TILED_IMAGE_FAULT;
}

GameUiTiledImageResult game_ui_draw_fullscreen_image(const GameUiIndexedImage *image,const GameUiTileOps *ops) {
    unsigned x,y,row;uint8_t tile[1024];
    if(!image || !image->pixels || image->width<640 || image->height<480 || image->stride<640 ||
        (uint64_t)image->stride*479u+640u>image->size)return GAME_UI_TILED_IMAGE_INVALID;
    if(!ops || !ops->begin || !ops->draw_tile || !ops->finish || !ops->begin(ops->context))return GAME_UI_TILED_IMAGE_FAULT;
    for(y=0;y<480;y+=32)for(x=0;x<640;x+=32) {
        GameRenderVertex vertices[4];
        for(row=0;row<32;row++)memcpy(tile+row*32,image->pixels+(size_t)(y+row)*image->stride+x,32);
        game_screen_fullscreen_tile_vertices(vertices,x,y,32);
        if(!ops->draw_tile(ops->context,tile,vertices))return GAME_UI_TILED_IMAGE_FAULT;
    }
    return ops->finish(ops->context)?GAME_UI_TILED_IMAGE_OK:GAME_UI_TILED_IMAGE_FAULT;
}
