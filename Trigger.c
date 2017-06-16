/************************************************************************/
/*                                                                      */
/*    Trigger.c                                                         */
/*                                                                      */
/*    Instrument triggers                                               */
/*                                                                      */
/************************************************************************/
/*    Author:     Keith Vogel                                           */
/*    Copyright 2016, Digilent Inc.                                     */
/************************************************************************/
/*  Revision History:                                                   */
/*                                                                      */
/*    8/10/2016 (KeithV): Created                                        */
/************************************************************************/
#include <OpenScope.h>



void 
__attribute__((nomips16)) _simple_tlb_refill_exception_handler (void)
{
    LATJSET = 0x16;     
  __builtin_software_breakpoint();  
}

/************************************************************************/
/*                   Examples of interrupt routines:                    */
/*                                                                      */
// void __attribute__((nomips16, at_vector(_ADC_DC1_VECTOR),interrupt(IPL7SRS))) TriggerISR(void)
// void __attribute__((nomips16, at_vector(_ADC_DC1_VECTOR),interrupt(IPL7SOFT))) TriggerISR(void)
// void __attribute__((nomips16, at_vector(_ADC_DC1_VECTOR),interrupt())) TriggerISR(void)
/*                                                                      */
/*                                                                      */
/************************************************************************/
void __attribute__((nomips16, at_vector(_ADC_DC1_VECTOR),interrupt(IPL5SRS))) TrigISR1(void)
{
    // immediately turn on the next interrupt which is a higher
    // priority and could preemt this interrupt and that is fine
//    IFS1CLR     = _IFS1_ADCDC2IF_MASK;    This should already be cleared
//    IEC1SET     = _IEC1_ADCDC2IE_MASK;
    ADCCMPCON2bits.ENDCMP   = 1;  // enable the next compare module

    // we hit the trigger, don't run this again
    // we will not take a nested interrupt because must run at a 
    // higher priority to be interrupted
    ADCCMPCON1bits.ENDCMP = 0;
    IEC1CLR = _IEC1_ADCDC1IE_MASK;
    IFS1CLR = _IFS1_ADCDC1IF_MASK;  
}

