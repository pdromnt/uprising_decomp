#ifndef UPRISING_NATIVE_PORTABLE_ABI_LEAVES_H
#define UPRISING_NATIVE_PORTABLE_ABI_LEAVES_H

#include <stdint.h>

uint32_t portable_abi_constant_u32(uint32_t value);
uint32_t portable_abi_replace_low_u16(uint32_t original, uint16_t value);
uint32_t portable_abi_stack_cleanup(uint32_t bytes);
void portable_abi_noop(void);

#endif
