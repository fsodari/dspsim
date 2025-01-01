/*

    Virtual memory mapped interface.

    Map mmis to a virtual address space.
*/
#pragma once
#include "dspsim/psoc/mmi.h"

#define VMMI_NAME_BUF_SIZE 16
typedef struct VMMITableEntryDef *VMMITableEntry;
struct VMMITableEntryDef
{
    uint32_t base_address;
    uint32_t size;
    uint32_t dtype;
    char name[VMMI_NAME_BUF_SIZE];
    MMI mmi;
};

typedef struct VMMIDef *VMMI;
struct VMMIDef
{
    // Inherit from mmi. This class is used as an mmi.
    struct MMIDef base;

    // Table of registered interfaces.
    VMMITableEntry *itable;
    uint32_t itable_max_size;
    // Current size.
    uint32_t itable_size;

    uint32_t next_address;
};

VMMI vmmi_create(uint32_t max_interfaces);

/*
    Add an mmi to the interface.
    It will automatically be assigned to the next virtual address
*/
uint32_t vmmi_register(VMMI self, MMI iface, const char *name);

uint32_t vmmi_n_interfaces(VMMI self);
VMMITableEntry vmmi_get_entry(VMMI self, uint32_t id);