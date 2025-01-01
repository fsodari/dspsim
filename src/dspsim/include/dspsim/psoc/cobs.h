/*
    Consistent overhead byte stuffing.
*/
#pragma once

#include "FreeRTOS.h"
#include "stream_buffer.h"
#include "message_buffer.h"
#include "task.h"

typedef struct CobsDef *Cobs;

// Encode buffer will encode messages from the message buffer and send them to the encode_buffer. This happends automatically in a task.
Cobs cobs_encode_start(MessageBufferHandle_t message_buffer, StreamBufferHandle_t encode_buffer, uint32_t max_message_size, uint32_t priority);

// This will read from the encode buffer until it detects a delimeter, then will decode and send to the message buffer.
Cobs cobs_decode_start(MessageBufferHandle_t message_buffer, StreamBufferHandle_t encode_buffer, uint32_t max_message_size, uint32_t priority);

// Plain functions for decoding/encoding.

// Decode a complete encoded message with 0 byte delimeter. Returns size of encoded message including 0.
uint32_t cobs_decode(uint8_t *dst, const uint8_t *src, uint32_t size);
// Encode a complete message and append a 0 byte delimeter. Returns size of decoded message.
uint32_t cobs_encode(uint8_t *dst, const uint8_t *src, uint32_t size);
