#include "gameplay_leaves.h"
#include "terrain_asset.h"

#include <string.h>
#include <math.h>

/* The original FTOL returns the low word of a truncated signed 64-bit value.
 * Invalid/out-of-range conversions produce the x87 indefinite integer. */
static uint32_t terrain_coordinate(long double value) {
    long double wide = value;
    if (!(wide >= -9223372036854775808.0L &&
          wide < 9223372036854775808.0L)) return 0u;
    return (uint32_t)(uint64_t)(int64_t)wide;
}
static uint32_t terrain_shift(uint32_t value, unsigned shift) {
    return (value >> shift) |
        ((value & 0x80000000u) ? (~0u << (32u - shift)) : 0u);
}
static uint32_t terrain_point_presence(GameMemory *memory, GameAddress object) {
    float x, z;
    uint32_t column, row;
    if (!game_memory_read(memory, object + 0x18u, &x, sizeof(x)) ||
        !game_memory_read(memory, object + 0x20u, &z, sizeof(z))) return 0u;
    column = terrain_shift(terrain_coordinate(x), 12u);
    row = terrain_shift(terrain_coordinate(z) & 0xfffff07fu, 7u);
    return game_memory_read_u8(memory, 0x006020f0u + row + column);
}
uint32_t game_terrain_cell_presence(GameMemory *memory, GameAddress first,
    GameAddress second) {
    uint32_t result = terrain_point_presence(memory, first);
    if (!second || memory->fault) return result;
    if (result) return 1u;
    return terrain_point_presence(memory, second) != 0u;
}

static float terrain_real(GameMemory *m,GameAddress address) {
    uint32_t bits=game_memory_read_u32(m,address);float value;
    memcpy(&value,&bits,4u);return value;
}
static int32_t terrain_signed(uint32_t value) {
    return value<=INT32_MAX?(int32_t)value:(int32_t)((int64_t)value-4294967296LL);
}
static uint8_t terrain_cell_flags(GameMemory *m,uint32_t index) {
    if(m->terrain && m->terrain->cells && index<65536u)
        return m->terrain->cells[index].flags;
    return game_memory_read_u8(m,game_memory_read_u32(m,0x597d90u)+index*8u+5u);
}
static void terrain_cell_mark(GameMemory *m,uint32_t index) {
    if(m->terrain && m->terrain->cells && index<65536u)
        m->terrain->cells[index].light=0xdfu;
    else game_memory_write_u8(m,game_memory_read_u32(m,0x597d90u)+index*8u+4u,0xdfu);
}
/*406930: two original crater profiles consume the authoritative terrain cell
 *height and the low byte of its reserved deformation budget. Geometry and
 *rendering already read this owner;42baa0 is a verified empty refresh method. */
