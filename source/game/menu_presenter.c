#include "menu_widgets.h"

extern int game_menu_surface_prepare(GameApplication *,int *restored);
extern int game_menu_surface_present_rectangle(GameApplication *,const GameUiRect *);
extern int game_menu_loading_tick(GameApplication *,GameAddress); /* 00492100 */

static GameUiRect presenter_rect(GameMemory *m,GameAddress at) {
    GameUiRect r;
    r.left=(int32_t)game_memory_read_u32(m,at);r.top=(int32_t)game_memory_read_u32(m,at+4u);
    r.right=(int32_t)game_memory_read_u32(m,at+8u);r.bottom=(int32_t)game_memory_read_u32(m,at+12u);
    return r;
}
static int presenter_intersection(GameUiRect *out,GameUiRect a,GameUiRect b) {
    out->left=a.left>b.left?a.left:b.left;out->top=a.top>b.top?a.top:b.top;
    out->right=a.right<b.right?a.right:b.right;out->bottom=a.bottom<b.bottom?a.bottom:b.bottom;
    return out->left<out->right && out->top<out->bottom;
}
static int presenter_snapshot(GameMemory *m,GameAddress node) {
    unsigned i;
    for(i=0;i<4u;++i)game_memory_write_u32(m,node+0x30u+i*4u,
        game_memory_read_u32(m,node+0x20u+i*4u));
    game_memory_write_u32(m,node+0x1cu,0);return !m->fault;
}
/* 004abdc0: consume the existing sorted drawable list and its dirty snapshots.
 * The platform owns surfaces; this function owns neither pixels nor a second
 * drawable list. Foreign draw methods remain failures, never skipped gameplay. */
int game_menu_window_present(GameApplication *a,GameAddress window) {
    GameMemory *m=&a->memory;GameAddress head=game_memory_read_u32(m,window+0x78u),node;
    GameUiRect bounds=presenter_rect(m,window+0x130u),clip;int restored=0,full;
    unsigned visits=0;
    if(m->fault)return 0;
    if(!head)return 1; /* Retail performs no presentation with an empty list. */
    if(!game_menu_surface_prepare(a,&restored))return 0;
    if(restored) {
        if(!game_memory_read_u32(m,head+0x1cu)) {
            unsigned i;
            for(i=0;i<4u;++i)game_memory_write_u32(m,head+0x30u+i*4u,
                game_memory_read_u32(m,head+0x20u+i*4u));
            game_memory_write_u32(m,head+0x1cu,1);
        }
        game_memory_write_u32(m,0x58fd3cu,1);
    }
    full=game_memory_read_u32(m,head+0x1cu)!=0 || game_memory_read_u32(m,0x58fd3cu)!=0;
    if(full) {
        for(node=head;node;node=game_memory_read_u32(m,node+8u)) {
            if(++visits>65536u || m->fault)return 0;
            if(presenter_intersection(&clip,bounds,presenter_rect(m,node+0x20u)) &&
                !game_menu_widget_draw(a,node,&clip))return 0;
            if(!presenter_snapshot(m,node))return 0;
        }
        game_memory_write_u32(m,0x58fd3cu,0);
        return !m->fault && game_menu_surface_present_rectangle(a,&bounds);
    }
    {
        GameAddress event=game_memory_read_u32(m,0x59d4c4u);
        uint32_t state=event?game_memory_read_u32(m,event+0x3cu):UINT32_MAX;
        if((state==11u || state==22u) && game_memory_read_u32(m,event+0x48u))
            return !m->fault && game_menu_loading_tick(a,event);
    }
    for(node=head;node;node=game_memory_read_u32(m,node+8u)) {
        GameUiRect old_bounds=presenter_rect(m,node+0x30u);
        GameUiRect current=presenter_rect(m,node+0x20u),damage;
        GameAddress layer;unsigned layers=0;
        if(++visits>65536u || m->fault)return 0;
        if(!game_memory_read_u32(m,node+0x1cu))continue;
        damage.left=old_bounds.left<current.left?old_bounds.left:current.left;
        damage.top=old_bounds.top<current.top?old_bounds.top:current.top;
        damage.right=old_bounds.right>current.right?old_bounds.right:current.right;
        damage.bottom=old_bounds.bottom>current.bottom?old_bounds.bottom:current.bottom;
        if(!presenter_intersection(&clip,bounds,damage)) {
            if(!presenter_snapshot(m,node))return 0;
            continue;
        }
        /* Recompose the damaged pixels from the authoritative sorted list.
         * The old partial recovery drew only predecessors and the dirty node:
         * moving controls lost their new footprint, and changing lower layers
         * erased unchanged foreground controls. No extra surface/list owner is
         * needed; the same clipped draw methods also handle transparent images. */
        for(layer=head;layer;layer=game_memory_read_u32(m,layer+8u)) {
            GameUiRect visible;
            if(++layers>65536u || m->fault)return 0;
            if(presenter_intersection(&visible,clip,presenter_rect(m,layer+0x20u)) &&
               !game_menu_widget_draw(a,layer,&visible))return 0;
        }
        if(!presenter_snapshot(m,node) ||
            !game_menu_surface_present_rectangle(a,&clip))return 0;
    }
    return !m->fault;
}
