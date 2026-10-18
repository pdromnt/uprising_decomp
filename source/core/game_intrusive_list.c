#include "game_intrusive_list.h"

GameListResult game_list_insert_after(GameMemory *memory, GameAddress list,
    GameAddress node, uint32_t next_offset, uint32_t previous_offset) {
    GameListResult result;
    GameAddress next;
    game_memory_write_u32(memory, list + previous_offset, node);
    next = game_memory_read_u32(memory, node + next_offset);
    game_memory_write_u32(memory, list + next_offset, next);
    next = game_memory_read_u32(memory, node + next_offset);
    if (!memory->fault && next != 0u)
        game_memory_write_u32(memory, next + previous_offset, list);
    if (!memory->fault) game_memory_write_u32(memory, node + next_offset, list);
    result.first = next;
    result.second = node;
    return result;
}

GameListResult game_list_remove_head(GameMemory *memory, GameAddress list,
    uint32_t head_offset, uint32_t next_offset, uint32_t previous_offset) {
    GameListResult result;
    GameAddress head = game_memory_read_u32(memory, list + head_offset);
    GameAddress next = game_memory_read_u32(memory, head + next_offset);
    if (!memory->fault) game_memory_write_u32(memory, list + head_offset, next);
    if (!memory->fault) {
        if (next != 0u) game_memory_write_u32(memory, next + previous_offset, 0u);
        else game_memory_write_u32(memory, list + previous_offset, 0u);
    }
    result.first = head;
    result.second = next;
    return result;
}

GameListResult game_list_detach_owner(GameMemory *memory, GameAddress manager,
    GameAddress node, uint32_t head_offset, uint32_t tail_offset,
    uint32_t previous_offset, uint32_t next_offset) {
    GameListResult result;
    GameAddress next = game_memory_read_u32(memory, node + next_offset);
    GameAddress previous = game_memory_read_u32(memory, node + previous_offset);
    if (!memory->fault) {
        if (next != 0u) game_memory_write_u32(memory, next + previous_offset, previous);
        else game_memory_write_u32(memory, manager + tail_offset, previous);
    }
    previous = game_memory_read_u32(memory, node + previous_offset);
    next = game_memory_read_u32(memory, node + next_offset);
    if (!memory->fault) {
        if (previous != 0u) game_memory_write_u32(memory, previous + next_offset, next);
        else game_memory_write_u32(memory, manager + head_offset, next);
    }
    if (!memory->fault) game_memory_write_u32(memory, node + next_offset, 0u);
    if (!memory->fault) game_memory_write_u32(memory, node + previous_offset, 0u);
    result.first = previous;
    result.second = node + previous_offset;
    return result;
}

GameListResult game_list_detach_node(GameMemory *memory, GameAddress node,
    uint32_t previous_offset, uint32_t next_offset) {
    GameListResult result;
    result.first = game_memory_read_u32(memory, node + previous_offset);
    result.second = game_memory_read_u32(memory, node + next_offset);
    if (!memory->fault && result.second != 0u)
        game_memory_write_u32(memory, result.second + previous_offset,
            result.first);
    if (!memory->fault && result.first != 0u)
        game_memory_write_u32(memory, result.first + next_offset,
            result.second);
    if (!memory->fault) game_memory_write_u32(memory, node + next_offset, 0u);
    if (!memory->fault) game_memory_write_u32(memory, node + previous_offset, 0u);
    return result;
}

GameOwnedListDetachResult game_list_detach_owned_node(GameMemory *memory,
    GameAddress node, uint32_t owner_offset, uint32_t previous_offset,
    uint32_t next_offset, uint32_t owner_head_offset) {
    GameOwnedListDetachResult result;
    result.next = game_memory_read_u32(memory, node + next_offset);
    result.previous = game_memory_read_u32(memory, node + previous_offset);
    result.owner = game_memory_read_u32(memory, node + owner_offset);
    result.owner_head = 0u;
    if (!memory->fault && result.next != 0u)
        game_memory_write_u32(memory, result.next + previous_offset,
            result.previous);
    if (!memory->fault && result.previous != 0u)
        game_memory_write_u32(memory, result.previous + next_offset,
            result.next);
    if (!memory->fault && result.owner != 0u) {
        result.owner_head = game_memory_read_u32(memory,
            result.owner + owner_head_offset);
        if (!memory->fault && result.owner_head == node)
            game_memory_write_u32(memory, result.owner + owner_head_offset,
                result.previous);
    }
    if (!memory->fault) game_memory_write_u32(memory, node + owner_offset, 0u);
    if (!memory->fault) game_memory_write_u32(memory, node + previous_offset, 0u);
    if (!memory->fault) game_memory_write_u32(memory, node + next_offset, 0u);
    return result;
}

