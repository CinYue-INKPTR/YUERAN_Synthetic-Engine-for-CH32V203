#include <stdint.h>
#include "YUERAN_AlphaE.h"
#include "YUERAN_Define.h"

#define AlphaE_Mode(Fcn)	((Fcn) >> 5)

static inline void AlphaE_Mode0(uint8_t Layer)
{
    uint16_t E_DrawLength = E_Layer[Layer].Xsize * E_Layer[Layer].Ysize;
	uint16_t i;
	
	switch(E_Layer[Layer].Alpha)
	{
		case 0:
		{
			for(i = 0 ; i < E_DrawLength ; i++)
			{
				E[i] = 0;
			}
		} break;

		case 1:
		{
			for(i = 0 ; i < E_DrawLength ; i++)
			{
				if(i & 3)
				{
					E[i] = 0;
				}
				else
				{
					if(i & 7)
					{
						E[i] &= 0x11;
					}
					else
					{
						E[i] &= 0x44;
					}
				}
			}
		} break;

		case 2:
		{
			for(i = 0 ; i < E_DrawLength ; i++)
			{
				if(i & 1)
				{
					E[i] = 0;
				}
				else
				{
					if(i & 0x03)
					{
						E[i] &= 0x11;
					}
					else
					{
						E[i] &= 0x44;
					}
				}
			}
		} break;

		case 3:
		{
			for(i = 0 ; i < E_DrawLength ; i++)
			{
				if(i & 1)
				{
					E[i] &= 0x11;
				}
				else
				{
					E[i] &= 0x44;
				}
			}
		} break;

		case 4:
		{
			for(i = 0 ; i < E_DrawLength ; i++)
			{
				if(i & 3)
				{
					if(i & 1)
					{
						E[i] &= 0xaa;
					}
					else
					{
						E[i] &= 0x44;
					}
				}
				else
				{
					E[i] &= 0x11;
				}
			}
		} break;

		case 5:
		{
			for(i = 0 ; i < E_DrawLength ; i++)
			{
				if(i & 1)
				{
					E[i] &= 0x55;
				}
				else
				{
					E[i] &= 0xaa;
				}
			}
		} break;

		case 6:
		{
			for(i = 0 ; i < E_DrawLength ; i++)
			{
				if(i & 3)
				{
					if(i & 1)
					{
						E[i] &= ~0xaa;
					}
					else
					{
						E[i] &= ~0x44;
					}
				}
				else
				{
					E[i] &= ~0x11;
				}
			}
		} break;

		case 7:
		{
			for(i = 0 ; i < E_DrawLength ; i++)
			{
				if(i & 1)
				{
					E[i] &= ~0x11;
				}
				else
				{
					E[i] &= ~0x44;
				}
			}
		} break;

		case 8:
		{
			for(i = 0 ; i < E_DrawLength ; i++)
			{
				if(i & 1)
				{
					continue;
				}
				else
				{
					if(i & 0x03)
					{
						E[i] &= ~0x11;
					}
					else
					{
						E[i] &= ~0x44;
					}
				}
			}
		} break;

		case 9:
		{
			for(i = 0 ; i < E_DrawLength ; i++)
			{
				if(i & 3)
				{
					continue;
				}
				else
				{
					if(i & 7)
					{
						E[i] &= ~0x11;
					}
					else
					{
						E[i] &= ~0x44;
					}
				}
			}
		} break;
	}
}

