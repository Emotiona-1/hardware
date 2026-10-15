#ifndef __CONTROL_H
#define __CONTROL_H

#include "NS800RTxxxx.h"

#define MODE_NONE        0
#define MODE_VIBRATION   1
#define MODE_BLOW        2
#define MODE_TOUCH       3

void Control_MainLoop(void);

uint8_t Control_GetMode(void);

void Control_ServoMoveByY(int16_t y);

void Control_ServoMoveByDistance(float distance);

void Control_ModeVibration(void);

void Control_ModeBlow(void);

void Control_ModeTouch(void);

#endif
