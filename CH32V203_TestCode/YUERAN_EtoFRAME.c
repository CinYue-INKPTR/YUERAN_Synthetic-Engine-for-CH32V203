#include <stdint.h>
#include "YUERAN_EtoFRAME.h"
#include "YUERAN_Define.h"




#define MixMode_Coverage    0
#define MixMode_OR          1
#define MixMode_AND         2
#define MixMode_NON         3
#define MixMode_ExclusiveOR 4




static inline void EmixFRAME(uint8_t MixMode, uint8_t* Frame, uint8_t Dat)
{
    switch(MixMode & 0x7f)
    {
        case MixMode_Coverage:
        {
			*Frame |= Dat;
        } break;
        case MixMode_OR:
        {
			*Frame |= Dat;
        } break;
        case MixMode_AND:
        {
            *Frame &= Dat;
        } break;
        case MixMode_NON:
        {
            *Frame &= ~Dat;
        } break;
        case MixMode_ExclusiveOR:
        {
			*Frame ^= Dat;
        } break;
    }
}




void EtoFRAME(uint8_t Layer)
{
    uint8_t iY, iX, temp;
    int8_t MBy, MBy_o, Sy, Sx;

    // Calculate the offset value MBy
    MBy = E_Layer[Layer].Ypos % 8;
    if (MBy < 0)  {MBy += 8;}
    MBy_o = 8 - MBy;

    // Navigate through elements E page by page
    for (iY = 0 ; iY < E_Layer[Layer].Ysize ; iY++)
    {
        // Calculate the mapping of the current element's rendering position to the video memory coordinates Sy
        Sy = (iY << 3) + E_Layer[Layer].Ypos;
        // If Sy is completely outside the display area at present, skip
        if ((Sy <= -8) || (Sy >= (FRAME_PageSize*8)))    {continue;}

        for (iX = 0 ; iX < E_Layer[Layer].Xsize ; iX++)
        {
            // Calculate the mapping of the current element's rendering position to the video memory coordinates Sx
            Sx = iX + E_Layer[Layer].Xpos;
            // If Sx is completely outside the display area at present, skip
            if ((Sx < 0) || (Sx >= FRAME_ListSize))  {continue;}

            temp = E[iY*E_Layer[Layer].Xsize + iX];

            // If Sy is negative (-7~-1)
            if (Sy < 0)
            {
                // If the current rendering is the last page of E
                if((iY+1) == E_Layer[Layer].Ysize)
                {
                    FRAME[0][Sx] &= (E_Layer[Layer].MixMode == MixMode_Coverage)? (0xff << MBy) : 0xff;
                    EmixFRAME(
                        E_Layer[Layer].MixMode,
                        &FRAME[0][Sx],
                        (E_Layer[Layer].MixMode == MixMode_AND)? ((temp >> (-Sy)) | (0xff << MBy)) : (temp >> (-Sy))
                        );
                }
                // If it is the last page of E, only complete the operations above and skip the subsequent steps. If not, just skip it. Let Sy+8 handle the rendering later.
                continue;
            }

			// At this point, the rendering coordinate Sy becomes positive.
            // If the current rendering position is on page 2 or later of E
            if (iY)
            {
                FRAME[Sy >> 3][Sx] &= (E_Layer[Layer].MixMode == MixMode_Coverage)? 0 : 0xff;
                EmixFRAME(
                    E_Layer[Layer].MixMode,
                    &FRAME[Sy >> 3][Sx],
                    (E[(iY-1)*E_Layer[Layer].Xsize + iX] >> MBy_o) | (temp << MBy)
                    );
            }
            // If the current rendering position is on the first page of E
            else
            {
                FRAME[Sy >> 3][Sx] &= (E_Layer[Layer].MixMode == MixMode_Coverage)? (0xff >> MBy_o) : 0xff;
                EmixFRAME(
                    E_Layer[Layer].MixMode,
                    &FRAME[Sy >> 3][Sx],
                    (E_Layer[Layer].MixMode == MixMode_AND)? ((temp << MBy) | (0xff >> MBy_o)) : (temp << MBy)
                    );
            }

            // If the current rendering position is on the last page of E
            if(((iY+1) == E_Layer[Layer].Ysize) && (((Sy >> 3)+1) < FRAME_PageSize))
            {
                FRAME[(Sy >> 3)+1][Sx] &= (E_Layer[Layer].MixMode == MixMode_Coverage)? (0xff << MBy) : 0xff;
                EmixFRAME(
                    E_Layer[Layer].MixMode,
                    &FRAME[(Sy >> 3)+1][Sx],
                    (E_Layer[Layer].MixMode == MixMode_AND)? ((temp >> MBy_o) | (0xff << MBy)) : (temp >> MBy_o)
                    );
            }
        }
    }
}
