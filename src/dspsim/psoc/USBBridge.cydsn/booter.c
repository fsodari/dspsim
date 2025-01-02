#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"

#include "booter.h"
#include <string.h>
#include <project.h>

uint32_t booter_write(void *self, uint32_t address, const void *src, uint32_t amount);

void do_bootload(TimerHandle_t id)
{
    (void)id;
#ifdef CY_BOOTLOADABLE_Bootloadable_H
    Bootloadable_Load();
#endif
}

Booter booter_create(int32_t password)
{
    Booter self = pvPortMalloc(sizeof(*self));
    self->password = password;

    mmi_init((MMI)self, booter_write, mmi_fwrite_only_err, sizeof(self->password), dint32);
    self->delay_timer = xTimerCreate("", pdMS_TO_TICKS(10), pdFALSE, 0, &do_bootload);
    return self;
}

void booter_set_password(Booter self, int32_t password)
{
    self->password = password;
}

uint32_t booter_write(void *_self, uint32_t address, const void *src, uint32_t amount)
{
    Booter self = _self;

    uint32_t error = 0;
    int32_t password = 0;

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
