#ifndef UPRISING_PORTABLE_HOOKS_H
#define UPRISING_PORTABLE_HOOKS_H
#include <windows.h>
#include "application_entry.h"
void game_application_hooks_bind(GameApplication *);
LRESULT CALLBACK game_application_message_hook(int,WPARAM,LPARAM);
LRESULT CALLBACK game_application_keyboard_hook(int,WPARAM,LPARAM);
#endif