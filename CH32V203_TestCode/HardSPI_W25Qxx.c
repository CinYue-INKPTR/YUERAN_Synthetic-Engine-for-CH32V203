#include "ch32v20x.h"
#include "HardSPI.h"
#include "HardSPI_W25Qxx.h"

#define W25Qxx_CS_RCC_APB2Periph	RCC_APB2Periph_GPIOC
// CS
#define W25Qxx_CS_Port    			GPIOC		// OUT
#define W25Qxx_CS_Pin     			GPIO_Pin_1	// OUT


// Flag
#define W25Qxx_Flag_Busy            0x01

// Cmd
#define W25Qxx_SRegCmd_Read         0x05

#define W25Qxx_Cmd_WriteEnable      0x06
#define W25Qxx_Cmd_WriteDisable     0x04

#define W25Qxx_Cmd_ReadData         0x03

#define W25Qxx_Cmd_PageProgram      0x02

#define W25Qxx_Cmd_SectorErase      0x20
#define W25Qxx_Cmd_BlockErase       0xd8
#define W25Qxx_Cmd_ChipErase        0xc7

#define W25Qxx_Cmd_PowerDown        0xb9
#define W25Qxx_Cmd_ReleasePowerDown 0xab

#define W25Qxx_Cmd_DeviceID	    	0x9f

#define W25Qxx_StorageINFO_Offset	0x14

void W25Qxx_Init(void)
{
    RCC_APB2PeriphClockCmd(W25Qxx_CS_RCC_APB2Periph, ENABLE);
	GPIO_InitTypeDef GPIO_InitStructure = {0};
    GPIO_InitStructure.GPIO_Pin     = W25Qxx_CS_Pin;
    GPIO_InitStructure.GPIO_Mode    = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed   = HardSPI_PinSpeed;
    GPIO_Init(W25Qxx_CS_Port, &GPIO_InitStructure);
    GPIO_SetBits(W25Qxx_CS_Port, W25Qxx_CS_Pin);
}



static inline void W25Qxx_CS(FlagStatus Dat)
{
    GPIO_WriteBit(W25Qxx_CS_Port, W25Qxx_CS_Pin, Dat);
}

uint8_t W25Qxx_ReadBusy(void)
{
    uint8_t i;

    W25Qxx_CS(RESET);
    W25Qxx_SwapBytes(W25Qxx_SRegCmd_Read);
	i = (W25Qxx_SwapBytes(0xff) & W25Qxx_Flag_Busy)? 1 : 0;
    W25Qxx_CS(SET);

    return i;
}

uint16_t W25Qxx_GetSectorQuantity(void)
{
	uint8_t INFO;
	
	W25Qxx_CS(RESET);
	W25Qxx_SwapBytes(W25Qxx_Cmd_DeviceID);
	if(W25Qxx_SwapBytes(0xff) == 0xef)
	{
		W25Qxx_SwapBytes(0xff);
		INFO = W25Qxx_SwapBytes(0xff) - W25Qxx_StorageINFO_Offset;
		INFO = 0x100 << INFO;
	}
	else
	{
		INFO = 0;
	}
	W25Qxx_CS(SET);

	return INFO;
}

void W25Qxx_ReadData(uint32_t ByteAdd, uint8_t* Dat, uint32_t Size)
{
	uint32_t i;
	
	W25Qxx_CS(RESET);
	W25Qxx_SwapBytes(W25Qxx_Cmd_ReadData);
	W25Qxx_SwapBytes((uint8_t)(ByteAdd >> 16));
	W25Qxx_SwapBytes((uint8_t)(ByteAdd >> 8));
	W25Qxx_SwapBytes((uint8_t)ByteAdd);
	for(i = 0 ; i < Size ; i++)
	{
		*Dat++ = W25Qxx_SwapBytes(0xff);
	}
	W25Qxx_CS(SET);
}

void W25Qxx_WriteEnable(uint8_t En)
{
	W25Qxx_CS(0);
	W25Qxx_SwapBytes((En)? W25Qxx_Cmd_WriteEnable : W25Qxx_Cmd_WriteDisable);
	W25Qxx_CS(1);
}

void W25Qxx_PageProgram(uint16_t PageAdd, uint8_t* Dat, uint16_t Size)
{
	uint16_t i;

	W25Qxx_WriteEnable(1);
	if(Size > 256)	{Size = 256;}
	W25Qxx_CS(RESET);
	W25Qxx_SwapBytes(W25Qxx_Cmd_PageProgram);
	W25Qxx_SwapBytes((uint8_t)(PageAdd >> 8));
	W25Qxx_SwapBytes((uint8_t)PageAdd);
	W25Qxx_SwapBytes(0x00);
	for(i = 0 ; i < Size ; i++)
	{
		W25Qxx_SwapBytes(*Dat++);
	}
	W25Qxx_CS(SET);
}

void W25Qxx_Erase_Chip(void)
{
	W25Qxx_WriteEnable(1);
	
	W25Qxx_CS(RESET);
	W25Qxx_SwapBytes(W25Qxx_Cmd_ChipErase);
	W25Qxx_CS(SET);
}

void W25Qxx_Erase_Block(uint8_t BlockAdd)
{
	W25Qxx_WriteEnable(1);
	
	W25Qxx_CS(RESET);
	W25Qxx_SwapBytes(W25Qxx_Cmd_BlockErase);
	W25Qxx_SwapBytes(BlockAdd);
	W25Qxx_SwapBytes(0x00);
	W25Qxx_SwapBytes(0x00);
	W25Qxx_CS(SET);
}

void W25Qxx_Erase_Sector(uint16_t SectorAdd)
{
	W25Qxx_WriteEnable(1);
	
	W25Qxx_CS(RESET);
	W25Qxx_SwapBytes(W25Qxx_Cmd_SectorErase);
	W25Qxx_SwapBytes((uint8_t)(SectorAdd >> 4));
	W25Qxx_SwapBytes((uint8_t)(SectorAdd << 4));
	W25Qxx_SwapBytes(0x00);
	W25Qxx_CS(SET);
}

void W25Qxx_PowerDown(uint8_t En)
{
	W25Qxx_CS(RESET);
	W25Qxx_SwapBytes((En)? W25Qxx_Cmd_PowerDown : W25Qxx_Cmd_ReleasePowerDown);
	W25Qxx_CS(SET);
}
