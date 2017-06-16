
/************************************************************************/
/*                                                                      */
/*    AWG.C                                                   */
/*                                                                      */
/*    Calibration file for the DC Outs                                  */
/*    It also contains the reference voltage identification             */
/*                                                                      */
/************************************************************************/
/*    Author:     Keith Vogel                                           */
/*    Copyright 2016, Digilent Inc.                                     */
/************************************************************************/
/************************************************************************/
/*  Revision History:                                                   */
/*                                                                      */
/*    3/3/2016 (KeithV): Created                                        */
/************************************************************************/
#include <OpenScope.h>

#if(SWDACSIZE > HWDACSIZE)
#error calibrated AWG/DAC must be less than or equal to the hardware DAC
#endif

STATE AWGCalibrate(HINSTR hAWG) {
    AWG * pAWG = (AWG *) hAWG;
    int32_t     uVHW;

    // make sure this is our function
    if (!(pAWG->comhdr.activeFunc == AWGFnCal || pAWG->comhdr.activeFunc == SMFnNone)) {
        return (Waiting);
    }

    switch (pAWG->comhdr.state) 
    {
        case Idle:
            pAWG->comhdr.activeFunc = AWGFnCal;
            pAWG->pOCoffset->OCxRS = 300;
            *(pAWG->pLATx) = DACDATA((HWDACSIZE - 1) / 2); // set the DAC value (511)
            pAWG->comhdr.tStart = SYSGetMilliSecond();
            pAWG->comhdr.state = AWGWaitPWMLow;
            break;

        case AWGWaitPWMLow:
            if (SYSGetMilliSecond() - pAWG->comhdr.tStart >= PWM_SETTLING_TIME) 
            {
                pAWG->comhdr.state = AWGReadPWMLow;
            }
            break;

        case AWGReadPWMLow:
            if(FBAWGorDCuV(pAWG->channelFB, &pAWG->t2) == Idle) 
            {
                pAWG->pOCoffset->OCxRS = 50;
                pAWG->comhdr.tStart = SYSGetMilliSecond();
                pAWG->comhdr.state = AWGWaitPWMHigh;
            }
            break;

       case AWGWaitPWMHigh:
            if (SYSGetMilliSecond() - pAWG->comhdr.tStart >= PWM_SETTLING_TIME) 
            {
                pAWG->comhdr.state = AWGReadPWMHigh;
            }
            break;

       case AWGReadPWMHigh:
            if(FBAWGorDCuV(pAWG->channelFB, &pAWG->t3) == Idle) 
            {
                *((int32_t *) &(pAWG->A)) = ((pAWG->t3 - pAWG->t2) + 125) / 250; // ~11995

                // might as well start centering now
                pAWG->pOCoffset->OCxRS = PWMIDEALCENTER;
                *(pAWG->pLATx) = DACDATA(0); // set the DAC value
                pAWG->comhdr.tStart = SYSGetMilliSecond();
                pAWG->comhdr.state = AWGWaitDACLow;
            }
            break;

        case AWGWaitDACLow:
            if (SYSGetMilliSecond() - pAWG->comhdr.tStart >= PWM_SETTLING_TIME) 
            {
               pAWG->comhdr.state = AWGReadDACLow;
            }
            break;

        case AWGReadDACLow:
            if(FBAWGorDCuV(pAWG->channelFB, &pAWG->t2) == Idle) 
            {
                *(pAWG->pLATx) = DACDATA(HWDACSIZE - 1); // set the DAC value
                pAWG->comhdr.tStart = SYSGetMilliSecond();
                pAWG->comhdr.state = AWGWaitDACHigh;
            }
            break;

        case AWGWaitDACHigh:
            if (SYSGetMilliSecond() - pAWG->comhdr.tStart >= AWG_SETTLING_TIME) {
                pAWG->comhdr.state = AWGReadDACHigh;
            }
            break;

        case AWGReadDACHigh:
            if(FBAWGorDCuV(pAWG->channelFB, &pAWG->t3) == Idle) 
            {
                int32_t uVPWMHigh = (pAWG->t3 - pAWG->t2 + 1) / 2;
                int32_t pwmCenter = 0;

                // if we need to move the offest down, that is, it is too high now
                // is > want
                if(pAWG->t3 > uVPWMHigh)
                {
                    // to move down, the PWM must be higher, the opamp is inverting
                    pwmCenter = PWMIDEALCENTER + ((pAWG->t3 - uVPWMHigh + pAWG->A/2) / pAWG->A);
                }

                // if we need to move the offset up
                else
                {
                    // we need to lower the PWM value
                    pwmCenter = PWMIDEALCENTER - ((uVPWMHigh - pAWG->t3 + pAWG->A/2) / pAWG->A);
                }

                pAWG->pOCoffset->OCxRS = pwmCenter; // set this to pulse duration

                // now that we found the center PWM
                // we can calculate B
                // AWGoffset = B - A(PWM) 
                // AWGoffset == 0 at PWMCenter
                // B = A(PWMCenter) ~ 2099125
                *((int32_t *) &(pAWG->B)) = pAWG->A * pwmCenter;

                pAWG->t1 = 0; // get ready to read the DAC
                *(pAWG->pLATx) = DACDATA(0);

                pAWG->comhdr.tStart = SYSGetMilliSecond();
                pAWG->comhdr.state = AWGWait;
            }
            break;

        case AWGWait:
            if (SYSGetMilliSecond() - pAWG->comhdr.tStart >= PWM_SETTLING_TIME) 
            {
                // we can jump directly to the read as
                // we waited the longer PWM settling time.

                pAWG->comhdr.state = AWGReadHW;
            }
            break;

        case AWGWaitHW:
            if (SYSGetMilliSecond() - pAWG->comhdr.tStart >= AWG_SETTLING_TIME) 
            {
                pAWG->comhdr.state = AWGReadHW;
            }

        case AWGReadHW:
            if(FBAWGorDCuV(pAWG->channelFB, &uVHW) == Idle) 
            {
                // read the channel data
                // must be no greater than sum 16 as we only have 16 bits, 12bit converter X 16 for 12+4=16 bits.
                pAWG->mVFB[(pAWG->t1)++] = (int16_t) (uVHW >= 0 ? ((uVHW + 500) / 1000) : -((-uVHW + 500) / 1000)); // DO NOT GO GREATER THAN 16 SUMMING

                if (pAWG->t1 < HWDACSIZE)
                {
                    // set up for the next read
                    *(pAWG->pLATx) = DACDATA(pAWG->t1);
                    pAWG->comhdr.tStart = SYSGetMilliSecond();
                    pAWG->comhdr.state = AWGWaitHW;
                } else {
                    // done, get out.
                    pAWG->t1 = 0;
                    pAWG->comhdr.state = AWGSort;
                }
            }
            break;

        case AWGSort:
            {
                int         i;
                uint32_t    cBubSortPass = 0;
                bool        fStillSorting = true;
                uint16_t    dacMap[HWDACSIZE];
                int16_t     dacValue[HWDACSIZE];

                // put things in local variable for faster / easier processing
                memcpy(dacValue, pAWG->mVFB, sizeof(dacValue));

                // init calibration array
                for (i = 0; i < HWDACSIZE; i++) {
                    dacMap[i] = i;
                }

                // bubble sort calibration array
                while (fStillSorting) {
                    fStillSorting = false;
                    for (i = 0; i < (HWDACSIZE - 1); i++) {
                        if (dacValue[i] > dacValue[i + 1]) {

                            uint16_t dacMapT    = dacMap[i];
                            int16_t  dacValueT  = dacValue[i];

                            dacValue[i]         = dacValue[i+1];
                            dacValue[i+1]       = dacValueT;

                            dacMap[i]           = dacMap[i+1];
                            dacMap[i+1]         = dacMapT;

                            fStillSorting = true;
                        }
                    }
                    cBubSortPass++;
                }

                // Save the map
                memcpy(pAWG->dacMap, dacMap,    sizeof (pAWG->dacMap));
                memcpy(pAWG->mVFB,  dacValue,   sizeof (pAWG->mVFB));

                pAWG->comhdr.state = AWGEncode;
            }
            break;

        // encode the map for direct PortH usage
        case AWGEncode:
            {
                int i;

                for (i = 0; i < HWDACSIZE; i++) {
                    pAWG->dacMap[i] = DACDATA(pAWG->dacMap[i]);
                }

                // we have calibrated
                pAWG->comhdr.idhdr.cfg       = CFGCAL;
            }

            // fall thru to default

        default:
            pAWG->comhdr.activeFunc = SMFnNone;
            pAWG->comhdr.state = Idle;
            break;
    }

    return (pAWG->comhdr.state);
}

