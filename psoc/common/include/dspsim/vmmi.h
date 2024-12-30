/*

    Virtual memory mapped interface.

    Map mmis to a virtual address space.
*/
#pragma once
#include "dspsim/mmi.h"

#define VMMI_NAME_BUF_SIZE 16

typedef struct VMMITableEntry
{
    MMI *mmi;
    const char name[VMMI_NAME_BUF_SIZE];
} VMMITableEntry;

typedef struct VMMI
{
    // Inherit from mmi. This class is used as an mmi.
    MMI base;

    MMI *_itable;
    uint32_t _itable_max_size;

    // Current size.
    uint32_t _itable_size;
} VMMI;

void vmmi_init_static(VMMI *self, MMI *itable, uint32_t itable_max_size);
void vmmi_init_alloc(VMMI *self, uint32_t max_size);

VMMI *vmmi_create(uint32_t max_size);

/*
    Add an mmi to the interface.
    It will automatically be assigned to the next virtual address
*/
uint32_t vmmi_register(VMMI *self, MMI *iface, const char *name);
