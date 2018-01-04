
/************************************************************************/
/*                                                                      */
/*    ProcessJSONCmd.C                                                  */
/*                                                                      */
/*    Periodic Task the process command submitted via JSON              */
/*                                                                      */
/************************************************************************/
/*    Author:     Keith Vogel                                           */
/*    Copyright 2016, Digilent Inc.                                     */
/************************************************************************/
/*  Revision History:                                                   */
/*                                                                      */
/*    8/3/2016 (KeithV): Created                                        */
/************************************************************************/
#include <OpenScope.h>

static void ALogProcess(IALOG& ialog)
{

    switch(ialog.state.processing)
    {
        case Idle:
        case Stopped:
        case Waiting:
            break;

        case Queued:
            ialog.state.processing = Working;
            break;

        case Working:
            {
                DFILE&      dFile               = *((DFILE *) ialog.pdFile);

                // make sure the file is closed
                if(dFile) dFile.fsclose();

                ialog.bidx.cTotalSamples    = 0;    // total number of valid samples          
                ialog.bidx.iDMAEnd          = 0;    // end of valid data in DMA buffer   
                ialog.bidx.cDMARoll         = 0;    // roll count
                ialog.bidx.iDMAStart        = 0;    // start of unsaved samples in DMA buffer, all of sample time
                ialog.bidx.cSavedRoll       = 0;    // unsaved DMA roll count
                ialog.bidx.cBackLog         = 0;    // in a file write, how much more to write
                ialog.stcd                  = STCDNormal;   // Normal stop condition.
                ialog.tStart                = ReadCoreTimer();
                ialog.state.processing      = Running;
                ialog.state.instrument      = WaitingRun;

                // if we are writing to the SD card open the file
                if(ialog.vol == VOLSD)
                {
                    // open the file it should already exist, seek to end                        
                    if( DFATFS::fschdrive(DFATFS::szFatFsVols[ialog.vol])                   != FR_OK    || 
                        DFATFS::fschdir(DFATFS::szRoot)                                     != FR_OK    ||
                        dFile.fsopen(ialog.szURI, FA_OPEN_EXISTING | FA_WRITE | FA_READ)    != FR_OK    ||
                        dFile.fslseek(dFile.fssize())                                       != FR_OK    )
                    {
                        dFile.fsclose();
                        ialog.stcd              = STCDError; 
                        ialog.state.processing  = Stopped;
                        ialog.bidx.cBackLog     = 0;
                    }
                }
            }
            break;

        case Running:
            {

                if(ialog.state.instrument != Idle) ialog.state.instrument = ALOGRun(&ialog);
 
                if(ialog.vol == VOLSD)
                { 
                    const OSC&  osc                 = *((ALOG *) rgInstr[ialog.id])->posc;
                    DFILE&      dFile               = *((DFILE *) ialog.pdFile);
                    uint32_t    tCur                = ReadCoreTimer();

                    uint32_t    cThisTime;
                    uint64_t    cTotalSampled;
                    uint64_t    cWrittenSampled;
                    int32_t     iDMA;
                    int32_t     clDMA;

                    // get a good dma location
                    do
                    {
                        clDMA   = ialog.bidx.cDMARoll;
                        iDMA    = osc.pDMAch2->DCHxDPTR;
                    } while(ialog.bidx.cDMARoll != clDMA);

                    // convert to sample index
                    iDMA /= sizeof(uint16_t);    

                    cTotalSampled   = iDMA + LOGDMASIZE * clDMA;
                    cWrittenSampled = ialog.bidx.iDMAStart + LOGDMASIZE * ialog.bidx.cSavedRoll;
                    ialog.bidx.cBackLog = cTotalSampled - cWrittenSampled;

                    // absolutely should not happen; 
                    ASSERT(cTotalSampled >= cWrittenSampled);

                    // The cBackLog should be < 100,000 worst case, but allow whatever
                    // but we can not wrap as cBackLog is an int32, the cTotalSampled/cWrittenSampled counters are uint64 
                    ASSERT(ialog.bidx.cBackLog >= 0);

                    // back log counter
                    maxLogWrite = max((uint32_t) ialog.bidx.cBackLog, maxLogWrite);

                    // if things are stopping, we should eventually hit an Idle state
                    // so do nothing until we get to idle
                    if(ialog.stcd != STCDNormal)
                    {
                        // overflow condition, we are done writting data
                        cThisTime               = 0;

                        // Remeber, DO NOT fool with the actual DMA counters until we are in an idle state
                    }

                    // if we either overflowed the RAM buffer, or we filled the file; say we overflowed
                    // this can only happen while writing to a file, RAM operations only stay in RAM and don't overflow
                    // unless the DMA misses a sample; and that we assume won't happen
                    else if(ialog.bidx.cBackLog > LOGMAXBACKLOG || cWrittenSampled >= LOGMAXFILESAMP)
                    {
                        // stop collecting data
                        ALOGStop(&ialog);

                        // overwrite STCDForce with overflow
                        ialog.stcd      = STCDOverflow;

                        // overflow condition, we are done writting data
                        cThisTime               = 0;

                        // Remeber, DO NOT fool with the actual DMA counters until we are in an idle state
                    }

                    // here we try to write beyond the end of file, limit it to the file length
                    // next pass we will overflow the file.
                    else if(cTotalSampled > LOGMAXFILESAMP)
                    {
                        cThisTime = LOGMAXFILESAMP - cWrittenSampled;
                    }

                    // our timer expired or we are finishing up, don't worry about sector alignment
                    else if((tCur - ialog.tStart) > LOGMAXTICKSBEFORESDSAVE || ialog.state.instrument == Idle)
                    {
                        cThisTime = ialog.bidx.cBackLog;
                    }

                    // normal write, sector align
                    else
                    {
                        cThisTime =  ialog.bidx.cBackLog - (ialog.bidx.cBackLog % ((_FATFS_CBSECTOR_)/sizeof(uint16_t)));
                    }

                    // limit us to what we will write in a pass
                    if(cThisTime > LOGMAXSMPTORWRT) cThisTime = LOGMAXSMPTORWRT;

                    // if we have something to write and we are not in an error condition, write the data.
                    // note, we may be
                    if(cThisTime > 0)
                    {
                        // open the file if it needs to be open
                        if(!dFile && ialog.stcd == STCDNormal &&
                            (DFATFS::fschdrive(DFATFS::szFatFsVols[ialog.vol])                  != FR_OK    || 
                             DFATFS::fschdir(DFATFS::szRoot)                                    != FR_OK    ||
                             dFile.fsopen(ialog.szURI, FA_OPEN_EXISTING | FA_WRITE | FA_READ)   != FR_OK    ))
                        {
                            dFile.fsclose();
                            ALOGStop(&ialog);
                            ialog.stcd              = STCDError; 
                        }

                        // write to the file
                        // we may need to seek to the end of the file is a read was done to get existing points.
                        else if(dFile.fstell() == dFile.fssize() || dFile.fslseek(dFile.fssize()) == FR_OK)
                        {
                            int16_t         rgu16[LOGMAXBUFFSIZE];
                            uint32_t        cSmpl                   = (ialog.bidx.iDMAStart + cThisTime) > LOGDMASIZE ? LOGDMASIZE - ialog.bidx.iDMAStart : cThisTime;;
                            uint32_t        cbWritten               = 0;

                            // copy and convert
                            memcpy(rgu16, &ialog.pBuff[ialog.bidx.iDMAStart], cSmpl*sizeof(uint16_t));
                            if(cSmpl < cThisTime) memcpy(&rgu16[cSmpl], ialog.pBuff, (cThisTime-cSmpl)*sizeof(uint16_t));

                            OSCVinFromDadcArray((HINSTR) &osc, rgu16, cThisTime);

                            if(dFile.fswrite(rgu16, cThisTime*sizeof(uint16_t), &cbWritten, LOGMAXSECTORWRT) != FR_OK)
                            {
                                ALOGStop(&ialog);
                                ialog.stcd              = STCDError; 
                            }

                            ialog.tStart = tCur;         // restart the timer

                            cbWritten /= sizeof(uint16_t);
                            ialog.bidx.iDMAStart += cbWritten;
                            if(ialog.bidx.iDMAStart > LOGDMASIZE)
                            {
                                ialog.bidx.cSavedRoll++;
                                ialog.bidx.iDMAStart -= LOGDMASIZE;
                            }
                            ialog.bidx.cBackLog -= cbWritten;

                            // counters
                            maxLogWrittenCnt = max(maxLogWrittenCnt, cbWritten);
                            if(cLogWrite == 1000) 
                            {
                                aveLogWrite = ((999ll * ((uint64_t) aveLogWrite)) + cbWritten) / 1000;
                            }
                            else
                            {
                                aveLogWrite = ((((uint64_t) cLogWrite) * aveLogWrite) + cbWritten) / (cLogWrite+1);
                                cLogWrite++;
                            }
                        }
                        else
                        {
                            dFile.fsclose();
                            ALOGStop(&ialog);
                            ialog.stcd              = STCDError; 
                        }
                    }
            
                    // finish and get out
                    else if(ialog.state.instrument == Idle)
                    {

                        // if an error, adjust our pointers to the last written
                        if(ialog.stcd != STCDNormal)
                        {
                            // we MUST be finished before we play with the DMA/ISR pointers!
                            // you can really hose up the calculations if you try to adjust these
                            // while running, even when processing an error condition.
                            ialog.bidx.iDMAEnd      = ialog.bidx.iDMAStart;
                            ialog.bidx.cDMARoll     = ialog.bidx.cSavedRoll;
                            ialog.bidx.cBackLog     = 0;
                        }
 
                        ASSERT(ialog.bidx.cBackLog == 0);
                        ASSERT(ialog.bidx.iDMAEnd == ialog.bidx.iDMAStart);
                        ialog.bidx.cTotalSamples = ((int64_t) ialog.bidx.cDMARoll) * LOGDMASIZE + ialog.bidx.iDMAEnd;

                        // open the file if it needs to be open
                        if( !dFile &&
                               (DFATFS::fschdrive(DFATFS::szFatFsVols[ialog.vol])                  != FR_OK    || 
                                DFATFS::fschdir(DFATFS::szRoot)                                    != FR_OK    ||
                                dFile.fsopen(ialog.szURI, FA_OPEN_EXISTING | FA_WRITE | FA_READ)   != FR_OK    ))
                        {
                            dFile.fsclose();
                            ialog.stcd              = STCDError; 
                        }

                        // write out the header
                        // make sure we seek to the front of the file
                        else if(dFile.fslseek(0) == FR_OK)
                        {
                            LogHeader   logHdr  = LogHeader();
                            uint32_t    cbHdr   = 0;  

                            logHdr.AHdr.stopReason   = ialog.stcd;
                            logHdr.AHdr.iStart       = 0;             
                            logHdr.AHdr.actualCount  = ialog.bidx.cTotalSamples;        
                            logHdr.AHdr.uSPS         = ialog.bidx.xsps;               
                            logHdr.AHdr.psDelay      = ialog.bidx.psDelay;  
                            dFile.fswrite(&logHdr, sizeof(logHdr), &cbHdr, DFILE::FS_INFINITE_SECTOR_CNT);
                        }
                           
                        dFile.fsclose();
                        ialog.state.processing = Stopped;
                        ialog.buffLock = LOCKAvailable;
                    }

                    // haven't written anything for awhile, close the file
                    else if((tCur - ialog.tStart) > LOGMAXTICKSBEFORESDSAVE)
                    {
                        dFile.fsclose();
                    }

                }

                // we are done running and done writing data; this is for VOLRAM
                else if(ialog.state.instrument == Idle)
                {
                    const OSC&  osc                 = *((ALOG *) rgInstr[ialog.id])->posc;

                    ASSERT(ialog.vol == VOLRAM);

                    // number of samples taken
                    if(ialog.bidx.cDMARoll == 0)  
                    {
                        ialog.bidx.iDMAStart        = 0;
                        ialog.bidx.cTotalSamples    = ialog.bidx.iDMAEnd;
                    }
                    else
                    {
                        ialog.bidx.iDMAStart        = ialog.bidx.iDMAEnd;
                        ialog.bidx.cTotalSamples    = LOGDMASIZE;        
                    }

                    // convert ADC data to mv
                    OSCVinFromDadcArray((HINSTR) &osc, ialog.pBuff, LOGDMASIZE);

                    ialog.state.processing = Stopped;
                    ialog.buffLock = LOCKAvailable;
                }
            }
            break;

        default:
            ASSERT(NEVER_SHOULD_GET_HERE);
            break;
    }
}

