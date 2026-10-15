#include "Servo.h"
#include "delay.h"

#define SERVO_UART          UART1
#define SERVO_PORT_TX       GPIOA
#define SERVO_PIN_TX        GPIO_PIN_12
#define SERVO_MUX_TX        ALT6_FUNCTION
#define SERVO_PORT_RX       GPIOA
#define SERVO_PIN_RX        GPIO_PIN_13
#define SERVO_MUX_RX        ALT6_FUNCTION
#define SERVO_BAUDRATE      115200U

static void SERVO_SendByte(uint8_t byte);
static uint8_t SERVO_CalcChecksum(uint8_t *data, uint8_t len);
static void SERVO_SendPacket(uint8_t *data, uint8_t len);

void SERVO_Init(void)
{
    GPIO_setPinConfig(SERVO_PORT_TX, SERVO_PIN_TX, SERVO_MUX_TX);
    GPIO_setAnalogMode(SERVO_PORT_TX, SERVO_PIN_TX, GPIO_ANALOG_DISABLED);
    GPIO_setPadConfig(SERVO_PORT_TX, SERVO_PIN_TX, GPIO_PIN_TYPE_STD);
    GPIO_setQualificationMode(SERVO_PORT_TX, SERVO_PIN_TX, GPIO_QUAL_SYNC);
    GPIO_setDirectionMode(SERVO_PORT_TX, SERVO_PIN_TX, GPIO_DIR_MODE_OUT);

    GPIO_setPinConfig(SERVO_PORT_RX, SERVO_PIN_RX, SERVO_MUX_RX);
    GPIO_setAnalogMode(SERVO_PORT_RX, SERVO_PIN_RX, GPIO_ANALOG_DISABLED);
    GPIO_setPadConfig(SERVO_PORT_RX, SERVO_PIN_RX, GPIO_PIN_TYPE_PULLUP);
    GPIO_setQualificationMode(SERVO_PORT_RX, SERVO_PIN_RX, GPIO_QUAL_SYNC);
    GPIO_setDirectionMode(SERVO_PORT_RX, SERVO_PIN_RX, GPIO_DIR_MODE_IN);

    UART_resetModule(SERVO_UART);
    UART_setBaud(SERVO_UART, SERVO_BAUDRATE);
    UART_setBitCountPerChar(SERVO_UART, UART_8_BITS_PER_CHAR, false);
    UART_setStopBitCount(SERVO_UART, UART_ONE_STOP_BIT);
    UART_setMSB(SERVO_UART, false);
    UART_disableTxFifo(SERVO_UART);
    UART_disableRxFifo(SERVO_UART);
    UART_setRxIdleCharacter(SERVO_UART, UART_IDLE_CHARACTER_CNT0);
    UART_enableTxModule(SERVO_UART);
    UART_enableRxModule(SERVO_UART);
}

static void SERVO_SendByte(uint8_t byte)
{
    while (!UART_isSpaceAvailable(SERVO_UART))
    {
    }
    UART_writeChar(SERVO_UART, byte);
}

static uint8_t SERVO_CalcChecksum(uint8_t *data, uint8_t len)
{
    uint16_t sum = 0;
    uint8_t i;
    for (i = 0; i < len; i++)
    {
        sum += data[i];
    }
    return ~((uint8_t)(sum & 0xFF));
}

static void SERVO_SendPacket(uint8_t *data, uint8_t len)
{
    uint8_t checksum;
    uint8_t i;

    checksum = SERVO_CalcChecksum(data, len);

    SERVO_SendByte(SERVO_FRAME_HEADER);
    SERVO_SendByte(SERVO_FRAME_HEADER);

    for (i = 0; i < len; i++)
    {
        SERVO_SendByte(data[i]);
    }

    SERVO_SendByte(checksum);

    while (SERVO_UART->STAT.BIT.TC == false)
    {
    }
}

void SERVO_MoveTimeWrite(uint8_t id, uint16_t angle, uint16_t time_ms)
{
    uint8_t data[7];

    data[0] = id;
    data[1] = 7;
    data[2] = SERVO_MOVE_TIME_WRITE;
    data[3] = (uint8_t)(angle & 0xFF);
    data[4] = (uint8_t)((angle >> 8) & 0xFF);
    data[5] = (uint8_t)(time_ms & 0xFF);
    data[6] = (uint8_t)((time_ms >> 8) & 0xFF);

    SERVO_SendPacket(data, 7);
}

void SERVO_MoveTimeWaitWrite(uint8_t id, uint16_t angle, uint16_t time_ms)
{
    uint8_t data[7];

    data[0] = id;
    data[1] = 7;
    data[2] = SERVO_MOVE_TIME_WAIT_WRITE;
    data[3] = (uint8_t)(angle & 0xFF);
    data[4] = (uint8_t)((angle >> 8) & 0xFF);
    data[5] = (uint8_t)(time_ms & 0xFF);
    data[6] = (uint8_t)((time_ms >> 8) & 0xFF);

    SERVO_SendPacket(data, 7);
}

void SERVO_MoveStart(uint8_t id)
{
    uint8_t data[3];

    data[0] = id;
    data[1] = 3;
    data[2] = SERVO_MOVE_START;

    SERVO_SendPacket(data, 3);
}

void SERVO_MoveStop(uint8_t id)
{
    uint8_t data[3];

    data[0] = id;
    data[1] = 3;
    data[2] = SERVO_MOVE_STOP;

    SERVO_SendPacket(data, 3);
}

void SERVO_LoadOrUnload(uint8_t id, uint8_t load)
{
    uint8_t data[4];

    data[0] = id;
    data[1] = 4;
    data[2] = SERVO_LOAD_OR_UNLOAD_WRITE;
    data[3] = load ? 1 : 0;

    SERVO_SendPacket(data, 4);
}

void SERVO_AngleLimitWrite(uint8_t id, uint16_t min_angle, uint16_t max_angle)
{
    uint8_t data[7];

    data[0] = id;
    data[1] = 7;
    data[2] = SERVO_ANGLE_LIMIT_WRITE;
    data[3] = (uint8_t)(min_angle & 0xFF);
    data[4] = (uint8_t)((min_angle >> 8) & 0xFF);
    data[5] = (uint8_t)(max_angle & 0xFF);
    data[6] = (uint8_t)((max_angle >> 8) & 0xFF);

    SERVO_SendPacket(data, 7);
}

void SERVO_IdWrite(uint8_t id, uint8_t new_id)
{
    uint8_t data[4];

    data[0] = id;
    data[1] = 4;
    data[2] = SERVO_ID_WRITE;
    data[3] = new_id;

    SERVO_SendPacket(data, 4);
}

void SERVO_SetAngleOffset(uint8_t id, int8_t offset)
{
    uint8_t data[4];

    data[0] = id;
    data[1] = 4;
    data[2] = SERVO_ANGLE_OFFSET_ADJUST;
    data[3] = (uint8_t)offset;

    SERVO_SendPacket(data, 4);
}

void SERVO_LedCtrl(uint8_t id, uint8_t on_off)
{
    uint8_t data[4];

    data[0] = id;
    data[1] = 4;
    data[2] = SERVO_LED_CTRL_WRITE;
    data[3] = on_off ? 1 : 0;

    SERVO_SendPacket(data, 4);
}
