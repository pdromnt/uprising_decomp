#include "ui_geometry.h"
#include <stdlib.h>
#include <string.h>

static int indexed_span_valid(const GameMenuPixelSpan *span){
    return span && span->pixels && span->width>0 && span->height>0 && span->stride>=span->width &&
        (uint64_t)(uint32_t)(span->height-1)*(uint32_t)span->stride+(uint32_t)span->width<=span->bytes;
}
int game_ui_indexed_copy(const GameMenuPixelSpan *destination,const GameMenuPixelSpan *source,
        const GameUiRect *from,const GameUiRect *to,int transparent,uint8_t key,int mirror){
    GameMenuPixelSpan view;GameUiRect rectangle;uint8_t *copy=NULL;int64_t sx,sy,dx,dy;
    int32_t x,y,left,top,right,bottom;uintptr_t a,b;
    if(!indexed_span_valid(destination) || !indexed_span_valid(source) || !from || !to)return 0;
    sx=(int64_t)from->right-from->left;sy=(int64_t)from->bottom-from->top;
    dx=(int64_t)to->right-to->left;dy=(int64_t)to->bottom-to->top;
    if(dx<=0 || dy<=0 || sx<=0 || sy<=0)return 1;
    if(from->left<0 || from->top<0 || from->right>source->width || from->bottom>source->height)return 0;
    left=to->left<0?0:to->left;top=to->top<0?0:to->top;
    right=to->right>destination->width?destination->width:to->right;
    bottom=to->bottom>destination->height?destination->height:to->bottom;
    if(left>=right || top>=bottom)return 1;
    view=*source;rectangle=*from;
    a=(uintptr_t)source->pixels;b=(uintptr_t)destination->pixels;
    if((a<=b && b-a<source->bytes) || (b<a && a-b<destination->bytes)){
        if((uint64_t)sx*(uint64_t)sy>SIZE_MAX)return 0;
        copy=malloc((size_t)sx*(size_t)sy);if(!copy)return 0;
        for(y=0;y<sy;++y)memcpy(copy+(size_t)y*(size_t)sx,
            source->pixels+(size_t)(from->top+y)*source->stride+from->left,(size_t)sx);
        view.pixels=copy;view.bytes=(size_t)sx*(size_t)sy;view.width=view.stride=(int32_t)sx;view.height=(int32_t)sy;
        rectangle.left=rectangle.top=0;rectangle.right=(int32_t)sx;rectangle.bottom=(int32_t)sy;
    }
    for(y=top;y<bottom;++y){
        int64_t row=((int64_t)y-to->top)*sy/dy;
        const uint8_t *input=view.pixels+(size_t)(rectangle.top+(mirror?sy-1-row:row))*view.stride;
        uint8_t *output=destination->pixels+(size_t)y*destination->stride;
        if(!mirror && !transparent && sx==dx){
            memcpy(output+left,input+rectangle.left+(int64_t)left-to->left,(size_t)(right-left));continue;
        }
        for(x=left;x<right;++x){
            int64_t column=((int64_t)x-to->left)*sx/dx;
            uint8_t value=input[rectangle.left+(mirror?sx-1-column:column)];
            if(!transparent || value!=key)output[x]=value;
        }
    }
    free(copy);return 1;
}

int game_ui_indexed_borders(uint8_t *pixels,size_t bytes,int32_t width,int32_t height,int32_t stride,
    const GameUiRect *rectangles,const uint8_t *colors,size_t count) {
    size_t i;if(!pixels || !rectangles || !colors || width<=0 || height<=0 || stride<width || count>4096 ||
        (uint64_t)(uint32_t)stride*(uint32_t)(height-1)+(uint32_t)width>bytes)return 0;
    for(i=0;i<count;++i){GameUiRect r=rectangles[i];int32_t first,last,x,y;
        if(r.right<r.left || r.bottom<r.top)return 0;
        first=r.left<0?0:r.left;last=r.right>=width?width-1:r.right;
        for(x=first;x<=last;++x){if(r.top>=0 && r.top<height)pixels[(size_t)r.top*stride+(uint32_t)x]=colors[i];
            if(r.bottom>=0 && r.bottom<height)pixels[(size_t)r.bottom*stride+(uint32_t)x]=colors[i];}
        first=r.top<0?0:r.top;last=r.bottom>=height?height-1:r.bottom;
        for(y=first;y<=last;++y){if(r.left>=0 && r.left<width)pixels[(size_t)y*stride+(uint32_t)r.left]=colors[i];
            if(r.right>=0 && r.right<width)pixels[(size_t)y*stride+(uint32_t)r.right]=colors[i];}}
    return 1;
}
static uint32_t mask_owned,mask_memory_based;
void game_ui_mask_stats(uint32_t *owned,uint32_t *memory_based){
    if(owned)*owned=mask_owned;
    if(memory_based)*memory_based=mask_memory_based;
}

