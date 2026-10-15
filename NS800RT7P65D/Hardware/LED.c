#include "NS800RTxxxx.h"
#include "LED.h"

#define LED1_PORT    GPIOB
#define LED1_PIN     GPIO_PIN_0
#define LED2_PORT    GPIOB
#define LED2_PIN     GPIO_PIN_1

void LED_Init(void)
{
    GPIO_setAnalogMode(LED1_PORT, LED1_PIN, GPIO_ANALOG_DISABLED);
    GPIO_setPinConfig(LED1_PORT, LED1_PIN, ALT0_FUNCTION);
    GPIO_setPadConfig(LED1_PORT, LED1_PIN, GPIO_PIN_TYPE_STD);
    GPIO_setQualificationMode(LED1_PORT, LED1_PIN, GPIO_QUAL_SYNC);
    GPIO_setDriveLevel(LED1_PORT, LED1_PIN, GPIO_DRV_MAX);
    GPIO_setDirectionMode(LED1_PORT, LED1_PIN, GPIO_DIR_MODE_OUT);

    GPIO_setAnalogMode(LED2_PORT, LED2_PIN, GPIO_ANALOG_DISABLED);
    GPIO_setPinConfig(LED2_PORT, LED2_PIN, ALT0_FUNCTION);
    GPIO_setPadConfig(LED2_PORT, LED2_PIN, GPIO_PIN_TYPE_STD);
    GPIO_setQualificationMode(LED2_PORT, LED2_PIN, GPIO_QUAL_SYNC);
    GPIO_setDriveLevel(LED2_PORT, LED2_PIN, GPIO_DRV_MAX);
    GPIO_setDirectionMode(LED2_PORT, LED2_PIN, GPIO_DIR_MODE_OUT);

    GPIO_setPin(LED1_PORT, LED1_PIN);
    GPIO_setPin(LED2_PORT, LED2_PIN);
}

void LED1_ON(void)
{
    GPIO_clearPin(LED1_PORT, LED1_PIN);
}

void LED1_OFF(void)
{
    GPIO_setPin(LED1_PORT, LED1_PIN);
}

void LED1_Turn(void)
{
    if (GPIO_readPin(LED1_PORT, LED1_PIN) == false)
    {
        GPIO_setPin(LED1_PORT, LED1_PIN);
    }
    else
    {
        GPIO_clearPin(LED1_PORT, LED1_PIN);
    }
}

void LED2_ON(void)
{
    GPIO_clearPin(LED2_PORT, LED2_PIN);
}

void LED2_OFF(void)
{
    GPIO_setPin(LED2_PORT, LED2_PIN);
}

void LED2_Turn(void)
{
    if (GPIO_readPin(LED2_PORT, LED2_PIN) == false)
    {
        GPIO_setPin(LED2_PORT, LED2_PIN);
    }
    else
    {
        GPIO_clearPin(LED2_PORT, LED2_PIN);
    }
}
