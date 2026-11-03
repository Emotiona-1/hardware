/*
 * app_ultrasonic.c —— 超声波测距实现（RT-Thread 版）
 *
 * 逻辑移植自裸机版 Ultrasonic.c：
 *   1. 拉高 TRIG 10us；
 *   2. ECAP 单次捕获模式，EVENT1 = 上升沿（并复位计数器），EVENT2 = 下降沿；
 *   3. 两个时间戳之差即回波脉宽（单位：ECAP 计数）；
 *   4. 脉宽(us) / 58 得到厘米。
 *
 * RT-Thread 适配点：
 *   - 捕获中断里调用 rt_sem_release()，用 rt_interrupt_enter/leave 包裹；
 *   - 用信号量 + 超时替代裸机版的忙等轮询；
 *   - 临界区用 rt_hw_interrupt_disable/enable 替换 __disable_irq/__enable_irq。
 */

#include "app_ultrasonic.h"

#define DBG_TAG    "app.us"
#define DBG_LVL    DBG_INFO
#include <rtdbg.h>

static struct rt_semaphore s_us_done;

static volatile rt_uint32_t s_cap_rising  = 0;
static volatile rt_uint32_t s_cap_falling = 0;
static volatile rt_uint32_t s_last_ticks  = 0;
static volatile rt_uint8_t  s_cap_state   = 0;

static float s_gain = 1.0f;
static rt_bool_t s_inited = RT_FALSE;

/* ────────────── 内部函数 ────────────── */

static rt_uint32_t us_ticks_per_us(void)
{
    /* ECAP 计数时钟 = SystemCoreClock / (预分频 + 1) */
    rt_uint32_t ticks = (SystemCoreClock / (APP_ECAP_PRESCALER + 1U)) / 1000000U;

    return (ticks == 0U) ? 1U : ticks;
}

static void us_tx_trigger(void)
{
    rt_pin_write(APP_PIN_US_TRIG, PIN_HIGH);
    rt_hw_us_delay(APP_US_TRIG_US);
    rt_pin_write(APP_PIN_US_TRIG, PIN_LOW);
}

/* 重新装载一次单次捕获 */
static void us_arm(void)
{
    ECAP_disableInterrupt(APP_ECAP_INSTANCE, ECAP_ECEINT_CEVT1_M | ECAP_ECEINT_CEVT2_M);
    ECAP_clearInterrupt(APP_ECAP_INSTANCE, ECAP_ECCLR_CEVT1_M | ECAP_ECCLR_CEVT2_M);
    ECAP_clearGlobalInterrupt(APP_ECAP_INSTANCE);
    ECAP_enableTimeStampCapture(APP_ECAP_INSTANCE);
    ECAP_reArm(APP_ECAP_INSTANCE);
    ECAP_enableInterrupt(APP_ECAP_INSTANCE, ECAP_ECEINT_CEVT2_M);
}

/* ────────────── 捕获中断 ────────────── */
static void us_ecap_isr(void)
{
    rt_interrupt_enter();

    s_cap_rising  = ECAP_getEventTimeStamp(APP_ECAP_INSTANCE, ECAP_EVENT_1);
    s_cap_falling = ECAP_getEventTimeStamp(APP_ECAP_INSTANCE, ECAP_EVENT_2);
    s_cap_state   = 2U;

    ECAP_disableInterrupt(APP_ECAP_INSTANCE, ECAP_ECEINT_CEVT2_M);
    ECAP_clearInterrupt(APP_ECAP_INSTANCE, ECAP_ECCLR_CEVT1_M | ECAP_ECCLR_CEVT2_M);
    ECAP_clearGlobalInterrupt(APP_ECAP_INSTANCE);

    rt_sem_release(&s_us_done);

    rt_interrupt_leave();
}

/* ────────────── 对外接口 ────────────── */