static int draw_owned_mask_rectangle(GameRenderVertex vertices[4],
    float left,float top,float right,float bottom,
    const GameUiMaskOps *ops){
    vertices[0].x=left;vertices[0].y=top;
    vertices[1].x=right;vertices[1].y=top;
    vertices[2].x=right;vertices[2].y=bottom;
    vertices[3].x=left;vertices[3].y=bottom;
    vertices[0].oow=vertices[1].oow=65536.0f;
    vertices[2].oow=vertices[3].oow=65536.0f;
    return (!ops->publish || ops->publish(ops->context,vertices)) &&
        ops->draw(ops->context,vertices);
}
GameUiResult game_ui_draw_screen_mask_owned(int enabled,
    const GameUiMaskFrame *frame,GameRenderVertex vertices[4],
    const GameUiMaskOps *ops,int *used_last_corner){
    unsigned region;
    if(!ops || !ops->clear || !ops->begin || !ops->draw ||
       !used_last_corner)return GAME_UI_RESULT_FAULT;
    *used_last_corner=0;
    if(!enabled){
        if(!ops->clear(ops->context,0x0083b9beu,0xffffu))
            return GAME_UI_RESULT_FAULT;
        ++mask_owned;return GAME_UI_RESULT_OK;
    }
    if(!frame || !vertices || !ops->begin(ops->context) ||
       !draw_owned_mask_rectangle(vertices,
           (float)(int32_t)frame->bounds[0],
           (float)(int32_t)frame->bounds[1],
           (float)(int32_t)frame->bounds[2],
           (float)(int32_t)frame->bounds[3],ops))
        return GAME_UI_RESULT_FAULT;
    if(frame->mode==7u){
        for(region=0;region<2u;region++){
            const int32_t *bounds=frame->overlays[region];
            if(!draw_owned_mask_rectangle(vertices,(float)bounds[0],
                (float)bounds[1],(float)bounds[2],(float)bounds[3],ops))
                return GAME_UI_RESULT_FAULT;
        }
        *used_last_corner=1;
    }
    ++mask_owned;return GAME_UI_RESULT_OK;
}

