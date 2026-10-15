#include "Ultrasonic.h"
#include "delay.h"

#define ULTRASONIC_ECAP              ECAP1
#define ULTRASONIC_ECAP_IRQ          ECAP1_IRQn
#define ULTRASONIC_ECAP_INPUT        ECAP_INPUT_XBAR_INPUT7
#define ULTRASONIC_ECAP_XBAR         XBAR_INPUT7

#define ULTRASONIC_TRIG_PORT         GPIOA
#define ULTRASONIC_TRIG_PIN          GPIO_PIN_5
#define ULTRASONIC_TRIG_MUX          ALT0_FUNCTION

#define ULTRASONIC_ECHO_PORT         GPIOA
#define ULTRASONIC_ECHO_PIN          GPIO_PIN_16
#define ULTRASONIC_ECHO_MUX          ALT0_FUNCTION

#define ULTRASONIC_CAPTURE_CLK_DIV   1U
#define ULTRASONIC_TIMEOUT_LOOPS     60000U
#define ULTRASONIC_TRIG_US           10U
#define ULTRASONIC_US_PER_CM         58.0f

static volatile uint32_t capture_rising  = 0;
static volatile uint32_t capture_falling = 0;
static volatile uint8_t  capture_state   = 0;

static void Ultrasonic_Echo_Isr(void);

void Ultrasonic_Init(void)
{
    GPIO_setAnalogMode(ULTRASONIC_TRIG_PORT, ULTRASONIC_TRIG_PIN, GPIO_ANALOG_DISABLED);
    GPIO_setPinConfig(ULTRASONIC_TRIG_PORT, ULTRASONIC_TRIG_PIN, ULTRASONIC_TRIG_MUX);
    GPIO_setPadConfig(ULTRASONIC_TRIG_PORT, ULTRASONIC_TRIG_PIN, GPIO_PIN_TYPE_STD);
    GPIO_setQualificationMode(ULTRASONIC_TRIG_PORT, ULTRASONIC_TRIG_PIN, GPIO_QUAL_SYNC);
    GPIO_setDriveLevel(ULTRASONIC_TRIG_PORT, ULTRASONIC_TRIG_PIN, GPIO_DRV_MAX);
    GPIO_setDirectionMode(ULTRASONIC_TRIG_PORT, ULTRASONIC_TRIG_PIN, GPIO_DIR_MODE_OUT);
    GPIO_clearPin(ULTRASONIC_TRIG_PORT, ULTRASONIC_TRIG_PIN);

    GPIO_setPinConfig(ULTRASONIC_ECHO_PORT, ULTRASONIC_ECHO_PIN, ULTRASONIC_ECHO_MUX);
    GPIO_setAnalogMode(ULTRASONIC_ECHO_PORT, ULTRASONIC_ECHO_PIN, GPIO_ANALOG_DISABLED);
    GPIO_setPadConfig(ULTRASONIC_ECHO_PORT, ULTRASONIC_ECHO_PIN, GPIO_PIN_TYPE_STD);
    GPIO_setQualificationMode(ULTRASONIC_ECHO_PORT, ULTRASONIC_ECHO_PIN, GPIO_QUAL_SYNC);
    GPIO_setDirectionMode(ULTRASONIC_ECHO_PORT, ULTRASONIC_ECHO_PIN, GPIO_DIR_MODE_IN);

    XBAR_setInputPin(XBAR, ULTRASONIC_ECAP_XBAR, ULTRASONIC_ECHO_PIN);

    ECAP_disableInterrupt(ULTRASONIC_ECAP, ECAP_ECEINT_CEVT1_M | ECAP_ECEINT_CEVT2_M);
    ECAP_clearInterrupt(ULTRASONIC_ECAP, ECAP_ECCLR_CEVT1_M | ECAP_ECCLR_CEVT2_M);
    ECAP_clearGlobalInterrupt(ULTRASONIC_ECAP);
    ECAP_disableTimeStampCapture(ULTRASONIC_ECAP);
    ECAP_stopCounter(ULTRASONIC_ECAP);

    ECAP_enableCaptureMode(ULTRASONIC_ECAP);
    ECAP_setCaptureMode(ULTRASONIC_ECAP, ECAP_ONE_SHOT_CAPTURE_MODE, ECAP_EVENT_2);
    ECAP_setEventPolarity(ULTRASONIC_ECAP, ECAP_EVENT_1, ECAP_EVNT_RISING_EDGE);
    ECAP_setEventPolarity(ULTRASONIC_ECAP, ECAP_EVENT_2, ECAP_EVNT_FALLING_EDGE);
    ECAP_enableCounterResetOnEvent(ULTRASONIC_ECAP, ECAP_EVENT_1);
    ECAP_selectECAPInput(ULTRASONIC_ECAP, ULTRASONIC_ECAP_INPUT);
    ECAP_setPhaseShiftCount(ULTRASONIC_ECAP, 0U);
    ECAP_enableLoadCounter(ULTRASONIC_ECAP);
    ECAP_setSyncOutMode(ULTRASONIC_ECAP, ECAP_SYNC_OUT_SYNCI);
    ECAP_setEmulationMode(ULTRASONIC_ECAP, ECAP_EMULATION_STOP);
    ECAP_setSyncInPulseSource(ULTRASONIC_ECAP, ECAP_SYNC_IN_PULSE_SRC_DISABLE);

    Interrupt_register(ULTRASONIC_ECAP_IRQ, &Ultrasonic_Echo_Isr);
    Interrupt_enable(ULTRASONIC_ECAP_IRQ);

    ECAP_startCounter(ULTRASONIC_ECAP);
    ECAP_enableTimeStampCapture(ULTRASONIC_ECAP);
    ECAP_reArm(ULTRASONIC_ECAP);
}

