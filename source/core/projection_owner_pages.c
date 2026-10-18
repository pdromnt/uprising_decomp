#include "projection_owner_pages.h"
static uint16_t pages[UINT32_C(1)<<20u];
const uint16_t *projection_owner_pages(void){return pages;}
int projection_owner_pages_change(GameAddress address,size_t size,int adding){
    uint64_t end=(uint64_t)address+size;uint32_t first,last,index;
    if(!size || end>UINT64_C(0x100000000))return 0;
    first=address>>12u;last=(uint32_t)((end-1u)>>12u);
    for(index=first;index<=last;index++)
        if(adding?pages[index]==UINT16_MAX:pages[index]==0u)return 0;
    for(index=first;index<=last;index++){
        if(adding)++pages[index];else --pages[index];
    }
    return 1;
}
