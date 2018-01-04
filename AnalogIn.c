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

extern void TimeOutTmr9(void);

// static uint32_t tStartL;

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
    OSC * pOSC = piosc->posc;
    STATE retState = Idle;
    STATE myState = Idle;
    uint32_t tCur = 0;
    
    if(piosc == NULL)
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

            if(!OSCSetGain((HINSTR) pOSC, piosc->gain))
            {
                return(OSCGainOutOfRange);
            }
    
            // this will return immediately if the offset PWM is already set up
            if((retState = OSCSetOffset((HINSTR) pOSC, piosc->mvOffset)) != Idle)
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

            // if the 2nd ADC is set up for logging (OC5), turn it off
            // if(ADCTRG1bits.TRGSRC2 == 0b01010) ADCTRG1bits.TRGSRC2 = 0;

            // this is only to catch the trigger setup differences from the data logger
            // we can only run the data logger or the OSC, but not both, so we can just 
            // configure both channels when we are running the OSC
            if(*pjcmd.ioscCh1.pTrgSrcADC1 != pjcmd.ioscCh1.trgSrcADC1) *pjcmd.ioscCh1.pTrgSrcADC1 = pjcmd.ioscCh1.trgSrcADC1;
            if(*pjcmd.ioscCh1.pTrgSrcADC2 != pjcmd.ioscCh1.trgSrcADC2) *pjcmd.ioscCh1.pTrgSrcADC2 = pjcmd.ioscCh1.trgSrcADC2;
            if(*pjcmd.ioscCh2.pTrgSrcADC1 != pjcmd.ioscCh2.trgSrcADC1) *pjcmd.ioscCh2.pTrgSrcADC1 = pjcmd.ioscCh2.trgSrcADC1;
            if(*pjcmd.ioscCh2.pTrgSrcADC2 != pjcmd.ioscCh2.trgSrcADC2) *pjcmd.ioscCh2.pTrgSrcADC2 = pjcmd.ioscCh2.trgSrcADC2;

            //ADCTRG1bits.TRGSRC0 = 0b00110;      // Set trigger TMR3
            //ADCTRG1bits.TRGSRC1 = 0b01010;      // Set trigger OC5
            //ADCTRG1bits.TRGSRC2 = 0b00111;      // set trigger TMR5
            //ADCTRG1bits.TRGSRC3 = 0b01000;      // Set trigger OC1

            // set up the DMA pointers
            pOSC->pDMAch1->DCHxSSA              = KVA_2_PA(piosc->pADCDATA1);
            pOSC->pDMAch1->DCHxDSA              = KVA_2_PA(piosc->pBuff);
            pOSC->pDMAch1->DCHxDSIZ             = AINDMASIZE;
            pOSC->pDMAch1->DCHxCON.CHPRI        = 2;
            pOSC->pDMAch1->DCHxECON.SIRQEN      = 1;                        // trigger on vector event
            pOSC->pDMAch1->DCHxECON.CHSIRQ      = piosc->adcData1Vector;    // vector to trigger on
            pOSC->pDMAch1->DCHxCON.CHAEN        = 1;

            pOSC->pDMAch2->DCHxSSA              = KVA_2_PA(piosc->pADCDATA2);
            pOSC->pDMAch2->DCHxDSA              = pOSC->pDMAch1->DCHxDSA + AINDMASIZE;
            pOSC->pDMAch2->DCHxDSIZ             = AINDMASIZE; 
            pOSC->pDMAch2->DCHxCON.CHPRI        = 2; 
            pOSC->pDMAch2->DCHxECON.SIRQEN      = 1;                        // trigger on vector event
            pOSC->pDMAch2->DCHxECON.CHSIRQ      = piosc->adcData2Vector;    // vector to trigger on
            pOSC->pDMAch2->DCHxCON.CHAEN        = 1;
 
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
            pOSC->pTMRtrg1->TMRx        = pOSC->pTMRtrg1->PRx - TMROCPULSE;  // ensure the TMR triggers the first interrupt event

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
                pOSC->pOCtrg2->OCxRS        = pOSC->pOCtrg2->OCxR + TMROCPULSE;    // Force us in the 300nsec zone

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
            if((retState = OSCSetOffset((HINSTR) pOSC, piosc->mvOffset)) == Idle)
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
            OSCVinFromDadcArray((HINSTR) pOSC, piosc->pBuff, AINDMASIZE);
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

