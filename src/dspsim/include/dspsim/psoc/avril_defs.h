#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum AvrilCommand
    {
        AvrilNop,
        AvrilWrite,
        AvrilRead,
        AvrilNopAck,
        AvrilWriteAck,
        AvrilReadAck,
    } AvrilCommand;

    typedef enum AvrilMode
    {
        AvrilVmmi,
        AvrilBootload,
        AvrilVMeta,
    } AvrilMode;

#ifdef __cplusplus
}
#endif