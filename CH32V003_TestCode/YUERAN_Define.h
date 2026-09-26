#ifndef YUERAN_Define_H
#define YUERAN_Define_H

#include <stdint.h>

//
#define Type_Picture        0
// Xsize, Ysize, MixMode, Xpos, Ypos, Alpha, Text_Font, Text[], Text_Length, Stream_Length, Stream_Attributes
#define Type_Char_ASCII     1
// Xsize, Ysize, MixMode, Xpos, Ypos, Alpha, Text_Font, Text[], Text_Length, Stream_Length, Stream_Attributes
#define Type_Char_GB2312	2
// MixMode, Xpos, Ypos, Alpha, Block_Xsize, Block_Ysize, Block_Corners
#define Type_DrawBlock      3

#define Alpha_Mode0         0
#define Alpha_Mode1         (1 << 5)
#define Alpha_Mode2         (2 << 5)
#define Alpha_Mode3         (3 << 5)
#define Alpha_Mode4         (4 << 5)

#define Text_ByteLength     32


// Size INFO
#define E_Layer_Size        3

#define FRAME_PageSize      4
#define FRAME_ListSize      128

#define E_Size              256


typedef struct
{
    // Element type     |   Status
    // ----------------------------------------------------------------------------------------------------------------------------
    // Type_Picture     |   Xsize, Ysize, MixMode, Xpos, Ypos, Alpha, Picture_FlashAdd, Stream_Length, Stream_Attributes
    // Type_Char_ASCII  |   Xsize, Ysize, MixMode, Xpos, Ypos, Alpha, Text_Font, Text[], Text_Length, Stream_Length, Stream_Attributes
    // Type_Char_GB2312 |   Xsize, Ysize, MixMode, Xpos, Ypos, Alpha, Text_Font, Text[], Text_Length, Stream_Length, Stream_Attributes
    // Type_DrawBlock   |   MixMode, Xpos, Ypos, Alpha, Alpha_Fcn, Block_Xsize, Block_Ysize, Block_Corners
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

    // Y position
    int8_t Ypos;
    // Y position target
    int8_t Ypos_Target;
    // Y position function, D7~6 select mode, D5~0 speed value for slow
    uint8_t Ypos_Fcn;

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
	
	// Y size for block
	uint8_t Block_Ysize;
	// Y size target for block
	uint8_t Block_Ysize_Target;
	// Y size function for block, D7~6 select mode, D5~0 speed value for slow
	uint8_t Block_Ysize_Fcn;
	
	// For block, D7 beveled corners / # rounded corners, D6~0 value
	// Corners  |   value
	// ----------------------------------
	// beveled  |   0~8 (|0x80)
	// rounded  |   0~6
    uint8_t Block_Corners;



    // Flash address for picture
    uint32_t Picture_FlashAdd;

    // Text font
    uint8_t Text_Font;
    // Text content
    uint8_t Text[Text_ByteLength];
    // For text
    uint8_t Text_Length;
    

    // Stream length, disable with value 0
    uint16_t Stream_Length;
    // D15 direction, D14 circulation, D13~7 step-length, D6~0 frame-freeze-value
    uint16_t Stream_Attributes;
} Element;

extern Element E_Layer[E_Layer_Size];
extern uint8_t FRAME[FRAME_PageSize][FRAME_ListSize];
extern uint8_t E[E_Size];

extern const uint8_t B_Corners_Mask[16][8];
extern const uint8_t R_Corners_Mask[12][8];

#endif
