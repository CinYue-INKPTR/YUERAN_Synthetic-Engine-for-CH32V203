#include "HardSPI.h"

#include "ch32v20x.h"
#include "YUERAN_Define.h"

#define HardSPI_SPI_Periph		    RCC_APB2Periph_SPI1
#define HardSPI_Port				SPI1

#define HardSPI_GPIO_Periph         RCC_APB2Periph_GPIOA
#define HardSPI_SCK_Port         	GPIOA
#define HardSPI_SCK_Pin          	GPIO_Pin_5
#define HardSPI_MOSI_Port        	GPIOA
#define HardSPI_MOSI_Pin         	GPIO_Pin_7
#define HardSPI_MISO_Port        	GPIOA
#define HardSPI_MISO_Pin         	GPIO_Pin_6
#define HardSPI_PinSpeed			GPIO_Speed_50MHz



void HardSPI_SpeedChoose(HardSPI_Speed Speed)
{
    SPI_InitTypeDef  SPI_InitStructure = {0};

    while (SPI_I2S_GetFlagStatus(HardSPI_Port, SPI_I2S_FLAG_BSY) == SET);
    SPI_Cmd(HardSPI_Port, DISABLE);

    SPI_InitStructure.SPI_CPOL              = SPI_CPOL_Low;
    SPI_InitStructure.SPI_CPHA              = SPI_CPHA_1Edge;
    SPI_InitStructure.SPI_Direction         = SPI_Direction_2Lines_FullDuplex;
    SPI_InitStructure.SPI_Mode              = SPI_Mode_Master;
    SPI_InitStructure.SPI_DataSize          = SPI_DataSize_8b;
    SPI_InitStructure.SPI_NSS               = SPI_NSS_Soft;
    SPI_InitStructure.SPI_FirstBit          = SPI_FirstBit_MSB;
    SPI_InitStructure.SPI_CRCPolynomial     = 7;
    switch (Speed)
    {
        case HardSPI_Speed_Flash:   {SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_4;} break;
        case HardSPI_Speed_OLED:    {SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_16;} break;
    }
    SPI_Init(HardSPI_Port, &SPI_InitStructure);

    SPI_Cmd(HardSPI_Port, ENABLE);
}






void HardSPI_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    SPI_InitTypeDef  SPI_InitStructure = {0};

    RCC_APB2PeriphClockCmd(HardSPI_GPIO_Periph, ENABLE);
    GPIO_InitStructure.GPIO_Pin     = HardSPI_SCK_Pin;
    GPIO_InitStructure.GPIO_Mode    = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed   = GPIO_Speed_50MHz;
    GPIO_Init(HardSPI_SCK_Port, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin     = HardSPI_MOSI_Pin;
    GPIO_Init(HardSPI_MOSI_Port, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin     = HardSPI_MISO_Pin;
    GPIO_InitStructure.GPIO_Mode    = GPIO_Mode_IPU;
    GPIO_Init(HardSPI_MISO_Port, &GPIO_InitStructure);

    RCC_APB2PeriphClockCmd(HardSPI_SPI_Periph, ENABLE);
    // SPI mode	|	SPI_CPOL		|	SPI_CPHA
	// -----------------------------------------------
	// 0		|	SPI_CPOL_Low	|	SPI_CPHA_1Edge
	// 1		|	SPI_CPOL_Low	|	SPI_CPHA_2Edge
	// 2		|	SPI_CPOL_High	|	SPI_CPHA_1Edge
	// 3		|	SPI_CPOL_High	|	SPI_CPHA_2Edge
    SPI_InitStructure.SPI_CPOL              = SPI_CPOL_Low;
    SPI_InitStructure.SPI_CPHA              = SPI_CPHA_1Edge;
    SPI_InitStructure.SPI_Direction         = SPI_Direction_2Lines_FullDuplex;
    SPI_InitStructure.SPI_Mode              = SPI_Mode_Master;
    SPI_InitStructure.SPI_DataSize          = SPI_DataSize_8b;
	SPI_InitStructure.SPI_NSS               = SPI_NSS_Soft;
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_16;
    SPI_InitStructure.SPI_FirstBit          = SPI_FirstBit_MSB;
    SPI_InitStructure.SPI_CRCPolynomial     = 7;
    SPI_Init(HardSPI_Port, &SPI_InitStructure);
    SPI_Cmd(HardSPI_Port, ENABLE);
}






uint8_t HardSPI_SwapBytes(uint8_t Tx_Byte)
{
    while (SPI_I2S_GetFlagStatus(HardSPI_Port, SPI_I2S_FLAG_TXE) == RESET);
    SPI_I2S_SendData(HardSPI_Port, Tx_Byte);
    while (SPI_I2S_GetFlagStatus(HardSPI_Port, SPI_I2S_FLAG_RXNE) == RESET);
    return SPI_I2S_ReceiveData(HardSPI_Port);
}