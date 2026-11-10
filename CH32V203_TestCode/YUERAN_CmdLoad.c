#include "YUERAN_CmdLoad.h"

#include "ch32v20x.h"
#include "YUERAN_Define.h"
#include "OLEDspi.h"



// ======================== CmdFlag
#define GetCmdFlag(Dat)     ((Dat) & 0xf0)
#define ElementOperation_F  0x00
#define LayerBaseValue_F    0x10
#define StreamSet_F         0x20
#define StreamValue_F       0x30
#define StreamPause_F       0x40
#define BlockValue_F        0x50
#define DisableAllLayer_F   0xa0
#define FrameClear_F        0xc0
#define ScreenSet_F         0xe0
#define GetCmdLayer(Dat)    ((Dat) & 0x0f)
#define GetCmdValue(Dat)    ((Dat) & 0x0f)



// ======================== ElementOperation
#define ElmOpr_Gp1_Layer    0
#define ElmOpr_Gp1_Type	    1

#define ElmOpr_p1_Dis_Check	2

#define ElmOpr_p1_Pcr_Piplin    2
#define ElmOpr_p1_Pcr_FlsIdx    3
#define ElmOpr_p1_Pcr_MixMod	4
#define ElmOpr_p1_Pcr_Xpos	    5
#define ElmOpr_p1_Pcr_XposFc    6
#define ElmOpr_p1_Pcr_Ypos	    7
#define ElmOpr_p1_Pcr_YposFc    8
#define ElmOpr_p1_Pcr_Alpha	    9
#define ElmOpr_p1_Pcr_AphaFc	10
#define ElmOpr_p2_Pcr_PcrAdd    11
#define ElmOpr_p1_Pcr_Check	    13

#define ElmOpr_p1_Chr_Piplin    2
#define ElmOpr_p1_Chr_FlsIdx	3
#define ElmOpr_p1_Chr_MixMod	4
#define ElmOpr_p1_Chr_Xpos	    5
#define ElmOpr_p1_Chr_XposFc    6
#define ElmOpr_p1_Chr_Ypos	    7
#define ElmOpr_p1_Chr_YposFc    8
#define ElmOpr_p1_Chr_Alpha	    9
#define ElmOpr_p1_Chr_AphaFc	10
#define ElmOpr_p1_Chr_TxtLen	11
#define ElmOpr_p1_Chr_Check	    12
#define ElmOpr_px_Chr_Contnt	13

#define ElmOpr_p1_Blk_Piplin    2
#define ElmOpr_p1_Blk_MixMod	3
#define ElmOpr_p1_Blk_Xpos	    4
#define ElmOpr_p1_Blk_XposFc    5
#define ElmOpr_p1_Blk_Ypos	    6
#define ElmOpr_p1_Blk_YposFc    7
#define ElmOpr_p1_Blk_Alpha	    8
#define ElmOpr_p1_Blk_AphaFc	9
#define ElmOpr_p1_Blk_BlkXsz	10
#define ElmOpr_p1_Blk_BlkYsz	11
#define ElmOpr_p1_Blk_BlkCnr	12
#define ElmOpr_p1_Blk_Check	    13



// ======================== LayerBaseValue
#define LyBsVu_p1_Layer		    0
#define LyBsVu_p1_Pipeline		1
#define LyBsVu_p1_XposTarget	2
#define LyBsVu_p1_XposFcn	    3
#define LyBsVu_p1_YposTarget	4
#define LyBsVu_p1_YposFcn	    5
#define LyBsVu_p1_AphaTarget	6
#define LyBsVu_p1_AphaFcn	    7
#define LyBsVu_p1_Check		    8



// ======================== StreamSet
#define SrmSet_p1_Layer		    0
#define SrmSet_p2_Add	        1
#define SrmSet_p1_Fcn	        3
#define SrmSet_p2_Atb           4
#define SrmSet_p1_Check	        6



// ======================== StreamValue
#define SrmVlu_p1_Layer         0
#define SrmVlu_p3_Add           1
#define SrmVlu_p1_Fcn           4
#define SrmVlu_p1_Check         5



