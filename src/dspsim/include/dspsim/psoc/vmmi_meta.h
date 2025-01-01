
/*

The metadata class can expose information about the registered interfaces. Then the host
controller can know where interfaces have been assigned.

Each metadata entry contains this information:
base_address, interface_size, interface_name

The metadata class also includes global registers indicating the name buffer size,
the interface version, the total interface size, and (?)

Entry size is 4 + 4 + 16[Name buf size] = 24
Memory layout:
0: entry0
24: entry1
...

abcdefghijklmnop
*/
#pragma once
#include "dspsim/psoc/vmmi.h"

typedef struct VMMIMetaDef *VMMIMeta;
struct VMMIMetaDef
{
    // Implemented as an mmi.
    struct MMIDef base;

    VMMI vmmi_ref; // Reference to the vmmi
};

// Reserve a virtual address space block so this can be used as the address 0 interface of the vmmi.
VMMIMeta vmmi_meta_create(VMMI vmmi, uint32_t reserve_size);