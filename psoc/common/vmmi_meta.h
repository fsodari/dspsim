
/*

The metadata class can expose information about the registered interfaces. Then the host
controller can know where interfaces have been assigned.

Each metadata entry contains this information:
virtual_base_address, interface_size, interface_name

The metadata class also includes global registers indicating the name buffer size,
the interface version, the total interface size, and (?)

Entry size is 4 + 4 + 16[Name buf size] = 24
Memory layout:
0: {inteface_version, name_buf_size, n_interfaces}
4: vmmi_memory_size
8: entry0
32: entry1
...

abcdefghijklmnop
*/
#pragma once
#include "vmmi.h"

typedef struct VMMIMeta
{
    // Implemented as an mmi.
    MMI base;

    VMMI *_vmmi_ref; // Reference to the vmmi

    uint8_t _name_buffer_size;
    uint8_t _interface_version;
    uint16_t _n_interfaces;

} VMMIMeta;

// Reserve a virtual address space block so this can be used as the address 0 interface of the vmmi.
void vmmi_meta_init(VMMIMeta *self, VMMI *vmmi, uint32_t reserve_size);