// ======================== StreamPause
#define SrmPus_p1_Layer		0
#define SrmPus_p1_PusEn		1
#define SrmPus_p1_Check		2



// ======================== BlockValue
#define BlkVlu_p1_Layer		0
#define BlkVlu_p1_XszTarget	1
#define BlkVlu_p1_XszFcn	2
#define BlkVlu_p1_YszTarget	3
#define BlkVlu_p1_YszFcn	4
#define BlkVlu_p1_Check		5



// ======================== ScreenSet
#define ScnSet_ShowF        0x08
#define ScnSet_XflipF       0x04
#define ScnSet_YflipF       0x02
#define ScnSet_ColorF       0x01





static inline uint8_t ElementOperation(uint8_t* ReadPoint);
static inline uint8_t LayerBaseValue(uint8_t* ReadPoint);
static inline uint8_t StreamSet(uint8_t* ReadPoint);
static inline uint8_t StreamValue(uint8_t* ReadPoint);
static inline uint8_t StreamPause(uint8_t* ReadPoint);
static inline uint8_t BlockValue(uint8_t* ReadPoint);
static inline uint8_t DisableAllLayer(uint8_t* ReadPoint);
static inline uint8_t FrameClear(uint8_t* ReadPoint);
static inline uint8_t ScreenSet(uint8_t* ReadPoint);
void RenderCmdLoad(void)
{
    uint8_t CmdCount, ReadPoint = USART_BUFFER_CmdData_p, i, LoadCheck;

    CmdCount = USART_BUFFER[USART_BUFFER_CmdNum_p];
    if (CmdCount == 0)
    {
        USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
        return;
    }

    for (i = 0 ; i < CmdCount ; i++)
    {
        switch (GetCmdFlag(USART_BUFFER[ReadPoint]))
        {
            case ElementOperation_F:    {LoadCheck = ElementOperation(&ReadPoint);} break;
            case LayerBaseValue_F:      {LoadCheck = LayerBaseValue(&ReadPoint);} break;
            case StreamSet_F:           {LoadCheck = StreamSet(&ReadPoint);} break;
            case StreamValue_F:         {LoadCheck = StreamValue(&ReadPoint);} break;
            case StreamPause_F:         {LoadCheck = StreamPause(&ReadPoint);} break;
            case BlockValue_F:          {LoadCheck = BlockValue(&ReadPoint);} break;
            case DisableAllLayer_F:     {LoadCheck = DisableAllLayer(&ReadPoint);} break;
            case FrameClear_F:          {LoadCheck = FrameClear(&ReadPoint);} break;
            case ScreenSet_F:           {LoadCheck = ScreenSet(&ReadPoint);} break;
            default:                    {LoadCheck = 1;}
        }
        if (LoadCheck)  {break;}
    }
    if (LoadCheck)  {BackData_Head = 0xf0;}
    else            {BackData_Head = 0x00;}

    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
}





static inline uint8_t CmdChecker(uint8_t ReadPoint, uint8_t CheckByte_p)
{
    uint8_t i, Check = 0;

    for (i = 0 ; i < CheckByte_p ; i++) {Check += USART_BUFFER[ReadPoint + i];}

    if(Check == USART_BUFFER[ReadPoint + CheckByte_p])  {return 0;}
    else                                                {return 1;}
}





