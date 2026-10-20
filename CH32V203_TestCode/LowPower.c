#include "LowPower.h"

#include "ch32v20x.h"
#include "HardSPI_W25Qxx.h"
#include "OLEDspi.h"

void LowPower_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD; // œ¬¿≠ ‰»Î
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    PWR_WakeUpPinCmd(ENABLE);
}

void LowPower_Enable(void)
{
    OLED_Set(OLED_SetMode_OtherOptions, OLED_SetMode_Show_DISABLE);
    W25Qxx_PowerDown(ENABLE);
    PWR_EnterSTANDBYMode();
}

void LowPower_WKUP(void)
{
    PWR_ClearFlag(PWR_FLAG_SB);
    W25Qxx_PowerDown(DISABLE);
    OLED_Set(OLED_SetMode_OtherOptions, OLED_SetMode_Show_ENABLE);
    Delay_Ms(1);
}