static float bits_float(uint32_t bits) {
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static int load_vertices(GameMemory *memory, GameAddress address,
    GameRenderVertex vertices[4]) {
    unsigned offset;
    for (offset = 0u; offset < sizeof(*vertices) * 4u; offset += 4u) {
        uint32_t bits = game_memory_read_u32(memory, address + offset);
        memcpy((uint8_t *)vertices + offset, &bits, sizeof(bits));
    }
    return !memory->fault;
}

static int store_vertices(GameMemory *memory, GameAddress address,
    const GameRenderVertex vertices[4]) {
    unsigned offset;
    for (offset = 0u; offset < sizeof(*vertices) * 4u; offset += 4u) {
        uint32_t bits;
        memcpy(&bits, (const uint8_t *)vertices + offset, sizeof(bits));
        game_memory_write_u32(memory, address + offset, bits);
    }
    return !memory->fault;
}

static int draw_mask_rectangle(GameMemory *memory, GameAddress scratch,
    GameRenderVertex vertices[4], float left, float top, float right,
    float bottom, const GameUiMaskOps *ops) {
    vertices[0].x = left;  vertices[0].y = top;
    vertices[1].x = right; vertices[1].y = top;
    vertices[2].x = right; vertices[2].y = bottom;
    vertices[3].x = left;  vertices[3].y = bottom;
    vertices[0].oow = vertices[1].oow = 65536.0f;
    vertices[2].oow = vertices[3].oow = 65536.0f;
    return store_vertices(memory, scratch, vertices) && ops->draw(ops->context, vertices);
}

static float truncated_global_float(GameMemory *memory, GameAddress address) {
    return (float)(int32_t)bits_float(game_memory_read_u32(memory, address));
}
static float integer_global_float(GameMemory *memory, GameAddress address) {
    return (float)(int32_t)game_memory_read_u32(memory, address);
}

GameUiResult game_ui_draw_screen_mask(GameMemory *memory, int enabled,
    GameAddress scratch, const GameUiMaskOps *ops, uint32_t *result_eax) {
    GameRenderVertex vertices[4];
    float left, top, right, bottom;
    uint32_t mode;
    ++mask_memory_based;
    if (!ops || !ops->clear || !ops->begin || !ops->draw || !result_eax)
        return GAME_UI_RESULT_FAULT;
    if (!enabled)
        return ops->clear(ops->context, 0x0083b9beu, 0xffffu) ? GAME_UI_RESULT_OK : GAME_UI_RESULT_FAULT;
    if (!ops->begin(ops->context) || !load_vertices(memory, scratch, vertices))
        return GAME_UI_RESULT_FAULT;
    left = truncated_global_float(memory, 0x006290d4u);
    top = truncated_global_float(memory, 0x006290d0u);
    right = truncated_global_float(memory, 0x006290dcu);
    bottom = truncated_global_float(memory, 0x006290d8u);
    if (memory->fault || !draw_mask_rectangle(memory, scratch, vertices,
            left, top, right, bottom, ops))
        return GAME_UI_RESULT_FAULT;
    mode = game_memory_read_u32(memory, 0x00604760u);
    *result_eax = mode;
    if (memory->fault) return GAME_UI_RESULT_FAULT;
    if (mode == 7u) {
        left = integer_global_float(memory, 0x006027b8u);
        top = integer_global_float(memory, 0x006027bcu);
        right = integer_global_float(memory, 0x006027c0u);
        bottom = integer_global_float(memory, 0x006027c4u);
        if (memory->fault || !draw_mask_rectangle(memory, scratch, vertices,
                left, top, right, bottom, ops))
            return GAME_UI_RESULT_FAULT;
        left = integer_global_float(memory, 0x006024f0u);
        top = integer_global_float(memory, 0x006024f4u);
        right = integer_global_float(memory, 0x006024f8u);
        bottom = integer_global_float(memory, 0x006024fcu);
        if (memory->fault || !draw_mask_rectangle(memory, scratch, vertices,
                left, top, right, bottom, ops))
            return GAME_UI_RESULT_FAULT;
        *result_eax = scratch + 3u * (uint32_t)sizeof(GameRenderVertex);
    }
    return GAME_UI_RESULT_OK;
}

static GameUiResult draw_image(const GameUiImageOps *ops, const GameUiRect *source,
    const GameUiRect *destination, const GameUiImage *image, int depth_write) {
    return ops && ops->draw && ops->draw(ops->context, source, destination, image,
        0x3f000000u, depth_write) ? GAME_UI_RESULT_OK : GAME_UI_RESULT_FAULT;
}

GameUiResult game_ui_draw_marker(GameMemory *memory, GameAddress pixels,
    const GameUiImageOps *ops) {
    uint32_t index = game_memory_read_u32(memory, 0x0060545cu);
    if (memory->fault || index > 1u) return GAME_UI_RESULT_FAULT;
    int32_t x = (int32_t)game_memory_read_u32(memory, 0x00605448u + index * 8u);
    int32_t y = (int32_t)game_memory_read_u32(memory, 0x0060544cu + index * 8u);
    GameUiRect rectangle;
    GameUiImage image = {640u, 480u, pixels};
    if (memory->fault) return GAME_UI_RESULT_FAULT;
    /*Outside markers have no drawable area; reject them before arithmetic.
     *This also bounds the original +/-5 operations on portable signed ints. */
    if (x <= -5 || x >= 645 || y <= -5 || y >= 485)
        return GAME_UI_RESULT_OK;
    rectangle.left=x-5; rectangle.top=y-5;
    rectangle.right=x+5; rectangle.bottom=y+5;
    if (rectangle.left < 0) rectangle.left = 0;
    if (rectangle.top < 0) rectangle.top = 0;
    if (rectangle.right > 640) rectangle.right = 640;
    if (rectangle.bottom > 480) rectangle.bottom = 480;
    if (rectangle.left >= rectangle.right || rectangle.top >= rectangle.bottom)
        return GAME_UI_RESULT_OK;
    return ops && ops->draw && ops->draw(ops->context, &rectangle, &rectangle,
        &image, 0x3f800000u, 1) ? GAME_UI_RESULT_OK : GAME_UI_RESULT_FAULT;
}

GameUiResult game_ui_draw_image_rectangle(int32_t left, int32_t top,
    int32_t right, int32_t bottom, uint32_t pitch, uint32_t height,
    GameAddress pixels, const GameUiImageOps *ops) {
    GameUiImage image = {(uint16_t)pitch, (uint16_t)height, pixels};
    GameUiRect source = {0, 0, (int32_t)(uint16_t)pitch, (int32_t)(uint16_t)height};
    GameUiRect destination = {left, top, right, bottom};
    return draw_image(ops, &source, &destination, &image, 0);
}

GameUiResult game_ui_draw_image_list(GameMemory *memory, GameAddress node,
    const GameUiImageOps *ops) {
    while (node) {
        int32_t x = (int32_t)(int16_t)game_memory_read_u16(memory, node);
        int32_t y = (int32_t)(int16_t)game_memory_read_u16(memory, node + 2u);
        GameUiImage image;
        GameUiRect source, destination;
        image.pitch = game_memory_read_u16(memory, node + 4u);
        image.height = game_memory_read_u16(memory, node + 6u);
        image.pixels = game_memory_read_u32(memory, node + 8u);
        source.left = source.top = 0;
        source.right = (int32_t)(int16_t)image.pitch;
        source.bottom = (int32_t)(int16_t)image.height;
        destination.left = x; destination.top = y;
        destination.right = x + (int32_t)(int16_t)image.pitch;
        destination.bottom = y + (int32_t)(int16_t)image.height;
        if (memory->fault || draw_image(ops, &source, &destination, &image, 0) != GAME_UI_RESULT_OK)
            return GAME_UI_RESULT_FAULT;
        node = game_memory_read_u32(memory, node + 0x0cu);
        if (memory->fault) return GAME_UI_RESULT_FAULT;
    }
    return GAME_UI_RESULT_OK;
}