static inline uint8_t ElementOperation(uint8_t* ReadPoint)
{
	uint8_t Layer, i, ItemTemp;
	
	Layer = GetCmdLayer(USART_BUFFER[*ReadPoint + ElmOpr_Gp1_Layer]);
	if (Layer >= E_Layer_Size)  {return 1;}

	// Char
    if ((USART_BUFFER[*ReadPoint + ElmOpr_Gp1_Type] == Type_ASCIIchr) || (USART_BUFFER[*ReadPoint + ElmOpr_Gp1_Type] == Type_GB2312chr))
    {
        if (USART_BUFFER_Size <= (*ReadPoint + ElmOpr_p1_Chr_Check + USART_BUFFER[*ReadPoint + ElmOpr_p1_Chr_TxtLen]))  {return 1;}
        if (CmdChecker(*ReadPoint, ElmOpr_p1_Chr_Check) ||
          (USART_BUFFER[*ReadPoint + ElmOpr_p1_Chr_TxtLen] > Text_ByteLength))                                          {return 1;}

        ItemTemp = USART_BUFFER[*ReadPoint + ElmOpr_p1_Chr_FlsIdx];
        if (ItemTemp >= FlashIndex_CNT)  {return 1;}

        E_Layer[Layer].Type         = USART_BUFFER[*ReadPoint + ElmOpr_Gp1_Type];
        E_Layer[Layer].PipelineFlag = USART_BUFFER[*ReadPoint + ElmOpr_p1_Chr_Piplin];
        E_Layer[Layer].Flash_Item   = USART_BUFFER[*ReadPoint + ElmOpr_p1_Chr_FlsIdx];
        E_Layer[Layer].MixMode      = USART_BUFFER[*ReadPoint + ElmOpr_p1_Chr_MixMod];
        E_Layer[Layer].Xpos         = USART_BUFFER[*ReadPoint + ElmOpr_p1_Chr_Xpos];
        E_Layer[Layer].Xpos_Target  = USART_BUFFER[*ReadPoint + ElmOpr_p1_Chr_Xpos];
        E_Layer[Layer].Xpos_Fcn     = USART_BUFFER[*ReadPoint + ElmOpr_p1_Chr_XposFc];
        E_Layer[Layer].Ypos         = USART_BUFFER[*ReadPoint + ElmOpr_p1_Chr_Ypos];
        E_Layer[Layer].Ypos_Target  = USART_BUFFER[*ReadPoint + ElmOpr_p1_Chr_Ypos];
        E_Layer[Layer].Ypos_Fcn     = USART_BUFFER[*ReadPoint + ElmOpr_p1_Chr_YposFc];
        E_Layer[Layer].Alpha        = USART_BUFFER[*ReadPoint + ElmOpr_p1_Chr_Alpha];
        E_Layer[Layer].Alpha_Target = USART_BUFFER[*ReadPoint + ElmOpr_p1_Chr_Alpha];
        E_Layer[Layer].Alpha_Fcn    = USART_BUFFER[*ReadPoint + ElmOpr_p1_Chr_AphaFc];
        E_Layer[Layer].Text_Length  = USART_BUFFER[*ReadPoint + ElmOpr_p1_Chr_TxtLen];
        for (i = 0 ; i < USART_BUFFER[*ReadPoint + ElmOpr_p1_Chr_TxtLen] ; i++)
        {
            E_Layer[Layer].Text[i] = USART_BUFFER[*ReadPoint + ElmOpr_px_Chr_Contnt + i];
        }
        // Characters require the display of multiple elements at once.
        // Xsize is used for EtoFRAME boundary processing.
        // Direct loading of single-element sizes is not allowed; otherwise, only one character can be displayed.
        // Use the parameters for drawing rectangles to store the size of a single character. The Xsize is automatically calculated in the LoadE function.
        E_Layer[Layer].Block_Xsize_Target	= FlashIndex[ItemTemp*FlashIndex_Size + FlsIdx_p1_Xsize];
        E_Layer[Layer].Ysize	            = FlashIndex[ItemTemp*FlashIndex_Size + FlsIdx_p1_Ysize];

        *ReadPoint += ElmOpr_p1_Chr_Check+1 + USART_BUFFER[*ReadPoint + ElmOpr_p1_Chr_TxtLen];
        return 0;
    }

	switch(USART_BUFFER[*ReadPoint + ElmOpr_Gp1_Type])
	{
	    // Disable Layer
		case Type_Disable:
		{
		    if (USART_BUFFER_Size <= (*ReadPoint + ElmOpr_p1_Dis_Check))    {return 1;}
			if (CmdChecker(*ReadPoint, ElmOpr_p1_Dis_Check))                {return 1;}
			
            E_Layer[Layer].Type             = 0;
            E_Layer[Layer].StreamAdd        = 0;
            E_Layer[Layer].StreamAdd_Target = 0;
            E_Layer[Layer].Stream_Pause     = 0;
            E_Layer[Layer].StreamOffset     = 0;

            *ReadPoint += ElmOpr_p1_Dis_Check+1;
            return 0;
		} break;

		// Picture
		case Type_Picture:
		{
		    if (USART_BUFFER_Size <= (*ReadPoint + ElmOpr_p1_Pcr_Check))    {return 1;}
		    if (CmdChecker(*ReadPoint, ElmOpr_p1_Pcr_Check))                {return 1;}

		    ItemTemp = USART_BUFFER[*ReadPoint + ElmOpr_p1_Chr_FlsIdx];
            if (ItemTemp >= FlashIndex_CNT)  {return 1;}

            E_Layer[Layer].Type         = USART_BUFFER[*ReadPoint + ElmOpr_Gp1_Type];
            E_Layer[Layer].PipelineFlag = USART_BUFFER[*ReadPoint + ElmOpr_p1_Pcr_Piplin];
            E_Layer[Layer].Flash_Item   = USART_BUFFER[*ReadPoint + ElmOpr_p1_Pcr_FlsIdx];
            E_Layer[Layer].MixMode      = USART_BUFFER[*ReadPoint + ElmOpr_p1_Pcr_MixMod];
            E_Layer[Layer].Xpos         = USART_BUFFER[*ReadPoint + ElmOpr_p1_Pcr_Xpos];
            E_Layer[Layer].Xpos_Target  = USART_BUFFER[*ReadPoint + ElmOpr_p1_Pcr_Xpos];
            E_Layer[Layer].Xpos_Fcn     = USART_BUFFER[*ReadPoint + ElmOpr_p1_Pcr_XposFc];
            E_Layer[Layer].Ypos         = USART_BUFFER[*ReadPoint + ElmOpr_p1_Pcr_Ypos];
            E_Layer[Layer].Ypos_Target  = USART_BUFFER[*ReadPoint + ElmOpr_p1_Pcr_Ypos];
            E_Layer[Layer].Ypos_Fcn     = USART_BUFFER[*ReadPoint + ElmOpr_p1_Pcr_YposFc];
            E_Layer[Layer].Alpha        = USART_BUFFER[*ReadPoint + ElmOpr_p1_Pcr_Alpha];
            E_Layer[Layer].Alpha_Target = USART_BUFFER[*ReadPoint + ElmOpr_p1_Pcr_Alpha];
            E_Layer[Layer].Alpha_Fcn    = USART_BUFFER[*ReadPoint + ElmOpr_p1_Pcr_AphaFc];
            E_Layer[Layer].Picture_Add  = ((uint16_t)USART_BUFFER[*ReadPoint + ElmOpr_p2_Pcr_PcrAdd] << 8) + USART_BUFFER[*ReadPoint + ElmOpr_p2_Pcr_PcrAdd+1];
            E_Layer[Layer].Xsize	    = FlashIndex[ItemTemp*FlashIndex_Size + FlsIdx_p1_Xsize];
            E_Layer[Layer].Ysize		= FlashIndex[ItemTemp*FlashIndex_Size + FlsIdx_p1_Ysize];

            *ReadPoint += ElmOpr_p1_Pcr_Check+1;
            return 0;
		} break;

		// Draw Block
		case Type_DrawBlock:
		{
		    if (USART_BUFFER_Size <= (*ReadPoint + ElmOpr_p1_Blk_Check))    {return 1;}
		    if (CmdChecker(*ReadPoint, ElmOpr_p1_Blk_Check))                {return 1;}

            E_Layer[Layer].Type                 = USART_BUFFER[*ReadPoint + ElmOpr_Gp1_Type];
            E_Layer[Layer].PipelineFlag         = USART_BUFFER[*ReadPoint + ElmOpr_p1_Blk_Piplin];
            E_Layer[Layer].MixMode              = USART_BUFFER[*ReadPoint + ElmOpr_p1_Blk_MixMod];
            E_Layer[Layer].Xpos                 = USART_BUFFER[*ReadPoint + ElmOpr_p1_Blk_Xpos];
            E_Layer[Layer].Xpos_Target          = USART_BUFFER[*ReadPoint + ElmOpr_p1_Blk_Xpos];
            E_Layer[Layer].Xpos_Fcn             = USART_BUFFER[*ReadPoint + ElmOpr_p1_Blk_XposFc];
            E_Layer[Layer].Ypos                 = USART_BUFFER[*ReadPoint + ElmOpr_p1_Blk_Ypos];
            E_Layer[Layer].Ypos_Target          = USART_BUFFER[*ReadPoint + ElmOpr_p1_Blk_Ypos];
            E_Layer[Layer].Ypos_Fcn             = USART_BUFFER[*ReadPoint + ElmOpr_p1_Blk_YposFc];
            E_Layer[Layer].Alpha                = USART_BUFFER[*ReadPoint + ElmOpr_p1_Blk_Alpha];
            E_Layer[Layer].Alpha_Target         = USART_BUFFER[*ReadPoint + ElmOpr_p1_Blk_Alpha];
            E_Layer[Layer].Alpha_Fcn            = USART_BUFFER[*ReadPoint + ElmOpr_p1_Blk_AphaFc];
            E_Layer[Layer].Block_Xsize          = USART_BUFFER[*ReadPoint + ElmOpr_p1_Blk_BlkXsz];
            E_Layer[Layer].Block_Xsize_Target   = USART_BUFFER[*ReadPoint + ElmOpr_p1_Blk_BlkXsz];
            E_Layer[Layer].Block_Ysize          = USART_BUFFER[*ReadPoint + ElmOpr_p1_Blk_BlkYsz];
            E_Layer[Layer].Block_Ysize_Target   = USART_BUFFER[*ReadPoint + ElmOpr_p1_Blk_BlkYsz];
            if (USART_BUFFER[*ReadPoint + ElmOpr_p1_Blk_BlkCnr] & 0x80) {E_Layer[Layer].Block_Corners = ((USART_BUFFER[*ReadPoint + ElmOpr_p1_Blk_BlkCnr]&0x7f) < 8)? USART_BUFFER[*ReadPoint + ElmOpr_p1_Blk_BlkCnr] : 0x87;}
            else                                                        {E_Layer[Layer].Block_Corners = (USART_BUFFER[*ReadPoint + ElmOpr_p1_Blk_BlkCnr] < 6)? USART_BUFFER[*ReadPoint + ElmOpr_p1_Blk_BlkCnr] : 0x05;}

            *ReadPoint += ElmOpr_p1_Blk_Check+1;
            return 0;
		} break;
		default:    {return 1;}
	}
}





