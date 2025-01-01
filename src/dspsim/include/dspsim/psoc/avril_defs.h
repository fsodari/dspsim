#pragma once

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
