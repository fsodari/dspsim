#include "FreeRTOS.h"
#include "dspsim/psoc/vmmi_meta.h"
#include "dspsim/psoc/error_codes.h"
#include <string.h>

#define VMMI_META_ENTRY_SIZE (12 + VMMI_NAME_BUF_SIZE)

uint32_t vmmi_meta_read(void *_self, uint32_t address, void *dst, uint32_t size);

// Reserve a virtual address space block so this can be used as the address 0 interface of the vmmi.
VMMIMeta vmmi_meta_create(VMMI vmmi, uint32_t reserve_size)
{
    VMMIMeta self = pvPortMalloc(sizeof(*self));

    self->vmmi_ref = vmmi;

    // Read-only.
    mmi_init((MMI)self, mmi_fread_only_err, vmmi_meta_read, reserve_size, dX);

    return self;
}

uint32_t vmmi_meta_read(void *_self, uint32_t address, void *dst, uint32_t size)
{
    VMMIMeta self = _self;
    uint32_t error = dErrNone;

    // The virtual size of the table. Entries are read out lazily from the vmmi, so this class doesn't take up any extra space.
    uint32_t meta_size = VMMI_META_ENTRY_SIZE * self->vmmi_ref->itable_size;

    // Check overflow.
    error = mmi_check_overflow_size(address, size, meta_size);
    if (error != dErrNone)
    {
        return error;
    }
    // Reads must start at entry boundaries and contain entire entries.
    error = mmi_check_align(address, size, VMMI_META_ENTRY_SIZE);
    if (error != dErrNone)
    {
        return error;
    }

    // Find the entry range.
    uint32_t start_entry = address / VMMI_META_ENTRY_SIZE;
    uint32_t end_entry = start_entry + (size / VMMI_META_ENTRY_SIZE);
    uint32_t n_entries = end_entry - start_entry;

    uint8_t *dbuf = dst;
    for (uint32_t i = 0; i < n_entries; i++)
    {
        VMMITableEntry entry = vmmi_get_entry(self->vmmi_ref, i + start_entry);
        memcpy(dbuf + i * VMMI_META_ENTRY_SIZE, entry, VMMI_META_ENTRY_SIZE);
    }

    return error;
}