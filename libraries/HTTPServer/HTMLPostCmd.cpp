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

/************************************************************************/
/*    Static local variables                                            */
/************************************************************************/
static CLIENTINFO *     pClientMutex        = NULL;
static uint32_t         cbTotal             = 0;
static uint32_t         cbContentLength     = 0;
static bool             fFirstWrite         = true;

/************************************************************************/
/*    State machine states                                              */
/************************************************************************/
typedef enum {
    CONTLEN,
    ENDHDR,
    JSONLEX,
    WRITEHTTP,
    WRITEJSON,
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
            else if(strncasecmp(szContentLength, (char *) pClientInfo->rgbIn, sizeof(szContentLength)-1) == 0)
            {
                cbContentLength = atoi((char *) &pClientInfo->rgbIn[sizeof(szContentLength)-1]);
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
                if(cbContentLength == 0)
                {
                    pClientInfo->htmlState = HTTPDISCONNECT;
                }

                else
                {
                    // get ready for lexing
                    oslex.Init(OSPAR::ICDStart);
                    oslex.fLocked = true;
                    cbTotal = 0;
                    fFirstWrite = true;
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

            switch(oslex.StreamOS((const char *) pClientInfo->rgbIn, pClientInfo->cbRead))
            {

                case GCMD::READ:

                    // add this to the total processed
                    cbTotal += pClientInfo->cbRead;

                    // is this all we are going to get
                    if(cbTotal >= cbContentLength) 
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

                case GCMD::WRITE:
                    if(fFirstWrite)
                    {
                        fFirstWrite = false;
                        pClientInfo->htmlState = WRITEHTTP;
                        retCMD = GCMD::CONTINUE;
                    }
                    else
                    {
                        pClientInfo->cbWrite    = oslex.cbOutput;
                        pClientInfo->pbOut      = oslex.pbOutput;
                        retCMD = GCMD::WRITE;
                    }

                    break;

                case GCMD::DONE:
                    // we are done, go put out the response
                    fFirstWrite = true;
                    pClientInfo->htmlState = HTTPDISCONNECT;
                    retCMD = GCMD::CONTINUE;
                    break;

                case GCMD::CONTINUE:
                    retCMD = GCMD::CONTINUE;
                    break;

                // never should get this
                default:
                    ASSERT(NEVER_SHOULD_GET_HERE); 
                    break;

            }
            break;

        case WRITEHTTP:

            // if there is binary to write out.
            if(oslex.cOData > 1)
            {
                uint32_t cbT = 0;
                uint32_t cb = 0;
                int32_t i = 0;

                // we need to caluculate the content length
                // it will be the sum of the output plus
                // the chunk sizes (in hex), plus \r\n

                // sum up all of the blocks
                for(i=0; i<oslex.cOData; i++) cbT += oslex.odata[i].cb;

                // The first chunk is the JSON, the second chunk is the rest of the binary
                // do the binary first, because cbT has the sum of all parts
                cb = cbT -  oslex.odata[0].cb;

                // now see how many digits it has, base 16
                for(i=0; cb>0; i++,cb>>=4);
                if(i==0) i = 1; // this should never happen
                cbT += i;

                // now do for the first chunk, the JSON chunk
                cb = oslex.odata[0].cb;
                for(i=0; cb>0; i++,cb>>=4);
                if(i==0) i = 1; // this should never happen
                cbT += i;

                // now we have to add all of the \r\n
                // each chunk is terminated with  a \r\n (there are 2 of them)
                // each count is terminate with a \r\n (there are 2 of them)
                // there is a zero lenght chunk at the end (1 zero char, and 2 \r\n)
                // in effect there are 3 chunks each with 2 \r\n (or 12 bytes) + the zero char (13 bytes)
                cbT += 13;

                // create the header
                pClientInfo->cbWrite  = BuildHTTPOKStr(true, cbT, ".osjb", (char *) pClientInfo->rgbScratch, sizeof(pClientInfo->rgbScratch));

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

            // Put out what came back for LEX and return there 
            pClientInfo->cbWrite    = oslex.cbOutput;
            pClientInfo->pbOut      = oslex.pbOutput;
            retCMD = GCMD::WRITE;

            pClientInfo->htmlState = JSONLEX;

            // write it out
            retCMD = GCMD::WRITE;
            break;

        case JMPFILENOTFOUND:

            Serial.println("Jumping to HTTP File Not Found page");
            pClientMutex = NULL;
            oslex.Init(OSPAR::ICDEnd);
            return(JumpToComposeHTMLPage(pClientInfo, ComposeHTTP404Error));
            break;

        case HTTPTIMEOUT:

            Serial.println("Timeout error occurred, closing the session");

            // fall thru to close

        case HTTPDISCONNECT:

            if(pClientMutex == pClientInfo)
            {
                int32_t i;
                
                for(i=0; i<oslex.cOData; i++)
                {
                    if(oslex.odata[i].pLockState != NULL && *oslex.odata[i].pLockState == LOCKOutput)
                    {
                        *oslex.odata[i].pLockState = LOCKAvailable;
                    }
                }

                // we are no longer parsing JSON
                oslex.Init(OSPAR::ICDEnd);

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
