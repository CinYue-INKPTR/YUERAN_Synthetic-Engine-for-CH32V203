#include "ch32v20x.h"

#include "HardSPI.h"
#include "OLEDspi.h"
#include "HardSPI_W25Qxx.h"
#include "USART.h"
#include "TIM.h"
#include "LowPower.h"

#include "YUERAN_Define.h"
#include "YUERAN_LoadE.h"
#include "YUERAN_AlphaE.h"
#include "YUERAN_EtoFRAME.h"
#include "YUERAN_RefrashFRAME.h"
#include "YUERAN_FcnE.h"
#include "YUERAN_CmdLoad.h"
#include "YUERAN_Backstage.h"




int main(void)
{
	// System
    SystemCoreClockUpdate();
    NVIC_Interrupt_GroupInit();
    Delay_Init();
	LowPower_Init();

	// Hardware initialization
    HardSPI_Init();
    W25Qxx_Init();
    OLED_Init(OLED_AddressingMode_HorizontalMode, OLED_SetMode_X_Flip_Normal, OLED_SetMode_Y_Flip_Normal, OLED_SetMode_Color_Normal, 255);

	if (PWR_GetFlagStatus(PWR_FLAG_SB) == SET)	{LowPower_WKUP();}

	// Software initialization
    USART_Setup();
    TIM_Init();
    W25Qxx_GetIndex();

    while(1)
    {
		if (GlobalFlag & GlobalFlag_RenderingMode)
		{
			uint8_t i;
			
			// Load Rendering Cmd
			if (GlobalFlag & GlobalFlag_USART_CmdFn)
			{
				GlobalFlag &= ~GlobalFlag_USART_CmdFn;
				RenderCmdLoad();
			}
			USART1_SendData();
			if (BackData_Head != 0xff)	{BackData_Head = 0xff;}

		    HardSPI_SpeedChoose(HardSPI_Speed_Flash);
			for (i = 0 ; i < E_Layer_Size ; i++)
			{
				if (E_Layer[i].Type)
				{
					LoadE(i);
					if (E_Layer[i].PipelineFlag & Pipeline_AlphaE)	{AlphaE(i);}
					EtoFRAME(i);
				}
			}
			HardSPI_SpeedChoose(HardSPI_Speed_OLED);
			RefrashFRAME(GlobalFlag & GlobalFlag_FrameClr);
			for (i = 0 ; i < E_Layer_Size ; i++)	{FcnE(i);}
			
			// Waiting for RXD
			GetRenderingTime();
			GlobalFlag |= GlobalFlag_WaitRendering;
			while (GlobalFlag & GlobalFlag_WaitRendering);
			GlobalFlag &= ~GlobalFlag_TimeFull;
		}
		else
		{
			HardSPI_SpeedChoose(HardSPI_Speed_Flash);
			USART_SwitchTo(2);
			Backstage();
			USART_SwitchTo(1);
			W25Qxx_GetIndex();
		}
    }
}
