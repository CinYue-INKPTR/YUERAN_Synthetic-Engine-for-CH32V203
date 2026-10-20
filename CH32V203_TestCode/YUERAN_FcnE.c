#include "YUERAN_FcnE.h"

#include <stdint.h>
#include "YUERAN_Define.h"

#define XYSSFcn_GetMode(Dat)        ((Dat) >> 6)
#define XYSSFcn_GetValue(Dat)       ((Dat) & 0x3f)
#define XYSSFcn_Mode_None           0
#define XYSSFcn_Mode_UniformSpeed   1
#define XYSSFcn_Mode_SpeedDown      2
#define XYSSFcn_Mode_SpeedUp        3
#define AlphaFcn_GetSwitch(Dat)	    ((Dat) & 0x10)
#define AlphaFcn_GetValue(Dat)	    ((Dat) & 0x0f)

#define SrmAtb_GetDirF(Dat)         ((Dat) & 0x8000)
#define SrmAtb_GetCirF(Dat)         ((Dat) & 0x4000)
#define SrmAtb_GetStpLen(Dat)       (((Dat) >> 7) & 0x7f)
#define SrmAtb_GetFrzVlu(Dat)       ((Dat) & 0x7f)

#define SrmPus_FrzVlu_Mask          0x7f
#define SrmPus_GetPusF(Dat)         ((Dat) & 0x80)
#define SrmPus_GetFrzVlu(Dat)       ((Dat) & SrmPus_FrzVlu_Mask)





static inline void FcnE_Pos(uint8_t Layer, int8_t* Value_Now, int8_t Value_Order, uint8_t FcnINFO, uint8_t* SDstep, uint8_t* SUstep)
{
	uint8_t value, distance, step;

	value = XYSSFcn_GetValue(FcnINFO);
	
	switch (XYSSFcn_GetMode(FcnINFO))
	{
		case XYSSFcn_Mode_None:
		{
			*Value_Now = Value_Order;
		} break;

		case XYSSFcn_Mode_UniformSpeed:
		{
		    if (*Value_Now == Value_Order)  {return;}

            if (Value_Order > *Value_Now)   {*Value_Now = ((Value_Order - *Value_Now) > value)? (*Value_Now + value) : (Value_Order);}
            else                            {*Value_Now = ((*Value_Now - Value_Order) > value)? (*Value_Now - value) : (Value_Order);}
		} break;

		case XYSSFcn_Mode_SpeedDown:
		{
//			if(value == 0)	{return;}
//			if(*Value_Now != Value_Order)
//			{
//				if(Value_Order > *Value_Now)
//				{
//					tmp = (Value_Order - *Value_Now) / value;
//					*Value_Now = (tmp)?
//						(*Value_Now + tmp) :
//						(*Value_Now + 1);
//				}
//				else
//				{
//					tmp = (*Value_Now - Value_Order) / value;
//					*Value_Now = (tmp)?
//						(*Value_Now - tmp) :
//						(*Value_Now - 1);
//				}
//			}

			if (value == 0)                 {return;}
            if (*Value_Now == Value_Order)  {*SDstep = 0;   return;}

            distance = (*Value_Now < Value_Order)? (Value_Order - *Value_Now) : (*Value_Now - Value_Order);
            step = distance/value + 1;

            if ((step < *SDstep) && (*SDstep > 1))  {step = *SDstep - 1;}
            if (step < 1)                           {step = 1;}
            if (step > distance)                    {step = distance;}
            *SDstep = step;

            if (*Value_Now < Value_Order)           {*Value_Now += step;}
            else                                    {*Value_Now -= step;}
		} break;

		case XYSSFcn_Mode_SpeedUp:
		{
		    if (*Value_Now == Value_Order)  {*SUstep = 0;   return;}

            if ((*SUstep + value) < 255)    {*SUstep += value;}
            else                            {*SUstep = 255;}

            if (Value_Order > *Value_Now)   {*Value_Now = ((Value_Order - *Value_Now) > *SUstep)? (*Value_Now + *SUstep) : (Value_Order);}
            else                            {*Value_Now = ((*Value_Now - Value_Order) > *SUstep)? (*Value_Now - *SUstep) : (Value_Order);}
		} break;
	}

	if (XYSSFcn_GetMode(FcnINFO) != XYSSFcn_Mode_SpeedDown) {*SDstep = 0;}
    if (XYSSFcn_GetMode(FcnINFO) != XYSSFcn_Mode_SpeedUp)   {*SUstep = 0;}
}





