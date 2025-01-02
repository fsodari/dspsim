/*

    Global set of error codes used by all modules in the dspsim universe.

*/
#pragma once

typedef enum dErrorCodes
{
    dErrNone = 0, // No error.
    dErrOverflow, // The write/read command would overflow the interface's address space
    dErrInvalidMode,
    dErrInvalidCommand,
    dErrInvalidAddress, // The address was not found within the mmi's address space.
    dErrReadOnly,       // The interface does not support write commands.
    dErrWriteOnly,      // The interface does not support read commands.
    dErrAddrAlign16,    // Data must have 16 bit alignment.
    dErrAddrAlign32,    // Data must have 32 bit alignment
    dErrAddrAlign64,    // Data must have 64 bit alignment
    dErrAddrAlignN,     // Data must hav N byte alignment. Application specific.
    dErrSizeAlign16,
    dErrSizeAlign32,
    dErrSizeAlign64,
    dErrSizeAlignN,
    dErrKeyNotFound,

}dErrorCodes;
