// v1.0.0.261113
#ifndef TIM_H
#define TIM_H

#include "ch32v20x.h"

#define TIM_PRESCALER    (SystemCoreClock / 1000000)

void TIM_Init(void);

#endif
