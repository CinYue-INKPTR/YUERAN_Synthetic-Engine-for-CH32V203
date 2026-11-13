// v1.0.0.261113
#include "YUERAN_LoadE.h"

#include <stdint.h>
#include "YUERAN_Define.h"
#include "HardSPI_W25Qxx.h"
#include "HardSPI.h"

#define SectorAddr_to_ByteAddr(Dat)	((Dat) << 12)

#define Addr_1(Dat)					((uint8_t)((Dat) >> 16))
#define Addr_2(Dat)					((uint8_t)((Dat) >> 8))
#define Addr_3(Dat)					((uint8_t)(Dat))

#define BlockCorners_GetMode(Dat)	((Dat) & 0x80)
#define BlockCorners_BeveledMode	0x80
#define BlockCorners_RoundedMode	0x00
#define BlockCorners_GetValue(Dat)	((Dat) & 0x7f)

#define R_Corners_Mask_LevelQuantity    6
#define B_Corners_Mask_LevelQuantity    8






static inline void LoadPicture(uint8_t Layer);
static inline void LoadChr_ASCII(uint8_t Layer);
static inline void LoadChr_GB2312(uint8_t Layer);
static inline void LoadDrawBlock(uint8_t Layer);
void LoadE(uint8_t Layer)
{
	switch(E_Layer[Layer].Type)
	{
		case Type_Picture: 		{LoadPicture(Layer);} break;
		case Type_ASCIIchr:		{LoadChr_ASCII(Layer);} break;
		case Type_GB2312chr:	{LoadChr_GB2312(Layer);} break;
		case Type_DrawBlock:	{LoadDrawBlock(Layer);} break;
	}
}






static inline void LoadPicture(uint8_t Layer)
{
    uint8_t i, j;
    uint16_t PcrSize;
    uint32_t BasicAdd, Add;

    PcrSize = E_Layer[Layer].Xsize * E_Layer[Layer].Ysize;
	if (PcrSize > E_Size)	{return;}

    // Get the basic sector address in flash
    BasicAdd = ((uint16_t)FlashIndex[E_Layer[Layer].Flash_Item*FlashIndex_Size + FlsIdx_p2_Add] << 8) + FlashIndex[E_Layer[Layer].Flash_Item*FlashIndex_Size + FlsIdx_p2_Add+1];

    Add = (E_Layer[Layer].Picture_Add + E_Layer[Layer].StreamOffset) * PcrSize + SectorAddr_to_ByteAddr(BasicAdd);

    W25Qxx_Start();
    HardSPI_SwapBytes(W25Qxx_Cmd_ReadData);
    HardSPI_SwapBytes(Addr_1(Add));
    HardSPI_SwapBytes(Addr_2(Add));
    HardSPI_SwapBytes(Addr_3(Add));
    for (i = 0 ; i < E_Layer[Layer].Ysize ; i++)
    {
        for (j = 0 ; j < E_Layer[Layer].Xsize ; j++)
        {
            E[i*E_Layer[Layer].Xsize + j] = HardSPI_SwapBytes(0xff);
        }
    }
    W25Qxx_Stop();
}





