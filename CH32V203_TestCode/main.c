#include "ch32v20x.h"
#include "CH32V203G6U6_ButtonLight.h"

#include "HardSPI.h"
#include "YUERAN_OLEDspi.h"
#include "HardSPI_W25Qxx.h"
#include "USART.h"

#include "YUERAN_Define.h"
#include "YUERAN_LoadE.h"
#include "YUERAN_AlphaE.h"
#include "YUERAN_EtoFRAME.h"
#include "YUERAN_RefrashFRAME.h"
#include "YUERAN_FcnE.h"
#include "YUERAN_CmdLoad.h"

void ModePin_Init(void)
{
    GPIO_InitTypeDef  GPIO_InitStructure = {0};

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
}

void ModePin_Switch(uint8_t* Flag)
{
    if(GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_7) == SET)
    {
        if(*Flag == 0)
        {
            *Flag = 1;
            USART1_OFF();
            USART2_ON();
            Delay_Ms(10);
        }
    }
    else
    {
        if(*Flag == 1)
        {
            *Flag = 0;
            USART2_OFF();
            USART1_ON();
            Delay_Ms(10);
        }
    }
}

int main(void)
{
    uint8_t Mode = 1, i;

    SystemCoreClockUpdate();
    Delay_Init();
    ButtonLight_Init();

    HardSPI_Init();
    W25Qxx_Init();
    OLED_Init(OLED_AddressingMode_HorizontalMode, OLED_SetMode_X_Flip_Normal, OLED_SetMode_Y_Flip_Normal, OLED_SetMode_Color_Normal, 255);
    ModePin_Init();

    USART_Setup();
    TIM2_40ms_Init();


    while(1)
    {
//        ModePin_Switch(&Mode);



        // Render mode
//        if(Mode == 1)
//        {



            if(ReadFlag_TIM2())
            {
                for(i = 0 ; i < 8 ; i++)
                {
                    if(E_Layer[i].Type)
                    {
                        LoadE(i);
                        AlphaE(i);
                        EtoFRAME(i);
                        FcnE(i);
                    }
                }
                RefrashFRAME(FRAME_ClnFlag);
                BackDataSend();
            }



            if(USART_GetFlagStatus(USART1, USART_FLAG_IDLE) == SET)
            {
                USART_ClearFlag(USART1, USART_FLAG_IDLE);
                if(DMA_GetFlagStatus(DMA1_FLAG_TC5) == SET) {RenderCmdLoad();}
                else                                        {RenderCmd_DMAreset();}
            }
            if(USART_GetFlagStatus(USART1, USART_FLAG_ORE) == SET)
            {
                USART_ClearFlag(USART1, USART_FLAG_ORE);
            }
//            RenderCmdLoad();
//        }
        // Flash mode
//        if(Mode == 0)
//        {
//
//        }
    }
}
