/************************************************************************/
/*                                                                      */
/*    HTMLSDPage.cpp                                                    */
/*                                                                      */
/*    Renders pages off of the SD card filesystem                       */
/*    Typically you would make this the default page handler            */
/*                                                                      */
/************************************************************************/
/*    Author:     Keith Vogel                                           */
/*    Copyright 2013, Digilent Inc.                                     */
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
/*  Revision History:                                                   */
/*    7/19/2013(KeithV): Created                                         */
/************************************************************************/
#include <OpenScope.h>

extern GCMD::ACTION ComposeHTMLRedirectPage(CLIENTINFO * pClientInfo);

static DFILE    dFile;              // Create a File handle to use to open files with
static const char * szFileName      = NULL;
static CLIENTINFO * pClientMutex    = NULL;
static uint32_t     cbSent          = 0;
static uint32_t     tStart          = 0;

/************************************************************************/
/*    HTML Strings                                                      */
/************************************************************************/
static const char szEndOfURL[] = " HTTP";
static const char szGET[] = "GET /";

/************************************************************************/
/*    State machine states                                              */
/************************************************************************/
typedef enum {
    PARSEFILENAME,
    BUILDHTTP,
    EXIT,
    SENDFILE,
    JMPFILENOTFOUND,
    DONE
 } SDSTATE;

 /***    uint32_t SDRead(File fileSD, uint8_t * pbRead, uint32_t cbRead)
 *
 *    Parameters:
 *          fileSD: An open SD file to read from, you must set the positon before calling SDRead
 *          pbRead: A pointer to a buffer to receive the data read
 *          cbRead: Max size of pbRead buffer
 *              
 *    Return Values:
 *          How many bytes were actually read.
 *
 *    Description: 
 *    
 *      Helper routine to read a buffer from a file.
 * ------------------------------------------------------------ */
uint32_t SDRead(DFILE& dFile, uint8_t * pbRead, uint32_t cbRead)
 {
    uint32_t cbA = min(min(dFile.fssize(), cbRead), DFILE::FS_DEFAULT_BUFF_SIZE);

    dFile.fsread(pbRead, cbA, &cbRead);
    return(cbRead);
 }

/***    GCMD::ACTION ComposeHTMLSDPage(CLIENTINFO * pClientInfo)
 *
 *    Parameters:
 *          pClientInfo - the client info representing this connection and web page
 *              
 *    Return Values:
 *          GCMD::ACTION    - GCMD::CONTINUE, just return with no outside action
 *                          - GCMD::READ, non-blocking read of input data into the rgbIn buffer appended to the end of cbRead
 *                          - GCMD::GETLINE, blocking read until a line of input is read or until the rgbIn buffer is full, always the line starts at the beginnig of the rgbIn
 *                          - GCMD::WRITE, loop writing until all cbWrite bytes are written from the pbOut buffer
 *                          - GCMD::DONE, we are done processing and the connection can be closed
 *
 *    Description: 
 *    
 *      This renders a page off of the SD filesystem. Pages of type
 *      .htm, .html, .jpeg, .png, .txt and more may be rendered
 *      The file extension on the filename determine the MIME type
 *      returned to the client.
 *    
 * ------------------------------------------------------------ */