static inline void AlphaE_Mode1(uint8_t Layer)
{
    uint16_t E_DrawLength = E_Layer[Layer].Xsize * E_Layer[Layer].Ysize;
	uint16_t i;
	
	switch(E_Layer[Layer].Alpha)
	{
		case 0:
		{
			for(i = 0 ; i < E_DrawLength ; i++)
			{
				E[i] = 0;
			}
		} break;

		case 1:
		{
			for(i = 0 ; i < E_DrawLength ; i++)
			{
				E[i] &= 0x01;
			}
		} break;

		case 2:
		{
			for(i = 0 ; i < E_DrawLength ; i++)
			{
				E[i] &= 0x03;
			}
		} break;

		case 3:
		{
			for(i = 0 ; i < E_DrawLength ; i++)
			{
				E[i] &= 0x07;
			}
		} break;

		case 4:
		{
			for(i = 0 ; i < E_DrawLength ; i++)
			{
				E[i] &= 0x0f;
			}
		} break;

		case 5:
		{
			for(i = 0 ; i < E_DrawLength ; i++)
			{
				E[i] &= 0x1f;
			}
		} break;

		case 6:
		{
			for(i = 0 ; i < E_DrawLength ; i++)
			{
				E[i] &= 0x3f;
			}
		} break;

		case 7:
		{
			for(i = 0 ; i < E_DrawLength ; i++)
			{
				E[i] &= 0x7f;
			}
		} break;
	}
}

static inline void AlphaE_Mode2(uint8_t Layer)
{
    uint16_t E_DrawLength = E_Layer[Layer].Xsize * E_Layer[Layer].Ysize;
	uint16_t i;
	
	switch(E_Layer[Layer].Alpha)
	{
		case 0:
		{
			for(i = 0 ; i < E_DrawLength ; i++)
			{
				E[i] = 0;
			}
		} break;

		case 1:
		{
			for(i = 0 ; i < E_DrawLength ; i++)
			{
				E[i] &= 0x80;
			}
		} break;

		case 2:
		{
			for(i = 0 ; i < E_DrawLength ; i++)
			{
				E[i] &= 0xd0;
			}
		} break;

		case 3:
		{
			for(i = 0 ; i < E_DrawLength ; i++)
			{
				E[i] &= 0xe0;
			}
		} break;

		case 4:
		{
			for(i = 0 ; i < E_DrawLength ; i++)
			{
				E[i] &= 0xf0;
			}
		} break;

		case 5:
		{
			for(i = 0 ; i < E_DrawLength ; i++)
			{
				E[i] &= 0xf8;
			}
		} break;

		case 6:
		{
			for(i = 0 ; i < E_DrawLength ; i++)
			{
				E[i] &= 0xfc;
			}
		} break;

		case 7:
		{
			for(i = 0 ; i < E_DrawLength ; i++)
			{
				E[i] &= 0xfe;
			}
		} break;
	}
}

static inline void AlphaE_Mode3(uint8_t Layer)
{
	uint8_t  i;
	uint16_t j, temp;

	// Alpha / 255 * Xsize

	temp = ((uint16_t)E_Layer[Layer].Xsize * (uint16_t)E_Layer[Layer].Alpha) >> 8;
	temp = E_Layer[Layer].Xsize - temp;

	for(i = 0 ; i < E_Layer[Layer].Ysize ; i++)
	{
		for(j = 0 ; j < temp ; j++)
		{
			E[(i+1)*E_Layer[Layer].Xsize - j - 1] = 0;
		}
	}
}

static inline void AlphaE_Mode4(uint8_t Layer)
{
	uint8_t  i;
	uint16_t j, temp;

	// Alpha / 255 * Xsize

	temp = ((uint16_t)E_Layer[Layer].Xsize * (uint16_t)E_Layer[Layer].Alpha) >> 8;
	temp = E_Layer[Layer].Xsize - temp;

	for(i = 0 ; i < E_Layer[Layer].Ysize ; i++)
	{
		for(j = 0 ; j < temp ; j++)
		{
			E[i*E_Layer[Layer].Xsize + j] = 0;
		}
	}
}

void AlphaE(uint8_t Layer)
{
    switch(AlphaE_Mode(E_Layer[Layer].Alpha_Fcn))
    {
        case 0:	{AlphaE_Mode0(Layer);} break;
        case 1:	{AlphaE_Mode1(Layer);} break;
        case 2:	{AlphaE_Mode2(Layer);} break;
        case 3:	{AlphaE_Mode3(Layer);} break;
        case 4:	{AlphaE_Mode4(Layer);} break;
        default:{AlphaE_Mode0(Layer);}
    }
}
