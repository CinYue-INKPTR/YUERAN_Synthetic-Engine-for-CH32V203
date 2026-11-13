// v1.0.0.261113
#include "YUERAN_AlphaE.h"

#include <stdint.h>
#include "YUERAN_Define.h"

#define AlphaFcn_GetMode(Dat)	((Dat) >> 5)
#define Uniform_GetLevel(Dat)	((Dat) >> 4)

#define AlphaFcn_Mode_UNIFORM	0
#define AlphaFcn_Mode_TOP       1
#define AlphaFcn_Mode_BOTTOM    2
#define AlphaFcn_Mode_LEFT      3
#define AlphaFcn_Mode_RIGHT     4




static inline void AlphaE_UNIFORM(uint8_t Layer);
static inline void AlphaE_TOP(uint8_t Layer);
static inline void AlphaE_BOTTOM(uint8_t Layer);
static inline void AlphaE_LEFT(uint8_t Layer);
static inline void AlphaE_RIGHT(uint8_t Layer);
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




static inline void AlphaE_UNIFORM(uint8_t Layer)
{
    uint8_t i, j, Value;
	static const uint8_t UniformMask[9][8] =
	{
		{0x11, 0x00, 0x00, 0x00, 0x44, 0x00, 0x00, 0x00}, // 1
		{0x11, 0x00, 0x44, 0x00, 0x11, 0x00, 0x44, 0x00}, // 2
		{0x11, 0x44, 0x11, 0x44, 0x11, 0x44, 0x11, 0x44}, // 3
		{0x11, 0xaa, 0x44, 0xaa, 0x11, 0xaa, 0x44, 0xaa}, // 4
		{0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa}, // 5
		{0xee, 0x55, 0xbb, 0x55, 0xee, 0x55, 0xbb, 0x55}, // 6
		{0xee, 0xbb, 0xee, 0xbb, 0xee, 0xbb, 0xee, 0xbb}, // 7
		{0xee, 0xff, 0xbb, 0xff, 0xee, 0xff, 0xbb, 0xff}, // 8
		{0xee, 0xff, 0xff, 0xff, 0xbb, 0xff, 0xff, 0xff}, // 9
	};
	
	if (E_Layer[Layer].Alpha_Target > 160)	{E_Layer[Layer].Alpha_Target = 160;}
	if (E_Layer[Layer].Alpha > 160)	{E_Layer[Layer].Alpha = 160;}
	if (E_Layer[Layer].Alpha == 160)	{return;}
	Value = Uniform_GetLevel(E_Layer[Layer].Alpha);

	if (Value)
	{
		for (i = 0 ; i <E_Layer[Layer].Ysize ; i++)
		{
			for (j = 0 ; j < E_Layer[Layer].Xsize ; j++)
			{
				E[i*E_Layer[Layer].Xsize + j] &= UniformMask[Value - 1][j&7];
			}
		}
	}
	else
	{
		for (i = 0 ; i <E_Layer[Layer].Ysize ; i++)
		{
			for (j = 0 ; j < E_Layer[Layer].Xsize ; j++)
			{
				E[i*E_Layer[Layer].Xsize + j] = 0;
			}
		}
	}
}






static inline void AlphaE_TOP(uint8_t Layer)
{
	uint8_t i, j, PgAddr, MvByte, Mask;
	uint16_t Ratio;

	// Alpha / 64 * Y_PXsize
	if (E_Layer[Layer].Alpha_Target > 64)	{E_Layer[Layer].Alpha_Target = 64;}
	if (E_Layer[Layer].Alpha > 64)	{E_Layer[Layer].Alpha = 64;}
	if (E_Layer[Layer].Alpha == 64)	{return;}
	Ratio = ((uint16_t)E_Layer[Layer].Ysize*8 * (uint16_t)E_Layer[Layer].Alpha) >> 6;

	// Ratio/8	Ratio%8
	PgAddr = Ratio >> 3;
	MvByte = Ratio & 7;
	Mask = 0xff >> (8 - MvByte);
	if (PgAddr < E_Layer[Layer].Ysize)
	{
		for (i = 0 ; i < E_Layer[Layer].Xsize ; i++)	{E[PgAddr*E_Layer[Layer].Xsize + i] &= Mask;}
		PgAddr++;
	}
	else	{return;}
	
	if (PgAddr < E_Layer[Layer].Ysize)
	{
		for (i = PgAddr ; i < E_Layer[Layer].Ysize ; i++)
		{
			for (j = 0 ; j < E_Layer[Layer].Xsize ; j++)
			{
				E[i*E_Layer[Layer].Xsize + j] = 0x00;
			}
		}
	}
}






static inline void AlphaE_BOTTOM(uint8_t Layer)
{
    uint8_t i, j, PgAddr, MvByte, Mask, tmp;
	uint16_t Ratio;

	// Alpha / 64 * Y_PXsize
	if (E_Layer[Layer].Alpha_Target > 64)	{E_Layer[Layer].Alpha_Target = 64;}
	if (E_Layer[Layer].Alpha > 64)	{E_Layer[Layer].Alpha = 64;}
	if (E_Layer[Layer].Alpha == 64)	{return;}
	Ratio = ((uint16_t)E_Layer[Layer].Ysize*8 * (uint16_t)E_Layer[Layer].Alpha) >> 6;

	tmp = Ratio >> 3;
	// Ratio/8	Ratio%8
	PgAddr = E_Layer[Layer].Ysize - tmp - 1;
	MvByte = Ratio & 7;
	Mask = 0xff << (8 - MvByte);

	// Clear pages before PgAddr
	for (i = 0 ; i < PgAddr ; i++)
	{
		for (j = 0 ; j < E_Layer[Layer].Xsize ; j++)
		{
			E[i*E_Layer[Layer].Xsize + j] = 0x00;
		}
	}

	// Edit PgAddr
	if (PgAddr < E_Layer[Layer].Ysize)
	{
		for (i = 0 ; i < E_Layer[Layer].Xsize ; i++)	{E[PgAddr*E_Layer[Layer].Xsize + i] &= Mask;}
	}
	else	{return;}
}






static inline void AlphaE_LEFT(uint8_t Layer)
{
	uint8_t  i;
	uint16_t j, Ratio;

	// Alpha / 128 * Xsize
	if (E_Layer[Layer].Alpha_Target > 128)	{E_Layer[Layer].Alpha_Target = 128;}
	if (E_Layer[Layer].Alpha > 128)	{E_Layer[Layer].Alpha = 128;}
	if (E_Layer[Layer].Alpha == 128)	{return;}
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
	if (E_Layer[Layer].Alpha_Target > 128)	{E_Layer[Layer].Alpha_Target = 128;}
	if (E_Layer[Layer].Alpha > 128)	{E_Layer[Layer].Alpha = 128;}
	if (E_Layer[Layer].Alpha == 128)	{return;}
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