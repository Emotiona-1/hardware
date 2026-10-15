#ifndef __MOTOR_H
#define __MOTOR_H
#include "NS800RTxxxx.h"

#define MOTOR_PUL_PORT     GPIOA
#define MOTOR_PUL_PIN      GPIO_PIN_2
#define MOTOR_DIR_PORT     GPIOA
#define MOTOR_DIR_PIN      GPIO_PIN_3
#define MOTOR_ENA_PORT     GPIOA
#define MOTOR_ENA_PIN      GPIO_PIN_4

#define DIR_CW             0
#define DIR_CCW            1

void Motor_Init(void);
void Motor_Enable(void);
void Motor_Disable(void);
void Motor_SetDirection(uint8_t dir);
void Motor_OnePulse(void);
void Motor_OnePulse_Slow(void);
void Motor_RunSteps(uint32_t steps);
void Motor_RunSteps_Slow(uint32_t steps);

#endif
