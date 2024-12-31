/*
    Consistent overhead byte stuffing.
*/
#pragma once

#include "FreeRTOS.h"
#include "stream_buffer.h"
#include "message_buffer.h"
#include "task.h"

typedef struct CobsDef *Cobs;
struct CobsDef
{
    MessageBufferHandle_t msg_buf;
    StreamBufferHandle_t encode_buf;
    uint32_t max_message_size;
    uint8_t *tmp_enc_buf;
    uint8_t *tmp_dec_buf;
    uint32_t buf_id;
    TaskHandle_t task_ref;
};

Cobs cobs_encode_start(MessageBufferHandle_t message_buffer, StreamBufferHandle_t encode_buffer, uint32_t max_message_size, uint32_t priority);
Cobs cobs_decode_start(MessageBufferHandle_t message_buffer, StreamBufferHandle_t encode_buffer, uint32_t max_message_size, uint32_t priority);

// Decode a complete encoded message with 0 byte delimeter. Returns size of encoded message including 0.
uint32_t cobs_decode(uint8_t *dst, const uint8_t *src, uint32_t size);
// Encode a complete message and append a 0 byte delimeter. Returns size of decoded message.
uint32_t cobs_encode(uint8_t *dst, const uint8_t *src, uint32_t size);

// Encoding: Write a complete message to the message buffer. It will be encoded, then written into the encode buffer.
// Decoding: Write data into the encode buffer, when a zero byte is added, it will be decoded

// // Add un-encoded message to the decode_buffer. It will be encoded, then written into the encode_buffer.
// uint32_t cobs_encode_msg(Cobs self, const void *src, uint32_t size);

// // When

// // uint32_t cobs_decode_data(Cobs self, void *dst, uint32_t size);