#include "game_text_rasterizer.h"

#define TEXT_FOREGROUND 0x0059d448u
#define TEXT_BACKGROUND 0x0059d44cu
#define TEXT_CLIP_LEFT 0x0059d450u
#define TEXT_CLIP_RIGHT 0x0059d454u
#define TEXT_CLIP_TOP 0x0059d458u
#define TEXT_CLIP_BOTTOM 0x0059d45cu
#define TEXT_ANIMATION 0x0059d460u
#define TEXT_TRANSPARENT 0x0059d468u
#define TEXT_CLIP_ENABLED 0x0059d46cu
#define TEXT_FONT 0x00575c40u
#define TEXT_GLYPH_MAP 0x006290f8u
#define TEXT_CYCLE_16 0x006291f8u
#define TEXT_CYCLE_256 0x00629208u
#define TEXT_PALETTE 0x005ad3a8u
#define TEXT_CURSOR_Y 0x00629708u
#define TEXT_CURSOR_X 0x0062970cu

/*005328c0: measure the original five-column font, including its inline control
 * bytes. Signed character indexing and unsigned width arithmetic are deliberate.
 * The ordinary byte span replaces the original unbounded variadic scratch text. */
uint32_t game_text_width_bytes(GameMemory *memory,const uint8_t *bytes,size_t length) {
    size_t i=0;uint32_t width=0;
    if(!length)return 0;
    while(i<length && bytes[i] && !memory->fault) {
        uint8_t value=bytes[i++];unsigned column;uint32_t glyph;
        if(value>=7 && value<=13) {
            if(value==11){if(length-i<3){memory->fault=1;return 0;}i+=3;}
            continue;
        }
        glyph=game_memory_read_u8(memory,TEXT_GLYPH_MAP+(uint32_t)(int32_t)(int8_t)value);
        for(column=0;column<5;++column)
            if(game_memory_read_u8(memory,TEXT_FONT+glyph*25+column)!=2)++width;
        ++width;
    }
    return width-1;
}
static uint8_t text_byte(GameMemory *memory, uint32_t address) {
    return game_memory_read_u8(memory, address);
}

static int32_t text_i32(GameMemory *memory, uint32_t address) {
    return (int32_t)game_memory_read_u32(memory, address);
}

static uint8_t text_palette_index(GameMemory *memory, unsigned mode,
    uint8_t normal, uint8_t row_color, uint32_t animation, int row_offset) {
    switch (mode) {
        case 0: return normal;
        case 1: return row_color;
        case 2: return text_byte(memory, TEXT_CYCLE_16 + (animation & 15u));
        case 3: return text_byte(memory, TEXT_CYCLE_256 +
            (uint32_t)row_offset * 256u + (animation & 255u));
        default: return normal;
    }
}

static void text_write_pixel(GameMemory *memory, const GameTextTarget *target,
    uint32_t pixel, uint8_t color) {
    uint32_t address;
    size_t offset;
    if(target->pixels){
        uint16_t packed=color;size_t bytes=target->rgb565?2u:1u;
        if((uint64_t)pixel*bytes>SIZE_MAX){memory->fault=1;return;}
        offset=(size_t)pixel*bytes;
        if(offset>target->pixel_bytes || bytes>target->pixel_bytes-offset){memory->fault=1;return;}
        if(target->rgb565){uint32_t entry=TEXT_PALETTE+(uint32_t)color*4;
            uint8_t red=text_byte(memory,entry),green=text_byte(memory,entry+1),blue=text_byte(memory,entry+2);
            packed=(uint16_t)(((uint16_t)(green&0xfc)<<3)|((uint16_t)(blue&0xf8)<<8)|(red>>3));}
        if(memory->fault)return;
        target->pixels[offset]=(uint8_t)packed;
        if(bytes==2)target->pixels[offset+1]=(uint8_t)(packed>>8);
        if(target->written_range){
            if(pixel<target->written_range[0])target->written_range[0]=pixel;
            if(pixel>=target->written_range[1])target->written_range[1]=pixel+1;
        }
        return;
    }
    if (!target->rgb565) {
        address = target->base + pixel;
        game_memory_write_u8(memory, address, color);
    } else {
        uint32_t entry = TEXT_PALETTE + (uint32_t)color * 4u;
        uint8_t red = text_byte(memory, entry);
        uint8_t green = text_byte(memory, entry + 1u);
        uint8_t blue = text_byte(memory, entry + 2u);
        uint16_t packed = (uint16_t)(((uint16_t)(green & 0xfcu) << 3) |
            ((uint16_t)(blue & 0xf8u) << 8) | (red >> 3));
        address = target->base + pixel * 2u;
        game_memory_write_u16(memory, address, packed);
    }
}

typedef struct TextSource {GameAddress address;const uint8_t *bytes;size_t length;} TextSource;
static uint8_t source_byte(GameMemory *memory,const TextSource *source,uint32_t offset){
    if(!source->bytes)return text_byte(memory,source->address+offset);
    if(offset>=source->length){memory->fault=1;return 0;}
    return source->bytes[offset];
}
static unsigned text_count_printable(GameMemory *memory, const TextSource *text) {
    unsigned count = 0;
    uint32_t offset=0;
    uint8_t value;
    while ((value = source_byte(memory, text,offset++)) != 0u && !memory->fault) {
        /* The retail fallback jump table oddly counts control 11 itself even
           though the rendered path consumes it as a four-byte color code. */
        if (value < 7u || value > 13u || value == 11u) count++;
    }
    return count;
}

