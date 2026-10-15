#include "NS800RTxxxx.h"
#include "DCMotor.h"
#include "PWM.h"

void DCMotor_Init(void)
{
    GPIO_setAnalogMode(DCMOTOR_NSLEEP_PORT, DCMOTOR_NSLEEP_PIN, GPIO_ANALOG_DISABLED);
    GPIO_setPinConfig(DCMOTOR_NSLEEP_PORT, DCMOTOR_NSLEEP_PIN, DCMOTOR_NSLEEP_MUX);
    GPIO_setPadConfig(DCMOTOR_NSLEEP_PORT, DCMOTOR_NSLEEP_PIN, GPIO_PIN_TYPE_STD);
    GPIO_setQualificationMode(DCMOTOR_NSLEEP_PORT, DCMOTOR_NSLEEP_PIN, GPIO_QUAL_SYNC);
    GPIO_setDriveLevel(DCMOTOR_NSLEEP_PORT, DCMOTOR_NSLEEP_PIN, GPIO_DRV_MAX);
    GPIO_setDirectionMode(DCMOTOR_NSLEEP_PORT, DCMOTOR_NSLEEP_PIN, GPIO_DIR_MODE_OUT);

    GPIO_setPin(DCMOTOR_NSLEEP_PORT, DCMOTOR_NSLEEP_PIN);

    PWM_Init();
}

void DCMotor_SetSpeed(int8_t Speed)
{
    if (Speed >= 0)
    {
        PWM_SetCompare3(Speed);
        PWM_SetCompare4(0);
    }
    else
    {
        PWM_SetCompare3(0);
        PWM_SetCompare4(-Speed);
    }
}
