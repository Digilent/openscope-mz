/************************************************************************/
/*                                                                      */
/*    AnalogIn.C                                                        */
/*                                                                      */
/*    Calibration for the Analog input channels                         */
/*                                                                      */
/************************************************************************/
/*    Author:     Keith Vogel                                           */
/*    Copyright 2016, Digilent Inc.                                     */
/************************************************************************/
/************************************************************************/
/*  Revision History:                                                   */
/*                                                                      */
/*    3/14/2016 (KeithV): Created                                       */
/************************************************************************/
#include <OpenScope.h>

// cDadc must divide by 2 evenly
bool OSCReorder(int16_t rgDadc[], uint32_t cDadc)
{
    int32_t     cHalf = cDadc / 2;
    int32_t     i = 0;
    uint16_t    rgSave[cHalf];

    memcpy(rgSave, &rgDadc[cHalf], (sizeof(rgDadc[0]) * cHalf));

    for(i=cHalf-1; i>0; i--) rgDadc[i*2] = rgDadc[i];
    for(i=cHalf-1; i>=0; i--) rgDadc[(i*2)+1] = rgSave[i];

    return(true);
}

STATE OSCCalibrate(HINSTR hOSC, HINSTR hDCVolt) 
{
    OSC *   pOSC    = (OSC *) hOSC;
    int32_t t1;
    uint32_t volatile __attribute__((unused)) flushADC;

    if(hOSC == NULL)
    {
        return(STATEError);
    }

    if(!(pOSC->comhdr.activeFunc == OSCFnCal || pOSC->comhdr.activeFunc == SMFnNone))
    {
        return(Waiting);   
    }
    
    switch(pOSC->comhdr.state)
    {
        case Idle:
            pOSC->comhdr.activeFunc     = OSCFnCal;
            pOSC->curGain               = 0;                // put at unity gain
            pOSC->pOCoffset->OCxRS      = PWMIDEALCENTER;   // center as best as possible
            OSCSetGain(hOSC, pOSC->curGain+1);
            pOSC->comhdr.state          = OSCPlusDC;

            // this is a percaution just to make sure the ADC DATA values
            // have been read. If not, ADCDSTAT1 (completion status flags) may not all be reset
            // remember the DMA controller is just turned off at the end of a trace, so it may
            // processed the last ADC completion event transfer andthe ADCDSTAT may still be set
            // reading the DATA will reset the flags
            flushADC = ADCDATA0;
            flushADC = ADCDATA1;
            flushADC = ADCDATA2;
            flushADC = ADCDATA3;

            // fall thru
        
        case OSCPlusDC:
            if(DCSetVoltage(hDCVolt, pOSC->rgGCal[pOSC->curGain].dVin) == Idle)
            {
                pOSC->comhdr.state          = OSCPlusReadDC;
            }
            break;

        case OSCPlusReadDC:       
            if(FBAWGorDCuV(((DCVOLT *) hDCVolt)->channelFB, &pOSC->t1) == Idle)
            {
                pOSC->comhdr.state          = OSCPlusReadOSC;
            }
            break;

        case OSCPlusReadOSC:
            if(AverageADC(pOSC->channelInA, 6, ((int32_t *) &(pOSC->rgGCal[pOSC->curGain].A))) == Idle)
            {
                pOSC->comhdr.state          = OSCMinusDC;
            }
            break;

        case OSCMinusDC:
            if(DCSetVoltage(hDCVolt, -pOSC->rgGCal[pOSC->curGain].dVin) == Idle)
            {
                pOSC->comhdr.state          = OSCMinusReadDC;
            }
            break;

        case OSCMinusReadDC:       
            if(FBAWGorDCuV(((DCVOLT *) hDCVolt)->channelFB, &t1) == Idle)
            {
                pOSC->t1          -= t1;        
                pOSC->comhdr.state          = OSCMinusReadOSC;
            }
            break;

        case OSCMinusReadOSC:
            if(AverageADC(pOSC->channelInA, 6, &t1) == Idle)
            {
                t1 =  pOSC->rgGCal[pOSC->curGain].A - t1;

                // this is the check to see if the DCout is hooked to the OSC in.
                // we only have to check this for one of the gain factors.
                if(pOSC->curGain == 0)
                {
                    int32_t uVDelta         = (int32_t) (((t1 * 1000000ll) + 682) / 1365); 
                    int32_t uVHigh          = ((pOSC->t1 * 11) + 5) / 10;      
                    int32_t uVLow           = ((pOSC->t1 * 9) + 5) / 10;

                    if(uVDelta < uVLow || uVHigh < uVDelta)
                    {
                        pOSC->comhdr.activeFunc = SMFnNone;
                        pOSC->comhdr.state      = Idle;               
                        return(OSCDcNotHookedToOSC);  
                    }
                }

                // calculate the coefficient A, delta uVin / delta Dadc
                *((int32_t *) &(pOSC->rgGCal[pOSC->curGain].A)) = pOSC->t1 / t1;

                pOSC->t1          = 0;             
                pOSC->comhdr.state          = OSCZeroDC;
            }
            break;


        // Now we need to get the PWM coefficient, hold the input at zero
        // and vary the PWM and read the result.
        // we could just make this a requirement
        case OSCZeroDC:
            if(DCSetVoltage(hDCVolt, 0) == Idle)
            {
                // PWM is inverted, so setting it low will cause the OSC input to read high
                pOSC->pOCoffset->OCxRS  = PWMIDEALCENTER - pOSC->rgGCal[pOSC->curGain].dPWM;      
                pOSC->comhdr.tStart     = SYSGetMilliSecond();
                pOSC->comhdr.state      = OSCWaitPWMHigh;
            }
            break;
            
        case OSCWaitPWMHigh:
            if((SYSGetMilliSecond() - pOSC->comhdr.tStart) >= PWM_SETTLING_TIME)
            {
                pOSC->comhdr.state      = OSCReadPWMHigh;
            }
            break;
            
        case OSCReadPWMHigh:
            if(AverageADC(pOSC->channelInA, 6, &pOSC->t1) == Idle)
            {
                pOSC->pOCoffset->OCxRS  = PWMIDEALCENTER + pOSC->rgGCal[pOSC->curGain].dPWM;
                pOSC->comhdr.tStart     = SYSGetMilliSecond();
                pOSC->comhdr.state      = OSCWaitPWMLow;
            }
            break;
            
        case OSCWaitPWMLow:
            if((SYSGetMilliSecond() - pOSC->comhdr.tStart) >= PWM_SETTLING_TIME)
            {
                pOSC->comhdr.state      = OSCReadPWMLow;
            }
            break;
            
        case OSCReadPWMLow:
            if(AverageADC(pOSC->channelInA, 6, &t1) == Idle)
            {
                *((int32_t *) &(pOSC->rgGCal[pOSC->curGain].B)) =  (int32_t) ((((int64_t) (pOSC->t1 - t1) * (pOSC->rgGCal[pOSC->curGain].A) + pOSC->rgGCal[pOSC->curGain].dPWM)) / (2* pOSC->rgGCal[pOSC->curGain].dPWM));
                
                // approx center the offset
                pOSC->pOCoffset->OCxRS  = PWMIDEALCENTER;

                pOSC->comhdr.tStart     = SYSGetMilliSecond();
                pOSC->comhdr.state      = OSCZeroPWM;        
            }
            break;
            
        case OSCZeroPWM:
            if((SYSGetMilliSecond() - pOSC->comhdr.tStart) >= PWM_SETTLING_TIME)
            {
                pOSC->comhdr.state      = OSCReadZeroDC;
            }
            break;
            
        case OSCReadZeroDC:       
            if(FBAWGorDCuV(((DCVOLT *) hDCVolt)->channelFB, &pOSC->t1) == Idle)
            {
                pOSC->comhdr.state          = OSCReadZeroOffset;
            }
            break;

        case OSCReadZeroOffset:
            if(AverageADC(pOSC->channelInA, 6, &t1) == Idle)
            {
                *((int32_t *) &(pOSC->rgGCal[pOSC->curGain].C)) =  pOSC->rgGCal[pOSC->curGain].A * t1 + pOSC->rgGCal[pOSC->curGain].B * PWMIDEALCENTER - pOSC->t1;
                pOSC->comhdr.state          = OSCNextGain;
            }
            break;
            
        case OSCNextGain:
            {            

                // go do the next gain setting.
                if(pOSC->curGain < 3)
                {
                    pOSC->curGain++;
                    OSCSetGain(hOSC, pOSC->curGain+1);
                    pOSC->comhdr.state          = OSCPlusDC;
                }

                // else we are done and DC is near zero, and offset is near zero
                else
                {
                    pOSC->comhdr.idhdr.cfg       = CFGCAL;
                    pOSC->comhdr.state          = Idle;
                    pOSC->comhdr.activeFunc     = SMFnNone;
                }
            }
            break;
                       
        default:
            pOSC->comhdr.state          = Idle;
            pOSC->comhdr.activeFunc     = SMFnNone;
            return(STATEError);
    }
    
    return(pOSC->comhdr.state);  
}

