/*
 * app_vibration.c —— 振动电机驱动实现（RT-Thread 版）
 */

#include "app_vibration.h"

#define DBG_TAG    "app.vib"
#define DBG_LVL    DBG_INFO
#include <rtdbg.h>

static rt_bool_t s_vib_on = RT_FALSE;

rt_err_t app_vibration_init(void)
{
    rt_pin_mode(APP_PIN_VIBRATION, PIN_MODE_OUTPUT);
    rt_pin_write(APP_PIN_VIBRATION, PIN_LOW);
    s_vib_on = RT_FALSE;

    LOG_I("振动电机就绪: GPIO_14 (PA14)");
    return RT_EOK;
}

void app_vibration_on(void)
{
    rt_pin_write(APP_PIN_VIBRATION, PIN_HIGH);
    s_vib_on = RT_TRUE;
}

void app_vibration_off(void)
{
    rt_pin_write(APP_PIN_VIBRATION, PIN_LOW);
    s_vib_on = RT_FALSE;
}

void app_vibration_toggle(void)
{
    if (s_vib_on)
    {
        app_vibration_off();
    }
    else
    {
        app_vibration_on();
    }
}

rt_bool_t app_vibration_is_on(void)
{
    return s_vib_on;
}

void app_vibration_pulse(rt_uint32_t ms)
{
    app_vibration_on();
    rt_thread_mdelay(ms);
    app_vibration_off();
}