static void Trig2(void)
{
    // do not initialize, we want to keep this code fast and not do anything before we
    // turn on the timer; not even run initalizers
    int16_t *  pBuff;
    uint16_t * puBuff;
    int32_t cBuff;
    bool fInterleave;
    uint16_t curLAValue;
    int32_t ch1DMATrig;
    int32_t ch2DMATrig1;
    int32_t ch2DMATrig2;
    int32_t laDMATrig1;
    int32_t laDMATrig2;
    uint16_t ChangeNotice;

    // Most likely order is we will be working on ch1 of the OSC, then ch2, than the LA
    laDMATrig1  = DCH7DPTR;     // hard coded this to the LA DMA pointer
    ch2DMATrig1 = DCH5DPTR;     // hard coded this to the ADC2 DMA pointer
    ch1DMATrig  = DCH3DPTR;     // hard coded this to the ADC0 DMA pointer 
    ch2DMATrig2 = DCH5DPTR;     // hard coded this to the ADC2 DMA pointer
    laDMATrig2  = DCH7DPTR;     // hard coded this to the LA DMA pointer
    ChangeNotice = CNFE;

    // turn on the timer, get it going right now
    // it is a higher priority and will interrupt this interrupt routine
    // but we can go about searching the buffer while the timer is running, it will complete
//    IFS1bits.T9IF   = 0;    
//    IEC1bits.T9IE   = 1;
    T9CONSET = _T9CON_ON_MASK;  // Turn on the timer

    // we hit the trigger, don't run this again
    ADCCMPCON2bits.ENDCMP = 0;
    IEC1CLR = _IEC1_ADCDC2IE_MASK;
    IFS1CLR = _IFS1_ADCDC2IF_MASK;  
    IEC3CLR = _IEC3_CNEIE_MASK;
    IFS3CLR = _IFS3_CNEIF_MASK;

    // average out our trigger points
    // this is only to sync the instruments together in timing
    // it is not used for searching   
    if(pjcmd.ioscCh1.bidx.fInterleave)  pjcmd.ioscCh1.bidx.iDMATrig  = ch1DMATrig;
    else                                pjcmd.ioscCh1.bidx.iDMATrig  = ch1DMATrig/2;
    
    if(pjcmd.ioscCh2.bidx.fInterleave)  pjcmd.ioscCh2.bidx.iDMATrig  = (ch2DMATrig1 + ch2DMATrig2) / 2;
    else                                pjcmd.ioscCh2.bidx.iDMATrig  = (ch2DMATrig1 + ch2DMATrig2) / 4;
       
    pjcmd.ila.bidx.iDMATrig      = (laDMATrig1 + laDMATrig2) / 4;

    // now we have a little time. The timer routine has high priority so it will stop
    // when the timer elapses. We can only need to find the crossing point before
    // DMA buffer wraps and we lose the crossing point.

    // who is the trigger source
    switch(pjcmd.trigger.idTrigSrc)
    {
        case OSC1_ID:
            pjcmd.trigger.indexBuff = ch1DMATrig/2; // pick the highest DMA point
            fInterleave = pjcmd.ioscCh1.bidx.fInterleave;
            cBuff = AINDMASIZE;
            if(fInterleave) 
            {
                cBuff /= 2;
            }
            pBuff = rgOSC1Buff;
            break;

        case OSC2_ID:
            pjcmd.trigger.indexBuff = ch2DMATrig2 / 2;  // pick the highest DMA point
            fInterleave = pjcmd.ioscCh2.bidx.fInterleave;
            cBuff = AINDMASIZE;
            if(fInterleave) 
            {
                cBuff /= 2;
            }
            pBuff = rgOSC2Buff;
            break;

        case LOGIC1_ID:
            curLAValue = PORTE;
            pjcmd.trigger.indexBuff = laDMATrig2 / 2;   // pick the highest DMA point
            cBuff = LADMASIZE;
            puBuff = rgLOGICBuff;
            break;

        default:
            ASSERT(NEVER_SHOULD_GET_HERE);
            return;
            break;
    }

    // DMA pointer, points one past the last transferred; so back it up to valid data
    pjcmd.trigger.indexBuff--;
    if(pjcmd.trigger.indexBuff<0) pjcmd.trigger.indexBuff = cBuff-1;

    if(pjcmd.trigger.idTrigSrc == LOGIC1_ID)
    {
        int32_t iStart = pjcmd.trigger.indexBuff;

        ASSERT(ChangeNotice != 0);

        // only look back so far and then quit, LAOVERSIZE is just an arbitrary number
        while((iStart - pjcmd.trigger.indexBuff) % cBuff < LAOVERSIZE)
        {
            uint16_t preLAValue = puBuff[pjcmd.trigger.indexBuff];

            // look to see where the change occured between two sampled values
            if(((preLAValue ^ curLAValue) & ChangeNotice) != 0)
            {
                // go just past the trigger
                pjcmd.trigger.indexBuff = (pjcmd.trigger.indexBuff + 1) % LADMASIZE;
                break;
            }

            curLAValue = preLAValue;
            pjcmd.trigger.indexBuff--;
            if(pjcmd.trigger.indexBuff<0) pjcmd.trigger.indexBuff = cBuff-1;
        }

        // if we got here, then we are LAOVERSIZE, too far back
        // This means the CN triggered but the input signal toggled back before
        // we DMA the data (too slow sample rate), so the best we can do is guess where the trigger is 
        if((iStart - pjcmd.trigger.indexBuff) % cBuff >= LAOVERSIZE) pjcmd.trigger.indexBuff = iStart;
    }

    // rising edge  
    // some assumptions about the DMA and the ADCDATAx buffers.
    // we will get the interrupt when the ADCDATA meets the criteria, however the DMA may not have
    // transferred it yet. However, the DMA is triggered on the completion event of the ADC, so the DMA
    // will happen very fast, much faster than we can get into this interrupt routine. So we know we will
    // have the value in here. Now the memory fills in with this interleaved buffer first, then the high 
    // buffer second. We know that if we hit the high value, the low must be behind it in either this or 
    // the last buffer. We always know the low value must be at least on DMA index before, which means 
    // we can check the next value in the high buffer without worry that the DMA hasn't put it there.
    else if(pjcmd.trigger.triggerType == TRGTPRising)
    { 
        int32_t iStart = pjcmd.trigger.indexBuff;
        int16_t curADCValue = pBuff[pjcmd.trigger.indexBuff];
        int16_t preADCValue;

        // only look back so far and then quit, LAOVERSIZE is just an arbitrary number
        while((iStart - pjcmd.trigger.indexBuff) % cBuff < AINOVERSIZE)
        {
            // we are backing put the index for the previous value, so we know the value at
            // pjcmd.trigger.indexBuff+cBuff (the high interleaved value) is already there.
            pjcmd.trigger.indexBuff--;
            if(pjcmd.trigger.indexBuff<0) pjcmd.trigger.indexBuff = cBuff-1;    
            preADCValue = pBuff[pjcmd.trigger.indexBuff];

           // look to see where the change occured between two sampled values
            if(preADCValue < ((int16_t) ADCCMP2bits.DCMPHI) && ((int16_t) ADCCMP2bits.DCMPHI) <= curADCValue)
            {

                // now see if interleaving, if the other buffer is low too
                // our index is currently at the preADCValue or below the trigger point
                // we know we can look in the other buffer because pjcmd.trigger.indexBuff is at least
                // one behind the last value DMA in, and therefore the other interleaved buffer is there as well
                if(fInterleave)
                {
                    int32_t indexBuffOdd = pjcmd.trigger.indexBuff + cBuff;     // look at the next point in the other buffer
                    pjcmd.trigger.indexBuff *= 2;
                    if(pBuff[indexBuffOdd] < ((int16_t) ADCCMP2bits.DCMPHI)) 
                    {
                        pjcmd.trigger.indexBuff++;
                    }                    
                }

                // restore our index to curADCValue index
                // this is one past the transition point.
                pjcmd.trigger.indexBuff = (pjcmd.trigger.indexBuff + 1) % AINDMASIZE;
                break;
            }

            curADCValue = preADCValue;
        }

        // if we got here, something really bad happened because we just triggered our trigger
        // which means the previous point must be lower.
        if((iStart - pjcmd.trigger.indexBuff) % cBuff >= AINOVERSIZE) 
        {
            ASSERT(NEVER_SHOULD_GET_HERE);
            if(fInterleave) iStart *= 2;
            pjcmd.trigger.indexBuff = iStart;
        }
    }

    //falling edge
    else
    { 
        int32_t iStart = pjcmd.trigger.indexBuff;
        int16_t curADCValue = pBuff[pjcmd.trigger.indexBuff];
        int16_t preADCValue;

        // only look back so far and then quit, LAOVERSIZE is just an arbitrary number
        while((iStart - pjcmd.trigger.indexBuff) % cBuff < AINOVERSIZE)
        {
            // we are backing put the index for the previous value, so we know the value at
            // pjcmd.trigger.indexBuff+cBuff (the high interleaved value) is already there.
            pjcmd.trigger.indexBuff--;
            if(pjcmd.trigger.indexBuff<0) pjcmd.trigger.indexBuff = cBuff-1;    
            preADCValue = pBuff[pjcmd.trigger.indexBuff];

           // look to see where the change occured between two sampled values
            if(preADCValue > ((int16_t) ADCCMP2bits.DCMPLO) && ((int16_t) ADCCMP2bits.DCMPLO) >= curADCValue)
            {

                // now see if interleaving, if the other buffer is low too
                // our index is currently at the preADCValue or below the trigger point
                // we know we can look in the other buffer because pjcmd.trigger.indexBuff is at least
                // one behind the last value DMA in, and therefore the other interleaved buffer is there as well
                if(fInterleave)
                {
                    int32_t indexBuffOdd = pjcmd.trigger.indexBuff + cBuff;     // look at the next point in the other buffer
                    pjcmd.trigger.indexBuff *= 2;
                    if(pBuff[indexBuffOdd] > ((int16_t) ADCCMP2bits.DCMPLO)) 
                    {
                        pjcmd.trigger.indexBuff++;
                    }
                }

                // restore our index to curADCValue index
                // this is one past the transition point.
                pjcmd.trigger.indexBuff = (pjcmd.trigger.indexBuff + 1) % AINDMASIZE;
                break;
            }

            curADCValue = preADCValue;
        }

        // if we got here, something really bad happened because we just triggered our trigger
        // which means the previous point must be higher.
        if((iStart - pjcmd.trigger.indexBuff) % cBuff >= AINOVERSIZE) 
        {
            ASSERT(NEVER_SHOULD_GET_HERE);
            if(fInterleave) iStart *= 2;
            pjcmd.trigger.indexBuff = iStart;
        }
    }
}

