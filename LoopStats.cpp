/************************************************************************/
/*                                                                      */
/*    LoopStats.cpp                                                     */
/*                                                                      */
/*    Calculate loop statistics                                         */
/*                                                                      */
/*      Use the following command to get them                           */
/*                                                                      */
/*      {"device":[{"command":"loopStats"}]}                            */
/*                                                                      */
/************************************************************************/
/*    Author:     Keith Vogel                                           */
/*    Copyright 2017, Digilent Inc.                                     */
/************************************************************************/
/*  Revision History:                                                   */
/*    9/28/2017(KeithV): Created                                        */
/************************************************************************/
#include    <OpenScope.h>

uint32_t            tLoop               = 0;
uint32_t            aveLoopTime         = 0;
uint32_t            maxLoopTime         = 0;
uint32_t            minLoopTime         = 0xFFFFFFFF;

uint32_t            maxLogWrite         = 0;
uint32_t            maxLogWrittenCnt    = 0;
uint32_t            aveLogWrite         = 0;
uint32_t            cLogWrite           = 0;  

static  uint32_t    tSLoop              = ReadCoreTimer();
static  uint32_t    cAve                = 0;

STATE LoopStatsTask(void)
{   
    // these are the loop status
    uint32_t    tELoop          = ReadCoreTimer(); // need this as we need tLoop to be 32 bits unsigned, tAve is 64 bits
    uint64_t    tAveSum;

    tLoop       = tELoop - tSLoop; // need this as we need tLoop to be 32 bits unsigned, tAve is 64 bits

    maxLoopTime = max(maxLoopTime, tLoop);
    minLoopTime = min(minLoopTime, tLoop);

    // this is average loop time
    tAveSum     = cAve * ((uint64_t) aveLoopTime) + tLoop;                  // max min loop time
    cAve++;
    aveLoopTime = (uint32_t) ((tAveSum + cAve/2) / cAve);
    if(cAve >= 1000) cAve = 999;
    tSLoop = tELoop;
 
    return(Idle);
}

