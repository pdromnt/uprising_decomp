#include "texture_residency.h"
#include <stdlib.h>
struct TextureResidencyBlock {
    uint32_t address,size,owner;
    struct TextureResidencyBlock *next;
};
void texture_residency_release(TextureResidency *ring) {
    TextureResidencyBlock *first=ring->cursor,*node;
    if(first){node=first->next;while(node!=first){TextureResidencyBlock *next=node->next;free(node);node=next;}free(first);}
    ring->cursor=NULL;ring->base=ring->capacity=0;
}
int texture_residency_reset(TextureResidency *ring,uint32_t base,uint32_t capacity) {
    TextureResidencyBlock *block;
    if(!capacity || capacity>UINT32_MAX-base)return 0;
    block=calloc(1,sizeof(*block));if(!block)return 0;
    texture_residency_release(ring);block->address=base;block->size=capacity;block->next=block;
    ring->cursor=block;ring->base=base;ring->capacity=capacity;return 1;
}
int texture_residency_allocate(TextureResidency *ring,uint32_t owner,uint32_t size,
    uint32_t frame,TextureLastUse last_use,TextureEvict evict,void *context,uint32_t *address) {
    TextureResidencyBlock *node=ring->cursor,*start=NULL,*end=NULL,*tail;uint32_t total=0;
    if(!node || !owner || !size || size>ring->capacity)return 0;
    do {
        if(start && node->address!=start->address+total){start=NULL;total=0;}
        if(node->owner && last_use(context,node->owner)>=frame-1){start=NULL;total=0;}
        else {if(!start)start=node;total+=node->size;if(total>=size){end=node;break;}}
        node=node->next;
    } while(node!=ring->cursor);
    if(!end)return 0;
    /* Allocate before mutation so allocation failure cannot evict live state. */
    tail=total>size?calloc(1,sizeof(*tail)):NULL;
    if(total>size && !tail)return 0;
    node=start;
    for(;;){if(node->owner)evict(context,node->owner);if(node==end)break;node=node->next;}
    node=start->next;
    if(start!=end){TextureResidencyBlock *after=end->next;while(node!=after){TextureResidencyBlock *next=node->next;free(node);node=next;}start->next=after;}
    start->owner=owner;start->size=size;
    if(tail){tail->address=start->address+size;tail->size=total-size;tail->next=start->next;start->next=tail;}
    ring->cursor=start->next;*address=start->address;return 1;
}

void texture_residency_forget(TextureResidency *ring,uint32_t owner) {
    TextureResidencyBlock *node=ring->cursor;if(!node)return;
    do {if(node->owner==owner)node->owner=0;node=node->next;}while(node!=ring->cursor);
}
