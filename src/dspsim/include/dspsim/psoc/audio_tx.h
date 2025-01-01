#pragma once
#include "FreeRTOS.h"
#include "message_buffer.h"
#include "queue.h"
#include "dspsim/psoc/cbuf.h"

typedef struct AudioTxDef *AudioTx;
struct AudioTxDef
{
    MessageBufferHandle_t msg_buf;
    QueueHandle_t fb_buf;
    CBuf cbuf;

    // I2S DMA Info
    uint8_t dma_ch;
};

// Monitor the buffer size
typedef struct AudioTxMonitorDef *AudiTxMonitor;
struct AudioTxMonitorDef
{
};

AudioTx audio_tx_start(MessageBufferHandle_t msg_buf, QueueHandle_t fb_buf, uint32_t buffer_size);
uint32_t audio_tx_buf_size(AudioTx self);