static inline uint8_t LayerBaseValue(uint8_t* ReadPoint)
{
    uint8_t Layer;

    Layer = GetCmdLayer(USART_BUFFER[*ReadPoint + LyBsVu_p1_Layer]);

    if (Layer >= E_Layer_Size)                                  {return 1;}
    if (USART_BUFFER_Size <= (*ReadPoint + LyBsVu_p1_Check))    {return 1;}
    if (CmdChecker(*ReadPoint, LyBsVu_p1_Check))                {return 1;}

    E_Layer[Layer].PipelineFlag = USART_BUFFER[*ReadPoint + LyBsVu_p1_Pipeline];
    E_Layer[Layer].Xpos_Target  = USART_BUFFER[*ReadPoint + LyBsVu_p1_XposTarget];
    E_Layer[Layer].Xpos_Fcn     = USART_BUFFER[*ReadPoint + LyBsVu_p1_XposFcn];
    E_Layer[Layer].Ypos_Target  = USART_BUFFER[*ReadPoint + LyBsVu_p1_YposTarget];
    E_Layer[Layer].Ypos_Fcn     = USART_BUFFER[*ReadPoint + LyBsVu_p1_YposFcn];
    E_Layer[Layer].Alpha_Target = USART_BUFFER[*ReadPoint + LyBsVu_p1_AphaTarget];
    E_Layer[Layer].Alpha_Fcn    = USART_BUFFER[*ReadPoint + LyBsVu_p1_AphaFcn];

    *ReadPoint += LyBsVu_p1_Check+1;
    return 0;
}





