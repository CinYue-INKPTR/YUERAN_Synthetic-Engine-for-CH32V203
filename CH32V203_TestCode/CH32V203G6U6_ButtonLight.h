#ifndef CH32V203G6U6_ButtonLight_H
#define CH32V203G6U6_ButtonLight_H

#include "ch32v20x.h"

void    ButtonLight_Init(void);
uint8_t ReadButton(void);
uint8_t ButtonLogic(void);
void    EditLight(uint8_t Dat);

#endif
