#include "dspsim/psoc/cobs.h"

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

void CobsDecodeTask(void *_self);
void CobsEncodeTask(void *_self);

Cobs cobs_create(MessageBufferHandle_t message_buffer, StreamBufferHandle_t encode_buffer, uint32_t max_message_size)
{
    Cobs self = pvPortMalloc(sizeof(*self));
    self->max_message_size = max_message_size;
    self->msg_buf = message_buffer;
    self->encode_buf = encode_buffer;
    self->tmp_enc_buf = pvPortMalloc(max_message_size);
    self->tmp_dec_buf = pvPortMalloc(max_message_size);
    self->buf_id = 0;

    return self;
}

Cobs cobs_encode_start(MessageBufferHandle_t message_buffer, StreamBufferHandle_t encode_buffer, uint32_t max_message_size, uint32_t priority)
{
    Cobs self = cobs_create(message_buffer, encode_buffer, max_message_size);
    xTaskCreate(&CobsEncodeTask, "", configMINIMAL_STACK_SIZE, self, priority, &self->task_ref);
    return self;
}

Cobs cobs_decode_start(MessageBufferHandle_t message_buffer, StreamBufferHandle_t encode_buffer, uint32_t max_message_size, uint32_t priority)
{
    Cobs self = cobs_create(message_buffer, encode_buffer, max_message_size);
    xTaskCreate(&CobsDecodeTask, "", configMINIMAL_STACK_SIZE, self, priority, &self->task_ref);
    return self;
}

void CobsDecodeTask(void *_self)
{
    Cobs self = _self;

    for (;;)
    {
        // Wait on data from stream buffer and copy into the temp buffer.
        uint32_t received = xStreamBufferReceive(self->encode_buf, &self->tmp_enc_buf[self->buf_id], 1, portMAX_DELAY);
        if (received)
        {
            // Check if it's a zero?
            int received_zero = self->tmp_enc_buf[self->buf_id] == 0;
            self->buf_id++; // Increment to count the byte.

            // If we have not received a zero but there are more bytes, keep checking.
            if (!received_zero)
            {
                uint32_t available = xStreamBufferBytesAvailable(self->encode_buf);
                for (uint32_t i = 0; i < available; i++)
                {
                    received = xStreamBufferReceive(self->encode_buf, &self->tmp_enc_buf[self->buf_id], 1, 0);
                    if (received)
                    {
                        received_zero = self->tmp_enc_buf[self->buf_id] == 0;
                        self->buf_id++;
                        if (received_zero)
                        {
                            break;
                        }
                    }
                }
            }

            if (received_zero)
            {
                uint32_t decoded = cobs_decode(self->tmp_dec_buf, self->tmp_enc_buf, self->buf_id);
                self->buf_id = 0;
                xMessageBufferSend(self->msg_buf, self->tmp_dec_buf, decoded, portMAX_DELAY);
            }
        }
    }
}

void CobsEncodeTask(void *_self)
{
    Cobs self = _self;

    for (;;)
    {
        volatile uint32_t received = xMessageBufferReceive(self->msg_buf, self->tmp_dec_buf, self->max_message_size, portMAX_DELAY);
        if (received)
        {
            uint32_t encoded = cobs_encode(self->tmp_enc_buf, self->tmp_dec_buf, received);
            xStreamBufferSend(self->encode_buf, self->tmp_enc_buf, encoded, pdMS_TO_TICKS(10));
        }
    }
}

// Decode a complete encoded message with 0 byte delimeter. Returns size of encoded message including 0.
uint32_t cobs_decode(uint8_t *dst, const uint8_t *src, uint32_t size)
{
    uint8_t *dst_ptr = dst;
    const uint8_t *src_ptr = src;
    const uint8_t *const src_end = src + size - 1; // Ignore the zero at the end

    while (src_ptr < src_end)
    {
        uint8_t code = *src_ptr++;
        code--;

        for (uint8_t i = code; i != 0; i--)
        {
            *dst_ptr++ = *src_ptr++;
        }
        if (src_ptr >= src_end)
        {
            break;
        }
        if (code != 0xFE)
        {
            *dst_ptr++ = 0;
        }
    }

    return dst_ptr - dst;
}

// Encode a complete message and append a 0 byte delimeter. Returns size of decoded message.
uint32_t cobs_encode(uint8_t *dst, const uint8_t *src, uint32_t size)
{
    uint8_t *dst_ptr = dst + 1;
    uint8_t *code_ptr = dst;

    uint8_t code = 1;

    for (uint32_t i = 0; i < size; i++)
    {
        uint8_t rx_byte = src[i];
        if (rx_byte == 0)
        {
            // Zero byte in data. Update the code.
            *code_ptr = code;
            code_ptr = dst_ptr++;
            code = 1;
            if (i == (size - 1))
            {
                break;
            }
        }
        else
        {
            // Non zero data, append until we need to reset the code.
            *dst_ptr++ = rx_byte;
            code++;
            if (i == (size - 1))
            {
                break;
            }
            // Set the code.
            if (code == 0xFF)
            {
                // Code gets set at the end. So skip setting the code if it's the last byte in src..
                *code_ptr = code;
                code_ptr = dst_ptr++;
                code = 1;
            }
        }
    }
    *code_ptr = code;
    *dst_ptr++ = 0; // append the 0
    return dst_ptr - dst;
}

/*

for (;;)
{
    if (dst_write_ptr >= dst_buf_end_ptr)
    {
        result.status |= COBS_ENCODE_OUT_BUFFER_OVERFLOW;
        break;
    }

    src_byte = *src_read_ptr++;
    if (src_byte == 0u)
    {
        *dst_code_write_ptr = search_len;
        dst_code_write_ptr = dst_write_ptr++;
        search_len = 1u;
        if (src_read_ptr >= src_end_ptr)
        {
            break;
        }
    }
    else
    {
        *dst_write_ptr++ = src_byte;
        search_len++;
        if (src_read_ptr >= src_end_ptr)
        {
            break;
        }
        if (search_len == 0xFFu)
        {
            *dst_code_write_ptr = search_len;
            dst_code_write_ptr = dst_write_ptr++;
            search_len = 1u;
        }
    }
}

*/
