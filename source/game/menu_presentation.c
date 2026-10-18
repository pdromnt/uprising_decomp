#include "menu_widgets.h"

/* 004abba0/004abc50. Window and drawable records are the authoritative
 * intrusive list; no second host list or per-frame serialization is needed. */
int game_menu_window_attach(GameApplication *a,GameAddress object) {
    GameMemory *m=&a->memory;
    GameAddress window=game_memory_read_u32(m,0x59d4bcu);
    GameAddress node,previous=0;
    int32_t depth=(int32_t)game_memory_read_u32(m,object+0x40u);
    unsigned visits=0,i;
    if (!window) return 0;
    node=game_memory_read_u32(m,window+0x78u);
    while (node && !m->fault && (int32_t)game_memory_read_u32(m,node+0x40u)<depth) {
        if (++visits>65536u || node==object) return 0;
        previous=node; node=game_memory_read_u32(m,node+8u);
    }
    if (node==object || m->fault) return 0;
    game_memory_write_u32(m,object+8u,node);
    game_memory_write_u32(m,object+12u,previous);
    if (previous) game_memory_write_u32(m,previous+8u,object);
    else game_memory_write_u32(m,window+0x78u,object);
    if (node) game_memory_write_u32(m,node+12u,object);
    else game_memory_write_u32(m,window+0x7cu,object);
    if (!game_memory_read_u32(m,object+0x1cu)) for (i=0;i<16u;i+=4u)
        game_memory_write_u32(m,object+0x30u+i,game_memory_read_u32(m,object+0x20u+i));
    game_memory_write_u32(m,object+0x1cu,1u);
    return !m->fault;
}
int game_menu_window_detach(GameApplication *a,GameAddress object) {
    GameMemory *m=&a->memory;
    GameAddress window=game_memory_read_u32(m,0x59d4bcu);
    GameAddress next=game_memory_read_u32(m,object+8u),previous=game_memory_read_u32(m,object+12u);
    if (!window) return 0;
    if (previous) game_memory_write_u32(m,previous+8u,next);
    else game_memory_write_u32(m,window+0x78u,next);
    if (next) game_memory_write_u32(m,next+12u,previous);
    else game_memory_write_u32(m,window+0x7cu,previous);
    game_memory_write_u32(m,object+8u,0); game_memory_write_u32(m,object+12u,0);
    return !m->fault;
}
