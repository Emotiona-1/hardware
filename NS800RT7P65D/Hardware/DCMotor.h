#ifndef __DCMOTOR_H
#define __DCMOTOR_H

#include "NS800RTxxxx.h"

#define DCMOTOR_NSLEEP_PORT   GPIOA
#define DCMOTOR_NSLEEP_PIN    GPIO_PIN_8
#define DCMOTOR_NSLEEP_MUX    ALT0_FUNCTION

void DCMotor_Init(void);
void DCMotor_SetSpeed(int8_t Speed);

#endif
