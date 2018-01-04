/************************************************************************/
/*                                                                      */
/*    TimeOutTmr9.c                                                     */
/*                                                                      */
/*    Free running timeout timer to stop instruments                    */
/*                                                                      */
/************************************************************************/
/*    Author:     Keith Vogel                                           */
/*    Copyright 2016, Digilent Inc.                                     */
/************************************************************************/
/*  Revision History:                                                   */
/*                                                                      */
/*    8/25/2017 (KeithV): Created                                        */
/************************************************************************/
#include <OpenScope.h>

#define MAXACTIVE   2               
#define CNTLOOP     0x10000

typedef struct _TOENT
{
    uint64_t cLoops;
    uint16_t initTicks;
    INSTR_ID id;
} TOENT;

typedef struct _TOTMR
{
    int32_t     cActive;
    TOENT       rgActive[MAXACTIVE];
} TOTMR;

static volatile TOTMR totmr = {0};
// static uint32_t tTime = 0;

void __attribute__((nomips16)) _simple_tlb_refill_exception_handler (void)
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
// void __attribute__((nomips16, at_vector(_TIMER_9_VECTOR),interrupt(IPL7SRS))) TimeOutTmr9(void)
void __attribute__((nomips16, interrupt(IPL7SRS), section(".user_interrupt"))) TimeOutTmr9(void)
{
    do {

        IFS1CLR = _IFS1_T9IF_MASK;  // clear the flag    

        if(totmr.cActive == 0) 
        {
            break;      // get out of the do/while loop
        }

        else if(totmr.rgActive[0].cLoops > 0)
        {
            totmr.rgActive[0].cLoops--;
        }

        else
        {
            INSTR_ID idCur = totmr.rgActive[0].id;

            // another one in the list needs to be loaded
            if(totmr.cActive > 1)
            {
                uint32_t cTicks = (CNTLOOP - totmr.rgActive[1].initTicks);

                // we know we just rolled, so the TMR9 value is small and we have
                // somewhere around 1/2ms to process this interrupt routine
                // however our next initial value may be small and TMR9 may be 
                //  high enough to get our new value to roll right now
                TMR9 = (uint16_t) (cTicks + TMR9);

                // check to see if the TMR value rolled when updating to the new
                // value, if so, set the IF flag to say a roll occured.
                // it does not matter if we set the IF, or if the TMR actually rolled
                // we know we must set the IF flag.
                // if the following condition is not true, we know a roll is coming up
                // and the IF flag will be set when it does roll
                if(TMR9 < cTicks)
                {
                    IFS1SET = _IFS1_T9IF_MASK;
                }

                // shift the data down
                memcpy((void *) totmr.rgActive, (void *) &totmr.rgActive[1], sizeof(TOENT) * (totmr.cActive-1));
            }

            StopInstrumentTimer(idCur);
            totmr.cActive--;
        }

    } while(IFS1 & _IFS1_T9IF_MASK);

    // if we have no need for the timer, turn it off
    if(totmr.cActive == 0) 
    {
        T9CONCLR    = _T9CON_ON_MASK;   // turn off the timer
        IEC1CLR     = _IEC1_T9IE_MASK;  // disable the TMR9 Interrupt 
        IFS1CLR     = _IFS1_T9IF_MASK;  // clear the flag    
    }
}

