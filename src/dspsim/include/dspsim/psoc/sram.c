#include "FreeRTOS.h"
#include "dspsim/psoc/sram.h"
#include <string.h>

uint32_t sram_write(void *self, uint32_t address, const void *src, uint32_t amount);
uint32_t sram_read(void *self, uint32_t address, void *dst, uint32_t amount);

Sram sram_create(uint32_t size, int32_t dtype)
{
    Sram self = pvPortMalloc(sizeof(*self));
    self->buf = pvPortMalloc(size);
    self->size = size;
    mmi_init((MMI)self, sram_write, sram_read, size, dtype);

    return self;
}

uint32_t sram_write(void *_self, uint32_t address, const void *src, uint32_t amount)
{
    Sram self = (Sram)_self;
    uint32_t error = 0;

    memcpy(&self->buf[address], src, amount);

    return error;
}

uint32_t sram_read(void *_self, uint32_t address, void *dst, uint32_t amount)
{
    Sram self = (Sram)_self;
    uint32_t error = 0;

    memcpy(dst, &self->buf[address], amount);

    return error;
}
