#include "ch32v00x.h"
#include "INKPTR_SoftIIC.h"
#include "INKPTR_OLEDiic.h"
#include "Font.h"

#define ButtonLight_Port    GPIOA
#define Button_Pin          GPIO_Pin_1
#define Light_Pin           GPIO_Pin_2



// Ԫ������
#define Type_Picture        0
#define Type_Char           1
#define Type_DrawBlock      2
// Alpha ģʽ
#define Alpha_Mode0         0
#define Alpha_Mode1         (1 << 5)
#define Alpha_Mode2         (2 << 5)
// �ı���󳤶�
#define Text_MaxLength      32
// ��Ⱦ��Դ����
#define E_Layer_Size        3
#define FRAME_PageSize      4
#define FRAME_ListSize      128
#define E_Size              256





typedef struct
{
    // ��ǰԪ������
    uint8_t Type;


    // ���εĵ�ǰ X �ߴ磬��ͼƬ�ַ��� X �ߴ�
    uint8_t SizeX;
    // �����ھ��Ρ������ڲ�����¼���ε�Ŀ�� X �ߴ�
    uint8_t SizeX_Target;
    // �����ھ��Σ� X �ߴ�Ĺ��ɺ����� D7~6 ģʽѡ�� D5~0 ������
    uint8_t SizeX_Fcn;
    // ���εĵ�ǰ Y �ߴ磬��ͼƬ�ַ��� Y �ߴ�
    uint8_t SizeY;
    // �����ھ��Ρ������ڲ�����¼���ε�Ŀ�� Y �ߴ�
    uint8_t SizeY_Target;
    // �����ھ��Σ� Y �ߴ�Ĺ��ɺ����� D7~6 ģʽѡ�� D5~0 ������
    uint8_t SizeY_Fcn;

    // ���ģʽ
    uint8_t MixMode;

    // ��ǰ֡��Ԫ�ص� X λ�ã�-128~127��
    int8_t Xpos;
    // Ԫ�ص�Ŀ�� X λ�ã�-128~127��
    int8_t Xpos_Target;
    // Ԫ���� X λ���ϵĹ��Ⱥ�����D7~6 Ϊ����ģʽ�� D5~0 �ٶȣ�
    uint8_t Xpos_Fcn;

    // ��ǰ֡��Ԫ�ص� Y λ�ã�-64~63��
    int8_t Ypos;
    // Ԫ�ص�Ŀ�� Y λ�ã�-64~63��
    int8_t Ypos_Target;
    // Ԫ���� Y λ���ϵĹ��Ⱥ�����D7~6 Ϊ����ģʽ�� D5~0 �ٶȣ�
    uint8_t Ypos_Fcn;

    // ��ǰ֡��Ԫ�ص� Alpha ��0~10 �� 0~8��
    uint8_t Alpha;
    // Ԫ�ص�Ŀ�� Alpha ��0~10 �� 0~8��
    uint8_t Alpha_Target;
    // Ԫ�� Alpha �Ĺ��Ⱥ�����D7~5 Ϊ͸����ģʽ�� D4 Ϊ����/#ֱ� D3~0 Ϊ�ٶȣ�
    uint8_t Alpha_Fcn;



    // ������ͼƬ�� Flash ��ַ
    uint32_t FlashAdd;

    // ���������֣�����
    uint8_t Font;
    // ���������֣��ı�����
    uint8_t Text[Text_MaxLength];
    // ���������֣��ı�����
    uint8_t TextLength;
    // �����ھ��Σ�����Բ��
    uint8_t RoundCorners;

    // �����ȣ�Ϊ 0 ʱ����������
    uint16_t Stream_Length;
    // D15 ������ D14 ��ѭ��ʹ�ܡ� D13~7 ������ֵ�� D6~0 ���ٻ�ֵ
    uint16_t Stream_Attributes;
} Element;
Element E_Layer[E_Layer_Size];
uint8_t FRAME[FRAME_PageSize][FRAME_ListSize];
uint8_t E[E_Size];




