/*
    Adaptive Virtual Register Interface Library

    An abstract application of the vmmi/mmi and vmmi_meta interfaces.

    Defines a protocol for sending and receiving virtual mmi commands and acknowledging responses.
    Uses dependency injection to support various low-level protocols (serial, spi, testbench, ethernet(?))

    Protocol:
    Host: Command -> Avril
    Avril: Response -> Host

    Command Packet format is symmetric for commands and responses.
    Response msg_id will be the same as the command msg_id.

    {
        uint32_t command; // uint8_t command, uint8_t mode, uint16_t msg_id
        uint32_t size;    // Payload size.
        uint32_t address; // Register start address.
        uint8_t data[];   // 0 or more bytes of data.
    }

    Commands: nop, write, read, nop_ack, write_ack, read_ack
    Modes: standard, metadata

    NopCommand
    {
        uint32_t command = {AVRIL_CMD_NOP, 0, msg_id}
        uint32_t size = 0;
        uint32_t address = address;
    }
    NopResponse
    {
        uint32_t command = {AVRIL_CMD_NOP_ACK, 0, msg_id}
        uint32_t size = 0;
        uint32_t address = address;
    }
    WriteCommand
    {
        uint32_t command = {AVRIL_CMD_WRITE, 0, msg_id};
        uint32_t size = sizeof(data);
        uint32_t address = address;
        uint8_t data[] = ...;
    }
    WriteResponse
    {
        uint32_t command = {AVRIL_CMD_WRITE_ACK, 0, msg_id}
        uint32_t size = 4;
        uint32_t address = address;
        uint32_t error = error;
    }
    ReadCommand
    {
        uint32_t command = {AVRIL_CMD_READ, 0, msg_id}
        uint32_t size = 0;
        uint32_t address = address;
        // no data. no problem.
    }
    ReadResponse
    {
        uint32_t command = {AVRIL_READ_ACK, 0, msg_id}
        uint32_t size = data_size + 4;
        uint32_t address = address;
        uint32_t error = error;
        uint8_t data[] = data
    }

    When a packet arrives, call this function.
    uint32_t execute_command(Avril *av, uint8_t *buf, uint32_t size);

    Supply a callback to send packets to the physical interface.
    uint32_t send_response(uint8_t *buf, uint32_t size);


*/
#pragma once
#include "dspsim/psoc/mmi.h"

#include <stdint.h>

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

typedef struct AvrilDef *Avril;

Avril avril_start(uint32_t n_modes, uint32_t max_msg_size, uint32_t priority);
enum dErrorCodes avril_add_mode(Avril self, uint32_t mode_id, MMI mode_interface);
