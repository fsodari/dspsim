#pragma once
#include "dspsim/psoc/usb.h"

// Enable processing data before transmitting.
#define ENABLE_PROC 1

// Switches
#define SW_BOOT 0x8
#define SW_SOMESWITCH 0x4
#define SW_MUTE 0x2

// Stack sizes
#define LOG_MIN_STACK_SIZE 512

// Task priorities
#define AUDIO_OUT_TASK_PRI 5
#define AUDIO_OUT_MONITOR_TASK_PRI 4
#define USB_TASK_PRI 3
#define SYNC_TASK_PRI 2
#define SERIAL_TASK_PRI 2
#define EAR_SAVER_TASK_PRI 2
#define LOG_TASK_PRI 1

// Initial sample rate
#define AUDIO_SAMPLE_RATE 48000

// // Enough for 96kHz
// #define USB_AUDIO_EP_BUF_SIZE 588
// #define USB_CONFIG_SERVICE_MAX_WAIT 100
// #define USB_FEEDBACK_MAX_WAIT 150
// #define USB_SERIAL_EP_BUF_SIZE 64

// // USB Interface Configuration
// #define USBFS_DEVICE 0
// #define USB_ALT_INVALID 0xFF

// #define USB_AUDIO_CTL_IFACE 0
// #define USB_AUDIO_CTL_EP 5

// #define USB_AUDIO_OUT_IFACE 1
// #define USB_AUDIO_OUT_EP 1
// #define USB_AUDIO_OUT_N_ALT 2
// #define USB_AUDIO_OUT_ALT_ZEROBW 0
// #define USB_AUDIO_OUT_ALT_STREAM 1

// #define USB_AUDIO_FB_EP 3
// // #define USB_AUDIO_IN_EP 2

// #define USB_AUDIO_HID_IFACE 2
// #define USB_AUDIO_HID_EP 4

//
// USB Serial Port
//
#define USB_N_INTERFACES 10

#define USB_SERIAL_CTL_IFACE 3
#define USB_SERIAL_DATA_IFACE 4

#define USB_SERIAL_CTL_EP 6
#define USB_SERIAL_TX_EP 7
#define USB_SERIAL_RX_EP 8

#define USB_SERIAL_TX_BUF_SIZE 1024
#define USB_SERIAL_TX_PRIORITY 3

#define USB_SERIAL_RX_BUF_SIZE 1024
#define USB_SERIAL_RX_PRIORITY 3

extern USBSerialTx usb_serial_tx;
static inline void usb_serial_main_tx_isr() { usb_serial_tx_ep_isr(usb_serial_tx); }

extern USBSerialRx usb_serial_rx;
static inline void usb_serial_main_rx_isr() { usb_serial_rx_ep_isr(usb_serial_rx); }

// Cobs config
#define COBS_ENCODE_PRIORITY 2
#define COBS_DECODE_PRIORITY 2

// Avril Config
#define VMMI_N_INTERFACES 5
#define AVRIL_N_MODES 4

#define AVRIL_MAX_MSG_SIZE 1024
#define AVRIL_PRIORITY 1

// VMMI Metadata
#define VMMI_META_RESERVE_SIZE 4096

#define BOOTLOAD_PASSWORD 0

// // USB Synchronization
// #define SYNC_WINDOW_SIZE 128
// #define SYNC_N_WINDOWS 16
// #define SYNC_SHIFT 5 // log2(SYNC_N_WINDOWS * SYNC_WINDOW_SIZE / 64). 3 for 96kHz
// #define SYNC_START_DELAY 200
// #define SYNC_MAX_WAIT 150

// // Audio transmit buffer
// #define AUDIO_TX_TRANSFER_SIZE 588
// #define AUDIO_TX_N_TDS 16
// #define AUDIO_TX_BUF_SIZE (AUDIO_TX_TRANSFER_SIZE * AUDIO_TX_N_TDS)
// // Turn on audio when it's half full
// #define AUDIO_TX_ACTIVE_LIMIT ((AUDIO_TX_TRANSFER_SIZE * AUDIO_TX_N_TDS) >> 1)

// #define AUDIO_TX_OVERFLOW_BLOCKS 4
// #define AUDIO_TX_OVERFLOW_LIMIT (AUDIO_TX_BUF_SIZE - AUDIO_TX_TRANSFER_SIZE * AUDIO_TX_OVERFLOW_BLOCKS)
// #define AUDIO_TX_UNDERFLOW_LIMIT (AUDIO_TX_TRANSFER_SIZE * AUDIO_TX_OVERFLOW_BLOCKS)

// #define AUDIO_TX_TX_MAX_WAIT 1
// #define AUDIO_TX_MONITOR_MAX_WAIT 1
// #define AUDIO_TX_LOGGING_INTERVAL 2000
// #define AUDIO_TX_DELTA_QUEUE_SIZE 8
// #define AUDIO_TX_MAX_BS_TDS ((AUDIO_TX_BUF_SIZE + DMA_MAX_TRANSFER_SIZE - 1) / DMA_MAX_TRANSFER_SIZE) // Sufficient number of tds for entire memory.

// // Ear Saver Config
// #define EAR_SAVER_STARTUP_DELAY 100
// #define EAR_SAVER_RESET_INTERVAL 2

// // Logging
// #define GLOBAL_LOG_LEVEL LOG_INFO
// #define LOG_MESSAGE_BUF_SIZE 256

//
#define DMA_MAX_TRANSFER_SIZE 4095

#define ever \
    ;        \
    ;