// gains are 1-4 to match the circuit equations Gx; x = 1-4.
bool OSCIsCurrentGain(HINSTR hOSC, uint32_t iGain)
{
    OSC *       pOSC        = (OSC *) hOSC;
    uint32_t    iCurGain    = 0;
    
    if( (*(pOSC->pLATIN1) & pOSC->maskIN1) != 0 ) iCurGain += 1;
    if( (*(pOSC->pLATIN2) & pOSC->maskIN2) != 0 ) iCurGain += 2;
    
    // if already set, do nothing
    return(iGain == (iCurGain+1));
 }

bool OSCSetGain(HINSTR hOSC, uint32_t iGain) 
{
    OSC *       pOSC        = (OSC *) hOSC;

    // if already set, do nothing
    if(OSCIsCurrentGain(hOSC, iGain))
    {
        return(true);
    }
    
    switch(iGain)
    {
        case 1:
            *(pOSC->pLATIN1) &= ~(pOSC->maskIN1);
            *(pOSC->pLATIN2) &= ~(pOSC->maskIN2);
            pOSC->curGain = 0;
            break;
        
        case 2:
            *(pOSC->pLATIN1) |= (pOSC->maskIN1);
            *(pOSC->pLATIN2) &= ~(pOSC->maskIN2);
            pOSC->curGain = 1;
            break;
        
        case 3:
            *(pOSC->pLATIN1) &= ~(pOSC->maskIN1);
            *(pOSC->pLATIN2) |= (pOSC->maskIN2);
            pOSC->curGain = 2;
            break;
        
        case 4:
            *(pOSC->pLATIN1) |= (pOSC->maskIN1);
            *(pOSC->pLATIN2) |= (pOSC->maskIN2);
            pOSC->curGain = 3;
            break;
           
        default:
            return(false);
    }
    
    return(true);
}

