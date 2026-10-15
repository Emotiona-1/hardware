#include "NS800RTxxxx.h"
#include "delay.h"
#include "motor.h"
#include "Serial.h"
#include "Servo.h"
#include "vibration_motor.h"
#include "Ultrasonic.h"
#include "DCMotor.h"
#include "Control.h"

float distance;

static void System_Init(void)
{
    Device_init();
    Device_unlockPeriphReg();
    Interrupt_initModule();
    Interrupt_initVectorTable();

    delay_init();
    Motor_Init();
    Motor_Enable();
    Serial2_Init();
    SERVO_Init();
    VibrationMotor_Init();
    DCMotor_Init();
    Ultrasonic_Init();

    SERVO_LoadOrUnload(1, 1);
    Delay_ms(100);
    SERVO_LoadOrUnload(2, 1);
    Delay_ms(100);
    SERVO_LoadOrUnload(3, 1);
    Delay_ms(100);

    SERVO_MoveTimeWrite(1, 400, 500);
    Delay_ms(600);
    SERVO_MoveTimeWrite(2, 600, 1000);
    Delay_ms(1100);
    SERVO_MoveTimeWrite(2, 400, 1000);
    Delay_ms(1100);
    SERVO_MoveTimeWrite(3, 1000, 1000);
    Delay_ms(1100);
    SERVO_MoveTimeWrite(3, 0, 1000);
    Delay_ms(1100);

    VibrationMotor_On();
    Delay_ms(1000);
    VibrationMotor_Off();
    Delay_ms(1000);

    distance = Ultrasonic_GetDistance();

    DCMotor_SetSpeed(0);

    DCMotor_SetSpeed(50);
    Delay_ms(1000);
    DCMotor_SetSpeed(0);
}

int main(void)
{
    System_Init();

    while (1)
    {
        Control_MainLoop();
    }
}
