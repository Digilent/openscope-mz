/************************************************************************/
/*                                                                      */
/*    HTTPHelpers.cpp                                                  */
/*                                                                      */
/*    Renders the HTTP 404 file not found page                          */
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
/*    2/1/2013(KeithV): Created                                         */
/*    7/22/2013(KeithV): Modified for generic WebServer operations      */
/************************************************************************/
#include <OpenScope.h>

/************************************************************************/
/*    HTTP Strings                                                      */
/************************************************************************/
static const char szHTTP404Error[] = "HTTP/1.1 404 Not Found\r\n\r\n";
static const char szHTTPOK[] = "HTTP/1.1 200 OK\r\n";
static const char szNoCache[] = "Cache-Control: no-cache\r\n";
static const char szConnection[] = "Connection: close\r\n";
static const char szContentType[] = "Content-Type: ";
static const char szContentLength[] = "Content-Length: ";
static const char szTransferEncodingChunked[] = "Transfer-Encoding: chunked\r\n";
static const char szAccessControlAllowOrigin[] = "Access-Control-Allow-Origin: *\r\n";
static const char szLineTerminator[] = "\r\n";

/************************************************************************/
/*    State machine states                                              */
/************************************************************************/
typedef enum {
    HTTPERROR, 
    DONE
} HTTPSTATE;

static CLIENTINFO * pClientMutex = NULL;


/***    GCMD::ACTION ComposeHTTP404Error(CLIENTINFO * pClientInfo)
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
 *      Renders the file not found 404 error  
 *    
 * ------------------------------------------------------------ */
GCMD::ACTION ComposeHTTP404Error(CLIENTINFO * pClientInfo)
{
 
    GCMD::ACTION retCMD = GCMD::CONTINUE;

    switch(pClientInfo->htmlState)
    {
        case HTTPSTART:

            if(pClientMutex != NULL)
            {
                break;
            }
            pClientMutex = pClientInfo;

            Serial.println("HTTP Error");
            pClientInfo->htmlState = HTTPERROR;
            break;

        case HTTPERROR:
            Serial.println();
            pClientInfo->pbOut = (const uint8_t *) szHTTP404Error;
            pClientInfo->cbWrite = sizeof(szHTTP404Error)-1;
            pClientInfo->htmlState = HTTPDISCONNECT;
            retCMD = GCMD::WRITE;
            break;

        case HTTPDISCONNECT:
            if(pClientMutex == pClientInfo)
            {
                pClientMutex = NULL;
            }
            // fall thru to done

        case DONE:
        default:
            pClientInfo->cbWrite = 0;
            retCMD = GCMD::DONE;
            break;
    }

    return(retCMD);
}

/***    const char * GetContentTypeFromExt(const char * szExt)
 *
 *    Parameters:
 *          szExt - A zero terminated string containing the file extension, starting at the .
 *              
 *    Return Values:
 *          const char *    - A pointer to a MIME type content string to use in 
 *                              an HTTP Content-Type: specifier
 *    Description: 
 *    
 *      A list of content types can be found at http://en.wikipedia.org/wiki/MIME_type
 *    
 * ------------------------------------------------------------ */
const char * GetContentTypeFromExt(const char * szExt)
{
    if(szExt == NULL || strlen(szExt) <= 1 || szExt[0] != '.') return("text/plain");
    szExt++;    // move past the .
  
    if(stricmp(szExt, "htm") == 0 || stricmp(szExt, "html") == 0) return("text/html");

    else if(stricmp(szExt, "jpg") == 0 || stricmp(szExt, "jpeg") == 0) return("image/jpeg");

    else if(stricmp(szExt, "png") == 0) return("image/png");

    else if(stricmp(szExt, "gif") == 0) return("image/gif");
    
    else if(stricmp(szExt, "ico") == 0) return("image/x-icon");

    else if(stricmp(szExt, "tif") == 0 || stricmp(szExt, "tiff") == 0) return("image/tiff");

    else if(stricmp(szExt, "txt") == 0) return("text/plain");

    else if(stricmp(szExt, "xml") == 0) return("text/xml");

    else if(stricmp(szExt, "zip") == 0) return("application/x-zip-compressed");

    else if(stricmp(szExt, "mpg") == 0 || stricmp(szExt, "mpeg") == 0) return("video/mpeg");

    else if(stricmp(szExt, "mp4") == 0) return("video/mp4");

    else if(stricmp(szExt, "wmv") == 0) return("video/x-ms-wmv");

    else if(stricmp(szExt, "flv") == 0) return("video/x-flv");

    else if(stricmp(szExt, "js") == 0) return("application/javascript");

    else if(stricmp(szExt, "json") == 0 || stricmp(szExt, "map") == 0) return("application/json");

    else if(stricmp(szExt, "css") == 0) return("text/css");

    else if(stricmp(szExt, "ttf") == 0) return("application/x-font-ttf");

    else if(stricmp(szExt, "svg") == 0) return("image/svg+xml");

    else if(stricmp(szExt, "osjb") == 0) return("application/octet-stream");

    else return("text/plain");
}

/***    const char * FindExtLocation(const char * szFile)
 *
 *    Parameters:
 *          szFile          - The full file name with extension
 *              
 *    Return Values:
 *          The pointer to the '.' to the file extension
 *
 *    Description: 
 *    
 *      This deals with long filenames with multiple '.'s in the name
 *      looking for the last dot and file extension.
 *    
 * ------------------------------------------------------------ */
const char * FindExtLocation(const char * szFile)
{
    uint32_t cbFile = (szFile == NULL) ? 0 : strlen(szFile);

    while(cbFile > 0)
    {
        cbFile--;
        if(szFile[cbFile] == '.')
        {
            return(&szFile[cbFile]);
        }
    }

    return(NULL);
}

