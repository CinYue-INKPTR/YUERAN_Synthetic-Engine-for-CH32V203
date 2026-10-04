#include "ch32v20x.h"
#include "TIM.h"
#include "YUERAN_Define.h"

#define TIM_PRESCALER    (SystemCoreClock / 1000000 - 1) // 预分频器，使计数频率为 1MHz (1us/tick)
#define TIM_PERIOD       (40000 - 1)                     // 自动重装载值，40ms = 40000us

void TIM2_40ms_Init(void)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure = {0};

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    TIM_TimeBaseInitStructure.TIM_Prescaler = TIM_PRESCALER;
    TIM_TimeBaseInitStructure.TIM_Period = TIM_PERIOD;
    TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseInitStructure);

    TIM_ClearFlag(TIM2, TIM_FLAG_Update);

    TIM_Cmd(TIM2, ENABLE);
}

uint8_t ReadFlag_TIM2(void)
{
    // If there is no record of the rendering time, then the current value represents the frame rendering time. Record it.
    if(Developer_FrameRenderingTime == 0)
    {
        Developer_FrameRenderingTime = TIM_GetCounter(TIM2);
    }
    // If the timer reaches 40ms, reset the timer and the frame duration record, and start a frame rendering.
    if (TIM_GetFlagStatus(TIM2, TIM_FLAG_Update) == SET)
    {
        Developer_FrameRenderingTime = 0;
        TIM_ClearFlag(TIM2, TIM_FLAG_Update);
        TIM_SetCounter(TIM2, 0);
        return 1;
    }
    else
    {
        return 0;
    }
}
