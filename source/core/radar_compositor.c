#include "radar_compositor.h"

#include <math.h>
#include <string.h>
#include <stdlib.h>

static struct {uint8_t *pixels;size_t size;} map_buffers[2];
static int32_t radar_signed(uint32_t bits){int32_t value;memcpy(&value,&bits,4u);return value;}
static int32_t radar_integer(long double value){
    int64_t integer=!isfinite(value) || value< -0x1p63L || value>=0x1p63L?INT64_MIN:(int64_t)value;
    return radar_signed((uint32_t)(uint64_t)integer);
}
uint8_t *game_radar_map_borrow(GameAddress address,size_t bytes){
    uint32_t offset,slot;
    if(address<RADAR_MAP_BASE || address>=RADAR_MAP_END)return NULL;
    offset=address-RADAR_MAP_BASE;slot=offset/RADAR_MAP_SLOT_BYTES;offset%=RADAR_MAP_SLOT_BYTES;
    if(!map_buffers[slot].pixels || offset>map_buffers[slot].size || bytes>map_buffers[slot].size-offset)return NULL;
    return map_buffers[slot].pixels+offset;
}
int game_radar_map_read(GameAddress address,void *output,size_t bytes){
    uint8_t *pixels=game_radar_map_borrow(address,bytes);if(!pixels || !output)return 0;
    memcpy(output,pixels,bytes);return 1;
}
int game_radar_map_write(GameAddress address,const void *input,size_t bytes){
    uint8_t *pixels=game_radar_map_borrow(address,bytes);if(!pixels || !input)return 0;
    memcpy(pixels,input,bytes);return 1;
}
int game_radar_map_release(GameMemory *memory){
    unsigned slot;for(slot=0u;slot<2u;slot++){
        GameAddress dimensions=slot?0x596100u:0x5960f8u;
        free(map_buffers[slot].pixels);map_buffers[slot].pixels=NULL;map_buffers[slot].size=0u;
        if(game_memory_read_u32(memory,dimensions+4u)){
            game_memory_write_u16(memory,dimensions,0u);game_memory_write_u16(memory,dimensions+2u,0u);
            game_memory_write_u32(memory,dimensions+4u,0u);
        }
    }
    return !memory->fault;
}
int game_radar_map_initialize(GameMemory *memory,const uint8_t *source,size_t bytes,uint32_t source_width,uint32_t source_height){
    uint32_t width=game_memory_read_u32(memory,0x6027c0u)-game_memory_read_u32(memory,0x6027b8u);
    uint32_t height=game_memory_read_u32(memory,0x6027c4u)-game_memory_read_u32(memory,0x6027bcu);
    int fog=game_memory_read_u32(memory,0x58de70u)!=0u;uint32_t row,column;size_t size;
    float x_scale,y_scale;uint8_t *visible,*hidden=NULL;
    if(memory->fault || !source || !source_width || !source_height || source_height>bytes/source_width ||
        !width || !height || width>32767u || height>32767u || height>RADAR_MAP_SLOT_BYTES/width ||
        map_buffers[0].pixels || map_buffers[1].pixels)return 0;
    size=(size_t)width*height;visible=calloc(size,1u);if(!visible)return 0;
    if(fog){hidden=malloc(size);if(!hidden){free(visible);return 0;}}
    {volatile float x=(float)((long double)source_width/(long double)width);
     volatile float y=(float)((long double)source_height/(long double)height);x_scale=x;y_scale=y;}
    for(row=0u;row<height;row++){
        uint32_t source_row=(uint32_t)radar_integer((long double)row*y_scale);
        for(column=0u;column<width;column++){
            uint32_t source_column=(uint32_t)radar_integer((long double)column*x_scale);
            uint8_t pixel;
            if(source_row>=source_height || source_column>=source_width){free(visible);free(hidden);return 0;}
            pixel=source[(size_t)source_row*source_width+source_column];
            (fog?hidden:visible)[(size_t)(height-1u-row)*width+column]=pixel;
        }
    }
    map_buffers[0].pixels=hidden;map_buffers[0].size=hidden?size:0u;
    map_buffers[1].pixels=visible;map_buffers[1].size=size;
    game_memory_write_u32(memory,0x605440u,source_width);game_memory_write_u32(memory,0x605458u,source_height);
    game_memory_write_u16(memory,0x596100u,(uint16_t)width);game_memory_write_u16(memory,0x596102u,(uint16_t)height);
    game_memory_write_u32(memory,0x596104u,RADAR_MAP_BASE+RADAR_MAP_SLOT_BYTES);
    if(fog){game_memory_write_u16(memory,0x5960f8u,(uint16_t)width);game_memory_write_u16(memory,0x5960fau,(uint16_t)height);
        game_memory_write_u32(memory,0x5960fcu,RADAR_MAP_BASE);}
    if(memory->fault){free(hidden);free(visible);memset(map_buffers,0,sizeof(map_buffers));return 0;}
    return 1;
}

