#include "NS800RTxxxx.h"
#include "PWM.h"

#define PWM_EPWM_INSTANCE      EPWM1
#define PWM_PORT_A             GPIOA
#define PWM_PIN_A              GPIO_PIN_0
#define PWM_MUX_A              ALT1_FUNCTION
#define PWM_PORT_B             GPIOA
#define PWM_PIN_B              GPIO_PIN_1
#define PWM_MUX_B              ALT1_FUNCTION

#define PWM_FREQUENCY_HZ       20000U
#define PWM_TBPRD_LIMIT        65535U
#define PWM_FULL_SCALE         100U

static uint16_t s_pwm_period = 0;

static void PWM_PinConfig(GPIO_TypeDef *port, GPIO_PinNum pin, GPIO_AltFunc mux)
{
    GPIO_setAnalogMode(port, pin, GPIO_ANALOG_DISABLED);
    GPIO_setPinConfig(port, pin, mux);
    GPIO_setPadConfig(port, pin, GPIO_PIN_TYPE_STD);
    GPIO_setQualificationMode(port, pin, GPIO_QUAL_SYNC);
}

void PWM_Init(void)
{
    uint32_t tbclk = RCC_getPclk1Frequency();
    uint32_t period = tbclk / PWM_FREQUENCY_HZ;

    if (period == 0U)
    {
        period = 1U;
    }

    if (period > (PWM_TBPRD_LIMIT + 1U))
    {
        period = PWM_TBPRD_LIMIT + 1U;
    }

    s_pwm_period = (uint16_t)(period - 1U);

    PWM_PinConfig(PWM_PORT_A, PWM_PIN_A, PWM_MUX_A);
    PWM_PinConfig(PWM_PORT_B, PWM_PIN_B, PWM_MUX_B);

    EPWM_setClockPrescaler(PWM_EPWM_INSTANCE, EPWM_CLOCK_DIVIDER_1, EPWM_HSCLOCK_DIVIDER_1);
    EPWM_setTimeBasePeriod(PWM_EPWM_INSTANCE, s_pwm_period);
    EPWM_setTimeBaseCounter(PWM_EPWM_INSTANCE, 0U);
    EPWM_disablePhaseShiftLoad(PWM_EPWM_INSTANCE);
    EPWM_setPhaseShift(PWM_EPWM_INSTANCE, 0U);
    EPWM_setTimeBaseCounterMode(PWM_EPWM_INSTANCE, EPWM_COUNTER_MODE_UP);

    EPWM_setCounterCompareShadowLoadMode(PWM_EPWM_INSTANCE, EPWM_COUNTER_COMPARE_A, EPWM_COMP_LOAD_ON_CNTR_ZERO);
    EPWM_setCounterCompareValue(PWM_EPWM_INSTANCE, EPWM_COUNTER_COMPARE_A, 0U);
    EPWM_setActionQualifierAction(PWM_EPWM_INSTANCE, EPWM_AQ_OUTPUT_A, EPWM_AQ_OUTPUT_HIGH, EPWM_AQ_OUTPUT_ON_TIMEBASE_ZERO);
    EPWM_setActionQualifierAction(PWM_EPWM_INSTANCE, EPWM_AQ_OUTPUT_A, EPWM_AQ_OUTPUT_LOW, EPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPA);

    EPWM_setCounterCompareShadowLoadMode(PWM_EPWM_INSTANCE, EPWM_COUNTER_COMPARE_B, EPWM_COMP_LOAD_ON_CNTR_ZERO);
    EPWM_setCounterCompareValue(PWM_EPWM_INSTANCE, EPWM_COUNTER_COMPARE_B, 0U);
    EPWM_setActionQualifierAction(PWM_EPWM_INSTANCE, EPWM_AQ_OUTPUT_B, EPWM_AQ_OUTPUT_HIGH, EPWM_AQ_OUTPUT_ON_TIMEBASE_ZERO);
    EPWM_setActionQualifierAction(PWM_EPWM_INSTANCE, EPWM_AQ_OUTPUT_B, EPWM_AQ_OUTPUT_LOW, EPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPB);
}

void PWM_SetCompare3(uint16_t Compare)
{
    if (Compare > PWM_FULL_SCALE)
    {
        Compare = PWM_FULL_SCALE;
    }

    EPWM_setCounterCompareValue(PWM_EPWM_INSTANCE, EPWM_COUNTER_COMPARE_A,
                                (uint16_t)((uint32_t)Compare * ((uint32_t)s_pwm_period + 1U) / PWM_FULL_SCALE));
}

void PWM_SetCompare4(uint16_t Compare)
{
    if (Compare > PWM_FULL_SCALE)
    {
        Compare = PWM_FULL_SCALE;
    }

    EPWM_setCounterCompareValue(PWM_EPWM_INSTANCE, EPWM_COUNTER_COMPARE_B,
                                (uint16_t)((uint32_t)Compare * ((uint32_t)s_pwm_period + 1U) / PWM_FULL_SCALE));
}
