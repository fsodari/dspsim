/*
    FreeRTOS includes for avril.
*/
#pragma once
#include "FreeRTOS.h"
#include "message_buffer.h"
#include "dspsim/psoc/avril.h"

MessageBufferHandle_t avril_tx_msg_buf(Avril self);
MessageBufferHandle_t avril_rx_msg_buf(Avril self);
