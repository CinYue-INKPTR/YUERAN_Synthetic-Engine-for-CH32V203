#include "ch32v20x.h"
#include "YUERAN_Define.h"

#define CmdEndIdx_Disable       2
#define CmdEndIdx_Picture       10
#define CmdEndIdx_Block         10

#define CmdEndIdx_BaseValue     7
#define CmdEndIdx_StreamSet     5
#define CmdEndIdx_StreamPause   2
#define CmdEndIdx_BlockSize     5


void RenderCmd_DMAreset(void)
{
    DMA_Cmd(DMA1_Channel5, DISABLE);
    DMA1_Channel5->MADDR = (uint32_t)RenderCmd;          // 回到缓冲区首地址
    DMA_SetCurrDataCounter(DMA1_Channel5, RenderCmd_Size);   // 重新装载 64
    DMA_ClearFlag(DMA1_FLAG_TC5 | DMA1_FLAG_HT5 | DMA1_FLAG_GL5 | DMA1_FLAG_TE5);
    DMA_Cmd(DMA1_Channel5, ENABLE);                      // 先清标志，后开通道
}

static inline void BackData_DMAreset(void)
{
    if(DMA_GetFlagStatus(DMA1_FLAG_TC4) == RESET) { return; }  // 上帧还没发完，跳过

    DMA_Cmd(DMA1_Channel4, DISABLE);
    DMA1_Channel4->MADDR = (uint32_t)BackData;
    DMA_SetCurrDataCounter(DMA1_Channel4, BackData_Size);      // 43
    DMA_ClearFlag(DMA1_FLAG_TC4 | DMA1_FLAG_HT4 | DMA1_FLAG_GL4 | DMA1_FLAG_TE4);
    DMA_Cmd(DMA1_Channel4, ENABLE);                            // DMAReq_Tx 已在 USART1_ON() 里开
}

