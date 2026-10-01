#include <stdint.h>
#include "YUERAN_FcnE.h"
#include "YUERAN_Define.h"

#define SUstep_Xpos			0
#define SUstep_Ypos			1
#define SUstep_BlockXsize	2
#define SUstep_BlockYsize	3

// Speed up step
static uint8_t SUstep[8][4] = {0};

static void FcnE_Pos(uint8_t Layer, int8_t* Value_Now, int8_t Value_Order, uint8_t FcnINFO, uint8_t SUstep_Flag)
{
	uint8_t tmp, value;

	value = FcnINFO & 0x3f;
	
	switch(FcnINFO >> 6)
	{
		// 瞬变
		// 让当前值直接等于目标值
		case 0:
		{
			*Value_Now = Value_Order;
		} break;
		// 匀速
		// 以参数作为步长匀速向目标值递进
		case 1:
		{
			if(*Value_Now != Value_Order)
			{
				if(Value_Order > *Value_Now)
				{
					*Value_Now = ((Value_Order - *Value_Now) > value)?
						(*Value_Now + value) :
						(Value_Order);
				}
				else
				{
					*Value_Now = ((*Value_Now - Value_Order) > value)?
						(*Value_Now - value) :
						(Value_Order);
				}
			}
		} break;
		// 减速
		// 以参数作为当前、目标值的差值的分母，除得的值作为递进步长值（最低为1）
		case 2:
		{
			if(value == 0)	{return;}
			if(*Value_Now != Value_Order)
			{
				if(Value_Order > *Value_Now)
				{
					tmp = (Value_Order - *Value_Now) / value;
					*Value_Now = (tmp)?
						(*Value_Now + tmp) :
						(*Value_Now + 1);
				}
				else
				{
					tmp = (*Value_Now - Value_Order) / value;
					*Value_Now = (tmp)?
						(*Value_Now - tmp) :
						(*Value_Now - 1);
				}
			}
		} break;
		// 加速
		case 3:
		{
			if(*Value_Now != Value_Order)
			{
				if((SUstep[Layer][SUstep_Flag] + value) < 255)	{SUstep[Layer][SUstep_Flag] += value;}
				else											{SUstep[Layer][SUstep_Flag] = 255;}
				
				if(Value_Order > *Value_Now)
				{
					
					*Value_Now = ((Value_Order - *Value_Now) > SUstep[Layer][SUstep_Flag])?
						(*Value_Now + SUstep[Layer][SUstep_Flag]) :
						(Value_Order);
				}
				else
				{
					*Value_Now = ((*Value_Now - Value_Order) > SUstep[Layer][SUstep_Flag])?
						(*Value_Now - SUstep[Layer][SUstep_Flag]) :
						(Value_Order);
				}
			}
			else
			{
				SUstep[Layer][SUstep_Flag] = 0;
			}
		} break;
	}
	// 如果加速模式未完成动画就切换模式，要清除历史值（因为没有进加速的else清零）
	if((FcnINFO >> 6) < 3)
	{
		SUstep[Layer][SUstep_Flag] = 0;
	}
}

static void FcnE_BlockSize(uint8_t Layer, uint8_t* Value_Now, uint8_t Value_Order, uint8_t FcnINFO, uint8_t SUstep_Flag)
{
	uint8_t tmp, value;

	value = FcnINFO & 0x3f;
	
	switch(FcnINFO >> 6)
	{
		// 瞬变
		// 让当前值直接等于目标值
		case 0:
		{
			*Value_Now = Value_Order;
		} break;
		// 匀速
		// 以参数作为步长匀速向目标值递进
		case 1:
		{
			if(*Value_Now != Value_Order)
			{
				if(Value_Order > *Value_Now)
				{
					*Value_Now = ((Value_Order - *Value_Now) > value)?
						(*Value_Now + value) :
						(Value_Order);
				}
				else
				{
					*Value_Now = ((*Value_Now - Value_Order) > value)?
						(*Value_Now - value) :
						(Value_Order);
				}
			}
		} break;
		// 减速
		// 以参数作为当前、目标值的差值的分母，除得的值作为递进步长值（最低为1）
		case 2:
		{
			if(value == 0)	{return;}
			if(*Value_Now != Value_Order)
			{
				if(Value_Order > *Value_Now)
				{
					tmp = (Value_Order - *Value_Now) / value;
					*Value_Now = (tmp)?
						(*Value_Now + tmp) :
						(*Value_Now + 1);
				}
				else
				{
					tmp = (*Value_Now - Value_Order) / value;
					*Value_Now = (tmp)?
						(*Value_Now - tmp) :
						(*Value_Now - 1);
				}
			}
		} break;
		// 加速
		case 3:
		{
			if(*Value_Now != Value_Order)
			{
				if((SUstep[Layer][SUstep_Flag] + value) < 255)	{SUstep[Layer][SUstep_Flag] += value;}
				else											{SUstep[Layer][SUstep_Flag] = 255;}
				
				if(Value_Order > *Value_Now)
				{
					
					*Value_Now = ((Value_Order - *Value_Now) > SUstep[Layer][SUstep_Flag])?
						(*Value_Now + SUstep[Layer][SUstep_Flag]) :
						(Value_Order);
				}
				else
				{
					*Value_Now = ((*Value_Now - Value_Order) > SUstep[Layer][SUstep_Flag])?
						(*Value_Now - SUstep[Layer][SUstep_Flag]) :
						(Value_Order);
				}
			}
			else
			{
				SUstep[Layer][SUstep_Flag] = 0;
			}
		} break;
	}
	if((FcnINFO >> 6) < 3)
	{
		SUstep[Layer][SUstep_Flag] = 0;
	}
}

static void FcnE_Alpha(uint8_t Layer)
{
    uint8_t value;
    if((E_Layer[Layer].Alpha_Fcn & 0x10) && (E_Layer[Layer].Alpha != E_Layer[Layer].Alpha_Target))
    {
        value = E_Layer[Layer].Alpha_Fcn & 0x0f;
        if(E_Layer[Layer].Alpha < E_Layer[Layer].Alpha_Target)
        {
            E_Layer[Layer].Alpha = ((E_Layer[Layer].Alpha_Target - E_Layer[Layer].Alpha) > value)?
                (E_Layer[Layer].Alpha + value) :
                (E_Layer[Layer].Alpha_Target);
        }
        else
        {
            E_Layer[Layer].Alpha = ((E_Layer[Layer].Alpha - E_Layer[Layer].Alpha_Target) > value)?
                (E_Layer[Layer].Alpha - value) :
                (E_Layer[Layer].Alpha_Target);
        }
    }
}

void FcnE(uint8_t Layer)
{
	FcnE_Pos(Layer, &E_Layer[Layer].Xpos, E_Layer[Layer].Xpos_Target, E_Layer[Layer].Xpos_Fcn, SUstep_Xpos);
	FcnE_Pos(Layer, &E_Layer[Layer].Ypos, E_Layer[Layer].Ypos_Target, E_Layer[Layer].Ypos_Fcn, SUstep_Ypos);
	FcnE_Alpha(Layer);
	FcnE_BlockSize(Layer, &E_Layer[Layer].Block_Xsize, E_Layer[Layer].Block_Xsize_Target, E_Layer[Layer].Block_Xsize_Fcn, SUstep_BlockXsize);
	FcnE_BlockSize(Layer, &E_Layer[Layer].Block_Ysize, E_Layer[Layer].Block_Ysize_Target, E_Layer[Layer].Block_Ysize_Fcn, SUstep_BlockYsize);
}