STATE AWGSetOffsetVoltage(HINSTR hAWG, int32_t mvDCOffset) {
    AWG * pAWG = (AWG *) hAWG;

    // make sure this is our function
    if (!(pAWG->comhdr.activeFunc == AWGFnSetOffset || pAWG->comhdr.activeFunc == SMFnNone)) {
        return (Waiting);
    }

    switch (pAWG->comhdr.state) {

        case Idle:
            pAWG->comhdr.activeFunc = AWGFnSetOffset;

            pAWG->pOCoffset->OCxRS = AWGMV2PWM(pAWG, mvDCOffset);

            pAWG->comhdr.tStart = SYSGetMilliSecond();
            pAWG->comhdr.state = AWGWait;
            break;

        case AWGWait:
            if (SYSGetMilliSecond() - pAWG->comhdr.tStart >= PWM_SETTLING_TIME) {
                pAWG->comhdr.activeFunc = SMFnNone;
                pAWG->comhdr.state = Idle;
            }
            break;

        default:
            pAWG->comhdr.activeFunc = SMFnNone;
            pAWG->comhdr.state = Idle;
            break;
    }

    return (pAWG->comhdr.state);
}

STATE AWGStop(HINSTR hAWG) 
{
    AWG *       pAWG    = (AWG *) hAWG;

    if(pAWG->comhdr.activeFunc != SMFnNone)
    {
        return (Waiting);
    }
    else if(DCH7DSA == KVA_2_PA(pAWG->pLATx) && pAWG->pTMR->TxCON.ON == 1)
    {
        // stop the trigger to the DMA
        pAWG->pTMR->TxCON.ON = 0; // turn on the timer 

        // turning off the DMA channel does not reset the point SRC/DST pointers
        pAWG->pDMA->DCHxCON.CHEN = 0; // disable this DMA channel
    }

    return(Idle);
}
 
