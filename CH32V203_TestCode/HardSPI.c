#include "ch32v20x.h"
#include "HardSPI.h"

void HardSPI_Init(void)
{
    RCC_APB2PeriphClockCmd(HardSPI_SCK_RCC_APB2Periph | HardSPI_MOSI_RCC_APB2Periph | HardSPI_MISO_RCC_APB2Periph, ENABLE);
	GPIO_InitTypeDef GPIO_InitStructure = {0};
	// SCK
    GPIO_InitStructure.GPIO_Pin     = HardSPI_SCK_Pin;
    GPIO_InitStructure.GPIO_Mode    = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed   = GPIO_Speed_50MHz;
    GPIO_Init(HardSPI_SCK_Port, &GPIO_InitStructure);
	// MISO
    GPIO_InitStructure.GPIO_Pin     = HardSPI_MISO_Pin;
    GPIO_InitStructure.GPIO_Mode    = GPIO_Mode_IN_FLOATING;
    GPIO_Init(HardSPI_MISO_Port, &GPIO_InitStructure);
	// MOSI
    GPIO_InitStructure.GPIO_Pin     = HardSPI_MOSI_Pin;
    GPIO_InitStructure.GPIO_Mode    = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed   = GPIO_Speed_50MHz;
    GPIO_Init(HardSPI_MOSI_Port, &GPIO_InitStructure);

    RCC_APB2PeriphClockCmd(HardSPI_RCC_APB2Periph, ENABLE);
    SPI_InitTypeDef  SPI_InitStructure = {0};
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
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_4;
    SPI_InitStructure.SPI_FirstBit          = SPI_FirstBit_MSB;
    SPI_InitStructure.SPI_CRCPolynomial     = 7;
    SPI_Init(HardSPI_Port, &SPI_InitStructure);
    SPI_Cmd(HardSPI_Port, ENABLE);
}

uint8_t HardSPI_SwapBytes(uint8_t Tx_Byte)
{
    while(SPI_I2S_GetFlagStatus(HardSPI_Port, SPI_I2S_FLAG_TXE) == RESET);
    SPI_I2S_SendData(HardSPI_Port, Tx_Byte);
    while(SPI_I2S_GetFlagStatus(HardSPI_Port, SPI_I2S_FLAG_RXNE) == RESET);
    return SPI_I2S_ReceiveData(HardSPI_Port);
}
