/*
 * app_dcmotor.h —— 直流电机（吹风）驱动（RT-Thread 版）
 *
 * 与裸机版 DCMotor.c + PWM.c 等价：EPWM1 的 A/B 两路构成 H 桥正反转，
 * NSLEEP 引脚唤醒驱动器。PWM 通过 RT-Thread 的 PWM 设备框架操作。
 */

#ifndef __APP_DCMOTOR_H__
#define __APP_DCMOTOR_H__

#include <rtthread.h>
#include "app_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  初始化 NSLEEP 引脚并打开 EPWM1 设备
 */
rt_err_t app_dcmotor_init(void);

/**
 * @brief  设置转速与方向
 * @param  percent -100 ~ +100，正数正转、负数反转、0 停止
 */
rt_err_t app_dcmotor_set_speed(rt_int16_t percent);

/**
 * @brief  读取最近一次设置的占空比
 */
rt_int16_t app_dcmotor_get_speed(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_DCMOTOR_H__ */