static void TRGProcess(void)
{
    uint32_t    i;
    bool     fReady = false;

    switch(pjcmd.trigger.state.processing)
    {

        case Calibrating:   // calibrating the instruments
        case Idle:
        case Waiting:
        case Triggered:
        case Stopped:
            break;

        case Run:

            // get everyone running, to the Armed state
            for(i=0; i<pjcmd.trigger.cRun; i++) 
            {
                if(pjcmd.trigger.rgtte[i].fWorking)
                {
                    STATE retState = Idle;
                    switch(pjcmd.trigger.rgtte[i].instrID)
                    {
                        case OSC1_ID:
                            if((retState = pjcmd.ioscCh1.state.instrument = OSCRun(rgInstr[pjcmd.ioscCh1.id], &pjcmd.ioscCh1)) == Armed)
                            {
                                pjcmd.trigger.rgtte[i].fWorking = false;
                            }
                            break;

                        case OSC2_ID:
                            if((retState = pjcmd.ioscCh2.state.instrument = OSCRun(rgInstr[pjcmd.ioscCh2.id], &pjcmd.ioscCh2)) == Armed)
                            {
                                pjcmd.trigger.rgtte[i].fWorking = false;
                            }
                            break;

                        case LOGIC1_ID:
                            if((retState = pjcmd.ila.state.instrument = LARun(rgInstr[LOGIC1_ID], &pjcmd.ila)) == Armed)
                            {
                                pjcmd.trigger.rgtte[i].fWorking = false;
                            }
                            break;
 
                        default:
                            ASSERT(NEVER_SHOULD_GET_HERE);
                            break;
                    }

                    // not going well....
                    if(IsStateAnError(retState))
                    {
                            pjcmd.trigger.state.processing = Idle;
                            for(i=0; i<pjcmd.trigger.cRun; i++) 
                            {
                                pjcmd.trigger.rgtte[i].pstate->processing = Idle;
                                *pjcmd.trigger.rgtte[i].pLockState = LOCKAvailable; 
                            }
                            return;
                    }
                }
            }

            // now lets see if everyone is set up
            for(fReady = true, i=0; i<pjcmd.trigger.cRun; i++) fReady &= !pjcmd.trigger.rgtte[i].fWorking;
            if(fReady)
            {
                for(i=0; i<pjcmd.trigger.cRun; i++) pjcmd.trigger.rgtte[i].fWorking = true;

                // go to the armed state
                pjcmd.trigger.state.processing = Armed;

                TRGSingle();
            }
            break;

       case Armed:

            // wait for everyone to complete
            for(i=0; i<pjcmd.trigger.cRun; i++) 
            {
                if(pjcmd.trigger.rgtte[i].fWorking)
                {
                    STATE retState = Idle;
                    switch(pjcmd.trigger.rgtte[i].instrID)
                    {
                        case OSC1_ID:
                            if((retState = pjcmd.ioscCh1.state.instrument = OSCRun(rgInstr[pjcmd.ioscCh1.id], &pjcmd.ioscCh1)) == Idle)
                            {
                                pjcmd.trigger.rgtte[i].fWorking = false;
                                pjcmd.ioscCh1.acqCount = pjcmd.trigger.acqCount;
                            }
                            break;

                        case OSC2_ID:
                            if((retState = pjcmd.ioscCh2.state.instrument = OSCRun(rgInstr[pjcmd.ioscCh2.id], &pjcmd.ioscCh2)) == Idle)
                            {
                                pjcmd.trigger.rgtte[i].fWorking = false;
                                pjcmd.ioscCh2.acqCount = pjcmd.trigger.acqCount;
                            }
                            break;

                        case LOGIC1_ID:
                            if((retState = pjcmd.ila.state.instrument = LARun(rgInstr[LOGIC1_ID], &pjcmd.ila)) == Idle)
                            {
                                pjcmd.trigger.rgtte[i].fWorking = false;
                                pjcmd.ila.acqCount = pjcmd.trigger.acqCount;
                            }
                            break;
 
                        // these can only be triggers, but we still have to get them running
                        default:
                            ASSERT(NEVER_SHOULD_GET_HERE);
                            break;
                    }

                    // not going well....
                    if(IsStateAnError(retState))
                    {
                            pjcmd.trigger.state.processing = Idle;
                            for(i=0; i<pjcmd.trigger.cRun; i++) 
                            {
                                pjcmd.trigger.rgtte[i].pstate->processing = Idle;
                                *pjcmd.trigger.rgtte[i].pLockState = LOCKAvailable; 
                            }
                            return;
                    }
                }
            }

            // now lets see if everyone is set up
            for(fReady = true, i=0; i<pjcmd.trigger.cRun; i++) fReady &= !pjcmd.trigger.rgtte[i].fWorking;
            if(fReady)
            {
                int64_t deltaPS = 0;

                // get the time delta from the DMA location and the actual trigger, will be a negative number
                switch(pjcmd.trigger.idTrigSrc)
                {
                    case OSC1_ID:
                    case FORCE_TRG_ID:
                        deltaPS = GetPicoSec(pjcmd.trigger.indexBuff - pjcmd.ioscCh1.bidx.iDMATrig, pjcmd.ioscCh1.bidx.xsps, 1000);
                        break;

                    case OSC2_ID:
                        deltaPS = GetPicoSec(pjcmd.trigger.indexBuff - pjcmd.ioscCh2.bidx.iDMATrig, pjcmd.ioscCh2.bidx.xsps, 1000);
                        break;

                    case LOGIC1_ID:
                        // we are OSC based in timing, so if the trigger is the LA, move the timing out to match the OSC which is delayed by the
                        // OSC conversion time.
                        deltaPS = GetPicoSec(pjcmd.trigger.indexBuff - pjcmd.ila.bidx.iDMATrig, pjcmd.ila.bidx.xsps, 1000) + ADCpsDELAY;
                        break;
 
                    default:
                        ASSERT(NEVER_SHOULD_GET_HERE);
                        break;
                }

                // Adjust the buffers to align to where we want them.
                for(i=0; i<pjcmd.trigger.cRun; i++) 
                {
                    switch(pjcmd.trigger.rgtte[i].instrID)
                    {
                        case OSC1_ID:
                            ScrollBuffer((uint16_t *) pjcmd.ioscCh1.pBuff, pjcmd.ioscCh1.bidx.cDMA, pjcmd.ioscCh1.bidx.iTrigDMA, (pjcmd.ioscCh1.bidx.iDMATrig + (int32_t) GetSamples(deltaPS, pjcmd.ioscCh1.bidx.xsps, 1000)));

                            pjcmd.ioscCh1.state.processing = Triggered;
                            pjcmd.ioscCh1.buffLock = LOCKAvailable;
                            break;

                        case OSC2_ID:
                            ScrollBuffer((uint16_t *) pjcmd.ioscCh2.pBuff, pjcmd.ioscCh2.bidx.cDMA, pjcmd.ioscCh2.bidx.iTrigDMA, (pjcmd.ioscCh2.bidx.iDMATrig + (int32_t) GetSamples(deltaPS, pjcmd.ioscCh2.bidx.xsps, 1000)));

                            pjcmd.ioscCh2.state.processing = Triggered;
                            pjcmd.ioscCh2.buffLock = LOCKAvailable;
                            break;

                        case LOGIC1_ID:
                            // remember we are OSC based in timing of the result buffers. BUT, there is a delay between when the signal hits the ADC and when the 
                            // DMA moves it into the result buffer, that is the conversion time. Ideally we would add this time as a delay to the ADCs, but since there are 2
                            // ADCs and only 1 LA, we can just subtract this time from the LA to align the LA result buffer to the ADC result buffer. 
                            ScrollBuffer(pjcmd.ila.pBuff, pjcmd.ila.bidx.cDMA, pjcmd.ila.bidx.iTrigDMA, (pjcmd.ila.bidx.iDMATrig + (int32_t) GetSamples(deltaPS - ADCpsDELAY, pjcmd.ila.bidx.xsps, 1000)));

                            pjcmd.ila.state.processing = Triggered;
                            pjcmd.ila.buffLock = LOCKAvailable;
                            break;
 
                        default:
                            ASSERT(NEVER_SHOULD_GET_HERE);
                            break;
                    }
                }
                
                pjcmd.trigger.state.processing = Triggered;
            }
            break;

         default:
            ASSERT(NEVER_SHOULD_GET_HERE);
            break;
   }
}

