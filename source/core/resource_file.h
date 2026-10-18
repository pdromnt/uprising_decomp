#ifndef UPRISING_RESOURCE_FILE_H
#define UPRISING_RESOURCE_FILE_H
#include "resource_archive.h"
typedef struct ResourceFile ResourceFile;
/* Binary read-only host file. The returned object owns the open stream. */
ResourceFile *resource_file_open(const char *path);
ResourceFile *resource_file_open_mode(const char *path, const char *mode);
void resource_file_close(ResourceFile *file);
size_t resource_file_read(ResourceFile *file, void *output, size_t bytes);
size_t resource_file_write(ResourceFile *file, const void *input, size_t bytes);
int resource_file_flush(ResourceFile *file);
int resource_file_seek(ResourceFile *file, int32_t offset, int origin);
int resource_file_tell(ResourceFile *file, uint32_t *position);
/* Returns bytes including the terminator, or zero on immediate EOF/error.
 * Positive capacity is required; capacity one writes an empty string. */
size_t resource_file_line(ResourceFile *file, char *output, size_t capacity);
ResourceArchive *resource_file_archive(ResourceFile *file);
#endif
