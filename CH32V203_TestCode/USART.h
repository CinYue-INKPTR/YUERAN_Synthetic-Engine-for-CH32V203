// Checked - 261017.1311
#ifndef USART_H
#define USART_H

#include "ch32v20x.h"

void GetRenderingTime(void);
void USART1_SendData(void);
void USART2_SendData(uint8_t* Dat, uint16_t Length);
//  static inline uint8_t USART_ReadByte_ClrFlg(USART_TypeDef *USARTx)
void USART_SwitchTo(uint8_t PORT);
void USART_Setup(void);

#endif