static inline void FcnE_BlockSize(uint8_t Layer, uint8_t* Value_Now, uint8_t Value_Order, uint8_t FcnINFO, uint8_t* SDstep, uint8_t* SUstep)
{
	uint8_t value, distance, step;

    value = XYSSFcn_GetValue(FcnINFO);
	
	switch (XYSSFcn_GetMode(FcnINFO))
	{
		case XYSSFcn_Mode_None:
		{
			*Value_Now = Value_Order;
		} break;

		case XYSSFcn_Mode_UniformSpeed:
		{
		    if (*Value_Now == Value_Order)  {return;}

            if (Value_Order > *Value_Now)   {*Value_Now = ((Value_Order - *Value_Now) > value)? (*Value_Now + value) : (Value_Order);}
            else                            {*Value_Now = ((*Value_Now - Value_Order) > value)? (*Value_Now - value) : (Value_Order);}
		} break;

		case XYSSFcn_Mode_SpeedDown:
		{
//			if(value == 0)	{return;}
//			if(*Value_Now != Value_Order)
//			{
//			    distance = (*Value_Now < Value_Order)? (Value_Order - *Value_Now) : (*Value_Now - Value_Order);
//			    step = distance/value;
//			    if(step)    {*Value_Now = (*Value_Now < Value_Order)? (*Value_Now + step) : (*Value_Now - step);}
//			    else        {*Value_Now = (*Value_Now < Value_Order)? (*Value_Now + 1) : (*Value_Now - 1);}
//			}

            if (value == 0)                 {return;}
            if (*Value_Now == Value_Order)  {*SDstep = 0;   return;}

            distance = (*Value_Now < Value_Order)? (Value_Order - *Value_Now) : (*Value_Now - Value_Order);
            step = distance/value + 1;

            if ((step < *SDstep) && (*SDstep > 1))  {step = *SDstep - 1;}
            if (step < 1)                           {step = 1;}
            if (step > distance)                    {step = distance;}
            *SDstep = step;

            if (*Value_Now < Value_Order)           {*Value_Now += step;}
            else                                    {*Value_Now -= step;}
		} break;

		case XYSSFcn_Mode_SpeedUp:
		{
		    if (*Value_Now == Value_Order)  {*SUstep = 0;   return;}

            if ((*SUstep + value) < 255)    {*SUstep += value;}
            else                            {*SUstep = 255;}

            if (Value_Order > *Value_Now)   {*Value_Now = ((Value_Order - *Value_Now) > *SUstep)? (*Value_Now + *SUstep) : (Value_Order);}
            else                            {*Value_Now = ((*Value_Now - Value_Order) > *SUstep)? (*Value_Now - *SUstep) : (Value_Order);}
		} break;
	}

	if (XYSSFcn_GetMode(FcnINFO) != XYSSFcn_Mode_SpeedDown) {*SDstep = 0;}
	if (XYSSFcn_GetMode(FcnINFO) != XYSSFcn_Mode_SpeedUp)   {*SUstep = 0;}
}





static inline void FcnE_Alpha(uint8_t Layer)
{
    uint8_t value;

    if (E_Layer[Layer].Alpha == E_Layer[Layer].Alpha_Target)    {return;}

    value = AlphaFcn_GetValue(E_Layer[Layer].Alpha_Fcn);
    if (AlphaFcn_GetSwitch(E_Layer[Layer].Alpha_Fcn))
    {
        if(E_Layer[Layer].Alpha < E_Layer[Layer].Alpha_Target)
        {
            E_Layer[Layer].Alpha = ((E_Layer[Layer].Alpha_Target - E_Layer[Layer].Alpha) > value)?
                (E_Layer[Layer].Alpha + value) : (E_Layer[Layer].Alpha_Target);
        }
        else
        {
            E_Layer[Layer].Alpha = ((E_Layer[Layer].Alpha - E_Layer[Layer].Alpha_Target) > value)?
                (E_Layer[Layer].Alpha - value) : (E_Layer[Layer].Alpha_Target);
        }
    }
    else    {E_Layer[Layer].Alpha = E_Layer[Layer].Alpha_Target;}
}