STATE OSCSetOffset(HINSTR hOSC, int32_t mVOffset)
{
    OSC * pOSC = (OSC *) hOSC;
    
    if(hOSC == NULL)
    {
        return(STATEError);
    }

    if(!(pOSC->comhdr.activeFunc == OSCFnSetOff || pOSC->comhdr.activeFunc == SMFnNone))
    {
        return(Waiting);   
    }
    
    switch(pOSC->comhdr.state)
    {
        uint32_t pwm = 0;
        
        case Idle:
                      
            pwm = OSCPWM(pOSC, pOSC->curGain, mVOffset);
            
            if( pwm <= PWMLOWLIMIT || PWMHIGHLIMIT <= pwm)
            {
                return(OSCPWMOutOfRange);
            }            
            
            // already there, do nothing
            if(pOSC->pOCoffset->OCxRS == pwm)
            {
                pOSC->comhdr.state      = Idle;
                pOSC->comhdr.activeFunc = SMFnNone;
            }
            else
            {
                // set the PWM value
                pOSC->pOCoffset->OCxRS  = pwm;
                pOSC->comhdr.tStart     = SYSGetMilliSecond();
            
                pOSC->comhdr.activeFunc = OSCFnSetOff;
                pOSC->comhdr.state      = Done;   
            }
            break;
            
        case Done:
            if((SYSGetMilliSecond() - pOSC->comhdr.tStart) >= PWM_SETTLING_TIME)
            {
                pOSC->comhdr.state      = Idle;
                pOSC->comhdr.activeFunc = SMFnNone;
            }
            break;
            
        default:
            pOSC->comhdr.state      = Idle;
            pOSC->comhdr.activeFunc = SMFnNone;
            return(STATEError); 
    }    
    
    return(pOSC->comhdr.state);
}

