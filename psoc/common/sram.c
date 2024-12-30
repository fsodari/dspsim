#include "FreeRTOS.h"
#include "dspsim/sram.h"
#include <string.h>

uint32_t sram_write(void *self, uint32_t address, const void *src, uint32_t amount);
uint32_t sram_read(void *self, uint32_t address, void *dst, uint32_t amount);

Sram sram_create(uint32_t size)
{
    Sram self = pvPortMalloc(sizeof(*self));
    self->buf = pvPortMalloc(size);
    self->size = size;
    mmi_init(self, sram_write, sram_read, size);

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
