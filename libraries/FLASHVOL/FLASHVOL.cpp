/************************************************************************/
/*                                                                      */
/*    RAMVOL.cpp                                                        */
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
/*    10/20/2015(KeithV): Created                                       */
/************************************************************************/
#include "FLASHVOL.h"


/*--------------------------------------------------------------------------

   Public Functions

---------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------*/
/* Get Disk Status                                                       */
/*-----------------------------------------------------------------------*/

DSTATUS FLASHVOL::disk_status(void)
{
    return(_dStatus);
}

/*-----------------------------------------------------------------------*/
/* Initialize Disk Drive                                                 */
/*-----------------------------------------------------------------------*/

DSTATUS FLASHVOL::disk_initialize(void)
{
    _dStatus &= ~STA_NOINIT;
    return(_dStatus);
}

/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT FLASHVOL::disk_read(
	uint8_t *buff,		/* Pointer to the data buffer to store read data */
	uint32_t sector,	/* Start sector number (LBA) */
	uint32_t count		/* Sector count (1..128) */
)
{
    uint8_t const * pStart  = _pFlashVolStart + (sector * _FATFS_CBSECTOR_);
    uint32_t        cbRead  = count * _FATFS_CBSECTOR_;
    uint8_t const * pEnd    = pStart + cbRead;

    if(_dStatus &= STA_NOINIT)
    {
        return(RES_NOTRDY);
    }

    if(pStart < _pFlashVolStart || pStart >= _pFlashVolEnd || pEnd < _pFlashVolStart || pEnd > _pFlashVolEnd)
    {
        return(RES_PARERR);  
    }

    // copy anything before the cache
    if(cbRead > 0 && pStart < _pCacheStart)
    {
        uint32_t  cbCopy = _pCacheStart - pStart;
        cbCopy = cbCopy > cbRead ? cbRead : cbCopy;

        memcpy(buff, pStart, cbCopy);
        cbRead -= cbCopy;
        pStart += cbCopy;
        buff += cbCopy;
    }

    // copy out of the cache
    if(cbRead > 0 && _pCacheStart <= pStart && pStart < _pCacheEnd)
    {
        uint8_t * pCacheCopy = pStart - _pCacheStart + _pCache;
        uint32_t  cbCacheCopy = BYTE_PAGE_SIZE - (pCacheCopy - _pCache);
        cbCacheCopy = cbCacheCopy > cbRead ? cbRead : cbCacheCopy;

        memcpy(buff, pCacheCopy, cbCacheCopy);
        cbRead -= cbCacheCopy;
        pStart += cbCacheCopy;
        buff += cbCacheCopy;
    }

    // and anything after the cache
    if(cbRead > 0)
    {
        memcpy(buff, pStart, cbRead);
    }
    return(RES_OK); 
}

/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

#if _USE_WRITE
DRESULT FLASHVOL::disk_write (
	const uint8_t *buff,		/* Pointer to the data to be written */
	uint32_t sector,			/* Start sector number (LBA) */
	uint32_t count				/* Sector count (1..128) */
)
{
    uint8_t const * pStart  = _pFlashVolStart + (sector * _FATFS_CBSECTOR_);
    uint32_t        cbWrite = count * _FATFS_CBSECTOR_;
    uint8_t const * pEnd    = pStart + cbWrite;

    if(_dStatus &= STA_NOINIT)
    {
        return(RES_NOTRDY);
    }

    if(pStart < _pFlashVolStart || pStart >= _pFlashVolEnd || pEnd < _pFlashVolStart || pEnd > _pFlashVolEnd)
    {
        return(RES_PARERR);  
    }

    // if we skip around, we could get some thrashing.
    while(cbWrite > 0)
    {

        // if we are at the cache, put stuff in the cache
        if(_pCacheStart <= pStart && pStart < _pCacheEnd)
        {
            uint8_t * pCacheCopy = pStart - _pCacheStart + _pCache;
            uint32_t  cbCacheCopy = BYTE_PAGE_SIZE - (pCacheCopy - _pCache);
            cbCacheCopy = cbCacheCopy > cbWrite ? cbWrite : cbCacheCopy;

            memcpy(pCacheCopy, buff, cbCacheCopy);
            cbWrite -= cbCacheCopy;
            pStart += cbCacheCopy;
            buff += cbCacheCopy;
        }

        // if we have more to do flush the cache and load the new page
        if(cbWrite > 0)
        {
            // if we have an active cache page and we are going to need 
            // write back a page 
            if(_pCacheStart != NULL)
            {
                FlashWritePage(_pCacheStart, _pCache);
            }

            // now copy the current page into the cache
            _pCacheStart = (uint8_t *) StartOfFlashPage(pStart);
            _pCacheEnd = _pCacheStart + BYTE_PAGE_SIZE;

            // fill the cache with the page
            memcpy(_pCache, _pCacheStart, BYTE_PAGE_SIZE);
        }
    }

    return(RES_OK); 
}
#endif

/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

#if _USE_IOCTL
DRESULT FLASHVOL::disk_ioctl (
	uint8_t cmd,		/* Control code */
	void *buff		/* Buffer to send/receive data block */
)
{
    switch(cmd)
    {
        /* Flush write-back cache, Wait for end of internal process */
        /* Complete pending write process (needed at _FS_READONLY == 0) */
        case CTRL_SYNC:

            // if we have an active cache page and we are going to need 
            // write back a page 
            if(_pCacheStart != NULL)
            {
                FlashWritePage(_pCacheStart, _pCache);
            }

            // Say nothing is in the cache
            _pCacheStart = NULL;
            _pCacheEnd = NULL;

            return(RES_OK);
            break;

        /* Get media size (needed at _USE_MKFS == 1) */
        /* Get number of sectors on the disk (WORD) */
        case GET_SECTOR_COUNT:
            *(uint32_t*) buff = (_cPages * BYTE_PAGE_SIZE) / _FATFS_CBSECTOR_;
            return(RES_OK);
            break;

        /* Get sector size (needed at _MAX_SS != _MIN_SS) */
        case GET_SECTOR_SIZE:
            *(uint32_t*) buff = _FATFS_CBSECTOR_;
            return(RES_OK);
            break;

        /* Get erase block size in unit of sectors (DWORD) */
        /* Get erase block size (needed at _USE_MKFS == 1) */
        case GET_BLOCK_SIZE:	
            *(uint32_t*) buff = BYTE_PAGE_SIZE / _FATFS_CBSECTOR_;
            return(RES_OK);
           break;

        /* Inform device that the data on the block of sectors is no longer used (needed at _USE_TRIM == 1) */
        case CTRL_TRIM:	
        default:
            return(RES_PARERR);
        break;
    }

    return(RES_PARERR);
}
#endif