STATE AWGRun(HINSTR hAWG) 
{
    AWG *       pAWG    = (AWG *) hAWG;

    if(pAWG->comhdr.activeFunc != SMFnNone)
    {
        return (Waiting);
    }
    else if(pAWG->cbSrc == 0)
    {
        return(AWGWaveformNotSet);
    }
    else if(pAWG->pTMR->TxCON.ON != 1)
    {
        // DMA setup, this is shared with the Logic Analyzer, so we have to set it up each time
        pAWG->pDMA->DCHxCON.CHEN    = 0;                        // make sure the DMA is disabled
        pAWG->pDMA->DCHxDSA         = KVA_2_PA(pAWG->pLATx);    // Latch H address for destination
        pAWG->pDMA->DCHxDSIZ        = 2;                        // destination size 2 byte
        pAWG->pDMA->DCHxSSA         = pAWG->addrPhySrc;         // physical address of the source buffer
        pAWG->pDMA->DCHxSSIZ        = pAWG->cbSrc;              // how many bytes (not items) of the source buffer

        // timer setup, we know the timer is disabled.
        pAWG->pTMR->TxCON.TCKPS     = AWGPRESCALER;             // 1:1 prescalar
        pAWG->pTMR->PRx             = pAWG->PRXOnRun;           // what is the period
        pAWG->pTMR->TMRx            = 0;                        // init timer value to 0

        // enable the DMA and the timer
        pAWG->pDMA->DCHxCON.CHEN    = 1;                        // enable this DMA channel
        pAWG->pTMR->TxCON.ON        = 1;                        // turn on the timer 
    }

    return(Idle);
}