//**************************************************************************
//**************************************************************************
//*******************    Data Logger       *********************************
//**************************************************************************
//**************************************************************************

STATE ALOGRun(IALOG * pialog)
{
    uint32_t volatile __attribute__((unused)) flushADC;
    ALOG * pALOG = (ALOG *) rgInstr[pialog->id];
    STATE retState = Idle;
    
    ASSERT(pialog != NULL);
 
    if(!(pALOG->comhdr.activeFunc == ALogFnRun || pALOG->comhdr.activeFunc == SMFnNone))
    {
        return (Waiting);
    }
     
    switch(pALOG->comhdr.state)
    {
        case Idle:

            if(!OSCSetGain((HINSTR) pALOG->posc, pialog->gain))
            {
                pialog->stcd    = STCDError;
                return(OSCGainOutOfRange);
            }
    
            // this will return immediately if the offset PWM is already set up
            if((retState = OSCSetOffset((HINSTR) pALOG->posc, pialog->mvOffset)) != Idle)
            {
                if(IsStateAnError(retState))
                {
                    pialog->stcd    = STCDError;
                    return(retState);  
                }

                // wait for the offset to stabalize
                pialog->stcd        = STCDNormal;
                pALOG->comhdr.state = OSCWaitOffset;
            }

            // otherwise this was all set up
            // just fall right on thru and start the setup of the Run
            // fall thru

        case OSCSetDMA:

            // Turn off the sample timer
            pALOG->pTMRdmaSampl->TxCON.ON = 0;

            // turn the DMA off
            pALOG->posc->pDMAch1->DCHxCON.CHEN = 0;
            pALOG->posc->pDMAch2->DCHxCON.CHEN = 0;
            IEC4CLR                            = pALOG->dmaRollIEFMask;     // clear the roll interrupt
            IFS4CLR                            = pALOG->dmaRollIEFMask;    // clear the flag    

            // the way this works is the we turn on both TM3 to trigger AN0 and OC5 to trigger AN2
            // we run those at top speed 3.125MHz
            // The we enable the ADC and Filter as needed
            // on completion of the filter, DMA1 transfers to our static buffer With priority 2
            // then on the TMR we trigger DMA2 our our sample rate, we run DMA2 at priority 1 (lower that DMA so it will stall DMA2 on collision).
            // DMA2 transfer a stable value into our result buffer
            // NOTE: only 1 filter can run at a time, so we must sync ADC0 and ADC2 so they do not convert at the same time, but one after the other.

            if(!pALOG->pTMRadc0->TxCON.ON)
            {
                // make sure the OC is off
                pALOG->pOCadc2->OCxCON.ON       = 0;

                // Make sure we have the Trigger ISR placed
                SetVector(_TIMER_9_VECTOR, TimeOutTmr9);

                // set up the timer for max Logging freq
                pALOG->pTMRadc0->TxCON.TCKPS    = 0;                                    // zero prescalar -> 1:1
                pALOG->pTMRadc0->PRx            = INTERLEAVEPR(LOGPRx);                 // 50KS/s => 2000                
                pALOG->pTMRadc0->TMRx           = pALOG->pTMRadc0->PRx - TMROCPULSE;    // ensure the TMR triggers the first interrupt event

                // The second ADC will run at the same freq, but half way through the cycle
                // so that both ADCs don't convert at the same time and use 2 filters at once (errata restriction)
                pALOG->pOCadc2->OCxR            = INTERLEAVEOC(LOGPRx);                 // remember ADC trigger on R, not RS
                pALOG->pOCadc2->OCxRS           = pALOG->pOCadc2->OCxR + TMROCPULSE;    // just long enough to cause a pulse

                // set up the ADC triggers
                // we use the OSC TMR and OC to run the 2 serialized ADC, we do NOT interleave so the interleaving ADC are not triggered.
                if(*pjcmd.ioscCh1.pTrgSrcADC1 != pjcmd.ioscCh1.trgSrcADC1) *pjcmd.ioscCh1.pTrgSrcADC1 = pjcmd.ioscCh1.trgSrcADC1;
                if(*pjcmd.ioscCh1.pTrgSrcADC2 != 0) *pjcmd.ioscCh1.pTrgSrcADC2 = 0;
                if(*pjcmd.ioscCh2.pTrgSrcADC1 != pjcmd.ioscCh1.trgSrcADC2) *pjcmd.ioscCh2.pTrgSrcADC1 = pjcmd.ioscCh1.trgSrcADC2;
                if(*pjcmd.ioscCh2.pTrgSrcADC2 != 0) *pjcmd.ioscCh2.pTrgSrcADC2 = 0;

                // what the OSC configures
                //ADCTRG1bits.TRGSRC0 = 0b00110;      // Set trigger TMR3
                //ADCTRG1bits.TRGSRC1 = 0b01010;      // Set trigger OC5
                //ADCTRG1bits.TRGSRC2 = 0b00111;      // set trigger TMR5
                //ADCTRG1bits.TRGSRC3 = 0b01000;      // Set trigger OC1
                
                // what the data logger configures
                //ADCTRG1bits.TRGSRC0 = 0b00110;      // Set trigger TMR3
                //ADCTRG1bits.TRGSRC1 = 0;            // Set trigger NONE
                //ADCTRG1bits.TRGSRC2 = 0b01010;      // Set trigger OC5
                //ADCTRG1bits.TRGSRC3 = 0;            // Set trigger NONE

                // turn on ADC conversions, both channels
                // remeber, the ADCs are always ON, so triggering them will make them run
                pALOG->pOCadc2->OCxCON.ON   = 1;
                pALOG->pTMRadc0->TxCON.ON   = 1;
            }
            
            // set up the timer to trigger the 2nd DMA, but DON'T Turn it on yet
            pALOG->pTMRdmaSampl->TxCON.TCKPS    =  pialog->bidx.tmrPreScalar;
            pALOG->pTMRdmaSampl->PRx            = PeriodToPRx(pialog->bidx.tmrPeriod);
            pALOG->pTMRdmaSampl->TMRx           = PeriodToPRx(pialog->bidx.tmrPeriod) - TMROCPULSE;         // make the first sample fast.

            // The first DMA will trigger when the ADC filter conversion is done
            pALOG->posc->pDMAch1->DCHxSSA           = KVA_2_PA(pALOG->pFilter);         //  ADCFLTRxbits.FLTRDATA
            pALOG->posc->pDMAch1->DCHxDSA           = KVA_2_PA(&pALOG->stableADCValue);
            pALOG->posc->pDMAch1->DCHxDSIZ          = 2;
            pALOG->posc->pDMAch1->DCHxCON.CHPRI     = 2;
            pALOG->posc->pDMAch1->DCHxECON.CHSIRQ   = pALOG->fltVector;
            pALOG->posc->pDMAch1->DCHxCON.CHAEN     = 1;                                // auto restart

            // the 2nd DMA will transfer from our temp stable location to the target OSC buffer.
            pALOG->posc->pDMAch2->DCHxSSA           = KVA_2_PA(&pALOG->stableADCValue);
            pALOG->posc->pDMAch2->DCHxDSA           = KVA_2_PA(pialog->pBuff);    
            pALOG->posc->pDMAch2->DCHxDSIZ          = 2*LOGDMASIZE; 
            pALOG->posc->pDMAch2->DCHxCON.CHPRI     = 1; 
            pALOG->posc->pDMAch2->DCHxECON.CHSIRQ   = pALOG->tmrVector;
            pALOG->posc->pDMAch2->DCHxCON.CHAEN     = 1;   

            if(pialog->bidx.tmrCnt > 1)
            {
                pialog->curCnt                          = pialog->bidx.tmrCnt;
                pALOG->posc->pDMAch2->DCHxECON.SIRQEN   = 0;  // the ISR will trigger the DMA not the timer
                pALOG->pIFSisrSample->clr               = pALOG->isrSampleIEFMask;    // clear the flag    
                pALOG->pIECisrSample->set               = pALOG->isrSampleIEFMask;    // enable the roll interrupt
            }
            else
            {
                pALOG->posc->pDMAch2->DCHxECON.SIRQEN = 1; // let the timer trigger this directly
                pALOG->pIFSisrSample->clr               = pALOG->isrSampleIEFMask;    // clear the flag    
                pALOG->pIECisrSample->clr               = pALOG->isrSampleIEFMask;    // enable the roll interrupt
            }

            // clear all ints
            pALOG->posc->pDMAch1->DCHxINTClr        = 0xFFFFFFFF;   // clear all interrupts   
            pALOG->posc->pDMAch2->DCHxINTClr        = 0xFFFFFFFF;   // clear all interrupts      

            // interrupt enabling
            pALOG->posc->pDMAch2->DCHxINT.CHBCIE    = 1;            // say we want Channel Block Transfer Complete set

             // number of rolls the DMA block transfer had
            pialog->bidx.cTotalSamples  = 0;    // total number of samples taken
            pialog->bidx.iDMAEnd        = 0;    // Logging: end of valid data in DMA buffer
            pialog->bidx.iDMAStart      = 0;    // Logging: start of unsaved samples in DMA buffer
            pialog->bidx.cDMARoll       = 0;    // Logging, this is the roll count
            pialog->bidx.cSavedRoll     = 0;    // Logging, this is the saved roll count

            // just to clear any ADC completion interrupts
            flushADC = *pALOG->piosc->pADCDATA1;

            // just to clear any Filter completion interrupts
            flushADC = pALOG->pFilter->FLTRDATA;

            // enable channel 1 DMA, during warmup we want data to be transferred to the
            // holding memory location, but not transferred to the resultant DMA buffer location
            pALOG->posc->pDMAch1->DCHxCON.CHEN  = 1; 

            // enable the digital filter
            // we should now be starting to transfer to the stableADCValue location
            pALOG->pFilter->AFEN = 1;                    // Turn on the Digital filter

            // set up the warm up/delay start timer
            pALOG->pTMRdmaSampl->TxCON.ON = 1;          // turn on the sampler, the delay timer9 will turn this off
            if(pialog->bidx.psDelay > LOGPSPRIME) TOInstrumentAdd(pialog->bidx.psDelay, pialog->id);
            else TOInstrumentAdd(LOGPSPRIME, pialog->id);
            TOStart();
//            tStartL = ReadCoreTimer();

            pALOG->comhdr.state      = OSCBeginRun;
            break;

        case OSCWaitOffset:
            // wait until the offset is set
            if((retState = OSCSetOffset((HINSTR) pALOG->posc, pialog->mvOffset)) == Idle)
            {
                pALOG->comhdr.state = OSCSetDMA;
            }
            break;

        case OSCBeginRun:
            // CORE_TMR_TICKS_PER_SEC * LOGPRx / TMRPBCLK == How many core timer ticks we have; will be the same as LOGPRx
            // lets run 4 conversions before starting (about 80uSec)
            if(!pALOG->pTMRdmaSampl->TxCON.ON)
            {
//                tStartL = (ReadCoreTimer() - tStartL) / CORE_TMR_TICKS_PER_USEC;

                // Now turn on DMA2 to start transferring to the result DMA buffer
                IFS4CLR                             = pALOG->dmaRollIEFMask;    // clear the flag    
                IEC4SET                             = pALOG->dmaRollIEFMask;    // enable the roll interrupt
                pALOG->posc->pDMAch2->DCHxCON.CHEN  = 1;                        // start DMA channel to, to transfer

                // turn on the timer to run DMA2 to collect samples
                pALOG->pTMRdmaSampl->TxCON.ON = 1;

                // add in how long to run, -1 means never stop
                if(pialog->maxSamples > 0)
                {
                    // we have LOGOVERSIZE == 64 oversize; we need to add 1 to the max samples because of specfic timing
                    // we may be right before the stop occurs before the last sample is taken, so add 1. But at 50KHz, we will extra time
                    // typically this will be 4 extra samples; we have up to 64 extra slop samples.
                    TOInstrumentAdd(GetPicoSec(pialog->maxSamples+1, pialog->bidx.xsps, 1000000), pialog->id);
                    TOStart();
//                    tStartL = ReadCoreTimer();
                }

                if(pialog->id == ALOG2_ID)
                {
                    pALOG->comhdr.state      = Armed;
                }
                pALOG->comhdr.state      = Armed;
            }
            break;

        case Armed:
            if(!pALOG->pTMRdmaSampl->TxCON.ON)
            {
//                tStartL = (ReadCoreTimer() - tStartL) / CORE_TMR_TICKS_PER_USEC;
                if(pialog->id == ALOG2_ID)
                {
                    pALOG->comhdr.state      = Armed;
                }

                // turn off the digital filter
                pALOG->pFilter->AFEN = 0; 

                // turn off the DMA channels
                pALOG->posc->pDMAch1->DCHxCON.CHEN  = 0;
                pALOG->posc->pDMAch2->DCHxCON.CHEN  = 0;

                // clear the roll interrupt
                IEC4CLR                             = pALOG->dmaRollIEFMask;    // enable the interrupt
                IFS4CLR                             = pALOG->dmaRollIEFMask;    // clear the flag    
                pALOG->pIFSisrSample->clr           = pALOG->isrSampleIEFMask;  // clear the flag    
                pALOG->pIECisrSample->clr           = pALOG->isrSampleIEFMask;  // enable the roll interrupt

                // set the DMA completion pointer
                pialog->bidx.iDMAEnd = pALOG->posc->pDMAch2->DCHxDPTR / sizeof(uint16_t);

                // go to triggered state
                pALOG->comhdr.state      = Triggered;
            }
            break;

        case Triggered:
            {
                // done
                pALOG->comhdr.state      = Idle;
                pALOG->comhdr.activeFunc = SMFnNone;  
            }
            break;

        default:
            ASSERT(NEVER_SHOULD_GET_HERE);
            break;
    }

    return(pALOG->comhdr.state);
}

