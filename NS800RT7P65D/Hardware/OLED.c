#include "NS800RTxxxx.h"
#include "OLED.h"
#include "OLED_Font.h"

#define OLED_PORT        GPIOB
#define OLED_PIN_SCL     GPIO_PIN_14
#define OLED_PIN_SDA     GPIO_PIN_15

static void OLED_W_SCL(uint8_t x)
{
    if (x)
    {
        GPIO_setPin(OLED_PORT, OLED_PIN_SCL);
    }
    else
    {
        GPIO_clearPin(OLED_PORT, OLED_PIN_SCL);
    }
}

static void OLED_W_SDA(uint8_t x)
{
    if (x)
    {
        GPIO_setPin(OLED_PORT, OLED_PIN_SDA);
    }
    else
    {
        GPIO_clearPin(OLED_PORT, OLED_PIN_SDA);
    }
}

void OLED_I2C_Init(void)
{
    GPIO_setAnalogMode(OLED_PORT, OLED_PIN_SCL, GPIO_ANALOG_DISABLED);
    GPIO_setPinConfig(OLED_PORT, OLED_PIN_SCL, ALT0_FUNCTION);
    GPIO_setPadConfig(OLED_PORT, OLED_PIN_SCL, GPIO_PIN_TYPE_OD);
    GPIO_setQualificationMode(OLED_PORT, OLED_PIN_SCL, GPIO_QUAL_SYNC);
    GPIO_setDriveLevel(OLED_PORT, OLED_PIN_SCL, GPIO_DRV_MAX);
    GPIO_setDirectionMode(OLED_PORT, OLED_PIN_SCL, GPIO_DIR_MODE_OUT);

    GPIO_setAnalogMode(OLED_PORT, OLED_PIN_SDA, GPIO_ANALOG_DISABLED);
    GPIO_setPinConfig(OLED_PORT, OLED_PIN_SDA, ALT0_FUNCTION);
    GPIO_setPadConfig(OLED_PORT, OLED_PIN_SDA, GPIO_PIN_TYPE_OD);
    GPIO_setQualificationMode(OLED_PORT, OLED_PIN_SDA, GPIO_QUAL_SYNC);
    GPIO_setDriveLevel(OLED_PORT, OLED_PIN_SDA, GPIO_DRV_MAX);
    GPIO_setDirectionMode(OLED_PORT, OLED_PIN_SDA, GPIO_DIR_MODE_OUT);

    OLED_W_SCL(1);
    OLED_W_SDA(1);
}

void OLED_I2C_Start(void)
{
    OLED_W_SDA(1);
    OLED_W_SCL(1);
    OLED_W_SDA(0);
    OLED_W_SCL(0);
}

void OLED_I2C_Stop(void)
{
    OLED_W_SDA(0);
    OLED_W_SCL(1);
    OLED_W_SDA(1);
}

void OLED_I2C_SendByte(uint8_t Byte)
{
    uint8_t i;
    for (i = 0; i < 8; i++)
    {
        OLED_W_SDA(!!(Byte & (0x80 >> i)));
        OLED_W_SCL(1);
        OLED_W_SCL(0);
    }
    OLED_W_SCL(1);
    OLED_W_SCL(0);
}

void OLED_WriteCommand(uint8_t Command)
{
    OLED_I2C_Start();
    OLED_I2C_SendByte(0x78);
    OLED_I2C_SendByte(0x00);
    OLED_I2C_SendByte(Command);
    OLED_I2C_Stop();
}

void OLED_WriteData(uint8_t Data)
{
    OLED_I2C_Start();
    OLED_I2C_SendByte(0x78);
    OLED_I2C_SendByte(0x40);
    OLED_I2C_SendByte(Data);
    OLED_I2C_Stop();
}

void OLED_SetCursor(uint8_t Y, uint8_t X)
{
    OLED_WriteCommand(0xB0 | Y);
    OLED_WriteCommand(0x10 | ((X & 0xF0) >> 4));
    OLED_WriteCommand(0x00 | (X & 0x0F));
}

