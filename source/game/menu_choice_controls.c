#include "menu_choice_controls.h"
#include "scene_projection_storage.h"
static uint32_t get(GameApplication *a,GameAddress p){return game_memory_read_u32(&a->memory,p);}
static void put(GameApplication *a,GameAddress p,uint32_t v){game_memory_write_u32(&a->memory,p,v);}
/*54d3c0: the selector owns this114-byte choice. Shared button construction
 * initializes the same record, borrowing images and the actual parameter value.
 * Labels/descriptor tables are borrowed; onlybc/c0 scratch allocations are owned. */
GameAddress game_menu_choice_create(GameApplication *a,uint32_t id,GameAddress value,GameAddress label,
        uint32_t minimum,uint32_t maximum,const GameAddress images[3]) {
    GameAddress p=scene_projection_storage_allocate(0x114,"portable research choice");
    GameAddress borrowed[4]={images[0],images[1],images[2],0};
    if(!p)return 0;
    if(!game_memory_fill_u8(&a->memory,p,0x114,0,0) ||
       !game_menu_button_borrowed_initialize(a,p,id,borrowed,UINT32_MAX,UINT32_MAX,0,UINT32_MAX,0))goto failed;
    put(a,p,0x577580);put(a,p+0xac,value);put(a,p+0x94,label);
    put(a,p+0xa4,minimum);put(a,p+0xa8,maximum);put(a,p+0xb0,1000);
    put(a,p+0x9c,0x5e1448);put(a,p+0xa0,1);put(a,p+0x6c,0x54d3a0);
    if(!a->memory.fault)return p;
failed:scene_projection_storage_free(p);return 0;
}
/*54d4c0: bounded ten-option metadata, selecting native choice behavior. */
int game_menu_choice_option(GameApplication *a,GameAddress p,unsigned index,GameAddress label,uint32_t value) {
    if(index>=10)return 0;
    put(a,p+0xc4+index*8,label);put(a,p+0xc8+index*8,value);return !a->memory.fault;
}
/*54d5f0: return the original unscaled charge/refund and debit the same budget.
 * Rejection has charge0; it is not a memory/service failure. All arithmetic
 * wraps explicitly before the original signed availability check. */
int game_menu_choice_charge(GameApplication *a,GameAddress p,int32_t target,uint32_t *charge) {
    uint32_t bits=get(a,get(a,p+0xac)),cost=0,index;GameAddress table,account;
    int64_t current=bits<0x80000000u?(int64_t)bits:(int64_t)bits-INT64_C(4294967296);
    *charge=0;if(a->memory.fault)return 0;
    if(current!=target) {
        table=get(a,p+0xb4);index=(uint32_t)(current<target?target:current);
        if(table) {if(index>=10)return 0;cost=get(a,table+index*176+0x50);}
        else {
            table=get(a,p+0xb8);
            if(table){if(index>=10)return 0;cost=get(a,table+index*112+0x30);}
            else cost=get(a,p+0xa0);
        }
        if(current>target)cost=0u-cost;
    }
    account=get(a,p+0x9c);bits=get(a,account)-cost*get(a,p+0xb0);
    if(a->memory.fault)return 0;
    if(bits&0x80000000u)return 1;
    put(a,account,bits);*charge=cost;return !a->memory.fault;
}
/*54d4e0: the same allocation reaches base button release after scratch owners
 * retire. Popup selection borrows the row, so invalidate it before freeing. */
int game_menu_choice_release(GameApplication *a,GameAddress p) {
    unsigned i;GameAddress scratch;
    for(i=0xbc;i<=0xc0;i+=4) {
        scratch=get(a,p+i);
        if(scratch && !scene_projection_storage_free(scratch))return 0;
        put(a,p+i,0);
    }
    if(get(a,0x5977c0)==p)put(a,0x5977c0,0);
    put(a,p,0x575908);return !a->memory.fault && game_menu_widget_destroy(a,p);
}
