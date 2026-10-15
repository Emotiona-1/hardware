#include "vibration_motor.h"
#include "NS800RTxxxx.h"
#include "delay.h"

static uint16_t g_pwm_period = 1000;

void VibrationMotor_Init(void)
{
    GPIO_setAnalogMode(VIBRATION_MOTOR_GPIO_PORT, VIBRATION_MOTOR_GPIO_PIN, GPIO_ANALOG_DISABLED);
    GPIO_setPinConfig(VIBRATION_MOTOR_GPIO_PORT, VIBRATION_MOTOR_GPIO_PIN, VIBRATION_MOTOR_GPIO_MUX);
    GPIO_setPadConfig(VIBRATION_MOTOR_GPIO_PORT, VIBRATION_MOTOR_GPIO_PIN, GPIO_PIN_TYPE_STD);
    GPIO_setQualificationMode(VIBRATION_MOTOR_GPIO_PORT, VIBRATION_MOTOR_GPIO_PIN, GPIO_QUAL_SYNC);
    GPIO_setDriveLevel(VIBRATION_MOTOR_GPIO_PORT, VIBRATION_MOTOR_GPIO_PIN, GPIO_DRV_MAX);
    GPIO_setDirectionMode(VIBRATION_MOTOR_GPIO_PORT, VIBRATION_MOTOR_GPIO_PIN, GPIO_DIR_MODE_OUT);

    GPIO_clearPin(VIBRATION_MOTOR_GPIO_PORT, VIBRATION_MOTOR_GPIO_PIN);
}

void VibrationMotor_On(void)
{
    GPIO_setPin(VIBRATION_MOTOR_GPIO_PORT, VIBRATION_MOTOR_GPIO_PIN);
}

void VibrationMotor_Off(void)
{
    GPIO_clearPin(VIBRATION_MOTOR_GPIO_PORT, VIBRATION_MOTOR_GPIO_PIN);
}

void VibrationMotor_Toggle(void)
{
    if (GPIO_readPin(VIBRATION_MOTOR_GPIO_PORT, VIBRATION_MOTOR_GPIO_PIN))
    {
        GPIO_clearPin(VIBRATION_MOTOR_GPIO_PORT, VIBRATION_MOTOR_GPIO_PIN);
    }
    else
    {
        GPIO_setPin(VIBRATION_MOTOR_GPIO_PORT, VIBRATION_MOTOR_GPIO_PIN);
    }
}

void VibrationMotor_Vibrate(uint16_t ms)
{
    VibrationMotor_On();
    Delay_ms(ms);
    VibrationMotor_Off();
}

void VibrationMotor_PWM_Init(uint16_t period, uint16_t prescaler)
{
    uint32_t want = (uint32_t)prescaler + 1U;
    uint32_t index = 0U;

    g_pwm_period = period + 1;

    while ((index < 7U) && ((1UL << index) < want))
    {
        index++;
    }

    GPIO_setAnalogMode(VIBRATION_MOTOR_GPIO_PORT, VIBRATION_MOTOR_GPIO_PIN, GPIO_ANALOG_DISABLED);
    GPIO_setPinConfig(VIBRATION_MOTOR_GPIO_PORT, VIBRATION_MOTOR_GPIO_PIN, VIBRATION_MOTOR_PWM_MUX);
    GPIO_setPadConfig(VIBRATION_MOTOR_GPIO_PORT, VIBRATION_MOTOR_GPIO_PIN, GPIO_PIN_TYPE_STD);
    GPIO_setQualificationMode(VIBRATION_MOTOR_GPIO_PORT, VIBRATION_MOTOR_GPIO_PIN, GPIO_QUAL_SYNC);

    EPWM_setClockPrescaler(VIBRATION_MOTOR_PWM, (EPWM_ClockDivider)index, EPWM_HSCLOCK_DIVIDER_1);
    EPWM_setTimeBasePeriod(VIBRATION_MOTOR_PWM, period);
    EPWM_setTimeBaseCounter(VIBRATION_MOTOR_PWM, 0U);
    EPWM_disablePhaseShiftLoad(VIBRATION_MOTOR_PWM);
    EPWM_setPhaseShift(VIBRATION_MOTOR_PWM, 0U);
    EPWM_setTimeBaseCounterMode(VIBRATION_MOTOR_PWM, EPWM_COUNTER_MODE_UP);

    EPWM_setCounterCompareShadowLoadMode(VIBRATION_MOTOR_PWM, EPWM_COUNTER_COMPARE_A, EPWM_COMP_LOAD_ON_CNTR_ZERO);
    EPWM_setCounterCompareValue(VIBRATION_MOTOR_PWM, EPWM_COUNTER_COMPARE_A, 0U);
    EPWM_setActionQualifierAction(VIBRATION_MOTOR_PWM, EPWM_AQ_OUTPUT_A, EPWM_AQ_OUTPUT_HIGH, EPWM_AQ_OUTPUT_ON_TIMEBASE_ZERO);
    EPWM_setActionQualifierAction(VIBRATION_MOTOR_PWM, EPWM_AQ_OUTPUT_A, EPWM_AQ_OUTPUT_LOW, EPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPA);
}

void VibrationMotor_SetIntensity(uint16_t intensity)
{
    if (intensity > 1000) intensity = 1000;

    uint16_t pulse = (uint16_t)((uint32_t)intensity * g_pwm_period / 1000U);
    EPWM_setCounterCompareValue(VIBRATION_MOTOR_PWM, EPWM_COUNTER_COMPARE_A, pulse);
}
