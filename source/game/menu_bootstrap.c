#include "menu_music.h"
#include "sound_platform.h"
#include "menu_settings.h"
#include "game_initializers.h"
#include <math.h>
#include <string.h>

extern int game_menu_startup_systems(GameApplication *); /* Actual 00505b30. */

static long double read_double(GameMemory *m,GameAddress address) {
    uint64_t bits=game_memory_read_u32(m,address);double value;
    bits|=(uint64_t)game_memory_read_u32(m,address+4u)<<32;
    memcpy(&value,&bits,sizeof(value));return value;
}
static void write_float(GameMemory *m,GameAddress address,float value) {
    uint32_t bits;memcpy(&bits,&value,4u);game_memory_write_u32(m,address,bits);
}
/* 00519f00: initialize the real transform stacks and 256 sine/cosine pairs.
 * These are C numeric algorithms, with no floating-point instruction emulator. */
static int initialize_render_state(GameApplication *a) {
    GameMemory *m=&a->memory;unsigned i;
    long double first_factor=read_double(m,0x575558u),second_factor=read_double(m,0x575560u);
    game_memory_write_u32(m,0x627074u,0);game_memory_write_u32(m,0x627080u,0);
    game_memory_write_u32(m,0x626e10u,0);
    if(!game_initializer_00519230(m))return 0;
    game_memory_write_u32(m,0x626e10u,1);
    if(!game_initializer_00519230(m))return 0;
    for(i=0;i<12u;++i)game_memory_write_u32(m,0x626e18u+i*4u,
        (i==0u || i==5u || i==10u)?0x3f800000u:0);
    game_memory_write_u32(m,0x626e10u,0);game_memory_write_u32(m,0x627060u,1);
    for(i=0;i<256u;++i) {
        long double angle=(long double)(i*2u)*first_factor;
        angle*=second_factor;
        write_float(m,0x6270f8u+i*8u,(float)cosl(angle));
        write_float(m,0x6270fcu+i*8u,(float)sinl(angle));
    }
    return !m->fault;
}
int game_menu_bootstrap(GameApplication *a,GameAddress event) {
    GameMemory *m=&a->memory;GameAddress settings;uint32_t volume;
    (void)event;
    if(!initialize_render_state(a) || !game_menu_startup_systems(a) ||
        !game_menu_settings_begin(a))return 0;
    settings=game_memory_read_u32(m,0x5943c8u);
    volume=game_memory_read_u32(m,settings+0x54u);
    if(volume!=UINT32_MAX)game_music_volume(volume);
    volume=game_memory_read_u32(m,settings+0x58u);
    if(volume!=UINT32_MAX)sound_platform_master_volume(volume);
    return !m->fault;
}