static inline void FcnE_Stream(uint8_t Layer)
{
    uint8_t value, srm_step;
    uint32_t distance, step;

    if (SrmPus_GetPusF(E_Layer[Layer].Stream_Pause))    {return;}

    if (SrmPus_GetFrzVlu(E_Layer[Layer].Stream_Pause) < SrmAtb_GetFrzVlu(E_Layer[Layer].Stream_Attributes))
    {
        E_Layer[Layer].Stream_Pause++;
        return;
    }
    E_Layer[Layer].Stream_Pause &= ~SrmPus_FrzVlu_Mask;

    srm_step = SrmAtb_GetStpLen(E_Layer[Layer].Stream_Attributes);

    if (SrmAtb_GetCirF(E_Layer[Layer].Stream_Attributes))
    {
        E_Layer[Layer].StreamAdd = (E_Layer[Layer].StreamAdd < E_Layer[Layer].StreamAdd_Target)? (E_Layer[Layer].StreamAdd + 1) : 0;

        if(SrmAtb_GetDirF(E_Layer[Layer].Stream_Attributes))    {E_Layer[Layer].StreamOffset = -(E_Layer[Layer].StreamAdd*srm_step);}
        else                                                    {E_Layer[Layer].StreamOffset = E_Layer[Layer].StreamAdd*srm_step;}
        return;
    }

    value = XYSSFcn_GetValue(E_Layer[Layer].StreamAdd_Fcn);

    switch (XYSSFcn_GetMode(E_Layer[Layer].StreamAdd_Fcn))
    {
        case XYSSFcn_Mode_None:
        {
            E_Layer[Layer].StreamAdd = E_Layer[Layer].StreamAdd_Target;
        } break;

        case XYSSFcn_Mode_UniformSpeed:
        {
            if (E_Layer[Layer].StreamAdd == E_Layer[Layer].StreamAdd_Target)    {return;}

            distance = (E_Layer[Layer].StreamAdd < E_Layer[Layer].StreamAdd_Target)?
                (E_Layer[Layer].StreamAdd_Target - E_Layer[Layer].StreamAdd) : (E_Layer[Layer].StreamAdd - E_Layer[Layer].StreamAdd_Target);

            if (E_Layer[Layer].StreamAdd_Target > E_Layer[Layer].StreamAdd)
            {
                E_Layer[Layer].StreamAdd = ((E_Layer[Layer].StreamAdd_Target - E_Layer[Layer].StreamAdd) > value)? (E_Layer[Layer].StreamAdd + value) : (E_Layer[Layer].StreamAdd_Target);
            }
            else
            {
                E_Layer[Layer].StreamAdd = ((E_Layer[Layer].StreamAdd - E_Layer[Layer].StreamAdd_Target) > value)? (E_Layer[Layer].StreamAdd - value) : (E_Layer[Layer].StreamAdd_Target);
            }
        } break;

        case XYSSFcn_Mode_SpeedDown:
        {
            // if (value == 0) {return;}
            // if (E_Layer[Layer].StreamAdd == E_Layer[Layer].StreamAdd_Target)    {E_Layer[Layer].StreamAdd_Fcn_SDstep = 0;   return;}

            // distance = (E_Layer[Layer].StreamAdd < E_Layer[Layer].StreamAdd_Target)? (E_Layer[Layer].StreamAdd_Target - E_Layer[Layer].StreamAdd) : (E_Layer[Layer].StreamAdd - E_Layer[Layer].StreamAdd_Target);
            // step = ((distance/value + 1) > 255)? 255 : (distance/value + 1);

            // if ((step < E_Layer[Layer].StreamAdd_Fcn_SDstep) && (E_Layer[Layer].StreamAdd_Fcn_SDstep > 1))  {step = E_Layer[Layer].StreamAdd_Fcn_SDstep - 1;}
            // if (step < 1)           {step = 1;}
            // if (step > distance)    {step = distance;}
            // E_Layer[Layer].StreamAdd_Fcn_SDstep = step;

            // if (E_Layer[Layer].StreamAdd_Fcn_SDstep < E_Layer[Layer].StreamAdd_Target)  {E_Layer[Layer].StreamAdd += step;}
            // else                                                                        {E_Layer[Layer].StreamAdd -= step;}

            if (value == 0)	{return;}
            if (E_Layer[Layer].StreamAdd == E_Layer[Layer].StreamAdd_Target)    {return;}
            distance = (E_Layer[Layer].StreamAdd < E_Layer[Layer].StreamAdd_Target)? (E_Layer[Layer].StreamAdd_Target - E_Layer[Layer].StreamAdd) : (E_Layer[Layer].StreamAdd - E_Layer[Layer].StreamAdd_Target);
            step = distance/value;
            if(step)    {E_Layer[Layer].StreamAdd = (E_Layer[Layer].StreamAdd < E_Layer[Layer].StreamAdd_Target)? (E_Layer[Layer].StreamAdd + step) : (E_Layer[Layer].StreamAdd - step);}
            else        {E_Layer[Layer].StreamAdd = (E_Layer[Layer].StreamAdd < E_Layer[Layer].StreamAdd_Target)? (E_Layer[Layer].StreamAdd + 1) : (E_Layer[Layer].StreamAdd - 1);}
        } break;

        case XYSSFcn_Mode_SpeedUp:
        {
            if (E_Layer[Layer].StreamAdd == E_Layer[Layer].StreamAdd_Target)    {E_Layer[Layer].StreamAdd_Fcn_SUstep = 0;   return;}

            if ((E_Layer[Layer].StreamAdd_Fcn_SUstep + value) < 255)    {E_Layer[Layer].StreamAdd_Fcn_SUstep += value;}
            else                                                        {E_Layer[Layer].StreamAdd_Fcn_SUstep = 255;}

            if (E_Layer[Layer].StreamAdd_Target > E_Layer[Layer].StreamAdd)
            {
                E_Layer[Layer].StreamAdd = ((E_Layer[Layer].StreamAdd_Target - E_Layer[Layer].StreamAdd) > E_Layer[Layer].StreamAdd_Fcn_SUstep)?
                        (E_Layer[Layer].StreamAdd + E_Layer[Layer].StreamAdd_Fcn_SUstep) : (E_Layer[Layer].StreamAdd_Target);
            }
            else
            {
                E_Layer[Layer].StreamAdd = ((E_Layer[Layer].StreamAdd - E_Layer[Layer].StreamAdd_Target) > E_Layer[Layer].StreamAdd_Fcn_SUstep)?
                    (E_Layer[Layer].StreamAdd - E_Layer[Layer].StreamAdd_Fcn_SUstep) : (E_Layer[Layer].StreamAdd_Target);
            }
        } break;
    }

    if (XYSSFcn_GetMode(E_Layer[Layer].StreamAdd_Fcn) != XYSSFcn_Mode_SpeedUp)      {E_Layer[Layer].StreamAdd_Fcn_SUstep = 0;}
    E_Layer[Layer].StreamOffset = E_Layer[Layer].StreamAdd*srm_step;
}





