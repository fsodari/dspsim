/* ========================================
 *
 * Copyright YOUR COMPANY, THE YEAR
 * All Rights Reserved
 * UNPUBLISHED, LICENSED SOFTWARE.
 *
 * CONFIDENTIAL AND PROPRIETARY INFORMATION
 * WHICH IS THE PROPERTY OF your company.
 *
 * ========================================
 */
#pragma once

#include "dspsim/usb_serial.h"

/*Define your macro callbacks here */
/*For more information, refer to the Writing Code topic in the PSoC Creator Help.*/

#define USBFS_EP_7_ISR_EXIT_CALLBACK
#define USBFS_EP_7_ISR_ExitCallback() usb_serial_tx_ep_isr()

#define USBFS_EP_8_ISR_ENTRY_CALLBACK
#define USBFS_EP_8_ISR_EntryCallback() usb_serial_rx_ep_isr()
