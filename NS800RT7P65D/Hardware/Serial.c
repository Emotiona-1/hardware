#include "NS800RTxxxx.h"
#include "Serial.h"
#include <stdio.h>
#include <stdarg.h>

#define SERIAL1_PORT_TX    GPIOA
#define SERIAL1_PIN_TX     GPIO_PIN_12
#define SERIAL1_MUX_TX     ALT6_FUNCTION
#define SERIAL1_PORT_RX    GPIOA
#define SERIAL1_PIN_RX     GPIO_PIN_13
#define SERIAL1_MUX_RX     ALT6_FUNCTION

#define SERIAL2_PORT_TX    GPIOB
#define SERIAL2_PIN_TX     GPIO_PIN_6
#define SERIAL2_MUX_TX     ALT1_FUNCTION
#define SERIAL2_PORT_RX    GPIOB
#define SERIAL2_PIN_RX     GPIO_PIN_7
#define SERIAL2_MUX_RX     ALT1_FUNCTION

#define SERIAL_BAUDRATE    115200U

volatile uint8_t Serial1_RxData;
volatile uint8_t Serial1_RxFlag;

volatile uint8_t Serial2_RxData;
volatile uint8_t Serial2_RxFlag;

static void Serial_TxPinConfig(GPIO_TypeDef *port, GPIO_PinNum pin, GPIO_AltFunc mux)
{
    GPIO_setPinConfig(port, pin, mux);
    GPIO_setAnalogMode(port, pin, GPIO_ANALOG_DISABLED);
    GPIO_setPadConfig(port, pin, GPIO_PIN_TYPE_STD);
    GPIO_setQualificationMode(port, pin, GPIO_QUAL_SYNC);
    GPIO_setDirectionMode(port, pin, GPIO_DIR_MODE_OUT);
}

static void Serial_RxPinConfig(GPIO_TypeDef *port, GPIO_PinNum pin, GPIO_AltFunc mux)
{
    GPIO_setPinConfig(port, pin, mux);
    GPIO_setAnalogMode(port, pin, GPIO_ANALOG_DISABLED);
    GPIO_setPadConfig(port, pin, GPIO_PIN_TYPE_PULLUP);
    GPIO_setQualificationMode(port, pin, GPIO_QUAL_SYNC);
    GPIO_setDirectionMode(port, pin, GPIO_DIR_MODE_IN);
}

static void UART1_Rx_Isr(void);
static void UART2_Rx_Isr(void);

void Serial1_Init(void)
{
    Serial_TxPinConfig(SERIAL1_PORT_TX, SERIAL1_PIN_TX, SERIAL1_MUX_TX);
    Serial_RxPinConfig(SERIAL1_PORT_RX, SERIAL1_PIN_RX, SERIAL1_MUX_RX);

    UART_resetModule(UART1);
    UART_setBaud(UART1, SERIAL_BAUDRATE);
    UART_setBitCountPerChar(UART1, UART_8_BITS_PER_CHAR, false);
    UART_setStopBitCount(UART1, UART_ONE_STOP_BIT);
    UART_setMSB(UART1, false);
    UART_disableTxFifo(UART1);
    UART_disableRxFifo(UART1);
    UART_setRxIdleCharacter(UART1, UART_IDLE_CHARACTER_CNT0);
    UART_enableTxModule(UART1);
    UART_enableRxModule(UART1);

    UART_enableInterrupt(UART1, UART_INT_RX_DATA_REG_FULL);
    Interrupt_register(UART1_RX_IRQn, &UART1_Rx_Isr);
    Interrupt_enable(UART1_RX_IRQn);
}

void Serial2_Init(void)
{
    Serial_TxPinConfig(SERIAL2_PORT_TX, SERIAL2_PIN_TX, SERIAL2_MUX_TX);
    Serial_RxPinConfig(SERIAL2_PORT_RX, SERIAL2_PIN_RX, SERIAL2_MUX_RX);

    UART_resetModule(UART2);
    UART_setBaud(UART2, SERIAL_BAUDRATE);
    UART_setBitCountPerChar(UART2, UART_8_BITS_PER_CHAR, false);
    UART_setStopBitCount(UART2, UART_ONE_STOP_BIT);
    UART_setMSB(UART2, false);
    UART_disableTxFifo(UART2);
    UART_disableRxFifo(UART2);
    UART_setRxIdleCharacter(UART2, UART_IDLE_CHARACTER_CNT0);
    UART_enableTxModule(UART2);
    UART_enableRxModule(UART2);

    UART_enableInterrupt(UART2, UART_INT_RX_DATA_REG_FULL);
    Interrupt_register(UART2_RX_IRQn, &UART2_Rx_Isr);
    Interrupt_enable(UART2_RX_IRQn);
}

void Serial1_SendByte(uint8_t Byte)
{
    while (!UART_isSpaceAvailable(UART1))
    {
    }
    UART_writeChar(UART1, Byte);
}

uint8_t Serial1_GetRxFlag(void)
{
    if (Serial1_RxFlag == 1)
    {
        Serial1_RxFlag = 0;
        return 1;
    }
    return 0;
}

uint8_t Serial1_GetRxData(void)
{
    return Serial1_RxData;
}

void Serial1_SendString(char *String)
{
    uint8_t i;
    for (i = 0; String[i] != '\0'; i++)
    {
        Serial1_SendByte(String[i]);
    }
}

void Serial1_Printf(char *format, ...)
{
    char String[100];
    va_list arg;
    va_start(arg, format);
    vsprintf(String, format, arg);
    va_end(arg);
    Serial1_SendString(String);
}

void Serial2_SendByte(uint8_t Byte)
{
    while (!UART_isSpaceAvailable(UART2))
    {
    }
    UART_writeChar(UART2, Byte);
}

uint8_t Serial2_GetRxFlag(void)
{
    if (Serial2_RxFlag == 1)
    {
        Serial2_RxFlag = 0;
        return 1;
    }
    return 0;
}

uint8_t Serial2_GetRxData(void)
{
    return Serial2_RxData;
}

void Serial2_SendString(char *String)
{
    uint8_t i;
    for (i = 0; String[i] != '\0'; i++)
    {
        Serial2_SendByte(String[i]);
    }
}

void Serial2_Printf(char *format, ...)
{
    char String[100];
    va_list arg;
    va_start(arg, format);
    vsprintf(String, format, arg);
    va_end(arg);
    Serial2_SendString(String);
}

static void UART1_Rx_Isr(void)
{
    if (UART_getStatusFlag(UART1, UART_RX_DATA_REG_FULL))
    {
        Serial1_RxData = (uint8_t)UART_readChar(UART1);
        Serial1_RxFlag = 1;
    }
}

static void UART2_Rx_Isr(void)
{
    if (UART_getStatusFlag(UART2, UART_RX_DATA_REG_FULL))
    {
        Serial2_RxData = (uint8_t)UART_readChar(UART2);
        Serial2_RxFlag = 1;
    }
}
