/*
 * app_control.c —— 业务控制层实现（RT-Thread 版）
 *
 * 线程模型：
 *   ┌──────────────┐  按行解析   ┌──────────────┐
 *   │ rx 线程       │ ─────────→ │ 消息队列      │
 *   │ (优先级 12)   │  one/two/   │ (深度 8)      │
 *   │               │  three 直接 │              │
 *   └──────────────┘  改模式      └──────┬───────┘
 *                                        │ 坐标指令
 *                                 ┌──────▼───────┐
 *                                 │ 动作线程      │  依次执行动作，回 workOK
 *                                 │ (优先级 15)   │
 *                                 └──────────────┘
 *
 * 与裸机版的差异（都是可讲清楚的改进）：
 *   1. 串口接收不再轮询，改为「接收中断 + 信号量」唤醒 rx 线程；
 *   2. 动作编排从主循环里挪到独立线程，主循环（main）不再被 3 秒延时卡住；
 *   3. 模式指令由 rx 线程直接生效（不必等当前动作做完），坐标指令进队列排队；
 *   4. 队列满时回 "busy"，而不是静默丢弃，避免 K230 等不到回复。
 */

#include <rtthread.h>
#include <stdlib.h>     /* strtol */
#include <string.h>     /* strchr / memcpy */

#include "app_control.h"
#include "app_servo.h"
#include "app_stepper.h"
#include "app_dcmotor.h"
#include "app_vibration.h"
#include "app_ultrasonic.h"

#define DBG_TAG    "app.ctrl"
#define DBG_LVL    DBG_INFO
#include <rtdbg.h>

/* ────────────── 命令队列 ────────────── */
typedef struct
{
    rt_int16_t angle;   /* 目标角度（度） */
    rt_int16_t y;       /* 目标姿态参数 0~200 */
} app_cmd_t;

#define APP_CMD_ALIGNED  RT_ALIGN(sizeof(app_cmd_t), RT_ALIGN_SIZE)
/* 队列里每条消息还要额外挂一个链表节点头，所以留足余量：
 * rt_mq_init() 会按 (msg_size + sizeof(struct rt_mq_message)) 切分内存池 */
#define APP_MQ_POOL_SIZE (APP_CMD_QUEUE_DEPTH * (APP_CMD_ALIGNED + 16U))

static struct rt_messagequeue s_cmd_mq;
static rt_uint8_t              s_mq_pool[APP_MQ_POOL_SIZE];

static struct rt_semaphore     s_rx_sem;

static rt_device_t             s_k230_uart = RT_NULL;
static struct rt_mutex         s_tx_lock;

static volatile rt_uint8_t     s_mode  = APP_MODE_NONE;
static volatile rt_bool_t      s_busy  = RT_FALSE;

static char                    s_line[APP_RX_LINE_MAX];
static rt_uint8_t              s_line_len = 0;

/* ────────────── 发送回复 ────────────── */
rt_err_t app_control_reply(const char *text)
{
    rt_size_t len;

    if (s_k230_uart == RT_NULL || text == RT_NULL)
    {
        return -RT_ERROR;
    }

    len = rt_strlen(text);

    rt_mutex_take(&s_tx_lock, RT_WAITING_FOREVER);
    rt_device_write(s_k230_uart, 0, text, len);
    rt_mutex_release(&s_tx_lock);

    return RT_EOK;
}

const char *app_mode_name(rt_uint8_t mode)
{
    switch (mode)
    {
    case APP_MODE_VIBRATION: return "振动(one)";
    case APP_MODE_BLOW:      return "吹风(two)";
    case APP_MODE_TOUCH:     return "触摸(three)";
    default:                 return "未选择";
    }
}

rt_err_t app_control_set_mode(rt_uint8_t mode)
{
    rt_base_t level;

    if (mode > APP_MODE_TOUCH)
    {
        return -RT_EINVAL;
    }

    level = rt_hw_interrupt_disable();
    s_mode = mode;
    rt_hw_interrupt_enable(level);

    LOG_I("作业模式 → %s", app_mode_name(mode));
    return RT_EOK;
}

rt_uint8_t app_control_get_mode(void)
{
    return s_mode;
}

rt_bool_t app_control_is_busy(void)
{
    return s_busy;
}