void OLED_Clear(void)
{
    uint8_t i, j;
    for (j = 0; j < 8; j++)
    {
        OLED_SetCursor(j, 0);
        for (i = 0; i < 128; i++)
        {
            OLED_WriteData(0x00);
        }
    }
}

void OLED_ShowChar(uint8_t Line, uint8_t Column, char Char)
{
    uint8_t i;
    OLED_SetCursor((Line - 1) * 2, (Column - 1) * 8);
    for (i = 0; i < 8; i++)
    {
        OLED_WriteData(OLED_F8x16[Char - ' '][i]);
    }
    OLED_SetCursor((Line - 1) * 2 + 1, (Column - 1) * 8);
    for (i = 0; i < 8; i++)
    {
        OLED_WriteData(OLED_F8x16[Char - ' '][i + 8]);
    }
}

void OLED_ShowString(uint8_t Line, uint8_t Column, char *String)
{
    uint8_t i;
    for (i = 0; String[i] != '\0'; i++)
    {
        OLED_ShowChar(Line, Column + i, String[i]);
    }
}

uint32_t OLED_Pow(uint32_t X, uint32_t Y)
{
    uint32_t Result = 1;
    while (Y--)
    {
        Result *= X;
    }
    return Result;
}

void OLED_ShowNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
    uint8_t i;
    for (i = 0; i < Length; i++)
    {
        OLED_ShowChar(Line, Column + i, Number / OLED_Pow(10, Length - i - 1) % 10 + '0');
    }
}

void OLED_ShowSignedNum(uint8_t Line, uint8_t Column, int32_t Number, uint8_t Length)
{
    uint8_t i;
    uint32_t Number1;
    if (Number >= 0)
    {
        OLED_ShowChar(Line, Column, '+');
        Number1 = Number;
    }
    else
    {
        OLED_ShowChar(Line, Column, '-');
        Number1 = -Number;
    }
    for (i = 0; i < Length; i++)
    {
        OLED_ShowChar(Line, Column + i + 1, Number1 / OLED_Pow(10, Length - i - 1) % 10 + '0');
    }
}

void OLED_ShowHexNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
    uint8_t i, SingleNumber;
    for (i = 0; i < Length; i++)
    {
        SingleNumber = Number / OLED_Pow(16, Length - i - 1) % 16;
        if (SingleNumber < 10)
        {
            OLED_ShowChar(Line, Column + i, SingleNumber + '0');
        }
        else
        {
            OLED_ShowChar(Line, Column + i, SingleNumber - 10 + 'A');
        }
    }
}

void OLED_ShowBinNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
    uint8_t i;
    for (i = 0; i < Length; i++)
    {
        OLED_ShowChar(Line, Column + i, Number / OLED_Pow(2, Length - i - 1) % 2 + '0');
    }
}

void OLED_Init(void)
{
    uint32_t i, j;

    for (i = 0; i < 1000; i++)
    {
        for (j = 0; j < 1000; j++);
    }

    OLED_I2C_Init();

    OLED_WriteCommand(0xAE);

    OLED_WriteCommand(0xD5);
    OLED_WriteCommand(0x80);

    OLED_WriteCommand(0xA8);
    OLED_WriteCommand(0x3F);

    OLED_WriteCommand(0xD3);
    OLED_WriteCommand(0x00);

    OLED_WriteCommand(0x40);

    OLED_WriteCommand(0xA1);

    OLED_WriteCommand(0xC8);

    OLED_WriteCommand(0xDA);
    OLED_WriteCommand(0x12);

    OLED_WriteCommand(0x81);
    OLED_WriteCommand(0xCF);

    OLED_WriteCommand(0xD9);
    OLED_WriteCommand(0xF1);

    OLED_WriteCommand(0xDB);
    OLED_WriteCommand(0x30);

    OLED_WriteCommand(0xA4);

    OLED_WriteCommand(0xA6);

    OLED_WriteCommand(0x8D);
    OLED_WriteCommand(0x14);

    OLED_WriteCommand(0xAF);

    OLED_Clear();
}
