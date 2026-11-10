#include "YUERAN_Define.h"

#include "ch32v20x.h"

Element E_Layer[E_Layer_Size] = {0};
uint8_t FRAME[FRAME_PageSize][FRAME_ListSize] = {0};
uint8_t E[E_Size] = {0};
uint16_t RenderingTime = 0;

uint8_t FlashIndex[FlashIndex_Quantity * FlashIndex_Size] = {0};
uint8_t FlashIndex_CNT = 0;

uint8_t TEST_VALUE[16];

volatile uint8_t USART_BUFFER[USART_BUFFER_Size] = {0};
volatile uint8_t USART_BUFFER_CNT = 0;
volatile uint8_t USART_BUFFER_TomeoutCNT = 0;
uint8_t USART_BackData[USART_BackData_Size] = {0};
uint8_t BackData_Head = 0xff;

volatile uint8_t GlobalFlag = GlobalFlag_FrameClr | GlobalFlag_WaitRendering;

void NVIC_Interrupt_GroupInit(void)
{
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);
}
