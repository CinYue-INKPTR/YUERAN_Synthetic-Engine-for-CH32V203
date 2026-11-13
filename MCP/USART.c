// v1.0.0.261113
#include "USART.h"

#include "ch32v20x.h"
#include "YUERAN_Define.h"
#include "YUERAN_CmdLoad.h"
#include "TIM.h"

#define USART_Port      GPIOA
#define USART1_TXD_Pin  GPIO_Pin_9
#define USART1_RXD_Pin  GPIO_Pin_10
#define USART2_TXD_Pin  GPIO_Pin_2
#define USART2_RXD_Pin  GPIO_Pin_3

#define USART1_TX_DMA_CHANNEL   DMA1_Channel4





void GetRenderingTime(void)
{
    RenderingTime = TIM_GetCounter(TIM2);
    RenderingTime = RenderingTime*100 / (1000/TIM_FPS*1000 - 1);
    if (GlobalFlag & GlobalFlag_TimeFull)   {RenderingTime = 100;}
}





void USART1_SendData(void)
{
    uint8_t i, Add_p = 0;

    // if (DMA_GetFlagStatus(DMA1_FLAG_TC4) == RESET)  {return;}

    USART_BackData[Add_p++] = BackData_Head;
    for (i = 0 ; i < E_Layer_Size ; i++)
	{
	    USART_BackData[Add_p++] = E_Layer[i].Xpos;
	    USART_BackData[Add_p++] = E_Layer[i].Ypos;
	    USART_BackData[Add_p++] = E_Layer[i].Alpha;
	    USART_BackData[Add_p++] = E_Layer[i].Block_Xsize;
	    USART_BackData[Add_p++] = E_Layer[i].Block_Ysize;
	    USART_BackData[Add_p++] = (uint8_t)(E_Layer[i].StreamAdd >> 8);
	    USART_BackData[Add_p++] = (uint8_t)(E_Layer[i].StreamAdd);
	}
    if (RenderingTime > 99) {USART_BackData[Add_p++] = 0xff;}
    else                    {USART_BackData[Add_p++] = ((RenderingTime / 10) << 4) + (RenderingTime % 10);}

    // Close DMA
    USART_DMACmd(USART1, USART_DMAReq_Tx, DISABLE);
    DMA_Cmd(USART1_TX_DMA_CHANNEL, DISABLE);
    // Clear Flag
    DMA_ClearFlag(DMA1_FLAG_TC4 | DMA1_FLAG_HT4 | DMA1_FLAG_TE4 | DMA1_FLAG_GL4);
    // Reload DMA
    USART1_TX_DMA_CHANNEL->MADDR = (uint32_t)USART_BackData;
    USART1_TX_DMA_CHANNEL->CNTR  = Add_p;
    // Open DMA
    USART_DMACmd(USART1, USART_DMAReq_Tx, ENABLE);
    DMA_Cmd(USART1_TX_DMA_CHANNEL, ENABLE);
}





void USART2_SendData(uint8_t* Dat, uint16_t Length)
{
    uint16_t i;

    for (i = 0 ; i < Length ; i++)
    {
        while (USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);
        USART_SendData(USART2, Dat[i]);
    }
    while (USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);
}





static inline uint8_t USART_ReadByte_ClrFlg(USART_TypeDef *USARTx)
{
    uint8_t Byte;

    (void)USARTx->STATR;
    Byte = (uint8_t)USART_ReceiveData(USARTx);

    return Byte;
}





void USART_SwitchTo(uint8_t PORT)
{
    USART_TypeDef *OpenUSART, *CloseUSART;
    
    switch (PORT)
    {
        case 1:
        {
            OpenUSART = USART1;
            CloseUSART = USART2;
        } break;
        case 2:
        {
            OpenUSART = USART2;
            CloseUSART = USART1;
        } break;
        default:    {return;}
    }
    
    USART_ITConfig(CloseUSART, USART_IT_RXNE, DISABLE);
    USART_Cmd(CloseUSART, DISABLE);

    USART_ReadByte_ClrFlg(CloseUSART);
    USART_ReadByte_ClrFlg(OpenUSART);

    USART_Cmd(OpenUSART, ENABLE);
    USART_ITConfig(OpenUSART, USART_IT_RXNE, ENABLE);
}