void RenderCmdLoad(void)
{
    uint8_t CmdCount, ReadPoint = 1, i, j, k, tmp, ReturnFlag;

    RenderCmd_DMAreset();

    CmdCount = RenderCmd[ReadPoint++];

    for(i = 0 ; i < CmdCount ; i++)
    {
        ReturnFlag = 1;

        switch(RenderCmd[ReadPoint] & 0xf0)
        {
            // Layer material loading/disabling command
            case 0x00:
            {


                switch(RenderCmd[ReadPoint + 1])
                {
                    // Disable Layer
                    case 0:
                    {
                        if((RenderCmd[ReadPoint + CmdEndIdx_Disable] == RenderCmd[ReadPoint]) && (RenderCmd_Size > (ReadPoint + CmdEndIdx_Disable+1)))
                        {
                            E_Layer[RenderCmd[ReadPoint]].Type  = RenderCmd[ReadPoint + 1];
                            ReadPoint += CmdEndIdx_Disable+1;
                            ReturnFlag = 0;
                        }
                    } break;

                    // Picture
                    case Type_Picture:
                    {
                        tmp = 0;
                        for(j = 0 ; j < CmdEndIdx_Picture ; j++)    {tmp += RenderCmd[ReadPoint + j];}

                        if((RenderCmd[ReadPoint + CmdEndIdx_Picture] == tmp) && (RenderCmd_Size > (ReadPoint + CmdEndIdx_Picture+1)))
                        {
                            E_Layer[RenderCmd[ReadPoint]].Type          = RenderCmd[ReadPoint + 1];
                            E_Layer[RenderCmd[ReadPoint]].Flash_Item    = RenderCmd[ReadPoint + 2];
                            E_Layer[RenderCmd[ReadPoint]].MixMode       = RenderCmd[ReadPoint + 3];
                            E_Layer[RenderCmd[ReadPoint]].Xpos          = RenderCmd[ReadPoint + 4];
                            E_Layer[RenderCmd[ReadPoint]].Xpos_Target   = RenderCmd[ReadPoint + 4];
                            E_Layer[RenderCmd[ReadPoint]].Ypos          = RenderCmd[ReadPoint + 5];
                            E_Layer[RenderCmd[ReadPoint]].Ypos_Target   = RenderCmd[ReadPoint + 5];
                            E_Layer[RenderCmd[ReadPoint]].Alpha         = RenderCmd[ReadPoint + 6];
                            E_Layer[RenderCmd[ReadPoint]].Alpha_Target  = RenderCmd[ReadPoint + 6];
                            E_Layer[RenderCmd[ReadPoint]].Alpha_Fcn             = RenderCmd[ReadPoint + 7] << 5;
                            E_Layer[RenderCmd[ReadPoint]].Picture_Add   = (RenderCmd[ReadPoint + 8] << 8) + RenderCmd[ReadPoint + 9];
                            ReadPoint += CmdEndIdx_Picture+1;
                            ReturnFlag = 0;
                        }
                    } break;

                    // Block
                    case Type_DrawBlock:
                    {
                        tmp = 0;
                        for(j = 0 ; j < CmdEndIdx_Block ; j++)    {tmp += RenderCmd[ReadPoint + j];}

                        if((RenderCmd[ReadPoint + CmdEndIdx_Block] == tmp) && (RenderCmd_Size > (ReadPoint + CmdEndIdx_Block+1)))
                        {
                            E_Layer[RenderCmd[ReadPoint]].Type                  = RenderCmd[ReadPoint + 1];
                            E_Layer[RenderCmd[ReadPoint]].MixMode               = RenderCmd[ReadPoint + 2];
                            E_Layer[RenderCmd[ReadPoint]].Xpos                  = RenderCmd[ReadPoint + 3];
                            E_Layer[RenderCmd[ReadPoint]].Xpos_Target           = RenderCmd[ReadPoint + 3];
                            E_Layer[RenderCmd[ReadPoint]].Ypos                  = RenderCmd[ReadPoint + 4];
                            E_Layer[RenderCmd[ReadPoint]].Ypos_Target           = RenderCmd[ReadPoint + 4];
                            E_Layer[RenderCmd[ReadPoint]].Alpha                 = RenderCmd[ReadPoint + 5];
                            E_Layer[RenderCmd[ReadPoint]].Alpha_Target          = RenderCmd[ReadPoint + 5];
                            E_Layer[RenderCmd[ReadPoint]].Alpha_Fcn             = RenderCmd[ReadPoint + 6] << 5;
                            E_Layer[RenderCmd[ReadPoint]].Block_Xsize           = RenderCmd[ReadPoint + 7];
                            E_Layer[RenderCmd[ReadPoint]].Block_Xsize_Target    = RenderCmd[ReadPoint + 7];
                            E_Layer[RenderCmd[ReadPoint]].Block_Ysize           = RenderCmd[ReadPoint + 8];
                            E_Layer[RenderCmd[ReadPoint]].Block_Ysize_Target    = RenderCmd[ReadPoint + 8];
                            E_Layer[RenderCmd[ReadPoint]].Block_Corners         = RenderCmd[ReadPoint + 9];
                            ReadPoint += CmdEndIdx_Block+1;
                            ReturnFlag = 0;
                        }
                    } break;
                }
                // ASCII & GB2312
                if((RenderCmd[ReadPoint + 1] == Type_ASCIIchr) || RenderCmd[ReadPoint + 1] == Type_GB2312chr)
                {
                    tmp = 0;
                    for(j = 0 ; j < 13 ; j++)  {tmp += RenderCmd[ReadPoint + j];}
                    if((RenderCmd[ReadPoint + 13] == tmp) && (RenderCmd_Size > (ReadPoint + RenderCmd[ReadPoint + 8] + 14)) && (RenderCmd[ReadPoint + 8] <= Text_ByteLength))
                    {
                        E_Layer[RenderCmd[ReadPoint]].Type                  = RenderCmd[ReadPoint + 1];
                        E_Layer[RenderCmd[ReadPoint]].Flash_Item            = RenderCmd[ReadPoint + 2];
                        E_Layer[RenderCmd[ReadPoint]].MixMode               = RenderCmd[ReadPoint + 3];
                        E_Layer[RenderCmd[ReadPoint]].Xpos                  = RenderCmd[ReadPoint + 4];
                        E_Layer[RenderCmd[ReadPoint]].Xpos_Target           = RenderCmd[ReadPoint + 4];
                        E_Layer[RenderCmd[ReadPoint]].Ypos                  = RenderCmd[ReadPoint + 5];
                        E_Layer[RenderCmd[ReadPoint]].Ypos_Target           = RenderCmd[ReadPoint + 5];
                        E_Layer[RenderCmd[ReadPoint]].Alpha                 = RenderCmd[ReadPoint + 6];
                        E_Layer[RenderCmd[ReadPoint]].Alpha_Target          = RenderCmd[ReadPoint + 6];
                        E_Layer[RenderCmd[ReadPoint]].Alpha_Fcn             = RenderCmd[ReadPoint + 7] << 5;
                        E_Layer[RenderCmd[ReadPoint]].Text_Length           = RenderCmd[ReadPoint + 8];
                        E_Layer[RenderCmd[ReadPoint]].Stream_Length         = (RenderCmd[ReadPoint + 9] << 8) + RenderCmd[ReadPoint + 10];
                        E_Layer[RenderCmd[ReadPoint]].Stream_Attributes     = (RenderCmd[ReadPoint + 11] << 8) + RenderCmd[ReadPoint + 12];
                        for(k = 0 ; k < E_Layer[RenderCmd[ReadPoint]].Text_Length ; k++)
                        {
                            E_Layer[RenderCmd[ReadPoint]].Text[k]           = RenderCmd[ReadPoint + 14 + k];
                        }
                        ReadPoint += CmdEndIdx_Block + 14 + RenderCmd[ReadPoint + 8];
                        ReturnFlag = 0;
                    }
                }
            } break;

            case 0x10:
            {
                tmp = 0;
                for(j = 0 ; j < CmdEndIdx_BaseValue ; j++)  {tmp += RenderCmd[ReadPoint + j];}
                RenderCmd[ReadPoint] = RenderCmd[ReadPoint] & 0x0f;

                if((RenderCmd[ReadPoint + CmdEndIdx_BaseValue] == tmp) && (RenderCmd_Size > (ReadPoint + CmdEndIdx_BaseValue+1)))
                {
                    E_Layer[RenderCmd[ReadPoint]].Xpos_Target   = RenderCmd[ReadPoint + 1];
                    E_Layer[RenderCmd[ReadPoint]].Xpos_Fcn      = RenderCmd[ReadPoint + 2];
                    E_Layer[RenderCmd[ReadPoint]].Ypos_Target   = RenderCmd[ReadPoint + 3];
                    E_Layer[RenderCmd[ReadPoint]].Ypos_Fcn      = RenderCmd[ReadPoint + 4];
                    E_Layer[RenderCmd[ReadPoint]].Alpha_Target  = RenderCmd[ReadPoint + 5];
                    E_Layer[RenderCmd[ReadPoint]].Alpha_Fcn     = RenderCmd[ReadPoint + 6];
                    ReadPoint += CmdEndIdx_BaseValue+1;
                    ReturnFlag = 0;
                }
            } break;

            case 0x20:
            {
                tmp = 0;
                for(j = 0 ; j < CmdEndIdx_StreamSet ; j++)  {tmp += RenderCmd[ReadPoint + j];}
                RenderCmd[ReadPoint] = RenderCmd[ReadPoint] & 0x0f;

                if((RenderCmd[ReadPoint + CmdEndIdx_StreamSet] == tmp) && (RenderCmd_Size > (ReadPoint + CmdEndIdx_StreamSet+1)))
                {
                    E_Layer[RenderCmd[ReadPoint]].Stream_Length     = (RenderCmd[ReadPoint + 1] << 8) + RenderCmd[ReadPoint + 2];
                    E_Layer[RenderCmd[ReadPoint]].Stream_Attributes = (RenderCmd[ReadPoint + 3] << 8) + RenderCmd[ReadPoint + 4];
                    ReadPoint += CmdEndIdx_StreamSet+1;
                    ReturnFlag = 0;
                }
            } break;

            case 0x30:
            {
                tmp = 0;
                for(j = 0 ; j < CmdEndIdx_StreamPause ; j++)  {tmp += RenderCmd[ReadPoint + j];}
                RenderCmd[ReadPoint] = RenderCmd[ReadPoint] & 0x0f;

                if((RenderCmd[ReadPoint + CmdEndIdx_StreamPause] == tmp) && (RenderCmd_Size > (ReadPoint + CmdEndIdx_StreamPause+1)))
                {
                    E_Layer[RenderCmd[ReadPoint]].Stream_PauseFlag = RenderCmd[ReadPoint + 1];
                    ReadPoint += CmdEndIdx_StreamPause+1;
                    ReturnFlag = 0;
                }
            } break;

            case 0x40:
            {
                tmp = 0;
                for(j = 0 ; j < CmdEndIdx_BlockSize ; j++)  {tmp += RenderCmd[ReadPoint + j];}
                RenderCmd[ReadPoint] = RenderCmd[ReadPoint] & 0x0f;

                if(RenderCmd[ReadPoint + CmdEndIdx_BlockSize] == tmp)
                {
                    E_Layer[RenderCmd[ReadPoint]].Block_Xsize_Target    = RenderCmd[ReadPoint + 1];
                    E_Layer[RenderCmd[ReadPoint]].Block_Xsize_Fcn       = RenderCmd[ReadPoint + 2];
                    E_Layer[RenderCmd[ReadPoint]].Block_Ysize_Target    = RenderCmd[ReadPoint + 3];
                    E_Layer[RenderCmd[ReadPoint]].Block_Ysize_Fcn       = RenderCmd[ReadPoint + 4];
                    ReadPoint += CmdEndIdx_BlockSize+1;
                    ReturnFlag = 0;
                }
            } break;
        }
        if(ReturnFlag)  {return;}
    }
    RenderCmd_Sequence = RenderCmd[0];
}

void BackDataSend(void)
{
    uint8_t i = 0, j;

    BackData[i++] = RenderCmd_Sequence;
    BackData[i++] = (uint8_t)(Developer_FrameRenderingTime >> 8);
    BackData[i++] = (uint8_t)Developer_FrameRenderingTime;
    for(j = 0 ; j < E_Layer_Size ; j++)
    {
        BackData[i++] = E_Layer[j].Xpos;
        BackData[i++] = E_Layer[j].Ypos;
        BackData[i++] = E_Layer[j].Alpha;
        BackData[i++] = E_Layer[j].Block_Xsize;
        BackData[i++] = E_Layer[j].Block_Ysize;
    }
    BackData_DMAreset();
}