void __attribute__((nomips16, at_vector(_CHANGE_NOTICE_E_VECTOR),interrupt(IPL6SRS))) TrigISR2LA(void)
{
    Trig2();
}

void __attribute__((nomips16, at_vector(_ADC_DC2_VECTOR),interrupt(IPL6SRS))) TrigISR2OSC(void)
{
    Trig2();
}

// each tick on the timer is 10ns, While we can do about 2 instructions per tick
// lets assume we can only do 1 instruction per tick
// how many instructions should we allow for to complete our work
#define nbrTickTooCloseToMove  20l
void __attribute__((nomips16, at_vector(_TIMER_9_VECTOR),interrupt(IPL7SRS))) TrigISRQuit(void)
{
    volatile int32_t iTMR;
    uint32_t cTMR;

    // if all we are doing is dec the count
    if(pjcmd.trigger.cTMR != 0)
    {
        IFS1CLR = _IFS1_T9IF_MASK;
        pjcmd.trigger.cTMR--;
        return;
    }

    // we have quite a bit of time, 65536 * 10^^-8, some 655usec. (> 1/2 msec).
    // the timer is going to have a low count in it, even if we process all instruments
    // we will be done long before the count rolls
    iTMR = 0;
    cTMR = 0;
    while(cTMR == 0 && iTMR <= 0)
    {
        // who is the target
        switch(pjcmd.trigger.rgtte[pjcmd.trigger.iTTE].instrID)
        {
            case OSC1_ID:
                T3CONCLR = _T3CON_ON_MASK;                  // Stop taking DMA samples on the ADC0/1 channel
                break;

            case OSC2_ID:
                T5CONCLR = _T5CON_ON_MASK;                  // Stop taking DMA samples on the ADC2/3 channel
                break;

            case LOGIC1_ID:
                T7CONCLR = _T7CON_ON_MASK;                  // Stop taking DMA samples on the LA channel
                break;

            default:
                ASSERT(NEVER_SHOULD_GET_HERE);
                TRGAbort();
                return;
                break;
        }

        // go to the next target
        pjcmd.trigger.iTTE++;

        // were done, get out
        if(pjcmd.trigger.iTTE >= pjcmd.trigger.cRun)
        {
            T9CONCLR    = _T9CON_ON_MASK;  // Stop the delay trigger timer
            IEC1CLR     = _IEC1_T9IE_MASK;
            IFS1CLR     = _IFS1_T9IF_MASK;
            return;
        }

        // get the roll count
        cTMR = pjcmd.trigger.rgtte[pjcmd.trigger.iTTE].cTMR;

        // accumulate any previous time into this time
        iTMR += pjcmd.trigger.rgtte[pjcmd.trigger.iTTE].iTMR;

        // if we are too close, then we just spin until we are into the next cTMR
        while((iTMR - ((int32_t) ((uint32_t) TMR9))) < nbrTickTooCloseToMove)
        {

            // we broke down into the next count
            // I am probably going to drop behind by 1 Tick
            // in the delay for setting my iTMR and the clearing of TMR9
            // but out slop is 64 ticks, so if all instruments fall behind by a 
            // tick, that is a falling behind of 5 or so ticks.
            if((iTMR - ((int32_t) ((uint32_t) TMR9))) <= 0)
            {
                if(cTMR > 0)
                {
                    cTMR--;
                    iTMR += 0x00010000l;
                    iTMR -= ((int32_t) ((uint32_t) TMR9));
                    TMR9 = 0;
                }
                else
                {
                   iTMR -= ((int32_t) ((uint32_t) TMR9));
                   TMR9 = 0;
                }

                // break out of the while loop
                break;
            }
        }            
    }

    // at this point we have more than nbrTickTooCloseToMove time
    // so we have plenty of time to set the TMR
    pjcmd.trigger.cTMR = cTMR;
    iTMR = ((0x10000 - iTMR) % 0x10000);

    IFS1CLR = _IFS1_T9IF_MASK;
    iTMR += ((int32_t) ((uint32_t) TMR9));
    TMR9 = (uint16_t) iTMR;
}

