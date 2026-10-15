#include "NS800RTxxxx.h"
#include "delay.h"
#include "Key.h"

#define KEY1_PORT    GPIOB
#define KEY1_PIN     GPIO_PIN_2
#define KEY2_PORT    GPIOB
#define KEY2_PIN     GPIO_PIN_12

void Key_Init(void)
{
    GPIO_setAnalogMode(KEY1_PORT, KEY1_PIN, GPIO_ANALOG_DISABLED);
    GPIO_setPinConfig(KEY1_PORT, KEY1_PIN, ALT0_FUNCTION);
    GPIO_setPadConfig(KEY1_PORT, KEY1_PIN, GPIO_PIN_TYPE_PULLUP);
    GPIO_setQualificationMode(KEY1_PORT, KEY1_PIN, GPIO_QUAL_SYNC);
    GPIO_setDirectionMode(KEY1_PORT, KEY1_PIN, GPIO_DIR_MODE_IN);

    GPIO_setAnalogMode(KEY2_PORT, KEY2_PIN, GPIO_ANALOG_DISABLED);
    GPIO_setPinConfig(KEY2_PORT, KEY2_PIN, ALT0_FUNCTION);
    GPIO_setPadConfig(KEY2_PORT, KEY2_PIN, GPIO_PIN_TYPE_PULLUP);
    GPIO_setQualificationMode(KEY2_PORT, KEY2_PIN, GPIO_QUAL_SYNC);
    GPIO_setDirectionMode(KEY2_PORT, KEY2_PIN, GPIO_DIR_MODE_IN);
}

uint8_t Key_GetNum(void)
{
    uint8_t KeyNum = 0;

    if (GPIO_readPin(KEY1_PORT, KEY1_PIN) == false)
    {
        Delay_ms(20);
        while (GPIO_readPin(KEY1_PORT, KEY1_PIN) == false);
        Delay_ms(20);
        KeyNum = 1;
    }

    if (GPIO_readPin(KEY2_PORT, KEY2_PIN) == false)
    {
        Delay_ms(20);
        while (GPIO_readPin(KEY2_PORT, KEY2_PIN) == false);
        Delay_ms(20);
        KeyNum = 2;
    }

    return KeyNum;
}
