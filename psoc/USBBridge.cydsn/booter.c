#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"

#include "booter.h"
#include <string.h>
#include <project.h>

uint32_t booter_write(void *self, uint32_t address, const void *src, uint32_t amount);
uint32_t booter_read(void *self, uint32_t address, void *dst, uint32_t amount);

void do_bootload(TimerHandle_t id)
{
#ifdef CY_BOOTLOADABLE_Bootloadable_H
    Bootloadable_Load();
#endif
}

Booter booter_create(uint32_t password)
{
    Booter self = pvPortMalloc(sizeof(*self));
    self->password = password;

    mmi_init((MMI)self, booter_write, booter_read, sizeof(self->password));
    self->delay_timer = xTimerCreate("", pdMS_TO_TICKS(10), pdFALSE, 0, &do_bootload);
    return self;
}

void booter_set_password(Booter self, uint32_t password)
{
    self->password = password;
}

uint32_t booter_write(void *_self, uint32_t address, const void *src, uint32_t amount)
{
    Booter self = _self;

    uint32_t error = 0;
    uint32_t password = 0;

    if (address == 0 && amount == sizeof(password))
    {
        memcpy(&password, src, sizeof(password));
        if (password == self->password)
        {
            xTimerStart(self->delay_timer, pdMS_TO_TICKS(10));
        }
    }
    else
    {
        error = 1;
    }

    return error;
}

uint32_t booter_read(void *_self, uint32_t address, void *dst, uint32_t amount)
{
    Booter self = _self;
    (void)self;
    uint32_t error = 0;

    return error;
}
