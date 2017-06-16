
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

static STATE TRGProcess(void)
{
    uint32_t    i;
    bool     fReady = false;

    switch(pjcmd.trigger.state.processing)
    {

        case Calibrating:   // calibrating the instruments
        case Idle:
        case Triggered:
        case Stopped:
            break;

        case Queued:

            for(i=0; i<pjcmd.trigger.cRun; i++) pjcmd.trigger.rgtte[i].fWorking = true;

            // see if we can process the trigger
            // TODO: move this to the single code, put in fWorking
            fReady = (pjcmd.trigger.state.processing == Triggered);
            for(i=0; i<pjcmd.trigger.cRun; i++) fReady &= (pjcmd.trigger.rgtte[i].pstate->processing == Triggered);

            // otherwise set up the trigger
            if(!fReady && !TRGSetUp())
            {
                pjcmd.trigger.state.processing = Idle;
                for(i=0; i<pjcmd.trigger.cRun; i++) pjcmd.trigger.rgtte[i].pstate->processing = Idle;
                return(TRGUnableToSetTrigger);
            }

            // keep the instruments static while we run
            for(i=0; i<pjcmd.trigger.cRun; i++)
            {
                    pjcmd.trigger.rgtte[i].pstate->processing = Armed;
                    *pjcmd.trigger.rgtte[i].pLockState = LOCKAcq; 
            }

            pjcmd.trigger.state.processing = Run;
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
                            return(retState);
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
                            return(retState);
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
                        deltaPS = GetPicoSec(pjcmd.trigger.indexBuff - pjcmd.ioscCh1.bidx.iDMATrig, pjcmd.ioscCh1.bidx.msps);
                        break;

                    case OSC2_ID:
                        deltaPS = GetPicoSec(pjcmd.trigger.indexBuff - pjcmd.ioscCh2.bidx.iDMATrig, pjcmd.ioscCh2.bidx.msps);
                        break;

                    case LOGIC1_ID:
                        // we are OSC based in timing, so if the trigger is the LA, move the timing out to match the OSC which is delayed by the
                        // OSC conversion time.
                        deltaPS = GetPicoSec(pjcmd.trigger.indexBuff - pjcmd.ila.bidx.iDMATrig, pjcmd.ila.bidx.msps) + ADCpsDELAY;
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
                            ScrollBuffer((uint16_t *) pjcmd.ioscCh1.pBuff, pjcmd.ioscCh1.bidx.cDMA, pjcmd.ioscCh1.bidx.iTrigDMA, (pjcmd.ioscCh1.bidx.iDMATrig + (int32_t) GetSamples(deltaPS, pjcmd.ioscCh1.bidx.msps)));

                            pjcmd.ioscCh1.state.processing = Triggered;
                            pjcmd.ioscCh1.buffLock = LOCKAvailable;
                            break;

                        case OSC2_ID:
                            ScrollBuffer((uint16_t *) pjcmd.ioscCh2.pBuff, pjcmd.ioscCh2.bidx.cDMA, pjcmd.ioscCh2.bidx.iTrigDMA, (pjcmd.ioscCh2.bidx.iDMATrig + (int32_t) GetSamples(deltaPS, pjcmd.ioscCh2.bidx.msps)));

                            pjcmd.ioscCh2.state.processing = Triggered;
                            pjcmd.ioscCh2.buffLock = LOCKAvailable;
                            break;

                        case LOGIC1_ID:
                            // remember we are OSC based in timing of the result buffers. BUT, there is a delay between when the signal hits the ADC and when the 
                            // DMA moves it into the result buffer, that is the conversion time. Ideally we would add this time as a delay to the ADCs, but since there are 2
                            // ADCs and only 1 LA, we can just subtract this time from the LA to align the LA result buffer to the ADC result buffer. 
                            ScrollBuffer(pjcmd.ila.pBuff, pjcmd.ila.bidx.cDMA, pjcmd.ila.bidx.iTrigDMA, (pjcmd.ila.bidx.iDMATrig + (int32_t) GetSamples(deltaPS - ADCpsDELAY, pjcmd.ila.bidx.msps)));

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
    return(pjcmd.trigger.state.processing);
}

static STATE AWGProcess(void)
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

    return(pjcmd.iawg.state.processing);
}

static STATE LAProcess(void)
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

    return(pjcmd.ila.state.processing);
}

static STATE DCProcess(IDC * pidc)
{
    switch(pidc->state.processing)
    {
        case Calibrating:   // calibrating the instruments
        case Idle:
            break;

       case Working:
           if((pidc->state.instrument = DCSetVoltage(rgInstr[pidc->id], pidc->mVolts)) == Idle || IsStateAnError(pidc->state.instrument))
           {
                pidc->state.processing = Idle;
           }
           break;

        default:
            ASSERT(NEVER_SHOULD_GET_HERE);
            break;
    }
    return(pidc->state.processing);
}

static STATE Calibrate(void)
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

   return(pjcmd.iCal.state.processing);
}

static STATE EnterBootloader(void)
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

   return(pjcmd.iBoot.processing);
}

static STATE WiFi(void)
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

   return(pjcmd.iWiFi.state.processing);
}

STATE JSONCmdTask(void)
{
    EnterBootloader();
    Calibrate();
    WiFi();

    DCProcess(&pjcmd.idcCh1);
    DCProcess(&pjcmd.idcCh2);

    AWGProcess();
    LAProcess();

    TRGProcess();

    Serial.PeriodicTask(&DCH1CON);
    
    return(Idle);
}