static inline void LoadChr_ASCII(uint8_t Layer)
{
	uint8_t i, j, k, ChrEnableQuantity;
	uint16_t ChrSize, PageBytes;
	uint32_t BasicAdd, Add;

	// E_Layer[Layer].Block_Xsize_Target: Why use Block_?
	// - Refer to the comments on the related parameters of the CmdLoad.
	ChrSize = E_Layer[Layer].Block_Xsize_Target * E_Layer[Layer].Ysize;
	ChrEnableQuantity = (ChrSize)? (E_Size / ChrSize) : 0;
	ChrEnableQuantity = (ChrEnableQuantity < E_Layer[Layer].Text_Length)? ChrEnableQuantity : E_Layer[Layer].Text_Length;
	PageBytes = ChrEnableQuantity * E_Layer[Layer].Block_Xsize_Target;
	while (PageBytes > 255)
	{
		PageBytes -= E_Layer[Layer].Block_Xsize_Target;
		ChrEnableQuantity--;
	}
	E_Layer[Layer].Xsize = PageBytes;

	// Get the basic sector address in flash
	BasicAdd = ((uint16_t)FlashIndex[E_Layer[Layer].Flash_Item*FlashIndex_Size + FlsIdx_p2_Add] << 8) + FlashIndex[E_Layer[Layer].Flash_Item*FlashIndex_Size + FlsIdx_p2_Add+1];

	for (i = 0 ; i < ChrEnableQuantity ; i++)
	{
		// Add the address of char in ASCII
		Add = (E_Layer[Layer].Text[i] - ' ' + E_Layer[Layer].StreamOffset) * ChrSize + SectorAddr_to_ByteAddr(BasicAdd);
		
		W25Qxx_Start();
		HardSPI_SwapBytes(W25Qxx_Cmd_ReadData);
		HardSPI_SwapBytes(Addr_1(Add));
		HardSPI_SwapBytes(Addr_2(Add));
		HardSPI_SwapBytes(Addr_3(Add));
		for (j = 0 ; j < E_Layer[Layer].Ysize ; j++)
		{
			for ( k = 0 ; k < E_Layer[Layer].Block_Xsize_Target ; k++)
			{
				E[j*PageBytes + i*E_Layer[Layer].Block_Xsize_Target + k] = HardSPI_SwapBytes(0xff);
			}
		}
		W25Qxx_Stop();
	}
}





static inline void LoadChr_GB2312(uint8_t Layer)
{
    uint8_t i, j, k, ChrEnableQuantity, Text_p;
    uint16_t ChrSize, PageBytes;
    uint32_t BasicAdd, Add;

    // E_Layer[Layer].Block_Xsize_Target: Why use Block_?
	// - Refer to the comments on the related parameters of the CmdLoad.
	ChrSize = E_Layer[Layer].Block_Xsize_Target * E_Layer[Layer].Ysize;
    ChrEnableQuantity = (ChrSize)? (E_Size / ChrSize) : 0;
	if ((E_Layer[Layer].Text_Length / 2) < ChrEnableQuantity)	{ChrEnableQuantity = E_Layer[Layer].Text_Length / 2;}
    PageBytes = ChrEnableQuantity * E_Layer[Layer].Block_Xsize_Target;
	while (PageBytes > 255)
	{
		PageBytes -= E_Layer[Layer].Block_Xsize_Target;
		ChrEnableQuantity--;
	}
	E_Layer[Layer].Xsize = PageBytes;

    // Get the basic sector address in flash
    BasicAdd = ((uint16_t)FlashIndex[E_Layer[Layer].Flash_Item*FlashIndex_Size + FlsIdx_p2_Add] << 8) + FlashIndex[E_Layer[Layer].Flash_Item*FlashIndex_Size + FlsIdx_p2_Add+1];

    for(i = 0 ; i < ChrEnableQuantity ; i++)
    {
		Text_p = i * 2;

        // Add the address of char in GB2312
        if ((E_Layer[Layer].Text[Text_p+1] >= 0xa0) && (E_Layer[Layer].Text[Text_p+1] <= 0xff))
        {
            // 0xa1a0 ~ 0xa9ff
            if ((E_Layer[Layer].Text[Text_p] >= 0xa1) && (E_Layer[Layer].Text[Text_p] <= 0xa9))
            {
                Add = ((E_Layer[Layer].Text[Text_p] - 0xa1)*(6*16) + (E_Layer[Layer].Text[Text_p+1] - 0xa0) + E_Layer[Layer].StreamOffset) * ChrSize + SectorAddr_to_ByteAddr(BasicAdd);
            }
            // 0xb0a0 ~ 0xf7ff
            else if ((E_Layer[Layer].Text[Text_p] >= 0xb0) && (E_Layer[Layer].Text[Text_p] <= 0xf7))
            {
                Add = ((E_Layer[Layer].Text[Text_p] - 0xb0)*(6*16) + (E_Layer[Layer].Text[Text_p+1] - 0xa0) + (9*6*16) + E_Layer[Layer].StreamOffset) * ChrSize + SectorAddr_to_ByteAddr(BasicAdd);
            }
			else	{return;}
        }
		else	{return;}

        W25Qxx_Start();
        HardSPI_SwapBytes(W25Qxx_Cmd_ReadData);
        HardSPI_SwapBytes(Addr_1(Add));
        HardSPI_SwapBytes(Addr_2(Add));
        HardSPI_SwapBytes(Addr_3(Add));
        for(j = 0 ; j < E_Layer[Layer].Ysize ; j++)
        {
            for( k = 0 ; k < E_Layer[Layer].Block_Xsize_Target ; k++)
            {
                E[j*PageBytes + i*E_Layer[Layer].Block_Xsize_Target + k] = HardSPI_SwapBytes(0xff);
            }
        }
        W25Qxx_Stop();
    }
}





