#include "ch32v20x.h"
#include "CH32V203G6U6_ButtonLight.h"

#include "HardSPI.h"
#include "YUERAN_OLEDspi.h"

#include "YUERAN_Define.h"
#include "YUERAN_LoadE.h"
#include "YUERAN_AlphaE.h"
#include "YUERAN_EtoFRAME.h"
#include "YUERAN_RefrashFRAME.h"
#include "YUERAN_FcnE.h"

int main(void)
{
    uint8_t i = 0;

    SystemCoreClockUpdate();
    Delay_Init();
    ButtonLight_Init();

    HardSPI_Init();
    OLED_Init(OLED_AddressingMode_HorizontalMode, OLED_SetMode_X_Flip_Normal, OLED_SetMode_Y_Flip_Normal, OLED_SetMode_Color_Normal, 255);

    E_Layer[0].Type = Type_DrawBlock;
    // Type_DrawBlock   |   MixMode, Xpos, Ypos, Alpha, Alpha_Fcn, Block_Xsize, Block_Ysize, Block_Corners
    E_Layer[0].MixMode = 0;

    E_Layer[0].Xpos = 0;
    E_Layer[0].Xpos_Fcn = (2 << 6)+4;

    E_Layer[0].Ypos = 0;
    E_Layer[0].Ypos_Fcn = (2 << 6)+4;

    E_Layer[0].Alpha = 16;
    E_Layer[0].Alpha_Fcn = (0 << 5)+(1 << 4)+1;

    E_Layer[0].Block_Xsize = 0;
    E_Layer[0].Block_Xsize_Fcn = (2 << 6)+2;

    E_Layer[0].Block_Ysize = 0;
    E_Layer[0].Block_Ysize_Fcn = (1 << 6)+2;

    E_Layer[0].Block_Corners = 2;

    while(1)
    {
        if(i)
        {
            E_Layer[0].Xpos_Target = 0;
            E_Layer[0].Ypos_Target = 0;
            E_Layer[0].Alpha_Target = 1;
            E_Layer[0].Block_Xsize_Target = 10;
            E_Layer[0].Block_Ysize_Target = 40;
        }
        else
        {
            E_Layer[0].Xpos_Target = 90;
            E_Layer[0].Ypos_Target = 50;
            E_Layer[0].Alpha_Target = 16;
            E_Layer[0].Block_Xsize_Target = 30;
            E_Layer[0].Block_Ysize_Target = 14;
        }



        LoadE(0);
        AlphaE(0);
        EtoFRAME(0);
        RefrashFRAME(1);
        FcnE(0);


        Delay_Ms(30);

        if(ButtonLogic())
        {
            i = !i;
        }
    }
}
