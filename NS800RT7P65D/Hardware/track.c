#include "track.h"
#include "motor.h"
#include <stdlib.h>

#define PULSE_PER_DEGREE    (1600.0 / 360.0)
#define DEAD_ZONE_ANGLE     2

static int16_t s_current_angle = 0;

void Motor_TrackX(int16_t target_angle)
{
    int16_t delta;
    uint32_t steps;

    delta = target_angle - s_current_angle;

    if (abs(delta) < DEAD_ZONE_ANGLE)
    {
        return;
    }

    steps = (uint32_t)(abs(delta) * PULSE_PER_DEGREE * 3);

    if (delta > 0)
    {
        Motor_SetDirection(DIR_CW);
    }
    else
    {
        Motor_SetDirection(DIR_CCW);
    }

    Motor_RunSteps_Slow(steps);

    s_current_angle = target_angle;
}
