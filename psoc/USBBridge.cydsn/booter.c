#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"

#include "booter.h"
#include <string.h>
#include <project.h>

MMI *booter_mmi;
static uint32_t secret_password = 0;
TimerHandle_t delay_timer = NULL;

uint32_t booter_write(void *self, uint32_t address, const void *src, uint32_t amount);
uint32_t booter_read(void *self, uint32_t address, void *dst, uint32_t amount);

void do_bootload(TimerHandle_t id)
{
#ifdef CY_BOOTLOADABLE_Bootloadable_H
    Bootloadable_Load();
#endif
}

void booter_start(void)
{
    booter_mmi = pvPortMalloc(sizeof(*booter_mmi));
    mmi_init(booter_mmi, booter_write, booter_read, sizeof(secret_password));

    delay_timer = xTimerCreate("", pdMS_TO_TICKS(10), pdFALSE, 0, &do_bootload);
}

void booter_set_password(uint32_t _password)
{
    secret_password = _password;
}

uint32_t booter_write(void *self, uint32_t address, const void *src, uint32_t amount)
{
    uint32_t error = 0;
    uint32_t password = 0;

    if (address == 0 && amount == sizeof(password))
    {
        memcpy(&password, src, sizeof(password));
        if (password == secret_password)
        {
            xTimerStart(delay_timer, pdMS_TO_TICKS(10));
        }
    }
    else
    {
        error = 1;
    }

    return error;
}

uint32_t booter_read(void *self, uint32_t address, void *dst, uint32_t amount)
{
    uint32_t error = 0;

    return error;
}