static void AWGProcess(void)
{
    switch(pjcmd.iawg.state.processing)
    {
        case Calibrating:   // calibrating the instruments
        case JSPARAwgWaitingRegularWaveform:       // Parser has set up the AWG and is waiting for processing of a regulary waveform
        case JSPARAwgWaitingArbitraryWaveform:      // waiting to run an arbitrary waveform, needs configuration
        case Stopped:       // AWG is stopped, but a run can be issued immediately
        case Running:       // AWG is running
        case Idle:          // AWG needs configuration
            break;

        case JSPARAwgRunRegularWaveform:
            if((pjcmd.iawg.state.instrument = AWGSetWaveform(rgInstr[AWG1_ID], pjcmd.iawg.waveform , pjcmd.iawg.freq, pjcmd.iawg.mvP2P, pjcmd.iawg.mvOffset)) == Idle)
            {
                pjcmd.iawg.state.processing = Run;       // still working, but go to a run ready parsing state
            }
            else if(IsStateAnError(pjcmd.iawg.state.instrument))
            {
                pjcmd.iawg.state.processing = Idle;    
            }
            break;

        case JSPARAwgRunArbitraryWaveform:
            if((pjcmd.iawg.state.instrument = AWGSetCustomWaveform(rgInstr[AWG1_ID], (int16_t *) pjcmd.iawg.pBuff, pjcmd.iawg.cBuff, pjcmd.iawg.mvOffset, pjcmd.iawg.sps)) == Idle)
            {
                pjcmd.iawg.state.processing = Run;       // still working, but go to a run ready parsing state
            }
            else if(IsStateAnError(pjcmd.iawg.state.instrument))
            {
                pjcmd.iawg.state.processing = Idle;    
            }
            break;

        case Run:    // start the AWG
            if((pjcmd.iawg.state.instrument = AWGRun(rgInstr[AWG1_ID])) == Idle)
            {
                pjcmd.iawg.state.processing = Running;
            }
            else if(IsStateAnError(pjcmd.iawg.state.instrument))
            {
                pjcmd.iawg.state.processing = Idle;
            }
            break;

        default:
            ASSERT(NEVER_SHOULD_GET_HERE);
            break;
    }
}

