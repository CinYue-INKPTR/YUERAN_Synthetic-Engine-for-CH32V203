#include "TIM.h"

#include "ch32v20x.h"
#include "YUERAN_Define.h"





void TIM_Init(void)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure = {0};
    NVIC_InitTypeDef NVIC_InitStructure = {0};

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
    TIM_TimeBaseInitStructure.TIM_Prescaler     = TIM_PRESCALER - 1;
    TIM_TimeBaseInitStructure.TIM_Period        = 1000/TIM_FPS*1000 - 1;
    TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInitStructure.TIM_CounterMode   = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseInitStructure);

    NVIC_InitStructure.NVIC_IRQChannel                      = TIM2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority    = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority           = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd                   = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    TIM_SetCounter(TIM2, 0);
    TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
    TIM_Cmd(TIM2, ENABLE);
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);
}





void TIM2_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void TIM2_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM2, TIM_IT_Update) == SET)
    {
		// Clear Flag
        TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
		// Start Frame Rendering
        GlobalFlag &= ~GlobalFlag_WaitRendering;
        // For RenderingTime
        GlobalFlag |= GlobalFlag_TimeFull;

        if (GlobalFlag & GlobalFlag_USART_CmdIn)   {USART_BUFFER_TomeoutCNT++;}
        if (USART_BUFFER_TomeoutCNT > 1)
        {
            USART_BUFFER_CNT = 0;
            USART_BUFFER_TomeoutCNT = 0;
            GlobalFlag &= ~GlobalFlag_USART_CmdIn;
        }
    }
}
