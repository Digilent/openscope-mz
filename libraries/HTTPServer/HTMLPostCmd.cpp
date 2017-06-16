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
static const char szContentLength[] = "Content-Length: ";
static const char szTerminateChunk[] = "\r\n0\r\n\r\n";


/************************************************************************/
/*    Static local variables                                            */
/************************************************************************/
static CLIENTINFO *     pClientMutex            = NULL;
static uint32_t cbTotal = 0;
static uint32_t cbContentLenght = 0;
static uint32_t iBinary = 0;

/************************************************************************/
/*    State machine states                                              */
/************************************************************************/
typedef enum {
    CONTLEN,
    ENDHDR,
    JSONLEX,
    WRITEHTTP,
    WRITEJSON,
    BINARYCHUNK,
    WRITEBINARY,
    TERMINATECHUNK,
    JMPFILENOTFOUND,
    DONE
} HSTATE;

/***    GCMD::ACTION ComposeHTMLPostCmd(CLIENTINFO * pClientInfo)
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
GCMD::ACTION ComposeHTMLPostCmd(CLIENTINFO * pClientInfo)
{
   GCMD::ACTION retCMD = GCMD::CONTINUE;

    switch(pClientInfo->htmlState)
    {
         case HTTPSTART:
                         
            // absolutely critical that we don't process 2 JSON commands concurrently
            // so much interaction between the states and the datastructures that new
            // information can not be added inbetween the states.
            if(pClientMutex != NULL || oslex.fLocked)
            {
                break;
            }
            pClientMutex = pClientInfo;
            oslex.fLocked = true;

            Serial.println("JSON Post Detected");
            pClientInfo->htmlState = CONTLEN;

            // read off the URL until the body
            retCMD = GCMD::GETLINE;
            break;

        case CONTLEN:

            // assumeing we will be getting the next line
            retCMD = GCMD::GETLINE;

            // if we hit the end of the header then there was no content length
            // and we don't know how to handle that, so exit with an error
            // File not found is probably the wrong error, but it does get out out
            // Fortunately all major browsers put in the content lenght, so this
            // will almost never fail.
            if(strlen((char *) pClientInfo->rgbIn) == 0)    // cbRead may be longer than just the line, so do a strlen()
            {
                pClientInfo->htmlState = JMPFILENOTFOUND;
                retCMD = GCMD::CONTINUE;
            }

            // found the content lengths
            else if(memcmp((uint8_t *) szContentLength, pClientInfo->rgbIn, sizeof(szContentLength)-1) == 0)
            {
                cbContentLenght = atoi((char *) &pClientInfo->rgbIn[sizeof(szContentLength)-1]);
                pClientInfo->htmlState = ENDHDR;
            }
            break;

        case ENDHDR:

            // the header is ended with a double \r\n\r\n, so I will get
            // a zero length line. Just keep reading lines until we get to the blank line
            if(strlen((char *) pClientInfo->rgbIn) == 0)    // cbRead may be longer than just the line, so do a strlen()
            {
                uint32_t i = 0;

                // go to beyond the \0
                for(i = 0; i < pClientInfo->cbRead && pClientInfo->rgbIn[i] == '\0'; i++);

                // move the buffer to the front
                pClientInfo->cbRead -= i;
                if(pClientInfo->cbRead > 0)
                {
                    memcpy(pClientInfo->rgbIn, &pClientInfo->rgbIn[i], pClientInfo->cbRead);
                }

                // if there is nothing left in the buffer, read some in
                else retCMD = GCMD::READ;

                // no content, just exit
                if(cbContentLenght == 0)
                {
                    pClientInfo->htmlState = HTTPDISCONNECT;
                }

                else
                {
                    // get ready for lexing
                    oslex.Init();
                    oslex.fLocked = true;
                    cbTotal = 0;
                    pClientInfo->cbWrite = 0;
                    pClientInfo->htmlState = JSONLEX;
                }
            }
            else
            {
                retCMD = GCMD::GETLINE;
            }
            break;

        case JSONLEX:

            switch(oslex.StreamJSON((const char *) pClientInfo->rgbIn, pClientInfo->cbRead))
            {

                case GCMD::READ:

                    // add this to the total processed
                    cbTotal += pClientInfo->cbRead;

                    // is this all we are going to get
                    if(cbTotal >= cbContentLenght) 
                    {
                        // setting this to 0 will tell the lexer there is no more
                        pClientInfo->cbRead = 0;
                    }

                    // otherwise get some more
                    else
                    {
                        pClientInfo->cbRead = 0;
                        retCMD = GCMD::READ;
                    }
                    break;

                case GCMD::DONE:
                case GCMD::ERROR:
                    // we are done, go put out the response
                    pClientInfo->htmlState = WRITEHTTP;
                    retCMD = GCMD::CONTINUE;
                    break;

                // continue
                default:
                    break;

            }
            break;

        case WRITEHTTP:

            // if there is binary to write out.
            if(oslex.cOData > 1)
            {
                uint32_t i = 0;
                uint32_t cb = 0;
                char szcbJSON[32];
                char szcbBinary[32];

                // create the length of the JSON part of the message
                utoa(oslex.odata[0].cb, szcbJSON, 16);
                cb = strlen(szcbJSON);
                szcbJSON[cb++] = '\r';
                szcbJSON[cb++] = '\n';
                pClientInfo->cbWrite = cb;

                // calculate the total size of the binary to follow
                cb = 0;
                for(i=1; i<oslex.cOData; i++) cb += oslex.odata[i].cb;

                utoa(cb, szcbBinary, 16);           // size of binary
                cb += strlen(szcbBinary) + 2;       // size of binary count with \r\n
                cb += oslex.odata[0].cb + 2;        // size of json with \r\n
                cb += pClientInfo->cbWrite;         // size of json count
                cb += sizeof(szTerminateChunk)-1;   // size of terminator

                // create the header
                cb = BuildHTTPOKStr(true, cb, ".osjb", (char *) pClientInfo->rgbScratch, sizeof(pClientInfo->rgbScratch));

                // add the JSON string after the header
                memcpy(&pClientInfo->rgbScratch[cb], szcbJSON, pClientInfo->cbWrite);
                pClientInfo->cbWrite += cb; // add the JSON count size on to the header size

                // push this out on the network the header and JSON count
                pClientInfo->pbOut = pClientInfo->rgbScratch;
            }

            // no binary, just JSON
            else
            {
                pClientInfo->cbWrite = BuildHTTPOKStr(true, oslex.odata[0].cb, ".json", (char *) pClientInfo->rgbScratch, sizeof(pClientInfo->rgbScratch));
                pClientInfo->pbOut = pClientInfo->rgbScratch;
            }
            pClientInfo->htmlState = WRITEJSON;
            retCMD = GCMD::WRITE;

            break;

        case WRITEJSON:

            // index 0 is always the JSON
            pClientInfo->cbWrite = oslex.odata[0].cb;
            pClientInfo->pbOut = oslex.odata[0].pbOut;

            // see if we have binary too
            if(oslex.cOData > 1) pClientInfo->htmlState = BINARYCHUNK;
            else pClientInfo->htmlState = HTTPDISCONNECT;

            // write it out
            retCMD = GCMD::WRITE;
            break;

        // Create the chuck size for the binary
        case BINARYCHUNK:
            {
                uint32_t i = 0;
                uint32_t cb = 0;

                // terminate the json chunk
                pClientInfo->cbWrite = 0;
                pClientInfo->rgbScratch[pClientInfo->cbWrite++] = '\r';
                pClientInfo->rgbScratch[pClientInfo->cbWrite++] = '\n';

                // calculate the total size of the binary to follow
                for(i=1; i<oslex.cOData; i++) cb += oslex.odata[i].cb;

                // create the length of the JSON part of the message
                utoa(cb, (char *) &pClientInfo->rgbScratch[pClientInfo->cbWrite], 16);
                pClientInfo->cbWrite += strlen((char *) &pClientInfo->rgbScratch[pClientInfo->cbWrite]);
                pClientInfo->rgbScratch[pClientInfo->cbWrite++] = '\r';
                pClientInfo->rgbScratch[pClientInfo->cbWrite++] = '\n';
                pClientInfo->pbOut = pClientInfo->rgbScratch;

                iBinary = 1;
                pClientInfo->htmlState = WRITEBINARY;
                retCMD = GCMD::WRITE;
            }
            break;

        case WRITEBINARY:

            // put out this block
            pClientInfo->cbWrite = oslex.odata[iBinary].cb;
            pClientInfo->pbOut = oslex.odata[iBinary].pbOut;
            iBinary++;

            if(iBinary >= oslex.cOData) pClientInfo->htmlState = TERMINATECHUNK;
            
            // Write out this binary
            retCMD = GCMD::WRITE;
            break;

        case TERMINATECHUNK:

            // put out the chunk terminator
            pClientInfo->cbWrite = sizeof(szTerminateChunk)-1;
            pClientInfo->pbOut = (uint8_t *) szTerminateChunk;
            pClientInfo->htmlState = HTTPDISCONNECT;
            retCMD = GCMD::WRITE;
            break;

        case JMPFILENOTFOUND:
            Serial.println("Jumping to HTTP File Not Found page");
            pClientMutex = NULL;
            oslex.Init();
            return(JumpToComposeHTMLPage(pClientInfo, ComposeHTTP404Error));
            break;

        case HTTPTIMEOUT:
            Serial.println("Timeout error occurred, closing the session");

            // fall thru to close

        case HTTPDISCONNECT:
            if(pClientMutex == pClientInfo)
            {
                uint32_t i;
                
                for(i=0; i<oslex.cOData; i++)
                {
                    if(oslex.odata[i].pLockState != NULL && *oslex.odata[i].pLockState == LOCKOutput)
                    {
                        *oslex.odata[i].pLockState = LOCKAvailable;
                    }
                }

                // we are no longer parsing JSON
                oslex.Init();

                if(pjcmd.trigger.state.parsing == JSPARTrgRead) 
                {
                    pjcmd.trigger.state.parsing = JSPARTrgTriggered;
                }
                Serial.print("Closing Client ID: 0x");
                Serial.println((uint32_t) pClientMutex, 16);
                pClientMutex = NULL;
            }
            // fall thru Done

        case DONE:
        default:
            pClientInfo->cbWrite    = 0;
            retCMD                  = GCMD::DONE;
            break;
    }

    return(retCMD);
}
