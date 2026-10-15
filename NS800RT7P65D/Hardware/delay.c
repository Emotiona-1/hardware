#include "NS800RTxxxx.h"
#include "delay.h"

#define DELAY_SYSTICK_MAX_LOAD   0x00FFFFFFUL

static uint32_t fac_us = 0;

static void delay_ticks(uint32_t ticks)
{
    uint32_t chunk;

    while (ticks > 0U)
    {
        chunk = (ticks > DELAY_SYSTICK_MAX_LOAD) ? DELAY_SYSTICK_MAX_LOAD : ticks;

        SysTick->LOAD = chunk - 1U;
        SysTick->VAL  = 0U;
        SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;

        while ((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) == 0U)
        {
        }

        SysTick->CTRL = 0U;
        SysTick->VAL  = 0U;

        ticks -= chunk;
    }
}

void delay_init(void)
{
    SystemCoreClockUpdate();

    fac_us = SystemCoreClock / 1000000U;

    if (fac_us == 0U)
    {
        fac_us = 1U;
    }

    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL  = 0U;
}

void delay_us(uint32_t nus)
{
    if (nus == 0U)
    {
        return;
    }

    delay_ticks(nus * fac_us);
}

void delay_ms(uint32_t nms)
{
    while (nms > 0U)
    {
        delay_us(1000U);
        nms--;
    }
}
