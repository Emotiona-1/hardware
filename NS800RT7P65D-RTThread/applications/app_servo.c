/*
 * app_servo.c —— 总线舵机驱动实现（RT-Thread 版）
 *
 * 报文与校验算法与裸机版一致，改动点：
 *   - 串口操作改为 rt_device_write()，不再直接操作 UART 寄存器；
 *   - 用互斥量保护发送过程，避免多线程同时写总线打乱报文；
 *   - 发送前的 GPIO 复用、波特率等全部由 BSP 的 uart 驱动完成。
 */

#include "app_servo.h"

#define DBG_TAG    "app.servo"
#define DBG_LVL    DBG_INFO
#include <rtdbg.h>

/* 一帧最长：2 字节帧头 + 1 字节 ID + 1 字节长度 + 1 字节指令 + 6 字节参数 + 1 字节校验 */
#define SERVO_TX_BUF_MAX    16U
#define SERVO_PARAM_MAX     6U

static rt_device_t      s_servo_uart = RT_NULL;
static struct rt_mutex  s_servo_lock;

static rt_err_t servo_ensure_ready(void)
{
    if (s_servo_uart == RT_NULL)
    {
        LOG_E("舵机串口未初始化，请先调用 app_servo_init()");
        return -RT_ERROR;
    }

    return RT_EOK;
}

static rt_err_t servo_uart_config(void)
{
    struct serial_configure cfg = RT_SERIAL_CONFIG_DEFAULT;

    cfg.baud_rate = APP_SERVO_UART_BAUD;
    cfg.data_bits = DATA_BITS_8;
    cfg.stop_bits = STOP_BITS_1;
    cfg.parity    = PARITY_NONE;

    return rt_device_control(s_servo_uart, RT_DEVICE_CTRL_CONFIG, &cfg);
}

rt_err_t app_servo_init(void)
{
    rt_err_t ret;

    s_servo_uart = rt_device_find(APP_SERVO_UART_NAME);
    if (s_servo_uart == RT_NULL)
    {
        LOG_E("找不到串口设备 %s，请检查 menuconfig 是否开启了对应的 UART",
              APP_SERVO_UART_NAME);
        return -RT_ERROR;
    }

    ret = rt_device_open(s_servo_uart, RT_DEVICE_FLAG_RDWR);
    if (ret != RT_EOK)
    {
        LOG_E("打开 %s 失败: %d", APP_SERVO_UART_NAME, ret);
        s_servo_uart = RT_NULL;
        return ret;
    }

    ret = servo_uart_config();
    if (ret != RT_EOK)
    {
        LOG_W("配置 %s 波特率失败: %d（BSP 默认 115200，可忽略）",
              APP_SERVO_UART_NAME, ret);
    }

    ret = rt_mutex_init(&s_servo_lock, "servo", RT_IPC_FLAG_PRIO);
    if (ret != RT_EOK)
    {
        LOG_E("创建舵机互斥量失败: %d", ret);
        rt_device_close(s_servo_uart);
        s_servo_uart = RT_NULL;
        return ret;
    }

    LOG_I("舵机总线就绪: %s @ %d", APP_SERVO_UART_NAME, APP_SERVO_UART_BAUD);
    return RT_EOK;
}

rt_err_t app_servo_send_cmd(rt_uint8_t id, rt_uint8_t cmd,
                            const rt_uint8_t *params, rt_uint8_t param_len)
{
    rt_uint8_t buf[SERVO_TX_BUF_MAX];
    rt_uint8_t idx = 0;
    rt_uint8_t sum = 0;
    rt_uint8_t i;
    rt_size_t  written;
    rt_err_t   ret;

    if (param_len > SERVO_PARAM_MAX)
    {
        return -RT_EINVAL;
    }

    ret = servo_ensure_ready();
    if (ret != RT_EOK)
    {
        return ret;
    }

    /* 帧头 */
    buf[idx++] = APP_SERVO_FRAME_HEADER;
    buf[idx++] = APP_SERVO_FRAME_HEADER;

    /* 负载：ID / 长度 / 指令 / 参数 */
    buf[idx++] = id;
    buf[idx++] = (rt_uint8_t)(3U + param_len);
    buf[idx++] = cmd;

    for (i = 0; i < param_len; i++)
    {
        buf[idx++] = params[i];
    }

    /* 校验和：负载部分求和取反 */
    for (i = 2; i < idx; i++)
    {
        sum = (rt_uint8_t)(sum + buf[i]);
    }
    buf[idx++] = (rt_uint8_t)(~sum);

    rt_mutex_take(&s_servo_lock, RT_WAITING_FOREVER);
    written = rt_device_write(s_servo_uart, 0, buf, idx);
    rt_mutex_release(&s_servo_lock);

    if (written != idx)
    {
        LOG_W("舵机报文发送不完整: %d/%d", (int)written, (int)idx);
        return -RT_ERROR;
    }

    return RT_EOK;
}

rt_err_t app_servo_move_time(rt_uint8_t id, rt_uint16_t angle, rt_uint16_t time_ms)
{
    rt_uint8_t params[4];

    params[0] = (rt_uint8_t)(angle & 0xFFU);
    params[1] = (rt_uint8_t)((angle >> 8) & 0xFFU);
    params[2] = (rt_uint8_t)(time_ms & 0xFFU);
    params[3] = (rt_uint8_t)((time_ms >> 8) & 0xFFU);

    return app_servo_send_cmd(id, APP_SERVO_CMD_MOVE_TIME_WRITE, params, 4);
}

rt_err_t app_servo_move_time_wait(rt_uint8_t id, rt_uint16_t angle, rt_uint16_t time_ms)
{
    rt_uint8_t params[4];

    params[0] = (rt_uint8_t)(angle & 0xFFU);
    params[1] = (rt_uint8_t)((angle >> 8) & 0xFFU);
    params[2] = (rt_uint8_t)(time_ms & 0xFFU);
    params[3] = (rt_uint8_t)((time_ms >> 8) & 0xFFU);

    return app_servo_send_cmd(id, APP_SERVO_CMD_MOVE_TIME_WAIT_WRITE, params, 4);
}

rt_err_t app_servo_load(rt_uint8_t id, rt_bool_t load)
{
    rt_uint8_t params[1];

    params[0] = load ? 1U : 0U;

    return app_servo_send_cmd(id, APP_SERVO_CMD_LOAD_OR_UNLOAD_WRITE, params, 1);
}

rt_err_t app_servo_led(rt_uint8_t id, rt_bool_t on)
{
    rt_uint8_t params[1];

    params[0] = on ? 1U : 0U;

    return app_servo_send_cmd(id, APP_SERVO_CMD_LED_CTRL_WRITE, params, 1);
}