// if not running you just add instruments
// if running, you dynamically add instruments on the fly
bool TOInstrumentAdd(uint64_t ps, INSTR_ID id)
{
    bool    fStartOver;
    uint64_t curTicks;
    uint64_t curTicksN;
    uint64_t cTicks     = GetTmrTicksFromPS(ps);
    uint16_t initTicks  = cTicks % CNTLOOP;
    uint64_t cLoops     = cTicks / CNTLOOP;

    ASSERT(ps > 0);

    do
    {
        fStartOver  = false;

        // first one, intialize everything
        if(totmr.cActive == 0)
        {

            ASSERT(T9CONbits.ON == 0);

            // initialize the interrupt
            IEC1CLR                     = _IEC1_T9IE_MASK;  // disable the TMR9 Interrupt 
            IFS1CLR                     = _IFS1_T9IF_MASK;  // clear the flag    

            // initialize the timer values
            T9CONbits.TCKPS             = 0;        // pre scalar of zero
            PR9                         = 0xFFFF;   // max roll buffer of 65536 count, the int will occur at TMR9 == 0, 2 sys clks after match
            TMR9                        = (uint16_t) (CNTLOOP - initTicks); 
            
            // initilalize first entry
            totmr.rgActive[0].cLoops    = cLoops;
            totmr.rgActive[0].initTicks = initTicks;
            totmr.rgActive[0].id        = id;

            // if the count is ready for dec
            if(initTicks == 0) IFS1SET = _IFS1_T9IF_MASK;  // go directly to the ISR to dec the cLoop count 

            // ISR does stuff once you say we are active!
            totmr.cActive               = 1;

            // return that added
            // don't just automatically start the timer as we may want to add a lot
            // to this list before starting.
            return(true);
        }

        // additional adds
        // we may be dynamically adding a timer value,
        // so we must be careful to work on snap shot values
        // all total, we should have .65ms to do this work (the roll of the timer).
        else
        {
            TOTMR       totmrT;
            uint16_t    tmr;
            int32_t     i = 0;

            // we have to snap shot a stable value of table
            do
            {
                tmr = TMR9;     // this is atomic

                // snap short the value, but the ISR may get this in the middle of the copy
                memcpy(&totmrT, (void *) &totmr, sizeof(TOTMR));    // copy safely

                // if the timer rolls, than tmr will be great then the TMR9 value
                // and we have to try again because totmr may have change in the ISR
            } while(tmr > TMR9);

            // a little trick, because we are finding the position in the list
            // we have to fix up the first/active initial ticks to the current value
            // but not, it is possible that the list emptied, and we are just doing math on an invalid entry, that is okay, no harm
            totmrT.rgActive[0].initTicks = (uint16_t) (CNTLOOP - tmr);

            // initTicks could be CNTLOOP; adjust so the cLoops is correct
            if(tmr == 0) totmrT.rgActive[0].cLoops++;

            // coalesce any Nulls out
            for(i=0; i<totmrT.cActive; i++)
            {
                if(totmrT.rgActive[i].id == NULL_ID)
                {
                    // fix up the next entry.
                    if(i < (totmrT.cActive-1)) 
                    {
                        uint64_t iTicks = (totmrT.rgActive[i].cLoops +  totmrT.rgActive[i+1].cLoops) * CNTLOOP + totmrT.rgActive[i].initTicks + totmrT.rgActive[i+1].initTicks;
                        memcpy(&totmrT.rgActive[i], &totmrT.rgActive[i+1], (totmrT.cActive - i - 1));
                        totmrT.rgActive[i].initTicks  = iTicks % CNTLOOP;
                        totmrT.rgActive[i].cLoops     = iTicks / CNTLOOP;
                    }
                    totmrT.cActive--;
                    i--;
                }
            }

            // we should have room for the slot
            ASSERT(totmrT.cActive < MAXACTIVE);

            // things could have finished or coalesced out; only dynamically add if we are still running
            // another timer value.
            if(totmrT.cActive > 0)
            {
                int32_t curActive = totmrT.cActive;

                curTicks = 0;
                curTicksN = totmrT.rgActive[0].cLoops * CNTLOOP + CNTLOOP - tmr;

                // we are adding one to the list; clear the next entry out
                memset(&totmrT.rgActive[totmrT.cActive], 0, sizeof(TOENT));

                // see where we belong in the current list
                for(i=0; i<totmrT.cActive; i++)
                {
                    
                    // if what we are adding is below where we currently are
                    if(cTicks <= curTicksN)
                    {
                        TOENT       rgTOENT[MAXACTIVE];
                        uint64_t    dTicks = curTicksN - cTicks;
                        
                        // fix up the current slot to be the next slot
                        totmrT.rgActive[i].initTicks  = dTicks % CNTLOOP;
                        totmrT.rgActive[i].cLoops     = dTicks / CNTLOOP;

                        // open up the slot, pushing the higher slots up
                        memcpy(rgTOENT, &totmrT.rgActive[i], sizeof(TOENT) * totmrT.cActive-i);
                        memcpy(&totmrT.rgActive[i+1], rgTOENT, sizeof(TOENT) * totmrT.cActive-i);

                        // make our new slot
                        dTicks = cTicks - curTicks;
                        totmrT.rgActive[i].initTicks  = cTicks % CNTLOOP;
                        totmrT.rgActive[i].cLoops     = cTicks / CNTLOOP;
                        totmrT.rgActive[i].id        = id;
                        break;
                    }

                    // we can do this because we know we cleared the entry one past the end of the current list
                    curTicks = curTicksN;
                    curTicksN += totmrT.rgActive[i+1].cLoops * CNTLOOP + totmrT.rgActive[i+1].initTicks;
                }

                // if we are adding to the very end
                if(i == totmrT.cActive)
                {
                    cTicks -= curTicks;     // difference we add
                    totmrT.rgActive[i].initTicks  = cTicks % CNTLOOP;
                    totmrT.rgActive[i].cLoops     = cTicks / CNTLOOP;
                    totmrT.rgActive[i].id        = id;
                }

                // say we have one more in the list
                totmrT.cActive++;

                ASSERT(totmrT.cActive <= MAXACTIVE);

                // Enter critical section, keep this FAST FAST FAST
                // if so, update the tables; currently this runs under 2usec
                OSDisableInterrupts();
                {
                    int32_t nTMR9 = (int32_t) ((int32_t) (CNTLOOP - totmrT.rgActive[0].initTicks) + TMR9) - (int32_t) tmr;
                    bool fRoll = (nTMR9 >= CNTLOOP);

                    if(fRoll) nTMR9 -= CNTLOOP;

//                    tTime = ReadCoreTimer();
                    if((curActive == totmr.cActive) && tmr <= TMR9 && nTMR9 < (0xFFFF - 200) && (IFS1 & _IFS1_T9IF_MASK) == 0)
                    {
                        TMR9 = (uint16_t) nTMR9;            // from here on we have 200 ticks to complete

                        if(fRoll)   IFS1SET = _IFS1_T9IF_MASK;
                        else        IFS1CLR = _IFS1_T9IF_MASK;

                        memcpy((void *) &totmr, &totmrT, sizeof(TOTMR));    
                    }
                    else fStartOver = true;

//                    tTime = (ReadCoreTimer() - tTime);
//                    ASSERT(tTime < 200);
                }
                OSEnableInterrupts();
            }
            
            // if they did finish, start over as cActive == 0;
            // because of Null in the list, just shut down the timer and turn it off
            else  
            {
                T9CONCLR    = _T9CON_ON_MASK;   // turn off the timer
                IEC1CLR     = _IEC1_T9IE_MASK;  // disable the TMR9 Interrupt 
                IFS1CLR     = _IFS1_T9IF_MASK;  // clear the flag    
                totmr.cActive = 0;
                fStartOver = true;
            }
        }

    } while(fStartOver);

    return(true);
}

void TONullInstrument(INSTR_ID id)
{
    uint32_t i;

    OSDisableInterrupts();
    for(i=0; i<totmr.cActive; i++)
    {
        // do not try to turn it off because it might not be in the list
        // and externally we may have to turn it off anyway
        if(totmr.rgActive[i].id == id) totmr.rgActive[i].id = NULL_ID;
    }
    OSEnableInterrupts();
}

bool TOStart(void)
{
    if(T9CONbits.ON)            return(true);
    else if(totmr.cActive == 0) return(false);

    IEC1SET     = _IEC1_T9IE_MASK;  // enable the TMR9 Interrupt 
    T9CONSET    = _T9CON_ON_MASK;   // turn on the timer

    // do not clear the IF flag as we may have manually set that

    return(true);
}

void TOAbort(void)
{
    uint32_t i;
    T9CONCLR    = _T9CON_ON_MASK;   // turn off the timer
    IEC1CLR     = _IEC1_T9IE_MASK;  // disable the TMR9 Interrupt 

    // if we stop the timer then all of the instruments are hosed up

    for(i=0; i<totmr.cActive; i++)
    {
        StopInstrumentTimer(totmr.rgActive[i].id);
    }

    memset((void *) &totmr, 0, sizeof(TOTMR));
}