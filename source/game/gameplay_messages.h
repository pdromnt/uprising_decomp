#ifndef UPRISING_PORTABLE_GAMEPLAY_MESSAGES_H
#define UPRISING_PORTABLE_GAMEPLAY_MESSAGES_H
#include "application_entry.h"
/*The existing0x544-byte runtime work record owns all queues/current text.
 *Arguments and returns are values; no callback emulates a guest ABI. */
int game_message_enqueue(GameApplication *,GameAddress work,GameAddress text,uint32_t category);
int game_message_enqueue_bytes(GameApplication *,GameAddress work,const char *,uint32_t category);
int game_message_dequeue(GameApplication *,GameAddress work,GameAddress output);
int game_level_messages_update(GameApplication *);
int game_input_message(GameApplication *,GameAddress text);
#endif
