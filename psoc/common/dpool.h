#pragma once
#include <stdint.h>
#include <stddef.h>

#define POOL_MIN_BLOCK_SIZE 4

void pool_init(uint8_t *mem, size_t size);
// Set all memory to zero after initializing.
void pool_zeros();

void *pool_alloc(size_t size);
void pool_free(void *x);
