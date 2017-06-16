/************************************************************************/
/*                                                                      */
/*    RAMVOL.h                                                          */
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
/*    12/7/2015(KeithV): Created                                        */
/************************************************************************/
#ifndef _FLASHVOL_INCLUDE_
#define _FLASHVOL_INCLUDE_

#ifdef __cplusplus

#include <stdarg.h>

#include "../DFATFS/DFATFS.h"
#include "flash.h"

// helper defines to help you declare your flash memory correctly
#define _FLASHVOL_CBPAGES(cbFlash)   ((((cbFlash) + BYTE_PAGE_SIZE - 1) / BYTE_PAGE_SIZE) * BYTE_PAGE_SIZE)
#define _FLASHVOL_CBMINFLASH_       _FLASHVOL_CBPAGES(_FATFS_CBMINSECTORS_ * _FATFS_CBSECTOR_)
#define _FLASHVOL_CBFLASH(cbReq)    (_FLASHVOL_CBPAGES(cbReq) > _FLASHVOL_CBMINFLASH_ ? _FLASHVOL_CBPAGES(cbReq) : _FLASHVOL_CBMINFLASH_)
#define DefineFLASHVOL(name, cbSize) const uint8_t __attribute__((aligned(BYTE_PAGE_SIZE))) name[_FLASHVOL_CBFLASH(cbSize)] = { 0xFF }

class FLASHVOL : public DFSVOL
{
    private:

        uint8_t const * const   _pFlashVolStart;
        uint8_t const * const   _pFlashVolEnd;
        uint32_t  const         _cPages;
        uint8_t const *         _pCacheStart;
        uint8_t const *         _pCacheEnd;
        // this is huge and abusive
        // we need a page because we need to do page erases
        // and we do Row programs, which in hardware is a bunch of QWORD programs
        // so we need to be QWORD aligned
        uint8_t                 __attribute__((aligned(16), coherent)) _pCacheData[BYTE_PAGE_SIZE]; 
        uint8_t * const         _pCache;
        DSTATUS                 _dStatus;
 
        // make default constructor illegal to use
        FLASHVOL();

    public:

        static const uint32_t CBMINFLASH    = _FLASHVOL_CBMINFLASH_;

        FLASHVOL(const uint8_t * pFlashVol, uint32_t cbFlashVol) : DFSVOL(1,1), _pFlashVolStart(((uint8_t *) VA2KSEG1(pFlashVol))), _pFlashVolEnd(((uint8_t *) VA2KSEG1(pFlashVol)) + cbFlashVol), _cPages(cbFlashVol/BYTE_PAGE_SIZE), _pCache((uint8_t *) VA2KSEG1(_pCacheData))
        {
            _dStatus = STA_NOINIT; 
            _pCacheStart = NULL;
            _pCacheEnd = NULL;
            memset(_pCache, 0, BYTE_PAGE_SIZE);
        }

        DSTATUS disk_initialize (void);
        DSTATUS disk_status (void);
        DRESULT disk_read (uint8_t* buff, uint32_t sector, uint32_t count);
        DRESULT disk_write (const uint8_t* buff, uint32_t sector, uint32_t count);
        DRESULT disk_ioctl (uint8_t cmd, void* buff);
};
#endif // c++
#endif // _FLASHVOL_INCLUDE_