void TRGAbort(void)
{

    // turn everything off
    T9CONbits.ON = 0;       // Stop the delay trigger timer
    T3CONbits.ON = 0;       // Stop taking DMA samples on the ADC0/1 channel
    T5CONbits.ON = 0;       // Stop taking DMA samples on the ADC2/3 channel
    IEC3CLR = _IEC3_CNEIE_MASK;     // Stop any LA interrupts

    // got to check to see if the DMA is working on the logic analyzer
    if(IsLALocked())
    {       
        T7CONbits.ON = 0;       // Stop taking LA samples
        UnLockLA();
    }

    ADCCMPCON1bits.ENDCMP   = 0;    // stop the compare module
    ADCCMPCON2bits.ENDCMP   = 0;    // stop the compare module

    // clear all interrupts
    IEC1CLR     = _IEC1_T9IE_MASK;
    IFS1CLR     = _IFS1_T9IF_MASK;    
    IEC1CLR     = _IEC1_ADCDC1IE_MASK;
    IFS1CLR     = _IFS1_ADCDC1IF_MASK;    
    IEC1CLR     = _IEC1_ADCDC2IE_MASK;
    IFS1CLR     = _IFS1_ADCDC2IF_MASK;  
    IEC3CLR     = _IEC3_CNEIE_MASK;
    IFS3CLR     = _IFS3_CNEIF_MASK;
}