int game_terrain_deform(GameMemory *m,const float position[3],float strength,int32_t radius){
    int32_t cx=terrain_signed(terrain_shift(terrain_coordinate(position[0]),9));
    int32_t cz=terrain_signed(terrain_shift(terrain_coordinate(position[2]),9));
    int32_t left=terrain_signed((uint32_t)cx-(uint32_t)radius),right=terrain_signed((uint32_t)cx+(uint32_t)radius+1u);
    int32_t top=terrain_signed((uint32_t)cz-(uint32_t)radius),bottom=terrain_signed((uint32_t)cz+(uint32_t)radius+1u);
    uint32_t mode=game_memory_read_u32(m,0x58332c);int32_t x,z;float scale=terrain_real(m,0x571078),p[3];double half;
    if(left<0)left=0;
    if(top<0)top=0;
    if(right>255)right=255;
    if(bottom>255)bottom=255;
    if(mode>1 || left>right || top>bottom)return !m->fault;
    if(left>255 || top>255 || right<0 || bottom<0 || (m->terrain && !m->terrain->cells)){m->fault=1;return 0;}
    if(!game_memory_read(m,0x571080,&half,8))return 0;
    p[0]=(float)((long double)position[0]*scale);p[1]=(float)((long double)position[1]*scale);p[2]=(float)((long double)position[2]*scale);
    for(z=top;z<=bottom && !m->fault;z++)for(x=left;x<=right && !m->fault;x++){
        uint32_t index=(uint32_t)z*256u+(uint32_t)x;TerrainCell *cell=m->terrain?m->terrain->cells+index:NULL;
        GameAddress address=cell?0:game_memory_read_u32(m,0x597d90)+index*8u;
        uint16_t height_bits=cell?(uint16_t)cell->height:game_memory_read_u16(m,address);int16_t height;uint8_t flags,budget;
        long double dx,dz,dy,distance_squared,ratio;int32_t drop;uint8_t remaining;
        memcpy(&height,&height_bits,2);flags=cell?cell->flags:game_memory_read_u8(m,address+5);
        if(flags&0x20){if(!mode)break;continue;}
        budget=cell?(uint8_t)cell->reserved:game_memory_read_u8(m,address+6);if(!budget)continue;
        dx=(long double)p[0]-x;dz=(long double)p[2]-z;dy=(long double)p[1]-(long double)height*scale;
        distance_squared=(dz*dz+dx*dx)+dy*dy;
        ratio=mode?distance_squared/terrain_signed((uint32_t)radius*(uint32_t)radius+2u):sqrtl(distance_squared)/((long double)radius+half);
        drop=terrain_signed(terrain_coordinate((long double)strength-ratio*strength));
        if(drop>(int32_t)budget*2){drop=(int32_t)budget*2;remaining=0;}
        else remaining=(uint8_t)((uint32_t)budget-terrain_shift((uint32_t)drop,1)-((uint32_t)drop&1u));
        if(cell)cell->reserved=(uint16_t)((cell->reserved&0xff00u)|remaining);
        else game_memory_write_u8(m,address+6,remaining);
        if(drop>0){height_bits=(uint16_t)(height_bits-(uint32_t)drop);
            if(cell)memcpy(&cell->height,&height_bits,2);else game_memory_write_u16(m,address,height_bits);}
    }
    return !m->fault;
}
void game_terrain_mark_area(GameMemory *m,float x,float z,float width,float height) {
    float factor=terrain_real(m,0x572bccu),limit_factor=terrain_real(m,0x572bd0u);
    float step=terrain_real(m,0x572bd4u),row_delta;
    uint32_t first_column,last_column,row,last_row,column;
    long double h=(long double)height*factor;
    float w=(float)((long double)width*factor);
    float limit=(float)(((long double)w*w+h*h)*limit_factor);
    first_column=(terrain_coordinate((long double)x-w)&0x1fe00u)>>9u;
    last_column=((terrain_coordinate((long double)x+w)>>9u)+1u)&255u;
    row=(terrain_coordinate((long double)z-h)&0x1fe00u)>>9u;
    last_row=((terrain_coordinate((long double)z+h)>>9u)+1u)&255u;
    row_delta=(float)-h;
    for(;row!=last_row && !m->fault;row=(row+1u)&255u) {
        float row_squared=(float)((long double)row_delta*row_delta);
        long double column_delta=-w;
        for(column=first_column;column!=last_column && !m->fault;column=(column+1u)&255u) {
            if(column_delta*column_delta+row_squared<limit)
                terrain_cell_mark(m,row*256u+column);
            column_delta+=step;
        }
        row_delta=(float)((long double)row_delta+step);
    }
}
void game_terrain_impact(GameMemory *m,float x,float z,int32_t extent) {
    int32_t half,span,column,row;int64_t end_column,end_row,r,c;
    if(!extent)return;
    half=terrain_signed(terrain_shift((uint32_t)extent,1u));
    span=terrain_signed(terrain_shift((uint32_t)extent,9u));
    column=terrain_signed(terrain_shift(terrain_coordinate((long double)x-half),9u));
    row=terrain_signed(terrain_shift(terrain_coordinate((long double)z-half),9u));
    end_column=(int64_t)column+span;end_row=(int64_t)row+span;
    for(r=row;r<=end_row && !m->fault;r++)for(c=column;c<=end_column && !m->fault;c++) {
        uint32_t index=(uint32_t)r*256u+(uint32_t)c;
        uint8_t flags=terrain_cell_flags(m,index);
        if((flags&0x20u) && !(flags&1u))return;
    }
    if(extent>=512 && !m->fault)game_terrain_mark_area(m,x,z,(float)extent,(float)extent);
}


void game_leaf_0044ed50(GameMemory *memory) {
    game_memory_write_u32(memory, 0x00585334u, 1u);
}

void game_leaf_004d94d0(GameMemory *memory) {
    game_memory_write_u32(memory, 0x00604f08u, 0u);
}

void game_leaf_0042f110(GameMemory *memory, GameAddress self) {
    game_memory_write_u32(memory, self, 0x00572230u);
}

uint32_t game_leaf_004cc1c0(void) { return 0x00008040u; }

uint32_t game_leaf_004f9b90(GameMemory *memory, GameAddress self) {
    return game_memory_read_u32(memory, self - 0x128u);
}

