#include <stdint.h>
#include "YUERAN_AlphaE.h"
#include "YUERAN_Define.h"

// ?? E ???? Alpha ????
void AlphaE(uint8_t Layer)
{
    uint16_t i, E_DrawLength;
    E_DrawLength = E_Layer[Layer].Xsize * E_Layer[Layer].Ysize;

    // ? Alpha ??????????
    switch(E_Layer[Layer].Alpha_Fcn >> 5)
    {
        case 0:
        {
            // ??? Alpha ????? E ????????
            switch(E_Layer[Layer].Alpha)
            {
                case 0:
                {
                    for(i = 0 ; i < E_DrawLength ; i++)
                    {
                        // ????????????
                        E[i] = 0;
                    }
                } break;


                case 1:
                {
                    for(i = 0 ; i < E_DrawLength ; i++)
                    {
                        // ????????????
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
                        // ????????????
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
                        // ????????????
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
                        // ????????????
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
                        // ????????????
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
                        // ????????????
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
                        // ????????????
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
                        // ????????????
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
                        // ????????????
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