#define RADAR_SOURCE 0x006025c0u
#define RADAR_TARGET 0x00602680u
#define RADAR_RECIPROCAL_TABLE 0x00596268u
#define RADAR_TRIG_TABLE 0x006298a8u

static float bits_float(uint32_t bits) {
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}
static uint32_t float_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}
static float read_float(GameMemory *memory, GameAddress address) {
    return bits_float(game_memory_read_u32(memory, address));
}
static int32_t round_i32(long double value) {
    return (int32_t)nearbyintl(value);
}
static int32_t truncate_i32(long double value) {
    return (int32_t)(int64_t)truncl(value);
}
static float store_float(long double value) {
    volatile float stored = (float)value;
    return stored;
}

typedef struct RadarArchiveDimensions {
    uint32_t source_width,source_height,width,height;
    GameAddress visible,hidden;
    size_t source_bytes,map_bytes;
} RadarArchiveDimensions;
static int radar_archive_dimensions(GameMemory *m,RadarArchiveDimensions *d){
    int32_t width=(int16_t)game_memory_read_u16(m,0x596100u);
    int32_t height=(int16_t)game_memory_read_u16(m,0x596102u);
    d->source_width=game_memory_read_u32(m,0x605440u);d->source_height=game_memory_read_u32(m,0x605458u);
    d->visible=game_memory_read_u32(m,0x596104u);d->hidden=game_memory_read_u32(m,0x5960fcu);
    if(m->fault || width<=0 || height<=0 || !d->source_width || !d->source_height ||
        d->source_width>INT32_MAX || d->source_height>INT32_MAX ||
        d->source_height>0x04000000u/d->source_width || (uint32_t)height>RADAR_MAP_SLOT_BYTES/(uint32_t)width)return 0;
    d->width=(uint32_t)width;d->height=(uint32_t)height;
    d->source_bytes=(size_t)d->source_width*d->source_height;d->map_bytes=(size_t)d->width*d->height;
    return d->visible!=0u;
}
static uint8_t radar_archive_pixel(GameMemory *m,GameAddress map,size_t index){
    uint8_t *pixel=game_radar_map_borrow(map+(uint32_t)index,1u);
    return pixel?*pixel:game_memory_read_u8(m,map+(uint32_t)index);
}
static void radar_archive_store(GameMemory *m,GameAddress map,size_t index,uint8_t value){
    uint8_t *pixel=game_radar_map_borrow(map+(uint32_t)index,1u);
    if(pixel)*pixel=value;else game_memory_write_u8(m,map+(uint32_t)index,value);
}
static void radar_archive_encode(uint8_t bytes[4],uint32_t value){
    unsigned i;for(i=0u;i<4u;i++)bytes[i]=(uint8_t)(value>>(i*8u));
}
static uint32_t radar_archive_decode(const uint8_t bytes[4]){
    return (uint32_t)bytes[0]|(uint32_t)bytes[1]<<8u|(uint32_t)bytes[2]<<16u|(uint32_t)bytes[3]<<24u;
}
int game_radar_map_save(GameMemory *m,const GameRadarArchiveServices *s){
    RadarArchiveDimensions d;uint8_t *image;uint32_t row,column,index;float x_scale,y_scale;int ok=0;
    if(!game_memory_read_u32(m,0x58de70u))return !m->fault;
    if(!s || !s->write_record || !radar_archive_dimensions(m,&d))return 0;
    image=malloc(d.source_bytes);if(!image)return 0;
    x_scale=store_float((long double)d.width/d.source_width);y_scale=store_float((long double)d.height/d.source_height);
    for(row=0u;row<d.source_height;row++){
        uint32_t y=(uint32_t)radar_integer((long double)row*y_scale);
        if(y>=d.height)goto done;
        for(column=0u;column<d.source_width;column++){
            uint32_t x=(uint32_t)radar_integer((long double)column*x_scale);
            if(x>=d.width)goto done;
            image[(size_t)row*d.source_width+column]=radar_archive_pixel(m,d.visible,(size_t)y*d.width+x);
        }
    }
    if(m->fault || !s->write_record(s->context,0xfedcba99u,image,d.source_bytes))goto done;
    for(index=0u;(int32_t)index<(int16_t)game_memory_read_u16(m,0x639a74u);index++){
        unsigned field;
        for(field=0u;field<3u;field++){
            GameAddress records=game_memory_read_u32(m,0x602698u);uint8_t bytes[4];
            radar_archive_encode(bytes,game_memory_read_u32(m,records+index*12u+field*4u));
            if(m->fault || !s->write_record(s->context,field<2u?0xfedcba94u:0xfedcba92u,bytes,4u))goto done;
        }
    }
    ok=!m->fault;
done:free(image);return ok;
}
int game_radar_map_load(GameMemory *m,const GameRadarArchiveServices *s){
    RadarArchiveDimensions d;uint8_t *image;uint32_t row,column,index;float x_scale,y_scale;int ok=0;
    if(!game_memory_read_u32(m,0x58de70u))return !m->fault;
    if(!s || !s->read_record || !radar_archive_dimensions(m,&d) || !d.hidden)return 0;
    image=calloc(d.source_bytes,1u);if(!image)return 0;
    if(!s->read_record(s->context,image,d.source_bytes))goto done;
    x_scale=store_float((long double)d.source_width/d.width);y_scale=store_float((long double)d.source_height/d.height);
    for(row=0u;row<d.height;row++){
        uint32_t y=(uint32_t)radar_integer((long double)row*y_scale);
        if(y>=d.source_height)goto done;
        for(column=0u;column<d.width;column++){
            uint32_t x=(uint32_t)radar_integer((long double)column*x_scale);
            if(x>=d.source_width)goto done;
            radar_archive_store(m,d.visible,(size_t)row*d.width+column,image[(size_t)y*d.source_width+x]);
        }
    }
    /* Original reconstruction advances by HEIGHT here, even on rectangular
     * maps. Retain its overlap while bounding access to the actual owner. */
    for(row=0u;row<d.height;row++)for(column=0u;column<d.width;column++){
        size_t position=(size_t)row*d.height+column;
        if(position>=d.map_bytes)goto done;
        if(radar_archive_pixel(m,d.visible,position))
            radar_archive_store(m,d.visible,position,radar_archive_pixel(m,d.hidden,position));
    }
    for(index=0u;(int32_t)index<(int16_t)game_memory_read_u16(m,0x639a74u);index++){
        unsigned field;
        for(field=0u;field<3u;field++){
            GameAddress records=game_memory_read_u32(m,0x602698u),address=records+index*12u+field*4u;uint8_t bytes[4];
            radar_archive_encode(bytes,game_memory_read_u32(m,address));
            if(m->fault || !s->read_record(s->context,bytes,4u))goto done;
            game_memory_write_u32(m,address,radar_archive_decode(bytes));
        }
    }
    ok=!m->fault;
done:free(image);return ok;
}