// ����Ԫ��ͼ�㣬����Ԫ�ص� E
void LoadE(uint8_t Layer)
{
    uint8_t i, j, k, temp;

    if(E_Layer[Layer].Type == Type_Picture)
    {
        return;
    }


    if(E_Layer[Layer].Type == Type_Char)
    {
        // �����ı��������ÿһ���ַ�
        for(i = 0 ; i < E_Layer[Layer].TextLength ; i++)
        {
            if(i < (E_Size / E_Layer[Layer].SizeY / E_Layer[Layer].SizeX))
            {
                // �����ַ���ռ��ÿһҳ
                for(j = 0 ; j < E_Layer[Layer].SizeY ; j++)
                {
                    // �����ַ�ҳ���ֵ�ÿһ��
                    for(k = 0 ; k < E_Layer[Layer].SizeX ; k++)
                    {
                        E[i*E_Layer[Layer].SizeX*E_Layer[Layer].SizeY + j*E_Layer[Layer].SizeX + k] = Font[(E_Layer[Layer].Text[i] - ' ')*6 + k];
                    }
                }
            }
        }
        return;
    }


    if(E_Layer[Layer].Type == Type_DrawBlock)
    {
        for(i = 0 ; i < E_Layer[Layer].SizeY ; i++)
        {
            for(j = 0 ; j < E_Layer[Layer].SizeX ; j++)
            {
                temp = i*E_Layer[Layer].SizeX + j;
                if(temp < E_Size)
                {
                    E[temp] = 0xaf;
                }
            }
        }
        return;
    }
}



