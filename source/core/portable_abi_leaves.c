#include "portable_abi_leaves.h"

uint32_t portable_abi_constant_u32(uint32_t value) { return value; }

uint32_t portable_abi_replace_low_u16(uint32_t original, uint16_t value) {
    return (original & 0xffff0000u) | (uint32_t)value;
}

uint32_t portable_abi_stack_cleanup(uint32_t bytes) { return bytes; }

void portable_abi_noop(void) {}
