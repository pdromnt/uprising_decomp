#include "menu_widgets.h"
#include "menu_network.h"

extern int game_menu_popup_release(GameApplication *); /* 00527eb0. */
extern int game_menu_sound_mode(GameApplication *,uint32_t); /* 0052de20. */
extern int game_menu_text_cache_release(GameApplication *); /* 0054c210. */

/* 004913c0: retire in original consumer order and clear each publication only
 * after its owner releases it. High values in the table are global slots. */
int game_menu_event_release_assets(GameApplication *a,GameAddress event) {
    static const uint32_t slots[]={
        0x70,0x88,0x8c,0x60,0x58abe8u,0x5c,0x6c,0x64,0x68,0x58,0x50,0x54,
        0x58abfcu,0x58ac00u,0x78,0x7c,0x80,0x84,0x74,0x58abecu,0x58abf0u,
        0x94,0x98,0x58abf4u,0x58abf8u,0x90,0xbc,0xc0,0xc4,0xc8,0xcc,0xec,
        0xd0,0xd4,0xdc,0xd8,0xe4,0xe0};
    unsigned i;
    if(!game_menu_network_page_release(a,event))return 0;
    for(i=0;i<sizeof(slots)/sizeof(slots[0]);++i) {
        GameAddress slot=slots[i]<0x178u?event+slots[i]:slots[i];
        GameAddress object=game_memory_read_u32(&a->memory,slot);
        if(object && !game_menu_widget_destroy(a,object))return 0;
        game_memory_write_u32(&a->memory,slot,0);
    }
    return !a->memory.fault && game_menu_popup_release(a) &&
        game_menu_sound_mode(a,1u) && game_menu_text_cache_release(a);
}
