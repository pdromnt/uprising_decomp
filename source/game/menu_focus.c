#include "application_entry.h"
#include "scene_projection_storage.h"

static uint32_t read(GameMemory *m, GameAddress p, unsigned offset) {
    return game_memory_read_u32(m,p+offset);
}
static void write(GameMemory *m, GameAddress p, unsigned offset, uint32_t value) {
    game_memory_write_u32(m,p+offset,value);
}
/* 00522350/00522030/00522120: focus owns its links. The menu state owns
 * receivers; removing focus never destroys a receiver. */
int game_menu_resources_begin(GameApplication *app) {
    GameMemory *m=&app->memory;
    GameAddress root=read(m,0x5980c4u,0);
    if (root) return !m->fault;
    root=scene_projection_storage_allocate(12u,"portable menu focus");
    if (!root) return 0;
    game_memory_fill_u8(m,root,12u,0,0); write(m,0x5980c4u,0,root);
    return !m->fault;
}
int game_menu_focus_add(GameApplication *app,GameAddress receiver) {
    GameMemory *m=&app->memory;
    GameAddress root=read(m,0x5980c4u,0),node,tail;
    if (!root) return !m->fault;
    node=scene_projection_storage_allocate(12u,"portable menu focus link");
    if (!node) return 0;
    tail=read(m,root,4);
    write(m,node,0,receiver); write(m,node,4,0); write(m,node,8,tail);
    if (tail) write(m,tail,4,node); else write(m,root,0,node);
    write(m,root,4,node); return !m->fault;
}
int game_menu_focus_remove(GameApplication *app,GameAddress receiver) {
    GameMemory *m=&app->memory;
    GameAddress root=read(m,0x5980c4u,0),node;
    unsigned visits=0;
    if (!root) return !m->fault;
    node=read(m,root,0);
    while (node && !m->fault) {
        GameAddress next=read(m,node,4),previous=read(m,node,8);
        if (++visits>65536u) return 0;
        if (read(m,node,0)==receiver) {
            if (read(m,root,8)==node) write(m,root,8,0);
            if (previous) write(m,previous,4,next); else write(m,root,0,next);
            if (next) write(m,next,8,previous); else write(m,root,4,previous);
            return !m->fault && scene_projection_storage_free(node);
        }
        node=next;
    }
    return !m->fault;
}
void game_menu_resources_end(GameApplication *app) {
    GameMemory *m=&app->memory;
    GameAddress root=read(m,0x5980c4u,0),node;
    unsigned visits=0;
    if (!root) return;
    node=read(m,root,0);
    while (node && !m->fault && ++visits<=65536u) {
        GameAddress next=read(m,node,4);
        if (!scene_projection_storage_free(node)) return;
        write(m,root,0,next); node=next;
    }
    if (node || m->fault) return;
    scene_projection_storage_free(root); write(m,0x5980c4u,0,0);
}
