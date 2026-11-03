/*
 * app_vibration.h —— 振动电机驱动（RT-Thread 版）
 *
 * 与裸机版 vibration_motor.c 的 GPIO 开关模式等价（PA14）。
 * 裸机版里那套 EPWM8 强度调节在本应用的作业流程中并未使用，故不做移植；
 * 如需要，可把 PA14 复用为 EPWM8_A（ALT1）后用 PWM 设备框架驱动。
 */

#ifndef __APP_VIBRATION_H__
#define __APP_VIBRATION_H__

#include <rtthread.h>
#include "app_config.h"

#ifdef __cplusplus
extern "C" {
#endif

rt_err_t app_vibration_init(void);
void     app_vibration_on(void);
void     app_vibration_off(void);
void     app_vibration_toggle(void);
rt_bool_t app_vibration_is_on(void);

/** 振动指定毫秒数（阻塞当前线程） */
void     app_vibration_pulse(rt_uint32_t ms);

#ifdef __cplusplus
}
#endif

#endif /* __APP_VIBRATION_H__ */
