/************************************************************************/
/*                                                                      */
/*    HTMLReboot.cpp                                                    */
/*                                                                      */
/*    Renders the HTML reboot page                                      */
/*                                                                      */
/************************************************************************/
/*    Author:     Keith Vogel                                           */
/*    Copyright 2013, Digilent Inc.                                     */
/************************************************************************/
/************************************************************************/
/*  Revision History:                                                   */
/*    7/24/2013(KeithV): Created                                         */
/************************************************************************/
#include <OpenScope.h>

/************************************************************************/
/*    HTML Strings                                                      */
/************************************************************************/

#if 0
console.log(e);
#endif

static const char szRedirect[] = 
"<html>\
<script type=\"text/javascript\">\
function onLoadHandler() {\
window.location = \"http://waveformslive.com\";\
}\
function onErrorHandler(e) {\
window.location = \"/index.html\";\
}\
var oReq = new XMLHttpRequest();\
oReq.addEventListener(\"load\", onLoadHandler);\
oReq.addEventListener(\"error\", onErrorHandler);\
oReq.open(\"GET\", \"http://waveformslive.com\");\
oReq.setRequestHeader(\"Cache-Control\", \"no-cache\");\
oReq.send();\
</script>\
</html>"; 

/************************************************************************/
/*    State machine states                                              */
/************************************************************************/
typedef enum {
    WRITECONTENT,
    DONE
} HTTPSTATE;

/***    GCMD::ACTION ComposeHTMLRestartPage(CLIENTINFO * pClientInfo)
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
GCMD::ACTION ComposeHTMLRedirectPage(CLIENTINFO * pClientInfo)
{

   GCMD::ACTION retCMD = GCMD::WRITE;

    switch(pClientInfo->htmlState)
    {
         case HTTPSTART:
            Serial.println("Redirect Request Detected");
            pClientInfo->cbWrite = BuildHTTPOKStr(true, sizeof(szRedirect)-1, ".htm", (char *) pClientInfo->rgbScratch, sizeof(pClientInfo->rgbScratch));
            pClientInfo->pbOut = pClientInfo->rgbScratch;
            pClientInfo->htmlState = WRITECONTENT;
            break;

         case WRITECONTENT:
             pClientInfo->pbOut = (const uint8_t *) szRedirect;
             pClientInfo->cbWrite = sizeof(szRedirect)-1;
             pClientInfo->htmlState = DONE;
             break;

        case HTTPTIMEOUT:
            Serial.print("Timeout error occurred, closing the session on socket: 0x");
            Serial.println((uint32_t) pClientInfo, 16);

            // fall thru to close

        case HTTPDISCONNECT:
            Serial.print("Closing Client ID: 0x");
            Serial.println((uint32_t) pClientInfo, 16);
            // fall thru Done

        case DONE:
        default:
            pClientInfo->cbWrite = 0;
            retCMD = GCMD::DONE;
            break;
    }

    return(retCMD);
}