STATE ALOGStop(IALOG * pialog)
{
    ALOG * pALOG = (ALOG *)     rgInstr[pialog->id];
   
    ASSERT(pialog != NULL);

    if(pALOG->comhdr.state == Armed && pALOG->pTMRdmaSampl->TxCON.ON)
    {
        // if it is in the timer list, clear it from the timer list
        TONullInstrument(pialog->id);

        // however we may have stated run forever and it won't be in the
        // the timer list, so make sure to turn it off so the running will stop
        // turn off the sampler
        pALOG->pTMRdmaSampl->TxCON.ON = 0;

        pialog->stcd          = STCDForce;

        return(Idle);
    }

    return(InstrumentNotArmed);
}


void __attribute__((nomips16, at_vector(_DMA4_VECTOR),interrupt(IPL4SRS))) AILog1RollISR(void)
{
    // clear the IF flag
    IFS4CLR     = _IFS4_DMA4IF_MASK;
    DCH4INTCLR  = _DCH4INT_CHBCIF_MASK;

    // say we rolled.
    pjcmd.iALog1.bidx.cDMARoll++; 
}

void __attribute__((nomips16, at_vector(_DMA6_VECTOR),interrupt(IPL4SRS))) AILog2RollISR(void)
{
    // clear the IF flag
    IFS4CLR = _IFS4_DMA6IF_MASK;
    DCH6INTCLR  = _DCH6INT_CHBCIF_MASK;

    // say we rolled.
    pjcmd.iALog2.bidx.cDMARoll++; 
}

void __attribute__((nomips16, at_vector(_TIMER_5_VECTOR),interrupt(IPL5SRS))) ISRTmr5SlowLog1DMATrigger(void)
{
    IFS0CLR = _IFS0_T5IF_MASK;

    pjcmd.iALog1.curCnt--;

    if(pjcmd.iALog1.curCnt == 0)
    {
        // fire the cell transfer
        DCH4ECONSET = _DCH4ECON_CFORCE_MASK;

        // restart the count
        pjcmd.iALog1.curCnt = pjcmd.iALog1.bidx.tmrCnt;
    }
}

void __attribute__((nomips16, at_vector(_TIMER_8_VECTOR),interrupt(IPL5SRS))) ISRTmr8SlowLog1DMATrigger(void)
{
    IFS1CLR = _IFS1_T8IF_MASK;

    pjcmd.iALog2.curCnt--;

    if(pjcmd.iALog2.curCnt == 0)
    {
        // fire the cell transfer
        DCH6ECONSET = _DCH6ECON_CFORCE_MASK;

        // restart the count
        pjcmd.iALog2.curCnt = pjcmd.iALog2.bidx.tmrCnt;
    }
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


