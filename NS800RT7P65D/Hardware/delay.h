#ifndef __DELAY_H
#define __DELAY_H

#include "NS800RTxxxx.h"

void delay_init(void);
void delay_us(uint32_t nus);
void delay_ms(uint32_t nms);

#define Delay_us(nus)  delay_us(nus)
#define Delay_ms(nms)  delay_ms(nms)

#endif