rt_err_t app_ultrasonic_init(void)
{
    rt_err_t ret;

    /* TRIG：推挽输出，默认低 */
    rt_pin_mode(APP_PIN_US_TRIG, PIN_MODE_OUTPUT);
    rt_pin_write(APP_PIN_US_TRIG, PIN_LOW);

    /* ECHO：输入，复用为 ALT0（与 BSP 的 ecap1 配置一致） */
    GPIO_setPinConfig(APP_ECAP_ECHO_PORT, APP_ECAP_ECHO_PIN, ALT0_FUNCTION);
    GPIO_setAnalogMode(APP_ECAP_ECHO_PORT, APP_ECAP_ECHO_PIN, GPIO_ANALOG_DISABLED);
    GPIO_setPadConfig(APP_ECAP_ECHO_PORT, APP_ECAP_ECHO_PIN, GPIO_PIN_TYPE_STD);
    GPIO_setQualificationMode(APP_ECAP_ECHO_PORT, APP_ECAP_ECHO_PIN, GPIO_QUAL_SYNC);
    GPIO_setDirectionMode(APP_ECAP_ECHO_PORT, APP_ECAP_ECHO_PIN, GPIO_DIR_MODE_IN);

    /* 把 ECHO 引脚接到 ECAP 的 XBAR 输入 */
    XBAR_setInputPin(XBAR, APP_ECAP_XBAR, APP_ECAP_ECHO_PIN);

    /* ECAP 单次捕获：EVENT1 上升沿（复位计数器），EVENT2 下降沿 */
    ECAP_disableInterrupt(APP_ECAP_INSTANCE, ECAP_ECEINT_CEVT1_M | ECAP_ECEINT_CEVT2_M);
    ECAP_clearInterrupt(APP_ECAP_INSTANCE, ECAP_ECCLR_CEVT1_M | ECAP_ECCLR_CEVT2_M);
    ECAP_clearGlobalInterrupt(APP_ECAP_INSTANCE);
    ECAP_disableTimeStampCapture(APP_ECAP_INSTANCE);
    ECAP_stopCounter(APP_ECAP_INSTANCE);

    ECAP_enableCaptureMode(APP_ECAP_INSTANCE);
    ECAP_setCaptureMode(APP_ECAP_INSTANCE, ECAP_ONE_SHOT_CAPTURE_MODE, ECAP_EVENT_2);
    ECAP_setEventPrescaler(APP_ECAP_INSTANCE, APP_ECAP_PRESCALER);
    ECAP_setEventPolarity(APP_ECAP_INSTANCE, ECAP_EVENT_1, ECAP_EVNT_RISING_EDGE);
    ECAP_setEventPolarity(APP_ECAP_INSTANCE, ECAP_EVENT_2, ECAP_EVNT_FALLING_EDGE);
    ECAP_enableCounterResetOnEvent(APP_ECAP_INSTANCE, ECAP_EVENT_1);
    ECAP_selectECAPInput(APP_ECAP_INSTANCE, APP_ECAP_INPUT);
    ECAP_setPhaseShiftCount(APP_ECAP_INSTANCE, 0U);
    ECAP_enableLoadCounter(APP_ECAP_INSTANCE);
    ECAP_setSyncOutMode(APP_ECAP_INSTANCE, ECAP_SYNC_OUT_SYNCI);
    ECAP_setEmulationMode(APP_ECAP_INSTANCE, ECAP_EMULATION_STOP);
    ECAP_setSyncInPulseSource(APP_ECAP_INSTANCE, ECAP_SYNC_IN_PULSE_SRC_DISABLE);

    Interrupt_register(APP_ECAP_IRQ, &us_ecap_isr);
    Interrupt_enable(APP_ECAP_IRQ);

    ECAP_startCounter(APP_ECAP_INSTANCE);
    ECAP_enableTimeStampCapture(APP_ECAP_INSTANCE);
    ECAP_reArm(APP_ECAP_INSTANCE);

    ret = rt_sem_init(&s_us_done, "us_done", 0, RT_IPC_FLAG_PRIO);
    if (ret != RT_EOK)
    {
        LOG_E("创建测距信号量失败: %d", ret);
        return ret;
    }

    s_inited = RT_TRUE;

    LOG_I("超声波就绪: TRIG=PA5, ECHO=PA16(ECAP1), 计数时钟 %d MHz",
          (int)(SystemCoreClock / (APP_ECAP_PRESCALER + 1U) / 1000000U));
    return RT_EOK;
}

float app_ultrasonic_get_distance_cm(void)
{
    rt_uint32_t pulse_ticks;
    rt_uint32_t ticks_per_us;
    rt_uint32_t level;

    if (!s_inited)
    {
        return 0.0f;
    }

    /* 清掉上一轮可能残留的信号量 */
    rt_sem_control(&s_us_done, RT_IPC_CMD_RESET, RT_NULL);

    /* 临界区：复位捕获状态并重新装载，期间禁止被打断 */
    level = rt_hw_interrupt_disable();
    s_cap_state = 0U;
    us_arm();
    rt_hw_interrupt_enable(level);

    /* 发一个 10us 的触发脉冲 */
    us_tx_trigger();

    /* 等回波捕获完成（带超时，避免无回波时永久阻塞） */
    if (rt_sem_take(&s_us_done,
                    rt_tick_from_millisecond(APP_US_TIMEOUT_MS)) != RT_EOK)
    {
        LOG_D("测距超时（无回波）");
        return 0.0f;
    }

    pulse_ticks  = s_cap_falling - s_cap_rising;
    ticks_per_us = us_ticks_per_us();
    s_last_ticks = pulse_ticks;

    if (pulse_ticks == 0U)
    {
        return 0.0f;
    }

    return (((float)pulse_ticks / (float)ticks_per_us) / APP_US_US_PER_CM) * s_gain;
}

rt_uint32_t app_ultrasonic_get_last_ticks(void)
{
    return s_last_ticks;
}

void app_ultrasonic_set_gain(float gain)
{
    if (gain > 0.2f && gain < 5.0f)
    {
        s_gain = gain;
        LOG_I("测距校准系数设为 %.3f", (double)gain);
    }
}

float app_ultrasonic_get_gain(void)
{
    return s_gain;
}