bool TRGSetUp(void)
{
    uint32_t    i;
    bool        fDone;
    int64_t     curTickCnt;
    int16_t     dadcLower;      // mvLower converted to the ADC compare value based on gain and offset
    int16_t     dadcHigher;     // mvHigher converted to the ADC compare value based on gain and offset

    // set up our DMA termination times for each instrument
    for(i = 0; i< pjcmd.trigger.cRun; i++)
    {

        switch(pjcmd.trigger.rgtte[i].instrID)
        {
            case OSC1_ID:
                pjcmd.trigger.rgtte[i].tmrTicks = pjcmd.ioscCh1.bidx.cDelayTmr;
                break;

            case OSC2_ID:
                pjcmd.trigger.rgtte[i].tmrTicks = pjcmd.ioscCh2.bidx.cDelayTmr;
                break;

            case LOGIC1_ID:
                pjcmd.trigger.rgtte[i].tmrTicks = pjcmd.ila.bidx.cDelayTmr;
                break;

            default:
                ASSERT(NEVER_SHOULD_GET_HERE);
                return(false);
                break;
        }
    }

    // sort the targets
    fDone = false;
    while(!fDone)
    {
        fDone = true;
        for(i = 0; i < pjcmd.trigger.cRun-1; i++)
        {
            // if the earlier one is greater than the next
            if(pjcmd.trigger.rgtte[i].tmrTicks > pjcmd.trigger.rgtte[i+1].tmrTicks)
            {
                TTE tteT;

                // exchange the order
                memcpy(&tteT, &pjcmd.trigger.rgtte[i], sizeof(TTE));
                memcpy(&pjcmd.trigger.rgtte[i], &pjcmd.trigger.rgtte[i+1], sizeof(TTE));
                memcpy(&pjcmd.trigger.rgtte[i+1], &tteT, sizeof(TTE));

                // say we are not done
                fDone = false;
            }
        }
    }

    // create the timer events
    curTickCnt = 0;
    for(i = 0; i < pjcmd.trigger.cRun; i++)
    {
        int64_t nextTmr = pjcmd.trigger.rgtte[i].tmrTicks - curTickCnt;
        curTickCnt = pjcmd.trigger.rgtte[i].tmrTicks;

        // How many times we need to cycle the TMR before to meet our delay
        // the timer rolls about once every 655.36 usec ~ 1/2 msec
        pjcmd.trigger.rgtte[i].cTMR  = (uint32_t) (nextTmr / 0x10000);

        // what is the initial value for the timer, this is the modulo
        // of the full timer count down time. Remember the match count is 0xFFFF
        pjcmd.trigger.rgtte[i].iTMR = (int32_t) (nextTmr % 0x10000);
    }

    // set up the delay timer
    T9CON           = 0;                    
    T9CONbits.TCKPS = 0;        // pre scalar of zero
    PR9 = 0xFFFF;               // max roll buffer of 65536 count
          
    // clear Digital compare registers and delay timer
    ADCCMP1 = 0;
    ADCCMPEN1 = 0; 
    ADCCMPCON1 = 0;
    ADCCMP2 = 0;
    ADCCMPEN2 = 0; 
    ADCCMPCON2 = 0;
    CNCONE = 0;

    // clear all interrupts
    IEC1CLR     = _IEC1_T9IE_MASK;
    IFS1CLR     = _IFS1_T9IF_MASK;    
    IEC1CLR     = _IEC1_ADCDC1IE_MASK;
    IFS1CLR     = _IFS1_ADCDC1IF_MASK;    
    IEC1CLR     = _IEC1_ADCDC2IE_MASK;
    IFS1CLR     = _IFS1_ADCDC2IF_MASK;  
    IEC3CLR     = _IEC3_CNEIE_MASK;
    IFS3CLR     = _IFS3_CNEIF_MASK;

    // set the trigger condition
    switch(pjcmd.trigger.idTrigSrc)
    {
        case OSC1_ID:
            dadcLower = OSCDadcFromVinGainOffset(((OSC *) rgInstr[pjcmd.trigger.idTrigSrc]), pjcmd.trigger.mvLower, pjcmd.ioscCh1.gain-1, pjcmd.ioscCh1.mvOffset);
            dadcHigher = OSCDadcFromVinGainOffset(((OSC *) rgInstr[pjcmd.trigger.idTrigSrc]), pjcmd.trigger.mvHigher, pjcmd.ioscCh1.gain-1, pjcmd.ioscCh1.mvOffset);
            ADCCMPEN1 = 0b0001;         // set the bit corresponding to AN0
            ADCCMPEN2 = 0b0001;         // set the bit corresponding to AN0
            break;

        case OSC2_ID:
            dadcLower = OSCDadcFromVinGainOffset(((OSC *) rgInstr[pjcmd.trigger.idTrigSrc]), pjcmd.trigger.mvLower, pjcmd.ioscCh2.gain-1, pjcmd.ioscCh2.mvOffset);
            dadcHigher = OSCDadcFromVinGainOffset(((OSC *) rgInstr[pjcmd.trigger.idTrigSrc]), pjcmd.trigger.mvHigher, pjcmd.ioscCh2.gain-1, pjcmd.ioscCh2.mvOffset);
            ADCCMPEN1 = 0b0100;         // set the bit corresponding to AN2
            ADCCMPEN2 = 0b0100;         // set the bit corresponding to AN2
            break;

        case LOGIC1_ID:
            CNCONEbits.EDGEDETECT = 1;
            CNNEE = pjcmd.trigger.negEdge;
            CNENE = pjcmd.trigger.posEdge;
            break;

        case FORCE_TRG_ID:
            break;

        default:
            ASSERT(NEVER_SHOULD_GET_HERE);
            return(false);
            break;
    }

    switch(pjcmd.trigger.idTrigSrc)
    {
        case OSC1_ID:
        case OSC2_ID:
            if(pjcmd.trigger.triggerType == TRGTPRising)
            {
                // Set the below low
                ADCCMP1bits.DCMPHI = 0;
                ADCCMP1bits.DCMPLO = dadcLower;
                ADCCMPCON1bits.IELOLO = 1;      // Create an event when the measured result is
                ADCCMPCON1bits.DCMPGIEN = 1;    // generate an interrupt
 
                // Set the above high
                ADCCMP2bits.DCMPLO = 0;
                ADCCMP2bits.DCMPHI = dadcHigher;
                ADCCMPCON2bits.IEHIHI = 1;      // Create an event when the measured result is
                ADCCMPCON2bits.DCMPGIEN = 1;    // generate an interrupt
            }

            // Falling osc Edge
            else if(pjcmd.trigger.triggerType == TRGTPFalling)
            {
                // Set the above high
                ADCCMP1bits.DCMPLO = 0;
                ADCCMP1bits.DCMPHI = dadcHigher;
                ADCCMPCON1bits.IEHIHI = 1;      // Create an event when the measured result is
                ADCCMPCON1bits.DCMPGIEN = 1;    // generate an interrupt
        
                // Set the below low
                ADCCMP2bits.DCMPHI = 0;
                ADCCMP2bits.DCMPLO = dadcLower;
                ADCCMPCON2bits.IELOLO = 1;      // Create an event when the measured result is
                ADCCMPCON2bits.DCMPGIEN = 1;    // generate an interrupt
            }
            break;

        case LOGIC1_ID:
            CNFE = 0;
            break;

        case FORCE_TRG_ID:
            break;

        default:
            ASSERT(NEVER_SHOULD_GET_HERE);
            return(false);
            break;
    }

    return(true);
}

