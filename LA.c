
/************************************************************************/
/*                                                                      */
/*    LA.C                                                              */
/*                                                                      */
/*    Logic Analyzer Instrument code                                    */
/*                                                                      */
/************************************************************************/
/*    Author:     Keith Vogel                                           */
/*    Copyright 2016, Digilent Inc.                                     */
/************************************************************************/
/************************************************************************/
/*  Revision History:                                                   */
/*                                                                      */
/*    3/20/2017 (KeithV): Created                                        */
/************************************************************************/
#include <OpenScope.h>


STATE LARun(HINSTR hLA, ILA * pila)
{
    LA * pLA = (LA *) hLA;
    
    if(hLA == NULL)
    {
        return(STATEError);
    }

    if(!(pLA->comhdr.activeFunc == LAFnRun || pLA->comhdr.activeFunc == SMFnNone))
    {
        return (Waiting);
    }

    switch(pLA->comhdr.state)
    {
        case Idle:

            // otherwise this was all set up
            // just fall right on thru and start the setup of the Run
            // fall thru

            pLA->comhdr.activeFunc          = LAFnRun;          // need to do this for the fall thru case
            pLA->comhdr.state               = Working;
                        
            // DMA setup, this is shared with the Logic Analyzer, so we have to set it up each time
            pLA->pDMA->DCHxCON.CHEN         = 0;                // make sure the DMA is disabled
            pLA->pDMA->DCHxDSA              = pLA->addPhyDst;   // RAM location for the LA
            pLA->pDMA->DCHxDSIZ             = pLA->cbDst;       // destination size of our target buffer
            pLA->pDMA->DCHxSSA              = pLA->addPhySrc;   // physical address of the source PORT E
            pLA->pDMA->DCHxSSIZ             = 2;                // how many bytes (not items) of the source buffer
            pLA->pDMA->DCHxINTClr           = 0xFFFFFFFF;       // clear all interrupts   

            pLA->pTMR->PRx                  = PeriodToPRx(pila->bidx.tmrPeriod);
            pLA->pTMR->TxCON.TCKPS          = pila->bidx.tmrPreScalar;
            pLA->pTMR->TMRx                 = 0;

            pLA->pDMA->DCHxCON.CHEN         = 1;                // enable the DMA channel
            pLA->pTMR->TxCON.ON             = 1;                // turn the timer on
            break;
 
        case Working:
            // we want to fill the history buffer with at least half the buffer so we can look 
            // back into time. Wait until the whole buffer (BLOCK) transfer is complete
            if(((int32_t) pLA->pDMA->DCHxDPTR/2) >= pila->bidx.cBeforeTrig || pLA->pDMA->DCHxINT.CHBCIF)           // full buffer
            {
                // now go arm the 
                pLA->comhdr.state      = Armed;
            }
            break;
            
        case Armed:
            if(!pLA->pTMR->TxCON.ON)
            {
                UnLockLA();
                pLA->comhdr.state      = Idle;
                pLA->comhdr.activeFunc = SMFnNone;
            }
            break;
            
        default:
            ASSERT(NEVER_SHOULD_GET_HERE);
            UnLockLA();
            pLA->comhdr.state      = Idle;
            pLA->comhdr.activeFunc = SMFnNone;
            return(STATEError); 
    }
    
    return(pLA->comhdr.state);
}

STATE LAReset(HINSTR hLA)
{
    LA * pLA = (LA *) hLA;
    
    if(IsLALocked())
    {
        T7CONbits.ON        = 0;                // turn off trigger timer
        DCH7CONbits.CHEN    = 0;                // turn off DMA
        DCH7DSA             = KVA_2_PA(&LATH);  // Latch H address for destination
        DCH7DSIZ            = 2;                // destination size 2 byte
        DCH7CSIZ            = 2;                // cell transfer size 2 byte
        DCH7SSIZ            = 0;                // init to zero
    }

    pLA->comhdr.activeFunc     = SMFnNone;
    pLA->comhdr.state          = Idle;

    return(Idle);
}


