#ifndef HardSPI_W25Qxx_H
#define HardSPI_W25Qxx_H

#define W25Qxx_CS_RCC_APB2Periph	RCC_APB2Periph_GPIOC
// CS
#define W25Qxx_CS_Port    			GPIOA
#define W25Qxx_CS_Pin     			GPIO_Pin_0

void W25Qxx_Init(void);
uint8_t W25Qxx_ReadBusy(void);
uint16_t W25Qxx_GetSectorQuantity(void);
void W25Qxx_WriteEnable(uint8_t En);
void W25Qxx_PageProgram(uint16_t PageAdd, uint8_t* Dat, uint16_t Size);
void W25Qxx_Erase_Chip(void);
void W25Qxx_Erase_Block(uint8_t BlockAdd);
void W25Qxx_Erase_Sector(uint16_t SectorAdd);
void W25Qxx_PowerDown(uint8_t En);

#endif