rt_err_t app_control_request_coordinate(rt_int16_t angle, rt_int16_t y)
{
    app_cmd_t cmd;

    cmd.angle = angle;
    cmd.y     = y;

    if (rt_mq_send(&s_cmd_mq, &cmd, sizeof(cmd)) != RT_EOK)
    {
        LOG_W("动作队列已满，丢弃指令 (%d,%d)", angle, y);
        app_control_reply("busy\r\n");
        return -RT_EFULL;
    }

    LOG_I("坐标指令入队: 角度=%d, Y=%d", angle, y);
    return RT_EOK;
}

/* ────────────── 动作编排（对应裸机版 Control.c） ────────────── */

static void action_move_by_y(rt_int16_t y)
{
    rt_int16_t arm1;
    rt_int16_t arm2;

    if (y < 0)
    {
        y = 0;
    }
    else if (y > (rt_int16_t)APP_SERVO_Y_MAX)
    {
        y = (rt_int16_t)APP_SERVO_Y_MAX;
    }

    arm1 = (rt_int16_t)((rt_int32_t)APP_SERVO_ARM1_BASE
                        - ((rt_int32_t)y * (rt_int32_t)APP_SERVO_Y_ARM1_SPAN)
                          / (rt_int32_t)APP_SERVO_Y_MAX);

    arm2 = (rt_int16_t)((rt_int32_t)APP_SERVO_ARM2_BASE
                        - (rt_int32_t)y * (rt_int32_t)APP_SERVO_Y_ARM2_COEF
                        + (rt_int32_t)APP_SERVO_Y_ARM2_OFFSET);

    app_servo_move_time(APP_SERVO_ID_ARM1, (rt_uint16_t)arm1, APP_ARM_MOVE_MS);
    app_servo_move_time(APP_SERVO_ID_ARM2, (rt_uint16_t)arm2, APP_ARM_MOVE_MS);

    rt_thread_mdelay(APP_ARM_SETTLE_MS);
}

static void action_move_by_distance(float distance)
{
    rt_uint16_t pos;

    if (distance <= 0.0f)
    {
        distance = 0.0f;
    }
    else if (distance > APP_TELESCOPIC_DIST_MAX_CM)
    {
        distance = APP_TELESCOPIC_DIST_MAX_CM;
    }

    pos = (rt_uint16_t)(distance * APP_TELESCOPIC_POS_PER_CM);

    LOG_I("伸缩舵机 → %d 位置（距离 %.1f cm）", pos, (double)distance);

    app_servo_move_time(APP_SERVO_ID_TELESCOPIC, pos, APP_TELESCOPIC_MOVE_MS);
    rt_thread_mdelay(APP_TELESCOPIC_HOLD_MS);
}

static void action_mode_vibration(void)
{
    float distance;

    rt_thread_mdelay(APP_MODE_PREPARE_MS);

    distance = app_ultrasonic_get_distance_cm();
    LOG_I("测距结果 %.1f cm", (double)distance);

    action_move_by_distance(distance);

    app_vibration_pulse(APP_VIBRATE_ON_MS);
    LOG_I("振动完成");
}

static void action_mode_blow(void)
{
    rt_thread_mdelay(APP_MODE_PREPARE_MS);

    app_dcmotor_set_speed(APP_BLOW_DUTY_PCT);
    rt_thread_mdelay(APP_BLOW_ON_MS);
    app_dcmotor_set_speed(0);

    LOG_I("吹风完成");
}

static void action_mode_touch(void)
{
    float distance;

    rt_thread_mdelay(APP_MODE_PREPARE_MS);

    distance = app_ultrasonic_get_distance_cm();
    LOG_I("测距结果 %.1f cm", (double)distance);

    action_move_by_distance(distance);
    LOG_I("触摸完成");
}

static void action_return_home(void)
{
    /* 伸缩归零 */
    app_servo_move_time(APP_SERVO_ID_TELESCOPIC, 0, APP_TELESCOPIC_MOVE_MS);
    rt_thread_mdelay(APP_HOME_WAIT_MS);

    /* 姿态归位 */
    app_servo_move_time(APP_SERVO_ID_ARM1, APP_SERVO_ARM1_BASE, APP_ARM_HOME_MS);
    app_servo_move_time(APP_SERVO_ID_ARM2, APP_SERVO_ARM2_BASE, APP_ARM_HOME_MS);
    rt_thread_mdelay(APP_HOME_WAIT_MS);

    /* 水平回零 */
    app_stepper_track_x(0);
    rt_thread_mdelay(APP_HOME_WAIT_MS);

    LOG_I("已回原点");
}

