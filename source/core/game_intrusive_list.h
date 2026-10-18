#ifndef UPRISING_GAME_INTRUSIVE_LIST_H
#define UPRISING_GAME_INTRUSIVE_LIST_H

#include "game_memory.h"

typedef struct GameListResult {
    GameAddress first;
    GameAddress second;
} GameListResult;

typedef struct GameLinkedDetachResult {
    GameAddress previous;
    GameAddress next;
    GameAddress head;
    GameAddress tail;
} GameLinkedDetachResult;

GameListResult game_list_insert_after(GameMemory *, GameAddress list,
    GameAddress node, uint32_t next_offset, uint32_t previous_offset);
GameListResult game_list_remove_head(GameMemory *, GameAddress list,
    uint32_t head_offset, uint32_t next_offset, uint32_t previous_offset);
GameListResult game_list_detach_owner(GameMemory *, GameAddress manager,
    GameAddress node, uint32_t head_offset, uint32_t tail_offset,
    uint32_t previous_offset, uint32_t next_offset);
GameListResult game_list_detach_node(GameMemory *, GameAddress node,
    uint32_t previous_offset, uint32_t next_offset);

typedef struct GameOwnedListDetachResult {
    GameAddress previous;
    GameAddress next;
    GameAddress owner;
    GameAddress owner_head;
} GameOwnedListDetachResult;

GameOwnedListDetachResult game_list_detach_owned_node(GameMemory *,
    GameAddress node, uint32_t owner_offset, uint32_t previous_offset,
    uint32_t next_offset, uint32_t owner_head_offset);
GameListResult game_list_queue_pop(GameMemory *, GameAddress list);
GameAddress game_list_queue_detach_head(GameMemory *, GameAddress list);
void game_list_queue_prepend(GameMemory *, GameAddress list,
    GameAddress node, GameAddress payload);
GameLinkedDetachResult game_list_detach_linked(GameMemory *, GameAddress list,
    GameAddress node);
GameAddress game_list_append_runtime_node(GameMemory *, GameAddress node,
    GameAddress previous_tail);

typedef struct GameListCallbackItem {
    GameAddress next;
    GameAddress record;
    GameAddress callback;
    uint32_t argument;
} GameListCallbackItem;

GameListCallbackItem game_list_take_callback(GameMemory *,
    GameAddress current, GameAddress cursor_slot);

#endif
