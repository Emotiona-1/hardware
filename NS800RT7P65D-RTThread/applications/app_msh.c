/*
 * app_msh.c —— msh 调试命令（RT-Thread 版）
 *
 * 这些命令既是联调工具，也是验收演示时的「逐模块展示」入口：
 * 评委可以现场输入命令，单独驱动每一个执行器，而不必等 K230 下发指令。
 *
 * 依赖 rtconfig.h 中的 RT_USING_FINSH / FINSH_USING_MSH（BSP 默认已开启）。
 */

#include <rtthread.h>
#include <stdlib.h>     /* strtol / atof */
#include <string.h>     /* strchr */

#include "app_config.h"
#include "app_servo.h"
#include "app_stepper.h"
#include "app_dcmotor.h"
#include "app_vibration.h"
#include "app_ultrasonic.h"
#include "app_control.h"

#ifdef FINSH_USING_MSH
#include <finsh.h>

/* ────────── mode：设定作业模式 ────────── */
static int cmd_mode(int argc, char **argv)
{
    if (argc < 2)
    {
        rt_kprintf("用法: mode <one|two|three>\n");
        rt_kprintf("  当前模式: %s\n", app_mode_name(app_control_get_mode()));
        return -1;
    }

    if (rt_strcmp(argv[1], "one") == 0)
    {
        app_control_set_mode(APP_MODE_VIBRATION);
    }
    else if (rt_strcmp(argv[1], "two") == 0)
    {
        app_control_set_mode(APP_MODE_BLOW);
    }
    else if (rt_strcmp(argv[1], "three") == 0)
    {
        app_control_set_mode(APP_MODE_TOUCH);
    }
    else
    {
        rt_kprintf("未知模式: %s（只支持 one / two / three）\n", argv[1]);
        return -1;
    }

    rt_kprintf("当前模式: %s\n", app_mode_name(app_control_get_mode()));
    return 0;
}
MSH_CMD_EXPORT_ALIAS(cmd_mode, mode, "设定作业模式: mode <one|two|three>");

/* ────────── act：投递一次坐标动作 ────────── */
static int cmd_act(int argc, char **argv)
{
    rt_int16_t angle;
    rt_int16_t y;
    char *comma;
    long val;

    if (argc < 2)
    {
        rt_kprintf("用法: act <角度>[,<y>]   例如: act 30,120\n");
        return -1;
    }

    comma = strchr(argv[1], ',');
    if (comma != RT_NULL)
    {
        *comma = '\0';
        y = (rt_int16_t)strtol(comma + 1, RT_NULL, 10);
    }
    else
    {
        y = 0;
    }

    val = strtol(argv[1], RT_NULL, 10);
    if (val > 90)
    {
        val = 90;
    }
    else if (val < -90)
    {
        val = -90;
    }
    angle = (rt_int16_t)val;

    if (app_control_get_mode() == APP_MODE_NONE)
    {
        rt_kprintf("提示: 当前未选择作业模式，动作会只回 noMode。请先执行 mode one/two/three\n");
    }

    return (app_control_request_coordinate(angle, y) == RT_EOK) ? 0 : -1;
}
MSH_CMD_EXPORT_ALIAS(cmd_act, act, "执行一次作业: act <角度>[,<y>]");

/* ────────── dist：连续测距 ────────── */
static int cmd_dist(int argc, char **argv)
{
    int times = (argc >= 2) ? (int)strtol(argv[1], RT_NULL, 10) : 5;
    int i;

    if (times <= 0 || times > 50)
    {
        times = 5;
    }

    for (i = 0; i < times; i++)
    {
        float d = app_ultrasonic_get_distance_cm();

        rt_kprintf("[%d] 距离 = %.1f cm, 原始计数 = %u\n",
                   i + 1, (double)d, (unsigned)app_ultrasonic_get_last_ticks());
        rt_thread_mdelay(200);
    }

    return 0;
}
MSH_CMD_EXPORT_ALIAS(cmd_dist, dist, "连续测距: dist [次数]");

/* ────────── servo：单独驱动一路舵机 ────────── */
static int cmd_servo(int argc, char **argv)
{
    int   id;
    int   pos;
    int   ms = 1000;

    if (argc < 3)
    {
        rt_kprintf("用法: servo <id> <位置0~1000> [时间ms]\n");
        return -1;
    }

    id  = (int)strtol(argv[1], RT_NULL, 10);
    pos = (int)strtol(argv[2], RT_NULL, 10);
    if (argc >= 4)
    {
        ms = (int)strtol(argv[3], RT_NULL, 10);
    }

    if (pos < 0)   { pos = 0; }
    if (pos > 1000) { pos = 1000; }

    if (app_servo_move_time((rt_uint8_t)id, (rt_uint16_t)pos, (rt_uint16_t)ms) != RT_EOK)
    {
        rt_kprintf("发送失败\n");
        return -1;
    }

    rt_kprintf("舵机 %d → %d（%d ms）\n", id, pos, ms);
    return 0;
}
MSH_CMD_EXPORT_ALIAS(cmd_servo, servo, "驱动舵机: servo <id> <位置> [时间ms]");

/* ────────── dc：直流电机（吹风） ────────── */
static int cmd_dc(int argc, char **argv)
{
    int pct;

    if (argc < 2)
    {
        rt_kprintf("用法: dc <-100~100>   当前 = %d%%\n", app_dcmotor_get_speed());
        return -1;
    }

    pct = (int)strtol(argv[1], RT_NULL, 10);

    if (app_dcmotor_set_speed((rt_int16_t)pct) != RT_EOK)
    {
        rt_kprintf("设置失败\n");
        return -1;
    }

    rt_kprintf("直流电机 = %d%%\n", app_dcmotor_get_speed());
    return 0;
}
MSH_CMD_EXPORT_ALIAS(cmd_dc, dc, "直流电机占空比: dc <-100~100>");