bool TRGSingle(void)
{
//    uint32_t i;

    if(IEC1bits.T9IE || IEC1bits.ADCDC1IE || IEC1bits.ADCDC2IE || IEC3bits.CNEIE)
    {
        return(false);
    }

    // turn everything off
    T9CONbits.ON = 0;
    ADCCMPCON1bits.ENDCMP   = 0;
    ADCCMPCON2bits.ENDCMP   = 0;
    CNCONEbits.ON = 0;

    // clear all interrupts
    IEC1CLR     = _IEC1_T9IE_MASK;
    IFS1CLR     = _IFS1_T9IF_MASK;    
    IEC1CLR     = _IEC1_ADCDC1IE_MASK;
    IFS1CLR     = _IFS1_ADCDC1IF_MASK;    
    IEC1CLR     = _IEC1_ADCDC2IE_MASK;
    IFS1CLR     = _IFS1_ADCDC2IF_MASK;
    IEC3CLR     = _IEC3_CNEIE_MASK;
    IFS3CLR     = _IFS3_CNEIF_MASK;

    // set up the delay timer
    pjcmd.trigger.iTTE = 0;                             // first trigger in target list
    TMR9 = (uint16_t) ((0x10000 - pjcmd.trigger.rgtte[0].iTMR) % 0x10000);      // initial timer count

    // if we are going to have a full count in the TMR9, then reduce the loop count
    if(pjcmd.trigger.rgtte[0].iTMR == 0 && pjcmd.trigger.rgtte[0].cTMR > 0) pjcmd.trigger.cTMR = pjcmd.trigger.rgtte[0].cTMR-1;
    else pjcmd.trigger.cTMR = pjcmd.trigger.rgtte[0].cTMR;

    // get things going
    IEC1SET           = _IEC1_T9IE_MASK;

    switch(pjcmd.trigger.idTrigSrc)
    {
        case OSC1_ID:
        case OSC2_ID:
            IEC1SET       = _IEC1_ADCDC2IE_MASK;
            IEC1SET       = _IEC1_ADCDC1IE_MASK;
            ADCCMPCON1bits.ENDCMP   = 1;  // enable compare module
            break;

        case LOGIC1_ID:
            CNFE            = 0;    // clear the flag register
            IEC3SET         = _IEC3_CNEIE_MASK;    // enable the change notice interrupt
            CNCONEbits.ON   = 1;    // enable the change notice controller        
            break;

        case FORCE_TRG_ID:
            TRGForce();                     // force a trigger
            break;

        default:
            ASSERT(NEVER_SHOULD_GET_HERE);
            return(false);
            break;
    }

    return(true);
}

