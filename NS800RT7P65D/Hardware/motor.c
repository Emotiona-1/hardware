#include "motor.h"
#include "delay.h"

static void Motor_GpioConfig(GPIO_TypeDef *port, GPIO_PinNum pin)
{
    GPIO_setAnalogMode(port, pin, GPIO_ANALOG_DISABLED);
    GPIO_setPinConfig(port, pin, ALT0_FUNCTION);
    GPIO_setPadConfig(port, pin, GPIO_PIN_TYPE_STD);
    GPIO_setQualificationMode(port, pin, GPIO_QUAL_SYNC);
    GPIO_setDriveLevel(port, pin, GPIO_DRV_MAX);
    GPIO_setDirectionMode(port, pin, GPIO_DIR_MODE_OUT);
}

void Motor_Init(void)
{
    Motor_GpioConfig(MOTOR_PUL_PORT, MOTOR_PUL_PIN);
    Motor_GpioConfig(MOTOR_DIR_PORT, MOTOR_DIR_PIN);
    Motor_GpioConfig(MOTOR_ENA_PORT, MOTOR_ENA_PIN);

    GPIO_clearPin(MOTOR_PUL_PORT, MOTOR_PUL_PIN);
    GPIO_clearPin(MOTOR_DIR_PORT, MOTOR_DIR_PIN);
    GPIO_setPin(MOTOR_ENA_PORT, MOTOR_ENA_PIN);
}

void Motor_Enable(void)
{
    GPIO_clearPin(MOTOR_ENA_PORT, MOTOR_ENA_PIN);
}

void Motor_Disable(void)
{
    GPIO_setPin(MOTOR_ENA_PORT, MOTOR_ENA_PIN);
}

void Motor_SetDirection(uint8_t dir)
{
    if (dir == DIR_CW)
        GPIO_clearPin(MOTOR_DIR_PORT, MOTOR_DIR_PIN);
    else
        GPIO_setPin(MOTOR_DIR_PORT, MOTOR_DIR_PIN);
}

void Motor_OnePulse(void)
{
    GPIO_setPin(MOTOR_PUL_PORT, MOTOR_PUL_PIN);
    delay_us(50);
    GPIO_clearPin(MOTOR_PUL_PORT, MOTOR_PUL_PIN);
    delay_us(50);
}

void Motor_OnePulse_Slow(void)
{
    GPIO_setPin(MOTOR_PUL_PORT, MOTOR_PUL_PIN);
    delay_us(600);
    GPIO_clearPin(MOTOR_PUL_PORT, MOTOR_PUL_PIN);
    delay_us(600);
}

void Motor_RunSteps(uint32_t steps)
{
    uint32_t i;
    for (i = 0; i < steps; i++)
    {
        Motor_OnePulse();
        delay_us(500);
    }
}

void Motor_RunSteps_Slow(uint32_t steps)
{
    uint32_t i;
    for (i = 0; i < steps; i++)
    {
        Motor_OnePulse_Slow();
    }
}