/* ────────── vib：振动电机 ────────── */
static int cmd_vib(int argc, char **argv)
{
    int ms = (argc >= 2) ? (int)strtol(argv[1], RT_NULL, 10) : 1000;

    if (ms < 0)   { ms = 0; }
    if (ms > 20000) { ms = 20000; }

    rt_kprintf("振动 %d ms...\n", ms);
    app_vibration_pulse((rt_uint32_t)ms);

    return 0;
}
MSH_CMD_EXPORT_ALIAS(cmd_vib, vib, "振动电机: vib [毫秒]");

/* ────────── step：步进电机转到指定角度 ────────── */
static int cmd_step(int argc, char **argv)
{
    int deg;

    if (argc < 2)
    {
        rt_kprintf("用法: step <角度>   当前角度 = %d°\n", app_stepper_get_angle());
        return -1;
    }

    deg = (int)strtol(argv[1], RT_NULL, 10);

    app_stepper_track_x((rt_int16_t)deg);

    rt_kprintf("步进电机当前角度 = %d°\n", app_stepper_get_angle());
    return 0;
}
MSH_CMD_EXPORT_ALIAS(cmd_step, step, "步进电机定位: step <角度>");

/* ────────── status：状态总览 ────────── */
static int cmd_status(int argc, char **argv)
{
    RT_UNUSED(argc);
    RT_UNUSED(argv);

    rt_kprintf("── 执行端状态 ──\n");
    rt_kprintf("  作业模式 : %s\n", app_mode_name(app_control_get_mode()));
    rt_kprintf("  动作线程 : %s\n", app_control_is_busy() ? "执行中" : "空闲");
    rt_kprintf("  步进角度 : %d°\n", app_stepper_get_angle());
    rt_kprintf("  直流电机 : %d%%\n", app_dcmotor_get_speed());
    rt_kprintf("  振动电机 : %s\n", app_vibration_is_on() ? "开" : "关");
    rt_kprintf("  测距系数 : %.3f\n", (double)app_ultrasonic_get_gain());
    rt_kprintf("  舵机串口 : %s\n", APP_SERVO_UART_NAME);
    rt_kprintf("  K230 串口: %s\n", APP_K230_UART_NAME);
    return 0;
}
MSH_CMD_EXPORT_ALIAS(cmd_status, status, "查看执行端状态");

/* ────────── us_cal：测距校准 ────────── */
static int cmd_us_cal(int argc, char **argv)
{
    float gain;

    if (argc < 2)
    {
        rt_kprintf("当前测距校准系数 = %.3f\n", (double)app_ultrasonic_get_gain());
        rt_kprintf("用法: us_cal <系数>   例如实测 101cm 实际 100cm，则填 0.99\n");
        return -1;
    }

    gain = (float)atof(argv[1]);
    app_ultrasonic_set_gain(gain);
    rt_kprintf("测距校准系数 = %.3f\n", (double)app_ultrasonic_get_gain());
    return 0;
}
MSH_CMD_EXPORT_ALIAS(cmd_us_cal, us_cal, "测距校准: us_cal <系数>");

/* ────────── selftest：上电自检（对应裸机版 System_Init 的自检流程） ────────── */
static int cmd_selftest(int argc, char **argv)
{
    float d;
    int i;

    RT_UNUSED(argc);
    RT_UNUSED(argv);

    rt_kprintf("=== 开始自检 ===\n");

    /* 1. 三路舵机上电 */
    for (i = 1; i <= 3; i++)
    {
        app_servo_load((rt_uint8_t)i, RT_TRUE);
        rt_thread_mdelay(100);
    }
    rt_kprintf("[1/6] 三路舵机已上电\n");

    /* 2. 舵机行程 */
    app_servo_move_time(APP_SERVO_ID_ARM1, 400, 500);
    rt_thread_mdelay(600);
    app_servo_move_time(APP_SERVO_ID_ARM2, 600, 1000);
    rt_thread_mdelay(1100);
    app_servo_move_time(APP_SERVO_ID_ARM2, 400, 1000);
    rt_thread_mdelay(1100);
    app_servo_move_time(APP_SERVO_ID_TELESCOPIC, 1000, 1000);
    rt_thread_mdelay(1100);
    app_servo_move_time(APP_SERVO_ID_TELESCOPIC, 0, 1000);
    rt_thread_mdelay(1100);
    rt_kprintf("[2/6] 舵机行程测试完成\n");

    /* 3. 振动电机 */
    app_vibration_pulse(1000);
    rt_kprintf("[3/6] 振动电机测试完成\n");

    /* 4. 超声波 */
    d = app_ultrasonic_get_distance_cm();
    rt_kprintf("[4/6] 测距 = %.1f cm\n", (double)d);

    /* 5. 直流电机 */
    app_dcmotor_set_speed(50);
    rt_thread_mdelay(1000);
    app_dcmotor_set_speed(0);
    rt_kprintf("[5/6] 直流电机测试完成\n");

    /* 6. 步进电机 */
    app_stepper_track_x(10);
    rt_thread_mdelay(200);
    app_stepper_track_x(0);
    rt_kprintf("[6/6] 步进电机测试完成\n");

    rt_kprintf("=== 自检结束 ===\n");
    return 0;
}
MSH_CMD_EXPORT_ALIAS(cmd_selftest, selftest, "上电自检（依次驱动全部执行器）");

#endif /* FINSH_USING_MSH */