static inline uint8_t StreamSet(uint8_t* ReadPoint)
{
    uint8_t Layer;

    Layer = GetCmdLayer(USART_BUFFER[*ReadPoint + SrmSet_p1_Layer]);

    if (Layer >= E_Layer_Size)                                  {return 1;}
    if (USART_BUFFER_Size <= (*ReadPoint + SrmSet_p1_Check))    {return 1;}
    if (CmdChecker(*ReadPoint, SrmSet_p1_Check))                {return 1;}

    E_Layer[Layer].StreamAdd            = ((uint16_t)USART_BUFFER[*ReadPoint + SrmSet_p2_Add] << 8) + USART_BUFFER[*ReadPoint + SrmSet_p2_Add+1];
    E_Layer[Layer].StreamAdd_Target     = ((uint16_t)USART_BUFFER[*ReadPoint + SrmSet_p2_Add] << 8) + USART_BUFFER[*ReadPoint + SrmSet_p2_Add+1];
    E_Layer[Layer].StreamAdd_Fcn        = USART_BUFFER[*ReadPoint + SrmSet_p1_Fcn];
    E_Layer[Layer].Stream_Attributes    = ((uint16_t)USART_BUFFER[*ReadPoint + SrmSet_p2_Atb] << 8) + USART_BUFFER[*ReadPoint + SrmSet_p2_Atb+1];

    *ReadPoint += SrmSet_p1_Check+1;
    return 0;
}





