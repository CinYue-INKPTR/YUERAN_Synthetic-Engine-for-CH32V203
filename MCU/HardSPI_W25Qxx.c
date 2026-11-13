// v1.0.0.261113
#include "HardSPI_W25Qxx.h"

#include "ch32v20x.h"
#include "HardSPI.h"
#include "YUERAN_Define.h"

// CS pin
#define W25Qxx_CS_Periph    		RCC_APB2Periph_GPIOB
#define W25Qxx_CS_Port              GPIOB
#define W25Qxx_CS_Pin               GPIO_Pin_0
#define W25Qxx_PinSpeed				GPIO_Speed_50MHz

// Cmd
#define W25Qxx_SRegCmd_Read         0x05
#define W25Qxx_Cmd_WriteEnable      0x06
#define W25Qxx_Cmd_WriteDisable     0x04

#define W25Qxx_Cmd_PageProgram      0x02

#define W25Qxx_Cmd_SectorErase      0x20
#define W25Qxx_Cmd_BlockErase       0xd8
#define W25Qxx_Cmd_ChipErase        0xc7

#define W25Qxx_Cmd_PowerDown        0xb9
#define W25Qxx_Cmd_ReleasePowerDown 0xab

#define W25Qxx_Cmd_DeviceID	    	0x9f

#define Addr_1(Dat)					((uint8_t)((Dat) >> 16))
#define Addr_2(Dat)					((uint8_t)((Dat) >> 8))
#define Addr_3(Dat)					((uint8_t)(Dat))





void W25Qxx_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure = {0};
	
    RCC_APB2PeriphClockCmd(W25Qxx_CS_Periph, ENABLE);
    GPIO_InitStructure.GPIO_Pin     = W25Qxx_CS_Pin;
    GPIO_InitStructure.GPIO_Mode    = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed   = W25Qxx_PinSpeed;
    GPIO_Init(W25Qxx_CS_Port, &GPIO_InitStructure);
	
	GPIO_SetBits(W25Qxx_CS_Port, W25Qxx_CS_Pin);
}
void W25Qxx_Start(void)	{GPIO_ResetBits(W25Qxx_CS_Port, W25Qxx_CS_Pin);}
void W25Qxx_Stop(void)	{GPIO_SetBits(W25Qxx_CS_Port, W25Qxx_CS_Pin);}






uint8_t W25Qxx_ReadBusy(void)
{
    uint8_t Busy;

    W25Qxx_Start();
    HardSPI_SwapBytes(W25Qxx_SRegCmd_Read);
	Busy = HardSPI_SwapBytes(0xff);
    W25Qxx_Stop();

    return Busy;
}





uint16_t W25Qxx_GetSectorQuantity(void)
{
	uint16_t INFO = 0;
	
	W25Qxx_Start();
	HardSPI_SwapBytes(W25Qxx_Cmd_DeviceID);
	if (HardSPI_SwapBytes(0xff) == 0xef)
	{
	    HardSPI_SwapBytes(0xff);
		INFO = HardSPI_SwapBytes(0xff);
		INFO = 1 << (INFO - 12);
	}
	else
	{
		INFO = 0;
	}
	W25Qxx_Stop();

	return INFO;
}






void W25Qxx_ReadData(uint32_t Addr, uint8_t* Dat, uint32_t Length)
{
	uint32_t i;
	
	W25Qxx_Start();
	HardSPI_SwapBytes(W25Qxx_Cmd_ReadData);
	HardSPI_SwapBytes(Addr_1(Addr));
	HardSPI_SwapBytes(Addr_2(Addr));
	HardSPI_SwapBytes(Addr_3(Addr));
	for(i = 0 ; i < Length ; i++)
	{
		*Dat++ = HardSPI_SwapBytes(0xff);
	}
	W25Qxx_Stop();
}






void W25Qxx_WriteEnable(FunctionalState NewState)
{
	W25Qxx_Start();
	if(NewState == ENABLE)	{HardSPI_SwapBytes(W25Qxx_Cmd_WriteEnable);}
	else					{HardSPI_SwapBytes(W25Qxx_Cmd_WriteDisable);}
	W25Qxx_Stop();
}





