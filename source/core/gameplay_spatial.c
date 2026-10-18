#include "gameplay_spatial.h"
#include "projection_owner_pages.h"
#include <string.h>
#include <math.h>

static int32_t spatial_float_integer(float value){
    if(!isfinite(value) || (long double)value<-2147483648.0L || (long double)value>=2147483648.0L)return INT32_MIN;
    return (int32_t)value;
}
static int32_t spatial_floor_divide(int32_t value,int32_t divisor){
    return value>=0?value/divisor:-1-(int32_t)((-(int64_t)value-1)/divisor);
}
int gameplay_spatial_select_visible_cells(GameMemory *memory,float x,float z,uint16_t heading){
    int32_t direction=spatial_floor_divide((int16_t)heading,64);
    int32_t count=(int16_t)game_memory_read_u16(memory,0x5ff810u+(uint32_t)direction*2u);
    int32_t origin_x=spatial_floor_divide(spatial_float_integer(x),4096);
    int32_t origin_z=spatial_floor_divide(spatial_float_integer(z),4096);
    GameAddress offsets=0x5ff850u+(uint32_t)direction*324u;uint32_t written=0;int32_t index;
    for(index=0;index<count && !memory->fault;index++){
        int64_t cell_x=(int64_t)origin_x+(int16_t)game_memory_read_u16(memory,offsets+(uint32_t)index*4u);
        int64_t cell_z=(int64_t)origin_z+(int16_t)game_memory_read_u16(memory,offsets+(uint32_t)index*4u+2u);
        if(cell_x>=0 && cell_x<32 && cell_z>=0 && cell_z<32){
            game_memory_write_u16(memory,0x5f06b8u+written*4u,(uint16_t)cell_x);
            game_memory_write_u16(memory,0x5f06bau+written*4u,(uint16_t)cell_z);written++;
        }
    }
    game_memory_write_u32(memory,0x6020d0u,written);return !memory->fault;
}

static GameplaySpatialGrid world_grid;
GameplaySpatialGrid *gameplay_spatial_grid(GameAddress manager){
    return world_grid.active && world_grid.identity==manager?&world_grid:NULL;
}
int gameplay_spatial_begin(GameAddress manager){
    unsigned plane,row,column;
    if(manager!=0x005f0800u)return 0;
    if(world_grid.active){
        for(plane=0u;plane<2u;++plane)for(row=0u;row<32u;++row)for(column=0u;column<32u;++column)
            if(world_grid.heads[plane][row][column])return -1;
    }else if(!projection_owner_pages_change(manager,0xf00cu,1))return -1;
    memset(&world_grid,0,sizeof(world_grid));world_grid.identity=manager;world_grid.active=1;
    return 1;
}
int gameplay_spatial_end(void){
    unsigned plane,row,column;
    if(!world_grid.active)return 1;
    for(plane=0u;plane<2u;++plane)for(row=0u;row<32u;++row)for(column=0u;column<32u;++column)
        if(world_grid.heads[plane][row][column])return 0;
    if(!projection_owner_pages_change(world_grid.identity,0xf00cu,0))return 0;
    memset(&world_grid,0,sizeof(world_grid));return 1;
}
static unsigned neighboring_axis(unsigned cell,unsigned values[3]){
    if(!cell){values[0]=0u;values[1]=1u;return 2u;}
    if(cell==31u){values[0]=31u;values[1]=30u;return 2u;}
    values[0]=cell-1u;values[1]=cell;values[2]=cell+1u;return 3u;
}
static void neighboring_cells(unsigned row,unsigned column,unsigned output[9]){
    unsigned rows[3],columns[3],nr=neighboring_axis(row,rows),nc=neighboring_axis(column,columns),r,c,n=0u;
    /* Edge ordering is part of collision priority. */
    if(column==0u || column==31u){
        for(c=0u;c<nc;++c)for(r=0u;r<nr;++r)output[n++]=rows[r]*32u+columns[c];
    }else for(r=0u;r<nr;++r)for(c=0u;c<nc;++c)output[n++]=rows[r]*32u+columns[c];
    while(n<9u)output[n++]=1024u;
}
static void neighboring_buckets(GameAddress manager,unsigned row,unsigned column,GameAddress output[9]){
    unsigned cells[9],i;neighboring_cells(row,column,cells);
    for(i=0u;i<9u;++i)output[i]=manager+(cells[i]==1024u?0xf000u:cells[i]*12u);
}
static int query_cell(float x,float z,unsigned *row,unsigned *column){
    uint32_t xb,zb;memcpy(&xb,&x,4u);memcpy(&zb,&z,4u);
    if(xb>0x80000000u || zb>0x80000000u || (int32_t)xb>=0x48000000 || (int32_t)zb>=0x48000000)return 0;
    *row=(unsigned)(int32_t)x>>12u;*column=(unsigned)(int32_t)z>>12u;return 1;
}
int gameplay_spatial_query_heads(GameplaySpatialGrid *grid,float x,float z,GameAddress heads[9]){
    unsigned row,column,cells[9],i;
    if(!grid || !grid->active || !query_cell(x,z,&row,&column))return 0;
    neighboring_cells(row,column,cells);
    for(i=0u;i<9u;++i)heads[i]=cells[i]==1024u?0u:grid->heads[0][cells[i]/32u][cells[i]%32u];
    return 1;
}
int gameplay_spatial_query_values(GameAddress manager,float x,float z,GameAddress buckets[9]){
    unsigned row,column;
    if(!query_cell(x,z,&row,&column))return 0;
    neighboring_buckets(manager,row,column,buckets);return 1;
}
int gameplay_spatial_query_group(GameAddress manager,float x,float z,GameAddress *group){
    GameAddress buckets[9];
    if(!group || !gameplay_spatial_query_values(manager,x,z,buckets))return 0;
    *group=manager+0x6000u+(((uint32_t)(int32_t)x>>12u)*32u+((uint32_t)(int32_t)z>>12u))*36u;
    return 1;
}
static int spatial_projection_word(GameAddress address,uint32_t *value,uint32_t **mutable){
    GameAddress offset;unsigned plane,cell,field;GameAddress neighbors[9];
    *mutable=NULL;
    if(!world_grid.active || address<world_grid.identity || address>=world_grid.identity+0xf00cu || (address&3u))return 0;
    offset=address-world_grid.identity;
    if(offset<0x6000u){
        plane=offset/0x3000u;cell=(offset%0x3000u)/12u;field=offset%12u;
        if(field==4u){*mutable=&world_grid.heads[plane][cell/32u][cell%32u];*value=**mutable;}
        else *value=field==0u?0x00572230u:0u;
    }else if(offset<0xf000u){
        cell=(offset-0x6000u)/36u;neighboring_buckets(world_grid.identity,cell/32u,cell%32u,neighbors);
        *value=neighbors[((offset-0x6000u)%36u)/4u];
    }else *value=offset==0xf000u?0x00572230u:0u;
    return 1;
}
int gameplay_spatial_projection_read(GameAddress address,void *output,size_t size){
    uint32_t word,*mutable;unsigned shift=(address&3u)*8u;
    if(!size || size>4u || shift+size*8u>32u || !spatial_projection_word(address&~3u,&word,&mutable))return 0;
    word>>=shift;memcpy(output,&word,size);return 1;
}
int gameplay_spatial_projection_write(GameAddress address,const void *input,size_t size){
    uint32_t word,*mutable,value=0u,mask;unsigned shift=(address&3u)*8u;
    if(!size || size>4u || shift+size*8u>32u || !spatial_projection_word(address&~3u,&word,&mutable) || !mutable)return 0;
    memcpy(&value,input,size);mask=size==4u?UINT32_MAX:((1u<<(size*8u))-1u)<<shift;
    *mutable=(word&~mask)|((value<<shift)&mask);return 1;
}

