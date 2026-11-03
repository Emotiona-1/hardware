/*
 * app_stepper.c —— 步进电机驱动实现（RT-Thread 版）
 *
 * 换算关系沿用裸机版 track.c：
 *   脉冲数 = |角度差| × (1600 / 360) × 细分系数(3)
 * 该系数是实机上标定出来的经验值，若换成不同细分/减速比的驱动器需要重新标定。
 */

#include "app_stepper.h"

#define DBG_TAG    "app.step"
#define DBG_LVL    DBG_INFO
#include <rtdbg.h>

static rt_int16_t s_current_angle = 0;

rt_err_t app_stepper_init(void)
{
    rt_pin_mode(APP_PIN_STEP_PUL, PIN_MODE_OUTPUT);
    rt_pin_mode(APP_PIN_STEP_DIR, PIN_MODE_OUTPUT);
    rt_pin_mode(APP_PIN_STEP_ENA, PIN_MODE_OUTPUT);

    rt_pin_write(APP_PIN_STEP_PUL, PIN_LOW);
    rt_pin_write(APP_PIN_STEP_DIR, PIN_LOW);
    rt_pin_write(APP_PIN_STEP_ENA, PIN_HIGH);   /* 高电平 = 禁用 */

    LOG_I("步进电机就绪: PUL/DIR/ENA = GPIO_2/GPIO_3/GPIO_4 (PA2/PA3/PA4)");
    return RT_EOK;
}

void app_stepper_enable(rt_bool_t enable)
{
    /* ENA 低有效，与裸机版 Motor_Enable() 一致 */
    rt_pin_write(APP_PIN_STEP_ENA, enable ? PIN_LOW : PIN_HIGH);
}

void app_stepper_set_dir(rt_uint8_t dir)
{
    rt_pin_write(APP_PIN_STEP_DIR,
                 (dir == APP_STEP_DIR_CW) ? PIN_LOW : PIN_HIGH);
}

void app_stepper_run_steps(rt_uint32_t steps, rt_bool_t slow)
{
    rt_uint32_t i;

    for (i = 0; i < steps; i++)
    {
        rt_pin_write(APP_PIN_STEP_PUL, PIN_HIGH);
        rt_hw_us_delay(slow ? APP_STEP_HALF_US : 50U);
        rt_pin_write(APP_PIN_STEP_PUL, PIN_LOW);

        if (slow)
        {
            rt_hw_us_delay(APP_STEP_HALF_US);
        }
        else
        {
            rt_hw_us_delay(APP_STEP_GAP_US);
        }
    }
}

rt_err_t app_stepper_track_x(rt_int16_t target_angle)
{
    rt_int16_t delta;
    rt_uint32_t steps;

    delta = (rt_int16_t)(target_angle - s_current_angle);

    if ((delta < APP_STEP_DEAD_ZONE_DEG) && (delta > -APP_STEP_DEAD_ZONE_DEG))
    {
        LOG_D("角度差 %d 在死区内，不动作", delta);
        return RT_EOK;
    }

    /* 脉冲数 = |delta| × 脉冲/度 × 细分 */
    steps = (rt_uint32_t)((float)((delta > 0) ? delta : -delta)
                          * APP_STEP_PULSE_PER_DEG * APP_STEP_SUBDIV);

    app_stepper_set_dir((delta > 0) ? APP_STEP_DIR_CW : APP_STEP_DIR_CCW);
    app_stepper_enable(RT_TRUE);
    app_stepper_run_steps(steps, RT_TRUE);
    app_stepper_enable(RT_FALSE);

    s_current_angle = target_angle;

    LOG_I("步进: %d° → %d°（%d 脉冲）", (int)(target_angle - delta),
          (int)target_angle, (int)steps);
    return RT_EOK;
}

rt_int16_t app_stepper_get_angle(void)
{
    return s_current_angle;
}

void app_stepper_reset_angle(rt_int16_t angle)
{
    s_current_angle = angle;
}
