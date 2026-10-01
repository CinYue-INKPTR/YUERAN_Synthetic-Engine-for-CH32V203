#ifndef HardSPI_H
#define HardSPI_H

#include "ch32v20x.h"

// HardSPI map
#define HardSPI_RCC_APB2Periph		RCC_APB2Periph_SPI1
// HardSPI Port
#define HardSPI_Port				SPI1
// Pin map
#define HardSPI_SCK_RCC_APB2Periph	RCC_APB2Periph_GPIOA
#define HardSPI_MOSI_RCC_APB2Periph	RCC_APB2Periph_GPIOA
#define HardSPI_MISO_RCC_APB2Periph	RCC_APB2Periph_GPIOA
// SCK
#define HardSPI_SCK_Port         	GPIOA
#define HardSPI_SCK_Pin          	GPIO_Pin_5
// MOSI
#define HardSPI_MOSI_Port        	GPIOA
#define HardSPI_MOSI_Pin         	GPIO_Pin_7
// MISO
#define HardSPI_MISO_Port        	GPIOA
#define HardSPI_MISO_Pin         	GPIO_Pin_6
// Pin speed
#define HardSPI_PinSpeed			GPIO_Speed_10MHz

void 	HardSPI_Init(void);
uint8_t HardSPI_SwapBytes(uint8_t Tx_Byte);

#endif
