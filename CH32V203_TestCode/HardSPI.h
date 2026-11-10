// Checked - 2610171113
#ifndef HardSPI_H
#define HardSPI_H

#include "ch32v20x.h"

typedef enum {HardSPI_Speed_Flash, HardSPI_Speed_OLED} HardSPI_Speed;

void HardSPI_SpeedChoose(HardSPI_Speed Speed);
void HardSPI_Init(void);
uint8_t HardSPI_SwapBytes(uint8_t Tx_Byte);

#endif
