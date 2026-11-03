/*
 * main.c —— 应用入口（RT-Thread 版，替换官方 BSP 的 applications/main.c）
 *
 * 启动流程：
 *   main() → 逐个初始化各驱动 → 启动业务层（UART2 收指令 + 动作线程）→ 常驻
 *
 * 硬件初始化全部交给 RT-Thread 的設備框架（uart / pin / pwm 设备），
 * 不再像裸机版那样手工调用 Device_init()、Interrupt_initModule() 等，
 * 这些已经由 BSP 的 board.c 在 rt_hw_board_init() 里完成。
 */

#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>

#include "app_config.h"
#include "app_servo.h"
#include "app_stepper.h"
#include "app_dcmotor.h"
#include "app_vibration.h"
#include "app_ultrasonic.h"
#include "app_control.h"

#define DBG_TAG    "app.main"
#define DBG_LVL    DBG_INFO
#include <rtdbg.h>

static void app_banner(void)
{
    rt_kprintf("\n");
    rt_kprintf("============================================================\n");
    rt_kprintf("  花卉授粉机器人 · 执行端（RT-Thread 版）\n");
    rt_kprintf("  MCU : NS800RT7P65D (Cortex-M7, 双核)\n");
    rt_kprintf("  OS  : RT-Thread %d.%d.%d\n", RT_VERSION, RT_SUBVERSION, RT_REVISION);
    rt_kprintf("  编译: %s %s\n", __DATE__, __TIME__);
    rt_kprintf("------------------------------------------------------------\n");
    rt_kprintf("  K230 指令串口 : %s @ %d 8N1\n", APP_K230_UART_NAME, APP_K230_UART_BAUD);
    rt_kprintf("  总线舵机串口  : %s @ %d 8N1\n", APP_SERVO_UART_NAME, APP_SERVO_UART_BAUD);
    rt_kprintf("  指令协议      : one / two / three 选模式，\"角度,Y\" 执行作业\n");
    rt_kprintf("============================================================\n");
}

int main(void)
{
    rt_err_t ret;

    app_banner();

    /* 各驱动独立初始化：某个模块失败时只告警，不阻断其它模块，
     * 方便在实物上逐项排查（用 msh 命令可以单独测试每个模块）。 */
    ret = app_servo_init();
    if (ret != RT_EOK)
    {
        LOG_E("总线舵机初始化失败: %d", ret);
    }

    ret = app_stepper_init();
    if (ret != RT_EOK)
    {
        LOG_E("步进电机初始化失败: %d", ret);
    }

    ret = app_dcmotor_init();
    if (ret != RT_EOK)
    {
        LOG_E("直流电机初始化失败: %d", ret);
    }

    ret = app_vibration_init();
    if (ret != RT_EOK)
    {
        LOG_E("振动电机初始化失败: %d", ret);
    }

    ret = app_ultrasonic_init();
    if (ret != RT_EOK)
    {
        LOG_E("超声波初始化失败: %d", ret);
    }

    ret = app_control_start();
    if (ret != RT_EOK)
    {
        LOG_E("业务层启动失败: %d", ret);
    }

    rt_kprintf("\n[main] 系统就绪，等待 K230 下发指令...\n");
    rt_kprintf("[main] 输入 help 查看调试命令（mode / act / dist / servo / dc / vib / step / status / selftest）\n\n");

    /* 主线程常驻；所有业务都在 rx / action 两个线程里跑 */
    while (1)
    {
        rt_thread_mdelay(1000);
    }

    return 0;
}
