#ifndef YUERAN_Define_H
#define YUERAN_Define_H

#include "ch32v20x.h"

// FPS
#define TIM_FPS         50

// Type
#define Type_Disable    0
#define Type_Picture    1
#define Type_ASCIIchr   2
#define Type_GB2312chr  3
#define Type_DrawBlock  4

// Size INFO
#define E_Layer_Size        16
#define FRAME_PageSize      8
#define FRAME_ListSize      128
#define E_Size              1024
#define Text_ByteLength     32

// Flash index
#define FlashIndex_Size     16
#define FlashIndex_Quantity 128

#define FlsIdx_p10_Name		0
#define FlsIdx_p2_DataSize	10
#define FlsIdx_p2_Add		12
#define FlsIdx_p1_Xsize		14
#define FlsIdx_p1_Ysize		15



#define USART_BUFFER_HeadFlag_p     0
#define USART_BUFFER_ByteLength_p   1
#define USART_BUFFER_CmdNum_p       2
#define USART_BUFFER_CmdData_p      3

#define USART_BUFFER_Size           256
#define USART_BackData_Size         256

#define GlobalFlag_WaitRendering	0x80
#define GlobalFlag_TimeFull         0x40
#define GlobalFlag_FrameClr		    0x20
#define GlobalFlag_USART_CmdIn      0x10
#define GlobalFlag_USART_CmdFn      0x08

#define Pipeline_AlphaE     0x80

typedef struct
{
    /**
     * @brief   Type of layer.
     * 
     * @param   Type_Disable
     *          Type_Picture
     *          Type_ASCIIchr
     *          Type_GB2312chr
     *          Type_DrawBlock
     */
    uint8_t Type;

    uint8_t PipelineFlag;


    // Element X size
    uint8_t Xsize;
    // Element Y size (Page)
    uint8_t Ysize;

    /**
     * @brief   Select mode for mixing in EtoFRAME
     * 
     * @param   MixMode_Coverage
     *          MixMode_OR
     *          MixMode_AND
     *          MixMode_NON
     *          MixMode_ExclusiveOR
     */
    uint8_t MixMode;

    int8_t Xpos;
    int8_t Xpos_Target;
    // D7~6 select mode, D5~0 value
    uint8_t Xpos_Fcn;
    // X pos SDstep Record
    uint8_t Xpos_Fcn_SDstep;
    // X pos SUstep Record
    uint8_t Xpos_Fcn_SUstep;

    int8_t Ypos;
    int8_t Ypos_Target;
    // D7~6 select mode, D5~0 speed value
    uint8_t Ypos_Fcn;
    // Y pos SDstep Record
    uint8_t Ypos_Fcn_SDstep;
    // Y pos SUstep Record
    uint8_t Ypos_Fcn_SUstep;

    uint8_t Alpha;
    uint8_t Alpha_Target;
    // D7~5 select mode,  D4 uniform-speed/#nonstop, D3~0 speed value
    uint8_t Alpha_Fcn;


	uint8_t Block_Xsize;
	uint8_t Block_Xsize_Target;
	// D7~6 select mode, D5~0 speed value
	uint8_t Block_Xsize_Fcn;
	// X size SDstep Record
    uint8_t Block_Xsize_Fcn_SDstep;
	// X size SUstep Record
    uint8_t Block_Xsize_Fcn_SUstep;
	
	uint8_t Block_Ysize;
	uint8_t Block_Ysize_Target;
	// D7~6 select mode, D5~0 speed value
	uint8_t Block_Ysize_Fcn;
	// Y size SDstep Record
    uint8_t Block_Ysize_Fcn_SDstep;
	// Y size SUstep Record
    uint8_t Block_Ysize_Fcn_SUstep;
	
    /**
     * @brief   D7 beveled corners / # rounded corners, D6~0 value
     * 
     * @param   Corners  |   value
     *          -------------------------
     *          beveled  |   0~8 (|0x80)
     *          rounded  |   0~6
     */
    uint8_t Block_Corners;

    uint8_t Flash_Item;

    uint16_t Picture_Add;

    uint8_t Text[Text_ByteLength];
    uint8_t Text_Length;
    


    int32_t StreamAdd;
    int32_t StreamAdd_Target;
    // D7~6 select mode, D5~0 speed value
    uint8_t StreamAdd_Fcn;
    uint8_t StreamAdd_Fcn_SUstep;
    // D15 direction, D14 circulation, D13~7 step-length, D6~0 frame-freeze-value
    uint16_t Stream_Attributes;
    // D7 Stream pause flag, D6~0 frame-freeze-value count.
    uint8_t Stream_Pause;
    // Stream offset value
    int32_t StreamOffset;
} Element;

extern Element E_Layer[E_Layer_Size];
extern uint8_t FRAME[FRAME_PageSize][FRAME_ListSize];
extern uint8_t E[E_Size];
extern uint16_t RenderingTime;

extern uint8_t FlashIndex[FlashIndex_Quantity * FlashIndex_Size];
extern uint8_t FlashIndex_CNT;

extern uint8_t TEST_VALUE[16];

extern volatile uint8_t USART_BUFFER[USART_BUFFER_Size];
extern volatile uint8_t USART_BUFFER_CNT;
extern volatile uint8_t USART_BUFFER_TomeoutCNT;
extern uint8_t USART_BackData[USART_BackData_Size];
extern uint8_t BackData_Head;

extern volatile uint8_t GlobalFlag;

void NVIC_Interrupt_GroupInit(void);

#endif
