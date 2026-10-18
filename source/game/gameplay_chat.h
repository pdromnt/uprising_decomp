#ifndef UPRISING_PORTABLE_GAMEPLAY_CHAT_H
#define UPRISING_PORTABLE_GAMEPLAY_CHAT_H
#include "gameplay_input.h"
int game_level_scene_audio_initialize(GameApplication *); /*5390a0: text history*/
int game_level_chat_release(GameApplication *);
int game_chat_line_append(GameApplication *,const char *);
int game_chat_line_replace(GameApplication *,const char *);
int game_input_chat_begin(GameApplication *);
int game_input_chat_prompt(GameApplication *);
int game_level_chat_frame(GameApplication *); /*539270*/
int game_input_chat_keyboard(GameApplication *,const GameInputEvent *);
#endif
