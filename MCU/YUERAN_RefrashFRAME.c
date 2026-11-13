// v1.0.0.261113
#include "YUERAN_RefrashFRAME.h"

#include <stdint.h>
#include "YUERAN_Define.h"
#include "OLEDspi.h"
#include "HardSPI.h"

// Push the full-screen cache FRAME onto the screen
void RefrashFRAME(uint8_t ClearEN)
{
    uint8_t i, j;

    OLED_Draw_CmdHead_WholeArea();
    for(i = 0 ; i < FRAME_PageSize ; i++)
    {
        for(j = 0 ; j < FRAME_ListSize ; j++)
        {
            OLED_Draw_CmdData(FRAME[i][j]);
        }
    }
    OLED_Draw_CmdTail();

    if(ClearEN)
    {
        for(i = 0 ; i < FRAME_PageSize ; i++)
        {
            for(j = 0 ; j < FRAME_ListSize ; j++)
            {
                FRAME[i][j] = 0;
            }
        }
    }
    
}
