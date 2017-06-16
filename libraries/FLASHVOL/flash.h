/************************************************************************/
/*                                                                      */
/*    PgmFlash.h                                                      */
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

#ifdef __cplusplus
extern "C"{
#endif

//*********************************************************************
//
//                 MX1,2 Page info
//
//*********************************************************************
#if defined(__PIC32MX2XX__) || defined(__PIC32MX2XX__)
    #define BYTE_PAGE_SIZE          1024                // Page size in Bytes

//*********************************************************************
//
//                 MZ Page info
//
//*********************************************************************
#elif defined(__PIC32MZ__)
    #define BYTE_PAGE_SIZE          16384                // Page size in Bytes

//*********************************************************************
//
//                 MX3,4,5,6,7 Page info
//
//*********************************************************************
#else
    #define BYTE_PAGE_SIZE          4096                // Page size in Bytes
#endif

//*********************************************************************
//
//                 Calculate the rest of the page info
//
//*********************************************************************
#define NUM_ROWS_PAGE           8                                   // Number of Rows per Page
#define BYTE_ROW_SIZE           (BYTE_PAGE_SIZE / NUM_ROWS_PAGE)    // Row size in Bytes
#define PAGE_SIZE               (BYTE_PAGE_SIZE / 4)                // # of 32-bit Instructions per Page
#define ROW_SIZE                (BYTE_ROW_SIZE / 4)                 // # of 32-bit Instructions per Row
#define PAGE_ALIGN_MASK         (~(BYTE_PAGE_SIZE - 1))             // align the address to a page boundary (lower)       

//*********************************************************************
//
//                  Macros
//
//*********************************************************************
// convert to a physical address
#define VA2PHY(v)           (((unsigned long) v) & 0x1fffffff)  // you can find this in <sys/kmen.h>
#define VA2KSEG0(v)         ((void *) (VA2PHY(v) | 0x80000000))
#define VA2KSEG1(v)         ((void *) (VA2PHY(v) | 0xA0000000))
#define VA2CACHED(v)        VA2KSEG0(v)
#define VA2NONCACHED(v)     VA2KSEG1(v)

#if defined(__PIC32MZ__)
    #define NVMOP_PROGRAM_ERASE 0x0007                              // Program erase operation
    #define NVMOP_UPFM_ERASE    0x0006                              // upper program flash memory erase
    #define NVMOP_QUADWORD_PGM  0x0002                              // Quad word (128 bit) program operation
#endif

    #define NVMOP_LPFM_ERASE    0x0005                              // program flash memory erase
    #define NVMOP_PAGE_ERASE    0x0004                              // Page erase operation
    #define NVMOP_ROW_PGM       0x0003                              // Row program operation
    #define NVMOP_WORD_PGM      0x0001                              // Word program operation

#define SuspendINT(status) asm volatile ("di %0" : "=r" (status))
#define RestoreINT(status) {if(status & 0x00000001) asm volatile ("ei"); else asm volatile ("di");}

#define ReadK0(dest) __asm__ __volatile__("mfc0 %0,$16" : "=r" (dest))
#define WriteK0(src) __asm__ __volatile__("mtc0 %0,$16" : : "r" (src))
#define K0_UNCACHED 0x00000002
#define K0_CACHED   0x00000003

#define FLASH_START_ADDR        0xBD000000ul
#define MAX_FLASH_ADDR          0xC0000000ul
#define MAX_PAGES               ((MAX_FLASH_ADDR - FLASH_START_ADDR) / BYTE_PAGE_SIZE) 

#define StartOfFlashPage(a) (((uint32_t) (a)) & PAGE_ALIGN_MASK)
#define NextFlashPage(a) (StartOfFlashPage(a) + BYTE_PAGE_SIZE)
#define PageIndex(a) ((StartOfFlashPage(a) - FLASH_START_ADDR) / BYTE_PAGE_SIZE)

//*********************************************************************
//
//                  MZ / MX differences conversions
//
//*********************************************************************
#if defined(__PIC32MZ__)
    #define NVMCON_WREN     _NVMCON_WREN_MASK
    #define NVMCON_WR       _NVMCON_WR_MASK
    #define NVMDATA         NVMDATA0
#endif

//*********************************************************************
//
//                  Forward References
//
//*********************************************************************
extern uint32_t FlashTask(uint32_t nvmop, void const * const pAddr, void const * const pData);
extern uint32_t FlashWritePage(void const * const pAddr, void const * const pData);

#ifdef __cplusplus
} // extern "C"
#endif