static void action_run_mode(rt_uint8_t mode)
{
    switch (mode)
    {
    case APP_MODE_VIBRATION: action_mode_vibration(); break;
    case APP_MODE_BLOW:      action_mode_blow();      break;
    case APP_MODE_TOUCH:     action_mode_touch();     break;
    default:                                          break;
    }
}

static void action_execute_coordinate(rt_int16_t angle, rt_int16_t y)
{
    rt_uint8_t mode_snapshot;

    /* 模式在动作开始时取一次快照，动作过程中改模式不影响本次动作 */
    mode_snapshot = s_mode;

    if (mode_snapshot == APP_MODE_NONE)
    {
        LOG_W("尚未选择作业模式，忽略坐标 (%d,%d)", angle, y);
        app_control_reply(APP_REPLY_NO_MODE);
        return;
    }

    s_busy = RT_TRUE;
    LOG_I("=== 开始作业: 模式=%s 角度=%d Y=%d ===",
          app_mode_name(mode_snapshot), angle, y);

    /* 1. 水平定位 */
    app_stepper_track_x(angle);

    /* 2. 姿态到位 */
    action_move_by_y(y);

    /* 3. 执行作业动作 */
    action_run_mode(mode_snapshot);

    /* 4. 回原点 */
    action_return_home();

    s_busy = RT_FALSE;

    app_control_reply(APP_REPLY_WORK_OK);
    LOG_I("=== 作业完成，已回复 workOK ===");
}

/* ────────────── 动作线程 ────────────── */
static void action_thread_entry(void *parameter)
{
    app_cmd_t cmd;

    RT_UNUSED(parameter);

    while (1)
    {
        if (rt_mq_recv(&s_cmd_mq, &cmd, sizeof(cmd), APP_ACTION_WAIT_TICK) == RT_EOK)
        {
            action_execute_coordinate(cmd.angle, cmd.y);
        }
    }
}

/* ────────────── 指令解析 ────────────── */

/* 把一个字符串解析成 [-90, 90] 的角度，失败返回 RT_FALSE */
static rt_bool_t parse_angle(const char *text, rt_int16_t *angle)
{
    char *end = RT_NULL;
    long  val;

    val = strtol(text, &end, 10);

    if (end == text)
    {
        return RT_FALSE;
    }

    if (val > 90)
    {
        val = 90;
    }
    else if (val < -90)
    {
        val = -90;
    }

    *angle = (rt_int16_t)val;
    return RT_TRUE;
}

/* 解析 "角度,Y" 或 "角度"，Y 省略时用上一次的值 */
static rt_bool_t parse_coordinate(const char *line, rt_int16_t *angle, rt_int16_t *y)
{
    static rt_int16_t s_last_y = 0;

    char *comma = strchr(line, ',');
    long  yval;

    if (comma == RT_NULL)
    {
        if (!parse_angle(line, angle))
        {
            return RT_FALSE;
        }

        *y = s_last_y;
        return RT_TRUE;
    }

    /* 把逗号临时截断，先解析角度 */
    {
        char angle_buf[16];
        rt_size_t len = (rt_size_t)(comma - line);

        if (len >= sizeof(angle_buf))
        {
            len = sizeof(angle_buf) - 1U;
        }

        rt_memcpy(angle_buf, line, len);
        angle_buf[len] = '\0';

        if (!parse_angle(angle_buf, angle))
        {
            return RT_FALSE;
        }
    }

    yval = strtol(comma + 1, RT_NULL, 10);

    if (yval < 0)
    {
        yval = 0;
    }
    else if (yval > 32767)
    {
        yval = 32767;
    }

    s_last_y = (rt_int16_t)yval;
    *y = s_last_y;

    return RT_TRUE;
}

static void handle_line(char *line)
{
    rt_int16_t angle;
    rt_int16_t y;

    if (line[0] == '\0')
    {
        return;
    }

    LOG_D("收到指令: \"%s\"", line);

    if (rt_strcmp(line, "one") == 0)
    {
        app_control_set_mode(APP_MODE_VIBRATION);
        return;
    }

    if (rt_strcmp(line, "two") == 0)
    {
        app_control_set_mode(APP_MODE_BLOW);
        return;
    }

    if (rt_strcmp(line, "three") == 0)
    {
        app_control_set_mode(APP_MODE_TOUCH);
        return;
    }

    if (parse_coordinate(line, &angle, &y))
    {
        app_control_request_coordinate(angle, y);
        return;
    }

    LOG_W("无法识别的指令，已忽略: \"%s\"", line);
}

