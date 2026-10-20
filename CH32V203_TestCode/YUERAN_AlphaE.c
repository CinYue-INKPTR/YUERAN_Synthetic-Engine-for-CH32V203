#include "YUERAN_AlphaE.h"

#include <stdint.h>
#include "YUERAN_Define.h"

#define AlphaFcn_GetMode(Dat)	((Dat) >> 5)

#define AlphaFcn_Mode_UNIFORM	0
#define AlphaFcn_Mode_TOP       1
#define AlphaFcn_Mode_BOTTOM    2
#define AlphaFcn_Mode_LEFT      3
#define AlphaFcn_Mode_RIGHT     4





static inline void AlphaE_UNIFORM(uint8_t Layer)
{
    uint16_t i, E_DrawLength;
	
	E_DrawLength = E_Layer[Layer].Xsize * E_Layer[Layer].Ysize;
	switch (E_Layer[Layer].Alpha >> 4)
	{
		case 0:
		{
			for (i = 0 ; i < E_DrawLength ; i++)
			{
				E[i] = 0;
			}
		} break;

		case 1:
		{
			for (i = 0 ; i < E_DrawLength ; i++)
			{
				if (i & 3)		{E[i] = 0;}
				else if (i & 7)	{E[i] &= 0x11;}
				else			{E[i] &= 0x44;}
			}
		} break;

		case 2:
		{
			for (i = 0 ; i < E_DrawLength ; i++)
			{
				if (i & 1)		{E[i] = 0;}
				else if (i & 3)	{E[i] &= 0x11;}
				else			{E[i] &= 0x44;}
			}
		} break;

		case 3:
		{
			for (i = 0 ; i < E_DrawLength ; i++)
			{
				if (i & 1)		{E[i] &= 0x11;}
				else			{E[i] &= 0x44;}
			}
		} break;

		case 4:
		{
			for (i = 0 ; i < E_DrawLength ; i++)
			{
				if (i & 3)
				{
					if (i & 1)		{E[i] &= 0xaa;}
					else			{E[i] &= 0x44;}
				}
				else			{E[i] &= 0x11;}
			}
		} break;

		case 5:
		{
			for (i = 0 ; i < E_DrawLength ; i++)
			{
				if (i & 1)		{E[i] &= 0x55;}
				else			{E[i] &= 0xaa;}
			}
		} break;

		case 6:
		{
			for (i = 0 ; i < E_DrawLength ; i++)
			{
				if (i & 3)
				{
					if (i & 1)		{E[i] &= ~0xaa;}
					else			{E[i] &= ~0x44;}
				}
				else			{E[i] &= ~0x11;}
			}
		} break;

		case 7:
		{
			for (i = 0 ; i < E_DrawLength ; i++)
			{
				if (i & 1)		{E[i] &= ~0x11;}
				else			{E[i] &= ~0x44;}
			}
		} break;

		case 8:
		{
			for (i = 0 ; i < E_DrawLength ; i++)
			{
				if (i & 1)		{continue;}
				else
				{
					if (i & 3)		{E[i] &= ~0x11;}
					else			{E[i] &= ~0x44;}
				}
			}
		} break;

		case 9:
		{
			for (i = 0 ; i < E_DrawLength ; i++)
			{
				if (i & 3)		{continue;}
				else
				{
					if (i & 7)		{E[i] &= ~0x11;}
					else			{E[i] &= ~0x44;}
				}
			}
		} break;
	}
}






static inline void AlphaE_TOP(uint8_t Layer)
{
    uint16_t i, E_DrawLength;
	
	E_DrawLength = E_Layer[Layer].Xsize * E_Layer[Layer].Ysize;
	switch(E_Layer[Layer].Alpha >> 4)
	{
		case 0:
		{
			for (i = 0 ; i < E_DrawLength ; i++)	{E[i] = 0;}
		} break;

		case 1:
		{
			for (i = 0 ; i < E_DrawLength ; i++)	{E[i] &= 0x01;}
		} break;

		case 2:
		{
			for (i = 0 ; i < E_DrawLength ; i++)	{E[i] &= 0x03;}
		} break;

		case 3:
		{
			for (i = 0 ; i < E_DrawLength ; i++)	{E[i] &= 0x07;}
		} break;

		case 4:
		{
			for (i = 0 ; i < E_DrawLength ; i++)	{E[i] &= 0x0f;}
		} break;

		case 5:
		{
			for (i = 0 ; i < E_DrawLength ; i++)	{E[i] &= 0x1f;}
		} break;

		case 6:
		{
			for (i = 0 ; i < E_DrawLength ; i++)	{E[i] &= 0x3f;}
		} break;

		case 7:
		{
			for (i = 0 ; i < E_DrawLength ; i++)	{E[i] &= 0x7f;}
		} break;
	}
}