int16_t AWGmVGetActualOffsets(HINSTR hAWG, int16_t mvOffset, int16_t * pTableOffset)
{
    int16_t pwmOffset;

    if(mvOffset > DACMVLIMIT)           pwmOffset   = AWGRequestedmvOffsetToActual(hAWG, DACMVLIMIT);
    else if(mvOffset < -DACMVLIMIT)     pwmOffset   = AWGRequestedmvOffsetToActual(hAWG, -DACMVLIMIT);
    else                                pwmOffset   = AWGRequestedmvOffsetToActual(hAWG, mvOffset);

    *pTableOffset                                   = mvOffset - pwmOffset;

    if(*pTableOffset < -DACMVLIMIT || DACMVLIMIT < *pTableOffset)
    {
        *pTableOffset = 0;
        return(0);
    }

    return(pwmOffset);
}

STATE AWGSetCustomWaveform(HINSTR hAWG, int16_t rgWaveform[], uint32_t cWaveformEntries, int16_t mvDCOffset, uint32_t cSamplesPerSec) {
    
    AWG *       pAWG = (AWG *) hAWG;
    uint32_t    myState = Idle;

    if (pAWG->comhdr.activeFunc == AWGFnSetCustomWaveform || pAWG->comhdr.activeFunc == SMFnNone) 
    {
        myState = pAWG->comhdr.state;
    }
    else if(pAWG->comhdr.cNest == 0 || pAWG->comhdr.activeFunc != AWGFnSetOffset)
    {
        return (Waiting);
    }
    else
    {
        myState = AWGWaitOffset;
    }
    
    switch (myState) {
        
        case Idle:       
            {
                uint32_t i = 0;
                int32_t mvMax = rgWaveform[0];
                int32_t mvMin = rgWaveform[0];
                int16_t mvTblOffset = 0;
                
                if(pAWG->pTMR->TxCON.ON)
                {
                    return(AWGCurrentlyRunning);
                }
                      
                else if(cSamplesPerSec > AWGMAXSPS)
                {
                    return(AWGExceedsMaxSamplPerSec);                
                }
                
                for (i = 0; i < cWaveformEntries; i++) 
                {
                    int32_t mvEntry = rgWaveform[i] + mvDCOffset;

                    if(mvEntry > mvMax) mvMax = mvEntry;
                    if(mvEntry < mvMin) mvMin = mvEntry;
                }

                if((mvMax - mvMin) > AWGMAXP2P || mvMin < -AWGMAXP2P || AWGMAXP2P < mvMax)
                {
                    return(AWGValueOutOfRange);
                }

                // get and actual offset we can use
                pAWG->mvDCOffset = AWGmVGetActualOffsets(hAWG, (int16_t) ((mvMax + mvMin) / 2), &mvTblOffset);

                // fill in the table buffer with adjust values to our actual offset
                for (i = 0; i < cWaveformEntries; i++) rgWaveform[i] += mvDCOffset - pAWG->mvDCOffset;

                pAWG->comhdr.state  = AWGMakeDMABuffer;
            }
            break;
                
        case AWGMakeDMABuffer:
            {
                int16_t * dacValue = pAWG->mVFB;
                int32_t i = 0;

                for (i = 0; i < cWaveformEntries; i++) 
                {
                    int32_t mvE = rgWaveform[i];
                    int32_t j = HWDACSIZE/2;
                    int32_t iTop = HWDACSIZE-2;
                    int32_t iBot = 0;

                    if(mvE <= dacValue[0])
                    {
                        mvE = dacValue[0];
                        j = 0;
                    }

                    if(mvE >= dacValue[HWDACSIZE-1]) 
                    {
                        mvE = dacValue[HWDACSIZE-1];
                        j = HWDACSIZE-2;
                    }

                    while(!(dacValue[j] <= mvE && mvE <= dacValue[j+1]))
                    {
                        if(mvE > dacValue[j+1]) iBot = j;
                        if(mvE < dacValue[j])   iTop = j;

                        j = (iTop + iBot) / 2;

                        ASSERT(j < HWDACSIZE-1);
                    }

                    // if the upper value is closer than the lower value
                    if((dacValue[j+1] - mvE) < (mvE - dacValue[j])) j++;

                    // copy over the code to drive this value
                    rgWaveform[i] = pAWG->dacMap[j];
                }
 
                // now set things up to run with the DMA
                pAWG->addrPhySrc = KVA_2_PA(rgAWGBuff); // start address
                pAWG->cbSrc = cWaveformEntries * sizeof (rgAWGBuff[0]); // source size our data buffer 
                          
                pAWG->PRXOnRun  = ((AWGPBCLK + (cSamplesPerSec/2)) / cSamplesPerSec) - 1;
                
                (pAWG->comhdr.cNest)++;
                pAWG->comhdr.state = Idle;
                pAWG->comhdr.activeFunc = AWGFnSetOffset;
                return(AWGWaitOffset);
            }      
            break;
            
        case AWGWaitOffset:
            {            
                STATE nestedState = AWGSetOffsetVoltage(hAWG, pAWG->mvDCOffset);

                if(IsStateAnError(nestedState))
                {
                   (pAWG->comhdr.cNest)--;
                    pAWG->comhdr.activeFunc = SMFnNone;
                    pAWG->comhdr.state = Idle;               
                    return(nestedState);  
                }
                else if(nestedState == Idle)
                {
                   (pAWG->comhdr.cNest)--;
                    pAWG->comhdr.activeFunc = SMFnNone;
                    pAWG->comhdr.state = Idle;               
                }
            }
            break;
            
        default:
            pAWG->comhdr.activeFunc = SMFnNone;
            pAWG->comhdr.state = Idle;
            break;
    }

    return (pAWG->comhdr.state);
}

