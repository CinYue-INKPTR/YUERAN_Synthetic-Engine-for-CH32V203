#include "ch32v00x.h"
#include "INKPTR_SoftIIC.h"
#include "INKPTR_OLEDiic.h"
#include "Font.h"
#include "YUERAN_Define.h"
#include "YUERAN_LoadE.h"
#include "YUERAN_AlphaE.h"
#include "YUERAN_EtoFRAME.h"
#include "YUERAN_RefrashFRAME.h"

#define ButtonLight_Port    GPIOA
#define Button_Pin          GPIO_Pin_1
#define Light_Pin           GPIO_Pin_2

void MainInit(void)
{
    uint8_t i;
    uint8_t Str[] = "ABC";

    SystemCoreClockUpdate();
    Delay_Init();

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
    GPIO_PinRemapConfig(GPIO_Remap_PA1_2, DISABLE);
    RCC_HSEConfig(RCC_HSE_OFF);

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    // Button
    GPIO_InitStructure.GPIO_Pin     = Button_Pin;
    GPIO_InitStructure.GPIO_Mode    = GPIO_Mode_IN_FLOATING;
    GPIO_InitStructure.GPIO_Speed   = GPIO_Speed_2MHz;
    GPIO_Init(ButtonLight_Port, &GPIO_InitStructure);
    // Light
    GPIO_InitStructure.GPIO_Pin     = Light_Pin;
    GPIO_InitStructure.GPIO_Mode    = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed   = GPIO_Speed_2MHz;
    GPIO_Init(ButtonLight_Port, &GPIO_InitStructure);

    SoftIIC_Init();
    OLED_Init(OLED_AddressingMode_HorizontalMode, OLED_SetMode_X_Flip_Normal, OLED_SetMode_Y_Flip_Normal, OLED_SetMode_Color_Normal, 255);



    E_Layer[0].Type = Type_Char;
    E_Layer[0].Xsize = 6;
    E_Layer[0].Ysize = 1;
    E_Layer[0].MixMode = 0;
    E_Layer[0].Xpos = 0;
    E_Layer[0].Ypos = 0;
    E_Layer[0].Alpha = 10;
    E_Layer[0].Alpha_Fcn = Alpha_Mode0;
    E_Layer[0].TextLength = 3;
    for(i = 0 ; i < E_Layer[0].TextLength ; i++)
    {
        E_Layer[0].Text[i] = Str[i];
    }

    E_Layer[1].Type = Type_DrawBlock;
    E_Layer[1].Xsize = 6;
    E_Layer[1].Ysize = 3;
    E_Layer[1].MixMode = 0;
    E_Layer[1].Xpos = 2;
    E_Layer[1].Ypos = 3;
    E_Layer[1].Alpha = 10;
    E_Layer[1].Alpha_Fcn = Alpha_Mode0;

}

int main(void)
{
    uint8_t i = 10, n, m = 100, l = 1;
    int8_t j = 127, k = 30;
    MainInit();


    while(1)
    {
        if(GPIO_ReadInputDataBit(ButtonLight_Port, Button_Pin) == RESET)
        {
            Delay_Ms(10);
            E_Layer[1].MixMode = (E_Layer[1].MixMode == 4)? 0 : E_Layer[1].MixMode+1;
            while(GPIO_ReadInputDataBit(ButtonLight_Port, Button_Pin) == RESET);
        }

        FRAME[0][0] = 0xaf;
        for(n = 0 ; n < 128 ; n++)
        {
            FRAME[1][n] = 0xff;
        }

        FRAME[0][16] = Font[(E_Layer[1].MixMode+0x30-0x20)*6 + 0];
        FRAME[0][17] = Font[(E_Layer[1].MixMode+0x30-0x20)*6 + 1];
        FRAME[0][18] = Font[(E_Layer[1].MixMode+0x30-0x20)*6 + 2];
        FRAME[0][19] = Font[(E_Layer[1].MixMode+0x30-0x20)*6 + 3];
        FRAME[0][20] = Font[(E_Layer[1].MixMode+0x30-0x20)*6 + 4];
        FRAME[0][21] = Font[(E_Layer[1].MixMode+0x30-0x20)*6 + 5];

        FRAME[3][0] = 0xaf;



        LoadE(1);
        AlphaE(1);
        EtoFRAME(1);
        E_Layer[1].Alpha = i--;
        E_Layer[1].Xpos = j;
        E_Layer[1].Ypos = k;
        RefrashFRAME(l);

        if(i == 0) i = 10;
        if(j > -5) j--;
        else j = 127;
        if(k > -23) k--;
        else k = 31;
        Delay_Ms(100);
        if(!m) {m=100; l = !l;}
        else {m--;}
    }
}