GCMD::ACTION ComposeHTMLSDPage(CLIENTINFO * pClientInfo)
{
    char * pFileNameEnd     = NULL;

    GCMD::ACTION retCMD = GCMD::CONTINUE;

    switch(pClientInfo->htmlState)
    {
        case HTTPSTART:

            if(pClientMutex != NULL)
            {
                break;
            }
            pClientMutex = pClientInfo;

            Serial.println("Read an HTML page off of the SD card");

            Serial.print("Entering Client ID: 0x");
            Serial.println((uint32_t) pClientMutex, 16);

            pClientInfo->htmlState = PARSEFILENAME;
            retCMD = GCMD::GETLINE;
            break;

        case PARSEFILENAME:

            // the assumption is that the file name will be on the first line of the command
            // there is a bunch of other stuff on the line we don't care about, but it is at the
            // end of the line.
            Serial.println((char *) pClientInfo->rgbIn);

            // find the begining of the file name
            szFileName = strstr((const char *) pClientInfo->rgbIn, szGET);
            if(szFileName == NULL)
            {
                pClientInfo->htmlState = JMPFILENOTFOUND;
                break;
            }
            szFileName += sizeof(szGET) - 1;

            // find the end of the file name
            pFileNameEnd = strstr((char *) szFileName, (char *) szEndOfURL);

            if(pFileNameEnd == NULL)
            {
                pClientInfo->htmlState = JMPFILENOTFOUND;
                break;
            }
            else if(pFileNameEnd == szFileName)
            {
                Serial.print("Default page detected, jumping to redirect page on socket: 0x");
                Serial.println((uint32_t) pClientMutex, 16);
                pClientMutex = NULL;
                return(JumpToComposeHTMLPage(pClientInfo, ComposeHTMLRedirectPage));
            }
            else
            {
                *pFileNameEnd = '\0';
            }

            Serial.print("SD FileName:");
            Serial.print(szFileName);
            Serial.print(" on socket: 0x");
            Serial.println((uint32_t) pClientMutex, 16);

            if( (DFATFS::fschdrive(DFATFS::szFatFsVols[VOLSD]) != FR_OK)    ||
                (DFATFS::fschdir(DFATFS::szRoot) != FR_OK)                  ||
                (dFile.fsopen(szFileName, FA_READ) != FR_OK)                )
            {
                Serial.print("Unable to find HTML page:");
                Serial.println(szFileName);
                pClientInfo->htmlState = JMPFILENOTFOUND;
            }
            else
            {
                Serial.print("HTML page:");
                Serial.print(szFileName);
                Serial.println(" exists!");
                pClientInfo->htmlState = BUILDHTTP;
            }
            break;

        // We need to build the HTTP directive
        case BUILDHTTP:

            if(dFile && (dFile.fslseek(0) == FR_OK))
            {
                pClientInfo->cbWrite = BuildHTTPOKStr(false, dFile.fssize(), szFileName, (char *) pClientInfo->rgbScratch, sizeof(pClientInfo->rgbScratch));
                if(pClientInfo->cbWrite > 0)
                {
                    pClientInfo->pbOut = pClientInfo->rgbScratch;
                    retCMD = GCMD::WRITE;
                    pClientInfo->htmlState = SENDFILE;
                    cbSent = 0;
                    tStart = SYSGetMilliSecond();
                    
                    Serial.println("Response HTTP Headers:");
                    Serial.print((char *) pClientInfo->pbOut);
                    Serial.println("End of header");

                    Serial.print("Writing file:");
                    Serial.print(szFileName);
                    Serial.print(" of length: ");
                    Serial.println(dFile.fssize(), 10);
                }
                else
                {
                    Serial.print("Unable to build HTTP directive for file:");
                    Serial.println(szFileName);
                    pClientInfo->htmlState = JMPFILENOTFOUND;
                }
            }
            else
            {
                Serial.print("Unable to open HTML page:");
                Serial.println(szFileName);
                pClientInfo->htmlState = JMPFILENOTFOUND;
            }
            break;

        // Send the file
        case SENDFILE:
            {
                uint32_t    cbT = 0;

                if((cbT = SDRead(dFile, pClientInfo->rgbScratch, sizeof(pClientInfo->rgbScratch))) > 0)
                {
                    cbSent += cbT;
                    pClientInfo->pbOut = pClientInfo->rgbScratch;
                    pClientInfo->cbWrite = cbT;
                    tStart = SYSGetMilliSecond();
                    retCMD = GCMD::WRITE;
                    
                    if(((cbSent - cbT) / 100000) < (cbSent / 100000) ||  cbSent == dFile.fssize())
                    {
                        Serial.print("Progress.... ");
                        Serial.print(cbSent - cbT, 10);
                        Serial.print(" bytes sent, next packet ");                        
                        Serial.print(cbT, 10);
                        Serial.println(" bytes.");                        
                    }
                }
                else if(cbSent == dFile.fssize())
                {
                   pClientInfo->htmlState = EXIT;
                }
                else if((SYSGetMilliSecond() - tStart) > SDREADTIMEOUT)
                {
                   pClientInfo->htmlState = HTTPTIMEOUT;
                }
            }
            break;
    
         case EXIT:
            Serial.print("Wrote page cleanly on socket: 0x");
            Serial.println((uint32_t) pClientMutex, 16);
            pClientInfo->htmlState = HTTPDISCONNECT;
            break;

        case JMPFILENOTFOUND:
            Serial.print("Jumping to HTTP File Not Found page on socket: 0x");
            Serial.println((uint32_t) pClientMutex, 16);
            dFile.fsclose();
            pClientMutex = NULL;
            return(JumpToComposeHTMLPage(pClientInfo, ComposeHTTP404Error));
            break;

        case HTTPTIMEOUT:
            Serial.print("Timeout error occurred, closing the session on socket: 0x");
            Serial.println((uint32_t) pClientMutex, 16);

            // fall thru to close

        case HTTPDISCONNECT:
            if(pClientMutex == pClientInfo)
            {
                Serial.print("Closing Client ID: 0x");
                Serial.println((uint32_t) pClientMutex, 16);
                dFile.fsclose();
                pClientMutex = NULL;
            }
            // fall thru Done

        case DONE:
        default:
            pClientInfo->cbWrite = 0;
            retCMD = GCMD::DONE;
            break;
    }

    return(retCMD);
}
