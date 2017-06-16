/************************************************************************/
/*                                                                      */
/*    PgmFlash.cpp                                                      */
/*                                                                      */
/************************************************************************/
/*    Author:     Keith Vogel                                           */
/*    Copyright 2015, Digilent Inc.                                     */
/************************************************************************/
/* 
*
* Copyright (c) 2015, Digilent <www.digilentinc.com>
* Contact Digilent for the latest version.
*
* This program is free software; distributed under the terms of 
* BSD 3-clause license ("Revised BSD License", "New BSD License", or "Modified BSD License")
*
* Redistribution and use in source and binary forms, with or without modification,
* are permitted provided that the following conditions are met:
*
* 1.    Redistributions of source code must retain the above copyright notice, this
*        list of conditions and the following disclaimer.
* 2.    Redistributions in binary form must reproduce the above copyright notice,
*        this list of conditions and the following disclaimer in the documentation
*        and/or other materials provided with the distribution.
* 3.    Neither the name(s) of the above-listed copyright holder(s) nor the names
*        of its contributors may be used to endorse or promote products derived
*        from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
* ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
* IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
* INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
* BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
* OF THE POSSIBILITY OF SUCH DAMAGE.
*/
/************************************************************************/
/*  Revision History:                                                   */
/*                                                                      */
/*    12/11/2015(KeithV): Created                                       */
/************************************************************************/
#include "p32xxxx.h"
#include <inttypes.h>
#include <string.h>
#include <stdbool.h>
#include "flash.h"

/***    void flashOperation(uint32 nvmop, uint32 addr, uint32 data)
**
**    Synopsis:   
**      Performs either a page erase, word write, or row write
**
**    Parameters:
**      nvmop    either NVMOP_PAGE_ERASE, NVMOP_WORD_PGM, or NVMOP_ROW_PGM
**        pAddr    A pointer to the flash location, VA or PHY
**        pData    a pointer to the data to write to flash
**
**    Return Values:
**      True if successful, false if failed
**
**    Errors:
**      None
**
**  Notes:
**      data has no meaning when page erase is specified and should be set to 0ul
**
*/
uint32_t FlashTask(uint32_t nvmop, void const * const pAddr, void const * const pData)
{
    unsigned long   t0;
    unsigned int    status;

// errata (#71) on the 320/340/360/420/440/
#if defined(_CHECON_PREFEN_MASK)
        unsigned long   K0;
        unsigned long   PFEN = CHECON & _CHECON_PREFEN_MASK;
#endif

    // Convert Address to Physical Address
    NVMADDR = VA2PHY(pAddr);

    switch(nvmop)
    {
        case NVMOP_WORD_PGM:
            NVMDATA = ((uint32_t const * const) pData)[0];
            break;

        case NVMOP_ROW_PGM:
            NVMSRCADDR = VA2PHY(pData);
            break;

#if defined(__PIC32MZ__)
        case NVMOP_QUADWORD_PGM:
            NVMDATA0 = ((uint32_t const * const) pData)[0];
            NVMDATA1 = ((uint32_t const * const) pData)[1];
            NVMDATA2 = ((uint32_t const * const) pData)[2];
            NVMDATA3 = ((uint32_t const * const) pData)[3];
            break;

        case NVMOP_PROGRAM_ERASE:
        case NVMOP_UPFM_ERASE:
#endif
        case NVMOP_LPFM_ERASE:
        case NVMOP_PAGE_ERASE:
            break;

        default:
            return(false);
            break;
    }

    // Suspend or Disable all Interrupts
    // errata (#43) on MX
    SuspendINT(status);

// errata (#71) on the 320/340/360/420/440/
#if defined(_CHECON_PREFEN_MASK)
    // disable predictive prefetching, see errata
    CHECONCLR = _CHECON_PREFEN_MASK;

    // turn off caching, see errata
    ReadK0(K0);
    WriteK0((K0 & ~0x07) | K0_UNCACHED);
#endif

 	// Enable Flash Write/Erase Operations
    NVMCON = NVMCON_WREN | nvmop;

    // this is a poorly documented yet very important
    // required delay on newer silicon.
    // If you do not delay, on some silicon it will
    // completely latch up the flash to where you need
    // to cycle power, so wait for at least
    // 6us for LVD start-up, see errata
    t0 = _CP0_GET_COUNT();
    while (_CP0_GET_COUNT() - t0 < ((6 * F_CPU) / 2 / 1000000UL));

    // magic unlock sequence
    NVMKEY = 0xAA996655;
    NVMKEY = 0x556699AA;
    NVMCONSET = NVMCON_WR;

    // Wait for WR bit to clear
    while (NVMCON & NVMCON_WR);

    // see errata, wait 500ns before writing to any NVM register
    t0 = _CP0_GET_COUNT();
    while (_CP0_GET_COUNT() - t0 < ((F_CPU / 2 / 2000000UL)));

    // Disable Flash Write/Erase operations
    NVMCONCLR = NVMCON_WREN;

    #if defined(_CHECON_PREFEN_MASK)
        // restore predictive prefetching and caching, see errata
        WriteK0(K0);
        CHECONSET = PFEN;
    #endif

    // Restore Interrupts
    // errata (#43) on MX
    RestoreINT(status);

    // assert no errors
    return(! (NVMCON & (_NVMCON_WRERR_MASK | _NVMCON_LVDERR_MASK)));
}

uint32_t FlashWritePage(void const * const pAddr, void const * const pData)
{
    int i = 0;

    if(memcmp(pAddr, pData, BYTE_PAGE_SIZE) != 0)
    {
        // erase the flash
        if(!FlashTask(NVMOP_PAGE_ERASE, pAddr, pData))
        {
            return(false);
        }

        // write the flash by rows.
        for(i = 0; i < NUM_ROWS_PAGE; i++)
        {
            if(!FlashTask(NVMOP_ROW_PGM, &(((uint8_t *) pAddr)[i*BYTE_ROW_SIZE]), &(((uint8_t *) pData)[i*BYTE_ROW_SIZE])))
            {
                return(false);
            }
        }


// invalidate the flash cache
#if defined(_CHECON_PREFEN_POSITION)
        CHECONbits.CHECOH = 1;
#elif defined(_PRECON_PREFEN_POSITION)
        {
            uint32_t curPrefEN = PRECONbits.PREFEN;
            PRECONbits.PREFEN = 0;
            PRECONbits.PREFEN = curPrefEN;
        }
#endif
    }

    return(true);
}