void W25Qxx_Program(uint32_t Addr, uint8_t* Dat, uint16_t Length)
{
	uint16_t i, PageLength;

	if(Length > 256)	{Length = 256;}
	
	while(Length)
	{
		// Page program can't cross the 256-Bytes page
		PageLength = 256 - (uint16_t)(Addr & 0xff);
		if(PageLength > Length)	{PageLength = Length;}

		W25Qxx_WriteEnable(ENABLE);
		W25Qxx_Start();
		HardSPI_SwapBytes(W25Qxx_Cmd_PageProgram);
		HardSPI_SwapBytes(Addr_1(Addr));
		HardSPI_SwapBytes(Addr_2(Addr));
		HardSPI_SwapBytes(Addr_3(Addr));
		for(i = 0 ; i < PageLength ; i++)
		{
		    HardSPI_SwapBytes(*Dat++);
		}
		W25Qxx_Stop();

		while (W25Qxx_ReadBusy() & W25Qxx_Flag_Busy)	{Delay_Ms(1);}

		Addr += PageLength;
		Length -= PageLength;
	}
}






void W25Qxx_Erase_Chip(void)
{
	W25Qxx_Start();
	HardSPI_SwapBytes(W25Qxx_Cmd_ChipErase);
	W25Qxx_Stop();
}






void W25Qxx_Erase_Block(uint32_t Addr)
{
	Addr = Addr << 12;
	W25Qxx_Start();
	HardSPI_SwapBytes(W25Qxx_Cmd_BlockErase);
	HardSPI_SwapBytes(Addr_1(Addr));
	HardSPI_SwapBytes(Addr_2(Addr));
	HardSPI_SwapBytes(Addr_3(Addr));
	W25Qxx_Stop();
}






void W25Qxx_Erase_Sector(uint32_t Addr)
{
	Addr = Addr << 12;
	W25Qxx_Start();
	HardSPI_SwapBytes(W25Qxx_Cmd_SectorErase);
	HardSPI_SwapBytes(Addr_1(Addr));
	HardSPI_SwapBytes(Addr_2(Addr));
	HardSPI_SwapBytes(Addr_3(Addr));
	W25Qxx_Stop();
}





static inline uint8_t W25Qxx_ReadByte(uint32_t Addr)
{
	uint8_t Byte;
	
	W25Qxx_Start();
	HardSPI_SwapBytes(W25Qxx_Cmd_ReadData);
	HardSPI_SwapBytes(Addr_1(Addr));
	HardSPI_SwapBytes(Addr_2(Addr));
	HardSPI_SwapBytes(Addr_3(Addr));
	Byte = HardSPI_SwapBytes(0xff);
	W25Qxx_Stop();

	return Byte;
}

