/************************************************************************/
/*                                                                      */
/*    deOpenScopeWebServer                                              */
/*                                                                      */
/*    configuration of the OpenScope Web Server pages                   */
/*                                                                      */
/************************************************************************/
/*    Author:     Keith Vogel                                           */
/*    Copyright 2014-2015, Digilent Inc.                                */
/************************************************************************/
/************************************************************************/
/*  Revision History:                                                   */
/*                                                                      */
/*    3/9/2017(KeithV): Created                                        */
/************************************************************************/
// if building in MPLABX we need to get all of
// the variables need to be defined
// in Arduino IDE, this is done in the .ino where the adaptor is included.
// #ifdef MPLABX
// 512 byte page manager (1<<6 = 64) * cPagesSocketBuffer = bytes assigned to sockets
// cPagesSocketBuffer can not be greater than 254
#define cPagesSocketBuffer 192      // 12,288 bytes for sockets
#define cbAdpHeap   8192            // space for the adaptor to use to pass data
#include <./libraries/MRF24G/MRF24G.h>       // include for hardware SPI
//#endif

#include <OpenScope.h>

/************************************************************************/
/*    HTTP URL Matching Strings                                         */
/************************************************************************/
// These are the HTTP URL match strings for the dynamically created
// HTML rendering functions.
// Make these static const so they get put in flash

static const char szHTMLPostCmd[]       = "POST / ";
static const char szHTMLOptions[]       = "OPTIONS / ";
static const char szHTMLReboot[]        = "GET /Reboot ";
static const char szHTMLFavicon[]       = "GET /favicon.ico ";
static const char szHTMLRedirect[]      = "GET /index.html ";

// here is our sample/example dynamically created HTML page
GCMD::ACTION ComposeHTMLPostCmd(CLIENTINFO * pClientInfo);
GCMD::ACTION ComposeHTMLOptions(CLIENTINFO * pClientInfo);
GCMD::ACTION ComposeHTMLRedirectPage(CLIENTINFO * pClientInfo);

// get rid of as much of the heap as we can, the SD library requires some heap
// #define CHANGE_HEAP_SIZE(size) __asm__ volatile ("\t.globl _min_heap_size\n\t.equ _min_heap_size, " #size "\n")
// CHANGE_HEAP_SIZE(0x200);

STATE HTTPState = Idle;

/***    void setup(void)
 *
 *    Parameters:
 *          None
 *              
 *    Return Values:
 *          None
 *
 *    Description: 
 *    
 *      Arduino Master Initialization routine
 *      
 *      
 * ------------------------------------------------------------ */
STATE HTTPSetup(void)
{   
    
    switch(HTTPState)
    {
        case Idle:
            // add rendering functions for dynamically created web pages
            // max of 10 AddHTMLPage() allowed 

            // the OpenScope Command URL
            AddHTMLPage(szHTMLPostCmd,       ComposeHTMLPostCmd);

            // the Options page
            AddHTMLPage(szHTMLOptions,       ComposeHTMLOptions);

            // Redirects to Waveformslive page
            // AddHTMLPage(szHTMLRedirect,       ComposeHTMLRedirectPage);

            // comment this out if you do not want to support
            // rebooting (effectively hitting MCLR) the server from a browser
            AddHTMLPage(szHTMLReboot,       ComposeHTMLRebootPage);

            // This example supports favorite ICONs, 
            // those are those icon's next to the URL in the address line 
            // on the browser once the page is displayed.
            // To support those icons, have at the root of the SD file direcotory
            // an ICON (.ico) file with your ICON in it. The file MUST be named
            // favicon.ico. If you do not have an icon, then uncomment the following
            // line so the server will tell the browser with an HTTP file not found
            // error that we don't have a favoite ICON.
            // AddHTMLPage(szHTMLFavicon,      ComposeHTTP404Error);

            // Make reading files from the SD card the default compose function
            SetDefaultHTMLPage(ComposeHTMLSDPage);

            // Initialize the HTTP server
            ServerSetup();
            
            HTTPState = Done;
            break;
            
        case HTTPEnabled:
        case Done:
            // stay stuck at the done state.
            // we only init the HTTP server once
            return(Idle);
            break;
            
        // this cover errors as well
        default:
            break;
    }
    
    return(HTTPState);
}

/***    void loop(void) 
 *
 *    Parameters:
 *          None
 *              
 *    Return Values:
 *          None
 *
 *    Description: 
 *    
 *      Arduino Master Loop routine
 *      
 *      
 * ------------------------------------------------------------ */
void HTTPTask(void)
{
    if(HTTPState == HTTPEnabled)
    {
        // process the HTTP Server
        ProcessServer();
    }
}

