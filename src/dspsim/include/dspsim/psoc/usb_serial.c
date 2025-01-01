
#include "FreeRTOS.h"
#include "task.h"
#include "stream_buffer.h"
#include "dspsim/psoc/usb_serial.h"
#include "usb_serial_pvt.h"
#include "dspsim/psoc/usb_core.h"

#include <USBFS.h>
#include <USBFS_cdc.h>
#include <stdint.h>

void usb_serial_ctl_cfg_change_cb(void *_self) {}
void usb_serial_data_cfg_change_cb(void *_self)
{
    USBSerial self = _self;
    USBFS_LoadInEP(self->tx_ep, self->tx_ep_buf, sizeof(self->tx_ep_buf));
    USBFS_ReadOutEP(self->rx_ep, self->rx_ep_buf, sizeof(self->rx_ep_buf));
    USBFS_EnableOutEP(self->rx_ep);
}

USBSerial usb_serial_start(
    USBCore usb_core,
    uint8_t ctl_iface,
    uint8_t ctl_ep,
    uint8_t data_iface,
    uint8_t tx_ep,
    uint8_t rx_ep)
{
    USBSerial self = pvPortMalloc(sizeof(*self));
    self->ctl_iface = ctl_iface;
    self->ctl_ep = ctl_ep;
    self->data_iface = data_iface;
    self->tx_ep = tx_ep;
    self->rx_ep = rx_ep;

    usb_register_interface(usb_core, self, ctl_iface, usb_serial_ctl_cfg_change_cb);
    usb_register_interface(usb_core, self, data_iface, usb_serial_data_cfg_change_cb);

    USBFS_CDC_Init();

    return self;
}
