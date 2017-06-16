/************************************************************************/
/*                                                                      */
/*    WebServer.h                                                       */
/*                                                                      */
/*    A chipKIT WiFi Server implementation                              */
/*    Designed to look for and parse /GET commands and automatically    */
/*    handle multiple open TCP connections                              */
/*    This sketch is designed to work with web browsers                 */
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
/************************************************************************/
#if !defined(_WEBSERVER_H)
#define	_WEBSERVER_H

#define OFFSETOF(t,m)        ((uint32_t) (&(((t *) 0)->m)))

#define CNTHTTPCMD          10      // The Max number of unique HTML pages we vector off of. This must be at least 1. 
#define secClientTO         1000      // time in seconds to wait for a response before aborting a client connection.
#define CBCLILENTINPUTBUFF  256     // The max size of the TCP read buffer, this typically only has to be as large as the URL up to the HTTP tag in the GET line
#define CBCLILENTSCRATCH    512     // This is scratch output buffer space to generate out going HTML strings it, it is optional if not needed can be set as small as 4;

// these are predefine HTML state machine states, HTTPINIT "must" be implemented, the others can be processed under case default if not needed.
#define HTTPSTART           10000   // This is a predefine state for the rendering HTML page to initialize the state machine 
#define HTTPDISCONNECT      20001   // This state is called after the TCP connection is closed, for whatever reason even timeouts, so the HTML state machine to clean up
#define HTTPTIMEOUT         20002   // If a read timeout occured, your state machine will be called with this timeout value

#define SDREADTIMEOUT 1000          // We have waited too long to read from the SD card in ms

#ifdef __cplusplus

#include <OSSerial.h>

// This are global commands used by the WebServer and ProcessClient state machines.
// again it there own namespace to not consume the names.
namespace GCMD {

    // These are global states that are used by both the HTTP Server and the Process Client code.
    typedef enum
    {
        // These are used mostly by process client to instruct from the
        // HTML page what acition it would like to perform next.
        CONTINUE = 0,   // just call the HTML code again on the next loop
//        MOVE,           // move the input buffer to the begining
        READ,           // Read more input from the TCP connection
        GETLINE,        // Read the next line of input.
        WRITE,          // iteratively write data to the TCP connection until the buffer has been completely written
        DONE,           // The HTML page is finished and the TCP connection can be closed. 
        ERROR,          // An error occured in the underlying code

        // These are specific to the Server and not used other than a command 
        // passed up to the server code to instruct the server to do something.
        ADDSOCKET,      // return the socket to the server object
        RESTART,        // Restart the network stack
        TERMINATE,      // Terminate the server; spin
        REBOOT,         // Reboot the processor; do a soft MCLR
    } ACTION;
}

typedef GCMD::ACTION (* FNRENDERHTML) (struct CLIENTINFO_T * pClientInfo);

// information that is needed to process the client and ultimately call and render and HTML page
typedef struct CLIENTINFO_T
{
    // TCP Client state machine varables
    TCPSocket *     pTCPClient;                     // the socket
    uint32_t        clientState;                    // a state machine variable for process client to use
    uint32_t        nextClientState;                // a delayed state variable for process client to use (state specific)
    uint32_t        tStartClient;                   // a timer value used for timeout
    uint32_t        cbRead;                         // number of valid bytes in rgbIn, 
    uint8_t            rgbIn[CBCLILENTINPUTBUFF];      // The input buffer, this is where the /GET and URL will be
    uint8_t            rgbOverflow[4];                 // some overflow space to put characters in while parsing; typically not used.

    // HTML processing variables
    uint32_t        htmlState;                      // a state variable for the HTML web page state machine to use; each page is different
    uint32_t        cbWrite;                        // number of bytes to write out when GCMD::WRITE is returned
    uint32_t        cbWritten;                      // a variable for process client to use to know how many bytes have been written

    union
    {
        uint8_t __attribute__((deprecated("Use rgbScratch instead")))  rgbOut[CBCLILENTSCRATCH];    // a per client working scratch output buffer space that can be used if the HTML page/data is being created dynamically. 
        uint8_t        rgbScratch[CBCLILENTSCRATCH];   // a per client working scratch output buffer space that can be used if the HTML page/data is being created dynamically. 
        uint32_t    rgU32Scratch[CBCLILENTSCRATCH/sizeof(uint32_t)];
    };
    const uint8_t *    pbOut;                          // The actual pointer to the output buffer for ProcessClient to write to the TCP connection. 
                                                    // Typically this is either set to a static string in flash, or to a dynmically created string stored in rgbScratch[] or another static buffer in RAM
                                                    // This is usally assigned in the HTML rendering code. If GCMD::WRITE is returned, this pointer must not be NULL.

    // pointer to the HTML page rendering function
    FNRENDERHTML    ComposeHTMLPage;
} __attribute__((packed)) CLIENTINFO;

// server/client functions
GCMD::ACTION ProcessClient(CLIENTINFO * pClientInfo);
GCMD::ACTION JumpToComposeHTMLPage(CLIENTINFO * pClientInfo, FNRENDERHTML FnJumpComposeHTMLPage);

// predefined rendering pages
GCMD::ACTION ComposeHTTP404Error(CLIENTINFO * pClientInfo);
GCMD::ACTION ComposeHTMLRestartPage(CLIENTINFO * pClientInfo);
GCMD::ACTION ComposeHTMLTerminatePage(CLIENTINFO * pClientInfo);
GCMD::ACTION ComposeHTMLRebootPage(CLIENTINFO * pClientInfo);
GCMD::ACTION ComposeHTMLSDPage(CLIENTINFO * pClientInfo);

// rendering functions
bool AddHTMLPage(const char * szMatchStr, FNRENDERHTML FnComposeHTMLPage);
void SetDefaultHTMLPage(FNRENDERHTML FnDefaultHTMLPage);

int GetIP(IPv4& ip, char * sz);
int GetMAC(MACADDR& mac, char * sz);

#endif // C++

void ServerSetup(void);
void ProcessServer(void);

// HTTP helper functions
uint32_t BuildHTTPOKStr(bool fNoCache, uint32_t cbContentLen, const char * szFile, char * szHTTPOKStr, uint32_t cbHTTPOK);

// Generic Helper functions
int GetDayAndTime(unsigned int epochTimeT, char * szDateTime);
int GetNumb(uint8_t * rgb, int cb, char chDelim, char * sz);


#include    "HTTPServerConfig.h"
#endif //	_WEBSERVER_H


