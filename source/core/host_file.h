#ifndef UPRISING_HOST_FILE_H
#define UPRISING_HOST_FILE_H
#include <stdio.h>
#include <stddef.h>
/* Normalize retail separators; existing read paths also tolerate asset case.
 * Returns an exact host path without changing filenames on disk. */
int host_file_path(const char *,char *,size_t);
FILE *host_file_open(const char *,const char *mode);
#endif
