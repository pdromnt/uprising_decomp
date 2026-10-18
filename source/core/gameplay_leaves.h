#ifndef UPRISING_NATIVE_GAMEPLAY_LEAVES_H
#define UPRISING_NATIVE_GAMEPLAY_LEAVES_H

#include "game_memory.h"

#include <stdint.h>

/* Source-level result for scalar/gameplay leaf entry points. The
   dispatcher owns the retail register/return ABI; this API owns behavior. */
typedef struct GameLeafBatchResult {
    uint32_t eax;
    uint32_t cleanup;
    int writes_eax;
} GameLeafBatchResult;

GameLeafBatchResult game_leaf_batch(GameMemory *memory, GameAddress entry,
    GameAddress self, uint32_t eax, uint32_t stack_argument);

void game_leaf_0044ed50(GameMemory *memory);
void game_leaf_004d94d0(GameMemory *memory);
void game_leaf_0042f110(GameMemory *memory, GameAddress self);
uint32_t game_leaf_004cc1c0(void);
uint32_t game_leaf_004f9b90(GameMemory *memory, GameAddress self);
GameAddress game_leaf_identity(GameAddress self);
void game_leaf_004765f0(GameMemory *memory, GameAddress self);
void game_leaf_0043b4c0(GameMemory *memory, GameAddress self);
GameAddress game_leaf_0048ef80(GameMemory *memory, GameAddress self);
uint32_t game_leaf_00416c10(GameMemory *memory, GameAddress self,
    uint32_t value);
uint32_t game_leaf_00416c20(GameMemory *memory, GameAddress self);
uint32_t game_leaf_00479024(GameMemory *memory, GameAddress self);
void game_leaf_00502e00(GameMemory *memory);
uint32_t game_leaf_0046ff30(void);
void game_leaf_00441420(GameMemory *memory);
uint32_t game_leaf_004073d0(void);
uint32_t game_leaf_004073e0(void);
uint32_t game_leaf_00454e90(GameMemory *memory, GameAddress self);
void game_leaf_0045f570(GameMemory *memory);
uint32_t game_leaf_clear_s16x3(GameMemory *memory, GameAddress object);
void game_leaf_copy_u32x3(GameMemory *memory, GameAddress destination,
    GameAddress source);
GameAddress game_leaf_find_nonzero_link(GameMemory *memory,
    GameAddress object);
typedef struct GameLeafDivisionResult {
    int32_t quotient;
    int32_t remainder;
} GameLeafDivisionResult;
GameLeafDivisionResult game_leaf_attenuate_s16x3(GameMemory *memory,
    GameAddress object, int32_t divisor);
long double game_leaf_horizontal_distance_squared(GameMemory *memory,
    GameAddress point, GameAddress reference);
uint32_t game_terrain_cell_presence(GameMemory *memory, GameAddress first,
    GameAddress second);
/* Impact scan and shared wrapped terrain-light marking. The level's cells
 * remain authoritative; bounded legacy access supports original comparisons. */
void game_terrain_mark_area(GameMemory *,float x,float z,float width,float height);
int game_terrain_deform(GameMemory *,const float position[3],float strength,int32_t cell_radius);
void game_terrain_impact(GameMemory *,float x,float z,int32_t extent);

#endif