STATE OSCSetGainAndOffset(HINSTR hOSC, uint32_t iGain, int32_t mVOffset)
{
  
    if(OSCSetGain(hOSC, iGain))
    {
        return(OSCSetOffset(hOSC, mVOffset));
    }
    
    return(OSCGainOutOfRange);
}

bool OSCVinFromDadcArray(HINSTR hOSC, int16_t rgDadc[], uint32_t cDadc)
{
    OSC * pOSC = (OSC *) hOSC;
    int32_t BandC = OSCBandC(pOSC, pOSC->curGain, pOSC->pOCoffset->OCxRS);
    uint32_t i = 0;
    
    for(i=0; i<cDadc; i++)
    {
        rgDadc[i] = OSCVinFromDadcBandC(pOSC, pOSC->curGain, rgDadc[i], BandC);
    }
    
    return(true);
}

STATE OSCRun(HINSTR hOSC, IOSC * piosc)
{
    uint32_t volatile __attribute__((unused)) flushADC;
    OSC * pOSC = (OSC *) hOSC;
    STATE retState = Idle;
    STATE myState = Idle;
    uint32_t tCur = 0;
    
    if(hOSC == NULL)
    {
        return(STATEError);
    }

    if (pOSC->comhdr.activeFunc == OSCFnRun || pOSC->comhdr.activeFunc == SMFnNone) 
    {
        myState = pOSC->comhdr.state;
    }
    else if(pOSC->comhdr.cNest == 0 || pOSC->comhdr.activeFunc != OSCFnSetOff)
    {
        return (Waiting);
    }
    else
    {
        myState = OSCWaitOffset;
    }
    
    switch(myState)
    {
        case Idle:

            if(!OSCSetGain(hOSC, piosc->gain))
            {
                return(OSCGainOutOfRange);
            }
    
            // this will return immediately if the offset PWM is already set up
            if((retState = OSCSetOffset(hOSC, piosc->mvOffset)) != Idle)
            {
                if(IsStateAnError(retState))
                {
                    return(retState);  
                }

                // wait for the offset to stabalize
                pOSC->comhdr.cNest++;
                return(OSCWaitOffset);
            }

            // otherwise this was all set up
            // just fall right on thru and start the setup of the Run
            // fall thru

        case OSCSetDMA:
            pOSC->comhdr.activeFunc = OSCFnRun;     // need to do this for the fall thru case
            pOSC->comhdr.state      = OSCBeginRun;
            
            // WARNING: Interleaving only works if the timer prescalar is set to 1:1 with the PB clock.
            // The timer event will occur 1 PB and 2 sysclocks after the PR match. With a PBCLK3 at 2:1, and a timer prescalar of 1:1, that
            // works that the interrupt event occurs on timer value 1 (2 ticks after PR)
            // WARNING: the ADC is triggered off of the rising edge of the OC, not the interrupt event, this means the ADC will trigger on a match to R
            // WARNING: however, the DMA will trigger off of the OC interrupt event, which is a match to RS
            // As documented the interrupt for the OC should occur one PB after the R match for the ADC and 1 PB after the RS match for the DMA
            // however there must be extra logic for the ADC as the ADC fires 3 PB after the R match, determined experimentally.
            // PR = INTERLEAVEPR(X)     ((X)-1)                 
            // OCxR = INTERLEAVEOC(X)   (((X)/2)-2)              
            // If the trigger clock must be slowed way down to where we need to put a pre-scaler on the timer greater than 1:1, we should 
            // then move to using only 1 ADC and the timer as the trigger, dropping the second ADC and OC completely. We can not interleave
            // and have the timer prescalar anything other than 1:1, we must have the PB clock == TMR tick rate.
            // but we can interleave up to PR == 65535 (period of 65536) which is 65536/32 (32 is the fastest we can run the ADCs), which is 2048 times
            // slower than the fastest we can run the ADC. PB = 100000000, PR == 32 Max ADC == 3125000, 2 ADC interleaved is 6250000. But we can interleave
            // down to a rate of 6250000 / 2048 ~ 3051 well below the 3125000 of using just 1 ADC.
            // We can interleave down to a PR == 64000 == 2000 * 32, or a sample rate of 6250000/2000 == 3125, then switch to a single ADC with a PR == 32000 and then
            // increase the single TMR and ADC for slower rates. The slowest we can go with a TMR of 1:1 with a single ADC is ~ 1525 Sps.
            // however we can run the TMR prescalar down to 1:256 or 100000000/ 256 / 65536 ~ 6 Sps. With 32,768 in our buffer that is 32,768/6 ~ 5461s or about 1.5 hours of samples
            // below 6 Sps we have enough CPU bandwidth to sample manually with the Core Timer.
            // Our full potential sampling range is 6 -> 6,250,000 Sps. 
                     
            // make sure the timer is not running
            pOSC->pTMRtrg1->TxCON.ON    = 0;
            pOSC->pOCtrg2->OCxCON.ON    = 0;
            pOSC->pDMAch1->DCHxCON.CHEN = 0;
            pOSC->pDMAch2->DCHxCON.CHEN = 0;

            // just to clear any ADC completion interrupts
            flushADC = *((uint32_t *) (KVA_2_KSEG1(pOSC->pDMAch1->DCHxSSA)));
            flushADC = *((uint32_t *) (KVA_2_KSEG1(pOSC->pDMAch2->DCHxSSA)));
            
            // clear all ints
            pOSC->pDMAch1->DCHxINTClr   = 0xFFFFFFFF;                           // clear all interrupts   
            pOSC->pDMAch2->DCHxINTClr   = 0xFFFFFFFF;                           // clear all interrupts      

            // Set the timer up for appropriate DMA1 triggering          
            pOSC->pTMRtrg1->TxCON.TCKPS = piosc->bidx.tmrPreScalar;        // prescalar for the timer
            pOSC->pTMRtrg1->PRx         = INTERLEAVEPR(piosc->bidx.tmrPeriod);  // Default match on 32 counts  
            
            // If we set a TMR = 0, the timer will first fire the interrupt only after it fully sweep the first period, hits PRx
            // and then rolls to zero.
            // The OC module always looks for R first, and that will fire the 2nd ADC before the timer if TMR = 0.
            // Because we want the OC ADC to fire, and then the DMA to transfer in the same cycle, ideally on top of each other
            // just like the Timer, we would want to place RS on top of R. However, this will cause the OC module to only fire the
            // OC event on every other period. To do something close, we put RS shortly after R, not too close to cause interrupts to toggle
            // but close enough to get the transfer in the same timer cycle, so put RS at R+4. BUT then to get the timer to tirgger the first TMR ADC first
            // we must move the initial position of the timer so it fires (rolls) before the OC ADC fires, so put the TMR initial count at PRx - 4.           
            pOSC->pTMRtrg1->TMRx        = pOSC->pTMRtrg1->PRx - 4;  // ensure the TMR triggers the first interrupt event

            // all DMA pointers are set to there pre-defined buffers
            // usually the size is half the buffer as it is interleaved, however
            // if only 1 ADC/DMA is used, we will have to twice the sizeof the buffer DSIZ == 0 means 65536
            // If we are interleaving, use the half buffer size which the second channel will always have.
            pOSC->pDMAch1->DCHxDSIZ     = pOSC->pDMAch2->DCHxDSIZ;                          // half the size of the buffer 

            // explicitly clear the source and destination pointers
            // toggling the channel enable does not do this 
            // but since these are read only registers, I must set the
            // size register to cause the source/dest pointers to be cleared
            pOSC->pDMAch2->DCHxDSIZ     = pOSC->pDMAch1->DCHxDSIZ;

            // if interleaving we need to set up the 2nd DMA and OCs.
            // our prescalar will be 1:1 (0) if we do interleaving
            if(piosc->bidx.fInterleave)
            {
                // set up the OC for the second ADC
                pOSC->pOCtrg2->OCxR         = INTERLEAVEOC(piosc->bidx.tmrPeriod);     // event is triggered on R (contrary to documentation)

                // for interleaving and for an ADC running at 3.125MHz, here is a pictorial view of who fires first (TMR = 0)
                // we will run with TMR set at PRx-4 to fire first, and RS = R+4 to put the DMA transfer close just after
                // the start of the next ADC conversion.
                // Remember the timer both triggers the next ADC conversion AND triggers the transfer of the last ADC data via the DMA
                // at the same time.
                // WARNING: there is some belief that the ADC completion event occurs one PB to early and would case the DMA
                // to transfer the last data if used instead of TMR/OC events.
                // First Int |---------- TMR----------|--Keep Out--|---------OC---------||--Keep Out--|---------OC---------|
                //           |                        | Freak Out  |    ADC convert     || Freak Out  |     ADC convert    |
                // __________|________________________|____OCxR____|___________PRx______||____OCxR____|___________PRx______|
                // OCxRS     0                       13     14     15           31    12  13   14     15           31       

                // as noted above, put RS at R+4.
                pOSC->pOCtrg2->OCxRS        = pOSC->pOCtrg2->OCxR + 4;    // Force us in the 300nsec zone

                // CORRECTION: we are now using ADC completion events to trigger the DMA transfer and the value of RS is not
                // used to trigger the DMA for ADC->MEM transfer. Look in instrument.c for exactly how the DMA is triggered

                // need to turn on the OC and second DMA channel
                pOSC->pOCtrg2->OCxCON.ON    = 1;
                pOSC->pDMAch2->DCHxCON.CHEN = 1;
            }
            else
            {
                // we want to set up for a full 64K buffer 
                // we need to truncate the result because it will overflow to 0x10000
                // putting zero in the DSIZ will yeild a 64K size

                // BUG BUG, the DMA will freak out if you put a size of 0 in, while documentation says
                // this means 64KB transfer, the DMA controller will completely trash the trigger bus
                // for this reason we set out buffer size to 32K - 2bytes so that when we double it here, we are
                // 4 bytes short of a full 64K
                pOSC->pDMAch1->DCHxDSIZ =  (uint16_t) (2ul * ((uint32_t) pOSC->pDMAch2->DCHxDSIZ));              
            }
            
            // The first DMA channel is always used, enable it
            pOSC->pDMAch1->DCHxCON.CHEN = 1;

            // TODO, set to something reasonable
            // Wait until all of the DMAs and OCs are ON and ready
            tCur = ReadCoreTimer();
            while(ReadCoreTimer() - tCur < CORE_TMR_TICKS_PER_USEC);

            // Turn on the master timer 
            // this starts everything
            pOSC->pTMRtrg1->TxCON.ON   = 1;            
            break;
 
        case OSCWaitOffset:
            // wait until the offset is set
            if((retState = OSCSetOffset(hOSC, piosc->mvOffset)) == Idle)
            {
                pOSC->comhdr.cNest--;
                pOSC->comhdr.activeFunc = OSCFnRun;
                pOSC->comhdr.state      = OSCSetDMA;
            }
            break;

        case OSCBeginRun:
            {
                int32_t iDMA = (int32_t) (piosc->bidx.fInterleave ? pOSC->pDMAch1->DCHxDPTR : pOSC->pDMAch1->DCHxDPTR / 2);

            // we want to fill the history buffer with at least half the buffer so we can look 
            // back into time. Wait until the whole buffer (BLOCK) transfer is complete
                if(iDMA >= piosc->bidx.cBeforeTrig || pOSC->pDMAch1->DCHxINT.CHBCIF)           // full buffer
                {
                    // now go arm the 
                    pOSC->comhdr.state      = Armed;
                }
            }
            break;
            
        case Armed:
            if(!pOSC->pTMRtrg1->TxCON.ON)
            {
                if(piosc->bidx.fInterleave)
                {
                    // Reorder the buffer for interleaving
                    OSCReorder(piosc->pBuff, AINDMASIZE);
                }

                // say we were triggered
                pOSC->comhdr.state      = Triggered;
            }
            break;
            
        case Triggered:
            OSCVinFromDadcArray(hOSC, piosc->pBuff, AINDMASIZE);
            pOSC->comhdr.state      = Idle;
            pOSC->comhdr.activeFunc = SMFnNone;
            break;
            
        default:
            pOSC->comhdr.state      = Idle;
            pOSC->comhdr.activeFunc = SMFnNone;
            return(STATEError); 
    }
    
    return(pOSC->comhdr.state);
}

