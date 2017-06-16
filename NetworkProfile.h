/************************************************************************/
/*																		*/
/*	NetworkProfile.x                                                    */
/*																		*/
/*	Network Hardware vector file for the MRF24WG PmodWiFi               */
/*																		*/
/************************************************************************/
/*	Author: 	Keith Vogel 											*/
/*	Copyright 2013, Digilent Inc.										*/
/************************************************************************/
/* 
*
* Copyright (c) 2013-2014, Digilent <www.digilentinc.com>
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
/*  Revision History:													*/
/*																		*/
/*	0/16/2013 (KeithV): Created											*/
/*																		*/
/************************************************************************/

#ifndef PMODWIFI_NETWORK_PROFILE_X
#define PMODWIFI_NETWORK_PROFILE_X

#define MRF24WG 

#include <p32xxxx.h>

#define DWIFIcK_WiFi_Hardware
#define DNETcK_Network_Hardware
#define DWIFIcK_PmodWiFi
#define USING_WIFI 1

//**************************************************************************
//**************************************************************************
//******************* Defines for the WiFi Module  *************************
//**************************************************************************
//**************************************************************************

#define _MRF24_SPI_CONFIG_

#define WF_INT              3
#define WF_SPI              4
#define WF_SPI_FREQ         20000000
#define WF_IPL_ISR          IPL3SRS
#define WF_IPL              3
#define WF_SUB_IPL          0

#define WF_INT_TRIS         (TRISGbits.TRISG8)
#define WF_INT_IO           (PORTGbits.RG8)

#define WF_HIBERNATE_TRIS   (TRISDbits.TRISD13)
#define	WF_HIBERNATE_IO     (PORTDbits.RD13)

#define WF_RESET_TRIS       (TRISAbits.TRISA4)
#define WF_RESET_IO         (LATAbits.LATA4)

#define WF_CS_TRIS          (TRISBbits.TRISB15)

// for PPS devices
#define WF_INT_PPS()    INT3R   = 0b0001    // INT3     G8      INT3R = 0b0001
#define WF_HIB_PPS()    // RPD13R  = 0b0000    // HIB      D13     GPIO
#define WF_RESET_PPS()  // RPA4R   = 0b0000    // RESET    A4      GPIO
#define WF_SCK_PPS()    RPD10R  = 0b0000    // SCK4     RD10    GPIO
#define WF_SDI_PPS()    SDI4R   = 0b0001    // SDI4     RG7     SDI4R = 0b0001
#define WF_SDO_PPS()    RPA15R  = 0b1000    // SDO4     RA15    RPA15R = 0b1000

#if defined(NO_IO_BUS)
    #define WF_CS_IO        (U4STAbits.UTXINV)
    #define WF_CS_PPS()     RPB15R  = 0b0010    // CS       B15     U4TX
    #define WF_INTA_PPS()   INT3R   = 0b1100    // INT3     G1      INT3R = 0b1100
#else
    #define WF_CS_IO        (LATBbits.LATB15)
    #define WF_CS_PPS()     RPB15R  = 0b0000    // CS       B15     GPIO
#endif

// board specific stuff
#if defined(_MRF24_SPI_CONFIG_)

    #include <./libraries/MRF24G/SPIandINT.h>

#elif defined (_BOARD_FUBARINO_SD_)

    #include <FubarSD-MRF24WG.x>

#else

    #error Neither the WiFi Shield nor PmodWiFi is supported by this board.

#endif

#include <./libraries/MRF24G/MRF24GAdaptor.h>
#include <./libraries/MRF24G/AdaptorClass.h>


#endif // PMODWIFI_NETWORK_PROFILE_X
