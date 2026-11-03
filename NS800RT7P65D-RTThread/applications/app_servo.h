/*
 * app_servo.h —— 总线舵机驱动（RT-Thread 版）
 *
 * 协议：0x55 0x55 | ID | 长度 | 指令 | 参数... | 校验和
 *   长度   = 3 + 参数个数（即 ID / 长度 / 指令 / 参数 的总字节数）
 *   校验和 = ~(ID + 长度 + 指令 + 参数) & 0xFF
 *
 * 与裸机版 NS800RT7P65D/Hardware/Servo.c 的报文格式完全一致，
 * 只是把「忙等 UART 寄存器」换成了 RT-Thread 的 uart 设备接口。
 */

#ifndef __APP_SERVO_H__
#define __APP_SERVO_H__

#include <rtthread.h>
#include "app_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  初始化舵机总线（查找并打开串口设备、创建互斥量）
 * @return RT_EOK 成功
 */
rt_err_t app_servo_init(void);

/**
 * @brief  在 time_ms 内把 id 号舵机转到 angle 位置
 * @param  id      舵机 ID（1~253）
 * @param  angle   目标位置 0~1000
 * @param  time_ms 运动时间（毫秒）
 */
rt_err_t app_servo_move_time(rt_uint8_t id, rt_uint16_t angle, rt_uint16_t time_ms);

/**
 * @brief  与 move_time 相同，但等上一次动作完成后再执行
 */
rt_err_t app_servo_move_time_wait(rt_uint8_t id, rt_uint16_t angle, rt_uint16_t time_ms);

/**
 * @brief  舵机扭力加载 / 卸载
 */
rt_err_t app_servo_load(rt_uint8_t id, rt_bool_t load);

/**
 * @brief  舵机自带 LED 开关
 */
rt_err_t app_servo_led(rt_uint8_t id, rt_bool_t on);

/**
 * @brief  发送一条自定义指令（调试用）
 * @param  id        舵机 ID
 * @param  cmd       指令码
 * @param  params    参数缓冲区，可为 RT_NULL
 * @param  param_len 参数个数
 */
rt_err_t app_servo_send_cmd(rt_uint8_t id, rt_uint8_t cmd,
                            const rt_uint8_t *params, rt_uint8_t param_len);

#ifdef __cplusplus
}
#endif

#endif /* __APP_SERVO_H__ */