// take freq in mHz, returns in mHz
// Highest freq we can support is 1MHz, or 1,000,000,000 mHz (still an int32_t)
// Highest buffer size AWGMAXBUF, which must be less than 64K (still an int32_t))
// Highest AWGMAXSPS we support is 10MS/s (still an int32_t)
// so while we take uint32_t as inputs, you can cast from an int32_t just fine.
// if we did 1S/s and had a size of 25,000 we could support a waveform as slow as 1/25,000 Hz or 40 uHz
uint32_t AWGCalculateBuffAndSps(uint32_t reqFreqmHz, uint32_t * pcBuff, uint32_t * pSps)
{
    // calculate the timer and buffer size
    if(reqFreqmHz == 0)             // just a DC Value
    {
        *pSps = AWGMAXSPS;
        *pcBuff = 1;
        return(0);
    }

    // PB = (buff)(tmr)(freq)
    // you can't have a bigger buffer because we can't push data out faster
    // with a buffer size of 25,000 and sample rate of 10,000,000 
    // we can use slower sample rate at 400 Hz
    else if(((((uint64_t) AWGMAXSPS) * 1000ull + (reqFreqmHz/2)) / reqFreqmHz) < (uint64_t) AWGMAXBUF)
    {
        // the cutoff freq for here is 400 Hz
        *pSps = AWGMAXSPS;                                       // samples per sec
        *pcBuff = (uint32_t) ((((uint64_t) AWGMAXSPS) * 1000ull + (reqFreqmHz/2)) / reqFreqmHz);             // size of buffer              
    }
    // PB = (buff)(tmr)(freq)
    // use the maximum buffer size to use the fastest clock
    // this is from 400Hz down to 40uHz
    else
    {
        uint64_t pbx1000000 = ((uint64_t) AWGPBCLK) * 1000000ull;
        uint32_t tmr        = (uint32_t) (((pbx1000000 / reqFreqmHz / AWGMAXBUF) + 500) / 1000); 
        *pcBuff             = (uint32_t) (((pbx1000000 / reqFreqmHz / tmr) + 500) / 1000);        // buffer size   
        *pSps               = ((*pcBuff) * (uint64_t) reqFreqmHz + 500) / 1000;                             // samples per second
    }

    // sps = freq * cbuff
    // freq = sps/cbuff
    // return in mHz
    return((uint32_t) ((((int64_t) (*pSps) * 1000ll) + (*pcBuff)/2) / (*pcBuff)));         
}

