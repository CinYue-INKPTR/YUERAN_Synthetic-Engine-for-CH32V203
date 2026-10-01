#include <stdint.h>
#include "YUERAN_LoadE.h"
#include "YUERAN_Define.h"

#define FlashReadCmd	0x30

#define FontAdd_Chr		0
#define FontAdd_Add		1

// 字符集起始字符编码，flash字模起始地址
static const uint32_t FontAdd[][2] =
{
	{' ', 0},
};



static inline void LoadPicture(uint8_t Layer)
{
	
}



static inline void LoadChr_ASCII(uint8_t Layer)
{
	uint8_t i, j, k, ChrSize, ChrCount;
	uint32_t ASCII_Add;
	// 特性：仅支持一个元素一行字符
	// 注意：在EtoFRAME中，页与页是对应的，因此加载字模时要按顺序每个字的第1页，然后每个字的第2页……，不能一个字的完整字模紧邻排序
	// 要先计算可容纳的字数，如果溢出E的存储空间就舍弃多余的字符
	// 为保证字体的自由度，同时确保不同字体映射显存的对齐，应该把字模加载函数单独开放出来
	// 用到的资源：
	// Block_Xsize Ysize Text_Font Text[] Text_Length
	ChrSize = E_Layer[Layer].Block_Xsize * E_Layer[Layer].Ysize;
	ChrCount = E_Size / ChrSize;
	ChrCount = (ChrCount < E_Layer[Layer].Text_Length)? ChrCount : E_Layer[Layer].Text_Length;

	// 1 2 3 4   1 2 3 4
	for(i = 0 ; i < ChrCount ; i++)
	{
//		INKPTR_SPI_Start_Flash();
//		INKPTR_SPI_SwapByte(FlashReadCmd);
		ASCII_Add =
			(E_Layer[Layer].Text[i] - FontAdd[E_Layer[Layer].Text_Font][FontAdd_Chr])*ChrSize +
			FontAdd[E_Layer[Layer].Text_Font][FontAdd_Add];
//		INKPTR_SPI_SwapByte((uint8_t)(ASCII_Add >> 16));
//		INKPTR_SPI_SwapByte((uint8_t)(ASCII_Add >> 8));
//		INKPTR_SPI_SwapByte((uint8_t)ASCII_Add);
		for(j = 0 ; j < E_Layer[Layer].Ysize ; j++)
		{
			for( k = 0 ; k < E_Layer[Layer].Block_Xsize ; k++)
			{
//				E[j*ChrCount*E_Layer[Layer].Block_Xsize + i*E_Layer[Layer].Block_Xsize + k] = INKPTR_SPI_SwapByte(0xff);
			}
		}
//		INKPTR_SPI_Stop_Flash();
	}
}



static inline void LoadChr_GB2312(uint8_t Layer)
{
	
}


static inline void LoadDrawBlock(uint8_t Layer)
{
	uint8_t i, j, MBy, CornersMode, temp;


	// 计算矩形下边界的偏移值
	MBy = E_Layer[Layer].Block_Ysize % 8;
	E_Layer[Layer].Ysize = (MBy)? (E_Layer[Layer].Block_Ysize / 8 + 1) : (E_Layer[Layer].Block_Ysize / 8);
	E_Layer[Layer].Xsize = E_Layer[Layer].Block_Xsize;
	temp = (MBy)? (8- MBy) : 0;

	// 填充整个矩形区域
	for(i = 0 ; i < E_Layer[Layer].Ysize ; i++)
	{
		for(j = 0 ; j < E_Layer[Layer].Block_Xsize ; j++)
		{
			E[i*E_Layer[Layer].Block_Xsize + j] = 0xff;
		}
	}

	// 根据实际偏移值裁切矩形下边界得到原始矩形
	for(i = 0 ; i < E_Layer[Layer].Block_Xsize ; i++)
	{
		E[(E_Layer[Layer].Ysize - 1)*E_Layer[Layer].Block_Xsize + i] &= 0xff >> temp;
	}

	// 根据模式修剪矩形圆角/切角
	CornersMode = E_Layer[Layer].Block_Corners & 0x7f;
	if(CornersMode)
	{
		if(E_Layer[Layer].Block_Corners & 0x80)
		{
			// 矩形切角
			for(i = 0 ; (i < E_Layer[Layer].Block_Xsize) && (i < 8) ; i++)
			{
				// 修剪矩形元素第一页的两个顶角
				E[i]							&= B_Corners_Mask[CornersMode - 1][i];
				E[E_Layer[Layer].Block_Xsize - i - 1]	&= B_Corners_Mask[CornersMode - 1][i];

				// 修剪矩形元素最后一页的两个底角
				E[(E_Layer[Layer].Ysize - 1)*E_Layer[Layer].Block_Xsize + i] &=
					B_Corners_Mask[CornersMode - 1 + 8][i] >> temp;
				E[E_Layer[Layer].Ysize*E_Layer[Layer].Block_Xsize - i - 1] &=
					B_Corners_Mask[CornersMode - 1 + 8][i] >> temp;

				// 如果矩形不止 1 页高，最后一页的前一页也要裁切角
				if((E_Layer[Layer].Ysize > 1) && MBy)
				{
					E[(E_Layer[Layer].Ysize - 2)*E_Layer[Layer].Block_Xsize + i] &=
						(B_Corners_Mask[CornersMode - 1 + 8][i] << MBy) | (0xff >> temp);
					E[(E_Layer[Layer].Ysize - 1)*E_Layer[Layer].Block_Xsize - i - 1] &=
						(B_Corners_Mask[CornersMode - 1 + 8][i] << MBy) | (0xff >> temp);
				}
			}
		}
		else
		{
			// 矩形圆角
			for(i = 0 ; (i < E_Layer[Layer].Block_Xsize) && (i < 8) ; i++)
			{
				// 修剪矩形元素第一页的两个顶角
				E[i]							&= R_Corners_Mask[CornersMode - 1][i];
				E[E_Layer[Layer].Block_Xsize - i - 1]	&= R_Corners_Mask[CornersMode - 1][i];

				// 修剪矩形元素最后一页的两个底角
				E[(E_Layer[Layer].Ysize - 1)*E_Layer[Layer].Block_Xsize + i] &=
					R_Corners_Mask[CornersMode - 1 + 6][i] >> temp;
				E[E_Layer[Layer].Ysize*E_Layer[Layer].Block_Xsize - i - 1] &=
					R_Corners_Mask[CornersMode - 1 + 6][i] >> temp;

				// 如果矩形不止 1 页高，最后一页的前一页也要裁切角
				if((E_Layer[Layer].Ysize > 1) && MBy)
				{
					E[(E_Layer[Layer].Ysize - 2)*E_Layer[Layer].Block_Xsize + i] &=
						(R_Corners_Mask[CornersMode - 1 + 6][i] << MBy) | (0xff >> temp);
					E[(E_Layer[Layer].Ysize - 1)*E_Layer[Layer].Block_Xsize - i - 1] &=
						(R_Corners_Mask[CornersMode - 1 + 6][i] << MBy) | (0xff >> temp);
				}
			}
		}
	}
}




// Load the original materials to E
void LoadE(uint8_t Layer)
{
	switch(E_Layer[Layer].Type)
	{
		case Type_Picture:		{LoadPicture(Layer);}	break;
		case Type_Char_ASCII:	{LoadChr_ASCII(Layer);}	break;
		case Type_Char_GB2312:	{LoadChr_GB2312(Layer);}break;
		case Type_DrawBlock:	{LoadDrawBlock(Layer);}	break;
	}
}