static inline uint8_t StreamValue(uint8_t* ReadPoint)
{
    uint8_t Layer;

    Layer = GetCmdLayer(USART_BUFFER[*ReadPoint + SrmVlu_p1_Layer]);

    if (Layer >= E_Layer_Size)                                  {return 1;}
    if (USART_BUFFER_Size <= (*ReadPoint + SrmVlu_p1_Check))    {return 1;}
    if (CmdChecker(*ReadPoint, SrmVlu_p1_Check))                {return 1;}

    E_Layer[Layer].StreamAdd_Target     = ((uint16_t)USART_BUFFER[*ReadPoint + SrmVlu_p3_Add] << 16)|
                                          ((uint16_t)USART_BUFFER[*ReadPoint + SrmVlu_p3_Add+1] << 8)|
                                          (uint16_t)USART_BUFFER[*ReadPoint + SrmVlu_p3_Add+2];
    E_Layer[Layer].StreamAdd_Fcn        = USART_BUFFER[*ReadPoint + SrmVlu_p1_Fcn];

    *ReadPoint += SrmVlu_p1_Check+1;
    return 0;
}





static inline uint8_t StreamPause(uint8_t* ReadPoint)
{
    uint8_t Layer;

    Layer = GetCmdLayer(USART_BUFFER[*ReadPoint + SrmPus_p1_Layer]);

    if (Layer >= E_Layer_Size)                                  {return 1;}
    if (USART_BUFFER_Size <= (*ReadPoint + SrmPus_p1_Check))    {return 1;}
    if (CmdChecker(*ReadPoint, SrmPus_p1_Check))                {return 1;}

    if (USART_BUFFER[*ReadPoint + SrmPus_p1_PusEn]) {E_Layer[Layer].Stream_Pause |= 0x80;}
    else                                            {E_Layer[Layer].Stream_Pause &= ~0x80;}

    *ReadPoint += SrmPus_p1_Check+1;
    return 0;
}