static void feed_byte(char ch)
{
    if (ch == '\n' || ch == '\r')
    {
        if (s_line_len > 0U)
        {
            s_line[s_line_len] = '\0';
            handle_line(s_line);
            s_line_len = 0U;
        }
        return;
    }

    if (s_line_len < (APP_RX_LINE_MAX - 1U))
    {
        s_line[s_line_len++] = ch;
    }
    else
    {
        /* 超长行丢弃，与裸机版一致 */
        s_line_len = 0U;
    }
}

/* ────────────── 接收线程 ────────────── */
static rt_err_t uart_rx_ind(rt_device_t dev, rt_size_t size)
{
    RT_UNUSED(dev);
    RT_UNUSED(size);

    return rt_sem_release(&s_rx_sem);
}

static void rx_thread_entry(void *parameter)
{
    rt_uint8_t buf[32];
    rt_size_t  len;
    rt_size_t  i;

    RT_UNUSED(parameter);

    while (1)
    {
        if (rt_sem_take(&s_rx_sem, rt_tick_from_millisecond(100)) != RT_EOK)
        {
            continue;
        }

        while ((len = rt_device_read(s_k230_uart, 0, buf, sizeof(buf))) > 0)
        {
            for (i = 0; i < len; i++)
            {
                feed_byte((char)buf[i]);
            }
        }
    }
}

/* ────────────── 启动 ────────────── */
rt_err_t app_control_start(void)
{
    struct serial_configure cfg = RT_SERIAL_CONFIG_DEFAULT;
    rt_thread_t tid;
    rt_err_t    ret;

    /* 消息队列 */
    ret = rt_mq_init(&s_cmd_mq, "appcmd", s_mq_pool, APP_CMD_ALIGNED,
                     sizeof(s_mq_pool), RT_IPC_FLAG_PRIO);
    if (ret != RT_EOK)
    {
        LOG_E("消息队列初始化失败: %d", ret);
        return ret;
    }

    /* 信号量与互斥量 */
    rt_sem_init(&s_rx_sem, "apprx", 0, RT_IPC_FLAG_PRIO);
    rt_mutex_init(&s_tx_lock, "apptx", RT_IPC_FLAG_PRIO);

    /* K230 串口 */
    s_k230_uart = rt_device_find(APP_K230_UART_NAME);
    if (s_k230_uart == RT_NULL)
    {
        LOG_E("找不到串口 %s，请检查 menuconfig 是否打开 BSP_USING_UART2",
              APP_K230_UART_NAME);
        return -RT_ERROR;
    }

    ret = rt_device_open(s_k230_uart, RT_DEVICE_FLAG_INT_RX | RT_DEVICE_OFLAG_RDWR);
    if (ret != RT_EOK)
    {
        LOG_E("打开 %s 失败: %d", APP_K230_UART_NAME, ret);
        s_k230_uart = RT_NULL;
        return ret;
    }

    cfg.baud_rate = APP_K230_UART_BAUD;
    (void)rt_device_control(s_k230_uart, RT_DEVICE_CTRL_CONFIG, &cfg);
    rt_device_set_rx_indicate(s_k230_uart, uart_rx_ind);

    /* 接收线程 */
    tid = rt_thread_create("app_rx", rx_thread_entry, RT_NULL,
                           APP_THREAD_STACK_RX, APP_THREAD_PRIO_RX, 10);
    if (tid == RT_NULL)
    {
        LOG_E("创建接收线程失败");
        return -RT_ENOMEM;
    }
    rt_thread_startup(tid);

    /* 动作线程 */
    tid = rt_thread_create("app_act", action_thread_entry, RT_NULL,
                           APP_THREAD_STACK_ACTION, APP_THREAD_PRIO_ACTION, 10);
    if (tid == RT_NULL)
    {
        LOG_E("创建动作线程失败");
        return -RT_ENOMEM;
    }
    rt_thread_startup(tid);

    LOG_I("业务层启动完成: K230 串口 %s @ %d", APP_K230_UART_NAME,
          APP_K230_UART_BAUD);
    return RT_EOK;
}
