#include "FreeRTOS.h"
#include "dspsim/psoc/vmmi.h"
#include "dspsim/psoc/error_codes.h"
#include <string.h>

uint32_t vmmi_write(void *_self, uint32_t address, const void *src, uint32_t size);
uint32_t vmmi_read(void *_self, uint32_t address, void *dst, uint32_t size);

uint32_t vmmi_find_interface(VMMI self, uint32_t virtual_address, MMI *found, uint32_t *relative_address);

VMMI vmmi_create(uint32_t max_interfaces)
{
    VMMI self = pvPortMalloc(sizeof(*self));

    self->itable = pvPortMalloc(max_interfaces * sizeof(*self->itable));
    self->itable_max_size = max_interfaces;
    self->itable_size = 0;
    self->next_address = 0;

    mmi_init((MMI)self, vmmi_write, vmmi_read, 0, dX);

    return self;
}

/*
    Add an mmi to the interface.
    It will automatically be assigned to the next virtual address
*/
uint32_t vmmi_register(VMMI self, MMI iface, const char *name)
{
    uint32_t error = dErrNone;

    // Create table entry.
    VMMITableEntry entry = pvPortMalloc(sizeof(*entry));

    entry->base_address = self->next_address;
    entry->size = iface->size;
    entry->dtype = iface->dtype;
    strncpy(entry->name, name, VMMI_NAME_BUF_SIZE);
    entry->mmi = iface;

    // Increment next address and mmi size
    self->next_address += iface->size;
    self->base.size += iface->size;

    // Update the table
    self->itable[self->itable_size] = entry;
    self->itable_size++;

    return error;
}

uint32_t vmmi_n_interfaces(VMMI self)
{
    return self->itable_size;
}

VMMITableEntry vmmi_get_entry(VMMI self, uint32_t id)
{
    return self->itable[id];
}

uint32_t vmmi_write(void *_self, uint32_t address, const void *src, uint32_t size)
{
    VMMI self = _self;
    uint32_t error = dErrNone;

    MMI iface;
    uint32_t relative_address;

    // Find the interface and relative address.
    error = vmmi_find_interface(self, address, &iface, &relative_address);
    if (error != dErrNone)
    {
        return error;
    }

    // Check for overflow
    error = mmi_check_overflow(iface, relative_address, size);
    if (error != dErrNone)
    {
        return error;
    }

    // execute the command.
    error = mmi_write(iface, relative_address, src, size);

    return error;
}

uint32_t vmmi_read(void *_self, uint32_t address, void *dst, uint32_t size)
{
    VMMI self = _self;
    uint32_t error = dErrNone;

    MMI iface;
    uint32_t relative_address;

    // Find the interface and relative address.
    error = vmmi_find_interface(self, address, &iface, &relative_address);
    if (error != dErrNone)
    {
        return error;
    }
    error = mmi_check_overflow(iface, relative_address, size);
    if (error != dErrNone)
    {
        return error;
    }

    // execute the command.
    error = mmi_read(iface, relative_address, dst, size);

    return error;
}

uint32_t vmmi_find_interface(VMMI self, uint32_t virtual_address, MMI *found, uint32_t *relative_address)
{
    uint32_t error = dErrInvalidAddress;
    *found = NULL;
    *relative_address = UINT32_MAX;

    for (uint32_t i = 0; i < self->itable_size; i++)
    {
        VMMITableEntry entry = self->itable[i];
        uint32_t end_address = entry->base_address + entry->mmi->size;
        if (virtual_address >= entry->base_address && virtual_address < end_address)
        {
            *found = entry->mmi;
            *relative_address = virtual_address - entry->base_address;
            error = dErrNone;
            break;
        }
    }

    return error;
}