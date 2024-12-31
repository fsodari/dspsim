#pragma once
#include <stdint.h>

struct USBSerialDef
{
    uint8_t ctl_iface;
    uint8_t ctl_ep;

    uint8_t data_iface;
    uint8_t tx_ep;
    uint8_t rx_ep;

    uint8_t tx_ep_buf[64];
    uint8_t rx_ep_buf[64];
};
