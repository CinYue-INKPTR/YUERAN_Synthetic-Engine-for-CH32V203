#ifndef YUERAN_Define_H
#define YUERAN_Define_H

#include <stdint.h>

#define Type_Picture    1
#define Type_ASCIIchr   2
#define Type_GB2312chr  3
#define Type_DrawBlock  4

#define Text_ByteLength     32


// Size INFO
#define E_Layer_Size        8
#define FRAME_PageSize      8
#define FRAME_ListSize      128
#define E_Size              1024

#define RenderCmd_Size      64
#define BackData_Size       43      // Sequence marking and Xpos, Ypos, Alpha, Block_Xsize, Block_Ysize (for 8 layers)
#define USART2_BUFFER_Size  256


typedef struct
{
    // Value    |   Status
    // --------------------------
    // 0        |   Layer disable
    // 1        |   Picture
    // 2        |   Char ASCII
    // 3        |   Char GB2312
    // 4        |   Draw Block
    uint8_t Type;


    // X size for text & picture
    uint8_t Xsize;
    // Y size for text & picture
    uint8_t Ysize;

    // Select mode for mixing in EtoFRAME
    uint8_t MixMode;

    // X position
    int8_t Xpos;
    // X position target
    int8_t Xpos_Target;
    // X position funtion, D7~6 select mode, D5~0 speed value for slow
    uint8_t Xpos_Fcn;
    // X pos SUstep Record
    uint8_t Xpos_Fcn_SUstep;


    // Y position
    int8_t Ypos;
    // Y position target
    int8_t Ypos_Target;
    // Y position function, D7~6 select mode, D5~0 speed value for slow
    uint8_t Ypos_Fcn;
    // Y pos SUstep Record
    uint8_t Ypos_Fcn_SUstep;

    // Alpha
    uint8_t Alpha;
    // Alpha target
    uint8_t Alpha_Target;
    // Alpha function, D7~5 select mode,  D4 uniform-speed/#nonstop, D3~0 speed value for slow
    uint8_t Alpha_Fcn;


	// X size for block
	uint8_t Block_Xsize;
	// X size target for block
	uint8_t Block_Xsize_Target;
	// X size function for block, D7~6 select mode, D5~0 speed value for slow
	uint8_t Block_Xsize_Fcn;
	// X size SUstep Record
    uint8_t Block_Xsize_Fcn_SUstep;
	
	// Y size for block
	uint8_t Block_Ysize;
	// Y size target for block
	uint8_t Block_Ysize_Target;
	// Y size function for block, D7~6 select mode, D5~0 speed value for slow
	uint8_t Block_Ysize_Fcn;
	// Y size SUstep Record
    uint8_t Block_Ysize_Fcn_SUstep;
	
	// For block, D7 beveled corners / # rounded corners, D6~0 value
	// Corners  |   value
	// ----------------------------------
	// beveled  |   0~8 (|0x80)
	// rounded  |   0~6
    uint8_t Block_Corners;


    // Material item
    uint8_t Flash_Item;

    // Picture Add
    uint16_t Picture_Add;

    // Text content
    uint8_t Text[Text_ByteLength];
    // For text
    uint8_t Text_Length;
    

    // Stream length, disable with value 0
    uint16_t Stream_Length;
    // D15 direction, D14 circulation, D13~7 step-length, D6~0 frame-freeze-value
    uint16_t Stream_Attributes;

    // Stream record
    uint16_t Stream_AddRecord;
    // Stream pause flag
    uint8_t Stream_PauseFlag;
} Element;

extern Element E_Layer[E_Layer_Size];
extern uint8_t FRAME[FRAME_PageSize][FRAME_ListSize];
extern uint8_t E[E_Size];
extern uint8_t FRAME_ClnFlag;

extern uint8_t RenderCmd[RenderCmd_Size];
extern uint8_t RenderCmd_Sequence;
extern uint8_t BackData[BackData_Size];

extern uint8_t USART2_BUFFER[USART2_BUFFER_Size];
extern uint16_t USART2_RxLen;
extern uint8_t USART2_RxFlag;

extern const uint8_t B_Corners_Mask[16][8];
extern const uint8_t R_Corners_Mask[12][8];

extern uint16_t Developer_FrameRenderingTime;

#endif
