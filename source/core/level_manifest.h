#ifndef UPRISING_LEVEL_MANIFEST_H
#define UPRISING_LEVEL_MANIFEST_H
#include <stdint.h>
typedef struct LevelManifest {
    float slick;
    int32_t terrain_offset;
    int32_t slots_offset;
} LevelManifest;
/* Read the three leading directives from a level .slk file. */
int level_manifest_read(const char *path,LevelManifest *output);
#endif
