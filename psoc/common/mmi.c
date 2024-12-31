#include "dspsim/mmi.h"

void mmi_init(MMI self, mmi_write_ft write, mmi_read_ft read, uint32_t size, int32_t dtype)
{
    self->write = write;
    self->read = read;
    self->size = size;
    self->dtype = dtype;
}

// Standardized functions that mmis can use.
uint32_t mmi_fread_only_err(void *self, uint32_t address, const void *src, uint32_t size)
{
    return dERR_READ_ONLY;
}

uint32_t mmi_fwrite_only_err(void *self, uint32_t address, void *dst, uint32_t size)
{
    return dERR_WRITE_ONLY;
}

uint32_t mmi_check_overflow(MMI self, uint32_t address, uint32_t size)
{
    return mmi_check_overflow_size(address, size, self->size);
}

uint32_t mmi_check_overflow_size(uint32_t address, uint32_t size, uint32_t interface_size)
{
    uint32_t error = address + size > interface_size ? dERR_OVERFLOW : dERR_NONE;
    return error;
}

uint32_t mmi_check_address_align(uint32_t address, uint32_t required_alignment)
{
    uint32_t error = dERR_NONE;
    if (address % required_alignment != 0)
    {
        switch (required_alignment)
        {
        case 2:
            error = dERR_ADDR_ALIGN2;
            break;
        case 4:
            error = dERR_ADDR_ALIGN4;
            break;
        case 8:
            error = dERR_ADDR_ALIGN8;
            break;
        default:
            error = dERR_ADDR_ALIGNN;
            break;
        }
    }
    return error;
}

uint32_t mmi_check_size_align(uint32_t size, uint32_t required_alignment)
{
    uint32_t error = dERR_NONE;
    if (size % required_alignment != 0)
    {
        switch (required_alignment)
        {
        case 2:
            error = dERR_SIZE_ALIGN2;
            break;
        case 4:
            error = dERR_SIZE_ALIGN4;
            break;
        case 8:
            error = dERR_SIZE_ALIGN8;
            break;
        default:
            error = dERR_SIZE_ALIGNN;
            break;
        }
    }
    return error;
}

uint32_t mmi_check_align(uint32_t address, uint32_t size, uint32_t required_alignment)
{
    uint32_t error = dERR_NONE;

    error = mmi_check_address_align(address, required_alignment);
    if (error != dERR_NONE)
    {
        return error;
    }
    error = mmi_check_size_align(size, required_alignment);
    return error;
}