/* 4a2ec0: reveal a circular span of the hidden map and mark reached records.
 * Keep the original conditional row-offset advance, including clipped rows. */
int game_radar_reveal(GameMemory *memory,GameAddress position,float radius){
    int32_t width,height,first_row,last_row,row,row_offset;
    GameAddress hidden,visible;long double scale,pixel_radius,center_y_extended;
    float scale_stored,radius_squared,center_y,center_x;uint32_t index;
    if(!game_memory_read_u32(memory,0x58de70u))return !memory->fault;
    width=(int16_t)game_memory_read_u16(memory,0x5960f8u);height=(int16_t)game_memory_read_u16(memory,0x5960fau);
    scale=(long double)width*read_float(memory,0x57434cu);scale_stored=store_float(scale);
    pixel_radius=scale*(long double)radius;radius_squared=store_float(pixel_radius*pixel_radius);
    center_y_extended=(long double)height-(long double)read_float(memory,position+8u)*scale_stored;
    center_y=store_float(center_y_extended);first_row=radar_integer(center_y_extended-pixel_radius);
    last_row=radar_signed((uint32_t)radar_integer((long double)center_y+pixel_radius)+1u);
    if(first_row<0)first_row=0;
    if(first_row>height-1 || last_row<0)return !memory->fault;
    if(last_row>height)last_row=height;
    center_x=store_float((long double)(float)width-(long double)read_float(memory,position)*scale_stored);
    hidden=game_memory_read_u32(memory,0x5960fcu);visible=game_memory_read_u32(memory,0x596104u);
    row_offset=first_row*width;
    for(row=first_row;row<last_row && !memory->fault;row++){
        long double delta=fabsl((long double)center_y-row),remaining=(long double)radius_squared-delta*delta;
        float stored=store_float(remaining);
        if(remaining>=0.0L){
            long double span=sqrtl((long double)stored);
            int32_t first_column=radar_integer((long double)center_x-span),last_column=radar_integer(span+(long double)center_x);
            if(first_column<0)first_column=0;
            if(first_column<=width-1 && last_column>=0){
                if(last_column>width)last_column=width;
                if(last_column>first_column){
                    uint32_t offset=(uint32_t)(row_offset+first_column),count=(uint32_t)(last_column-first_column);
                    uint8_t *source=game_radar_map_borrow(hidden+offset,count),*destination=game_radar_map_borrow(visible+offset,count);
                    if(source && destination){
                        if((uint64_t)hidden+offset<(uint64_t)visible+offset+count &&
                            (uint64_t)visible+offset<(uint64_t)hidden+offset+count){
                            uint32_t pixel;for(pixel=0u;pixel<count;pixel++)destination[pixel]=source[pixel];
                        }else memcpy(destination,source,count);
                    }
                    else game_memory_copy_elements(memory,visible+offset,hidden+offset,count,1u,0);
                }
                row_offset+=width;
            }
        }
    }
    {
        int32_t left=radar_signed(game_memory_read_u32(memory,0x6027b8u)),top=radar_signed(game_memory_read_u32(memory,0x6027bcu));
        int32_t world_width=radar_signed(game_memory_read_u32(memory,0x6027c0u)-(uint32_t)left);
        int32_t world_height=radar_signed(game_memory_read_u32(memory,0x6027c4u)-(uint32_t)top);
        float x_scale=store_float((long double)(float)width/world_width),y_scale=store_float((long double)(float)width/world_height);
        float map_left=store_float(left),map_top=store_float(top);
        for(index=0u;!memory->fault && (int32_t)index<(int16_t)game_memory_read_u16(memory,0x639a74u);index++){
            GameAddress record=game_memory_read_u32(memory,0x602698u)+index*12u;
            if(!game_memory_read_u32(memory,record+8u)){
                long double x=((long double)radar_signed(game_memory_read_u32(memory,record))-map_left)*x_scale-center_x;
                long double y=((long double)radar_signed(game_memory_read_u32(memory,record+4u))-map_top)*y_scale-center_y;
                if(!(x*x+y*y>=(long double)radius_squared))game_memory_write_u32(memory,record+8u,1u);
            }
        }
    }
    return !memory->fault;
}