bool TRGForce(void)
{
    uint32_t intStatus = 0;
    bool    fSetIndex = false;
    int32_t ch1DMATrig;
    int32_t ch2DMATrig1;
    int32_t ch2DMATrig2;
    int32_t laDMATrig1;
    int32_t laDMATrig2;

    // see if we are armed
    if(pjcmd.trigger.state.processing != Armed) return(false);

    // turn off the first
    IEC1CLR       = _IEC1_ADCDC2IE_MASK;
    IEC1CLR       = _IEC1_ADCDC1IE_MASK;
    ADCCMPCON1bits.ENDCMP   = 0;    // stop the compare module ISR1
    ADCCMPCON2bits.ENDCMP   = 0;    // stop the compare module ISR2

    // keep this short and sweet.
    intStatus = OSDisableInterrupts();

    // if the T9 interrupt is enabled, but the T9 timer is not on
    // we need to turn on the timer and take the DMA pointer.
    if(IEC1bits.T9IE && !T9CONbits.ON)
    {
        // Most likely order is we will be working on ch1 of the OSC, then ch2, than the LA
        laDMATrig1  = DCH7DPTR;     // hard coded this to the LA DMA pointer
        ch2DMATrig1 = DCH5DPTR;     // hard coded this to the ADC2 DMA pointer
        ch1DMATrig  = DCH3DPTR;     // hard coded this to the ADC0 DMA pointer 
        ch2DMATrig2 = DCH5DPTR;     // hard coded this to the ADC2 DMA pointer
        laDMATrig2  = DCH7DPTR;     // hard coded this to the LA DMA pointer

        // average out our trigger points
        // this is only to sync the instruments together in timing
        // it is not used for searching   
        if(pjcmd.ioscCh1.bidx.fInterleave)  pjcmd.ioscCh1.bidx.iDMATrig  = ch1DMATrig;
        else                                pjcmd.ioscCh1.bidx.iDMATrig  = ch1DMATrig/2;

        if(pjcmd.ioscCh2.bidx.fInterleave)  pjcmd.ioscCh2.bidx.iDMATrig  = (ch2DMATrig1 + ch2DMATrig2) / 2;
        else                                pjcmd.ioscCh2.bidx.iDMATrig  = (ch2DMATrig1 + ch2DMATrig2) / 4;

        pjcmd.ila.bidx.iDMATrig      = (laDMATrig1 + laDMATrig2) / 4;

        T9CONSET = _T9CON_ON_MASK;  // Turn on the timer
        fSetIndex = true;
    }
    OSRestoreInterrupts(intStatus);

    if(fSetIndex)
    {
        // who is the trigger source
        switch(pjcmd.trigger.idTrigSrc)
        {
            case OSC1_ID:
            case FORCE_TRG_ID:
                pjcmd.trigger.indexBuff = ch1DMATrig / 2;
                if(pjcmd.ioscCh1.bidx.fInterleave) pjcmd.trigger.indexBuff *= 2;
                break;

            case OSC2_ID:
                pjcmd.trigger.indexBuff = ch2DMATrig2 / 2;
                if(pjcmd.ioscCh2.bidx.fInterleave) pjcmd.trigger.indexBuff *= 2;
                break;

            case LOGIC1_ID:
                pjcmd.trigger.indexBuff = laDMATrig2 / 2;
                ASSERT(!pjcmd.ila.bidx.fInterleave);
                break;

            default:
                ASSERT(NEVER_SHOULD_GET_HERE);
                break;
        }
    }

    return(true);
}


