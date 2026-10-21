// Checked - 261017.1224
#ifndef TIM_H
#define TIM_H

#include "ch32v20x.h"

#define TIM_PRESCALER    (SystemCoreClock / 1000000)

void TIM_Init(void);

#endif
