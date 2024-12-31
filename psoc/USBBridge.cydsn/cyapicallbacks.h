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

#include "project_config.h"

/*Define your macro callbacks here */
/*For more information, refer to the Writing Code topic in the PSoC Creator Help.*/

#define USBFS_EP_7_ISR_EXIT_CALLBACK
#define USBFS_EP_7_ISR_ExitCallback() usb_serial_main_tx_isr()

#define USBFS_EP_8_ISR_ENTRY_CALLBACK
#define USBFS_EP_8_ISR_EntryCallback() usb_serial_main_rx_isr()
