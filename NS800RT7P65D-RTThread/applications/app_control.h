/*
 * app_control.h —— 业务控制层（RT-Thread 版）
 *
 * 职责：
 *   1. 接收 K230 视觉节点从 UART2 下发的 ASCII 行指令；
 *   2. 解析出「作业模式」与「目标坐标」；
 *   3. 在独立线程里按原有时序编排动作：步进定位 → 舵机姿态 → 作业动作 → 回原点；
 *   4. 回 workOK / noMode 给 K230。
 *
 * 与裸机版 Control.c 的指令协议完全一致，只是把「主循环轮询 + 阻塞延时」
 * 拆成了「RX 线程 + 动作线程 + 消息队列」。
 */

#ifndef __APP_CONTROL_H__
#define __APP_CONTROL_H__

#include <rtthread.h>
#include "app_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 作业模式（与裸机版 Control.h 的编号一致） */
#define APP_MODE_NONE       0U
#define APP_MODE_VIBRATION  1U   /* 振动 */
#define APP_MODE_BLOW       2U   /* 吹风 */
#define APP_MODE_TOUCH      3U   /* 触摸 */

/**
 * @brief  初始化 UART2、消息队列并启动 RX / 动作两个线程
 */
rt_err_t app_control_start(void);

/**
 * @brief  设置作业模式（线程安全，可在线程外调用）
 */
rt_err_t app_control_set_mode(rt_uint8_t mode);

/**
 * @brief  读取当前作业模式
 */
rt_uint8_t app_control_get_mode(void);

/**
 * @brief  投递一次坐标动作请求（不阻塞等待执行完毕）
 */
rt_err_t app_control_request_coordinate(rt_int16_t angle, rt_int16_t y);

/**
 * @brief  动作线程是否正在执行动作
 */
rt_bool_t app_control_is_busy(void);

/**
 * @brief  向 K230 回复一行文本
 */
rt_err_t app_control_reply(const char *text);

/**
 * @brief  模式编号转字符串（日志用）
 */
const char *app_mode_name(rt_uint8_t mode);

#ifdef __cplusplus
}
#endif

#endif /* __APP_CONTROL_H__ */
