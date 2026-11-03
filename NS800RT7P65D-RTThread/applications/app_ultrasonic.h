/*
 * app_ultrasonic.h —— 超声波测距（RT-Thread 版）
 *
 * 硬件：TRIG = PA5（GPIO 推挽输出），ECHO = PA16（ECAP1 输入，经 XBAR INPUT7）
 * 方案：直接操作厂商 ECAP 外设，单次捕获「上升沿 → 下降沿」两个时间戳。
 *
 * 为什么不用 BSP 的 ecap1 设备驱动：
 *   BSP 的 drv_ecap.c 固定配置为「连续捕获 + 4 个边沿 + 每个边沿都复位计数器」，
 *   适合测量连续 PWM 的高低电平，而超声波每次触发只产生一个脉冲，
 *   拿不到完整的 4 边沿序列，因此这里沿用裸机版验证过的单次捕获配置。
 *   （所以 menuconfig 里不要打开 ECAP，避免两套代码争用 ECAP1。）
 */

#ifndef __APP_ULTRASONIC_H__
#define __APP_ULTRASONIC_H__

#include <rtthread.h>
#include "app_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  初始化 TRIG 引脚与 ECAP1 捕获通道
 */
rt_err_t app_ultrasonic_init(void);

/**
 * @brief  测一次距离（阻塞）
 * @return 距离（厘米）；0.0f 表示超时或无回波
 */
float app_ultrasonic_get_distance_cm(void);

/**
 * @brief  最近一次的原始脉宽计数值（调试用）
 */
rt_uint32_t app_ultrasonic_get_last_ticks(void);

/**
 * @brief  测距校准系数：默认 1.0
 * @param  gain 实测值 / 理论值，例如测得 101cm 而实际 100cm 则传 0.99
 */
void app_ultrasonic_set_gain(float gain);

/**
 * @brief  读取当前校准系数
 */
float app_ultrasonic_get_gain(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_ULTRASONIC_H__ */
