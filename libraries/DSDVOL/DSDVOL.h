/************************************************************************/
/*                                                                      */
/*    DSDVOL.h                                                          */
/*                                                                      */
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
/*    10/19/2015(KeithV): Created                                       */
/************************************************************************/
#ifndef _DSDVOL_INCLUDE_
#define _DSDVOL_INCLUDE_

#ifdef __cplusplus

#include "../DFATFS/DFATFS.h"
#include "../DSPI/DSPI.h"           // required for the interface DGSPI
                                    // if board_Defs uses a macro to define a SoftSPI
#define sdSpiFast           4000000
#define sdSpiSlow           400000

#define	FCLK_SLOW()		dSDspi.setSpeed(sdSpiSlow)	/* Set slow clock (100k-400k) */
#define	FCLK_FAST()		dSDspi.setSpeed(sdSpiFast)	/* Set fast clock (depends on the CSD) */

/*--------------------------------------------------------------------------

   MMC/SDC commands

---------------------------------------------------------------------------*/

/* Definitions for MMC/SDC command */
#define CMD0   (0)			/* GO_IDLE_STATE */
#define CMD1   (1)			/* SEND_OP_COND */
#define ACMD41 (41|0x80)	/* SEND_OP_COND (SDC) */
#define CMD8   (8)			/* SEND_IF_COND */
#define CMD9   (9)			/* SEND_CSD */
#define CMD10  (10)			/* SEND_CID */
#define CMD12  (12)			/* STOP_TRANSMISSION */
#define ACMD13 (13|0x80)	/* SD_STATUS (SDC) */
#define CMD16  (16)			/* SET_BLOCKLEN */
#define CMD17  (17)			/* READ_SINGLE_BLOCK */
#define CMD18  (18)			/* READ_MULTIPLE_BLOCK */
#define CMD23  (23)			/* SET_BLOCK_COUNT */
#define ACMD23 (23|0x80)	/* SET_WR_BLK_ERASE_COUNT (SDC) */
#define CMD24  (24)			/* WRITE_BLOCK */
#define CMD25  (25)			/* WRITE_MULTIPLE_BLOCK */
#define CMD41  (41)			/* SEND_OP_COND (ACMD) */
#define CMD55  (55)			/* APP_CMD */
#define CMD58  (58)			/* READ_OCR */

// Timer definitions
#define tmsDefineTimer(a) uint32_t tStart##a, tTimeout##a
#define tmsSetTimer(a) tStart##a = _ReadCoreTimerMS(); tTimeout##a
#define tmsTimer(a) ((_ReadCoreTimerMS() - tStart##a) < tTimeout##a)

class DSDVOL : public DFSVOL
{
    private:
	    static const uint8_t	pinPullupNone = 0xFF;		// if no pullup is needed on the pin

        // point to either a softSPI or a DSPI method
        DGSPI&              dSDspi;

        // variables used by the MMC/SD
        volatile DSTATUS    Stat;	
        uint32_t            CardType;
        tmsDefineTimer(1);
        tmsDefineTimer(2);

        // make default constructor illegal to use
        DSDVOL();
        
        uint32_t inline _ReadCoreTimerMS(void)
        {
            const uint32_t cTickPerMillisecond = F_CPU / 2000ul;
            uint32_t coreTimerCount;
            
            __asm__ __volatile__("mfc0 %0,$9" : "=r" (coreTimerCount));
            
            return((coreTimerCount + (cTickPerMillisecond/2)) / cTickPerMillisecond);
        }

        // internal SD implementation
        void power_on (void);
        void power_off (void);
        BYTE xchg_spi (uint8_t bSnd)
        {
            return(dSDspi.transfer(bSnd));
        }
        void xmit_spi_multi (const uint8_t* buff,	UINT cnt)
        {
            dSDspi.transfer(cnt, (uint8_t *) buff);
        }
        void rcvr_spi_multi (uint8_t* buff, uint32_t cnt)
        {
            dSDspi.transfer(cnt, 0xFF, (uint8_t *) buff);
        }
        int wait_ready (void);
        void deselect (void);
        int select (void);
        int rcvr_datablock (uint8_t *buff,	uint32_t btr);
        int xmit_datablock (const uint8_t *buff, uint8_t token);
        uint8_t send_cmd (uint8_t cmd, uint32_t arg);

    public:

        DSDVOL(DGSPI& dspi) : DFSVOL(0,1), dSDspi(dspi), Stat(STA_NOINIT) {}

        DSTATUS disk_initialize (void);
        DSTATUS disk_status (void);
        DRESULT disk_read (uint8_t* buff, uint32_t sector, uint32_t count);
        DRESULT disk_write (const uint8_t* buff, uint32_t sector, uint32_t count);
        DRESULT disk_ioctl (uint8_t cmd, void* buff);
};

#endif // c++
#endif // _DSDVOL_INCLUDE_

