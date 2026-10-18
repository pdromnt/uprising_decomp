#ifndef UPRISING_GAME_MATRIX_H
#define UPRISING_GAME_MATRIX_H

#include "game_memory.h"

void game_matrix_compose_values(float output[12], const float left[12],
    const float right[12]);
void game_matrix_transform_point_values(float output[3],
    const float matrix[12], const float point[3]);
void game_matrix_attachment_orientation(GameMemory *memory, float output[12],
    const uint16_t angles[3]);
int game_matrix_extract_angles(GameMemory *memory, const float matrix[12],
    uint16_t angles[3]);
int game_matrix_extract_heading_pitch(GameMemory *,const float matrix[12],uint16_t angles[2]);
/* Presentation transform policy: 0 prepends, 1 appends, others replace. */
void game_matrix_apply_presentation(float active[12],
    const float incoming[12], uint32_t policy);
int game_matrix_compose(GameMemory *memory, GameAddress output,
    GameAddress left, GameAddress right);
int game_matrix_transform_point(GameMemory *memory, GameAddress output,
    GameAddress matrix, GameAddress point);
int game_matrix_camera_projection(GameMemory *,GameAddress output,
    const float position[3],uint16_t roll,uint16_t pitch,uint16_t heading,float scale);
int game_matrix_camera_view(GameMemory *,GameAddress output,
    const float position[3],uint16_t roll,uint16_t pitch,uint16_t heading);

#endif
