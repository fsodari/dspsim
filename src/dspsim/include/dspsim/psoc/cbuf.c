#include "dspsim/psoc/cbuf.h"
#include "FreeRTOS.h"
#include <string.h>

CBuf cbuf_create(uint32_t capacity)
{
    CBuf self = pvPortMalloc(sizeof(*self));
    self->buf = pvPortMalloc(capacity);
    self->capacity = capacity;
    self->size = 0;

    self->end = self->buf + capacity;
    self->write_ptr = self->buf;
    self->read_ptr = self->buf;

    return self;
}

uint32_t cbuf_write(CBuf self, const void *_src, uint32_t size)
{
    const uint8_t *src = _src;
    uint32_t remaining = self->end - self->write_ptr;
    uint32_t offset = 0;

    if (remaining <= size)
    {
        memcpy(self->write_ptr, src + offset, remaining);
        self->write_ptr = self->buf;

        self->size += remaining;
        size -= remaining;
        offset += remaining;
    }
    memcpy(self->write_ptr, src + offset, size);
    self->write_ptr += size;
    self->size += size;
    
    return 0;
}

uint32_t cbuf_read(CBuf self, void *_dst, uint32_t size)
{
    uint8_t *dst = _dst;
    uint32_t remaining = self->end - self->read_ptr;
    uint32_t offset = 0;
    if (remaining <= size)
    {
        memcpy(dst + offset, self->read_ptr, remaining);
        self->read_ptr = self->buf;
        self->size -= remaining;
        size -= remaining;
        offset += remaining;
    }
    memcpy(dst + offset, self->read_ptr, size);
    self->read_ptr += size;
    self->size -= remaining;
    return 0;
}

uint8_t *cbuf_buf(CBuf self)
{
    return self->buf;
}

uint32_t cbuf_size(CBuf self)
{
    return self->size;
}
