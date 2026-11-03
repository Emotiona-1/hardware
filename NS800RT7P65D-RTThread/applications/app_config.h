/*
 * app_config.h —— 执行端应用层集中配置（RT-Thread 版）
 *
 * 所有引脚、串口、时长、舵机行程等参数都集中在这里，改硬件只需要改本文件。
 * 引脚编号统一使用厂商 SDK 的 GPIO_xx 宏（PA12 = GPIO_12，PB6 = GPIO_38，即
 * 「端口序号 × 32 + 引脚序号」），再经 BSP 的 PIN_NUM() 转换成 rt_pin_* 使用的编号。
 *
 * 对应官方 BSP：bsp/novosns/ns800/ns800rt7p65-nssinepad
 */

#ifndef __APP_CONFIG_H__
#define __APP_CONFIG_H__

#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>

/* ════════════════════════════════════════════════════════════════
 * 1. 串口分工
 *
 *   官方 BSP 默认把控制台放在 UART1（PA12/PA13），而本项目的总线舵机
 *   正好也接在 PA12/PA13 上 —— 两者不能共用，必须错开。这里提供两个方案：
 *
 *   方案 A（默认，推荐）：舵机总线 = UART1（与现有接线一致）
 *                        控制台   = UART3（PB24/PB25，需外接 USB-TTL）
 *                        rtconfig.h 里要改：RT_CONSOLE_DEVICE_NAME "uart3"
 *
 *   方案 B：控制台 = UART1（用板载 USB 转串口）
 *           舵机总线 = UART3（把舵机总线改接到 PB24/PB25）
 *           把下面的 APP_SERVO_ON_UART3 改成 1 即可
 * ════════════════════════════════════════════════════════════════ */
#define APP_SERVO_ON_UART3      0

#if APP_SERVO_ON_UART3
#define APP_SERVO_UART_NAME     "uart3"
#else
#define APP_SERVO_UART_NAME     "uart1"
#endif

/* 与 K230 视觉节点的通信串口（不可更改：硬件固定接 PB6/PB7） */
#define APP_K230_UART_NAME      "uart2"

#define APP_SERVO_UART_BAUD     115200
#define APP_K230_UART_BAUD      115200

/* ════════════════════════════════════════════════════════════════
 * 2. 引脚分配（与裸机版 NS800RT7P65D/Hardware 完全一致）
 * ════════════════════════════════════════════════════════════════ */
/* 步进电机：脉冲 / 方向 / 使能 */
#define APP_PIN_STEP_PUL        PIN_NUM(GPIO_2)      /* PA2  */
#define APP_PIN_STEP_DIR        PIN_NUM(GPIO_3)      /* PA3  */
#define APP_PIN_STEP_ENA        PIN_NUM(GPIO_4)      /* PA4  */

/* 超声波：TRIG 推挽输出；ECHO 由 ECAP1 捕获（PA16，ALT0，经 XBAR INPUT7） */
#define APP_PIN_US_TRIG         PIN_NUM(GPIO_5)      /* PA5  */
#define APP_ECAP_INSTANCE       ECAP1
#define APP_ECAP_IRQ            ECAP1_IRQn
#define APP_ECAP_INPUT          ECAP_INPUT_XBAR_INPUT7
#define APP_ECAP_XBAR           XBAR_INPUT7
#define APP_ECAP_ECHO_PORT      GPIOA
#define APP_ECAP_ECHO_PIN       GPIO_PIN_16          /* PA16 */

/* 直流电机（吹风）：PWM 用 EPWM1 的 A/B 两路（PA0/PA1），NSLEEP 唤醒 */
#define APP_PWM_DEV_NAME        "epwm1"
#define APP_PWM_CH_A            0                    /* EPWMX_A */
#define APP_PWM_CH_B            1                    /* EPWMX_B */
#define APP_PWM_PERIOD_US       50U                  /* 50us → 20kHz */
#define APP_PIN_DC_NSLEEP       PIN_NUM(GPIO_8)      /* PA8  */

/* 振动电机：GPIO 开关（PA14；同一引脚也可作 EPWM8_A，本应用只用开关） */
#define APP_PIN_VIBRATION       PIN_NUM(GPIO_14)     /* PA14 */

/* ════════════════════════════════════════════════════════════════
 * 3. 步进电机参数
 * ════════════════════════════════════════════════════════════════ */
#define APP_STEP_PULSE_PER_DEG  (1600.0f / 360.0f)   /* 电机一圈 1600 脉冲 */
#define APP_STEP_SUBDIV         3.0f                 /* 与裸机版一致的细分倍数 */
#define APP_STEP_DEAD_ZONE_DEG  2                    /* 小于该角度差不动 */
#define APP_STEP_HALF_US        600U                 /* 慢速模式：高低电平各 600us */
#define APP_STEP_GAP_US         500U                 /* 快速模式：脉冲间间隔 500us */

