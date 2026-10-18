#include "application_entry.h"
#include "menu_runtime_tables.h"
#include "asset_bitmap.h"
#include <math.h>
extern uint32_t game_menu_milliseconds(GameApplication *);
extern int game_menu_palette_read_bytes(GameApplication *,uint8_t[1024]);
extern int game_menu_palette_write_bytes(GameApplication *,const uint8_t[1024]);
extern int game_menu_transition_notify(GameApplication *);
/*00517ac0/004db110: palette-only bitmaps share the resource/blob owner.
 * A partial palette retains the original332 defaults for its remaining entries. */
int game_menu_palette_load_named(GameApplication *a,GameAddress name) {
    char path[260];ResourceBlob blob={0};AssetBitmap bitmap;uint8_t encoded[1028],bytes[1024];
    unsigned i;size_t written;int ok;
    for(i=0;i<sizeof(path);++i){path[i]=(char)game_memory_read_u8(&a->memory,name+i);if(!path[i])break;}
    if(i==sizeof(path) || a->memory.fault || !game_application_load_blob(a,path,&blob))return 0;
    ok=asset_bitmap_startup_palette(&blob,&bitmap,encoded,sizeof(encoded),&written);
    if(ok) {
        for(i=0;i<256;++i){bytes[i*4]=(uint8_t)((i>>5)*255/7);bytes[i*4+1]=(uint8_t)(((i>>2)&7)*255/7);
            bytes[i*4+2]=(uint8_t)((i&3)*255/3);bytes[i*4+3]=0;}
        for(i=0;i<bitmap.palette_count*4;++i)bytes[i]=encoded[i+4];
        ok=game_menu_palette_write_bytes(a,bytes);
    }
    resource_blob_release(&blob);return ok && !a->memory.fault;
}

/*0051a840/0051a890: the palette owner is the native window. This1024-byte
 * snapshot is the original fade input, not a competing writable palette. */
int game_menu_palette_fade_begin(GameApplication *a,uint32_t duration,int direction,const uint8_t input[1024]) {
    uint32_t now=game_menu_milliseconds(a);
    game_memory_write_u32(&a->memory,0x627084,duration);
    game_memory_write_u32(&a->memory,0x597f94,(uint32_t)direction);
    game_memory_write_u32(&a->memory,0x627924,now);
    game_memory_write_u32(&a->memory,0x597fa0,now);
    return game_memory_write(&a->memory,0x626950,input,1024);
}
int game_menu_palette_fade_tick(GameApplication *a,int *active) {
    GameMemory *m=&a->memory;uint8_t palette[1024];uint32_t elapsed,duration,amount;unsigned i,c;
    long double ratio,value;int direction;
    *active=0;elapsed=game_menu_milliseconds(a)-game_memory_read_u32(m,0x627924);
    duration=game_memory_read_u32(m,0x627084);
    if((int32_t)duration<(int32_t)elapsed)return !m->fault;
    if(!game_menu_palette_read_bytes(a,palette))return 0;
    ratio=(long double)(int32_t)elapsed/(long double)(int32_t)duration;
    direction=(int32_t)game_memory_read_u32(m,0x597f94);
    value=direction==-1?ratio:(long double)game_menu_runtime_float(m,0x57554c)-(long double)(float)ratio;
    value*=game_menu_runtime_float(m,0x57556c);value*=game_menu_runtime_float(m,0x575570);
    amount=!isfinite(value) || value<-9223372036854775808.0L || value>=9223372036854775808.0L?
        0:(uint32_t)(uint64_t)(int64_t)truncl(value);
    for(i=1;i<255;++i)for(c=0;c<3;++c) {
        uint32_t original=game_memory_read_u8(m,0x626950+i*4+c),v=original-amount;
        palette[i*4+c]=(int32_t)v<0?0:(int32_t)v<(int32_t)original?(uint8_t)v:(uint8_t)original;
    }
    if(m->fault || !game_menu_palette_write_bytes(a,palette))return 0;
    *active=1;return 1;
}
/* Original keyboard callback captures the palette, fades down, captures again,
 * then fades up while draining real host messages. No synthetic frame or CPU. */
int game_menu_palette_transition(GameApplication *a,int down,int up) {
    uint8_t input[1024];unsigned phase;int active;
    for(phase=0;phase<2;++phase) {
        if(!(phase?up:down))continue;
        if(!game_menu_palette_read_bytes(a,input) || !game_memory_write(&a->memory,0x627fa8,input,1024) ||
           !game_menu_palette_fade_begin(a,2000,phase?1:-1,input))return 0;
        do {
            if(!game_menu_palette_fade_tick(a,&active))return 0;
            if(active && !game_menu_transition_notify(a))return 0;
        } while(active);
    }
    return !a->memory.fault;
}
