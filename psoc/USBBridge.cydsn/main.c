
#include "project_config.h"
// #include "project.h"
#include "CyLib.h"
#include "LEDCtl.h"

#include "dspsim/usb.h"
#include "dspsim/avril.h"
#include "dspsim/sram.h"
#include "dspsim/cobs.h"
#include "booter.h"

#include "FreeRTOS.h"
#include "task.h"

// Set up psoc to use free rtos.
static void HardwareSetup(void);
void Blinky(void *arg)
{
    (void)arg;

    for (ever)
    {
        LEDCtl_Write(0x1);
        vTaskDelay(pdMS_TO_TICKS(500));
        LEDCtl_Write(0x0);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/*
    This function will be called when the scheduler starts.
    Create all tasks here. You can use FreeRToS features since the scheduler will have started already.
*/
void vApplicationDaemonTaskStartupHook(void)
{
    // Start usb
    usb_start(0, 10);
    // Start usb serial.
    usb_serial_start(
        USB_SERIAL_CTL_IFACE, USB_SERIAL_CTL_EP,
        USB_SERIAL_DATA_IFACE, USB_SERIAL_TX_EP, USB_SERIAL_RX_EP,
        USB_SERIAL_TX_BUF_SIZE, USB_SERIAL_RX_BUF_SIZE,
        USB_SERIAL_PRIORITY);

    //
    booter_start();

    MessageBufferHandle_t rx_msg_buf = xMessageBufferCreate(1024);
    MessageBufferHandle_t tx_msg_buf = xMessageBufferCreate(1024);

    Cobs decoder = cobs_decode_create(rx_msg_buf, usb_serial_rx_buf(), 1024, 2);
    Cobs encoder = cobs_encode_create(tx_msg_buf, usb_serial_tx_buf(), 1024, 2);

    avril_start(10, tx_msg_buf, rx_msg_buf);
    avril_add_mode(0, (MMI *)sram_create(1024));
    avril_add_mode(1, booter_mmi);

    xTaskCreate(&Blinky, "", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
}

int main(void)
{
    CyGlobalIntEnable;
    HardwareSetup();
    vTaskStartScheduler();

    for (ever)
    {
    }
}

static void HardwareSetup(void)
{
    /* Port layer functions that need to be copied into the vector table. */
    extern void xPortPendSVHandler(void);
    extern void xPortSysTickHandler(void);
    extern void vPortSVCHandler(void);
    extern cyisraddress CyRamVectors[];

    /* Install the OS Interrupt Handlers. */
    CyRamVectors[11] = (cyisraddress)vPortSVCHandler;
    CyRamVectors[14] = (cyisraddress)xPortPendSVHandler;
    CyRamVectors[15] = (cyisraddress)xPortSysTickHandler;
}

/*---------------------------------------------------------------------------*/
void vApplicationStackOverflowHook(TaskHandle_t pxTask, char *pcTaskName)
{
    (void)pxTask;
    (void)pcTaskName;
    /* The stack space has been exceeded for a task, considering allocating more. */
    taskDISABLE_INTERRUPTS();
    for (ever)
    {
    }
}
/*---------------------------------------------------------------------------*/

void vApplicationMallocFailedHook(void)
{
    /* The heap space has been exceeded. */
    taskDISABLE_INTERRUPTS();
    for (ever)
    {
    }
}
