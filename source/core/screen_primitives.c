#include "screen_primitives.h"

#include <string.h>

static float integer_float(int32_t value) {
    volatile float rounded = (float)value;
    return rounded;
}

static float integer_add_one_float(int32_t value) {
    volatile float rounded = (float)((long double)value + 1.0L);
    return rounded;
}

static void set_color(GameRenderVertex *vertex, uint8_t red,
    uint8_t green, uint8_t blue) {
    vertex->r = integer_float((int32_t)red);
    vertex->g = integer_float((int32_t)green);
    vertex->b = integer_float((int32_t)blue);
}

void game_screen_build_point(GameRenderVertex *output,
    const GameRenderVertex *inherited, int32_t x, int32_t y,
    uint8_t red, uint8_t green, uint8_t blue) {
    *output = *inherited;
    output->x = integer_float(x);
    output->y = integer_float(y);
    set_color(output, red, green, blue);
    output->oow = 0.5f;
}

void game_screen_build_vertical_line(GameRenderVertex output[2],
    const GameRenderVertex *inherited, int32_t x, int32_t y,
    uint8_t red, uint8_t green, uint8_t blue) {
    game_screen_build_point(&output[0], inherited, x, y, red, green, blue);
    output[0].oow = 1.0f;
    output[1] = output[0];
    output[1].y = (float)((long double)output[0].y + 1.0L);
}

void game_screen_build_segment(GameRenderVertex output[2],
    const GameRenderVertex inherited[2], const int32_t coordinates[4],
    uint8_t red, uint8_t green, uint8_t blue) {
    output[0] = inherited[0];
    output[1] = inherited[1];
    output[0].x = integer_float(coordinates[0]);
    output[0].y = integer_float(coordinates[1]);
    output[1].x = integer_float(coordinates[2]);
    output[1].y = integer_float(coordinates[3]);
    set_color(&output[0], red, green, blue);
    set_color(&output[1], red, green, blue);
    output[0].oow = output[1].oow = 0.5f;
}

void game_screen_build_quad(GameRenderVertex output[4],
    const GameRenderVertex inherited[4], int32_t left, int32_t top,
    int32_t right, int32_t bottom, const uint8_t colors[4][3],
    uint32_t reciprocal_depth_bits) {
    float left_f = integer_float(left);
    float top_f = integer_float(top);
    float right_f = integer_add_one_float(right);
    float bottom_f = integer_add_one_float(bottom);
    unsigned index;
    memcpy(output, inherited, sizeof(GameRenderVertex) * 4u);
    output[0].x = output[3].x = left_f;
    output[0].y = output[1].y = top_f;
    output[1].x = output[2].x = right_f;
    output[2].y = output[3].y = bottom_f;
    for (index = 0u; index < 4u; ++index) {
        set_color(&output[index], colors[index][0], colors[index][1],
            colors[index][2]);
        memcpy(&output[index].oow, &reciprocal_depth_bits,
            sizeof(reciprocal_depth_bits));
    }
}

void game_screen_build_colored_polygon_vertex(GameRenderVertex *output,
    const GameRenderVertex *inherited, float x, float y, float epsilon,
    uint8_t red, uint8_t green, uint8_t blue, uint32_t depth_bits) {
    volatile float rounded;
    *output = *inherited;
    rounded = x + epsilon;
    output->x = rounded - epsilon;
    rounded = y + epsilon;
    output->y = rounded - epsilon;
    set_color(output, red, green, blue);
    memcpy(&output->oow, &depth_bits, sizeof(depth_bits));
}

void game_screen_build_textured_polygon_vertex(GameRenderVertex *output,
    const GameRenderVertex *inherited, float x, float y, float epsilon,
    float s, float t, float texture_scale) {
    long double adjusted;
    *output = *inherited;
    adjusted = (long double)x + (long double)epsilon;
    adjusted -= (long double)epsilon;
    output->x = (float)adjusted;
    adjusted = (long double)y + (long double)epsilon;
    adjusted -= (long double)epsilon;
    output->y = (float)adjusted;
    output->oow = 0.5f;
    output->sow0 = (float)((long double)s * (long double)texture_scale);
    output->tow0 = (float)((long double)t * (long double)texture_scale);
}

void game_screen_fullscreen_tile_fields(GameFullscreenTileFields *fields,
    unsigned x, unsigned y, unsigned tile_size) {
    fields->x[0] = fields->x[3] = (float)x;
    fields->x[1] = fields->x[2] = (float)(x + tile_size);
    fields->y[0] = fields->y[1] = (float)y;
    fields->y[2] = fields->y[3] = (float)(y + tile_size);
    fields->oow[0] = fields->oow[1] = 1.0f;
    fields->oow[2] = fields->oow[3] = 1.0f;
    fields->texture_extent = 256.0f;
}