STATE AWGSetWaveform(HINSTR hAWG, WAVEFORM waveform , uint32_t freqmHz, int32_t mvP2P, int32_t mvDCOffset) 
{
    
    AWG *       pAWG    = (AWG *) hAWG;
    uint32_t    myState = Idle;
    
    if (pAWG->comhdr.activeFunc == AWGFnSetWaveform || pAWG->comhdr.activeFunc == SMFnNone) 
    {
        myState = pAWG->comhdr.state;
    }
    else if(pAWG->comhdr.cNest == 0 || (pAWG->comhdr.activeFunc != AWGFnSetCustomWaveform && pAWG->comhdr.activeFunc != AWGFnSetOffset))
    {
        return (Waiting);
    }
    else
    {
        myState = AWGWaitCustomWaveform;
    }

    switch (myState) {
        
        case Idle: 
            
            if(pAWG->pTMR->TxCON.ON) return(AWGCurrentlyRunning);
            else if( mvP2P > AWGMAXP2P || mvP2P < 0  || 
                (mvDCOffset - mvP2P/2)  < -AWGMAXP2P || AWGMAXP2P < (mvP2P/2 + mvDCOffset) ||
                freqmHz > (AWGMAXFREQ * 1000) 
                ) return(AWGValueOutOfRange);

            AWGCalculateBuffAndSps(freqmHz, (uint32_t *) &pAWG->t2, (uint32_t *) &pAWG->t1);

            switch(waveform)
            {
                case waveDC:
                    pAWG->t1 = AWGMAXSPS;
                    pAWG->t2 = 1;
                    pAWG->comhdr.state = AWGDC;
                    break;
                    
                case waveSine:
                    pAWG->comhdr.state = AWGSine;
                    break;
                    
                case waveSquare:
                    pAWG->comhdr.state = AWGSquare;
                    break;
                    
                case waveTriangle:
                    pAWG->comhdr.state = AWGTriangle;
                    break;
                    
                case waveSawtooth:
                    pAWG->comhdr.state = AWGSawtooth;
                    break;

                default:
                    return(AWGWaveformNotSupported);
            }
            
            pAWG->comhdr.activeFunc = AWGFnSetWaveform;
            break;
        
        case AWGDC:
            {
                rgAWGBuff[0] =  0;                   
                
                (pAWG->comhdr.cNest)++;
                pAWG->comhdr.state = Idle;
                pAWG->comhdr.activeFunc = AWGFnSetCustomWaveform;
                return(AWGWaitCustomWaveform);
            }
            break;

        case AWGSquare:
            {
                int32_t i;
                int16_t halfMag = ((int16_t) mvP2P) / 2;
                int16_t mhalfMag = -halfMag;
                
                for (i = 0; i < ((pAWG->t2)/2); i++) {
                    rgAWGBuff[i] =  (uint16_t) mhalfMag;                   
                }
                
                for (; i < pAWG->t2; i++) {
                    rgAWGBuff[i] =  (uint16_t) halfMag;                   
                }
                
                (pAWG->comhdr.cNest)++;
                pAWG->comhdr.state = Idle;
                pAWG->comhdr.activeFunc = AWGFnSetCustomWaveform;
                return(AWGWaitCustomWaveform);
            }
            break;
            
        case AWGSawtooth:
            {
                int32_t i;
                double dblBuffSize      = ((double) pAWG->t2);
                double dblMagnitude     = (double) mvP2P;
                double dblHalfMagnitude = dblMagnitude / 2;
                
                for (i = 0; i < pAWG->t2; i++) {
                    rgAWGBuff[i] =  (uint16_t) ((int16_t) ((dblMagnitude * (((double) i) / dblBuffSize)) - dblHalfMagnitude));                   
                }
                
                (pAWG->comhdr.cNest)++;
                pAWG->comhdr.state = Idle;
                pAWG->comhdr.activeFunc = AWGFnSetCustomWaveform;
                return(AWGWaitCustomWaveform);
            }
            break;
            
        case AWGTriangle:
            {
                int32_t i;
                int32_t j;
                
                int32_t buffDown        = (pAWG->t2 / 2);           // truncates odd numbers low
                int32_t buffUp          = (pAWG->t2 - buffDown);    // may be one longer than buffDown
                double dblBuffSizeUp    = ((double) buffUp);
                double dblBuffSizeDown  = ((double) buffDown);
                double dblMagnitude     = (double) mvP2P;
                double dblHalfMagnitude = dblMagnitude / 2;
                
                // create up slope
                for (i = 0; i < buffUp; i++) {
                    rgAWGBuff[i] =  (uint16_t) ((int16_t) ((dblMagnitude * (((double) i) / dblBuffSizeUp)) - dblHalfMagnitude));                   
                }
                
                // create down slope
                for (j = 0; i < pAWG->t2; i++, j++) {
                    rgAWGBuff[i] =  (uint16_t) ((int16_t) ((dblMagnitude * ((dblBuffSizeDown - ((double) j)) / dblBuffSizeDown)) - dblHalfMagnitude));                   
                }
                
                (pAWG->comhdr.cNest)++;
                pAWG->comhdr.state = Idle;
                pAWG->comhdr.activeFunc = AWGFnSetCustomWaveform;
                return(AWGWaitCustomWaveform);
            }
            break;
        
        case AWGSine:
            {
                int32_t i;
                double dblBuffSize = ((double) pAWG->t2);
                double dblMagnitude = ((double) mvP2P) / 2;
                
                for (i = 0; i < pAWG->t2; i++) {
                    rgAWGBuff[i] =  (uint16_t) ((int16_t) (dblMagnitude * sin((((double) i) / dblBuffSize) * M_TWOPI)));                   
                }
                
                (pAWG->comhdr.cNest)++;
                pAWG->comhdr.state = Idle;
                pAWG->comhdr.activeFunc = AWGFnSetCustomWaveform;
                return(AWGWaitCustomWaveform);
            }
            break;
            
        case AWGBode:
           {
                uint32_t j;
                int32_t  i;
                double dblBuffSize = ((double) pAWG->t2);
                double dblMagnitude = (4 * (double) mvP2P) / M_TWOPI;
                
                // clear the buffer
                memset(rgAWGBuff, 0, pAWG->t2 * sizeof(rgAWGBuff[0]));

                for(j = 1; (j *  freqmHz) < 2000000000; j += 2)
                {
                    double dblMagnitudeT = dblMagnitude / j;
                    for (i = 0; i < pAWG->t2; i++) 
                    {
                        rgAWGBuff[i] +=  (uint16_t) ((int16_t) (dblMagnitudeT * sin((((double) (j * i)) / dblBuffSize) * M_TWOPI)));                   
                    }
                }
                
                (pAWG->comhdr.cNest)++;
                pAWG->comhdr.state = Idle;
                pAWG->comhdr.activeFunc = AWGFnSetCustomWaveform;
                return(AWGWaitCustomWaveform);
            }
            break;
            
        case AWGWaitCustomWaveform:
            {            
                STATE nestedState = AWGSetCustomWaveform(hAWG, (int16_t *) rgAWGBuff, (uint32_t) pAWG->t2, mvDCOffset, pAWG->t1);

                if(nestedState == Idle || IsStateAnError(nestedState))
                {
                   (pAWG->comhdr.cNest)--;
                    pAWG->comhdr.activeFunc = SMFnNone;
                    pAWG->comhdr.state = Idle;               
                }

                return(nestedState);
            }
            break;
            
        default:
            pAWG->comhdr.activeFunc = SMFnNone;
            pAWG->comhdr.state = Idle;
            break;
    }

    return (pAWG->comhdr.state);
}
