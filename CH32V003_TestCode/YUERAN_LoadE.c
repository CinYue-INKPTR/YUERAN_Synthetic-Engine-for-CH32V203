#include <stdint.h>
#include "YUERAN_LoadE.h"
#include "YUERAN_Define.h"
#include "Font.h"

// Load the original materials to E
void LoadE(uint8_t Layer)
{
    uint8_t i, j, k, temp;

    if(E_Layer[Layer].Type == Type_Picture)
    {
        return;
    }


    if(E_Layer[Layer].Type == Type_Char)
    {
        // ?????????????????????
        for(i = 0 ; i < E_Layer[Layer].TextLength ; i++)
        {
            if(i < (E_Size / E_Layer[Layer].Ysize / E_Layer[Layer].Xsize))
            {
                // ???????????????
                for(j = 0 ; j < E_Layer[Layer].Ysize ; j++)
                {
                    // ?????????????????
                    for(k = 0 ; k < E_Layer[Layer].Xsize ; k++)
                    {
                        E[i*E_Layer[Layer].Xsize*E_Layer[Layer].Ysize + j*E_Layer[Layer].Xsize + k] = Font[(E_Layer[Layer].Text[i] - ' ')*6 + k];
                    }
                }
            }
        }
        return;
    }


    if(E_Layer[Layer].Type == Type_DrawBlock)
    {
        for(i = 0 ; i < E_Layer[Layer].Ysize ; i++)
        {
            for(j = 0 ; j < E_Layer[Layer].Xsize ; j++)
            {
                temp = i*E_Layer[Layer].Xsize + j;
                if(temp < E_Size)
                {
                    E[temp] = 0xaf;
                }
            }
        }
        return;
    }
}