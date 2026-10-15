#ifndef __VIBRATION_MOTOR_H
#define __VIBRATION_MOTOR_H

#include "NS800RTxxxx.h"

#define VIBRATION_MOTOR_GPIO_PORT    GPIOA
#define VIBRATION_MOTOR_GPIO_PIN     GPIO_PIN_14
#define VIBRATION_MOTOR_GPIO_MUX     ALT0_FUNCTION

#define VIBRATION_MOTOR_PWM          EPWM8
#define VIBRATION_MOTOR_PWM_MUX      ALT1_FUNCTION

void VibrationMotor_Init(void);

void VibrationMotor_On(void);

void VibrationMotor_Off(void);

void VibrationMotor_Toggle(void);

void VibrationMotor_Vibrate(uint16_t ms);

void VibrationMotor_PWM_Init(uint16_t period, uint16_t prescaler);

void VibrationMotor_SetIntensity(uint16_t intensity);

#endif