#ifdef LASTOP
static void LAProcess(void)
{
    switch(pjcmd.ila.state.processing)
    {
        case Idle:
        case Waiting:
        case Armed:
        case Triggered:
        case JSPARLaRun:
            break;

        case JSPARLaStop:
            {
                LA& la = *((LA *) rgInstr[LOGIC1_ID]);
                
                // turn off the DMA
                T7CONbits.ON = 0;
                la.pDMA->DCHxCON.CHEN    = 0; 

                // wait for it not to be busy
                while(la.pDMA->DCHxCON.CHBUSY);

                UnLockLA();
                pjcmd.ila.state.processing = Idle;
            }
            break;
            
        default:
            ASSERT(NEVER_SHOULD_GET_HERE);
            break;
    }
}
#endif

static void DCProcess(IDC& idc)
{
    switch(idc.state.processing)
    {
        case Calibrating:   // calibrating the instruments
        case Idle:
            break;

       case Working:
           if((idc.state.instrument = DCSetVoltage(rgInstr[idc.id], idc.mVolts)) == Idle || IsStateAnError(idc.state.instrument))
           {
                idc.state.processing = Idle;
           }
           break;

        default:
            ASSERT(NEVER_SHOULD_GET_HERE);
            break;
    }
}

static void Calibrate(void)
{
    switch(pjcmd.iCal.state.processing)
    {
        case Idle:
        case NotCfgForCalibration:
            break;

        // Calibrate the system
        case JSPARCalibrationStart:
            if((pjcmd.iCal.state.instrument = CFGCalibrateInstruments(instrGrp)) == Idle) 
            {
                pjcmd.iCal.state.processing     = Idle;

                pjcmd.trigger.state.processing  = Idle;
                pjcmd.iawg.state.processing     = Idle;
                pjcmd.idcCh1.state.processing   = Idle;
                pjcmd.idcCh2.state.processing   = Idle;
                pjcmd.ioscCh1.state.processing  = Idle;
                pjcmd.ioscCh2.state.processing  = Idle;
                pjcmd.ila.state.processing      = Idle;
                pjcmd.iALog1.state.processing   = Idle;
                pjcmd.iALog2.state.processing   = Idle;
            }
            else if(IsStateAnError(pjcmd.iCal.state.instrument))
            {
                pjcmd.iCal.state.processing     = NotCfgForCalibration;

                pjcmd.trigger.state.processing  = Idle;
                pjcmd.iawg.state.processing     = Idle;
                pjcmd.idcCh1.state.processing   = Idle;
                pjcmd.idcCh2.state.processing   = Idle;
                pjcmd.ioscCh1.state.processing  = Idle;
                pjcmd.ioscCh2.state.processing  = Idle;
                pjcmd.ila.state.processing      = Idle;
                pjcmd.iALog1.state.processing   = Idle;
                pjcmd.iALog2.state.processing   = Idle;
            }
            break;

        // save the calibration data
        case JSPARCalibrationSave:

            switch(pjcmd.iCal.config)
            {
                case CFGSD:
                    pjcmd.iCal.state.instrument = CFGSaveCalibration(instrGrp, USER_CALIBRATION);
                    break;

                case CFGFLASH:
                    pjcmd.iCal.state.instrument = CFGSaveCalibration(instrGrp, FACTORY_CALIBRATION);
                    break;

                default:
                    pjcmd.iCal.state.instrument = InvalidCommand;
                    break;
            }

            // are we done
            if(pjcmd.iCal.state.instrument == Idle || IsStateAnError(pjcmd.iCal.state.instrument)) 
            {
                pjcmd.iCal.state.processing     = Idle;
            }
            break;

        // save the calibration data
        case JSPARCalibrationLoad:

            switch(pjcmd.iCal.config)
            {
                case CFGSD:
                    pjcmd.iCal.state.instrument = CFGReadCalibrationInfo(instrGrp, USER_CALIBRATION);
                    break;

                case CFGFLASH:
                    pjcmd.iCal.state.instrument = CFGReadCalibrationInfo(instrGrp, FACTORY_CALIBRATION);
                    break;

                default:
                    pjcmd.iCal.state.instrument = InvalidCommand;
                    break;
            }

            // are we done
            if(pjcmd.iCal.state.instrument == Idle || IsStateAnError(pjcmd.iCal.state.instrument)) 
            {
                pjcmd.iCal.state.processing     = Idle;
            }
            break;

        default:
            ASSERT(NEVER_SHOULD_GET_HERE);
            break;
    }
}

