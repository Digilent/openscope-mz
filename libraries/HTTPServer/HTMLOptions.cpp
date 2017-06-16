/************************************************************************/
/*                                                                      */
/*    HTMLOptions.cpp                                                   */
/*                                                                      */
/*    Renders the options HTTP header                                   */
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
/*    9/15/2016(KeithV): Created                                         */
/************************************************************************/
#include <OpenScope.h>

/************************************************************************/
/*    HTML Strings                                                      */
/************************************************************************/
static const char szOptions[] = 
"HTTP/1.1 200 OK\r\n\
Access-Control-Allow-Origin: *\r\n\
Access-Control-Allow-Headers: Content-Type\r\n\
Access-Control-Max-Age: 86400\r\n\r\n\
<body>\r\n\
Options Response Successful\r\n\
<br>\r\n\
</body>\r\n";

/***    GCMD::ACTION ComposeHTMLOptions(CLIENTINFO * pClientInfo)
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
 *      Renders the server restart HTML page 
 *    
 * ------------------------------------------------------------ */
GCMD::ACTION ComposeHTMLOptions(CLIENTINFO * pClientInfo)
{
    switch(pClientInfo->htmlState)
    {
         case HTTPSTART:
            Serial.println("Option Page detected");
            pClientInfo->cbWrite = sizeof(szOptions)-1;
            pClientInfo->pbOut = (uint8_t * const) szOptions;
            pClientInfo->htmlState = HTTPDISCONNECT;
            return(GCMD::WRITE);
            break;

        default:
            pClientInfo->cbWrite = 0;
            break;
    }

    return(GCMD::DONE);
}
