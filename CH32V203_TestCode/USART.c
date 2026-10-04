#include "ch32v20x.h"
#include "USART.h"
#include "YUERAN_Define.h"

void USART1_ON(void)
{
    DMA1_Channel5->MADDR = (uint32_t)RenderCmd;
    DMA_SetCurrDataCounter(DMA1_Channel5, RenderCmd_Size);
    DMA1_Channel4->MADDR = (uint32_t)BackData;
    DMA_SetCurrDataCounter(DMA1_Channel4, BackData_Size);

    DMA_ClearFlag(DMA1_FLAG_TC4 | DMA1_FLAG_TC5 | // Transfer Complete Flag
                  DMA1_FLAG_HT4 | DMA1_FLAG_HT5 | // Half Transfer Flag
                  DMA1_FLAG_TE4 | DMA1_FLAG_TE5 | // Transfer Error Flag
                  DMA1_FLAG_GL4 | DMA1_FLAG_GL5); // Global Flag

    DMA_Cmd(DMA1_Channel5, ENABLE); // RXD enable first
    DMA_Cmd(DMA1_Channel4, ENABLE); // TXD

    USART_Cmd(USART1, ENABLE);

    USART_DMACmd(USART1, USART_DMAReq_Rx, ENABLE);
    USART_DMACmd(USART1, USART_DMAReq_Tx, ENABLE);

}

void USART1_OFF(void)
{
    USART_DMACmd(USART1, USART_DMAReq_Tx | USART_DMAReq_Rx, DISABLE);

    DMA_Cmd(DMA1_Channel4, DISABLE); // TXD
    DMA_Cmd(DMA1_Channel5, DISABLE); // RXD

    DMA_ClearFlag(DMA1_FLAG_TC4 | DMA1_FLAG_TC5 | // Transfer Complete Flag
                  DMA1_FLAG_HT4 | DMA1_FLAG_HT5 | // Half Transfer Flag
                  DMA1_FLAG_TE4 | DMA1_FLAG_TE5 | // Transfer Error Flag
                  DMA1_FLAG_GL4 | DMA1_FLAG_GL5); // Global Flag

    USART_Cmd(USART1, DISABLE);
}

void USART2_ON(void)
{
    GPIO_InitTypeDef  GPIO_InitStructure = {0};

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    USART_Cmd(USART2, ENABLE);
    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);
    USART_ITConfig(USART2, USART_IT_IDLE, ENABLE);
}

void USART2_OFF(void)
{
    GPIO_InitTypeDef  GPIO_InitStructure = {0};

    USART_ITConfig(USART2, USART_IT_RXNE, DISABLE);
    USART_ITConfig(USART2, USART_IT_IDLE, DISABLE);
    USART_Cmd(USART2, DISABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    GPIO_SetBits(GPIOA, GPIO_Pin_2);
}



void USART_Setup(void)
{
    GPIO_InitTypeDef  GPIO_InitStructure = {0};
    USART_InitTypeDef USART_InitStructure = {0};
    DMA_InitTypeDef DMA_InitStructure = {0};
    NVIC_InitTypeDef NVIC_InitStructure = {0};

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);  // USART1
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);  // USART2
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);   // GPIO
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);      // DMA1



    // =======================================================================
    // ***USART 1 pins
    // TXD - A9
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    // RXD - A10
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // ***USART 2 pins
    // TXD - A2
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    // RXD - A3
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // ***USART settings
    USART_InitStructure.USART_BaudRate = 115200;                                    // BaudRate
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;                     //
    USART_InitStructure.USART_StopBits = USART_StopBits_1;                          //
    USART_InitStructure.USART_Parity = USART_Parity_No;                             //
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None; //
    USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;                 //
    USART_Init(USART1, &USART_InitStructure);
    USART_Init(USART2, &USART_InitStructure);



    // =======================================================================
    // ***USART 1 RXD - Channel5
    DMA_DeInit(DMA1_Channel5);
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)(&USART1->DATAR);  //
    DMA_InitStructure.DMA_MemoryBaseAddr     = (uint32_t)RenderCmd;         // Data array
    DMA_InitStructure.DMA_DIR                = DMA_DIR_PeripheralSRC;       //
    DMA_InitStructure.DMA_BufferSize         = RenderCmd_Size;              //
    DMA_InitStructure.DMA_PeripheralInc      = DMA_PeripheralInc_Disable;   //
    DMA_InitStructure.DMA_MemoryInc          = DMA_MemoryInc_Enable;        //
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte; //
    DMA_InitStructure.DMA_MemoryDataSize     = DMA_MemoryDataSize_Byte;     //
    DMA_InitStructure.DMA_Mode               = DMA_Mode_Normal;           //
    DMA_InitStructure.DMA_Priority           = DMA_Priority_VeryHigh;       // Priority
    DMA_InitStructure.DMA_M2M                = DMA_M2M_Disable;             //
    DMA_Init(DMA1_Channel5, &DMA_InitStructure);
    // ***USART 1 TXD - Channel4
    DMA_DeInit(DMA1_Channel4);
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)(&USART1->DATAR);  //
    DMA_InitStructure.DMA_MemoryBaseAddr     = (uint32_t)BackData;          // Data array
    DMA_InitStructure.DMA_DIR                = DMA_DIR_PeripheralDST;       //
    DMA_InitStructure.DMA_BufferSize         = BackData_Size;               // Data operation size
    DMA_InitStructure.DMA_Mode               = DMA_Mode_Normal;             // Mode
    DMA_InitStructure.DMA_Priority           = DMA_Priority_High;           // Priority
    DMA_Init(DMA1_Channel4, &DMA_InitStructure);



    // =======================================================================
    // USART 2 - interrupt
    NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);



    USART2_OFF();
    USART1_ON();
}





void USART2_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void USART2_IRQHandler(void)
{
    uint8_t Rx_Byte;
    if(USART_GetITStatus(USART2, USART_IT_RXNE) == SET)
    {
        Rx_Byte = (uint8_t)USART_ReceiveData(USART2);
        if(USART2_RxLen < USART2_BUFFER_Size)
        {
            USART2_BUFFER[USART2_RxLen++] = Rx_Byte;
        }
    }
    if(USART_GetITStatus(USART2, USART_IT_IDLE) == SET)
    {
        USART2_RxFlag = 1;
    }
}