/* ════════════════════════════════════════════════════════════════
 * 4. 总线舵机参数（0x55 0x55 帧头协议）
 * ════════════════════════════════════════════════════════════════ */
#define APP_SERVO_FRAME_HEADER  0x55U

#define APP_SERVO_CMD_MOVE_TIME_WRITE       1U
#define APP_SERVO_CMD_MOVE_TIME_WAIT_WRITE  7U
#define APP_SERVO_CMD_MOVE_START            11U
#define APP_SERVO_CMD_MOVE_STOP             12U
#define APP_SERVO_CMD_ANGLE_OFFSET_ADJUST   17U
#define APP_SERVO_CMD_ANGLE_LIMIT_WRITE     20U
#define APP_SERVO_CMD_LOAD_OR_UNLOAD_WRITE  31U
#define APP_SERVO_CMD_LED_CTRL_WRITE        33U
#define APP_SERVO_CMD_ID_WRITE              13U

#define APP_SERVO_ID_ARM1       1U
#define APP_SERVO_ID_ARM2       2U
#define APP_SERVO_ID_TELESCOPIC 3U

/* 姿态与伸缩的行程（从裸机版 Control.c 的调试经验值搬过来） */
#define APP_SERVO_ARM1_BASE     350U
#define APP_SERVO_ARM2_BASE     400U
#define APP_SERVO_Y_MAX         200U
#define APP_SERVO_Y_ARM1_SPAN   350U
#define APP_SERVO_Y_ARM2_COEF   2U
#define APP_SERVO_Y_ARM2_OFFSET 110U

#define APP_TELESCOPIC_POS_MAX      1000U
#define APP_TELESCOPIC_POS_PER_CM   200.0f
#define APP_TELESCOPIC_DIST_MAX_CM  5.0f
#define APP_TELESCOPIC_MOVE_MS      1000U
#define APP_TELESCOPIC_HOLD_MS      5000U

/* ════════════════════════════════════════════════════════════════
 * 5. 动作时序（毫秒，与裸机版保持一致，便于对照验收）
 * ════════════════════════════════════════════════════════════════ */
#define APP_ARM_MOVE_MS          500U    /* 舵机 1/2 运动时间 */
#define APP_ARM_SETTLE_MS        3000U   /* 姿态到位后等待 */
#define APP_MODE_PREPARE_MS      2000U   /* 每种作业开始前的稳定延时 */
#define APP_VIBRATE_ON_MS        1000U   /* 振动持续 */
#define APP_BLOW_DUTY_PCT        100     /* 吹风占空比 */
#define APP_BLOW_ON_MS           3000U
#define APP_HOME_WAIT_MS         2000U
#define APP_ARM_HOME_MS          1000U

/* ════════════════════════════════════════════════════════════════
 * 6. 超声波参数
 * ════════════════════════════════════════════════════════════════ */
#define APP_US_TRIG_US           10U      /* TRIG 高电平宽度 */
#define APP_US_TIMEOUT_MS        120U     /* 单次测距超时 */
#define APP_US_US_PER_CM         58.0f    /* 声速换算：58us/cm */

/* ECAP 计数时钟 = SystemCoreClock / (预分频 + 1)。
 * 裸机版用预分频 0，这里沿用，保证换算系数与已验证的代码一致。 */
#define APP_ECAP_PRESCALER       0U

/* ════════════════════════════════════════════════════════════════
 * 7. 线程与队列参数
 * ════════════════════════════════════════════════════════════════ */
#define APP_THREAD_STACK_RX      1024U
#define APP_THREAD_STACK_ACTION  2048U
#define APP_THREAD_STACK_MSH     1024U

#define APP_THREAD_PRIO_RX       12
#define APP_THREAD_PRIO_ACTION   15

#define APP_CMD_QUEUE_DEPTH      8U       /* 动作队列深度 */
#define APP_RX_LINE_MAX          32U      /* 单行指令最大长度（与裸机版一致） */

/* 动作线程等待指令的超时时间 */
#define APP_ACTION_WAIT_TICK     RT_WAITING_FOREVER

/* ════════════════════════════════════════════════════════════════
 * 8. 兼容性说明
 *
 * rt_hw_us_delay() 由 BSP 的 drv_common.c 实现，声明在 RT-Thread 的
 * include/rthw.h 里，因此这里显式包含它（只包含 rtthread.h 时
 * 会出现隐式声明告警）。
 * ════════════════════════════════════════════════════════════════ */
#include <rthw.h>

/* ════════════════════════════════════════════════════════════════
 * 9. 与 K230 的报文
 * ════════════════════════════════════════════════════════════════ */
#define APP_REPLY_WORK_OK       "workOK\r\n"
#define APP_REPLY_NO_MODE       "noMode\r\n"

#endif /* __APP_CONFIG_H__ */