/***    uint32_t BuildHTTPOKStr(bool fNoCache, uint32_t cbContentLen, const char * szFile, char * szHTTPOK, uint32_t cbHTTPOK)
 *
 *    Parameters:
 *          fNoCache:       - true if you want the HTTP header to specify that the HTML page should not be cached by the browser
 *          cbContentLen:   - The length of the content, specify zero if you do not want this tag in there.
 *          szFile          - The full file name with extension. The content type will be derived from the file extension, 
 *                              if no extension a text content type is assumed
 *          szHTTPOK        - A pointer to a string to take the HTTP OK command
 *          cbHTTPOK        - the length in bytes of the szHTTPOK string, this must be big enough to hold the whole HTTP directive
 *                              figure about 128 bytes. The ClientInfo->rgbScratch output buffer big enough to hold the HTTP directive
 *              
 *    Return Values:
 *          The number of bytes in the HTTP OK directive; less the null terminator
 *          zero is return on error of if szHTTPOKStr was not big enough to hold the whole directive
 *
 *    Description: 
 *    
 *      This builds an HTTP OK directive. szHTTPOK must be large enough to hold the whole directive or zero will be returned
 *    
 * ------------------------------------------------------------ */
uint32_t BuildHTTPOKStr(bool fNoCache, uint32_t cbContentLen, const char * szFile, char * szHTTPOKStr, uint32_t cbHTTPOK)
{
    uint32_t i = 0;
    const char * szExt = FindExtLocation(szFile);
//    bool fChunked = (strcmp(szExt, ".osjb") == 0);
    bool fChunked = false;

    char szContentLenStr[36];
    uint32_t cbContentLenStr = 0;

    const char * szContentTypeStr = GetContentTypeFromExt(szExt);
    uint32_t cbContentTypeStr = 0;

    uint32_t cb = sizeof(szHTTPOK) + sizeof(szLineTerminator) - 2;
    
    itoa(cbContentLen, szContentLenStr, 10);
    cbContentLenStr = strlen(szContentLenStr);
    
    if(fNoCache)
    {
        cb += sizeof(szNoCache) - 1;
    }

    cb += sizeof(szConnection) - 1;

    if(szContentTypeStr == NULL)
    {
        // this will always pass
        // by default if we have no ext on the file and thus no content type
        // we will assume a txt content
        szContentTypeStr = GetContentTypeFromExt("txt");
    }
    cbContentTypeStr = strlen(szContentTypeStr);
    cb += cbContentTypeStr + sizeof(szContentType) + sizeof(szLineTerminator) - 2;

    // szTransferEncodingChunked = "Transfer-Encoding: chunked"
    // we don't have to have a content length, but the browser will shutdown faster if we do
    if(fChunked)
    {
        cb += sizeof(szTransferEncodingChunked)-1;
    }
    else if(cbContentLen > 0)
    {
        cb += cbContentLenStr + sizeof(szContentLength) + sizeof(szLineTerminator) - 2;
    }

    cb += sizeof(szAccessControlAllowOrigin) - 1;

    // make sure we have enough room
    // we say >= because we must leave room for the terminating NULL
    if(cb >= cbHTTPOK)
    {
        return(0);
    }

    // now start to build the HTTP OK header
    memcpy(szHTTPOKStr, szHTTPOK, sizeof(szHTTPOK) - 1);
    i = sizeof(szHTTPOK) - 1;

    // put in the no cache line if requested
    if(fNoCache)
    {
        memcpy(&szHTTPOKStr[i], szNoCache, sizeof(szNoCache) - 1);
        i += sizeof(szNoCache) - 1;
    }

    // put in the connection type
    memcpy(&szHTTPOKStr[i], szConnection, sizeof(szConnection) - 1);
    i += sizeof(szConnection) - 1;

    // put in the content type
    if(szContentTypeStr != NULL)
    {
        memcpy(&szHTTPOKStr[i], szContentType, sizeof(szContentType) - 1);
        i += sizeof(szContentType) - 1;
        memcpy(&szHTTPOKStr[i], szContentTypeStr, cbContentTypeStr);
        i += cbContentTypeStr;
        memcpy(&szHTTPOKStr[i], szLineTerminator, sizeof(szLineTerminator) - 1);
        i += sizeof(szLineTerminator) - 1;
    }

    // szTransferEncodingChunked = "Transfer-Encoding: chunked"
    if(fChunked)
    {
        memcpy(&szHTTPOKStr[i], szTransferEncodingChunked, sizeof(szTransferEncodingChunked) - 1);
        i += sizeof(szTransferEncodingChunked) - 1;
    }
    // we don't have to have a content length, but the browser will shutdown faster if we do
    else if(cbContentLen > 0)
    {
        memcpy(&szHTTPOKStr[i], szContentLength, sizeof(szContentLength) - 1);
        i += sizeof(szContentLength) - 1;
        memcpy(&szHTTPOKStr[i], szContentLenStr, cbContentLenStr);
        i += cbContentLenStr;
        memcpy(&szHTTPOKStr[i], szLineTerminator, sizeof(szLineTerminator) - 1);
        i += sizeof(szLineTerminator) - 1;
    }

    // put in the origin access control
    memcpy(&szHTTPOKStr[i], szAccessControlAllowOrigin, sizeof(szAccessControlAllowOrigin) - 1);
    i += sizeof(szAccessControlAllowOrigin) - 1;

    // terminate the HTTP header
    memcpy(&szHTTPOKStr[i], szLineTerminator, sizeof(szLineTerminator) - 1);
    i += sizeof(szLineTerminator) - 1;

    // put in the null terminator
    szHTTPOKStr[i] = '\0';

    return(i);
}
