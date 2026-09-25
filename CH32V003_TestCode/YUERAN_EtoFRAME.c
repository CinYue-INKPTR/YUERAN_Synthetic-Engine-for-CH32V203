#include <stdint.h>
#include "YUERAN_EtoFRAME.h"
#include "YUERAN_Define.h"

static inline void EmixFRAME(uint8_t MixMode, uint8_t* Frame, uint8_t Dat)
{
    switch(MixMode)
    {
        case 0:
        {
            *Frame |= Dat;
        } break;
        case 1:
        {
            *Frame |= Dat;
        } break;
        case 2:
        {
            *Frame &= Dat;
        } break;
        case 3:
        {
            *Frame &= ~Dat;
        } break;
        case 4:
        {
            *Frame ^= Dat;
        } break;
    }
}

void EtoFRAME(uint8_t Layer)
{
    uint8_t iY, iX, temp, MixMode;
    int8_t MBy, Sy, Sx;

    // Calculate the offset value MBy
    MBy = E_Layer[Layer].Ypos % 8;
    if(MBy < 0)  {MBy += 8;}

    MixMode = E_Layer[Layer].MixMode;

    // Navigate through elements E page by page
    for(iY = 0 ; iY < E_Layer[Layer].Ysize ; iY++)
    {
        // Calculate the mapping of the current element's rendering position to the video memory coordinates Sy
        Sy = iY*8 + E_Layer[Layer].Ypos;
        // If Sy is completely outside the display area at present, skip
        if((Sy <= -8) || (Sy >= (FRAME_PageSize*8))){continue;}

        for(iX = 0 ; iX < E_Layer[Layer].Xsize ; iX++)
        {
            // Calculate the mapping of the current element's rendering position to the video memory coordinates Sx
            Sx = iX + E_Layer[Layer].Xpos;
            // If Sx is completely outside the display area at present, skip
            if((Sx < 0) || (Sx >= FRAME_ListSize))  {continue;}

            temp = E[iY*E_Layer[Layer].Xsize + iX];

            // If Sy is negative (-7~-1)
            if(Sy < 0)
            {
                // If the current rendering is the last page of E
                if((iY+1) == E_Layer[Layer].Ysize)
                {
                    FRAME[0][Sx] &= (MixMode == 0)?
                            (0xff << MBy) :
                        0xff;
                    EmixFRAME(MixMode, &FRAME[0][Sx], (MixMode == 2)?
                            ((temp >> (-Sy)) | (0xff << MBy)) :
                        (temp >> (-Sy))
                        );
                }
                // If it is the last page of E, only complete the operations above and skip the subsequent steps. If not, just skip it. Let Sy+8 handle the rendering later.
                continue;
            }

            // ???? Sy ?? 0~FRAME_PageSize*8-1?????? 0~63??
            // ???????? E ???????????????????????
            if(iY)
            {
                FRAME[Sy >> 3][Sx] &= (MixMode == 0)?
                        0 :
                    0xff;
                EmixFRAME(MixMode, &FRAME[Sy >> 3][Sx],
                    (E[(iY-1)*E_Layer[Layer].Xsize + iX] >> (8-MBy)) |
                    (temp << MBy)
                    );
            }
            // ????? E ????????????????
            else
            {
                FRAME[Sy >> 3][Sx] &= (MixMode == 0)?
                        (0xff >> (8-MBy)) :
                    0xff;
                EmixFRAME(MixMode, &FRAME[Sy >> 3][Sx], (MixMode == 2)?
                        ((temp << MBy) | (0xff >> (8-MBy))) :
                    (temp << MBy)
                    );
            }


            // ???????? E ????????????????尾??
            if(((iY+1) == E_Layer[Layer].Ysize) && (((Sy >> 3)+1) < FRAME_PageSize))
            {
                FRAME[(Sy >> 3)+1][Sx] &= (MixMode == 0)?
                        (0xff << MBy) :
                    0xff;
                EmixFRAME(MixMode, &FRAME[(Sy >> 3)+1][Sx], (MixMode == 2)?
                        ((temp >> (8-MBy)) | (0xff << MBy)) :
                    (temp >> (8-MBy))
                    );
            }
        }
    }
}