/*
 * app_dcmotor.c —— 直流电机驱动实现（RT-Thread 版）
 *
 * 注意（踩坑点）：本 BSP 的 epwm 驱动把 struct rt_pwm_configuration.period
 * 当作【微秒】使用（drv_epwm.c 里 signal_freq = 1 / (period / 1e6)），
 * 与 RT-Thread 文档中「纳秒」的约定不同。因此这里周期填 50，即 50us = 20kHz。
 */

#include "app_dcmotor.h"

#define DBG_TAG    "app.dc"
#define DBG_LVL    DBG_INFO
#include <rtdbg.h>

static struct rt_device_pwm *s_pwm_dev = RT_NULL;
static rt_int16_t            s_speed_pct = 0;

static rt_err_t dcmotor_apply(rt_uint32_t ch, rt_uint32_t pulse_us)
{
    rt_err_t ret;

    ret = rt_pwm_set(s_pwm_dev, ch, APP_PWM_PERIOD_US, pulse_us);
    if (ret != RT_EOK)
    {
        LOG_E("rt_pwm_set(ch=%d, pulse=%d) 失败: %d", (int)ch, (int)pulse_us, ret);
        return ret;
    }

    ret = rt_pwm_enable(s_pwm_dev, ch);
    if (ret != RT_EOK)
    {
        LOG_E("rt_pwm_enable(ch=%d) 失败: %d", (int)ch, ret);
        return ret;
    }

    return RT_EOK;
}

rt_err_t app_dcmotor_init(void)
{
    rt_device_t dev;

    /* NSLEEP 高电平唤醒驱动器（与裸机版一致） */
    rt_pin_mode(APP_PIN_DC_NSLEEP, PIN_MODE_OUTPUT);
    rt_pin_write(APP_PIN_DC_NSLEEP, PIN_HIGH);

    dev = rt_device_find(APP_PWM_DEV_NAME);
    if (dev == RT_NULL)
    {
        LOG_E("找不到 PWM 设备 %s，请在 menuconfig 里打开 BSP_USING_EPWM1",
              APP_PWM_DEV_NAME);
        return -RT_ERROR;
    }

    (void)rt_device_open(dev, RT_DEVICE_OFLAG_RDWR);
    s_pwm_dev = (struct rt_device_pwm *)dev;

    /* 初始化时两路都输出 0% */
    dcmotor_apply(APP_PWM_CH_A, 0);
    dcmotor_apply(APP_PWM_CH_B, 0);

    s_speed_pct = 0;

    LOG_I("直流电机就绪: %s A/B 通道, 20kHz", APP_PWM_DEV_NAME);
    return RT_EOK;
}

rt_err_t app_dcmotor_set_speed(rt_int16_t percent)
{
    rt_uint32_t pulse;
    rt_err_t    ret;

    if (s_pwm_dev == RT_NULL)
    {
        return -RT_ERROR;
    }

    if (percent > 100)
    {
        percent = 100;
    }
    else if (percent < -100)
    {
        percent = -100;
    }

    /* 占空比换算成微秒脉宽（周期 50us） */
    if (percent >= 0)
    {
        pulse = (rt_uint32_t)((APP_PWM_PERIOD_US * percent) / 100);
        ret = dcmotor_apply(APP_PWM_CH_A, pulse);
        if (ret == RT_EOK)
        {
            ret = dcmotor_apply(APP_PWM_CH_B, 0);
        }
    }
    else
    {
        pulse = (rt_uint32_t)((APP_PWM_PERIOD_US * (-percent)) / 100);
        ret = dcmotor_apply(APP_PWM_CH_A, 0);
        if (ret == RT_EOK)
        {
            ret = dcmotor_apply(APP_PWM_CH_B, pulse);
        }
    }

    if (ret == RT_EOK)
    {
        s_speed_pct = percent;
        LOG_I("直流电机: %d%%", (int)percent);
    }

    return ret;
}

rt_int16_t app_dcmotor_get_speed(void)
{
    return s_speed_pct;
}
