/*
    Circular buffer.
*/
#pragma once
#include <stdint.h>

typedef struct CBufDef *CBuf;
struct CBufDef
{
    uint8_t *buf;
    uint32_t capacity;

    uint32_t size;
    uint8_t *end;
    uint8_t *write_ptr;
    uint8_t *read_ptr;
};

CBuf cbuf_create(uint32_t capacity);

uint32_t cbuf_write(CBuf self, const void *src, uint32_t size);
uint32_t cbuf_read(CBuf self, void *dst, uint32_t size);

uint8_t *cbuf_buf(CBuf self);
uint32_t cbuf_size(CBuf self);