GameAddress game_list_queue_detach_head(GameMemory *memory,
        GameAddress list) {
    GameAddress head = game_memory_read_u32(memory, list);
    GameAddress next;
    if (memory->fault || head == 0u) return head;
    next = game_memory_read_u32(memory, head + 4u);
    if (!memory->fault) game_memory_write_u32(memory, list, next);
    return head;
}

GameListResult game_list_queue_pop(GameMemory *memory, GameAddress list) {
    GameListResult result;
    result.first = game_memory_read_u32(memory, list);
    result.second = result.first == 0u ? 0u :
        game_memory_read_u32(memory, result.first + 4u);
    if (!memory->fault && result.first != 0u) {
        game_memory_write_u32(memory, list, result.second);
        if (!memory->fault) {
            if (result.second == 0u)
                game_memory_write_u32(memory, list + 4u, 0u);
            else
                game_memory_write_u32(memory, result.second, 0u);
        }
    }
    return result;
}

void game_list_queue_prepend(GameMemory *memory, GameAddress list,
        GameAddress node, GameAddress payload) {
    GameAddress old = game_memory_read_u32(memory, list);
    if (memory->fault) return;
    game_memory_write_u32(memory, node + 8u, payload);
    if (!memory->fault) {
        if (old == 0u) game_memory_write_u32(memory, list + 4u, node);
        else game_memory_write_u32(memory, old, node);
    }
    if (!memory->fault) game_memory_write_u32(memory, node + 4u, old);
    if (!memory->fault) game_memory_write_u32(memory, list, node);
    if (!memory->fault) game_memory_write_u32(memory, node, 0u);
}

GameLinkedDetachResult game_list_detach_linked(GameMemory *memory,
        GameAddress list,
        GameAddress node) {
    GameLinkedDetachResult result;
    result.previous = game_memory_read_u32(memory, node);
    result.next = game_memory_read_u32(memory, node + 4u);
    result.head = game_memory_read_u32(memory, list);
    result.tail = game_memory_read_u32(memory, list + 4u);
    if (!memory->fault && result.next != 0u)
        game_memory_write_u32(memory, result.next, result.previous);
    if (!memory->fault && result.previous != 0u)
        game_memory_write_u32(memory, result.previous + 4u, result.next);
    if (!memory->fault && result.head == node)
        game_memory_write_u32(memory, list, result.next);
    if (!memory->fault && result.tail == node)
        game_memory_write_u32(memory, list + 4u, result.previous);
    return result;
}

GameAddress game_list_append_runtime_node(GameMemory *memory,
        GameAddress node, GameAddress previous_tail) {
    GameAddress tail;
    if (previous_tail != 0u)
        game_memory_write_u32(memory, previous_tail + 8u, node);
    if (memory->fault) return 0u;
    tail = game_memory_read_u32(memory, 0x00585374u);
    if (memory->fault) return 0u;
    game_memory_write_u32(memory, node + 4u, tail);
    if (!memory->fault)
        game_memory_write_u32(memory, 0x00585374u, node);
    if (!memory->fault)
        game_memory_write_u32(memory, node + 8u, 0u);
    return tail;
}

GameListCallbackItem game_list_take_callback(GameMemory *memory,
        GameAddress current, GameAddress cursor_slot) {
    GameListCallbackItem item = {0u, 0u, 0u, 0u};
    item.next = game_memory_read_u32(memory, current + 4u);
    if (memory->fault) return item;
    item.record = game_memory_read_u32(memory, current + 8u);
    if (memory->fault) return item;
    game_memory_write_u32(memory, cursor_slot, item.next);
    if (memory->fault) return item;
    item.callback = game_memory_read_u32(memory, item.record + 4u);
    if (memory->fault) return item;
    item.argument = game_memory_read_u32(memory, item.record + 0x0cu);
    return item;
}