STATE OSCReset(HINSTR hOSC)
{
    OSC * pOSC = (OSC *) hOSC;

    pOSC->comhdr.activeFunc     = SMFnNone;
    pOSC->comhdr.state          = Idle;
    pOSC->comhdr.cNest          = 0;     
    pOSC->comhdr.tStart         = 0; 

    return(Idle);
}
/************************************************************************/
/*  Revision History:                                                   */
/*                                                                      */
/*    7/132015 (KeithV): Created                                        */
/************************************************************************/

//**************************************************************************
//**************************************************************************
//******************* ADC Config       *********************************
//**************************************************************************
//**************************************************************************

// We run the ADC at 3.125MHz, We run the TAD at 50MHz, sysclock / 4. but....
// Unofficially, it works up to 70MHz and if below 70C, 100MHz. But MCHP does not guarantee those values; they are just our internal observations. 

// ADC 0/1 Timer 3 and OC5
#define T3PRESCALER 0                                   // this is 2^^T3PRESCALER (0 - 7, however 7 == 256 not 128)
#define T3MATCH     32                                  // How many ticks until an interrupt (1 - 65536))
#define T3FREQ      (PB3FREQ / ((T3PRESCALER < 7) ? (1 << T3PRESCALER) : 256) / T3MATCH)
#if (SHCONVFREQDC < T3FREQ)
    #error Sample and Hold freq must be faster than T3 sampling freq 
#endif

// ADC 2/3 Timer 5 and OC1
#define T5PRESCALER 0                                   // this is 2^^T3PRESCALER (0 - 7, however 7 == 256 not 128)
#define T5MATCH     32                                  // How many ticks until an interrupt (1 - 65536))
#define T5FREQ      (PB3FREQ / ((T5PRESCALER < 7) ? (1 << T5PRESCALER) : 256) / T5MATCH)
#if (SHCONVFREQDC < T5FREQ)
    #error Sample and Hold freq must be faster than T5 sampling freq 
#endif

// DMA Abort Timers
#define T9PRESCALER 5                                   // this is 2^^T3PRESCALER (0 - 7, however 7 == 256 not 128)