void game_screen_fullscreen_tile_vertices(GameRenderVertex output[4],unsigned x,unsigned y,unsigned size) {
    GameFullscreenTileFields fields;unsigned i;GameRenderVertex zero={0};
    game_screen_fullscreen_tile_fields(&fields,x,y,size);
    for(i=0;i<4;i++){output[i]=zero;output[i].x=fields.x[i];output[i].y=fields.y[i];output[i].oow=fields.oow[i];output[i].oow0=1;}
    output[1].sow0=output[2].sow0=fields.texture_extent;
    output[2].tow0=output[3].tow0=fields.texture_extent;
}

int game_screen_build_world_polygon(GameRenderVertex *output,const GameWorldVertexInput *input,
    unsigned count,const GameWorldPolygonParams *params) {
    unsigned i;float s_scale,t_scale;
    if(!output || !input || !params || !count || count>32 || !params->width || !params->height)return 0;
    t_scale=params->fog_scale/(float)params->height;s_scale=params->fog_scale/(float)params->width;
    if(params->height<params->width)t_scale=(t_scale/(float)params->width)*(float)params->height;
    else if(params->width<params->height)s_scale=(s_scale/(float)params->height)*(float)params->width;
    memset(output,0,count*sizeof(*output));
    for(i=0;i<count;i++) {
        uint32_t color_bits,color=0;volatile float rounded;
        memcpy(&color_bits,&input[i].light,4);
        rounded=input[i].x+params->projection_magic;rounded=rounded-params->projection_magic;output[i].x=rounded;
        rounded=input[i].y+params->projection_magic;rounded=rounded-params->projection_magic;output[i].y=rounded;
        if(params->flags&0x40000000u){
            if(!(params->flags&4u))color=color_bits;
            else if(color_bits<=0x80000000u)color=(int32_t)color_bits<=(int32_t)0x437f0000u?color_bits:0x437f0000u;
        }
        memcpy(&output[i].r,&color,4);output[i].g=output[i].r;output[i].b=output[i].r;
        output[i].oow=output[i].oow0=input[i].depth;
        if(params->flags&4u){output[i].sow0=input[i].u*s_scale;output[i].tow0=input[i].v*t_scale;}
        else {output[i].sow0=input[i].u*s_scale*input[i].depth;output[i].tow0=input[i].v*t_scale*input[i].depth;}
        if(params->flags&0x400u){
            float alpha=params->fog_max-(output[i].oow-params->fog_start)*params->fog_factor;uint32_t bits;
            memcpy(&bits,&alpha,4);if(alpha>params->fog_max)alpha=255.0f;else if(bits>0x80000000u)alpha=0;
            output[i].a=alpha;
        }
    }
    return 1;
}

unsigned game_screen_polygon_triangles(const float *y,unsigned count,uint8_t output[30][3]) {
    uint32_t keys[32],smallest;unsigned center=0,left,right,i,remaining,n=0;
    if(!y || !output || !count || count>32)return 0;
    if(count==3){output[0][0]=0;output[0][1]=1;output[0][2]=2;return 1;}
    for(i=0;i<count;i++)memcpy(&keys[i],&y[i],4);
    smallest=keys[0]*2u;
    for(i=1;i<count;i++)if(keys[i]*2u<smallest){smallest=keys[i]*2u;center=i;}
    left=center?center-1:count-1;right=center+1==count?0:center+1;
    output[n][0]=(uint8_t)center;output[n][1]=(uint8_t)left;output[n++][2]=(uint8_t)right;
    /*481e54 tests the remainder as signed. Clipping can retain one/two
     *vertices; retail emits its two degenerate triangles without the middle
     *loop. A one-point ring repeats its valid vertex instead of retail's
     *extra vertex probe. Three vertices take the direct path above. */
    remaining=count>4?count-4:0;
    while(remaining--){unsigned next_left=left?left-1:count-1,next_right=right+1==count?0:right+1,pivot;
        if(keys[next_right]>keys[next_left]){pivot=left;left=next_left;}else{pivot=right;right=next_right;}
        output[n][0]=(uint8_t)pivot;output[n][1]=(uint8_t)left;output[n++][2]=(uint8_t)right;
    }
    i=left;left=left?left-1:count-1;
    output[n][0]=(uint8_t)i;output[n][1]=(uint8_t)left;output[n++][2]=(uint8_t)right;return n;
}