void USART_Setup(void)
{
    GPIO_InitTypeDef  GPIO_InitStructure = {0};
    USART_InitTypeDef USART_InitStructure = {0};
    NVIC_InitTypeDef NVIC_InitStructure = {0};
    DMA_InitTypeDef DMA_InitStructure = {0};

    // GPIO
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    // USART1
    GPIO_InitStructure.GPIO_Pin     = USART1_TXD_Pin;
    GPIO_InitStructure.GPIO_Speed   = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode    = GPIO_Mode_AF_PP;
    GPIO_Init(USART_Port, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin     = USART1_RXD_Pin;
    GPIO_InitStructure.GPIO_Mode    = GPIO_Mode_IPU;
    GPIO_Init(USART_Port, &GPIO_InitStructure);
    // USART2
    GPIO_InitStructure.GPIO_Pin     = USART2_TXD_Pin;
    GPIO_InitStructure.GPIO_Speed   = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode    = GPIO_Mode_AF_PP;
    GPIO_Init(USART_Port, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin     = USART2_RXD_Pin;
    GPIO_InitStructure.GPIO_Mode    = GPIO_Mode_IPU;
    GPIO_Init(USART_Port, &GPIO_InitStructure);

    // NVIC
    NVIC_InitStructure.NVIC_IRQChannel                      = USART1_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority    = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority           = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd                   = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
    NVIC_InitStructure.NVIC_IRQChannel                      = USART2_IRQn;
    NVIC_Init(&NVIC_InitStructure);

    // USART
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);
    USART_InitStructure.USART_BaudRate              = 115200;
    USART_InitStructure.USART_WordLength            = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits              = USART_StopBits_1;
    USART_InitStructure.USART_Parity                = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl   = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode                  = USART_Mode_Tx | USART_Mode_Rx;
    USART_Init(USART1, &USART_InitStructure);
    USART_InitStructure.USART_BaudRate              = 1000000;
    USART_Init(USART2, &USART_InitStructure);
    
    // DMA
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
    DMA_Cmd(USART1_TX_DMA_CHANNEL, DISABLE);
    DMA_ClearFlag(DMA1_FLAG_TC4 | DMA1_FLAG_HT4 | DMA1_FLAG_TE4 | DMA1_FLAG_GL4);
    // Data Add
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&USART1->DATAR;
    DMA_InitStructure.DMA_MemoryBaseAddr     = 0;
    // Direction
    DMA_InitStructure.DMA_DIR                = DMA_DIR_PeripheralDST;
    // Data Length
    DMA_InitStructure.DMA_BufferSize         = 0;
    // Add++ Set
    DMA_InitStructure.DMA_PeripheralInc      = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc          = DMA_MemoryInc_Enable;
    // Data Type
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    DMA_InitStructure.DMA_MemoryDataSize     = DMA_MemoryDataSize_Byte;
    // DMA Mode
    DMA_InitStructure.DMA_Mode               = DMA_Mode_Normal;
    DMA_InitStructure.DMA_Priority           = DMA_Priority_Medium;
    DMA_InitStructure.DMA_M2M                = DMA_M2M_Disable;
    DMA_Init(USART1_TX_DMA_CHANNEL, &DMA_InitStructure);

    USART_SwitchTo(1);
}





void USART1_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void USART1_IRQHandler(void)
{
    if(USART_GetITStatus(USART1, USART_IT_RXNE) == SET)
    {
        GlobalFlag |= GlobalFlag_USART_CmdIn;
        USART_BUFFER_TomeoutCNT = 0;

        if (USART_BUFFER_CNT == USART_BUFFER_HeadFlag_p)
        {
            USART_BUFFER[USART_BUFFER_CNT++] = USART_ReadByte_ClrFlg(USART1);
            if (USART_BUFFER[USART_BUFFER_HeadFlag_p] != 0xab)
            {
                GlobalFlag &= ~GlobalFlag_USART_CmdIn;
                USART_BUFFER_CNT = 0;
                return;
            }
        }
        else
        {
            USART_BUFFER[USART_BUFFER_CNT++] = USART_ReadByte_ClrFlg(USART1);
            if ((USART_BUFFER_CNT > 3) && (USART_BUFFER_CNT >= USART_BUFFER[USART_BUFFER_ByteLength_p]))
            {
                GlobalFlag &= ~GlobalFlag_USART_CmdIn;
                GlobalFlag |= GlobalFlag_USART_CmdFn;
                USART_BUFFER_CNT = 0;
                USART_ITConfig(USART1, USART_IT_RXNE, DISABLE);
            }
        }
    }
}


void USART2_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void USART2_IRQHandler(void)
{
    if(USART_GetITStatus(USART2, USART_IT_RXNE) == SET)
    {
        GlobalFlag |= GlobalFlag_USART_CmdIn;
        USART_BUFFER_TomeoutCNT = 0;

        if (USART_BUFFER_CNT == USART_BUFFER_HeadFlag_p)
        {
            USART_BUFFER[USART_BUFFER_CNT++] = USART_ReadByte_ClrFlg(USART2);
            if (USART_BUFFER[USART_BUFFER_HeadFlag_p] != 0xab)
            {
                GlobalFlag &= ~GlobalFlag_USART_CmdIn;
                USART_BUFFER_CNT = 0;
                return;
            }
        }
        else
        {
            USART_BUFFER[USART_BUFFER_CNT++] = USART_ReadByte_ClrFlg(USART2);
            if ((USART_BUFFER_CNT > 3) && (USART_BUFFER_CNT >= USART_BUFFER[USART_BUFFER_ByteLength_p]))
            {
                GlobalFlag &= ~GlobalFlag_USART_CmdIn;
                GlobalFlag |= GlobalFlag_USART_CmdFn;
                USART_BUFFER_CNT = 0;
                USART_ITConfig(USART2, USART_IT_RXNE, DISABLE);
            }
        }
    }
}