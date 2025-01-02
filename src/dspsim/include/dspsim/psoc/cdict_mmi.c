#include "FreeRTOS.h"
#include "dspsim/psoc/cdict_mmi.h"

uint32_t dict_mmi_write(void *_self, uint32_t address, const void *src, uint32_t size);
uint32_t dict_mmi_read(void *_self, uint32_t address, void *dst, uint32_t size);

DictMMI dict_mmi_create(uint32_t n_bins, uint32_t n_regs, DType dtype)
{
    DictMMI self = pvPortMalloc(sizeof(*self));

    self->dict = dict_create(n_bins, hash_32);
    
    mmi_init((MMI)self, dict_mmi_write, dict_mmi_read, n_regs * dtype_size(dtype), dtype);
    
    return self;
}

uint32_t dict_mmi_write(void *_self, uint32_t _address, const void *_src, uint32_t size)
{
    DictMMI self = _self;
    const uint8_t *src = _src;

    uint32_t dsize = dtype_size(self->base.dtype);
    // Require register alignment.
    uint32_t error = mmi_check_align(_address, size, dsize);
    if (error != dErrNone)
    {
        return error;
    }

    //
    uint32_t end_address = _address + size;
    for (uint32_t addr = _address; addr < end_address; addr += dsize)
    {
        error = dict_set(self->dict, &addr, src, dsize);
        src += dsize;
        if (error != dErrNone)
        {
            return error;
        }
    }

    return error;
}

uint32_t dict_mmi_read(void *_self, uint32_t _address, void *_dst, uint32_t size)
{
    DictMMI self = _self;
    uint8_t *dst = _dst;

    uint32_t dsize = dtype_size(self->base.dtype);
    // Require register alignment.
    uint32_t error = mmi_check_align(_address, size, dsize);
    if (error != dErrNone)
    {
        return error;
    }

    //
    uint32_t end_address = _address + size;
    for (uint32_t addr = _address; addr < end_address; addr += dsize)
    {
        error = dict_read(self->dict, &addr, dst, dsize);
        dst += dsize;
        if (error != dErrNone)
        {
            return error;
        }
    }

    return error;
}
