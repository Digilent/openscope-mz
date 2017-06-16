/************************************************************************/
/*                                                                      */
/*    OSSerial.cpp                                                      */
/*                                                                      */
/*    The Serial object used by the OpenScope                           */
/*                                                                      */
/************************************************************************/
/*    Author:     Keith Vogel                                           */
/*    Copyright 2017, Digilent Inc.                                     */
/************************************************************************/
/************************************************************************/
/*  Revision History:                                                   */
/*    1/20/2017(KeithV): Created                                        */
/************************************************************************/
#include    "OpenScope.h"

// print methods
size_t OSSerial::print(const char sz[]) 
{ 
    uint32_t cb = strlen(sz);
    uint32_t iSz = 0;

    // if we are NOT printing debug prints
    // typically when in JSON mode
    if(fModeJSON && !fEnablePrint) return(cb);

    while(cb > 0)
    {
        uint32_t cbNow = ((iPrintBuffEnd + cb) > sizeof(_printBuff)) ? sizeof(_printBuff) - iPrintBuffEnd : cb;

        memcpy(&printBuff[iPrintBuffEnd], &sz[iSz], cbNow);
        iSz += cbNow;
        cb -= cbNow;
        iPrintBuffEnd = (iPrintBuffEnd + cbNow) % sizeof(_printBuff);
    }

    return(cb);
 }

size_t OSSerial::print(char ch) { 

    // if we are NOT printing debug prints
    // typically when in JSON mode
    if(fModeJSON && !fEnablePrint) return(1);

    printBuff[iPrintBuffEnd++] = (uint8_t) ch;
    iPrintBuffEnd %= sizeof(_printBuff);

    return(1);
}

void OSSerial::PeriodicTask(volatile void * pDMA)
{
    volatile DMA& dma = *((DMA *) pDMA);

    // if any DMA is being done, don't write more out now
    if(isDMATxDone())
    {
        // if the DMA is done, then we know cbDMA is out the door
        // adjust our point to reflect it is done.
        if(cbDMA > 0)
        {
            iPrintBuffStart = (iPrintBuffStart + cbDMA) % sizeof(_printBuff);
            cbDMA = 0;
        }

        if(!oslex.fLocked && iPrintBuffStart != iPrintBuffEnd)
        {
            bool fWritten;

            cbDMA = (iPrintBuffStart < iPrintBuffEnd) ? iPrintBuffEnd - iPrintBuffStart : sizeof(_printBuff) - iPrintBuffStart;
            fWritten = Serial.writeBuffer(&printBuff[iPrintBuffStart], cbDMA, &dma);

            // this should always pass because we know the DMA was done before entering.
            // otherwise no bytes would have been printed
            ASSERT(fWritten);
        }
    }
}

