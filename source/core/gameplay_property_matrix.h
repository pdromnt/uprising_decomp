#ifndef UPRISING_GAMEPLAY_PROPERTY_MATRIX_H
#define UPRISING_GAMEPLAY_PROPERTY_MATRIX_H
#include <stddef.h>
#include <stdint.h>
/* 18 types by 13 properties; first row/column are unity. */
int gameplay_property_matrix_parse(const uint8_t *,size_t,float output[234]);
#endif