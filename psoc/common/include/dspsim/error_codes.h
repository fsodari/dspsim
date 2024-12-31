/*

    Global set of error codes used by all modules in the dspsim universe.

*/
#pragma once

typedef enum dErrorCodes
{
    dERR_NONE = 0,        // No error.
    dERR_OVERFLOW,        // The write/read command would overflow the interface's address space
    dERR_INVALID_ADDRESS, // The address was not found within the mmi's address space.
    dERR_READ_ONLY,       // The interface does not support write commands.
    dERR_WRITE_ONLY,      // The interface does not support read commands.
    dERR_ADDR_ALIGN2,     // Data must have 16 bit alignment.
    dERR_ADDR_ALIGN4,     // Data must have 32 bit alignment
    dERR_ADDR_ALIGN8,     // Data must have 64 bit alignment
    dERR_ADDR_ALIGNN,     // Data must hav N byte alignment. Application specific.
    dERR_SIZE_ALIGN2,
    dERR_SIZE_ALIGN4,
    dERR_SIZE_ALIGN8,
    dERR_SIZE_ALIGNN,

} dErrorCodes;