static void EnterBootloader(void)
{
    switch(pjcmd.iBoot.processing)
    {
        case Idle:
            break;

        case Queued:
            pjcmd.iBoot.tStart = SYSGetMilliSecond();
            pjcmd.iBoot.processing = Waiting;
            break;
 
        case Waiting:
            if(SYSGetMilliSecond() - pjcmd.iBoot.tStart > 1000)
            {
                // This is where the bootloader will look to
                // enter the bootloader. C12 gets cleared on MZ
                rgOSC1Buff[0] = 1;

                // execute a software reset
                _softwareReset();
            }
            break;

        default:
            ASSERT(NEVER_SHOULD_GET_HERE);
            break;
    }
}

static void WiFi(void)
{
    switch(pjcmd.iWiFi.state.processing)
    {
        case Idle:
            break;

        case JSPARNicDisconnect:
            pjcmd.iWiFi.fScanReady = false;
            if((pjcmd.iWiFi.state.instrument =  WiFiDisconnect()) == Idle || IsStateAnError(pjcmd.iWiFi.state.instrument))
            {
                pjcmd.iWiFi.state.processing = Idle;

                // put back to default values
                pjcmd.iWiFi.fForceConn          = false;
                pjcmd.iWiFi.fWorking            = false;
            }
            break;
           
        case JSPARNicConnect:
            {
                WiFiConnectInfo& wifiConn = pjcmd.iWiFi.fWorking ? pjcmd.iWiFi.wifiWConn : pjcmd.iWiFi.wifiAConn;

                pjcmd.iWiFi.fScanReady = false;
                if((pjcmd.iWiFi.state.instrument =  WiFiConnect(wifiConn, pjcmd.iWiFi.fForceConn)) == Idle || IsStateAnError(pjcmd.iWiFi.state.instrument))
                {
                    pjcmd.iWiFi.state.processing    = Idle;

                    // put back to default values
                    pjcmd.iWiFi.fForceConn          = false;
                    pjcmd.iWiFi.fWorking            = false;
                }
            }
            break;

        case JSPARWiFiScan:
            pjcmd.iWiFi.fScanReady = false;
            if((pjcmd.iWiFi.state.instrument =  WiFiScan(pjcmd.iWiFi.wifiScan)) == Idle || IsStateAnError(pjcmd.iWiFi.state.instrument))
            {
                pjcmd.iWiFi.state.processing = Idle;
                pjcmd.iWiFi.fScanReady = true;
            }
            break;

        case JSPARWiFiSaveParameters:
            if((pjcmd.iWiFi.state.instrument = WiFiSaveConnInfo(dWiFiFile, pjcmd.iWiFi.vol, pjcmd.iWiFi.wifiWConn)) == Idle || IsStateAnError(pjcmd.iWiFi.state.instrument))
            {
                pjcmd.iWiFi.state.processing = Idle;
            }
            break;

        case JSPARWiFiLoadParameters:
            
            if((pjcmd.iWiFi.state.instrument = WiFiLoadConnInfo(dWiFiFile, pjcmd.iWiFi.vol, pjcmd.iWiFi.szSSID, pjcmd.iWiFi.wifiWConn)) == Idle || IsStateAnError(pjcmd.iWiFi.state.instrument))
            {
                pjcmd.iWiFi.state.processing = Idle;
            }
            break;

        default:
            ASSERT(NEVER_SHOULD_GET_HERE);
            break;
    }
}

STATE JSONCmdTask(void)
{
    EnterBootloader();
    Calibrate();
    WiFi();

    DCProcess(pjcmd.idcCh1);
    DCProcess(pjcmd.idcCh2);

    AWGProcess();

#ifdef LASTOP
    LAProcess();
#endif

    TRGProcess();

    ALogProcess(pjcmd.iALog1);
    ALogProcess(pjcmd.iALog2);

    Serial.PeriodicTask(&DCH1CON);
    
    return(Idle);
}