GameAddress game_leaf_identity(GameAddress self) { return self; }

void game_leaf_004765f0(GameMemory *memory, GameAddress self) {
    game_memory_write_u32(memory, self + 0x14u, 1u);
}

void game_leaf_0043b4c0(GameMemory *memory, GameAddress self) {
    game_memory_write_u32(memory, self, 0x00572dc0u);
}

GameAddress game_leaf_0048ef80(GameMemory *memory, GameAddress self) {
    game_memory_write_u32(memory, self, 0u);
    if (!memory->fault)
        game_memory_write_u16(memory, self + 8u, 0xffffu);
    return self;
}

uint32_t game_leaf_00416c10(GameMemory *memory, GameAddress self,
    uint32_t value) {
    game_memory_write_u32(memory, self + 4u, value);
    return value;
}

uint32_t game_leaf_00416c20(GameMemory *memory, GameAddress self) {
    return game_memory_read_u32(memory, self + 4u);
}

uint32_t game_leaf_00479024(GameMemory *memory, GameAddress self) {
    return game_memory_read_u32(memory, self + 0x220u);
}

void game_leaf_00502e00(GameMemory *memory) {
    game_memory_write_u32(memory, 0x00624ee4u, 0u);
}

uint32_t game_leaf_0046ff30(void) { return 0x54u; }

void game_leaf_00441420(GameMemory *memory) {
    game_memory_write_u32(memory, 0x00584668u, 0u);
    if (!memory->fault)
        game_memory_write_u32(memory, 0x005acbe4u, 0u);
}

uint32_t game_leaf_004073d0(void) { return 100u; }
uint32_t game_leaf_004073e0(void) { return 10u; }

uint32_t game_leaf_00454e90(GameMemory *memory, GameAddress self) {
    return game_memory_read_u32(memory, self + 0x1804u);
}

void game_leaf_0045f570(GameMemory *memory) {
    game_memory_write_u32(memory, 0x005e147cu, 0u);
    if (!memory->fault)
        game_memory_write_u32(memory, 0x005e1480u, 0u);
}

uint32_t game_leaf_clear_s16x3(GameMemory *memory, GameAddress object) {
    game_memory_write_u16(memory, object + 0x0cu, 0u);
    if (!memory->fault) game_memory_write_u16(memory, object + 0x0eu, 0u);
    if (!memory->fault) game_memory_write_u16(memory, object + 0x10u, 0u);
    return 0u;
}

void game_leaf_copy_u32x3(GameMemory *memory, GameAddress destination,
    GameAddress source) {
    unsigned index;
    for (index = 0u; index < 3u && !memory->fault; ++index) {
        uint32_t value = game_memory_read_u32(memory, source + index * 4u);
        if (!memory->fault)
            game_memory_write_u32(memory, destination + index * 4u, value);
    }
}

GameAddress game_leaf_find_nonzero_link(GameMemory *memory,
    GameAddress object) {
    GameAddress node = game_memory_read_u32(memory, object + 0x48u);
    while (!memory->fault && node != 0u &&
        game_memory_read_u32(memory, node + 0x58u) == 0u)
        node = game_memory_read_u32(memory, node + 0x48u);
    return node;
}

GameLeafDivisionResult game_leaf_attenuate_s16x3(GameMemory *memory,
    GameAddress object, int32_t divisor) {
    GameLeafDivisionResult result = {0, 0};
    unsigned index;
    if (divisor <= 1) return result;
    for (index = 0u; index < 3u && !memory->fault; ++index) {
        int32_t value = (int16_t)game_memory_read_u16(memory,
            object + 0x0cu + index * 2u);
        result.quotient = value / divisor;
        result.remainder = value % divisor;
        if (!memory->fault)
            game_memory_write_u16(memory, object + 0x0cu + index * 2u,
                (uint16_t)result.quotient);
    }
    return result;
}

static float game_leaf_bits_float(uint32_t bits) {
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

long double game_leaf_horizontal_distance_squared(GameMemory *memory,
    GameAddress point, GameAddress reference) {
    long double dx = (long double)game_leaf_bits_float(
        game_memory_read_u32(memory, point)) -
        (long double)game_leaf_bits_float(
            game_memory_read_u32(memory, reference + 0x0cu));
    long double dz = (long double)game_leaf_bits_float(
        game_memory_read_u32(memory, point + 8u)) -
        (long double)game_leaf_bits_float(
            game_memory_read_u32(memory, reference + 0x14u));
    return dz * dz + dx * dx;
}
