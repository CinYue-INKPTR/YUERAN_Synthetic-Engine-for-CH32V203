// Checked - 261017.1220

#ifndef HardSPI_W25Qxx_H
#define HardSPI_W25Qxx_H

#define W25Qxx_Flag_Busy            0x01
#define W25Qxx_Cmd_ReadData         0x03
#define W25Qxx_SectorSize           4096
#define W25Qxx_Block_SectorNum      16

#include "ch32v20x.h"

void W25Qxx_Init(void);
void W25Qxx_Start(void);
void W25Qxx_Stop(void);

uint8_t W25Qxx_ReadBusy(void);
uint16_t W25Qxx_GetSectorQuantity(void);

void W25Qxx_ReadData(uint32_t Addr, uint8_t* Dat, uint32_t Length);

void W25Qxx_WriteEnable(FunctionalState NewState);
void W25Qxx_Program(uint32_t Addr, uint8_t* Dat, uint16_t Length);
void W25Qxx_Erase_Chip(void);
void W25Qxx_Erase_Block(uint32_t Addr);
void W25Qxx_Erase_Sector(uint32_t Addr);

//  static inline uint8_t W25Qxx_ReadByte(uint32_t Addr)
void W25Qxx_GetIndex(void);


#endif
