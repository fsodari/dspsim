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
#include "dspsim/vmmi.h"
#include "dspsim/vmmi_meta.h"
#include "FreeRTOS.h"
#include "stream_buffer.h"
#include <stdint.h>

#define AVRIL_CMD_NOP 0
#define AVRIL_CMD_WRITE 1
#define AVRIL_CMD_READ 2
#define AVRIL_CMD_NOP_ACK 3
#define AVRIL_CMD_WRITE_ACK 4
#define AVRIL_CMD_READ_ACK 5

typedef uint32_t (*avril_write_cmd_cb)(uint32_t address, const void *src, uint32_t amount);
typedef uint32_t (*avril_read_cmd_cb)(uint32_t address, void *dst, uint32_t amount);

void avril_start(uint32_t n_modes, StreamBufferHandle_t tx_buffer, StreamBufferHandle_t rx_buffer);

void avril_add_mode(uint32_t mode_id, MMI *mode_interface);

// Callbacks for unused write/read commands.
uint32_t avril_unused_write_cb(uint32_t address, const void *src, uint32_t amount);
uint32_t avril_unused_read_cb(uint32_t address, void *dst, uint32_t amount);