GameplaySpatialInsertResult gameplay_spatial_insert(GameMemory *memory,
    GameAddress manager, GameAddress entity, int32_t x_cell,
        int32_t z_cell, int alternate_plane) {
    GameplaySpatialInsertResult result = {0};
    GameplaySpatialGrid *grid=gameplay_spatial_grid(manager);
    uint32_t cell = (uint32_t)x_cell * 32u + (uint32_t)z_cell;
    if(grid && !gameplay_spatial_cell_inside(x_cell,z_cell)){
        memory->fault=1;memory->fault_address=manager;memory->fault_size=4u;memory->fault_write=1;
        return result;
    }
    result.bucket_base = manager + cell * 12u;
    result.bucket = result.bucket_base + (alternate_plane ? 0x3000u : 0u);
    game_memory_write_u32(memory, entity + 8u, result.bucket);
    if (!memory->fault)
        result.previous_head = grid?grid->heads[alternate_plane!=0][x_cell][z_cell]:
            game_memory_read_u32(memory,result.bucket + 4u);
    if (!memory->fault)
        game_memory_write_u32(memory, entity + 4u, result.previous_head);
    if (!memory->fault && result.previous_head != 0u)
        game_memory_write_u32(memory, result.previous_head + 8u, entity);
    if (!memory->fault){
        if(grid)grid->heads[alternate_plane!=0][x_cell][z_cell]=entity;
        else game_memory_write_u32(memory, result.bucket + 4u, entity);
    }
    return result;
}

GameAddress gameplay_spatial_extended_bucket(GameAddress manager,
        int32_t combined_cell) {
    return manager + 0x6000u + (uint32_t)combined_cell * 36u;
}

void gameplay_spatial_clear_mask(GameMemory *memory, GameAddress mask,
        uint32_t dword_count, int reverse) {
    uint32_t index;
    for (index = 0u; index < dword_count && !memory->fault; ++index)
        game_memory_write_u32(memory,
            reverse ? mask - index * 4u : mask + index * 4u, 0u);
}