static inline void LoadDrawBlock(uint8_t Layer)
{
	static const uint8_t R_Corners_Mask[12][8] =
	{
		{0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
		{0xfc, 0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
		{0xf0, 0xfc, 0xfe, 0xfe, 0xff, 0xff, 0xff, 0xff},
		{0xe0, 0xf8, 0xfc, 0xfe, 0xfe, 0xff, 0xff, 0xff},
		{0x80, 0xe0, 0xf8, 0xfc, 0xfc, 0xfe, 0xfe, 0xff},
		{0x00, 0xc0, 0xf0, 0xf8, 0xfc, 0xfc, 0xfe, 0xfe},

		{0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
		{0x3f, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
		{0x0f, 0x3f, 0x7f, 0x7f, 0xff, 0xff, 0xff, 0xff},
		{0x07, 0x1f, 0x3f, 0x7f, 0x7f, 0xff, 0xff, 0xff},
		{0x01, 0x07, 0x1f, 0x3f, 0x3f, 0x7f, 0x7f, 0xff},
		{0x00, 0x03, 0x0f, 0x1f, 0x3f, 0x3f, 0x7f, 0x7f}
	};
	static const uint8_t B_Corners_Mask[16][8] =
	{
		{0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
		{0xfc, 0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
		{0xf8, 0xfc, 0xfe, 0xff, 0xff, 0xff, 0xff, 0xff},
		{0xf0, 0xf8, 0xfc, 0xfe, 0xff, 0xff, 0xff, 0xff},
		{0xe0, 0xf0, 0xf8, 0xfc, 0xfe, 0xff, 0xff, 0xff},
		{0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe, 0xff, 0xff},
		{0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe, 0xff},
		{0x00, 0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe},

		{0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
		{0x3f, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
		{0x1f, 0x3f, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xff},
		{0x0f, 0x1f, 0x3f, 0x7f, 0xff, 0xff, 0xff, 0xff},
		{0x07, 0x0f, 0x1f, 0x3f, 0x7f, 0xff, 0xff, 0xff},
		{0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7f, 0xff, 0xff},
		{0x01, 0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7f, 0xff},
		{0x00, 0x01, 0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7f},
	};
	uint8_t i, j, MBy, MBy_o, CornerValue;

	// Get INFO of move bits
	MBy = E_Layer[Layer].Block_Ysize % 8;
	MBy_o = (MBy)? (8 - MBy) : 0;
	E_Layer[Layer].Ysize = (MBy)? (E_Layer[Layer].Block_Ysize / 8 + 1) : (E_Layer[Layer].Block_Ysize / 8);
	E_Layer[Layer].Xsize = E_Layer[Layer].Block_Xsize;

	if ((E_Layer[Layer].Ysize*E_Layer[Layer].Xsize) > E_Size)	{return;}

	// Fill the whole area of block
	for (i = 0 ; i < E_Layer[Layer].Ysize ; i++)
	{
		for (j = 0 ; j < E_Layer[Layer].Block_Xsize ; j++)
		{
			E[i*E_Layer[Layer].Block_Xsize + j] = 0xff;
		}
	}

	// Cut the bottom of block
	for (i = 0 ; i < E_Layer[Layer].Block_Xsize ; i++)
	{
		E[(E_Layer[Layer].Ysize - 1)*E_Layer[Layer].Block_Xsize + i] &= 0xff >> MBy_o;
	}

	// Operate corners
	CornerValue = BlockCorners_GetValue(E_Layer[Layer].Block_Corners);
	if(CornerValue == 0)	{return;}

	switch (BlockCorners_GetMode(E_Layer[Layer].Block_Corners))
	{
		case BlockCorners_BeveledMode:
		{
			for (i = 0 ; (i < E_Layer[Layer].Block_Xsize) && (i < B_Corners_Mask_LevelQuantity) ; i++)
			{
				// Operate the first page.
				E[i]									&= B_Corners_Mask[CornerValue - 1][i];
				E[E_Layer[Layer].Block_Xsize - 1 - i]	&= B_Corners_Mask[CornerValue - 1][i];

				// Operate the last page.
				E[(E_Layer[Layer].Ysize - 1)*E_Layer[Layer].Block_Xsize + i] &= B_Corners_Mask[CornerValue - 1 + B_Corners_Mask_LevelQuantity][i] >> MBy_o;
				E[E_Layer[Layer].Ysize*E_Layer[Layer].Block_Xsize - 1 - i] &= B_Corners_Mask[CornerValue - 1 + B_Corners_Mask_LevelQuantity][i] >> MBy_o;

				// If the hight of block takes moer than 1 paind of the last one should be operated.
				if((E_Layer[Layer].Ysize > 1) && MBy)
				{
					E[(E_Layer[Layer].Ysize - 2)*E_Layer[Layer].Block_Xsize + i] &= (B_Corners_Mask[CornerValue - 1 + B_Corners_Mask_LevelQuantity][i] << MBy) | (0xff >> MBy_o);
					E[(E_Layer[Layer].Ysize - 1)*E_Layer[Layer].Block_Xsize - 1 - i] &= (B_Corners_Mask[CornerValue - 1 + B_Corners_Mask_LevelQuantity][i] << MBy) | (0xff >> MBy_o);
				}
			}
		} break;

		case BlockCorners_RoundedMode:
		{
			for(i = 0 ; (i < E_Layer[Layer].Block_Xsize) && (i < R_Corners_Mask_LevelQuantity) ; i++)
			{
				// Operate the first page.
				E[i]									&= R_Corners_Mask[CornerValue - 1][i];
				E[E_Layer[Layer].Block_Xsize - i - 1]	&= R_Corners_Mask[CornerValue - 1][i];

				// Operate the last page.
				E[(E_Layer[Layer].Ysize - 1)*E_Layer[Layer].Block_Xsize + i] &= R_Corners_Mask[CornerValue - 1 + R_Corners_Mask_LevelQuantity][i] >> MBy_o;
				E[E_Layer[Layer].Ysize*E_Layer[Layer].Block_Xsize - i - 1] &= R_Corners_Mask[CornerValue - 1 + R_Corners_Mask_LevelQuantity][i] >> MBy_o;

				// If the hight of block takes moer than 1 page, the page behind of the last one should be operated.
				if((E_Layer[Layer].Ysize > 1) && MBy)
				{
					E[(E_Layer[Layer].Ysize - 2)*E_Layer[Layer].Block_Xsize + i] &= (R_Corners_Mask[CornerValue - 1 + 6][i] << MBy) | (0xff >> MBy_o);
					E[(E_Layer[Layer].Ysize - 1)*E_Layer[Layer].Block_Xsize - i - 1] &= (R_Corners_Mask[CornerValue - 1 + 6][i] << MBy) | (0xff >> MBy_o);
				}
			}
		} break;
	}
}




// Load the original materials to E