// �� E ���� Alpha ����
void AlphaE(uint8_t Layer)
{
    uint16_t i, E_DrawLength;
    E_DrawLength = E_Layer[Layer].SizeX * E_Layer[Layer].SizeY;

    // ȡ Alpha ģʽ�����ദ��
    switch(E_Layer[Layer].Alpha_Fcn >> 5)
    {
        case 0:
        {
            // ת�� Alpha ֵ���� E ��������
            switch(E_Layer[Layer].Alpha)
            {
                case 0:
                {
                    for(i = 0 ; i < E_DrawLength ; i++)
                    {
                        // ���￪ʼʵ������
                        E[i] = 0;
                    }
                } break;


                case 1:
                {
                    for(i = 0 ; i < E_DrawLength ; i++)
                    {
                        // ���￪ʼʵ������
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
                        // ���￪ʼʵ������
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
                        // ���￪ʼʵ������
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
                        // ���￪ʼʵ������
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
                        // ���￪ʼʵ������
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
                        // ���￪ʼʵ������
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
                        // ���￪ʼʵ������
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
                        // ���￪ʼʵ������
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
                        // ���￪ʼʵ������
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
        } break;



        case 1:
        {
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



//            case 2:
//            {
//
//            }
        } break;
    }
}



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

    // �������ҳ��ҳƫ��ֵ
    MBy = E_Layer[Layer].Ypos % 8;
    if(MBy < 0)  {MBy += 8;}

    MixMode = E_Layer[Layer].MixMode;


	for(iY = 0 ; iY < E_Layer[Layer].SizeY ; iY++)
	{
		Sy = iY*8 + E_Layer[Layer].Ypos;
		if((Sy <= -8) || (Sy >= (FRAME_PageSize*8))){continue;}

		for(iX = 0 ; iX < E_Layer[Layer].SizeX ; iX++)
		{
			Sx = iX + E_Layer[Layer].Xpos;
			if((Sx < 0) || (Sx >= FRAME_ListSize))  {continue;}

			temp = E[iY*E_Layer[Layer].SizeX + iX];
			
            // ��� Sy �� -7~-1
            if(Sy < 0)
            {
                // ����� E ���һ����Ⱦҳ
                if((iY+1) == E_Layer[Layer].SizeY)
                {
                    FRAME[0][Sx] &= (MixMode == 0)?
                            (0xff << MBy) :
                        0xff;
                    EmixFRAME(MixMode, &FRAME[0][Sx], (MixMode == 2)?
                            ((temp >> (-Sy)) | (0xff << MBy)) :
                        (temp >> (-Sy))
                        );
                }
                // ��������һ����Ⱦҳ��ô����Ĳ��ÿ��ǣ��������������Ҳ����������һ�� Sy+8 �Ժ����ϳ�
                continue;
            }

            // ���� Sy �� 0~FRAME_PageSize*8-1������ 0~63��
            // ���Ŀǰ��Ⱦ E ����ҳ��ҳ��ץ��һҳ�ͱ�ҳ�ĺϳ�
            if(iY)
            {
                FRAME[Sy >> 3][Sx] &= (MixMode == 0)?
                        0 :
                    0xff;
                EmixFRAME(MixMode, &FRAME[Sy >> 3][Sx],
                    (E[(iY-1)*E_Layer[Layer].SizeX + iX] >> (8-MBy)) |
                    (temp << MBy)
                    );
            }
            // ����� E ����ҳ��ֻ�ñ�ҳ�ĺϳ�
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


            // �����ǰ�� E ���һ����Ⱦҳ������β��
            if(((iY+1) == E_Layer[Layer].SizeY) && (((Sy >> 3)+1) < FRAME_PageSize))
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











//    switch(E_Layer[Layer].MixMode)
//    {
//        case 0:
//        {
//        } break;
//
//        case 1:
//		{
//            // ��ҳ���� E
//            for(iY = 0 ; iY < E_Layer[Layer].SizeY ; iY++)
//            {
//                // ���㵱ǰ E ��Ⱦλ�õ�ҳԭ������ Sy
//                Sy = E_Layer[Layer].Ypos + (iY*8);
//
//				for(iX = 0 ; iX < E_Layer[Layer].SizeX ; iX++)
//				{
//					Sx = E_Layer[Layer].Xpos + iX;
//
//					if((Sx >= 0) && (Sx < (FRAME_ListSize*8)))
//					{
//						if((Sy > -8) && (Sy < 0))
//						{
//							temp |=
//						}
//					}
//				}
//
//
//
//
//
//                // �����ǰ Sy ����ʾ����
//                if((Sy >= 0) && (Sy < (FRAME_PageSize*8)))
//                {
//                    // ���б��� E �ĵ�ǰҳ
//                    for(iX = 0 ; iX < E_Layer[Layer].SizeX ; iX++)
//                    {
//                        // ���㵱ǰ E ��Ⱦλ�õ���ԭ������ Sx
//                        Sx = E_Layer[Layer].Xpos + iX;
//                        // �����ǰ Sx ����ʾ����
//                        if((Sx >= 0) && (Sx < FRAME_ListSize))
//                        {
//							// Ԫ�صڶ�ҳ��
//                            if(iY)
//                            {
//                                FRAME[Sy/8][Sx] |=
//                                        (E[(iY-1)*E_Layer[Layer].SizeX + iX] >> (8-MBy)) |
//                                        (E[iY*E_Layer[Layer].SizeX + iX] << MBy);
//                            }
//                            else
//                            {
//                                FRAME[Sy/8][Sx] |= E[iX] << MBy;
//                            }
//                            if(((iY+1) == E_Layer[Layer].SizeY) && ((Sy/8+1) < FRAME_PageSize))
//                            {
//                                FRAME[Sy/8+1][Sx] |= E[iY*E_Layer[Layer].SizeX + iX] >> (8-MBy);
//                            }
//
//                        }
//                    }
//                }
//                else
//                {
//                    if((Sy > -8) && (Sy < 0) && ((iY+1) == E_Layer[Layer].SizeY))
//                    {
//                        for(iX = 0 ; iX < E_Layer[Layer].SizeX ; iX++)
//                        {
//                            Sx = E_Layer[Layer].Xpos + iX;
//                            if((Sx >= 0) && (Sx < FRAME_ListSize))
//                            {
//                                FRAME[0][Sx] |= E[iY * E_Layer[Layer].SizeX + iX] >> (-Sy);
//                            }
//                        }
//                    }
//                }
//            }
//        } break;
//
//        case 2:
//        {
//            for(iY = 0 ; iY < E_Layer[Layer].SizeY ; iY++)
//            {
//                Sy = E_Layer[Layer].Ypos + (iY << 3); // �˴���λΪ�˳���
//                for(iX = 0 ; iX < E_Layer[Layer].SizeX ; iX++)
//                {
//                    Sx = E_Layer[Layer].Xpos + iX;
//                    if(Sx >= 0)
//                    {
//                        Utmp = E[iY * 8 + iX] << MBy;
//                        Dtmp = E[iY * 8 + iX] >> (8-MBy);
//
//                        if((Sy >= 0) && (Sy < 64))
//                        {
//                            FRAME[Sy>>3][Sx] &= Utmp; // �˴���λΪ�˳���
//                        }
//                        if(((Sy+8)>0) && (Sy<56))
//                        {
//                            FRAME[(Sy + 8)>>3][Sx] &= Dtmp; // �˴���λΪ�˳���
//                        }
//                    }
//                }
//            }
//        } break;
//
//        case 3:
//        {
//            for(iY = 0 ; iY < E_Layer[Layer].SizeY ; iY++)
//            {
//                Sy = E_Layer[Layer].Ypos + (iY << 3); // �˴���λΪ�˳���
//                for(iX = 0 ; iX < E_Layer[Layer].SizeX ; iX++)
//                {
//                    Sx = E_Layer[Layer].Xpos + iX;
//                    if((Sx+1) > 0)
//                    {
//                        Utmp = E[iY * 8 + iX] << MBy;
//                        Dtmp = E[iY * 8 + iX] >> (8-MBy);
//
//                        if(((Sy+1) > 0) && (Sy < 64))
//                        {
//                            FRAME[Sy>>3][Sx] &= ~Utmp; // �˴���λΪ�˳���
//                        }
//                        if(((Sy+8) > 0) && (Sy < 56))
//                        {
//                            FRAME[(Sy + 8)>>3][Sx] &= ~Dtmp; // �˴���λΪ�˳���
//                        }
//                    }
//                }
//            }
//        } break;
//
//        case 4:
//        {
//            for(iY = 0 ; iY < E_Layer[Layer].SizeY ; iY++)
//            {
//                Sy = E_Layer[Layer].Ypos + (iY << 3); // �˴���λΪ�˳���
//                for(iX = 0 ; iX < E_Layer[Layer].SizeX ; iX++)
//                {
//                    Sx = E_Layer[Layer].Xpos + iX;
//                    if((Sx+1) > 0)
//                    {
//                        Utmp = E[iY * 8 + iX] << MBy;
//                        Dtmp = E[iY * 8 + iX] >> (8-MBy);
//
//                        if(((Sy+1) > 0) && (Sy < 64))
//                        {
//                            FRAME[Sy>>3][Sx] = (FRAME[Sy>>3][Sx] | Utmp) - (FRAME[Sy>>3][Sx] & Utmp); // �˴���λΪ�˳���
//                        }
//                        if(((Sy+8) > 0) && (Sy < 56))
//                        {
//                            FRAME[(Sy + 8)>>3][Sx] = (FRAME[(Sy+8)>>3][Sx] | Dtmp) - (FRAME[(Sy+8)>>3][Sx] & Dtmp); // �˴���λΪ�˳���
//                        }
//                    }
//                }
//            }
//        } break;
//    }
}




// ���͡����֡
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

    for(i = 0 ; i < FRAME_PageSize ; i++)
    {
        for(j = 0 ; j < FRAME_ListSize ; j++)
        {
            if(ClearEN) {FRAME[i][j] = 0;}
        }
    }
}




void MainInit(void)
{
    uint8_t i;
    uint8_t Str[] = "ABC";

    SystemCoreClockUpdate();
    Delay_Init();

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
    GPIO_PinRemapConfig(GPIO_Remap_PA1_2, DISABLE);
    RCC_HSEConfig(RCC_HSE_OFF);

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    // Button
    GPIO_InitStructure.GPIO_Pin     = Button_Pin;
    GPIO_InitStructure.GPIO_Mode    = GPIO_Mode_IN_FLOATING;
    GPIO_InitStructure.GPIO_Speed   = GPIO_Speed_2MHz;
    GPIO_Init(ButtonLight_Port, &GPIO_InitStructure);
    // Light
    GPIO_InitStructure.GPIO_Pin     = Light_Pin;
    GPIO_InitStructure.GPIO_Mode    = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed   = GPIO_Speed_2MHz;
    GPIO_Init(ButtonLight_Port, &GPIO_InitStructure);

    SoftIIC_Init();
    OLED_Init(OLED_AddressingMode_HorizontalMode, OLED_SetMode_X_Flip_Normal, OLED_SetMode_Y_Flip_Normal, OLED_SetMode_Color_Normal, 255);



    E_Layer[0].Type = Type_Char;
    E_Layer[0].SizeX = 6;
    E_Layer[0].SizeY = 1;
    E_Layer[0].MixMode = 0;
    E_Layer[0].Xpos = 0;
    E_Layer[0].Ypos = 0;
    E_Layer[0].Alpha = 10;
    E_Layer[0].Alpha_Fcn = Alpha_Mode0;
    E_Layer[0].TextLength = 3;
    for(i = 0 ; i < E_Layer[0].TextLength ; i++)
    {
        E_Layer[0].Text[i] = Str[i];
    }

    E_Layer[1].Type = Type_DrawBlock;
    E_Layer[1].SizeX = 6;
    E_Layer[1].SizeY = 3;
    E_Layer[1].MixMode = 0;
    E_Layer[1].Xpos = 2;
    E_Layer[1].Ypos = 3;
    E_Layer[1].Alpha = 10;
    E_Layer[1].Alpha_Fcn = Alpha_Mode0;

}












int main(void)
{
    uint8_t i = 10, n, m = 100, l = 1;
    int8_t j = 127, k = 30;
    MainInit();


    while(1)
    {
        if(GPIO_ReadInputDataBit(ButtonLight_Port, Button_Pin) == RESET)
        {
            Delay_Ms(10);
            E_Layer[1].MixMode = (E_Layer[1].MixMode == 4)? 0 : E_Layer[1].MixMode+1;
            while(GPIO_ReadInputDataBit(ButtonLight_Port, Button_Pin) == RESET);
        }

        FRAME[0][0] = 0xaf;
        for(n = 0 ; n < 128 ; n++)
        {
            FRAME[1][n] = 0xff;
        }

        FRAME[0][16] = Font[(E_Layer[1].MixMode+0x30-0x20)*6 + 0];
        FRAME[0][17] = Font[(E_Layer[1].MixMode+0x30-0x20)*6 + 1];
        FRAME[0][18] = Font[(E_Layer[1].MixMode+0x30-0x20)*6 + 2];
        FRAME[0][19] = Font[(E_Layer[1].MixMode+0x30-0x20)*6 + 3];
        FRAME[0][20] = Font[(E_Layer[1].MixMode+0x30-0x20)*6 + 4];
        FRAME[0][21] = Font[(E_Layer[1].MixMode+0x30-0x20)*6 + 5];

        FRAME[3][0] = 0xaf;



        LoadE(1);
        AlphaE(1);
        EtoFRAME(1);
        E_Layer[1].Alpha = i--;
        E_Layer[1].Xpos = j;
        E_Layer[1].Ypos = k;
        RefrashFRAME(l);

        if(i == 0) i = 10;
        if(j > -5) j--;
        else j = 127;
        if(k > -23) k--;
        else k = 31;
        Delay_Ms(100);
        if(!m) {m=100; l = !l;}
        else {m--;}
//        for(i = 0 ; i < 3 ; i++)
//        {
//            for(j = 0 ; j < 6 ; j++)
//            {
//                FRAME[0][i*6 + j] = Font[(Str[i]-' ')*6 + j];
//            }
//        }
//        RefrashFRAME(1);

    }
}
