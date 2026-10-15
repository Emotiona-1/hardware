#ifndef __SERIAL_H
#define __SERIAL_H

#include "NS800RTxxxx.h"
#include <stdio.h>

void Serial1_Init(void);
void Serial1_SendByte(uint8_t Byte);
void Serial1_SendString(char *String);
void Serial1_Printf(char *format, ...);
uint8_t Serial1_GetRxFlag(void);
uint8_t Serial1_GetRxData(void);

void Serial2_Init(void);
void Serial2_SendByte(uint8_t Byte);
void Serial2_SendString(char *String);
void Serial2_Printf(char *format, ...);
uint8_t Serial2_GetRxFlag(void);
uint8_t Serial2_GetRxData(void);

#endif
