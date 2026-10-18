#ifndef UPRISING_APPLICATION_ARCHIVES_H
#define UPRISING_APPLICATION_ARCHIVES_H
#include "application_entry.h"
/*Stable per-application file identities; the platform owns full-width
 * ResourceFile pointers. Borrowed files expire at close/application shutdown. */
GameAddress game_application_archive_open(GameApplication *,const char *,int write);
ResourceFile *game_application_archive_file(GameApplication *,GameAddress);
int game_application_archive_close(GameApplication *,GameAddress);
int game_application_file_path(GameApplication *,const char *,int write,char *,size_t);
int game_application_file_delete(GameApplication *,const char *);
int game_application_file_copy(GameApplication *,const char *,const char *);
#endif