static uint32_t rasterize_source(GameMemory *memory, const TextSource *text,
    const GameTextTarget *target, int32_t fallback_x) {
    int clip_enabled = text_i32(memory, TEXT_CLIP_ENABLED) != 0;
    int32_t clip_left = text_i32(memory, TEXT_CLIP_LEFT);
    int32_t clip_right = text_i32(memory, TEXT_CLIP_RIGHT);
    int32_t clip_top = text_i32(memory, TEXT_CLIP_TOP);
    int32_t clip_bottom = text_i32(memory, TEXT_CLIP_BOTTOM);
    int32_t start_x = text_i32(memory, TEXT_CURSOR_X);
    int32_t start_y = text_i32(memory, TEXT_CURSOR_Y);
    uint8_t normal = text_byte(memory, TEXT_FOREGROUND);
    uint8_t background = text_byte(memory, TEXT_BACKGROUND);
    uint32_t initial_animation = game_memory_read_u32(memory, TEXT_ANIMATION);
    uint32_t default_transparency = game_memory_read_u32(memory, TEXT_TRANSPARENT);
    uint32_t animation = initial_animation;
    unsigned count = 0;
    int counted = 0;
    int32_t final_x = fallback_x;
    int row;

    for (row = 0; row < 5 && !memory->fault; row++) {
        int32_t y = start_y + row;
        uint8_t row_color = (uint8_t)(initial_animation + 0xa0u + row);
        int32_t x = start_x;
        uint32_t cursor = 0;
        unsigned mode = 0;
        uint8_t foreground = normal;
        uint32_t transparent = default_transparency;
        uint32_t pixel = (uint32_t)(y * target->stride + start_x);
        if (clip_enabled && (y < clip_top || y >= clip_bottom)) continue;

        while (!memory->fault) {
            uint8_t character = source_byte(memory, text,cursor);
            uint32_t next;
            int column;
            if (!character) break;
            next = cursor + 1u;
            switch (character) {
                case 7: mode = 1; cursor = next; continue;
                case 8:
                    mode = 0; foreground = normal;
                    transparent = default_transparency;
                    cursor = next; continue;
                case 9:
                    transparent = transparent == 0u;
                    cursor = next; continue;
                case 10: cursor = next; continue;
                case 11: {
                    int a = (int)(int8_t)source_byte(memory, text,cursor + 1u);
                    int b = (int)(int8_t)source_byte(memory, text,cursor + 2u);
                    int c = (int)(int8_t)source_byte(memory, text,cursor + 3u);
                    foreground = (uint8_t)(c + 48 + (a * 10 + b) * 10);
                    cursor += 4u;
                    continue;
                }
                case 12: mode = 3; cursor = next; continue;
                case 13: mode = 2; cursor = next; continue;
                default: break;
            }
            if (!counted) count++;
            {
                int glyph_index = (int)(int8_t)character;
                uint8_t glyph = text_byte(memory,
                    TEXT_GLYPH_MAP + (uint32_t)glyph_index);
                for (column = 0; column < 5; column++) {
                    uint8_t cell;
                    animation++;
                    cell = text_byte(memory, TEXT_FONT + (uint32_t)row * 5u +
                        (uint32_t)glyph * 25u + (uint32_t)column);
                    if (cell == 2u) continue;
                    if (!clip_enabled || (x >= clip_left && x < clip_right)) {
                        if (cell == 1u) {
                            uint8_t color = text_palette_index(memory, mode,
                                foreground, row_color, animation, row);
                            text_write_pixel(memory, target, pixel, color);
                        } else if (transparent == 0u) {
                            text_write_pixel(memory, target, pixel, background);
                        }
                        pixel++;
                        x++;
                        if (transparent == 0u &&
                            source_byte(memory, text,next) != 0u &&
                            (!clip_enabled || (x >= clip_left && x < clip_right)))
                            text_write_pixel(memory, target, pixel, background);
                    } else {
                        pixel++;
                        x++;
                    }
                }
            }
            pixel++;
            x++;
            cursor = next;
        }
        final_x = x;
        counted = 1;
    }
    if (!count) count = text_count_printable(memory, text);
    return ((uint32_t)(final_x - start_x) << 16) | count;
}
uint32_t game_text_rasterize(GameMemory *memory,GameAddress text,const GameTextTarget *target,int32_t fallback_x){
    TextSource source={text,NULL,0};return rasterize_source(memory,&source,target,fallback_x);
}
uint32_t game_text_rasterize_bytes(GameMemory *memory,const uint8_t *bytes,size_t length,const GameTextTarget *target,int32_t fallback_x){
    TextSource source={0,bytes,length};
    if(!bytes || !length || length>UINT32_MAX){memory->fault=1;return 0;}
    return rasterize_source(memory,&source,target,fallback_x);
}
