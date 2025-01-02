/*
    Adaptive Virtual Register Interface Library

    Defines a protocol for sending and receiving mmi commands and acknowledging responses.

    Uses FreeRTOS MessageBuffers for receiving/sending packets. Generic interface.

    Protocol:
    Host sends a command and waits for an ack.

    Host: Command -> Avril
    Avril: Response -> Host

    Command Packet format is symmetric for commands and responses.
    Response msg_id will be the same as the command msg_id.

    {
        uint8_t command; // Command. Nop, Write, Read, NopAck, WriteAck, ReadAck, ...
        uint8_t mode;  // Mode id.
        uint16_t msg_id; // Msg id. Acks will have the same id as the command.
        uint32_t size;    // Payload size.
        uint32_t address; // Register start address.
        uint8_t data[];   // 0 or more bytes of data.
    }

    Commands: nop, write, read, nop_ack, write_ack, read_ack
    Commonly used Modes: vmmi, bootload, metadata

*/
#pragma once

#include "FreeRTOS.h"
#include "message_buffer.h"

#include "dspsim/psoc/avril_defs.h"
#include "dspsim/psoc/mmi.h"

#include <stdint.h>

typedef struct AvrilDef *Avril;

/*
    Start the avril service. This will wait on messages sent to the rx buffer.
*/
Avril avril_start(uint32_t n_modes, uint32_t max_msg_size, uint32_t priority);

/*
    Add an interface mode.
*/
dErrorCodes avril_add_mode(Avril self, uint32_t mode_id, MMI mode_interface);

MessageBufferHandle_t avril_tx_msg_buf(Avril self);
MessageBufferHandle_t avril_rx_msg_buf(Avril self);
