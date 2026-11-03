/*
 * app_stepper.h —— 步进电机驱动（RT-Thread 版）
 *
 * 与裸机版 motor.c / track.c 等价：PUL/DIR/ENA 三线，软件脉冲，按角度定位。
 * 差异：脉冲间的微秒延时改用 RT-Thread 的 rt_hw_us_delay()，
 *       角度记忆改为线程安全的读写接口。
 */

#ifndef __APP_STEPPER_H__
#define __APP_STEPPER_H__

#include <rtthread.h>
#include "app_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#define APP_STEP_DIR_CW     0U
#define APP_STEP_DIR_CCW    1U

/**
 * @brief  初始化 PUL/DIR/ENA 三个引脚（默认禁用驱动器）
 */
rt_err_t app_stepper_init(void);

/**
 * @brief  使能 / 关闭步进驱动器（ENA 低有效）
 */
void app_stepper_enable(rt_bool_t enable);

/**
 * @brief  设置方向
 */
void app_stepper_set_dir(rt_uint8_t dir);

/**
 * @brief  走指定脉冲数
 * @param  steps 脉冲个数
 * @param  slow  RT_TRUE 用慢速脉冲（600us 高/低），否则 50us + 500us 间隔
 */
void app_stepper_run_steps(rt_uint32_t steps, rt_bool_t slow);

/**
 * @brief  转到目标角度（相对上电零点，带 ±2° 死区）
 * @param  target_angle 目标角度（度，可正可负）
 */
rt_err_t app_stepper_track_x(rt_int16_t target_angle);

/**
 * @brief  读取当前记录的角度
 */
rt_int16_t app_stepper_get_angle(void);

/**
 * @brief  把软件角度记忆清零（例如手动回零后调用）
 */
void app_stepper_reset_angle(rt_int16_t angle);

#ifdef __cplusplus
}
#endif

#endif /* __APP_STEPPER_H__ */
