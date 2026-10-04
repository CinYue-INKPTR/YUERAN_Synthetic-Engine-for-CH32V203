#include "ch32v20x.h"
#include "CH32V203G6U6_ButtonLight.h"

#define ButtonLight_RCC_APB2Periph  RCC_APB2Periph_GPIOD

#define Button_Port                 GPIOD
#define Button_Pin                  GPIO_Pin_1

#define Light_Port                  GPIOD
#define Light_Pin                   GPIO_Pin_0

void ButtonLight_Init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
    RCC_HSEConfig(RCC_HSE_OFF);
    GPIO_PinRemapConfig(GPIO_Remap_PD01, ENABLE);

    Delay_Init();

    RCC_APB2PeriphClockCmd(ButtonLight_RCC_APB2Periph, ENABLE);
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.GPIO_Pin        = Button_Pin;
    GPIO_InitStruct.GPIO_Speed      = GPIO_Speed_2MHz;
    GPIO_InitStruct.GPIO_Mode       = GPIO_Mode_IPU;
    GPIO_Init(Button_Port, &GPIO_InitStruct);

    GPIO_InitStruct.GPIO_Pin        = Light_Pin;
    GPIO_InitStruct.GPIO_Speed      = GPIO_Speed_2MHz;
    GPIO_InitStruct.GPIO_Mode       = GPIO_Mode_Out_PP;
    GPIO_Init(Light_Port, &GPIO_InitStruct);
    GPIO_WriteBit(Light_Port, Light_Pin, Bit_RESET);
}

uint8_t ReadButton(void)
{
    if(GPIO_ReadInputDataBit(Button_Port, Button_Pin) == Bit_RESET) {return 1;}
    else                                                            {return 0;}
}

uint8_t ButtonLogic(void)
{
    uint8_t i = 0;

    if(ReadButton())
    {
        Delay_Ms(10);
        if(ReadButton())    {i = 1;}
        else                {i = 0;}
        while(GPIO_ReadInputDataBit(Button_Port, Button_Pin) == Bit_RESET);
    }

    return i;
}

void EditLight(uint8_t Dat)
{
    if(Dat) {GPIO_WriteBit(Light_Port, Light_Pin, Bit_SET);}
    else    {GPIO_WriteBit(Light_Port, Light_Pin, Bit_RESET);}
}
