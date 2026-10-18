#include "application_entry.h"
#include "scene_projection_storage.h"
/*4f0190/4f01c0 are the asset loader's temporary name array, shared by preview
 * and level loading. Keep its verified bounds and retire only owned handles.
 * Native material names themselves belong to MaterialCache, not this scratch. */
int game_level_geometry_prepare(GameApplication *a) {
    GameMemory *m=&a->memory;
    if(game_memory_read_u32(m,0x596278) && !game_memory_fill_u32(m,0x605900,512,0,0))return 0;
    game_memory_write_u32(m,0x606698,0);game_memory_write_u32(m,0x596278,0);
    return !m->fault;
}
int game_level_geometry_finalize(GameApplication *a) {
    GameMemory *m=&a->memory;uint32_t count=game_memory_read_u32(m,0x606698),i;
    if(count&0x80000000u)return !m->fault;
    if(count>512)return 0;
    for(i=0;i<count;++i) {
        GameAddress slot=0x605900+i*4,p=game_memory_read_u32(m,slot);
        if(m->fault || (p && !scene_projection_storage_free(p)))return 0;
        game_memory_write_u32(m,slot,0);
    }
    return !m->fault;
}
