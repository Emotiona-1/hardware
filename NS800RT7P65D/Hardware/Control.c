#include "Control.h"
#include "Serial.h"
#include "track.h"
#include "Servo.h"
#include "vibration_motor.h"
#include "DCMotor.h"
#include "Ultrasonic.h"
#include "delay.h"
#include <string.h>
#include <stdio.h>

#define RX_BUFFER_SIZE   32
#define TELESCOPIC_ID        3
#define TELESCOPIC_POS_MIN   0
#define TELESCOPIC_POS_MAX   1000
#define TELESCOPIC_DIST_THRESHOLD_CM   5.0f
#define TELESCOPIC_POS_PER_CM          200.0f
#define TELESCOPIC_MOVE_TIME 1000
#define TELESCOPIC_WAIT_MS   1500
#define TELESCOPIC_HOLD_MS   5000

static char    s_rx_buffer[RX_BUFFER_SIZE];
static uint8_t s_rx_index = 0;
static uint8_t s_mode     = MODE_NONE;
static int16_t s_last_y   = 0;

static void Control_ProcessLine(char *line);
static void Control_OnCoordinate(int16_t angle, int16_t y);
static void Control_RunModeAction(void);
static void Control_ReturnHome(void);

void Control_ServoMoveByY(int16_t y)
{
    if (y < 0)
        y = 0;
    else if (y > 200)
        y = 200;
    int16_t motor1 = (int16_t)(350 - ((int32_t)y * 350 / 200));
    int16_t motor2 = (int16_t)(400 - y * 2 + 110);
    SERVO_MoveTimeWrite(1, motor1, 500);
    SERVO_MoveTimeWrite(2, motor2, 500);
    Delay_ms(3000);
}

void Control_ServoMoveByDistance(float distance)
{
    Delay_ms(2000);
    uint16_t pos;

    if (distance <= 0.0f)
        distance = 0.0f;
    else if (distance > TELESCOPIC_DIST_THRESHOLD_CM)
        distance = TELESCOPIC_DIST_THRESHOLD_CM;
    pos = (uint16_t)(distance * TELESCOPIC_POS_PER_CM);

    SERVO_MoveTimeWrite(TELESCOPIC_ID, pos, TELESCOPIC_MOVE_TIME);
    Delay_ms(TELESCOPIC_HOLD_MS);
}

void Control_ModeVibration(void)
{
    Delay_ms(2000);
    float distance;
    distance = Ultrasonic_GetDistance();
    Control_ServoMoveByDistance(distance);
    VibrationMotor_On();
    Delay_ms(1000);
    VibrationMotor_Off();
}

void Control_ModeBlow(void)
{
    Delay_ms(2000);
    DCMotor_SetSpeed(100);
    Delay_ms(3000);
    DCMotor_SetSpeed(0);
}

void Control_ModeTouch(void)
{
    Delay_ms(2000);
    float distance;
    distance = Ultrasonic_GetDistance();
    Control_ServoMoveByDistance(distance);
}

static void Control_ReturnHome(void)
{
    SERVO_MoveTimeWrite(TELESCOPIC_ID, 0, TELESCOPIC_MOVE_TIME);
    Delay_ms(2000);

    SERVO_MoveTimeWrite(1, 350, 1000);
    SERVO_MoveTimeWrite(2, 400, 1000);
    Delay_ms(2000);

    Motor_TrackX(0);
    Delay_ms(2000);
}

static void Control_OnCoordinate(int16_t angle, int16_t y)
{
    if (s_mode == MODE_NONE)
    {
        Serial2_SendString("noMode\r\n");
        return;
    }

    Motor_TrackX(angle);

    Control_ServoMoveByY(y);

    Control_RunModeAction();

    Control_ReturnHome();

    Serial2_SendString("workOK\r\n");
}

static void Control_RunModeAction(void)
{
    switch (s_mode)
    {
        case MODE_VIBRATION: Control_ModeVibration(); break;
        case MODE_BLOW:      Control_ModeBlow();      break;
        case MODE_TOUCH:     Control_ModeTouch();     break;
        case MODE_NONE:
        default:
            break;
    }
}

static void Control_ProcessLine(char *line)
{
    int16_t angle, y;

    if (strcmp(line, "one")   == 0) { s_mode = MODE_VIBRATION; return; }
    if (strcmp(line, "two")   == 0) { s_mode = MODE_BLOW;      return; }
    if (strcmp(line, "three") == 0) { s_mode = MODE_TOUCH;     return; }

    y = s_last_y;
    if (sscanf(line, "%hd,%hd", &angle, &y) >= 1)
    {
        if (angle >  90) angle =  90;
        if (angle < -90) angle = -90;

        s_last_y = y;
        Control_OnCoordinate(angle, y);
    }
}

void Control_MainLoop(void)
{
    uint8_t data;

    if (Serial2_GetRxFlag() == 1)
    {
        data = Serial2_GetRxData();

        if (data == '\n' || data == '\r')
        {
            if (s_rx_index > 0)
            {
                s_rx_buffer[s_rx_index] = '\0';
                Control_ProcessLine(s_rx_buffer);
                s_rx_index = 0;
            }
        }
        else
        {
            if (s_rx_index < RX_BUFFER_SIZE - 1)
            {
                s_rx_buffer[s_rx_index++] = data;
            }
            else
            {
                s_rx_index = 0;
            }
        }
    }
}

uint8_t Control_GetMode(void)
{
    return s_mode;
}