void W25Qxx_GetIndex(void)
{
	uint32_t Add_p = 0, Add_tmp = 0xffffffff;
	uint16_t i;
	uint8_t tmp;

	FlashIndex_CNT = 0;
	
	// v3
	// First, determine the position of the valid header.
	for (i = 0 ; i < (2*W25Qxx_SectorSize/FlashIndex_Size) ; i++)
	{
	    // Add_tmp records the position of the table header
	    tmp = W25Qxx_ReadByte(Add_p);
	    if ((tmp == 0x00) && ((Add_tmp == 0xffffffff) || (Add_p > Add_tmp)))  {Add_tmp = Add_p;}
	    // Reads data from the empty data area for quick jumps, enhancing the speed of table lookup
	    if ((tmp == 0xff) && (Add_p < W25Qxx_SectorSize))    {Add_p = W25Qxx_SectorSize;}
	    if ((tmp == 0xff) && (Add_p >= W25Qxx_SectorSize))   {break;}
	    
		Add_p += FlashIndex_Size;
	}

	if (Add_tmp == 0xffffffff)	{return;}

	// Calibrate the pointer to point to the first file index.
	Add_tmp += FlashIndex_Size;
	// When the calibration pointer goes beyond the bottom, it will loop back.
	if (Add_tmp >= (2*W25Qxx_SectorSize))    {Add_tmp -= 2*W25Qxx_SectorSize;}

	// First, read the first byte of the index to determine if the condition is met to prevent incorrect counting.
	tmp = W25Qxx_ReadByte(Add_tmp);
	// Once the conditions are met, start counting while loading the index table.
	for (i = 0 ; tmp != 0xff ; i++)
	{
	    // When the current address stores a valid index, first count the number, then input it.
	    // If the count indicates that the index table is too long, then discard the loading.
	    FlashIndex_CNT++;
	    if (FlashIndex_CNT > FlashIndex_Quantity)
	    {
	        FlashIndex_CNT = 0;
	        break;
	    }
	    W25Qxx_ReadData(Add_tmp, &FlashIndex[i*FlashIndex_Size], FlashIndex_Size);

	    // Move to the next index and read the first byte.
	    // Let the loop check if the first byte is the index, and then proceed to the next round of loading.
	    Add_tmp += FlashIndex_Size;
	    if (Add_tmp >= (2*W25Qxx_SectorSize))    {Add_tmp -= 2*W25Qxx_SectorSize;}
	    tmp = W25Qxx_ReadByte(Add_tmp);
	}
}

// v2
//	while(1)
//	{
//	    i++;
//	    tmp = W25Qxx_ReadByte(Add_p);
//
//	    // 0xff
//	    if(tmp == 0xff)
//	    {
//	        if(Add_p)   {break;}
//	        else        {Add_p = W25Qxx_SectorSize;}
//	    }
//	    else    {Add_p += FlashIndex_Size;}
//
//	    // 0x00
//	    if(tmp == 0x00) {FlashIndex_CNT = 0;}
//	    else            {FlashIndex_CNT++;}
//
//	    if(i > (2*W25Qxx_SectorSize/FlashIndex_Size))                {break;}
//	    if((FlashIndex_CNT == FlashIndex_Quantity) && (tmp == 0xff))  {break;}
//	}







// v1
//	for(i = 0 ; (i < (2*W25Qxx_SectorSize/FlashIndex_Size + FlashIndex_Quantity)) && (FlashIndex_CNT < FlashIndex_Quantity) ; i++)
//	{
//		tmp = W25Qxx_ReadByte(Add_p);
//
//		// Find the enabled index first
//		if(tmp == 0xff)
//		{
//		    if(Add_p)   {CntFlag = 0x01;}
//		    else        {Add_p = W25Qxx_SectorSize - FlashIndex_Size;}
//		}
//
//		// If finded the enabled index, count back
//		if(CntFlag)
//		{
//		    if(Add_p)   {Add_p -= FlashIndex_Size;}
//		    else        {Add_p = 2*W25Qxx_SectorSize - FlashIndex_Size;}
//		    FlashIndex_CNT++;
//		}
//		else            {Add_p += FlashIndex_Size;}
//
//		// If finded the head of enabled index, adjust Add_p and FlashIndex_CNT, break
//		if((tmp == 0x00) && CntFlag)
//		{
//		    Add_p += 2*FlashIndex_Size;
//		    if(Add_p >= (2*W25Qxx_SectorSize))  {Add_p -= 2*W25Qxx_SectorSize;}
//		    CntFlag = 0xff;
//		    if(FlashIndex_CNT > 2)   {FlashIndex_CNT -= 2;}
//		    else                    {FlashIndex_CNT = 0;}
//		    break;
//		}
//	}
//
//	if(CntFlag == 0x01) {FlashIndex_CNT = 0;}
//
//
//	for(i = 0 ; i < FlashIndex_CNT ; i++)
//	{
//	    if(Add == (2*W25Qxx_SectorSize - FlashIndex_Size))   {Add_p = 0;}
//	    else                                                {Add_p += FlashIndex_Size;}
//	    W25Qxx_ReadData(Add_p, &FlashIndex[i*FlashIndex_Size], FlashIndex_Size);
//	}