static inline uint8_t BlockValue(uint8_t* ReadPoint)
{
    uint8_t Layer;

    Layer = GetCmdLayer(USART_BUFFER[*ReadPoint + BlkVlu_p1_Layer]);

    if (Layer >= E_Layer_Size)                                  {return 1;}
    if (USART_BUFFER_Size <= (*ReadPoint + BlkVlu_p1_Check))    {return 1;}
    if (CmdChecker(*ReadPoint, BlkVlu_p1_Check))                {return 1;}

    E_Layer[Layer].Block_Xsize_Target   = USART_BUFFER[*ReadPoint + BlkVlu_p1_XszTarget];
    E_Layer[Layer].Block_Xsize_Fcn      = USART_BUFFER[*ReadPoint + BlkVlu_p1_XszFcn];
    E_Layer[Layer].Block_Ysize_Target   = USART_BUFFER[*ReadPoint + BlkVlu_p1_YszTarget];
    E_Layer[Layer].Block_Ysize_Fcn      = USART_BUFFER[*ReadPoint + BlkVlu_p1_YszFcn];

    *ReadPoint += BlkVlu_p1_Check+1;
    return 0;
}





static inline uint8_t DisableAllLayer(uint8_t* ReadPoint)
{
    uint8_t i;

    if (USART_BUFFER_Size <= (*ReadPoint + 3))  {return 1;}

    for (i = 0 ; i < 3 ; i++)
    {
        if (USART_BUFFER[*ReadPoint + i] != 0xa0)   {return 1;}
    }

    for (i = 0 ; i < 16 ; i++)
    {
        E_Layer[i].Type             = 0;
        E_Layer[i].StreamAdd        = 0;
        E_Layer[i].StreamAdd_Target = 0;
        E_Layer[i].Stream_Pause     = 0;
        E_Layer[i].StreamOffset     = 0;
    }

    *ReadPoint += 3;
    return 0;
}





static inline uint8_t FrameClear(uint8_t* ReadPoint)
{
    uint8_t i;

    if (USART_BUFFER_Size <= (*ReadPoint + 3))  {return 1;}

    for (i = 0 ; i < 3 ; i++)
    {
        if (USART_BUFFER[*ReadPoint + i] != USART_BUFFER[*ReadPoint])   {return 1;}
    }

    if (GetCmdValue(USART_BUFFER[*ReadPoint]))  {GlobalFlag |= GlobalFlag_FrameClr;}
    else                                        {GlobalFlag &= ~GlobalFlag_FrameClr;}

    *ReadPoint += 3;
    return 0;
}





static inline uint8_t ScreenSet(uint8_t* ReadPoint)
{
    uint8_t i;

    if (USART_BUFFER_Size <= (*ReadPoint + 3))  {return 1;}

    for (i = 0 ; i < 3 ; i++)
    {
        if(USART_BUFFER[*ReadPoint + i] != USART_BUFFER[*ReadPoint])    {return 1;}
    }

    if (GetCmdValue(USART_BUFFER[*ReadPoint]) & ScnSet_ShowF)   {OLED_Set(OLED_SetMode_OtherOptions, OLED_SetMode_Show_ENABLE);}
    else                                                        {OLED_Set(OLED_SetMode_OtherOptions, OLED_SetMode_Show_DISABLE);}

    if (GetCmdValue(USART_BUFFER[*ReadPoint]) & ScnSet_XflipF)  {OLED_Set(OLED_SetMode_OtherOptions, OLED_SetMode_X_Flip_Normal);}
    else                                                        {OLED_Set(OLED_SetMode_OtherOptions, OLED_SetMode_X_Flip_Mirror);}

    if (GetCmdValue(USART_BUFFER[*ReadPoint]) & ScnSet_YflipF)  {OLED_Set(OLED_SetMode_OtherOptions, OLED_SetMode_Y_Flip_Normal);}
    else                                                        {OLED_Set(OLED_SetMode_OtherOptions, OLED_SetMode_Y_Flip_Mirror);}

    if (GetCmdValue(USART_BUFFER[*ReadPoint]) & ScnSet_ColorF)  {OLED_Set(OLED_SetMode_OtherOptions, OLED_SetMode_Color_Normal);}
    else                                                        {OLED_Set(OLED_SetMode_OtherOptions, OLED_SetMode_Color_Invert);}

    *ReadPoint += 3;
    return 0;
}