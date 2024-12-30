
#include "project_config.h"
// #include "project.h"
#include "CyLib.h"
#include "LEDCtl.h"

#include "dspsim/usb.h"
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

void Echo(void *arg)
{
    (void)arg;
    static uint8_t tmp_buf[4096];
    uint32_t timeout = pdMS_TO_TICKS(100);

    for (ever)
    {
        uint32_t count = usb_serial_read(tmp_buf, 4096, timeout);
        if (count)
        {
            usb_serial_write(tmp_buf, count, timeout);
        }
    }
}

/*
    This function will be called when the scheduler starts.
    Create all tasks here. You can use FreeRToS features since the scheduler will have started already.
*/
void vApplicationDaemonTaskStartupHook(void)
{
    // Startup usb
    usb_start(0, 10);
    // Start up usb serial.
    usb_serial_start(
        USB_SERIAL_CTL_IFACE, USB_SERIAL_CTL_EP,
        USB_SERIAL_DATA_IFACE, USB_SERIAL_TX_EP, USB_SERIAL_RX_EP,
        USB_SERIAL_TX_BUF_SIZE, USB_SERIAL_RX_BUF_SIZE,
        USB_SERIAL_PRIORITY);
    // Create all tasks here.
    xTaskCreate(&Blinky, "", configMINIMAL_STACK_SIZE, NULL, 3, NULL);

    // USB Serial echo
    xTaskCreate(&Echo, "", configMINIMAL_STACK_SIZE, NULL, 3, NULL);
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