float Ultrasonic_GetDistance(void)
{
    uint32_t pulse_ticks;
    uint32_t timeout;
    uint32_t ticks_per_us;

    ticks_per_us = (SystemCoreClock / ULTRASONIC_CAPTURE_CLK_DIV) / 1000000U;

    if (ticks_per_us == 0U)
    {
        ticks_per_us = 1U;
    }

    __disable_irq();
    capture_state = 0;
    ECAP_disableInterrupt(ULTRASONIC_ECAP, ECAP_ECEINT_CEVT1_M | ECAP_ECEINT_CEVT2_M);
    ECAP_clearInterrupt(ULTRASONIC_ECAP, ECAP_ECCLR_CEVT1_M | ECAP_ECCLR_CEVT2_M);
    ECAP_clearGlobalInterrupt(ULTRASONIC_ECAP);
    ECAP_enableTimeStampCapture(ULTRASONIC_ECAP);
    ECAP_reArm(ULTRASONIC_ECAP);
    ECAP_enableInterrupt(ULTRASONIC_ECAP, ECAP_ECEINT_CEVT2_M);
    __enable_irq();

    GPIO_setPin(ULTRASONIC_TRIG_PORT, ULTRASONIC_TRIG_PIN);
    Delay_us(ULTRASONIC_TRIG_US);
    GPIO_clearPin(ULTRASONIC_TRIG_PORT, ULTRASONIC_TRIG_PIN);

    timeout = ULTRASONIC_TIMEOUT_LOOPS;
    while (capture_state < 2)
    {
        if (--timeout == 0U)
        {
            return 0.0f;
        }
        Delay_us(1);
    }

    pulse_ticks = capture_falling - capture_rising;

    return ((float)pulse_ticks / (float)ticks_per_us) / ULTRASONIC_US_PER_CM;
}

static void Ultrasonic_Echo_Isr(void)
{
    capture_rising  = ECAP_getEventTimeStamp(ULTRASONIC_ECAP, ECAP_EVENT_1);
    capture_falling = ECAP_getEventTimeStamp(ULTRASONIC_ECAP, ECAP_EVENT_2);
    capture_state   = 2;

    ECAP_disableInterrupt(ULTRASONIC_ECAP, ECAP_ECEINT_CEVT2_M);
    ECAP_clearInterrupt(ULTRASONIC_ECAP, ECAP_ECCLR_CEVT1_M | ECAP_ECCLR_CEVT2_M);
    ECAP_clearGlobalInterrupt(ULTRASONIC_ECAP);
}