static GameRadarResult compose_radar(GameMemory *memory,
    const GameRadarInput *input, GameAddress scratch_vertices,
    const GameRadarOps *ops,uint8_t *pixels,size_t pixel_bytes,GameRadarGeometry *geometry) {
    static const uint8_t outside_mask[8] = {0,0,0,0,0x6e,0,0,0};
    static const uint8_t center_glyph[42] = {
        1,1,0,0,0,1,1, 1,1,0,0,1,1,1, 1,1,0,1,1,1,1,
        1,1,1,1,0,1,1, 1,1,1,0,0,1,1, 1,1,0,0,0,1,1
    };
    GameAddress scratch_texcoords = scratch_vertices + 0x200u;
    GameAddress scratch_vertex_pointers = scratch_vertices + 0x300u;
    GameAddress scratch_descriptor = scratch_vertices + 0x380u;
    int32_t source_width = (int16_t)game_memory_read_u16(memory, RADAR_SOURCE);
    int32_t source_height = (int16_t)game_memory_read_u16(memory, RADAR_SOURCE + 2u);
    GameAddress source_pixels = game_memory_read_u32(memory, RADAR_SOURCE + 4u);
    const uint8_t *source_view=source_width>0 && source_height>0?
        game_radar_map_borrow(source_pixels,(size_t)source_width*(size_t)source_height):NULL;
    int32_t target_width = (int16_t)game_memory_read_u16(memory, RADAR_TARGET);
    int32_t target_height = (int16_t)game_memory_read_u16(memory, RADAR_TARGET + 2u);
    if(pixels && (target_width<=0 || target_height<=0 ||
        (size_t)target_width*(size_t)target_height>pixel_bytes))return GAME_RADAR_FAULT;
    GameAddress target_pixels = game_memory_read_u32(memory, RADAR_TARGET + 4u);
    GameAddress reciprocal_table = game_memory_read_u32(memory, RADAR_RECIPROCAL_TABLE);
    int32_t center_source_x, center_source_y, half_source_x, half_source_y;
    int32_t source_left, source_right, source_top, source_bottom;
    int32_t x_step_fixed, y_step_fixed, x_step_whole;
    int32_t y_step_rows, y_step_rows_with_carry;
    uint32_t x_step_fraction, y_step_fraction, y_fraction = 0u;
    int32_t source_row, source_area, target_row, remaining_y;
    unsigned glyph_y, vertex;
    float radius, center_x, center_y;
    if (memory->fault || !input || (!geometry && (!ops || !ops->draw)))
        return GAME_RADAR_FAULT;
    if (source_width <= 0 || source_width > 2048 ||
        source_height <= 0 || source_height > 2048 ||
        target_width <= 0 || target_width > 2048 ||
        target_height <= 0 || target_height > 2048 ||
        source_pixels < 0x10000u || target_pixels < 0x10000u ||
        reciprocal_table < 0x10000u)
        return GAME_RADAR_SKIPPED;

    center_source_x = source_width -
        (truncate_i32((long double)source_width * input->source_x) >> 17);
    center_source_y = source_height -
        (truncate_i32((long double)source_height * input->source_y) >> 17);
    half_source_x = truncate_i32((long double)(float)source_width *
        input->source_scale) >> 18;
    half_source_y = truncate_i32((long double)(float)source_height *
        input->source_scale) >> 18;
    source_left = center_source_x - half_source_x;
    source_right = center_source_x + half_source_x;
    source_top = center_source_y - half_source_y;
    source_bottom = center_source_y + half_source_y;
    x_step_fixed = round_i32((long double)(source_right - source_left) *
        (int32_t)game_memory_read_u32(memory,
            reciprocal_table + (uint32_t)target_width * 4u));
    y_step_fixed = round_i32((long double)(source_top - source_bottom) *
        (int32_t)game_memory_read_u32(memory,
            reciprocal_table + (uint32_t)target_height * 4u));
    if (memory->fault) return GAME_RADAR_FAULT;
    x_step_fraction = (uint32_t)x_step_fixed << 16;
    y_step_fraction = (uint32_t)y_step_fixed << 16;
    x_step_whole = x_step_fixed >> 16;
    y_step_rows = round_i32((long double)(y_step_fixed >> 16) * source_width);
    y_step_rows_with_carry = y_step_rows + source_width;
    source_row = round_i32((long double)source_bottom * source_width);
    source_area = round_i32((long double)source_height * source_width);
    target_row = 0;
    remaining_y = target_height;
    while (remaining_y != 0) {
        uint32_t x_fraction = 0u;
        int32_t source_column = source_left;
        int32_t remaining_x = target_width;
        while (remaining_x != 0) {
            uint8_t pixel;
            uint32_t previous_fraction;
            if (source_column >= 0 && source_column < source_width &&
                source_row >= 0 && source_row < source_area)
                pixel = source_view?source_view[(uint32_t)(source_row+source_column)]:
                    game_memory_read_u8(memory, source_pixels +(uint32_t)(source_row+source_column));
            else
                pixel = outside_mask[(uint32_t)remaining_y & 7u] |
                    outside_mask[(uint32_t)remaining_x & 7u];
            if(pixels)pixels[(uint32_t)target_row]=pixel;
            else game_memory_write_u8(memory, target_pixels + (uint32_t)target_row, pixel);
            if (memory->fault) return GAME_RADAR_FAULT;
            ++target_row;
            previous_fraction = x_fraction;
            x_fraction += x_step_fraction;
            source_column += x_step_whole + (x_fraction < previous_fraction ? 1 : 0);
            --remaining_x;
        }
        {
            uint32_t previous_fraction = y_fraction;
            y_fraction += y_step_fraction;
            source_row += y_fraction < previous_fraction ? y_step_rows_with_carry : y_step_rows;
        }
        --remaining_y;
    }

    target_row = (target_height - 18) * target_width + (target_width >> 1) - 4;
    for (glyph_y = 0; glyph_y < 6u; ++glyph_y) {
        unsigned glyph_x;
        for (glyph_x = 0; glyph_x < 7u; ++glyph_x)
            if (center_glyph[glyph_y * 7u + glyph_x]){
                uint32_t index=(uint32_t)(target_row+(int32_t)glyph_x);
                if(pixels){if(index>=pixel_bytes)return GAME_RADAR_FAULT;pixels[index]=0xf6u;}
                else game_memory_write_u8(memory,target_pixels+index,0xf6u);
            }
        target_row += target_width;
    }
    if (memory->fault) return GAME_RADAR_FAULT;

    radius = store_float((long double)target_height * 0.5L);
    center_x = (float)input->screen_x;
    center_y = (float)input->screen_y;
    for (vertex = 0; vertex < 32u; ++vertex) {
        uint16_t angle = (uint16_t)(vertex * 0x40u);
        float direction_x = read_float(memory, RADAR_TRIG_TABLE +
            ((uint32_t)(input->heading + angle) & 0x7ffu) * 4u);
        float direction_y = read_float(memory, RADAR_TRIG_TABLE +
            ((uint32_t)(uint16_t)(input->heading + angle + 0x200u) & 0x7ffu) * 4u);
        float texture_x = read_float(memory, RADAR_TRIG_TABLE +
            ((uint32_t)angle & 0x7ffu) * 4u);
        float texture_y = read_float(memory, RADAR_TRIG_TABLE +
            ((uint32_t)(uint16_t)(angle + 0x200u) & 0x7ffu) * 4u);
        float vertex_x = store_float((long double)direction_x * radius + center_x);
        float vertex_y = store_float((long double)center_y - (long double)direction_y * radius);
        float texture_u = store_float(((long double)texture_x + 1.0L) * 128.0L);
        float texture_v = store_float(((long double)texture_y + 1.0L) * 128.0L);
        GameAddress vertex_address = scratch_vertices + vertex * 16u;
        if(geometry){
            geometry->positions[vertex][0]=vertex_x;geometry->positions[vertex][1]=vertex_y;
            geometry->uv[vertex][0]=texture_u;geometry->uv[vertex][1]=texture_v;
            continue;
        }
        game_memory_write_u32(memory, vertex_address, float_bits(vertex_x));
        game_memory_write_u32(memory, vertex_address + 4u, float_bits(vertex_y));
        game_memory_write_u32(memory, scratch_vertex_pointers + vertex * 4u, vertex_address);
        game_memory_write_u32(memory, scratch_texcoords + vertex * 8u, float_bits(texture_u));
        game_memory_write_u32(memory, scratch_texcoords + vertex * 8u + 4u, float_bits(texture_v));
    }
    if(geometry){
        geometry->source_x=center_source_x;geometry->source_y=center_source_y;
        return memory->fault?GAME_RADAR_FAULT:GAME_RADAR_OK;
    }
    game_memory_write_u32(memory, scratch_descriptor, 32u);
    game_memory_write_u32(memory, scratch_descriptor + 4u, scratch_vertex_pointers);
    game_memory_write_u32(memory, scratch_descriptor + 8u, 1u);
    game_memory_write_u32(memory, scratch_descriptor + 12u, 0u);
    game_memory_write_u32(memory, scratch_descriptor + 16u, scratch_texcoords);
    game_memory_write_u32(memory, scratch_descriptor + 20u, RADAR_TARGET);
    if (memory->fault) return GAME_RADAR_FAULT;
    return ops->draw(ops->context, scratch_descriptor, center_source_x,
        center_source_y) ? GAME_RADAR_OK : GAME_RADAR_FAULT;
}
GameRadarResult game_ui_compose_radar_pixels(GameMemory *memory,const GameRadarInput *input,
    GameAddress scratch,const GameRadarOps *ops,uint8_t *pixels,size_t bytes){
    return compose_radar(memory,input,scratch,ops,pixels,bytes,NULL);
}
GameRadarResult game_ui_compose_radar_geometry(GameMemory *memory,const GameRadarInput *input,
    uint8_t *pixels,size_t bytes,GameRadarGeometry *geometry){
    if(!geometry)return GAME_RADAR_FAULT;
    return compose_radar(memory,input,0,NULL,pixels,bytes,geometry);
}
GameRadarResult game_ui_compose_radar(GameMemory *memory,const GameRadarInput *input,
    GameAddress scratch_vertices,const GameRadarOps *ops){
    return game_ui_compose_radar_pixels(memory,input,scratch_vertices,ops,NULL,0u);
}
