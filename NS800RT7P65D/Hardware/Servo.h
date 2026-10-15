#ifndef __SERVO_H
#define __SERVO_H

#include "NS800RTxxxx.h"

#define SERVO_FRAME_HEADER          0x55

#define SERVO_MOVE_TIME_WRITE       1
#define SERVO_MOVE_TIME_WAIT_WRITE  7
#define SERVO_MOVE_START            11
#define SERVO_MOVE_STOP             12
#define SERVO_ID_WRITE              13
#define SERVO_ANGLE_OFFSET_ADJUST   17
#define SERVO_ANGLE_OFFSET_WRITE    18
#define SERVO_ANGLE_LIMIT_WRITE     20
#define SERVO_VIN_LIMIT_WRITE       22
#define SERVO_TEMP_MAX_LIMIT_WRITE  24
#define SERVO_OR_MOTOR_MODE_WRITE   29
#define SERVO_LOAD_OR_UNLOAD_WRITE  31
#define SERVO_LED_CTRL_WRITE        33
#define SERVO_LED_ERROR_WRITE       35

#define SERVO_MOVE_TIME_READ        2
#define SERVO_MOVE_TIME_WAIT_READ   8
#define SERVO_ID_READ               14
#define SERVO_ANGLE_OFFSET_READ     19
#define SERVO_ANGLE_LIMIT_READ      21
#define SERVO_VIN_LIMIT_READ        23
#define SERVO_TEMP_MAX_LIMIT_READ   25
#define SERVO_TEMP_READ             26
#define SERVO_VIN_READ              27
#define SERVO_POS_READ              28
#define SERVO_OR_MOTOR_MODE_READ    30
#define SERVO_LOAD_OR_UNLOAD_READ   32
#define SERVO_LED_CTRL_READ         34
#define SERVO_LED_ERROR_READ        36
#define SERVO_DIS_READ              48

#define SERVO_BROADCAST_ID          254

void SERVO_Init(void);
void SERVO_MoveTimeWrite(uint8_t id, uint16_t angle, uint16_t time_ms);
void SERVO_MoveTimeWaitWrite(uint8_t id, uint16_t angle, uint16_t time_ms);
void SERVO_MoveStart(uint8_t id);
void SERVO_MoveStop(uint8_t id);
void SERVO_LoadOrUnload(uint8_t id, uint8_t load);
void SERVO_AngleLimitWrite(uint8_t id, uint16_t min_angle, uint16_t max_angle);
void SERVO_IdWrite(uint8_t id, uint8_t new_id);
void SERVO_SetAngleOffset(uint8_t id, int8_t offset);
void SERVO_LedCtrl(uint8_t id, uint8_t on_off);

#endif