void FcnE(uint8_t Layer)
{
	FcnE_Pos(Layer, &E_Layer[Layer].Xpos, E_Layer[Layer].Xpos_Target, E_Layer[Layer].Xpos_Fcn, &E_Layer[Layer].Xpos_Fcn_SDstep, &E_Layer[Layer].Xpos_Fcn_SUstep);
	FcnE_Pos(Layer, &E_Layer[Layer].Ypos, E_Layer[Layer].Ypos_Target, E_Layer[Layer].Ypos_Fcn, &E_Layer[Layer].Xpos_Fcn_SDstep, &E_Layer[Layer].Ypos_Fcn_SUstep);
	FcnE_Alpha(Layer);
	FcnE_Stream(Layer);
	if(E_Layer[Layer].Type == Type_DrawBlock)
	{
	    FcnE_BlockSize(Layer, &E_Layer[Layer].Block_Xsize, E_Layer[Layer].Block_Xsize_Target, E_Layer[Layer].Block_Xsize_Fcn, &E_Layer[Layer].Block_Xsize_Fcn_SDstep, &E_Layer[Layer].Block_Xsize_Fcn_SUstep);
	    FcnE_BlockSize(Layer, &E_Layer[Layer].Block_Ysize, E_Layer[Layer].Block_Ysize_Target, E_Layer[Layer].Block_Ysize_Fcn, &E_Layer[Layer].Block_Xsize_Fcn_SDstep, &E_Layer[Layer].Block_Ysize_Fcn_SUstep);
	}
}