uint8_t gameplay_spatial_mask_value(GameMemory *memory, GameAddress mask,
        uint32_t cell_index) {
    return game_memory_read_u8(memory, mask + cell_index);
}

uint32_t gameplay_spatial_mask_bit(GameMemory *memory, GameAddress mask,
        uint32_t cell_index, uint32_t bit_index) {
    uint32_t bit = 1u << (bit_index & 31u);
    return gameplay_spatial_mask_value(memory, mask, cell_index) & bit;
}

int gameplay_spatial_cell_inside(int32_t x_cell, int32_t z_cell) {
    return x_cell >= 0 && z_cell >= 0 && x_cell < 32 && z_cell < 32;
}

static void write_neighbor_group(GameMemory *memory, GameAddress output,
        const GameAddress values[9]) {
    unsigned index;
    for (index = 0u; index < 9u && !memory->fault; ++index)
        game_memory_write_u32(memory, output + index * 4u, values[index]);
}

void gameplay_spatial_initialize_neighbors(GameMemory *memory,
        GameAddress manager) {
    const GameAddress sentinel = manager + 0xf000u;
    unsigned row, column;
    const GameAddress top_left[9] = {manager, manager + 0x180u,
        manager + 0x0cu, manager + 0x18cu, sentinel, sentinel, sentinel,
        sentinel, sentinel};
    const GameAddress top_right[9] = {manager + 0x174u,
        manager + 0x2f4u, manager + 0x168u, manager + 0x2e8u,
        sentinel, sentinel, sentinel, sentinel, sentinel};
    const GameAddress bottom_left[9] = {manager + 0x2e80u,
        manager + 0x2d00u, manager + 0x2e8cu, manager + 0x2d0cu,
        sentinel, sentinel, sentinel, sentinel, sentinel};
    const GameAddress bottom_right[9] = {manager + 0x2ff4u,
        manager + 0x2e74u, manager + 0x2fe8u, manager + 0x2e68u,
        sentinel, sentinel, sentinel, sentinel, sentinel};
    game_memory_write_u32(memory, sentinel + 8u, 0u);
    game_memory_write_u32(memory, sentinel + 4u, 0u);
    game_memory_write_u32(memory, sentinel, 0x00572230u);
    write_neighbor_group(memory, manager + 0x6000u, top_left);
    write_neighbor_group(memory, manager + 0x645cu, top_right);
    write_neighbor_group(memory, manager + 0xeb80u, bottom_left);
    write_neighbor_group(memory, manager + 0xefdcu, bottom_right);
    for (column = 0u; column < 30u && !memory->fault; ++column) {
        GameAddress source = manager + column * 12u;
        GameAddress values[9] = {source, source + 0x0cu,
            source + 0x18u, source + 0x180u, source + 0x18cu,
            source + 0x198u, sentinel, sentinel, sentinel};
        write_neighbor_group(memory, manager + 0x6024u + column * 36u,
            values);
    }
    for (column = 0u; column < 30u && !memory->fault; ++column) {
        GameAddress source = manager + 0x2e80u + column * 12u;
        GameAddress values[9] = {source, source + 0x0cu,
            source + 0x18u, source - 0x180u, source - 0x174u,
            source - 0x168u, sentinel, sentinel, sentinel};
        write_neighbor_group(memory, manager + 0xeba4u + column * 36u,
            values);
    }
    for (row = 0u; row < 30u && !memory->fault; ++row) {
        GameAddress source = manager + row * 0x180u;
        GameAddress left[9] = {source, source + 0x180u,
            source + 0x300u, source + 0x0cu, source + 0x18cu,
            source + 0x30cu, sentinel, sentinel, sentinel};
        source = manager + 0x174u + row * 0x180u;
        GameAddress right[9] = {source, source + 0x180u,
            source + 0x300u, source - 0x0cu, source + 0x174u,
            source + 0x2f4u, sentinel, sentinel, sentinel};
        write_neighbor_group(memory, manager + 0x6480u + row * 0x480u,
            left);
        write_neighbor_group(memory, manager + 0x68dcu + row * 0x480u,
            right);
    }
    for (row = 0u; row < 30u && !memory->fault; ++row) {
        GameAddress source_row = manager + row * 0x180u;
        GameAddress output_row = manager + 0x64a4u + row * 0x480u;
        for (column = 0u; column < 30u && !memory->fault; ++column) {
            GameAddress source = source_row + column * 12u;
            GameAddress values[9] = {source, source + 0x0cu,
                source + 0x18u, source + 0x180u, source + 0x18cu,
                source + 0x198u, source + 0x300u, source + 0x30cu,
                source + 0x318u};
            write_neighbor_group(memory, output_row + column * 36u,
                values);
        }
    }
}

GameAddress gameplay_spatial_bucket_head(GameAddress manager,
        uint32_t row, uint32_t column, uint32_t plane) {
    return manager + 4u + row * 0x180u + column * 12u +
        plane * 0x3000u;
}