static inline void AlphaE_BOTTOM(uint8_t Layer)
{
    uint16_t i, E_DrawLength;
	
	E_DrawLength = E_Layer[Layer].Xsize * E_Layer[Layer].Ysize;
	switch(E_Layer[Layer].Alpha >> 4)
	{
		case 0:
		{
			for (i = 0 ; i < E_DrawLength ; i++)	{E[i] = 0;}
		} break;

		case 1:
		{
			for (i = 0 ; i < E_DrawLength ; i++)	{E[i] &= 0x80;}
		} break;

		case 2:
		{
			for (i = 0 ; i < E_DrawLength ; i++)	{E[i] &= 0xd0;}
		} break;

		case 3:
		{
			for (i = 0 ; i < E_DrawLength ; i++)	{E[i] &= 0xe0;}
		} break;

		case 4:
		{
			for (i = 0 ; i < E_DrawLength ; i++)	{E[i] &= 0xf0;}
		} break;

		case 5:
		{
			for (i = 0 ; i < E_DrawLength ; i++)	{E[i] &= 0xf8;}
		} break;

		case 6:
		{
			for (i = 0 ; i < E_DrawLength ; i++)	{E[i] &= 0xfc;}
		} break;

		case 7:
		{
			for (i = 0 ; i < E_DrawLength ; i++)	{E[i] &= 0xfe;}
		} break;
	}
}






static inline void AlphaE_LEFT(uint8_t Layer)
{
	uint8_t  i;
	uint16_t j, Ratio;

	// Alpha / 128 * Xsize
	Ratio = ((uint16_t)E_Layer[Layer].Xsize * (uint16_t)E_Layer[Layer].Alpha) >> 7;
	Ratio = (Ratio > E_Layer[Layer].Xsize)?   0 : (E_Layer[Layer].Xsize - Ratio);

	for(i = 0 ; i < E_Layer[Layer].Ysize ; i++)
	{
		for(j = 0 ; j < Ratio ; j++)
		{
			E[(i+1)*E_Layer[Layer].Xsize - j - 1] = 0;
		}
	}
}





static inline void AlphaE_RIGHT(uint8_t Layer)
{
	uint8_t  i;
	uint16_t j, Ratio;

	// Alpha / 128 * Xsize
	Ratio = ((uint16_t)E_Layer[Layer].Xsize * (uint16_t)E_Layer[Layer].Alpha) >> 7;
	Ratio = (Ratio > E_Layer[Layer].Xsize)?   0 : (E_Layer[Layer].Xsize - Ratio);

	for(i = 0 ; i < E_Layer[Layer].Ysize ; i++)
	{
		for(j = 0 ; j < Ratio ; j++)
		{
			E[i*E_Layer[Layer].Xsize + j] = 0;
		}
	}
}





void AlphaE(uint8_t Layer)
{
    switch(AlphaFcn_GetMode(E_Layer[Layer].Alpha_Fcn))
    {
        case AlphaFcn_Mode_UNIFORM:	{AlphaE_UNIFORM(Layer);} break;
        case AlphaFcn_Mode_TOP:		{AlphaE_TOP(Layer);} break;
        case AlphaFcn_Mode_BOTTOM:	{AlphaE_BOTTOM(Layer);} break;
        case AlphaFcn_Mode_LEFT:	{AlphaE_LEFT(Layer);} break;
        case AlphaFcn_Mode_RIGHT:	{AlphaE_RIGHT(Layer);} break;
        default:					{AlphaE_UNIFORM(Layer);}
    }
}
