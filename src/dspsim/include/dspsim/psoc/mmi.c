#include "dspsim/psoc/mmi.h"
#include "dspsim/psoc/mmi_iter.h"
#include <stdlib.h>

void mmi_init(MMI self, mmi_write_ft write, mmi_read_ft read, uint32_t size, DType dtype)
{
    self->write = write;
    self->read = read;
    self->size = size;
    self->dtype = dtype;
    self->next = miter_next_inc; // Default iter function. Override if you need to.
}

// Standardized functions that mmis can use.
uint32_t mmi_fread_only_err(void *self, uint32_t address, const void *src, uint32_t size)
{
    (void)self;
    (void)address;
    (void)src;
    (void)size;
    return dErrReadOnly;
}

uint32_t mmi_fwrite_only_err(void *self, uint32_t address, void *dst, uint32_t size)
{
    (void)self;
    (void)address;
    (void)dst;
    (void)size;
    return dErrWriteOnly;
}

uint32_t mmi_check_overflow(MMI self, uint32_t address, uint32_t size)
{
    return mmi_check_overflow_size(address, size, self->size);
}

uint32_t mmi_check_overflow_size(uint32_t address, uint32_t size, uint32_t interface_size)
{
    uint32_t error = address + size > interface_size ? dErrOverflow : dErrNone;
    return error;
}

uint32_t mmi_check_address_align(uint32_t address, uint32_t required_alignment)
{
    uint32_t error = dErrNone;
    if (address % required_alignment != 0)
    {
        switch (required_alignment)
        {
        case 2:
            error = dErrAddrAlign16;
            break;
        case 4:
            error = dErrAddrAlign32;
            break;
        case 8:
            error = dErrAddrAlign64;
            break;
        default:
            error = dErrAddrAlignN;
            break;
        }
    }
    return error;
}

uint32_t mmi_check_size_align(uint32_t size, uint32_t required_alignment)
{
    uint32_t error = dErrNone;
    if (size % required_alignment != 0)
    {
        switch (required_alignment)
        {
        case 2:
            error = dErrSizeAlign16;
            break;
        case 4:
            error = dErrSizeAlign32;
            break;
        case 8:
            error = dErrSizeAlign64;
            break;
        default:
            error = dErrSizeAlignN;
            break;
        }
    }
    return error;
}

uint32_t mmi_check_align(uint32_t address, uint32_t size, uint32_t required_alignment)
{
    uint32_t error = dErrNone;

    error = mmi_check_address_align(address, required_alignment);
    if (error != dErrNone)
    {
        return error;
    }
    error = mmi_check_size_align(size, required_alignment);
    return error;
}
