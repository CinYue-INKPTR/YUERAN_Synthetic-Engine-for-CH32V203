#include "YUERAN_Backstage.h"

#include "ch32v20x.h"
#include "YUERAN_Define.h"
#include "HardSPI_W25Qxx.h"
#include "USART.h"

static const uint8_t BkSt_INFO[] = "INFO";
static const uint8_t BkSt_ClearChip[] = "CCC";

static const uint8_t BkSt_DONE[] = {0xab, 0xcd, 0xef};




void Backstage(void)
{
    uint8_t Check;

    if (!(GlobalFlag & GlobalFlag_USART_CmdFn))    {return;}
    GlobalFlag &= ~GlobalFlag_USART_CmdFn;

    switch (USART_BUFFER[USART_BUFFER_CmdData_p])
    {
        // INFO
        case 'I':
        {
            uint8_t i;
            uint16_t tmp;

            Check = 1;
            for (i = 0 ; i < 4 ; i++)
            {
                if (USART_BUFFER[USART_BUFFER_CmdData_p + i] != BkSt_INFO[i])    {Check = 0;}
            }

            if (Check)
            {
                USART2_SendData((uint8_t*)USART_BUFFER, 4);

                tmp = W25Qxx_GetSectorQuantity();
                USART_BackData[0] = (uint8_t)(tmp >> 8);
                USART_BackData[1] = (uint8_t)tmp;
                USART2_SendData(USART_BackData, 2);

                USART2_SendData((uint8_t*)BkSt_DONE, sizeof(BkSt_DONE));
            }
        } break;



        // 'R' + 3-Bytes ByteAddr + 2-Bytes ByteLength + Check
        case 'R':
        {
            uint8_t i;
            uint16_t Length;
            uint32_t Addr;

            Check = 0;
            for (i = 0 ; i < 6 ; i++)   {Check += USART_BUFFER[USART_BUFFER_CmdData_p + i];}

            if (USART_BUFFER[USART_BUFFER_CmdData_p + i] == Check)
            {
                USART2_SendData((uint8_t*)USART_BUFFER, 4);

                i = USART_BUFFER_CmdData_p + 1;
                Addr = (uint32_t)USART_BUFFER[i++] << 16;
                Addr |= (uint32_t)USART_BUFFER[i++] << 8;
                Addr |= (uint32_t)USART_BUFFER[i++];
                Length = (uint16_t)USART_BUFFER[i++] << 8;
                Length |= (uint16_t)USART_BUFFER[i++];

                while (Length)
                {
                    if(Length > USART_BackData_Size)
                    {
                        Length -= USART_BackData_Size;
                        W25Qxx_ReadData(Addr, USART_BackData, USART_BackData_Size);
                        USART2_SendData(USART_BackData, USART_BackData_Size);
                        Addr += USART_BackData_Size;
                    }
                    else
                    {
                        W25Qxx_ReadData(Addr, USART_BackData, Length);
                        USART2_SendData(USART_BackData, Length);
                        Length = 0;
                    }
                }
            }
        } break;



        // 'W' + 3-Bytes ByteAddr + 1-Bytes ByteLength + Check + DataBytes...
        case 'W':
        {
            uint8_t i, Length;
            uint32_t Addr;

            Check = 0;
            for (i = 0 ; i < 5 ; i++)   {Check += USART_BUFFER[USART_BUFFER_CmdData_p + i];}

            if (USART_BUFFER[USART_BUFFER_CmdData_p + i] == Check)
            {
                USART2_SendData((uint8_t*)USART_BUFFER, 4);
                
                i = USART_BUFFER_CmdData_p + 1;
                Addr = (uint32_t)USART_BUFFER[i++] << 16;
                Addr |= (uint32_t)USART_BUFFER[i++] << 8;
                Addr |= (uint32_t)USART_BUFFER[i++];
                Length = USART_BUFFER[i++];

                if(Length > (255 - USART_BUFFER_CmdData_p - 6))    {Length = (255 - USART_BUFFER_CmdData_p - 6);}

                for (i = 0 ; i < Length ; i++)  {USART_BUFFER[USART_BUFFER_CmdData_p + i] = USART_BUFFER[USART_BUFFER_CmdData_p + i+6];}
                while (W25Qxx_ReadBusy() & W25Qxx_Flag_Busy)   {Delay_Ms(1);}
                W25Qxx_WriteEnable(ENABLE);
                while (W25Qxx_ReadBusy() & W25Qxx_Flag_Busy)   {Delay_Ms(1);}
                W25Qxx_Program(Addr, (uint8_t*)&USART_BUFFER[USART_BUFFER_CmdData_p], Length);
                while (W25Qxx_ReadBusy() & W25Qxx_Flag_Busy)   {Delay_Ms(1);}
                W25Qxx_WriteEnable(DISABLE);

                USART2_SendData((uint8_t*)BkSt_DONE, sizeof(BkSt_DONE));
            }
        } break;



        case 'C':
        {
            uint8_t i;

            Check = 1;
            for (i = 0 ; i < 3 ; i++)
            {
                if (USART_BUFFER[USART_BUFFER_CmdData_p + i] != BkSt_ClearChip[i])  {Check = 0;}
            }

            if (Check)
            {
                USART2_SendData((uint8_t*)USART_BUFFER, 4);

                while (W25Qxx_ReadBusy() & W25Qxx_Flag_Busy)   {Delay_Ms(1);}
                W25Qxx_WriteEnable(ENABLE);
                while (W25Qxx_ReadBusy() & W25Qxx_Flag_Busy)   {Delay_Ms(1);}
                W25Qxx_Erase_Chip();
                while (W25Qxx_ReadBusy() & W25Qxx_Flag_Busy)   {Delay_Ms(1);}
                W25Qxx_WriteEnable(DISABLE);

                USART2_SendData((uint8_t*)BkSt_DONE, sizeof(BkSt_DONE));
            }
        } break;



        // 'D' + 2-Bytes SectorAddr + 2-Bytes SectorLength + Check
        case 'D':
        {
            uint8_t i;
            uint16_t Addr, Length;

            Check = 0;
            for (i = 0 ; i < 5 ; i++)   {Check += USART_BUFFER[USART_BUFFER_CmdData_p + i];}

            if (USART_BUFFER[USART_BUFFER_CmdData_p + i] == Check)
            {
                USART2_SendData((uint8_t*)USART_BUFFER, 4);

                i = USART_BUFFER_CmdData_p + 1;
                Addr = (uint16_t)USART_BUFFER[i++] << 8;
                Addr |= (uint16_t)USART_BUFFER[i++];
                Length = (uint16_t)USART_BUFFER[i++] << 8;
                Length |= (uint16_t)USART_BUFFER[i++];

                while(Length)
                {
                    if((Length >= W25Qxx_Block_SectorNum) && (!(Addr % W25Qxx_Block_SectorNum)))
                    {
                        while (W25Qxx_ReadBusy() & W25Qxx_Flag_Busy)   {Delay_Ms(1);}
                        W25Qxx_WriteEnable(ENABLE);
                        Length -= W25Qxx_Block_SectorNum;
                        while (W25Qxx_ReadBusy() & W25Qxx_Flag_Busy)   {Delay_Ms(1);}
                        W25Qxx_Erase_Block((uint32_t)Addr);
                        Addr += W25Qxx_Block_SectorNum;
                    }
                    else
                    {
                        while (W25Qxx_ReadBusy() & W25Qxx_Flag_Busy)   {Delay_Ms(1);}
                        W25Qxx_WriteEnable(ENABLE);
                        Length--;
                        while (W25Qxx_ReadBusy() & W25Qxx_Flag_Busy)   {Delay_Ms(1);}
                        W25Qxx_Erase_Sector((uint32_t)Addr);
                        Addr++;
                    }
                }
                while (W25Qxx_ReadBusy() == W25Qxx_Flag_Busy)   {Delay_Ms(1);}
                W25Qxx_WriteEnable(DISABLE);

                USART2_SendData((uint8_t*)BkSt_DONE, sizeof(BkSt_DONE));
            }
        } break;
    }
    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);
}
