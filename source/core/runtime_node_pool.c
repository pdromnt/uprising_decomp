#include "runtime_node_pool.h"
#include "projection_owner_pages.h"
#include "scene_projection_storage.h"
#include <stdlib.h>
#include <string.h>

enum { NODE_COUNT=1000, HEADER=0x594460u, HEADER_SIZE=28 };
typedef struct NodeBlock { RuntimeNode nodes[NODE_COUNT];struct NodeBlock *next;GameAddress projection;uint8_t registered; } NodeBlock;
static NodeBlock *blocks;
static RuntimeNode **index_nodes;
static size_t index_count;
static RuntimeNode *last_node;
static struct {
    RuntimeNode *head;
    uint32_t active,peak,initialized;
    float position[3];
    int imported;
} pool;
static RuntimeNodePoolStats stats;
static int node_state_read(void *,uint32_t,void *,size_t);
static int node_state_write(void *,uint32_t,const void *,size_t);
static int block_state_read(void *,uint32_t,void *,size_t);
static int block_state_write(void *,uint32_t,const void *,size_t);
void runtime_node_pool_stats(RuntimeNodePoolStats *output){if(output)*output=stats;}
static int node_compare(const void *left,const void *right){
    GameAddress a=(*(RuntimeNode *const *)left)->projection,b=(*(RuntimeNode *const *)right)->projection;
    return a<b?-1:a>b;
}
RuntimeNode *runtime_node_pool_find(GameAddress address){
    size_t low=0u,high=index_count;
    while(low<high){size_t middle=low+(high-low)/2u;
        if(index_nodes[middle]->projection<address)low=middle+1u;else high=middle;}
    return low<index_count && index_nodes[low]->projection==address && index_nodes[low]->live?
        index_nodes[low]:NULL;
}
static RuntimeNode *node_at(GameAddress address,uint32_t *offset){
    size_t low=0u,high=index_count;RuntimeNode *node;
    if(!index_count || address<index_nodes[0]->projection ||
       (uint64_t)address>=(uint64_t)index_nodes[index_count-1u]->projection+48u)return NULL;
    if(last_node && last_node->live && address-last_node->projection<48u){
        *offset=address-last_node->projection;return last_node;
    }
    while(low<high){size_t middle=low+(high-low)/2u;
        if(index_nodes[middle]->projection<=address)low=middle+1u;else high=middle;}
    if(!low)return NULL;
    node=index_nodes[low-1u];*offset=address-node->projection;
    if(node->live && *offset<48u){last_node=node;return node;}return NULL;
}
static GameAddress address_of(const RuntimeNode *node){return node?node->projection:0u;}
static int import_header(GameMemory *memory){
    uint32_t words[7];unsigned i;RuntimeNode *head;
    if(pool.imported)return 1;
    for(i=0u;i<7u && !memory->fault;i++)words[i]=game_memory_read_u32(memory,HEADER+i*4u);
    if(memory->fault)return 0;
    head=runtime_node_pool_find(words[0]);if(words[0] && !head)return 0;
    pool.head=head;pool.active=words[1];pool.peak=words[2];pool.initialized=words[3];
    memcpy(pool.position,words+4u,12u);
    if(!projection_owner_pages_change(HEADER,HEADER_SIZE,1))return 0;
    pool.imported=1;
    return 1;
}
static int publish_header(GameMemory *memory){
    game_memory_write_u32(memory,HEADER,address_of(pool.head));
    game_memory_write_u32(memory,HEADER+4u,pool.active);
    game_memory_write_u32(memory,HEADER+8u,pool.peak);
    game_memory_write_u32(memory,HEADER+12u,pool.initialized);
    return !memory->fault;
}
static void initialize_node(RuntimeNode *node){
    node->kind=3u;node->target=0u;node->target_identity=0u;
    node->parameter=node->sequence=node->flags=0u;
    memcpy(node->position,pool.position,sizeof(node->position));
}
int runtime_node_pool_ensure(GameMemory *memory){
    NodeBlock *block;RuntimeNode **next_index;unsigned i;
    if(!memory || memory->fault || !import_header(memory))return 0;
    if(pool.head)return 1;
    block=calloc(1u,sizeof(*block));if(!block)goto failed;
    block->projection=scene_projection_storage_allocate(48u*NODE_COUNT,"runtime presentation generation");
    if(!block->projection)goto rollback;
    if(!projection_owner_pages_change(block->projection,48u*NODE_COUNT,1))goto rollback;
    block->registered=1u;
    if(!scene_projection_storage_bind_state(block->projection,block,block_state_read,block_state_write))goto rollback;
    for(i=0u;i<NODE_COUNT;i++){
        RuntimeNode *node=block->nodes+i;
        node->projection=block->projection+i*48u;
        node->live=node->in_pool=1u;initialize_node(node);
        node->next=i?block->nodes+i-1u:NULL;
        if(i)node->next->previous=node;
    }
    next_index=realloc(index_nodes,(index_count+NODE_COUNT)*sizeof(*next_index));
    if(!next_index)goto rollback;
    index_nodes=next_index;
    for(i=0u;i<NODE_COUNT;i++)index_nodes[index_count+i]=block->nodes+i;
    index_count+=NODE_COUNT;qsort(index_nodes,index_count,sizeof(*index_nodes),node_compare);
    last_node=NULL;
    block->next=blocks;blocks=block;pool.head=block->nodes+NODE_COUNT-1u;
    pool.initialized=1u;stats.generations++;stats.allocated+=NODE_COUNT;stats.live+=NODE_COUNT;
    return publish_header(memory);
rollback:
    if(block->registered)projection_owner_pages_change(block->projection,48u*NODE_COUNT,0);
    if(block->projection)scene_projection_storage_free(block->projection);
    free(block);
failed:
    memory->fault=1;return 0;
}
GameAddress runtime_node_pool_acquire(GameMemory *memory){
    RuntimeNode *node,*previous,*next,*next_previous;
    uint32_t active,peak;
    if(!runtime_node_pool_ensure(memory))return 0u;
    node=pool.head;previous=node->previous;next=node->next;
    next_previous=next?next->previous:NULL;active=pool.active;peak=pool.peak;
    pool.head=next;node->in_pool=0u;
    /* Retail retains the last node's links when the free list becomes empty. */
    if(pool.head){node->previous=node->next=NULL;pool.head->previous=NULL;}
    ++pool.active;if((int32_t)pool.active>(int32_t)pool.peak)pool.peak=pool.active;
    if(!publish_header(memory)){
        pool.head=node;pool.active=active;pool.peak=peak;node->in_pool=1u;
        node->previous=previous;node->next=next;
        if(next)next->previous=next_previous;
        return 0u;
    }
    ++stats.acquired;
    return node->projection;
}
int runtime_node_pool_prepare(GameMemory *memory,GameAddress address){
    RuntimeNode *node=runtime_node_pool_find(address);
    if(!memory || memory->fault || !node)return 0;
    initialize_node(node);return !memory->fault;
}
int runtime_node_pool_return_chain(GameMemory *memory,GameAddress address){
    RuntimeNode *first,*last,*cursor,*split=NULL,*prefix_last=NULL,*slow,*fast,*cycle=NULL,*before_cycle=NULL;
    uint32_t count=0u,probes=0u;int entered=0;
    if(!memory || memory->fault)return -1;
    if(!address)return 1;
    first=runtime_node_pool_find(address);if(!first)return 0;
    if(first->in_pool)return publish_header(memory)?1:-1;
    /*53c840 can restore a linear prefix followed by a suffix ring. Find its
     *entry before retirement so each owned node is returned exactly once. */
    slow=fast=first;
    while(fast && fast->next){
        if(++probes>stats.live){memory->fault=1;return -1;}
        slow=slow->next;fast=fast->next->next;
        if(slow==fast){cycle=first;while(cycle!=slow){cycle=cycle->next;slow=slow->next;}break;}
    }
    if(cycle && cycle!=first && !(cycle->flags&1u)){memory->fault=1;return -1;}
    cursor=first;last=first;
    do {
        if(!cursor->live || cursor->in_pool || ++count>stats.live){memory->fault=1;return -1;}
        if(cursor==cycle){if(count>1)before_cycle=last;entered=1;}
        if(!(first->flags&1u) && !split && cursor!=first && (cursor->flags&1u)){
            split=cursor;prefix_last=last;
        }
        last=cursor;cursor=cursor->next;
    }while(cursor && !(entered && cursor==cycle));
    /*Prepending a temporary command makes the ring entry's back link point
     *to the prefix. Restored rings can still retain their tail back link.
     *Both describe the same bounded forward chain; unrelated links are invalid. */
    if(cycle && cycle!=first && cycle->previous!=last &&
       cycle->previous!=before_cycle){memory->fault=1;return -1;}
    cursor=first;
    for(uint32_t i=0u;i<count;i++){cursor->in_pool=1u;cursor=cursor->next;}
    if(cycle && cycle!=first){
        cycle->previous=before_cycle;
        last->next=pool.head;if(pool.head)pool.head->previous=last;
        first->previous=NULL;pool.head=first;
    }else if(split){
        prefix_last->next=pool.head;
        if(pool.head)pool.head->previous=prefix_last;
        first->previous=last;last->next=first;split->previous=NULL;
        pool.head=split;
    }else {
        last->next=pool.head;
        if(pool.head)pool.head->previous=last;
        first->previous=NULL;pool.head=first;
    }
    pool.active-=count;stats.returned+=count;
    return publish_header(memory)?1:-1;
}
static void header_words(uint32_t words[7]){
    words[0]=address_of(pool.head);words[1]=pool.active;words[2]=pool.peak;words[3]=pool.initialized;
    memcpy(words+4u,pool.position,12u);
}
int runtime_node_pool_advance(GameMemory *memory,RuntimeNode *node,GameStateRelinkResult *result){
    RuntimeNode *successor,*predecessor,*head,*cursor,*next;uint32_t visits=0u;
    if(!memory || memory->fault || !node || !node->live || node->in_pool || !result)return 0;
    memset(result,0,sizeof(*result));successor=node->next;
    result->final_eax=address_of(successor);result->flag_value=node->flags&255u;
    result->flag_kind=GAME_STATE_RELINK_TEST_BYTE;
    if(node->flags&1u)return 1;
    if(!successor){memory->fault=1;return 0;}
    predecessor=node->previous;head=pool.head;
    if(predecessor)predecessor->next=successor;
    successor->previous=predecessor;
    node->next=node->previous=NULL;
    result->seed_left=pool.active;result->seed_right=1u;
    result->modified=1;
    if(runtime_node_pool_return_chain(memory,node->projection)!=1)return 0;
    result->final_ecx=successor->flags;
    result->final_edx=address_of(head);result->flag_value=successor->flags&255u;
    if(successor->flags&1u){
        cursor=successor;next=NULL;
        while(cursor){
            if(++visits>stats.live){memory->fault=1;return 0;}
            next=cursor->next;result->seed_left=successor->projection;
            result->seed_right=address_of(next);result->seed_compare=1;
            if(next==successor)break;
            cursor=next;
        }
        result->final_ecx=address_of(cursor);result->final_edx=address_of(next);
        result->flag_value=address_of(cursor);result->flag_kind=GAME_STATE_RELINK_TEST_WORD;
        if(cursor)successor->previous=cursor;
    }
    return 1;
}
static void node_words(const RuntimeNode *node,uint32_t words[12]){
    words[0]=0x5749b0u;words[1]=address_of(node->next);words[2]=address_of(node->previous);
    words[3]=node->kind;words[4]=node->target;words[5]=node->target_identity;
    memcpy(words+6u,node->position,12u);
    words[9]=node->parameter;words[10]=node->sequence;words[11]=node->flags;
}
static int node_state_read(void *context,uint32_t offset,void *bytes,size_t size){
    RuntimeNode *node=context;uint32_t words[12];
    if(!node->live || offset>=48u || size>48u-offset)return 0;
    node_words(node,words);memcpy(bytes,(uint8_t *)words+offset,size);return 1;
}
static int node_state_write(void *context,uint32_t offset,const void *bytes,size_t size){
    RuntimeNode *node=context,*next,*previous;uint32_t words[12];
    if(!node->live || offset>=48u || size>48u-offset)return 0;
    node_words(node,words);memcpy((uint8_t *)words+offset,bytes,size);
    next=runtime_node_pool_find(words[1]);previous=runtime_node_pool_find(words[2]);
    if((words[1] && !next) || (words[2] && !previous) || words[0]!=0x5749b0u)return 0;
    node->next=next;node->previous=previous;node->kind=words[3];node->target=words[4];
    node->target_identity=words[5];memcpy(node->position,words+6u,12u);
    node->parameter=words[9];node->sequence=words[10];node->flags=words[11];return 1;
}
static int block_state_read(void *context,uint32_t offset,void *bytes,size_t size){
    NodeBlock *block=context;
    if(offset>=48u*NODE_COUNT)return 0;
    return node_state_read(block->nodes+offset/48u,offset%48u,bytes,size);
}
static int block_state_write(void *context,uint32_t offset,const void *bytes,size_t size){
    NodeBlock *block=context;
    if(offset>=48u*NODE_COUNT)return 0;
    return node_state_write(block->nodes+offset/48u,offset%48u,bytes,size);
}
int runtime_node_pool_projection_read(GameAddress address,void *bytes,size_t size){
    uint32_t offset,words[12];RuntimeNode *node;
    if(!bytes)return 0;
    if(pool.imported && address>=HEADER && address-HEADER<HEADER_SIZE){
        offset=address-HEADER;if(size>HEADER_SIZE-offset)return 0;
        header_words(words);memcpy(bytes,(uint8_t *)words+offset,size);return 1;
    }
    node=node_at(address,&offset);if(!node || size>48u-offset)return 0;
    node_words(node,words);memcpy(bytes,(uint8_t *)words+offset,size);return 1;
}
int runtime_node_pool_projection_write(GameAddress address,const void *bytes,size_t size){
    uint32_t offset,words[12];RuntimeNode *node;
    if(!bytes)return 0;
    if(pool.imported && address>=HEADER && address-HEADER<HEADER_SIZE){
        RuntimeNode *old=pool.head,*next;
        offset=address-HEADER;if(size>HEADER_SIZE-offset)return 0;
        header_words(words);memcpy((uint8_t *)words+offset,bytes,size);
        next=runtime_node_pool_find(words[0]);if(words[0] && !next)return 0;
        if(next!=old){
            if(old && next==old->next)old->in_pool=0u;
            else {RuntimeNode *cursor=next;uint32_t visits=0u;
                while(cursor && cursor!=old && visits++<stats.live){cursor->in_pool=1u;cursor=cursor->next;}}
        }
        pool.head=next;pool.active=words[1];pool.peak=words[2];pool.initialized=words[3];
        memcpy(pool.position,words+4u,12u);return 1;
    }
    node=node_at(address,&offset);if(!node || size>48u-offset)return 0;
    return node_state_write(node,offset,bytes,size);
}
int runtime_node_pool_shutdown(GameMemory *memory){
    RuntimeNode *cursor,*next;NodeBlock **block_cursor;size_t i,kept=0u;uint32_t visits=0u;
    if(!memory || memory->fault)return -1;
    if(!pool.imported)return 0;
    for(cursor=pool.head;cursor;cursor=cursor->next){
        uint32_t base,size;
        if(!cursor->live || ++visits>stats.live ||
           !scene_projection_storage_describe(cursor->projection,&base,&size,NULL) ||
           size!=48u*NODE_COUNT || cursor->projection<base ||
           cursor->projection-base>=size || (cursor->projection-base)%48u!=0u){memory->fault=1;return -1;}
    }
    cursor=pool.head;pool.head=NULL;pool.initialized=0u;
    last_node=NULL;
    while(cursor){
        next=cursor->next;
        cursor->live=0u;++stats.released;--stats.live;cursor=next;
    }
    for(i=0u;i<index_count;i++)if(index_nodes[i]->live)index_nodes[kept++]=index_nodes[i];
    index_count=kept;if(!kept){free(index_nodes);index_nodes=NULL;}
    block_cursor=&blocks;
    while(*block_cursor){
        NodeBlock *block=*block_cursor;int live=0;
        for(i=0u;i<NODE_COUNT;i++)if(block->nodes[i].live){live=1;break;}
        if(live)block_cursor=&block->next;else {
            projection_owner_pages_change(block->projection,48u*NODE_COUNT,0);
            if(!scene_projection_storage_free(block->projection)){memory->fault=1;return -1;}
            *block_cursor=block->next;free(block);
        }
    }
    if(!index_count){
        if(!projection_owner_pages_change(HEADER,HEADER_SIZE,0)){memory->fault=1;return -1;}
        pool.imported=0;
        /*Ownership returns to the level's bounded globals only after the last
         *native node retires. Preserve their shared command-position value. */
        game_memory_write(memory,HEADER+16u,pool.position,12u);
    }
    return publish_header(memory)?1:-1;
}
